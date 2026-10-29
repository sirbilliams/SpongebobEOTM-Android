# Neutron ports: 8888 surface on Android (colour depth / dither fix)

Symptom (SpongeBob port on the AYN Thor, gl4es on GL ES 2): a checkerboard dither over everything and harsh banding.
Cause: no `SDL_GL_RED/GREEN/BLUE/ALPHA_SIZE` is requested, so SDL's Android EGL chooser picks an RGB565 window surface, and
`GL_DITHER` is on by default. The off-screen FBO textures are RGBA8, so only the final window surface is affected.
gl4es keeps textures at 8 bits (LIBGL_AVOID16BITS defaults to 1 except on PowerVR, where it would store RGBA4), so the env guard below is belt and braces.

Apply in the SDL video bootstrap that sets the GL attributes (before `SDL_CreateWindow`), Android only:

```cpp
#ifdef __ANDROID__
	// port: SDL's Android default is an RGB565 surface (dither + banding); ask for 8888
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
#endif
```

Where the window and context are created, retry once with the defaults if 8888 is not available (EGL sizes are minimums, so this is
only a safety net):

```cpp
#ifdef __ANDROID__
	for (int pass = 0; pass < 2; pass++)
	{
		if (pass == 1)	// no 8888 config: back to RGB565
		{
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5); SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5); SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		}
		win = SDL_CreateWindow(title, x, y, w, h, flags);
		if (win) ctx = SDL_GL_CreateContext(win);
		if (win && ctx) break;
		if (win) { SDL_DestroyWindow(win); win = NULL; }
	}
#endif
```

After `SDL_GL_MakeCurrent` (any platform, harmless on 8-bit):

```cpp
	glDisable(GL_DITHER);
```

Before `initialize_gl4es()`:

```cpp
	setenv("LIBGL_AVOID16BITS", "1", 0);
	setenv("LIBGL_FORCE16BITS", "0", 0);
```

Check: log the real depth after creating the context (`SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r)` etc., expect 8/8/8/8 on the Thor).

Also check that the neutron engines do not upload textures as `GL_RGBA4` / `GL_RGB565` / `GL_RGB5_A1` (grep `glTexImage2D` internal formats; the SpongeBob engine uses GL_RGBA + GL_UNSIGNED_BYTE everywhere).

Second Thor finding (SpongeBob only, HUD): buttons drawn "faint until the pointer hovers" have no hover on a touch screen and look absent; draw the solid variant on Android. Neutron has nothing like it unless a hover-only widget exists.
