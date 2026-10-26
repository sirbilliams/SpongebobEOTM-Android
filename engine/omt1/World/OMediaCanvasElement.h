/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaCanvasElement: a 2D element showing one canvas (+0xc4), or the canvas of a database
    handle (+0xc8, loaded when first drawn); +0xcc collision mask.                              */
#pragma once
#ifndef OMEDIA1_CanvasElement_H
#define OMEDIA1_CanvasElement_H

#include "OMedia2DElement.h"

class OMediaDataBase;
class OMediaDBObject;

// A database object by id, loaded on demand (12 bytes in OMT.dll)
template <class T> class OMediaDBHandle
{
	public:
	OMediaDataBase	*database;
	T				*object;
	long			id;
	OMediaDBHandle() {database = NULL; object = NULL; id = 0;}
	OMediaDBHandle(OMediaDataBase *db, long i) {database = db; object = NULL; id = i;}
};

class OMediaCanvasElement : public OMedia2DElement
{
	public:
	omtshared OMediaCanvasElement();						// 0x1003cea0
	omtshared OMediaCanvasElement(OMediaCanvas *canvas);	// 0x1003cf00
	omtshared virtual ~OMediaCanvasElement();				// 0x1003cf90

	omtshared virtual OMediaCanvas *getcanvas(void);					// 0x1003d050
	omtshared virtual OMediaCanvas *get_collision_mask(void);			// 0x1003d1a0
	omtshared virtual void setcanvas(OMediaCanvas *c);					// 0x1003d0c0  vt+0x94
	omtshared virtual void setcanvas_handle(OMediaDBHandle<OMediaCanvas> *h);	// 0x1003d150  vt+0x98
	omtshared virtual void set_collision_mask(OMediaCanvas *c);			// 0x1003d1b0  vt+0x9c

	protected:
	OMediaCanvas					*canvas;			// +0xc4
	OMediaDBHandle<OMediaCanvas>	*canvas_handle;		// +0xc8
	OMediaCanvas					*collision_mask;	// +0xcc
};

#endif
