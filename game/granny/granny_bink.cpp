/*  granny_bink.cpp - the Bink texture decoder of granny.dll (RAD Granny 1.2b, 2000), for the textures of the
    game's .grn files.  See docs/granny-replacement.md.

    granny.dll stores a model's textures as one-frame Bink videos (revision 'i', a single key frame, no alpha)
    and decodes them at GrannyOpenModel with the Bink library linked into the DLL: BinkOpen (0x1003c440),
    BinkDoFrame (0x1003db40, planes 0x10043300 / 0x10043480), BinkCopyToBuffer (0x1003d590) to 24-bit RGB with
    the MMX blitter 0x1004a7e0 and its colour tables (built by 0x10046880).  Everything here is integer
    arithmetic and gives the DLL's pixels exactly.

    The Bink tables (Huffman lookup tables, block patterns, scan order, quantisers) are not in this source
    tree: they are read from the player's own granny.dll at run time (granny_dll_tables), like the game's
    data section is read from Neutron2.exe.                                                                */
#include "granny_internal.h"

#include <string.h>

namespace granny_internal {

namespace {

/* bit reader: least significant bit first from little-endian 32-bit words (the DLL keeps {u32 *ptr, u32 bits,
   u32 count}); reading past the end gives zeros */
struct Bits
{
	const unsigned char *buf;
	size_t size;     /* bytes */
	size_t pos;      /* bits */
	u32 get(int n)
	{
		if (n <= 0) return 0;
		size_t byte = pos >> 3;
		int shift = (int)(pos & 7);
		uint64_t w = 0;
		for (int i = 0; i < 8; i++)
			if (byte + i < size) w |= (uint64_t)buf[byte + i] << (8 * i);
		pos += (size_t)n;
		return (u32)((w >> shift) & (n == 32 ? 0xffffffffu : ((1u << n) - 1)));
	}
	u32 peek(int n)
	{
		size_t p = pos;
		u32 v = get(n);
		pos = p;
		return v;
	}
	u32 bit() { return get(1); }
	void skip(int n) { pos += (size_t)n; }
	void align32() { if (pos & 31) pos += 32 - (pos & 31); }
};

/* a Huffman tree as transmitted: one of the 16 fixed code tables + a symbol permutation (0x100448c0) */
struct Tree
{
	int table;
	unsigned char syms[16];
};

static int get_huff(Bits &b, const Tree &t, const BinkTables &tab)
{
	int bits = tab.huff_bits[t.table];
	unsigned char e = tab.huff[t.table][b.peek(bits)];  /* (code length << 4) | symbol index */
	b.skip(e >> 4);
	return t.syms[e & 15];
}

/* 0x10044d40 */
static void merge(Bits &b, unsigned char *dst, const unsigned char *src, int size)
{
	const unsigned char *src2 = src + size;
	int size2 = size;
	do
	{
		if (!b.bit()) { *dst++ = *src++; size--; }
		else { *dst++ = *src2++; size2--; }
	} while (size && size2);
	while (size--) *dst++ = *src++;
	while (size2--) *dst++ = *src2++;
}

/* 0x100448c0 */
static void read_tree(Bits &b, Tree &t)
{
	t.table = (int)b.get(4);
	if (!t.table)
	{
		for (int i = 0; i < 16; i++) t.syms[i] = (unsigned char)i;
		return;
	}
	if (b.bit())
	{
		int len = (int)b.get(3);
		u32 unused = 0xffff;
		int n = 0;
		for (int i = 0; i <= len; i++)
		{
			int s = (int)b.get(4);
			unused &= ~(1u << s);
			t.syms[n++] = (unsigned char)s;
		}
		for (int i = 0; i < 16 && unused; i++, unused >>= 1)
			if ((unused & 1) && n < 16) t.syms[n++] = (unsigned char)i;
	}
	else
	{
		int len = (int)b.get(2);
		unsigned char tmp[2][16];
		unsigned char *in = tmp[0], *out = tmp[1];
		for (int i = 0; i < 16; i++) in[i] = (unsigned char)i;
		for (int i = 0; i <= len; i++)
		{
			int size = 1 << i;
			for (int k = 0; k < 16; k += size << 1) merge(b, out + k, in + k, size);
			unsigned char *x = in; in = out; out = x;
		}
		memcpy(t.syms, in, 16);
	}
}

enum { SRC_BLOCK_TYPES, SRC_SUB_BLOCK_TYPES, SRC_COLORS, SRC_PATTERN, SRC_X_OFF, SRC_Y_OFF, SRC_INTRA_DC,
       SRC_INTER_DC, SRC_RUN, NB_SRC };

enum { SKIP_BLOCK, SCALED_BLOCK, MOTION_BLOCK, RUN_BLOCK, RESIDUE_BLOCK, INTRA_BLOCK, FILL_BLOCK, INTER_BLOCK,
       PATTERN_BLOCK, RAW_BLOCK };

/* a bundle: a stream of values decoded a block row ahead of their use (0x100447f0) */
struct Bundle
{
	int len;                    /* bits of the per-row value count */
	Tree tree;
	std::vector<int> data;      /* values (bytes, signed bytes or 16-bit DCs) */
	size_t cur_dec;             /* next free slot, (size_t)-1: no more data in this plane */
	size_t cur_ptr;             /* next value to use */
	void init(u32 width, int per_block, size_t blocks)
	{
		u32 n = 0x200 + (width >> 3) * (u32)per_block - 1, bits = 0;
		while (n) { bits++; n >>= 1; }
		len = (int)bits;
		data.assign(blocks * 64 + 64, 0);     /* values accumulate over the whole plane */
		cur_dec = cur_ptr = 0;
	}
	bool done() const { return cur_dec == (size_t)-1 || cur_dec > cur_ptr; }
	void put(int v) { if (cur_dec < data.size()) data[cur_dec] = v; cur_dec++; }
	int get() { int v = cur_ptr < data.size() ? data[cur_ptr] : 0; cur_ptr++; return v; }
};

struct Ctx
{
	const BinkTables *tab;
	Bundle bundle[NB_SRC];
	Tree col_high[16];
	int col_lastval;
};

static const int DC_START_BITS = 11;

/* a new value count for this row: false when the bundle needs nothing (still has values, or has ended) */
static bool check_read(Bits &b, Bundle &bd, int &t)
{
	if (bd.done()) return false;
	t = (int)b.get(bd.len);
	if (!t) { bd.cur_dec = (size_t)-1; return false; }
	return true;
}

/* 0x10044e60 */
static void read_block_types(Bits &b, Ctx &c, Bundle &bd)
{
	int t;
	if (!check_read(b, bd, t)) return;
	size_t end = bd.cur_dec + (size_t)t;
	if (b.bit())
	{
		int v = (int)b.get(4);
		for (int i = 0; i < t; i++) bd.put(v);
		return;
	}
	int last = 0;
	while (bd.cur_dec < end)
	{
		int v = get_huff(b, bd.tree, *c.tab);
		if (v < 12) { last = v; bd.put(v); }
		else
		{
			int run = c.tab->rle_lens[v - 12];
			for (int i = 0; i < run; i++) bd.put(last);
		}
	}
}

/* 0x100458d0 */
static void read_patterns(Bits &b, Ctx &c, Bundle &bd)
{
	int t;
	if (!check_read(b, bd, t)) return;
	size_t end = bd.cur_dec + (size_t)t;
	while (bd.cur_dec < end)
	{
		int v = get_huff(b, bd.tree, *c.tab);
		v |= get_huff(b, bd.tree, *c.tab) << 4;
		bd.put(v);
	}
}

/* 0x10045100 (revision 'i' colours) */
static void read_colors(Bits &b, Ctx &c, Bundle &bd)
{
	int t;
	if (!check_read(b, bd, t)) return;
	size_t end = bd.cur_dec + (size_t)t;
	if (b.bit())
	{
		c.col_lastval = get_huff(b, c.col_high[c.col_lastval], *c.tab);
		int v = get_huff(b, bd.tree, *c.tab);
		v = (c.col_lastval << 4) | v;
		for (int i = 0; i < t; i++) bd.put(v);
		return;
	}
	while (bd.cur_dec < end)
	{
		c.col_lastval = get_huff(b, c.col_high[c.col_lastval], *c.tab);
		int v = get_huff(b, bd.tree, *c.tab);
		bd.put((c.col_lastval << 4) | v);
	}
}

/* 0x10045ac0 */
static void read_motion_values(Bits &b, Ctx &c, Bundle &bd)
{
	int t;
	if (!check_read(b, bd, t)) return;
	size_t end = bd.cur_dec + (size_t)t;
	if (b.bit())
	{
		int v = (int)b.get(4);
		if (v && b.bit()) v = -v;
		for (int i = 0; i < t; i++) bd.put(v);
		return;
	}
	while (bd.cur_dec < end)
	{
		int v = get_huff(b, bd.tree, *c.tab);
		if (v && b.bit()) v = -v;
		bd.put(v);
	}
}

/* 0x10045d20: DCs, delta coded in groups of 8 */
static void read_dcs(Bits &b, Bundle &bd, int start_bits, int has_sign)
{
	int len;
	if (!check_read(b, bd, len)) return;
	int v = (int)b.get(start_bits - has_sign);
	if (v && has_sign && b.bit()) v = -v;
	bd.put((int16_t)v);
	len--;
	for (int i = 0; i < len; i += 8)
	{
		int len2 = len - i < 8 ? len - i : 8;
		int bsize = (int)b.get(4);
		if (bsize)
		{
			for (int j = 0; j < len2; j++)
			{
				int v2 = (int)b.get(bsize);
				if (v2 && b.bit()) v2 = -v2;
				v += v2;
				bd.put((int16_t)v);
			}
		}
		else
			for (int j = 0; j < len2; j++) bd.put((int16_t)v);
	}
}

/* 0x100456d0 */
static void read_runs(Bits &b, Ctx &c, Bundle &bd)
{
	int t;
	if (!check_read(b, bd, t)) return;
	size_t end = bd.cur_dec + (size_t)t;
	if (b.bit())
	{
		int v = (int)b.get(4);
		for (int i = 0; i < t; i++) bd.put(v);
		return;
	}
	while (bd.cur_dec < end) bd.put(get_huff(b, bd.tree, *c.tab));
}

static int coef_value(Bits &b, int bits)
{
	if (!bits) return b.bit() ? -1 : 1;
	int t = (int)(b.get(bits) | 1u << bits);
	return b.bit() ? -t : t;
}

/* 0x100488e0: DCT coefficients by bit planes, into block[] (16-bit, raster order); returns the quantiser */
static int read_dct_coeffs(Bits &b, int16_t block[64], const unsigned char *scan)
{
	int coef_list[128], mode_list[128];
	int list_start = 64, list_end = 64;
	coef_list[list_end] = 4;  mode_list[list_end++] = 0;
	coef_list[list_end] = 24; mode_list[list_end++] = 0;
	coef_list[list_end] = 44; mode_list[list_end++] = 0;
	coef_list[list_end] = 1;  mode_list[list_end++] = 3;
	coef_list[list_end] = 2;  mode_list[list_end++] = 3;
	coef_list[list_end] = 3;  mode_list[list_end++] = 3;
	for (int bits = (int)b.get(4) - 1; bits >= 0; bits--)
	{
		int list_pos = list_start;
		while (list_pos < list_end)
		{
			if (!(mode_list[list_pos] | coef_list[list_pos]) || !b.bit()) { list_pos++; continue; }
			int ccoef = coef_list[list_pos], mode = mode_list[list_pos];
			switch (mode)
			{
			case 0:
			case 2:
				if (mode == 0) { coef_list[list_pos] = ccoef + 4; mode_list[list_pos] = 1; }
				else { coef_list[list_pos] = 0; mode_list[list_pos++] = 0; }
				for (int i = 0; i < 4; i++, ccoef++)
				{
					if (b.bit()) { coef_list[--list_start] = ccoef; mode_list[list_start] = 3; }
					else block[scan[ccoef & 63]] = (int16_t)coef_value(b, bits);
				}
				break;
			case 1:
				mode_list[list_pos] = 2;
				for (int i = 0; i < 3; i++)
				{
					ccoef += 4;
					coef_list[list_end] = ccoef; mode_list[list_end++] = 2;
				}
				break;
			case 3:
				block[scan[ccoef & 63]] = (int16_t)coef_value(b, bits);
				coef_list[list_pos] = 0; mode_list[list_pos++] = 0;
				break;
			}
		}
	}
	return (int)b.get(4);
}

/* 0x10049b30 (intra blocks): dequantise (coefficient * q >> 11, raster order) and the
   integer IDCT; the row pass rounds with (x + 0x7f) >> 8 and the result is stored as a byte, not clamped */
static void idct(const int16_t *in, const int32_t *q, int out[64])
{
	int tmp[64];
	for (int i = 0; i < 8; i++)
	{
		const int16_t *s = in + i;
		const int32_t *k = q + i;
		int *d = tmp + i;
		if (!(s[8] | s[16] | s[24] | s[32] | s[40] | s[48] | s[56]))
		{
			int v = s[0] * k[0] >> 11;
			for (int r = 0; r < 8; r++) d[8 * r] = v;
			continue;
		}
		int s0 = s[0] * k[0] >> 11, s1 = s[8] * k[8] >> 11, s2 = s[16] * k[16] >> 11, s3 = s[24] * k[24] >> 11;
		int s4 = s[32] * k[32] >> 11, s5 = s[40] * k[40] >> 11, s6 = s[48] * k[48] >> 11, s7 = s[56] * k[56] >> 11;
		int a0 = s0 + s4, a1 = s0 - s4, a2 = s2 + s6, a3 = ((s2 - s6) * 0xb50 >> 11) - a2;
		int a4 = s5 + s3, a5 = s5 - s3, a6 = s1 + s7, a7 = s1 - s7;
		int b0 = a4 + a6, b1 = (a5 + a7) * 0xec8 >> 11;
		int b2 = ((a5 * -0x14e8 >> 11) - b0) + b1;
		int b3 = ((a6 - a4) * 0xb50 >> 11) - b2;
		int b4 = ((a7 * 0x8a9 >> 11) - b1) + b3;
		d[0] = a0 + a2 + b0;       d[56] = a0 + a2 - b0;
		d[8] = a1 + a3 + b2;       d[48] = a1 + a3 - b2;
		d[16] = a1 - a3 + b3;      d[40] = a1 - a3 - b3;
		d[32] = a0 - a2 + b4;      d[24] = a0 - a2 - b4;
	}
	for (int i = 0; i < 8; i++)
	{
		const int *s = tmp + 8 * i;
		int *d = out + 8 * i;
		int a0 = s[0] + s[4], a1 = s[0] - s[4], a2 = s[2] + s[6], a3 = ((s[2] - s[6]) * 0xb50 >> 11) - a2;
		int a4 = s[5] + s[3], a5 = s[5] - s[3], a6 = s[1] + s[7], a7 = s[1] - s[7];
		int b0 = a4 + a6, b1 = (a5 + a7) * 0xec8 >> 11;
		int b2 = ((a5 * -0x14e8 >> 11) - b0) + b1;
		int b3 = ((a6 - a4) * 0xb50 >> 11) - b2;
		int b4 = ((a7 * 0x8a9 >> 11) - b1) + b3;
		d[0] = (a0 + a2 + b0 + 0x7f) >> 8;  d[7] = (a0 + a2 - b0 + 0x7f) >> 8;
		d[1] = (a1 + a3 + b2 + 0x7f) >> 8;  d[6] = (a1 + a3 - b2 + 0x7f) >> 8;
		d[2] = (a1 - a3 + b3 + 0x7f) >> 8;  d[5] = (a1 - a3 - b3 + 0x7f) >> 8;
		d[4] = (a0 - a2 + b4 + 0x7f) >> 8;  d[3] = (a0 - a2 - b4 + 0x7f) >> 8;
	}
}

struct Plane
{
	std::vector<unsigned char> pix;
	int w, h, stride;   /* stride = w rounded up to 8 (the decoder writes whole blocks) */
	unsigned char *at(int x, int y) { return &pix[(size_t)y * stride + x]; }
};

static void put_scaled(Plane &p, int x0, int y0, const unsigned char ub[64])
{
	for (int j = 0; j < 16; j++)
		for (int i = 0; i < 16; i++)
			if (x0 + i < p.stride && y0 + j < (int)(p.pix.size() / p.stride)) *p.at(x0 + i, y0 + j) = ub[(j >> 1) * 8 + (i >> 1)];
}

/* 0x10043480: one plane */
static bool decode_plane(Ctx &c, Bits &b, Plane &p)
{
	const BinkTables &tab = *c.tab;
	int bw = (p.w + 7) >> 3, bh = (p.h + 7) >> 3;
	u32 width = (u32)(bw * 8);
	size_t nblocks = (size_t)bw * bh;
	c.bundle[SRC_BLOCK_TYPES].init(width, 1, nblocks);
	c.bundle[SRC_SUB_BLOCK_TYPES].init(width >> 1, 1, nblocks);
	c.bundle[SRC_COLORS].init(width, 64, nblocks);
	c.bundle[SRC_PATTERN].init(width, 8, nblocks);
	c.bundle[SRC_X_OFF].init(width, 1, nblocks);
	c.bundle[SRC_Y_OFF].init(width, 1, nblocks);
	c.bundle[SRC_INTRA_DC].init(width, 1, nblocks);
	c.bundle[SRC_INTER_DC].init(width, 1, nblocks);
	c.bundle[SRC_RUN].init(width, 48, nblocks);
	read_tree(b, c.bundle[SRC_BLOCK_TYPES].tree);
	read_tree(b, c.bundle[SRC_SUB_BLOCK_TYPES].tree);
	for (int i = 0; i < 16; i++) read_tree(b, c.col_high[i]);    /* 0x10044e00 */
	c.col_lastval = 0;
	read_tree(b, c.bundle[SRC_COLORS].tree);
	read_tree(b, c.bundle[SRC_PATTERN].tree);
	read_tree(b, c.bundle[SRC_X_OFF].tree);
	read_tree(b, c.bundle[SRC_Y_OFF].tree);
	read_tree(b, c.bundle[SRC_RUN].tree);
	unsigned char ublock[64];
	int16_t dct[64];
	int out[64];
	for (int by = 0; by < bh; by++)
	{
		read_block_types(b, c, c.bundle[SRC_BLOCK_TYPES]);
		read_block_types(b, c, c.bundle[SRC_SUB_BLOCK_TYPES]);
		read_colors(b, c, c.bundle[SRC_COLORS]);
		read_patterns(b, c, c.bundle[SRC_PATTERN]);
		read_motion_values(b, c, c.bundle[SRC_X_OFF]);
		read_motion_values(b, c, c.bundle[SRC_Y_OFF]);
		read_dcs(b, c.bundle[SRC_INTRA_DC], DC_START_BITS, 0);
		read_dcs(b, c.bundle[SRC_INTER_DC], DC_START_BITS, 1);
		read_runs(b, c, c.bundle[SRC_RUN]);
		for (int bx = 0; bx < bw; bx++)
		{
			int x0 = bx * 8, y0 = by * 8;
			int blk = c.bundle[SRC_BLOCK_TYPES].get();
			/* a 16x16 block on an odd row is the lower half of one already decoded */
			if ((by & 1) && blk == SCALED_BLOCK) { bx++; continue; }
			switch (blk)
			{
			case SCALED_BLOCK:
			{
				int sub = c.bundle[SRC_SUB_BLOCK_TYPES].get();
				switch (sub)
				{
				case RUN_BLOCK:
				{
					const unsigned char *scan = tab.patterns + 64 * b.get(4);
					int si = 0;   /* (bounded for broken data: a run cannot leave the pattern) */
					int i = 0;
					do
					{
						int run = c.bundle[SRC_RUN].get() + 1;
						i += run;
						if (b.bit())
						{
							int v = c.bundle[SRC_COLORS].get();
							for (int j = 0; j < run; j++) ublock[scan[si++ & 63]] = (unsigned char)v;
						}
						else
							for (int j = 0; j < run; j++) ublock[scan[si++ & 63]] = (unsigned char)c.bundle[SRC_COLORS].get();
					} while (i < 63);
					if (i == 63) ublock[scan[si++ & 63]] = (unsigned char)c.bundle[SRC_COLORS].get();
					break;
				}
				case INTRA_BLOCK:
				{
					memset(dct, 0, sizeof dct);
					dct[0] = (int16_t)c.bundle[SRC_INTRA_DC].get();
					int q = read_dct_coeffs(b, dct, tab.scan) & 15;
					idct(dct, tab.intra_quant + 64 * q, out);
					for (int i = 0; i < 64; i++) ublock[i] = (unsigned char)out[i];
					break;
				}
				case FILL_BLOCK:
				{
					int v = c.bundle[SRC_COLORS].get();
					memset(ublock, v, 64);
					break;
				}
				case PATTERN_BLOCK:
				{
					int col[2];
					col[0] = c.bundle[SRC_COLORS].get();
					col[1] = c.bundle[SRC_COLORS].get();
					for (int j = 0; j < 8; j++)
					{
						int v = c.bundle[SRC_PATTERN].get();
						for (int i = 0; i < 8; i++, v >>= 1) ublock[i + j * 8] = (unsigned char)col[v & 1];
					}
					break;
				}
				case RAW_BLOCK:
					for (int i = 0; i < 64; i++) ublock[i] = (unsigned char)c.bundle[SRC_COLORS].get();
					break;
				default:
					return false;
				}
				put_scaled(p, x0, y0, ublock);
				bx++;
				break;
			}
			case RUN_BLOCK:
			{
				const unsigned char *scan = tab.patterns + 64 * b.get(4);
				int si = 0;
				int i = 0;
				do
				{
					int run = c.bundle[SRC_RUN].get() + 1;
					i += run;
					if (b.bit())
					{
						int v = c.bundle[SRC_COLORS].get();
						for (int j = 0; j < run; j++) { int pos = scan[si++ & 63]; *p.at(x0 + (pos & 7), y0 + (pos >> 3)) = (unsigned char)v; }
					}
					else
						for (int j = 0; j < run; j++) { int pos = scan[si++ & 63]; *p.at(x0 + (pos & 7), y0 + (pos >> 3)) = (unsigned char)c.bundle[SRC_COLORS].get(); }
				} while (i < 63);
				if (i == 63) { int pos = scan[si++ & 63]; *p.at(x0 + (pos & 7), y0 + (pos >> 3)) = (unsigned char)c.bundle[SRC_COLORS].get(); }
				break;
			}
			case INTRA_BLOCK:
			{
				memset(dct, 0, sizeof dct);
				dct[0] = (int16_t)c.bundle[SRC_INTRA_DC].get();
				int q = read_dct_coeffs(b, dct, tab.scan) & 15;
				idct(dct, tab.intra_quant + 64 * q, out);
				for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) *p.at(x0 + i, y0 + j) = (unsigned char)out[j * 8 + i];
				break;
			}
			case FILL_BLOCK:
			{
				int v = c.bundle[SRC_COLORS].get();
				for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) *p.at(x0 + i, y0 + j) = (unsigned char)v;
				break;
			}
			case PATTERN_BLOCK:
			{
				int col[2];
				col[0] = c.bundle[SRC_COLORS].get();
				col[1] = c.bundle[SRC_COLORS].get();
				for (int j = 0; j < 8; j++)
				{
					int v = c.bundle[SRC_PATTERN].get();
					for (int i = 0; i < 8; i++, v >>= 1) *p.at(x0 + i, y0 + j) = (unsigned char)col[v & 1];
				}
				break;
			}
			case RAW_BLOCK:
				for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) *p.at(x0 + i, y0 + j) = (unsigned char)c.bundle[SRC_COLORS].get();
				break;
			default:
				/* SKIP, MOTION, RESIDUE, INTER: blocks that refer to the previous frame, which a texture's
				   single key frame does not use (none of the game's 85 textures has one) */
				return false;
			}
		}
	}
	b.align32();
	return true;
}

static inline int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

} /* namespace */

/* the one key frame of a Bink 'i' file -> 24-bit RGB, rows top to bottom, bytes R G B (the DLL's
   BinkCopyToBuffer surface 2 path).  false if the data is not something this decoder handles. */
bool bink_decode_rgb24(const unsigned char *data, size_t size, const BinkTables &tab, u32 *out_w, u32 *out_h,
                       std::vector<unsigned char> &rgb)
{
	if (!tab.ok || size < 48 || memcmp(data, "BIK", 3)) return false;
	u32 rd[12];
	for (int i = 0; i < 12; i++) rd[i] = rd32(data + 4 * i);
	char rev = (char)data[3];
	u32 frames = rd[2], w = rd[5], h = rd[6], flags = rd[9], audio = rd[10];
	if (rev != 'i' || frames < 1 || audio || (flags & 0x00100000) || !w || !h || w > 4096 || h > 4096) return false;
	if (size < 44 + 8) return false;
	u32 f0 = rd32(data + 44) & ~1u, f1 = rd32(data + 48) & ~1u;
	if (f0 >= size || f1 > size || f1 <= f0) return false;
	const unsigned char *frame = data + f0;
	size_t fsize = f1 - f0;
	Ctx c;
	c.tab = &tab;
	c.col_lastval = 0;
	Plane planes[3];
	for (int i = 0; i < 3; i++)
	{
		planes[i].w = i ? (int)((w + 1) >> 1) : (int)w;
		planes[i].h = i ? (int)((h + 1) >> 1) : (int)h;
		planes[i].stride = (planes[i].w + 7) & ~7;
		planes[i].pix.assign((size_t)planes[i].stride * ((planes[i].h + 7) & ~7), 0);
	}
	/* 0x10043300: revision 'i' starts with the byte offset of the chroma planes */
	Bits b = { frame, fsize, 32 };
	if (!decode_plane(c, b, planes[0])) return false;
	{
		u32 coff = rd32(frame);
		b.pos = (size_t)coff * 8;
	}
	if (!(flags & 0x00020000))
	{
		if (!decode_plane(c, b, planes[1])) return false;
		if (!decode_plane(c, b, planes[2])) return false;
	}
	else
	{
		memset(&planes[1].pix[0], 128, planes[1].pix.size());
		memset(&planes[2].pix[0], 128, planes[2].pix.size());
	}
	/* 0x10046880 colour tables (signed division by 32768, rounding toward zero) and the MMX blitter
	   0x1004a7e0: Y' = (sat(Y - 16) * 4 * 0x4a85) >> 16, R = Y' + Vr, G = Y' + Ug + Vg, B = Y' + Ub,
	   clamped to 0..255; chroma by nearest sample.  The first chroma plane in the stream is V (Cr). */
	int Vg[256], Ug[256], Vr[256], Ub[256];
	for (int i = 0; i < 256; i++)
	{
		Vg[i] = ((128 - i) * 0x680f) / 32768;
		Ug[i] = ((128 - i) * 0x3225) / 32768;
		Vr[i] = ((i - 128) * 0xcc4b) / 32768;
		Ub[i] = ((i - 128) * 0x10235) / 32768;
	}
	rgb.resize((size_t)w * h * 3);
	for (u32 y = 0; y < h; y++)
		for (u32 x = 0; x < w; x++)
		{
			int Y = *planes[0].at((int)x, (int)y);
			int V = *planes[1].at((int)(x >> 1), (int)(y >> 1));
			int U = *planes[2].at((int)(x >> 1), (int)(y >> 1));
			int ys = Y - 16 < 0 ? 0 : Y - 16;
			int yy = (int)(((ys << 2) * 0x4a85) >> 16);
			unsigned char *o = &rgb[((size_t)y * w + x) * 3];
			o[0] = (unsigned char)clamp255(yy + Vr[V]);
			o[1] = (unsigned char)clamp255(yy + (int16_t)(Ug[U] + Vg[V]));
			o[2] = (unsigned char)clamp255(yy + Ub[U]);
		}
	*out_w = w;
	*out_h = h;
	return true;
}

} /* namespace granny_internal */
