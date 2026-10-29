# Port menu, diagnostics overlay, lid suspend / resume (lane c)

Three port features (nothing of this is in sb2.exe). Code: `game/src/port/PortMenu.{h,cpp}`, `PortGfx.{h,cpp}` (+ `PortFontData.inc`),
`Diagnostics.cpp`, `android/.../PortPrefs.java`; engine hooks in `engine/sdl/OMediaSDLGfx.cpp` and `engine/omt1/Input/OMediaEventManager.cpp`.

## Port menu

Open it while playing (the menu over the main menu or a movie is not offered):

| Input | Result |
|---|---|
| hold Android Back 0.6 s | menu opens |
| tap Back twice within 0.35 s | menu opens |
| one short Back | what it always did: the game menu (F10), now 0.35 s after the release (the wait for a possible second tap) |
| desktop: hold F9 | menu opens (a short F9 does nothing; F10 is still the game menu at once) |

The game stands still behind it (`game->update` is skipped; the picture, widescreen pan and mixer keep running). Closing: Resume, B, Back, Esc.
The menu is drawn in code over the presented picture (`omgl_set_overlay` hook in `omgl_present`: window pixels, a 1920x1080 layout scaled to
fit, so it looks the same at any size and is in screenshots): a rope frame round a timber board, the launcher's teal pills, Liberation Sans Bold text
from the embedded atlas. No show art.

Rows (d-pad up/down moves, left/right changes, tap / click / Enter / right = next value):

| Row | Values | Applies | Saved as (the launcher's keys) |
|---|---|---|---|
| Bottom screen | Panel / Touchpad / Off | at once (`CompanionPanel.portApply`: shows / hides the panel, switches its mode) | `spongebob`: `bottom_panel`, `bottom_mode` |
| Cursor theme | Spatula / Net / Bubble / Classic | at once (`Cursor::setTheme`) | `settings.cursor` (0..3) |
| Widescreen | Fill / Side panels / Stretch / Off | at once (`SB_WIDESCREEN` is read per frame) | `settings.widescreen` (0..3) |
| Right-stick cursor | On / Off | at once (`OMediaEventManager::set_pad_cursor`) | `settings.pad_cursor` |
| Diagnostics | On / Off | at once | `settings.diagnostics` (new) |
| Resume | | closes | |
| Game menu | | closes, then the original menu as F10 | |

Persistence: the native side calls `PortPrefs.load(Activity)` once at start (initial values) and `PortPrefs.save(...)` on every change through JNI
(`SDL_AndroidGetActivity`, class found through the activity's class loader). `PortPrefs` writes the same SharedPreferences files and keys as
`SetupActivity`, so the launcher shows the same values afterwards. `SpongeActivity.mPanel` is package-private and `CompanionPanel.portApply()` was
added (both marked lane c). The launcher (`LauncherView`/`SetupActivity`) was only read. Desktop has no preferences: the starting values come from
`SB_WIDESCREEN`, `SB_CURSOR`, `SB_PAD_CURSOR`, `SB_BOTTOM=panel|touchpad|off`, `SB_DIAG=1`, and a change is applied to the process environment and
logged ("would save ...").

Input while open: the menu takes keys, the pad's d-pad / B and the pointer (finger, mouse, right stick; A / R3 click where the pointer is). The game
sees none of it, and nothing is stuck when it closes.

## Diagnostics overlay

Corner box (top left) when on: FPS and frame time (avg, max of the last 0.5 s), resident memory (`/proc/self/statm`), GL framebuffer bits (the
`OMT: framebuffer r/g/b/a` value), bottom-screen state (panel / touchpad / off, touchpad events received: `OMediaEventManager::touchpad_events`), scene
name. While on it also logs one line every 10 s (`diag: FPS ..., framebuffer 8/8/8/0, ...`), which is in `omt-log.txt` on Android. It is drawn in every
presented frame (menu loops too), not in the game surface.

## Lid suspend / resume

Android `onPause` (lid close, home, screen off, another app) reaches the engine as SDL's `SDL_APP_WILLENTERBACKGROUND`; the desktop equivalent is
`SDL_WINDOWEVENT_MINIMIZED`. `OMediaEventManager` (omt1) handles them in an SDL event watch, i.e. the instant SDL posts them (on Android SDL then
blocks the game thread in its event pump until the app is back, so the queue would be too late):

- the lifecycle hook (`PortMenu.cpp lifeHook`) pauses the mixer (`omf_SDLAudioPause`, the only mixer API used: voices, music and the Bink
  streams keep their place) and stops OMT's millisecond clock (`OMediaTimeCounter::pause_millisecs`);
- held input is released (a finger or R3 / A button that was down is sent as a button up, pad pointer state is dropped);
- `poll()` does not return while suspended (desktop: it sleeps in `SDL_WaitEventTimeout`), so the game loop, scripts and the scene stand still;
- on `SDL_APP_DIDENTERFOREGROUND` / `SDL_WINDOWEVENT_RESTORED` the mixer and the clock go on, `OMediaEventManager::resume_count()` changes and the game
  loop restarts its frame clock (`dt` is 16 ms on the first frame, no jump). SDL_QUIT while suspended ends the wait.
- GL context loss: SDL (2.0.x Android) keeps the EGL context across a pause where the driver allows it and otherwise creates a new one and posts
  `SDL_RENDER_DEVICE_RESET` (also `SDL_RENDER_TARGETS_RESET`). That bumps a context epoch (`omgl_context_reset`): every surface (FBO + texture + depth),
  memory-world texture and 3D texture remembers the epoch it was made in and is made again, lazily, the next time it is used; the old ids are not
  deleted (they died with the context). The game then repaints everything (`Game::forceRedraw`), the font atlas of the overlay is re-uploaded.
  **Not tested on a device.** What is not covered: gl4es's own state (its shader / program cache) is not rebuilt; if the driver really loses the
  context, programs gl4es linked in the old one are stale. If that is seen on the Thor, the fix is to re-create gl4es's state
  (`NewGLState` / `ActivateGLState`) in `omgl_context_reset`.
- The movie loop (`play_movie`) polls SDL directly and is not suspended by this (SDL's Android pump still blocks it and pauses its audio).

## Tests (desktop, `SB_TEST_*`; all through the real event path)

- `SB_TEST_BACK="n:d|u,..."`, `SB_TEST_F9=...`: Android Back / F9 down / up at game-loop iteration n (real time passes between iterations: about 50 iterations = 0.6 s).
  Verified: hold (40:d,100:u) opens the menu; double tap (40:d,42:u,50:d,52:u) opens it; one tap (40:d,42:u) opens the game menu; F9 hold opens it.
- `SB_TEST_PM="n:open|close|up|down|left|right|enter,..."`: drives the menu.
- `SB_TEST_SUSPEND="frame:ms[m],..."` (frames = event-poll frames): the app goes to the background at that frame and is back after ms (`m`: window
  minimise / restore events instead of the Android app events). Log lines: `port: suspended`, `mixer paused (device paused)`, `resumed after N ms`,
  `mixer resumed (device playing)`, `resume: game frame N, scene S, frame clock restarted`; a held R3 at the time is released (`pointer: ... type 1026`).
- `SB_TEST_GLRESET="frame,..."`: posts `SDL_RENDER_DEVICE_RESET`; the picture comes back intact (screenshot at 1920x1080).
- `SB_DIAG=1` with `OMT_SHOTS` / `OMT_SCREENSHOT` for the overlay and menu PNGs.
