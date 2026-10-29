/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaElement.h"
#include "OMediaWorld.h"

//------------------------------------------------------------------------------ OMediaElementContainer

OMediaElementContainer::OMediaElementContainer() {}		// 0x10062e10

// 0x10062e60: an element's destructor unlinks it from here
OMediaElementContainer::~OMediaElementContainer()
{
	while (elements.size()) delete elements.front();
}

// 0x10062f10
OMediaElement *OMediaElementContainer::search_by_id(long id)
{
	for (omt_ElementList::iterator i = elements.begin(); i != elements.end(); i++)
	{
		if ((*i)->getid() == id) return *i;
		OMediaElement *e = (*i)->search_by_id(id);
		if (e) return e;
	}
	return NULL;
}

//------------------------------------------------------------------------------ OMediaElement

// 0x10062890
OMediaElement::OMediaElement()
{
	world = NULL;
	superelement = NULL;
	hide_count = 0;
	absz = absy = absx = 0;
	container = NULL;
	pause_count = 0;
	hide_sub = true;
	id = 0;
	no_click = false;
	delete_flag = false;
}

// 0x10062950
OMediaElement::~OMediaElement()
{
	OMediaElement::unlink();
}

omt_ElementType OMediaElement::get_type(void) const { return omcet_Element; }

// 0x100629e0
void OMediaElement::link(OMediaWorld *w)
{
	unlink();
	container = NULL;
	container_link(w);
	superelement = NULL;
	update_world_ptr(w);
	update_abs_position();
}

// 0x10062a20
void OMediaElement::link(OMediaElement *super)
{
	unlink();
	container = NULL;
	container_link(super);
	superelement = super;
	update_world_ptr(super->world);
	update_abs_position();
}

// 0x10062a60: in a plain container (not drawn, not updated by a world)
void OMediaElement::link(OMediaElementContainer *c)
{
	container_link(c);
	container = c;
}

// 0x10062a80
void OMediaElement::unlink(void)
{
	if (container)
	{
		container_unlink(container);
		container = NULL;	// port: OMT.dll leaves it set, so a destructor's second unlink would unlink again
		return;
	}
	if (superelement) container_unlink(superelement);
	else if (world) container_unlink(world);
	else
	{
		update_world_ptr(NULL);
		superelement = NULL;
		return;
	}
	update_world_ptr(NULL);
	superelement = NULL;
}

// 0x10062ad0
void OMediaElement::container_link(OMediaElementContainer *c)
{
	c->elements.push_back(this);
	node = --c->elements.end();
}

// 0x10062b40
void OMediaElement::container_unlink(OMediaElementContainer *c)
{
	c->elements.erase(node);
}

// 0x10062b70
void OMediaElement::doplace(float nx, float ny, float nz)
{
	if (x != nx || y != ny || z != nz)
	{
		x = nx; y = ny; z = nz;
		update_abs_position();
	}
}

// 0x10062bc0
void OMediaElement::domove(float dx, float dy, float dz)
{
	if (dx != 0.0f || dy != 0.0f || dz != 0.0f)
	{
		x = dx + x; y = dy + y; z = dz + z;
		update_abs_position();
	}
}

// 0x10062c20
void OMediaElement::update_abs_position(void)
{
	if (!superelement)
	{
		absx = x; absy = y; absz = z;
	}
	else
	{
		absx = superelement->getabsoluteposx() + x;
		absy = superelement->getabsoluteposy() + y;
		absz = superelement->getabsoluteposz() + z;
	}
	for (omt_ElementList::iterator i = elements.begin(); i != elements.end(); i++) (*i)->update_abs_position();
}

// 0x10062c90
void OMediaElement::update_world_ptr(OMediaWorld *w)
{
	world = w;
	for (omt_ElementList::iterator i = elements.begin(); i != elements.end(); i++) (*i)->update_world_ptr(w);
}

float OMediaElement::getabsoluteposx(void) const { return absx; }		// 0x10062cc0
float OMediaElement::getabsoluteposy(void) const { return absy; }		// 0x10062cd0
float OMediaElement::getabsoluteposz(void) const { return absz; }		// 0x10062ce0

// 0x10062cf0: the automatic move, with the world's timer
void OMediaElement::update(void)
{
	if (world && pause_count == 0)
	{
		float dx, dy, dz;
		if (automove_update(dx, dy, dz, world->get_timer())) domove(dx, dy, dz);
	}
}

// 0x10062d50
void OMediaElement::hide(bool h)
{
	hide_count += h ? 1 : -1;
}

// 0x10062d70
void OMediaElement::pause(bool p)
{
	if (p) pause_count++;
	else pause_count--;
}

// 0x10062d90: visible when it and all its super-elements are shown
bool OMediaElement::getvisible_extend(void)
{
	OMediaElement *e = this;
	for (;;)
	{
		if (e->hide_count) return false;
		e = e->superelement;
		if (!e) return true;
	}
}

void OMediaElement::process_click(OMediaWorldClickInfo *) {}
bool OMediaElement::collide(OMediaElement *) { return false; }		// 0x10062db0
