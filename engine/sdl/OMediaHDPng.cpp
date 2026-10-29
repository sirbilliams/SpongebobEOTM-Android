// port: HD PNG reader, deliberately separate from the original game's PNG loader.
#include "OMediaHDPng.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
using std::string;
static unsigned int be32(const unsigned char *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }

static int paeth(int a, int b, int c)
{
	const int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
	return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

static const char *decode(const std::string &file, omhd::DecodedPng &image, size_t budget)
{
	FILE *f = fopen(file.c_str(), "rb");
	if (!f) return "cannot be opened";
	struct CloseFile { FILE *f; CloseFile(FILE *p) : f(p) {} ~CloseFile() { fclose(f); } } close(f);
	if (fseek(f, 0, SEEK_END) != 0) return "cannot be sized";
	const long length = ftell(f);
	if (length < 0 || (size_t)length > 64u * 1024u * 1024u || (size_t)length > budget / 4)
		return "exceeds the compressed PNG memory limit";
	rewind(f);
	std::vector<unsigned char> data;
	data.reserve((size_t)length);
	unsigned char buf[65536];
	size_t got;
	while ((got = fread(buf, 1, sizeof buf, f)) > 0) {
		if (data.size() + got > (size_t)length) return "changed while reading";
		data.insert(data.end(), buf, buf + got);
	}
	if (ferror(f)) return "cannot be read";
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (data.size() < 8 || memcmp(&data[0], sig, 8) != 0) return "is not a PNG file";

	unsigned int w = 0, h = 0;
	int depth = 0, ctype = -1, interlace = 0;
	std::vector<unsigned char> idat, plte, trns;
	idat.reserve(data.size());
	size_t pos = 8;
	bool end = false;
	while (!end && pos + 12 <= data.size())
	{
		const unsigned int len = be32(&data[pos]);
		if (len > data.size() - pos - 12) return "is truncated";
		const unsigned char *t = &data[pos + 4], *d = &data[pos + 8];
		if ((unsigned long)crc32(crc32(0L, Z_NULL, 0), t, len + 4) != be32(d + len)) return "has a bad chunk CRC";
		if (pos == 8 && memcmp(t, "IHDR", 4)) return "has no initial IHDR";
		if (!memcmp(t, "IHDR", 4))
		{
			if (pos != 8 || len != 13) return "has an invalid IHDR";
			w = be32(d); h = be32(d + 4); depth = d[8]; ctype = d[9]; interlace = d[12];
			if (d[10] || d[11] || interlace > 1) return "uses an unsupported PNG method";
		}
		else if (!memcmp(t, "PLTE", 4)) {
			if (!len || len > 768 || len % 3) return "has a bad palette";
			plte.assign(d, d + len);
		}
		else if (!memcmp(t, "tRNS", 4)) {
			if (len > 256) return "has bad transparency";
			trns.assign(d, d + len);
		}
		else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), d, d + len);
		else if (!memcmp(t, "IEND", 4)) { if (len) return "has a bad IEND"; end = true; }
		else if (!(t[0] & 32)) return "has an unknown critical chunk";
		pos += 12 + len;
	}
	if (!end) return "has no IEND";
	int chans;
	switch (ctype) { case 0: chans = 1; break; case 2: chans = 3; break; case 3: chans = 1; break;
					 case 4: chans = 2; break; case 6: chans = 4; break; default: return "has an unknown colour type"; }
	if (w == 0 || h == 0 || w > 32768 || h > 32768) return "has a bad size";
	if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return "has a bad bit depth";
	if ((ctype == 3 && depth == 16) || ((ctype == 2 || ctype == 4 || ctype == 6) && depth < 8))
		return "has an invalid colour type / bit depth combination";
	if (ctype == 3 && (plte.empty() || plte.size() / 3 > (1u << depth))) return "has a bad palette";
	const int bpp = (chans * depth + 7) / 8;			// bytes per pixel for the filters (at least 1)

	// the passes (Adam7 or one), their raw size, then inflate everything at once
	static const int ax[7] = { 0, 4, 0, 2, 0, 1, 0 }, ay[7] = { 0, 0, 4, 0, 2, 0, 1 },
					 dx[7] = { 8, 8, 4, 4, 2, 2, 1 }, dy[7] = { 8, 8, 8, 4, 4, 2, 2 };
	const int npass = interlace ? 7 : 1;
	unsigned int pw[7], ph[7];
	size_t total = 0;
	for (int p = 0; p < npass; p++)
	{
		pw[p] = interlace ? (w > (unsigned)ax[p] ? (w - ax[p] + dx[p] - 1) / dx[p] : 0) : w;
		ph[p] = interlace ? (h > (unsigned)ay[p] ? (h - ay[p] + dy[p] - 1) / dy[p] : 0) : h;
		if (pw[p] && ph[p]) total += (size_t)ph[p] * (1 + ((size_t)pw[p] * chans * depth + 7) / 8);
	}
	// Conservative peak: source + reserved IDAT, inflate buffer, RGBA output and a second
	// RGBA canvas copy, plus filter scratch. Check BEFORE allocating the inflate/pixel buffers.
	const size_t pixels = (size_t)w * h * 4;
	const size_t peak = data.size() * 2 + total + pixels * 2 + ((size_t)w * chans * depth + 7) / 8;
	if (peak > budget) return "exceeds the PNG decode working-memory budget";
	std::vector<unsigned char> raw(total);
	uLongf rawlen = (uLongf)total;
	if (idat.empty() || uncompress(&raw[0], &rawlen, &idat[0], (uLong)idat.size()) != Z_OK || rawlen != total)
		return "cannot be decompressed";

	image.pixels.resize(pixels);
	unsigned char *out = &image.pixels[0];
	size_t off = 0;
	for (int p = 0; p < npass; p++)
	{
		if (!pw[p] || !ph[p]) continue;
		const size_t stride = ((size_t)pw[p] * chans * depth + 7) / 8;
		std::vector<unsigned char> prev(stride, 0);
		for (unsigned int y = 0; y < ph[p]; y++)
		{
			const int filter = raw[off];
			if (filter > 4) return "has an invalid row filter";
			unsigned char *row = &raw[off + 1];
			for (size_t i = 0; i < stride; i++)
			{
				const int a = i >= (size_t)bpp ? row[i - bpp] : 0, b = prev[i], cc = i >= (size_t)bpp ? prev[i - bpp] : 0;
				switch (filter)
				{
					case 1: row[i] = (unsigned char)(row[i] + a); break;
					case 2: row[i] = (unsigned char)(row[i] + b); break;
					case 3: row[i] = (unsigned char)(row[i] + ((a + b) >> 1)); break;
					case 4: row[i] = (unsigned char)(row[i] + paeth(a, b, cc)); break;
				}
			}
			for (unsigned int x = 0; x < pw[p]; x++)
			{
				int v[4];									// samples scaled to 8 bits
				unsigned int sraw[4];						// raw samples (for tRNS)
				for (int k = 0; k < chans; k++)
				{
					unsigned int s;
					if (depth == 16) { s = (row[(x * chans + k) * 2] << 8) | row[(x * chans + k) * 2 + 1]; v[k] = s >> 8; }
					else if (depth == 8) { s = row[x * chans + k]; v[k] = s; }
					else
					{
						const size_t bit = (size_t)(x * chans + k) * depth;
						s = (row[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
						v[k] = ctype == 3 ? s : s * 255 / ((1 << depth) - 1);
					}
					sraw[k] = s;
				}
				unsigned char r, g, bl, al = 255;
				if (ctype == 3)
				{
					const unsigned int i = sraw[0];
					if (3 * i + 2 >= plte.size()) return "has an invalid palette index";
					r = 3 * i + 2 < plte.size() ? plte[3 * i] : 0;
					g = 3 * i + 2 < plte.size() ? plte[3 * i + 1] : 0;
					bl = 3 * i + 2 < plte.size() ? plte[3 * i + 2] : 0;
					if (i < trns.size()) al = trns[i];
				}
				else if (ctype == 0 || ctype == 4)
				{
					r = g = bl = (unsigned char)v[0];
					if (ctype == 4) al = (unsigned char)v[1];
					else if (trns.size() >= 2 && sraw[0] == ((unsigned)trns[0] << 8 | trns[1])) al = 0;
				}
				else
				{
					r = (unsigned char)v[0]; g = (unsigned char)v[1]; bl = (unsigned char)v[2];
					if (ctype == 6) al = (unsigned char)v[3];
					else if (trns.size() >= 6 && sraw[0] == ((unsigned)trns[0] << 8 | trns[1]) &&
							 sraw[1] == ((unsigned)trns[2] << 8 | trns[3]) && sraw[2] == ((unsigned)trns[4] << 8 | trns[5])) al = 0;
				}
				const size_t ox = interlace ? ax[p] + (size_t)x * dx[p] : x, oy = interlace ? ay[p] + (size_t)y * dy[p] : y;
				unsigned char *o = out + (oy * w + ox) * 4;
				o[0] = r; o[1] = g; o[2] = bl; o[3] = al;
			}
			memcpy(&prev[0], row, stride);
			off += 1 + stride;
		}
	}
	image.width = w; image.height = h;
	return NULL;
}


const char *omhd::decode_png(const std::string &file, DecodedPng &image, size_t budget)
{
    image.width = image.height = 0;
    image.pixels.clear();
    try {
        DecodedPng pending;
        const char *err = decode(file, pending, budget);
        if (err) return err;
        image.width = pending.width; image.height = pending.height;
        image.pixels.swap(pending.pixels);
        return NULL;
    } catch (const std::bad_alloc &) { return "could not allocate PNG working memory"; }
}
