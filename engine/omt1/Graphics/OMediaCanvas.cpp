/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaCanvas.h"
#include "OMediaGfxWorldFactory.h"
#include "OMediaDisplay.h"
#include "OMediaRefreshList.h"
#include "OMediaStreamOperators.h"
#include "OMediaError.h"

//------------------------------------------------------------------------------ converters

// OMediaCanvasConverter (0x1003bf50) / OMediaOMTConverter (0x1003c090)
class OMediaCanvasConverter
{
	public:
	OMediaCanvasConverter(OMediaStreamOperators *s, OMediaCanvas *c) {stream = s; canvas = c;}
	virtual ~OMediaCanvasConverter() {}
	virtual void convert(void) = 0;
	protected:
	OMediaStreamOperators	*stream;	// +4
	OMediaCanvas			*canvas;	// +8
};

class OMediaOMTConverter : public OMediaCanvasConverter
{
	public:
	OMediaOMTConverter(OMediaStreamOperators *s, OMediaCanvas *c) : OMediaCanvasConverter(s, c) {}

	// 0x1003c0e0: the attributes the world needs, then the world's own stream
	virtual void convert(void)
	{
		unsigned int attributes;
		*stream >> attributes;
		if (!canvas->getgfxworld()) canvas->createbestgfxworld(attributes);
		if (canvas->getgfxworld()) canvas->getgfxworld()->read_class(*stream);
	}
};

// fcn.1003bfa0: the converter for the stream's 4-byte tag. port: no GIF converter (no canvas
// of the game is a GIF; OMT.dll would build an OMediaGIFConverter for 'GIF8')
static OMediaCanvasConverter *omf_create_converter(OMediaStreamOperators *stream, OMediaCanvas *canvas)
{
	unsigned char tag[4];
	stream->read(tag, 4);
	if (tag[0] == 'O' && tag[1] == 'm' && ((tag[2] == 'C' && tag[3] == 'v') || (tag[2] == 'M' && tag[3] == 'C')))
		return new OMediaOMTConverter(stream, canvas);
	return NULL;
}

//------------------------------------------------------------------------------ OMediaCanvas

// 0x1003b350
OMediaCanvas::OMediaCanvas()
{
	gfxworld = NULL;
	height = 0;
	width = 0;
}

// 0x1003b3f0
OMediaCanvas::~OMediaCanvas()
{
	db_update();
	delete gfxworld;
}

// 0x1003b610
OMediaDBObject *OMediaCanvas::db_builder(void) { return new OMediaCanvas; }

// 0x1003b670
unsigned int OMediaCanvas::get_approximate_size(void)
{
	unsigned int s = 0x34;
	if (gfxworld) s += gfxworld->get_approximate_size();
	return s;
}

// 0x1003b500
bool OMediaCanvas::createbestgfxworld(unsigned long attributes)
{
	if (gfxworld)
	{
		delete gfxworld;
		gfxworld = NULL;
	}
	coordinate();
	setgfxworld(OMediaGfxWorldFactory::getfactory()->createbestgfxworld(attributes));
	return gfxworld != NULL;
}

// 0x1003b470
void OMediaCanvas::create(short w, short h, OMediaGfxWorld *model, OMediaRGBColor *transparent, short depth)
{
	if (gfxworld)
	{
		gfxworld->create(w, h, model, transparent, depth);
		coordinate();
	}
}

// 0x1003b6f0
void OMediaCanvas::setgfxworld(OMediaGfxWorld *w)
{
	if (gfxworld) delete gfxworld;
	gfxworld = w;
	coordinate();
}

// 0x1003b690
void OMediaCanvas::draw_full(OMediaGfxWorld *dest, short x, short y, OMediaMaskWorld *mask)
{
	if (gfxworld) gfxworld->draw_full(dest, x, y, mask);
}

// 0x1003b6c0
void OMediaCanvas::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, short x, short y, OMediaMaskWorld *mask)
{
	if (gfxworld) gfxworld->draw(dest, src_rect, x, y, mask);
}

// 0x1003b4b0
void OMediaCanvas::coordinate(void)
{
	if (!gfxworld)
	{
		height = 0;
		width = 0;
		approximate_size_changed();
		return;
	}
	width = (short)gfxworld->getwidth();
	height = (short)gfxworld->getheight();
	approximate_size_changed();
}

OMediaGfxWorld *OMediaCanvas::getdefaultdrawgfxworld(void) { return gfxworld; }		// 0x1003b4f0
bool OMediaCanvas::is_double_canvas(void) { return false; }							// 0x10069a50
bool OMediaCanvas::support_page_flipping(void) { return false; }						// 0x10069a50

// 0x1003b720
bool OMediaCanvas::require_alpha_channel(void)
{
	return gfxworld && gfxworld->get_alpha_channel();
}

// 0x1003b550
void OMediaCanvas::read_class(OMediaStreamOperators &stream)
{
	OMediaDBObject::read_class(stream);
	OMediaCanvasConverter *c = omf_create_converter(&stream, this);
	if (c)
	{
		c->convert();
		delete c;
	}
	coordinate();
}

// 0x1003b5a0. port: canvases are not written (the game never saves a database)
void OMediaCanvas::write_class(OMediaStreamOperators &)
{
	omd_EXCEPTION(omcerr_BadFormat);
}

//------------------------------------------------------------------------------ OMediaDoubleCanvas

// 0x10056840: a 1x1 second world from the screen device
OMediaDoubleCanvas::OMediaDoubleCanvas()
{
	second_world = OMediaScreenDevice::getdefaultdevice()->create_offscreen_gfxworld();
	second_world->create(1, 1, NULL, NULL, 0);
	cover.set(0, 0, 0, 0);
	full_refresh = true;
}

// 0x10056900
OMediaDoubleCanvas::~OMediaDoubleCanvas()
{
	delete second_world;		// the front world is the display's: OMediaDisplay's destructor detaches it first
}

// 0x10056980: grow the covered part to include 'r' (nothing when the front world flips pages)
void OMediaDoubleCanvas::add_coverrect(OMediaShortRect *r)
{
	if (gfxworld && gfxworld->support_page_flipping()) return;
	if (r->left >= cover.left && r->top >= cover.top && r->right <= cover.right && r->bottom <= cover.bottom) return;
	if (cover.right <= cover.left || cover.bottom <= cover.top)
	{
		set_coverrect(r);
		return;
	}
	OMediaShortRect u;
	u.left = r->left < cover.left ? r->left : cover.left;
	u.top = r->top < cover.top ? r->top : cover.top;
	u.right = r->right > cover.right ? r->right : cover.right;
	u.bottom = r->bottom > cover.bottom ? r->bottom : cover.bottom;
	set_coverrect(&u);
}

// 0x10056a60: recreate the second world for the part of the canvas 'r' covers
void OMediaDoubleCanvas::set_coverrect(OMediaShortRect *r)
{
	if (gfxworld && gfxworld->support_page_flipping()) return;
	second_world->force_unlock();
	purge_secondworld();
	short l = 0, t = 0, rr = width, b = height;
	if (r->bottom < 1 || r->right < 1 || r->top >= b || r->left >= rr)
	{
		second_world->create(1, 1, NULL, NULL, 0);
		return;
	}
	if (r->left > -1) l = r->left;
	if (r->top > -1) t = r->top;
	if (r->right < rr) rr = r->right;
	if (r->bottom < b) b = r->bottom;
	second_world->create(rr - l, b - t, NULL, NULL, 0);
	cover.set(l, t, rr, b);
	second_world->set_origin(cover.left, cover.top);
}

// 0x10056b80
void OMediaDoubleCanvas::purge_secondworld(void)
{
	second_world->purge();
	full_refresh = true;
	cover.set(0, 0, 0, 0);
}

// 0x10056bb0
void OMediaDoubleCanvas::empty_coverrect(void)
{
	purge_secondworld();
	second_world->create(1, 1, NULL, NULL, 0);
}

// 0x10056bd0
void OMediaDoubleCanvas::update_second_world_type(void)
{
	full_refresh = true;
	cover.set(0, 0, 0, 0);
	delete second_world;
	second_world = OMediaScreenDevice::getdefaultdevice()->create_offscreen_gfxworld();
	second_world->create(1, 1, NULL, NULL, 0);
}

// 0x10056c20: on a size change, clip the covered part to the new size
void OMediaDoubleCanvas::coordinate(void)
{
	if (gfxworld && gfxworld->support_page_flipping())
	{
		OMediaCanvas::coordinate();
		return;
	}
	short ow = width, oh = height;
	OMediaCanvas::coordinate();
	if (ow != width || oh != height)
	{
		if (width == 0 || height == 0) empty_coverrect();
		else if (second_world)
		{
			if (cover.bottom < 1 || cover.right < 1 || cover.top >= height || cover.left >= width) empty_coverrect();
			else
			{
				OMediaShortRect c;
				c.left = cover.left > -1 ? cover.left : 0;
				c.top = cover.top > -1 ? cover.top : 0;
				c.right = cover.right < width ? cover.right : width;
				c.bottom = cover.bottom < height ? cover.bottom : height;
				if (c.left != cover.left || c.top != cover.top || c.right != cover.right || c.bottom != cover.bottom) set_coverrect(&c);
			}
		}
		if (second_world) second_world->set_origin(cover.left, cover.top);
	}
}

// 0x10056d60. Page flipping front world: flip (or copy the refresh list back to front in the
// fine mode / the setmode flag's mode). Otherwise copy the second world into the front world:
// the refresh list's rectangles, or 'r', or all of the covered part.
void OMediaDoubleCanvas::swap(OMediaShortRect *r, OMediaRefreshList *rl, bool wait)
{
	bool flip = gfxworld && gfxworld->support_page_flipping();
	if (!second_world && !flip) return;
	if (!gfxworld) return;
	if (flip)
	{
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		if (dev->get_pageflipping_finemode() || dev->get_mode_flag()) gfxworld->back_to_front_copy(rl);
		else if (!wait) gfxworld->page_flip(false);
		return;
	}
	OMediaRect src;
	if (rl)
	{
		second_world->lock();
		for (omt_ShortRectList::iterator i = rl->get_list()->begin(); i != rl->get_list()->end(); i++)
		{
			short l = (*i).left, t = (*i).top, rr = (*i).right, b = (*i).bottom;
			if (t >= cover.bottom || l >= cover.right || b <= cover.top || rr <= cover.left) continue;
			if (l < cover.left) l = cover.left;
			if (t < cover.top) t = cover.top;
			if (rr > cover.right) rr = cover.right;
			if (b > cover.bottom) b = cover.bottom;
			src.set(l - cover.left, t - cover.top, rr - cover.left, b - cover.top);
			second_world->draw(gfxworld, &src, l, t, NULL);
		}
		second_world->unlock();
		return;
	}
	if (!r)
	{
		if (cover.right - cover.left == width && cover.bottom - cover.top == height)
		{
			second_world->draw_full(gfxworld, 0, 0, NULL);
			return;
		}
		src.set(0, 0, cover.right - cover.left, cover.bottom - cover.top);
		second_world->draw(gfxworld, &src, cover.left, cover.top, NULL);
		return;
	}
	if (r->top < cover.bottom && r->left < cover.right && cover.top < r->bottom && cover.left < r->right)
	{
		short l = cover.left > r->left ? cover.left : r->left, t = cover.top > r->top ? cover.top : r->top;
		short rr = r->right <= cover.right ? r->right : cover.right, b = cover.bottom < r->bottom ? cover.bottom : r->bottom;
		src.set(l - cover.left, t - cover.top, rr - cover.left, b - cover.top);
		second_world->draw(gfxworld, &src, l, t, NULL);
	}
}

// 0x10057080
OMediaGfxWorld *OMediaDoubleCanvas::getdefaultdrawgfxworld(void)
{
	if (gfxworld && gfxworld->support_page_flipping()) return gfxworld;
	return second_world;
}

bool OMediaDoubleCanvas::is_double_canvas(void) { return true; }		// 0x10399d40

// 0x100570a0
bool OMediaDoubleCanvas::support_page_flipping(void)
{
	return gfxworld && gfxworld->support_page_flipping();
}
