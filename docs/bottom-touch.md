# Bottom-screen touch on the AYN Thor: diagnosis, routing, touchpad

Problem (reported on the real Thor): the companion panel on the bottom screen did not react to touch. We cannot test on the device
here, so the port logs what the hardware does and routes around the likely causes. Code: `TouchDiag`, `TouchRouter`,
`CompanionPanel`, `CompanionView` (touchpad), `TouchTestActivity`/`TouchTestView` in `android/.../spongebob/`; game side
`OMediaEventManager::pad_cursor_delta/_button`, `Companion_nativeTouchpad`.

## What can be wrong (and which variant covers it)
1. The panel window receives the touch (display id = panel display): the original code should work. Log: `panel-window` lines.
2. The firmware sends the bottom screen's touches to the *focused* window, the game Activity on the top screen ("bind Home/Back / input to
   the focus-locked screen"), with the bottom display's id, or with the top display's id but from the bottom touchscreen's input device.
   Log: `game-window` lines with `display=<panel id>` or a device that is not the top screen's. -> **routing (a)**: `TouchRouter`.
3. Nobody gets the touch (dropped by the firmware). Log: no `panel-window` and no `game-window` lines while the test is touched. -> **fallback (c)**.

## Diagnosis: what Share log shows
Launcher > Engine room > **Share log** now also sends `touch-log.txt` and `display-log.txt` (after `omt-log.txt`). Read `touch-log.txt`:
* `---- touch session (...): panel display N WxH, game display 0` then one line per input device: `name, sources=0x..., TOUCHSCREEN, descriptor,
  vid:pid, range=WxH, associatedDisplay=<id or -1>` and one per display (`<- panel`, `<- game`). Two TOUCHSCREEN devices with different
  ranges (one ~16:9, one ~1240x1080) = the two screens. `associatedDisplay=N` equal to the panel's id = the firmware binds that touchscreen to the bottom display.
* `panel-window #k ACTION_DOWN x= y= raw= display= device= src=`: the first 20 events the panel got. Seen => case 1 works (and the device is remembered as the bottom touchscreen).
* `game-window #k ...`: the first 20 events the game Activity got (every touch, top screen too). If `display=` is the panel's id or the `device` is the bottom
  touchscreen, case 2 is happening; `device 7(...) -> BOTTOM (reason)` says how the router recognised it (bound display / remembered / range aspect).
* `forwarded-to-panel #k`: the same event after mapping to the panel's pixels. Then `panel-window` should also show the forwarded press as a CompanionView target firing.
* `learned bottom touchscreen:` the descriptor kept in prefs (`bottom_touch_desc`) once the panel itself got a touch.
* No device line with TOUCHSCREEN for the bottom screen => the OS does not expose it (hardware-level): nothing to do in the app.
Quick check: **Engine room > Touch test**: a draw-where-you-touch screen on top (Activity) and bottom (Presentation made with the same flags and router) with the counters
"reached the bottom window / reached the top window / forwarded to the bottom". Touch the bottom: bottom counter rises = native; top counter + forwarded = rescued by routing; neither = case 3.

## Routing variants
* **(a) Forward (default, always on).** `SpongeActivity.dispatchTouchEvent` logs, asks `TouchRouter.isBottom`, and for a bottom touch maps it
  (identity when the event carries the panel's display id, else scaled window -> panel) and calls `CompanionView.dispatchTouchEvent`, consuming it so SDL never turns it into a game click.
  A device counts as bottom when (1) the event's display id is the panel's, (2) `InputDevice.getAssociatedDisplayId()` (hidden, reflection) is the panel's,
  (3) its descriptor was seen reaching the panel / carrying the panel's id (prefs), (4) with >= 2 touchscreens it is the one with the bottom screen's aspect (4%).
  Top-screen touches stay SDL's absolute pointer ("touch always controls the cursor").
* **(b) Panel window flags.** Default `FLAG_NOT_FOCUSABLE | FLAG_KEEP_SCREEN_ON | FLAG_SPLIT_TOUCH`: chosen as most likely to work because touch routing on Android is by display + window
  and does not need focus, while *taking* focus would (1) route gamepad keys to the panel's display and (2) make SDL pause (SDLActivity pauses when its window loses focus). The alternative, Launcher >
  Captain's settings > **Panel window: Takes focus** (`panel_focusable`), drops NOT_FOCUSABLE (adds NOT_TOUCH_MODAL) and the Presentation forwards every KeyEvent to the game Activity;
  use it only when the log shows the panel gets no events yet the touchscreen is bound to its display (focus-locked firmware). Expect the game to pause if SDL sees the focus loss: then switch back.
* **(c) Nothing arrives.** The panel keeps showing the game state (snapshot polling does not need touch); MENU/SKIP etc. just do not respond. The Touch test and log show it.

## UX: Panel and Touchpad (Settings > "Bottom screen: Panel / Touchpad", default Panel; also the PANEL | TOUCHPAD switch in the panel's header)
* **Panel**: the dashboard (hint ticket, dialogue choices / Next, pocket cards, map trail).
* **Touchpad** (layout in 1240x1080): header as in Panel (scene, MENU, SKIP); a **large pad** (1160 x ~660) as one glass surface with a dot grid that drifts with the finger; below it a slim
  **pocket row** (4 cards, paging arrows, HINT, PUT AWAY). When a dialogue line is open a slim strip with the line and **Next** sits above the pad; when a dialogue offers choices the Panel layout takes over
  (big numbered buttons: no cursor is needed).
* Pad gestures (single finger): **drag** moves the game cursor relatively, with acceleration (gain 0.45 crawling .. 2.25 flicking; ~520 .. 2600 game px across the pad's width); **tap** (< 320 ms, moved < 22 px) is a left click
  (down, up 70 ms later: the game polls the button once a frame); **press and hold** (380 ms without moving) puts the left button down (firm haptic, gold ring, "HOLDING") and the following drag moves it
  (carry an item onto a character); lifting releases. A tap on a pocket card puts the item in the cursor (`item:<id>`); the next pad click uses it.
* Feedback: pressed highlight on every target, haptic tick (`performHapticFeedback` + `Vibrator`), the system click sound.
* The path into the game: `CompanionView.Pad` -> `Companion.touchpad(dx, dy, button)` -> JNI `nativeTouchpad` -> `OMediaEventManager::pad_cursor_delta/_button` -> applied in order
  by `poll()` through the same state as the right stick (`pad_x/pad_y`, `warp`, `push_mouse`), so a stick, a mouse and the pad do not fight. Independent of `SB_PAD_CURSOR`.

## Verified here (desktop)
* `tools/companion/render.sh <snap> <out.png> [pad|pad:tap|pad:drag|pad:hold|test]` renders the layouts with the real `CompanionView.java` (and `TouchTestView.java`) and replays gestures through
  the real touch code (the shim has a one-pointer `MotionEvent`); the Pad callbacks print the cursor deltas / buttons. Pictures in `.shots/touch/`.
* `SB_TEST_TOUCHPAD="150:-2000,-2000,-1;160:427,267,-1;200:0,0,1;230:0,0,0"` in c1l5s4: the cursor is moved (clamped to the corner, then to Larry), the click starts `dialogue AAA` (smoke step `touchpad`).
* Android classes compile with javac 17 against an android-23 jar plus reflection for API 29+ calls. Not verified on the device: everything the log is meant to settle.
