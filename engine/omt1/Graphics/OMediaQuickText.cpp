/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaQuickText.h"
#include "OMediaDrawPort.h"
#include <string.h>

// 0x10083fe0. The translation table starts as the identity; the character spacing is 2.
OMediaQuickText::OMediaQuickText()
{
	font = NULL;
	fwidth = 0;
	fheight = 0;
	vertical_delta = 0;
	space_size = 0;
	proportional = false;
	char_space = 2;
	for (int i = 0; i < 256; i++) trans_tab[i] = (unsigned char)i;
	memset(widths, 0, sizeof(widths));		// (OMT.dll leaves the widths as they are until the table is made)
}

// 0x10084050
OMediaQuickText::~OMediaQuickText()
{
	if (font) font->db_unlock();
}

// 0x100321f0
void OMediaQuickText::set_font_canvas(OMediaCanvas *c)
{
	if (font) font->db_unlock();
	font = c;
	if (c) c->db_lock();
}

// 0x10084070. A vertical delta of -1 is the character height.
void OMediaQuickText::set_font_canvas(OMediaCanvas *c, short fw, short fh, short vd)
{
	if (font) font->db_unlock();
	font = c;
	if (c) c->db_lock();
	space_size = fw;
	fwidth = fw;
	fheight = fh;
	vertical_delta = (vd == -1) ? fh : vd;
}

// 0x100840c0
void OMediaQuickText::set_translation_tab(const unsigned char *tab, short size)
{
	if (size > 0x100) size = 0x100;
	memcpy(trans_tab, tab, size);
}

// 0x100846d0
void OMediaQuickText::set_proportional_tab(short *tab, short size)
{
	if (size > 0x100) size = 0x100;
	memcpy(widths, tab, size * sizeof(short));
	proportional = true;
}

// 0x10084700. The width of a character is how far right its cell has a pixel that is not the
// background colour (a space width when the cell is empty).
void OMediaQuickText::create_proportional_tab(const OMediaRGBColor *background)
{
	OMediaDrawPort dp;
	if (!font) return;
	dp.setgfxworld(font->getgfxworld());
	dp.lock();
	short maxw = (short)font->getgfxworld()->getwidth();
	if (fwidth < maxw) maxw = fwidth;
	short y = 0;
	short n = 0;
	int top = 0;
	if (fheight <= font->getheight())
	{
		for (;;)
		{
			if (top / vertical_delta > 0xff) break;
			short last = 0;
			bool found = false;
			short w;
			if (fheight < 1) w = space_size;
			else
			{
				for (short row = 0; row < fheight; row++)
					for (short x = 0; x < maxw; x++)
					{
						OMediaRGBColor c;
						dp.readpixel(x, top + row, &c);
						if (c.red != background->red || c.green != background->green || c.blue != background->blue)
						{
							if (last < x) last = x;
							found = true;
						}
					}
				w = found ? (short)(last + 1) : space_size;
			}
			widths[top / vertical_delta] = w;
			n += vertical_delta;
			top = n;
			y = (short)top;
			if (fheight + top > font->getheight()) break;
		}
	}
	(void)y;
	dp.unlock();
	proportional = true;		// port: OMT.dll leaves the flag to the caller (set_proportional)
}

// 0x100840f0. Every character is a whole cell; fixed: the cell advances by the font width,
// proportional: a space is the space width, the others their width plus the character spacing.
void OMediaQuickText::draw_full(string text, OMediaGfxWorld *dest, short x, short y)
{
	if (!font) return;
	OMediaGfxWorld *fw = font->getgfxworld();
	if (fw) fw->lock();
	dest->lock();
	short len = (short)text.size();
	long px = x;
	if (!proportional)
	{
		for (short i = 0; i < len; i++)
		{
			int idx = trans_tab[(unsigned char)text[i]];
			int ctop = vertical_delta * idx;
			OMediaRect src(0, ctop, fwidth, ctop + fheight);
			font->draw(dest, &src, (short)px, y, NULL);
			px += fwidth - 0;
		}
	}
	else
	{
		for (short i = 0; i < len; i++)
		{
			if (text[i] == ' ') { px += space_size; continue; }
			int idx = trans_tab[(unsigned char)text[i]];
			int w = widths[idx];
			int ctop = vertical_delta * idx;
			OMediaRect src(0, ctop, w, ctop + fheight);
			font->draw(dest, &src, (short)px, y, NULL);
			px += char_space + w;
		}
	}
	if (fw) fw->unlock();
	dest->unlock();
}

// 0x10084310. The part of the text that falls inside 'clip' is drawn with the clip's corner at (x,y).
void OMediaQuickText::draw(string text, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y)
{
	if (!font) return;
	OMediaGfxWorld *fw = font->getgfxworld();
	if (fw) fw->lock();
	dest->lock();
	short tx = (short)(x - clip->left);					// where the text starts
	int ty = (short)(y - clip->top);
	int cl = clip->left + tx, ct = clip->top + ty, cr = clip->right + tx, cb = clip->bottom + ty;
	short len = (short)text.size();
	if (!proportional)
	{
		for (short i = 0; i < len; i++)
		{
			int idx = trans_tab[(unsigned char)text[i]];
			int cx0 = fwidth * i + tx, cx1 = cx0 + fwidth;
			int cy0 = ty, cy1 = ty + fheight;
			if (ct < cy1 && cl < cx1 && cy0 < cb && cx0 < cr)
			{
				int ix0 = cl > cx0 ? cl : cx0;
				int iy0 = ct > cy0 ? ct : cy0;
				int ix1 = cr < cx1 ? cr : cx1;
				int iy1 = cb < cy1 ? cb : cy1;
				OMediaRect src(ix0 - cx0, vertical_delta * idx - ty + iy0, 0, 0);
				src.right = (ix1 - ix0) + src.left;
				src.bottom = (iy1 - iy0) + src.top;
				font->draw(dest, &src, (short)ix0, (short)iy0, NULL);
			}
		}
	}
	else
	{
		int px = tx;
		for (short i = 0; i < len; i++)
		{
			int adv;
			if (text[i] == ' ') adv = space_size;
			else
			{
				int idx = trans_tab[(unsigned char)text[i]];
				int w = widths[idx];
				int cx0 = px, cx1 = px + w;
				int cy0 = ty, cy1 = ty + fheight;
				if (ct < cy1 && cl < cx1 && cy0 < cb && cx0 < cr)
				{
					int ix0 = cl > cx0 ? cl : cx0;
					int iy0 = ct > cy0 ? ct : cy0;
					int ix1 = cr < cx1 ? cr : cx1;
					int iy1 = cb < cy1 ? cb : cy1;
					OMediaRect src(ix0 - cx0, vertical_delta * idx - ty + iy0, 0, 0);
					src.right = (ix1 - ix0) + src.left;
					src.bottom = (iy1 - iy0) + src.top;
					font->draw(dest, &src, (short)ix0, (short)iy0, NULL);
				}
				adv = char_space + w;
			}
			px += adv;
		}
	}
	if (fw) fw->unlock();
	dest->unlock();
}

// 0x100848e0. Proportional: widths added, the character spacing between characters only.
short OMediaQuickText::get_text_length(string text)
{
	short len = (short)text.size();
	if (!proportional) return (short)(fwidth * len);
	short total = 0;
	for (short i = 0; i < len; i++)
	{
		if (text[i] == ' ') total = (short)(total + space_size);
		else
		{
			total = (short)(total + widths[trans_tab[(unsigned char)text[i]]]);
			if ((unsigned)i != text.size() - 1) total = (short)(total + char_space);
		}
	}
	return total;
}
