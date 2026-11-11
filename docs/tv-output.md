# Play on TV

Captain's settings > **TV output: Auto / Off** (`settings` / `tv_output`, boolean,
default Auto). Connect HDMI/USB-C DisplayPort or connect a wireless display using
Android's display settings; the app does not discover receivers or initiate casting.

`TvOutput` watches DisplayManager during the game. It chooses the lowest-id public,
valid presentation display, excluding display 0, `BottomScreen.find()`'s Thor bottom
panel, internal displays and overlays. Display type uses the existing reflection /
description fallback; wireless public virtual presentation displays are eligible.
Adds, removals, task moves and failures go to `display-log.txt` (Share log).
The listener is removed when the game Activity is destroyed.

On API 26+, Auto relaunches the **existing task** with NEW_TASK / SINGLE_TOP and
ActivityOptions.setLaunchDisplayId. SpongeActivity has a separate task affinity,
singleTask, resizeableActivity=true, and no picture-in-picture. Configuration changes
stay in the same SDL Activity; the native game is never deliberately destroyed.
The live scene, script, inventory, movie and audio state stay in memory, using SDL's
pause/resume lifecycle, as lid suspend does (PortMenu::lifeHook pauses clock/audio).
There is no disk checkpoint or restart-from-save: process death has the usual limits.
Unplugging returns the same task to display 0; Off also targets display 0.
See [Android's launch-display API](https://developer.android.com/reference/android/app/ActivityOptions#setLaunchDisplayId(int)).

While playing on TV, a non-focusable full-screen Presentation on Thor's top display
contains CompanionView in Touchpad mode and a **Playing on TV** label. Bottom-screen
companion selection/preferences stay independent. Neither TV helper window takes
controller focus; top touches delivered to the game window can be forwarded to the pad.

If secondary Activities are unsupported, launching throws, or the task stays on its
old display after one second, the app logs why and falls back to a TV Presentation.
PixelCopy reads SDL's SurfaceView (including GLES output), scales to at most 1280 px
wide, and updates an aspect-preserving ImageView at at most 15 fps. The game stays on
top with a touchpad overlay. This avoids restarting SDL on restrictive firmware;
extra latency and copy cost make it less smooth. A refused target is retried after
display add/remove or a new game run. Copy failures are logged. No native changes.

## Device checks (not verified here)
- Auto: start with HDMI connected, then connect during play; TV shows the game,
  top shows the pad, bottom keeps its panel. Confirm task/Activity survives in logs.
- Move in a scene, dialogue, cutscene/movie and menu; verify position, inventory,
  dialogue/script and audio continue. Test pad drag/tap/hold and all gamepad controls.
- Unplug, reconnect repeatedly, change TV resolution; game returns to top without
  restarting. Background/resume, close/open lid; no stale windows or held mouse button.
- Off: connect before/during play; game stays on top, no app TV window. Restart app
  to check preference persistence. Test bottom panel both enabled and disabled.
- Wireless/Xiaomi box: first connect with Android; verify it actually exposes a
  public presentation display. Many receivers offer mirroring only, not a second display.
- On firmware refusing Activity moves, confirm the logged fallback, scaled TV image,
  working pad/controller, disconnect cleanup and acceptable latency.

Android's system **Cast screen** mirror of the top screen works without this feature.
Mirroring-only routes cannot carry separate TV content; use Off for those routes.
