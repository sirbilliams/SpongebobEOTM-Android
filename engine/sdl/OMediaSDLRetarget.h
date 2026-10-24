/*  OMT SDL platform layer - retarget objects (per-object platform data).  LGPL. */
#pragma once
#ifndef OMEDIA_SDLRetarget_H
#define OMEDIA_SDLRetarget_H

#include "OMediaSysDefs.h"
#include "OMediaRetarget.h"
#include "OMediaVideoEngine.h"
#include "OMediaOffscreenBuffer.h"
#include <string>
#include <SDL.h>

class OMediaWindow;

string omf_SDLResolvePath(const string &in);

class OMediaSDLRtgFilePath : public OMediaRetarget
{
	public:
	OMediaSDLRtgFilePath();
	virtual ~OMediaSDLRtgFilePath();
	string	path;
	void	*file;			// FILE* while open
};

class OMediaSDLRtgWindow : public OMediaRetarget
{
	public:
	OMediaSDLRtgWindow() : OMediaRetarget(omcrtg_Window) { sdlwin = NULL; window = NULL; }
	SDL_Window		*sdlwin;
	OMediaWindow	*window;

	static OMediaWindow *find(Uint32 sdl_window_id);
};

class OMediaSDLRtgSound : public OMediaRetarget
{
	public:
	OMediaSDLRtgSound() : OMediaRetarget(omcrtg_Sound)
	{ sound = NULL; sound_size = 0; pcm = NULL; pcm_length = 0; freq = 0; channels = 0; bits = 0; }
	char			*sound;			// whole RIFF image as stored in the database
	unsigned int	sound_size;
	char			*pcm;			// points into 'sound' at the data chunk
	unsigned int	pcm_length;
	int				freq, channels, bits;
};

class OMediaSDLVideoEngine : public OMediaVideoEngine
{
	public:
	OMediaSDLVideoEngine(OMediaWindow *master_window);
	virtual ~OMediaSDLVideoEngine();

	virtual OMediaVideoMode *get_current_video_mode(void);
	virtual void link(OMediaVideoCard *vcard);
	virtual void unlink(void);
	virtual void get_bounds(OMediaRect &rect) const;
	virtual void set_mode(OMediaVideoMode *mode);
	virtual void select_renderer(OMediaRendererDef*, omt_ZBufferBitDepthFlags zbuffer =omfzbc_16Bits);
	virtual void deselect_renderer(void);
	virtual void listen_to_message(omt_Message msg, void *param =NULL);
	virtual OMediaOffscreenBuffer *create_offscreen_buffer(int width, int height,
															omt_OffscreenBufferPixelFormat pixel_format);

	// Size used for the logical "screen" (fullscreen window). Defaults to the game's 640x480.
	// Set before creating the window; set_widescreen() keeps the height and matches the display aspect.
	static int logical_width, logical_height;
	static void set_widescreen(int height = 480);

	protected:
	OMediaVideoMode	*default_video_mode, *current_video_mode;
	OMediaVideoCard	*linked_card;
	void create_video_cards(void);
	void update_renderer_defs(void);
};

class OMediaSDLOffscreenBuffer : public OMediaOffscreenBuffer
{
	public:
	OMediaSDLOffscreenBuffer(OMediaVideoEngine *engine, int width, int height, omt_OffscreenBufferPixelFormat pixformat);
	virtual ~OMediaSDLOffscreenBuffer();
	virtual void lock(void);
	virtual void unlock(void);
	virtual void get_pixmap(void *&pixels, int &rowbytes);
	virtual void draw(OMediaWindow *window, int x, int y);
	protected:
	SDL_Surface	*surface;
	int			lock_count;
};

#endif
