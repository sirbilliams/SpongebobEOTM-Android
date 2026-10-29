/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaElementContainer: a list of elements (a world, or an element's sub-elements).
    OMediaElement: a positioned object of a world; elements nest (a sub-element's absolute
    position is relative to its super-element). Layout in OMT.dll (for reading sb2.exe):
      +0x00 OMediaWorldPosition (x,y,z +4..+0xc)   +0x10 OMediaElementContainer (list +0x14)
      +0x20 OMediaAutoMove                        +0x40 world  +0x44 super-element
      +0x48 container  +0x4c hide count  +0x50 absolute x,y,z  +0x5c list node
      +0x60 pause count  +0x64 hide sub-elements too  +0x65 not clickable  +0x66 delete at update
      +0x68 id                                                                                 */
#pragma once
#ifndef OMEDIA1_Element_H
#define OMEDIA1_Element_H

#include "OMediaWorldPosition.h"
#include <list>

class OMediaElement;
class OMediaWorld;
class OMediaViewPort;
class OMediaCanvas;

typedef list<OMediaElement*> omt_ElementList;

enum omt_ElementType { omcet_Element = 0, omcet_2DElement = 1, omcet_3DElement = 2, omcet_3DLight = 3 };

// What a click (or a position query) found; 0x2c bytes in OMT.dll
enum omt_ClickType { omcct_Nothing = 0, omcct_Background = 1, omcct_Canvas = 2, omcct_3DElement = 3, omcct_Element = 4 };

class OMediaWorldClickInfo
{
	public:
	int				type;			// +0x00 omt_ClickType
	OMediaViewPort	*viewport;		// +0x04
	OMediaElement	*element;		// +0x08
	int				field_c;		// +0x0c
	OMediaCanvas	*canvas;		// +0x10
	short			x, y;			// +0x14 +0x16 (viewport coordinates once found)
	short			field_18;
	short			first;			// +0x1a (1)
	int				field_1c[4];	// +0x1c .. +0x2b (3D: hit point and polygon)
	// What OMediaWorldClickDispatcher adds (0x38 bytes in all): the click as the dispatcher
	// received it, for the elements that react to it (text fields, ...)
	short			click_x, click_y;		// +0x2c +0x2e
	unsigned short	modifiers;				// +0x30 OMediaEvent modifier bits
	unsigned short	right_button;			// +0x32 1 when it was not the left button
	int				event_type;				// +0x34 OMediaEvent type
	OMediaWorldClickInfo() {click_x = click_y = 0; modifiers = 0; right_button = 0; event_type = 0; type = 0; viewport = NULL; element = NULL; field_c = 0; canvas = NULL; x = y = 0;
							field_18 = 0; first = 1; field_1c[0] = field_1c[1] = field_1c[2] = field_1c[3] = 0;}
};
typedef OMediaWorldClickInfo OMediaWorldPositionInfo;

class OMediaElementContainer
{
	public:
	omtshared OMediaElementContainer();				// 0x10062e10
	omtshared virtual ~OMediaElementContainer();	// 0x10062e60  deletes the elements

	omtshared virtual OMediaElement *search_by_id(long id);	// 0x10062f10  vt+0x04 (depth first)

	inline omt_ElementList *get_element_list(void) {return &elements;}

	protected:
	friend class OMediaElement;
	omt_ElementList		elements;		// +0x04
};

class OMediaElement : public OMediaWorldPosition, public OMediaElementContainer, public OMediaAutoMove
{
	public:

	omtshared OMediaElement();					// 0x10062890
	omtshared virtual ~OMediaElement();			// 0x10062950

	omtshared virtual omt_ElementType get_type(void) const;			// vt+0x0c
	omtshared virtual void link(OMediaElementContainer *c);			// 0x10062a60  vt+0x10
	omtshared virtual void link(OMediaElement *super);				// 0x10062a20  vt+0x14
	omtshared virtual void link(OMediaWorld *world);				// 0x100629e0  vt+0x18
	omtshared virtual void unlink(void);							// 0x10062a80  vt+0x1c
	omtshared virtual void update(void);							// 0x10062cf0  vt+0x20
	omtshared virtual float getabsoluteposx(void) const;			// 0x10062cc0  vt+0x24
	omtshared virtual float getabsoluteposy(void) const;			// 0x10062cd0  vt+0x28
	omtshared virtual float getabsoluteposz(void) const;			// 0x10062ce0  vt+0x2c
	omtshared virtual void hide(bool h);							// 0x10062d50  vt+0x30 (counted)
	omtshared virtual bool getvisible_extend(void);				// 0x10062d90  vt+0x34
	omtshared virtual void pause(bool p);							// 0x10062d70  vt+0x38 (counted)
	omtshared virtual void update_abs_position(void);				// 0x10062c20  vt+0x3c
	omtshared virtual void process_click(OMediaWorldClickInfo *ci);	// vt+0x40 (nothing)
	omtshared virtual bool collide(OMediaElement *e);				// 0x10062db0  vt+0x44

	inline OMediaWorld *getworld(void) const {return world;}
	inline OMediaElement *getsuperelement(void) const {return superelement;}
	inline OMediaElementContainer *getcontainer(void) const {return container;}
	inline bool gethide(void) const {return hide_count != 0;}
	inline int get_hide_count(void) const {return hide_count;}
	inline bool is_paused(void) const {return pause_count != 0;}
	inline long getid(void) const {return id;}
	inline void setid(long i) {id = i;}
	inline void set_hide_subelements(bool h) {hide_sub = h;}
	inline void set_clickable(bool c) {no_click = !c;}
	inline bool get_clickable(void) const {return !no_click;}
	inline void delete_at_next_update(void) {delete_flag = true;}
	inline omt_ElementList::iterator get_node(void) {return node;}
	inline void set_node(omt_ElementList::iterator n) {node = n;}

	protected:

	omtshared virtual void doplace(float nx, float ny, float nz);	// 0x10062b70
	omtshared virtual void domove(float dx, float dy, float dz);	// 0x10062bc0
	omtshared virtual void update_world_ptr(OMediaWorld *w);		// 0x10062c90  vt+0x48
	omtshared virtual void container_link(OMediaElementContainer *c);	// 0x10062ad0  vt+0x4c
	omtshared virtual void container_unlink(OMediaElementContainer *c);	// 0x10062b40  vt+0x50

	friend class OMediaWorld;

	OMediaWorld				*world;				// +0x40
	OMediaElement			*superelement;		// +0x44
	OMediaElementContainer	*container;			// +0x48
	int						hide_count;			// +0x4c
	float					absx, absy, absz;	// +0x50
	omt_ElementList::iterator	node;			// +0x5c
	int						pause_count;		// +0x60
	bool					hide_sub;			// +0x64
	bool					no_click;			// +0x65
	bool					delete_flag;		// +0x66
	long					id;					// +0x68
};

#endif
