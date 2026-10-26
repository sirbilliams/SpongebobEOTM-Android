/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaCanvas: a database object ('Canv') that owns a gfx world. Its stream is a 4-byte
    format tag ('OmCv'/'OmMC': OMT, 'GIF8': GIF) then the converter's data; the OMT converter
    reads the attributes, asks the factory for the best world and streams the world in.
    OMediaDoubleCanvas: the screen canvas (front world = the display's, second world = a back
    buffer from the screen device, unless the front world flips pages itself).                  */
#pragma once
#ifndef OMEDIA1_Canvas_H
#define OMEDIA1_Canvas_H

#include "OMediaDBObject.h"
#include "OMediaGfxWorld.h"

class OMediaRefreshList;

class OMediaCanvas : public OMediaDBObject
{
	public:

	enum { db_type = 'Canv' };

	omtshared OMediaCanvas();					// 0x1003b350
	omtshared virtual ~OMediaCanvas();			// 0x1003b3f0

	omtshared static OMediaDBObject *db_builder(void);		// 0x1003b610

	omtshared virtual unsigned int get_approximate_size(void);	// 0x1003b670  vt+0x00

	omtshared virtual bool createbestgfxworld(unsigned long attributes);			// 0x1003b500  vt+0x14
	omtshared virtual void create(short width, short height, OMediaGfxWorld *model = NULL,
								  OMediaRGBColor *transparent = NULL, short depth = 0);	// 0x1003b470  vt+0x18
	omtshared virtual void setgfxworld(OMediaGfxWorld *w);							// 0x1003b6f0  vt+0x1c
	omtshared virtual void draw_full(OMediaGfxWorld *dest, short x, short y, OMediaMaskWorld *mask = NULL);	// 0x1003b690  vt+0x20
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, short x, short y,
								OMediaMaskWorld *mask = NULL);										// 0x1003b6c0  vt+0x24
	omtshared virtual void coordinate(void);										// 0x1003b4b0  vt+0x28
	omtshared virtual OMediaGfxWorld *getdefaultdrawgfxworld(void);				// 0x1003b4f0  vt+0x2c
	omtshared virtual bool is_double_canvas(void);								// 0x10069a50  vt+0x30
	omtshared virtual bool require_alpha_channel(void);							// 0x1003b720  vt+0x34
	omtshared virtual bool support_page_flipping(void);							// 0x10069a50  vt+0x38

	omtshared virtual void read_class(OMediaStreamOperators &stream);			// 0x1003b550
	omtshared virtual void write_class(OMediaStreamOperators &stream);			// 0x1003b5a0

	inline OMediaGfxWorld *getgfxworld(void) {return gfxworld;}
	inline void detach_gfxworld(void) {gfxworld = NULL;}
	inline short getwidth(void) const {return width;}
	inline short getheight(void) const {return height;}

	// The element/frame reference count OMT.dll keeps inline: lock the world and the object
	inline void element_link(void) {if (gfxworld) gfxworld->lock(); db_lock();}
	inline void element_unlink(void) {if (gfxworld) gfxworld->unlock(); db_unlock();}

	protected:

	OMediaGfxWorld	*gfxworld;		// +0x24
	short			width;			// +0x28
	short			height;			// +0x2a
};

class OMediaDoubleCanvas : public OMediaCanvas
{
	public:

	omtshared OMediaDoubleCanvas();				// 0x10056840
	omtshared virtual ~OMediaDoubleCanvas();	// 0x10056900

	omtshared virtual void coordinate(void);								// 0x10056c20  vt+0x28
	omtshared virtual OMediaGfxWorld *getdefaultdrawgfxworld(void);		// 0x10057080  vt+0x2c
	omtshared virtual bool is_double_canvas(void);						// 0x10399d40  vt+0x30
	omtshared virtual bool support_page_flipping(void);					// 0x100570a0  vt+0x38
	omtshared virtual void add_coverrect(OMediaShortRect *r);				// 0x10056980  vt+0x3c
	omtshared virtual void set_coverrect(OMediaShortRect *r);				// 0x10056a60  vt+0x40
	omtshared virtual void empty_coverrect(void);							// 0x10056bb0  vt+0x44
	omtshared virtual void swap(OMediaShortRect *r, OMediaRefreshList *rl, bool wait);	// 0x10056d60  vt+0x48
	omtshared virtual void purge_secondworld(void);						// 0x10056b80  vt+0x4c
	omtshared virtual void update_second_world_type(void);				// 0x10056bd0  vt+0x50

	inline OMediaGfxWorld *getsecondworld(void) {return second_world;}
	inline bool get_full_refresh(void) const {return full_refresh;}
	inline void set_full_refresh(bool f) {full_refresh = f;}

	protected:

	OMediaGfxWorld	*second_world;	// +0x2c
	OMediaShortRect	cover;			// +0x30  the part of the canvas the second world covers
	bool			full_refresh;	// +0x38  the second world was (re)created: the next swap copies everything
};

#endif
