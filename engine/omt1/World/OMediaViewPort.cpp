/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaViewPort.h"
#include "OMediaCanvas.h"

//------------------------------------------------------------------------------ OMediaVPModel

// 0x1038f6e0
OMediaVPModel::OMediaVPModel(OMediaWorld *w, OMediaViewPort *vp)
{
	world = w;
	viewport = vp;
	buffer = NULL;
	work = NULL;
	full_redraw = true;
}

// 0x1038f730
OMediaVPModel::~OMediaVPModel()
{
	if (buffer)
	{
		if (buffer->getgfxworld()) buffer->getgfxworld()->unlock();
		delete buffer;
	}
	free(work);
}

// 0x1038f780: a model that draws nothing still swaps the double canvas
void OMediaVPModel::render(void)
{
	OMediaDoubleCanvas *dc = viewport->getdoublecanvas();
	if (dc)
	{
		OMediaRect r = viewport->get_bounds();
		world->add_new_swapcanvas(dc, &r, NULL, dc->support_page_flipping(), false);
	}
}

void OMediaVPModel::inform_vpchanged(void) {}

// 0x1038f7e0
void OMediaVPModel::inform_vpsizechanged(void)
{
	if (buffer) delete buffer;
	buffer = NULL;
}

long OMediaVPModel::getmaxvpwidth(void) { return 0; }
long OMediaVPModel::getmaxvpheight(void) { return 0; }
void OMediaVPModel::test_click(OMediaWorldClickInfo *) {}

//------------------------------------------------------------------------------ OMediaViewPort

// 0x1038ec00. Joins the world's viewports; bounds 0,0,64,64; the world makes the model.
OMediaViewPort::OMediaViewPort(OMediaWorld *w)
{
	navigator_mode = false;
	world = w;
	w->viewports.push_back(this);
	node = --w->viewports.end();
	canvas = NULL;
	double_canvas = NULL;
	pause_count = 0;
	bounds.set(0, 0, 64, 64);
	vpmodel = w->create_vp_model(this);
}

// 0x1038ed60. OMT.dll first broadcasts -603 (viewport deleted) on the viewport's broadcaster.
OMediaViewPort::~OMediaViewPort()
{
	delete vpmodel;
	world->viewports.erase(node);
	unlock_canvas();
}

// 0x1038ee70
void OMediaViewPort::setbounds(OMediaRect *r)
{
	bool size_changed = !(r->get_width() == bounds.get_width() && r->get_height() == bounds.get_height());
	bounds = *r;
	vpmodel->inform_vpchanged();
	if (size_changed) vpmodel->inform_vpsizechanged();
}

// 0x1038ef00
void OMediaViewPort::doplace(float nx, float ny, float nz)
{
	x = nx; y = ny; z = nz;
	vpmodel->inform_vpchanged();
}

// 0x1038ef20
void OMediaViewPort::domove(float dx, float dy, float dz)
{
	x = dx + x; y = dy + y; z = dz + z;
	vpmodel->inform_vpchanged();
}

// 0x1038ef50
void OMediaViewPort::render(void)
{
	if (canvas) vpmodel->render();
}

// 0x1038ef70
void OMediaViewPort::set_angle(short ax, short ay, short az)
{
	if (navigator_mode) OMedia3DNavigator::set_angle(ax, ay, az);
	else OMediaWorldAngle::set_angle(ax, ay, az);
}

// 0x1038efb0. Keeps the bounds inside the canvas and the model's maximum size, tells the double
// canvas which part it covers, then the automatic move/rotation (or the navigator).
void OMediaViewPort::update(void)
{
	if (canvas)
	{
		if (bounds.left < 0) bounds.left = 0;
		if (bounds.top < 0) bounds.top = 0;
		if (canvas->getwidth() < bounds.right) bounds.right = canvas->getwidth();
		if (canvas->getheight() < bounds.bottom) bounds.bottom = canvas->getheight();
	}
	long mw = vpmodel->getmaxvpwidth(), mh = vpmodel->getmaxvpheight();
	if (mw && mw < bounds.right - bounds.left) bounds.right = mw + bounds.left;
	if (mh && mh < bounds.bottom - bounds.top) bounds.bottom = bounds.top + mh;
	if (double_canvas)
	{
		OMediaShortRect r((short)bounds.left, (short)bounds.top, (short)bounds.right, (short)bounds.bottom);
		double_canvas->add_coverrect(&r);
	}
	if (pause_count) return;
	if (navigator_mode)
	{
		OMedia3DNavigator::update(this, this, this, this, world->get_timer());
		return;
	}
	float dx, dy, dz;
	if (automove_update(dx, dy, dz, world->get_timer())) domove(dx, dy, dz);
	short ax, ay, az;
	if (autorotate_update(ax, ay, az, world->get_timer()))
		set_angle(anglex + ax, angley + ay, anglez + az);
}

// 0x1038f190
void OMediaViewPort::setcanvas(OMediaCanvas *c)
{
	unlock_canvas();
	canvas = c;
	double_canvas = (c && c->is_double_canvas()) ? (OMediaDoubleCanvas*)c : NULL;
	vpmodel->inform_vpchanged();
	lock_canvas();
}

// 0x1038f1e0
void OMediaViewPort::lock_canvas(void)
{
	if (canvas && canvas->getgfxworld()) canvas->getgfxworld()->lock();
	if (double_canvas) double_canvas->getdefaultdrawgfxworld()->lock();
}

// 0x1038f220
void OMediaViewPort::unlock_canvas(void)
{
	if (canvas && canvas->getgfxworld()) canvas->getgfxworld()->unlock();
	if (double_canvas) double_canvas->getdefaultdrawgfxworld()->unlock();
}

// 0x1038f260
void OMediaViewPort::pause(bool p)
{
	if (p) pause_count++;
	else pause_count--;
}
