/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaCCaption.h"
#include "OMediaQuickText.h"
#include "OMedia2DWorld.h"
#include "OMediaCanvas.h"

// 0x1003dc10
OMediaCCaption::OMediaCCaption()
{
	font = NULL;
	width = 0;
	height = 0;
}

OMediaCCaption::~OMediaCCaption() {}		// 0x1003dc80

void OMediaCCaption::set_string(const string &s)
{
	text = s;
	compute_size();
	moved = true;
}

void OMediaCCaption::set_font(OMediaQuickText *f)
{
	font = f;
	compute_size();
	moved = true;
}

// 0x1003ded0: the string's length in the font, the font's height
void OMediaCCaption::compute_size(void)
{
	if (!font) return;
	if (text.size())
	{
		width = font->get_text_length(text);
		height = font->get_font_height();
		return;
	}
	height = 0;
	width = 0;
}

// 0x1003dcf0
void OMediaCCaption::getsize(long &w, long &h)
{
	w = width;
	h = height;
}

// 0x1003dd10. 'src' is the part to draw (NULL: all) at x,y; with a refresh list, its pieces.
void OMediaCCaption::draw(OMediaRect *src, long x, long y, OMediaCanvas *dest, OMediaRefreshList *rl)
{
	if (!dest || !font) return;
	if (!rl)
	{
		if (src)
		{
			if (width == src->get_width() && height == src->get_height())
				font->draw_full(text, dest->getdefaultdrawgfxworld(), (short)x, (short)y);
			else
				font->draw(text, dest->getdefaultdrawgfxworld(), src, (short)x, (short)y);
			return;
		}
		font->draw_full(text, dest->getdefaultdrawgfxworld(), (short)x, (short)y);
		return;
	}
	OMediaGfxWorld *target = dest->getgfxworld();
	if (target) target->lock();
	for (omt_ShortRectList::iterator i = rl->get_list()->begin(); i != rl->get_list()->end(); i++)
	{
		OMediaRect piece((*i).left - x, (*i).top - y, (*i).right - x, (*i).bottom - y);
		font->draw(text, dest->getdefaultdrawgfxworld(), &piece, (*i).left, (*i).top);
	}
	if (target) target->unlock();
}
