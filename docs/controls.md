# Controls (touch, gamepad cursor)

Touch is the main control on the AYN Thor; the gamepad is a pointer. Code: `engine/omt1/Input/OMediaEventManager.cpp`
(`OMediaEventManager::poll`, the event source of the menu loop, game loop and `spend_time`; it replaces `SDL_PollEvent`).

## Touch
- One path: SDL's own touch->mouse events are off (`SDL_HINT_TOUCH_MOUSE_EVENTS=0`, `SDL_HINT_MOUSE_TOUCH_EVENTS=0`, and any mouse event with
  `which == SDL_TOUCH_MOUSEID` is dropped), so a tap is exactly one click.
- `poll` turns the first finger into mouse events: finger down = motion + left button down at the finger, finger motion = motion (a drag, with the
  button held), finger up = left button up. The system pointer is warped to the finger, so everything that polls the pointer (the cursor sprite,
  hover captions, menu highlight, the fill mode's auto-pan, `OMediaInputDriver::get_mouse_position`) follows it. A second finger is ignored.
- Window -> logical mapping is `omgl_window_to_logical`, which covers fill / panels / stretch.
- The cursor sprite stays at the last touch point.

## Right-stick cursor (launcher toggle, default ON: nothing else in the game reads the pad)
- Android: launcher button "Right-stick cursor: On/Off" (`SetupActivity`, pref `settings.pad_cursor`), passed by `SpongeActivity` as env
  `SB_PAD_CURSOR=1/0`. Desktop reads `SB_PAD_CURSOR` (unset = on, `0` = off).
- Right stick moves the pointer: 20% radial dead zone, square acceleration curve, 3/4 of the logical width (480 px) per second at full tilt,
  clamped to the logical screen; the pointer is warped (window = `omgl_logical_to_window`), so it works in every widescreen mode and the fill mode
  pans to follow it. Moving a real mouse or a finger re-syncs the pad position.
- `SB_WIDESCREEN=panels`: the pad pointer is clamped to the 640x480 picture and cannot enter the side panels. That is intended: the panels are decoration
  with no click targets (a mouse or finger click there is outside the logical screen and does nothing). The cursor sprite is a world element of the 640x480
  picture, so at the right / bottom edge `Cursor::update` keeps it fully inside the picture (`// port:`; the click point stays at the pointer).
  Verified at 1920x1080 with `SB_TEST_PAD=40:rx=1,ry=1` (sprite at the picture's bottom-right corner, not cut).
- R3 (`SDL_CONTROLLER_BUTTON_RIGHTSTICK`) and A (`SDL_CONTROLLER_BUTTON_A`) are both the left mouse button: press = button down, release = up
  (so dragging an item from the HUD bar works). Either held keeps it down. The repo has no A/B swap; SDL's A is used as reported
  (Thor: the button labelled A). The polled state is reported through `OMediaEventManager::pad_button_down()` to `mouse_down()`.
- Controllers are opened at the first poll and on `SDL_CONTROLLERDEVICEADDED` (hot-plug), closed on removal.
- Nothing else on the pad is mapped (B, X, Y, triggers, d-pad, L3 do nothing; movies still skip on any controller button). Esc / F10 as before. Android Back: a single short tap still leaves to the game menu (0.35 s after the release); hold it 0.6 s or tap it twice (desktop: hold F9) for the port menu, docs/port-menu.md.

## Tests (desktop only)
- `SB_TEST_TOUCHPAD="frame:dx,dy,btn;..."` the bottom-screen touchpad (relative move in logical px, button 0/1/-1; `OMediaEventManager::pad_cursor_delta/_button`, docs/bottom-touch.md).
- `SB_TEST_PAD="frame:rx=<-1..1>,ry=<..>,r3=<0|1>,a=<0|1>;..."` fake pad (values go through the same dead zone / curve), `SB_TEST_TOUCH="frame:d|m|u:x:y,..."`
  synthetic `SDL_FINGERDOWN/MOTION/UP` at logical x,y (pushed into SDL's queue). Frames count `poll` cycles of the process (about = `OMT_SHOTS` frames).
  `OMT_PAD_LOG=1` logs the generated pointer events. With these hooks the pad tick is a fixed 1/60 s.
- Verified: `SB_TEST_NEWGAME=1 SB_SKIP_MOVIES=1 SB_TEST_SCENE=c1l5s4 SB_TEST_ITEMS=...` fill mode 1280x540: R3 click on Larry starts his dialogue; A held while
  moving drags the Urchin Chip Pie from the bar onto Larry ("accepted"); a finger tap on Larry starts the dialogue; ASan/UBSan clean (fill, panels).

## Reporting a problem (AYN Thor)

1. Reproduce it, then quit the game (back to the launcher). The game writes `omt-log.txt` (SDL_Log output plus
   a native crash backtrace) in `Android/data/com.sirbilliams.spongebob/files/`; the run before is kept as
   `omt-log.prev.txt`.
2. Launcher: Test > "Share log". The share sheet gets the app version (versionName + git commit), device and
   Android info, and the current and previous log as text. Send it to Bill.
3. Test > "App version" shows the version/commit; Test > "Reset settings" restores the launcher options
   (widescreen, cursor, panel) without touching saves or the installed game.
