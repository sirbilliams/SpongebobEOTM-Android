/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaUndiv2DElement.h"
#include "OMediaCanvas.h"
#include "OMedia2DWorld.h"
#include "OMediaRefreshList.h"

// 0x1038e7c0
OMediaUndiv2DElement::OMediaUndiv2DElement()
{
	backdrop = false;
}

OMediaUndiv2DElement::~OMediaUndiv2DElement() {}		// 0x1038e810 (the 2D element's destructor does the rest)

// 0x1038eb20. Redrawn when it moved or has a backdrop; a still element without one is not erased first.
void OMediaUndiv2DElement::update(void)
{
	OMedia2DElement::update();
	no_erase = (!backdrop && !moved);
}

void OMediaUndiv2DElement::draw_full(OMediaGfxWorld *, long, long) {}		// 0x10397770

// 0x1038e830. 'src' is the part of the element to draw (NULL: all of it) at x,y of the canvas;
// with a refresh list, the parts of the list.
void OMediaUndiv2DElement::draw(OMediaRect *src, long x, long y, OMediaCanvas *dest, OMediaRefreshList *rl)
{
	if (!dest) return;
	long w, h;
	if (!rl)
	{
		if (src)
		{
			getsize(w, h);
			if (w != src->get_width() || h != src->get_height())
			{
				// a part: draw the element into a work world, copy the part
				if (!w || !h) return;
				OMediaGfxWorld *tmp = world->get_temp_buffer((short)w, (short)h);
				OMediaGfxWorld *target = dest->getdefaultdrawgfxworld();
				if (backdrop)
				{
					OMediaRect r(x, y, x + w, y + h);
					r.offset(-(target->get_origin_x() + src->left), -(target->get_origin_y() + src->top));
					dest->getdefaultdrawgfxworld()->draw(tmp, &r, 0, 0, NULL);
				}
				draw_full(tmp, 0, 0);
				tmp->draw(dest->getdefaultdrawgfxworld(), src, x, y, NULL);
				return;
			}
		}
		draw_full(dest->getdefaultdrawgfxworld(), x, y);
		return;
	}

	getsize(w, h);
	if (!w || !h) return;
	OMediaGfxWorld *tmp = world->get_temp_buffer((short)w, (short)h);
	if (backdrop)
	{
		OMediaGfxWorld *target = dest->getdefaultdrawgfxworld();
		OMediaRect r(x, y, x + w, y + h);
		r.offset(-target->get_origin_x(), -target->get_origin_y());
		dest->getdefaultdrawgfxworld()->draw(tmp, &r, 0, 0, NULL);
	}
	draw_full(tmp, 0, 0);
	OMediaGfxWorld *target = dest->getgfxworld();
	if (target) target->lock();
	for (omt_ShortRectList::iterator i = rl->get_list()->begin(); i != rl->get_list()->end(); i++)
	{
		OMediaRect piece((*i).left - x, (*i).top - y, (*i).right - x, (*i).bottom - y);
		tmp->draw(dest->getdefaultdrawgfxworld(), &piece, (*i).left, (*i).top, NULL);
	}
	if (target) target->unlock();
}
