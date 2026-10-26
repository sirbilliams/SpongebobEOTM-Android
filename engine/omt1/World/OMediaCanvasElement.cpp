/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaCanvasElement.h"
#include "OMediaCanvas.h"
#include "OMediaDataBase.h"
#include "OMedia2DWorld.h"

OMediaCanvasElement::OMediaCanvasElement()		// 0x1003cea0
{
	canvas = NULL;
	canvas_handle = NULL;
	collision_mask = NULL;
}

OMediaCanvasElement::OMediaCanvasElement(OMediaCanvas *c)	// 0x1003cf00
{
	canvas_handle = NULL;
	canvas = c;
	collision_mask = NULL;
	if (c) c->element_link();
}

// 0x1003cf90
OMediaCanvasElement::~OMediaCanvasElement()
{
	presence_removed();
	if (canvas) canvas->element_unlink();
	if (collision_mask) collision_mask->element_unlink();
	delete canvas_handle;
}

// 0x1003d050
OMediaCanvas *OMediaCanvasElement::getcanvas(void)
{
	if (canvas_handle)
	{
		if (!canvas_handle->object && canvas_handle->database)
			canvas_handle->object = (OMediaCanvas*)canvas_handle->database->get_object(OMediaCanvas::db_type, canvas_handle->id);
		return canvas_handle->object;
	}
	return canvas;
}

// 0x1003d0c0
void OMediaCanvasElement::setcanvas(OMediaCanvas *c)
{
	delete canvas_handle;
	canvas_handle = NULL;
	if (canvas) canvas->element_unlink();
	canvas = c;
	if (c) c->element_link();
	moved = true;
	if (world && !superelement && ((OMedia2DWorld*)world)->get_sort_mode() != omc2dsort_Z)
		((OMedia2DWorld*)world)->set_sort_dirty();
}

// 0x1003d150
void OMediaCanvasElement::setcanvas_handle(OMediaDBHandle<OMediaCanvas> *h)
{
	setcanvas(NULL);
	if (h) canvas_handle = new OMediaDBHandle<OMediaCanvas>(*h);
}

OMediaCanvas *OMediaCanvasElement::get_collision_mask(void) { return collision_mask; }	// 0x1003d1a0

// 0x1003d1b0
void OMediaCanvasElement::set_collision_mask(OMediaCanvas *c)
{
	if (collision_mask) collision_mask->element_unlink();
	collision_mask = c;
	if (c) c->element_link();
}
