# Asset Viewer (port feature, not in the original)

`SB_ASSET_VIEW=models` or `SB_ASSET_VIEW=scene[:<name>]` starts the game binary in a viewer instead of the movies and menu.
Code: `game/src/port/AssetView.{h,cpp}`, hooked in `StartupApp::run()`; two small presentation hooks in `engine/sdl/OMediaSDLGfx.cpp`
(`omgl_view_window`, `omgl_view_pointer`). Leaving (Back, Esc, Backspace, Android Back, pad B / Back) ends the app, so Android returns to the launcher.
Nothing is read from or written to the saves; no art ships in the repo (everything is loaded from the player's files at run time).

## Launcher hook (for the lead; LauncherView.java is not touched here)
`SpongeActivity` reads the Intent extra `asset_view` (String, `SpongeActivity.EXTRA_ASSET_VIEW`) and sets `SB_ASSET_VIEW` (unset otherwise).
Wire the button: `new Intent(ctx, SpongeActivity.class).putExtra("asset_view", "models")` (model browser) or `"scene"` /
`"scene:c1l2s1"` (scene explorer; bare `scene` starts at the first scene).

## Model browser (`models`)
All models of `models/*/` (`<x>_model.grn`, 125 files): Characters, Props (bb, beachball, boat1, can, goal, shadow, shadow2), Items; sorted by name,
name and "n of N" shown. Loaded through the game's Granny wrapper (`OMedia3DGrannyModel`, same skinning and materials as a scene); its animations are
the `<x>_<anim>.grn` files next to it (idle1 starts when there is one), looped. Orbit / zoom / pan about the model's bounding box.
- Mouse / touch: drag turns, wheel zooms, right/middle drag or the Pan button pans; buttons Prev/Next model, Prev/Next anim, Zoom, Pan, Spin, Reset, tabs, Back.
- Gamepad: the right stick is the pointer and A the button (as in the game), so everything above works; also d-pad left/right model, up/down animation,
  L1/R1 category, left stick turns (pans when Pan is on), triggers zoom, X pan, Y spin, Start reset, B / Back leave.
- Keys: arrows (model/anim), PgUp/PgDn category, +/- zoom, WASD turn, Space spin, P pan, Home/R reset.

## Scene explorer (`scene[:name]`)
Any of `scenes/*.scn` is loaded by the game's own `Game` / `GameScene` / `World3D`, given about 40 settled frames (enter script, bars), then frozen: the scene is
no longer updated (no scripts, input or saves). A pan/zoom window (zoom 1 to 5; `omgl_view_window` crops the picture between the bars on presentation, so zoomed
art is the 640x480 render magnified) plus gold outlines and captions of every hotspot, character and item with a description; pointing at one shows its caption
and kind in the bottom bar. Prev / Next reloads another scene. The Bars button (key H, pad Y) hides the bars (tap the top-left corner to bring them back); the
22 / 48 px rows under the bars are visible only with the bars hidden. Same mouse / pad scheme (drag pans, wheel zooms at the pointer, d-pad left/right scene,
up/down zoom, left stick pans, X captions, Start reset).

## Tests (desktop)
`SB_ASSET_MODEL=<Name> SB_ASSET_ANIM=<anim> SB_ASSET_YAW/PITCH/ZOOM SB_ASSET_NOSPIN=1` (models), `SB_ASSET_ZOOM/CX/CY` (scene); `SB_TEST_PAD` / `SB_TEST_TOUCH`
work as in docs/controls.md (fixed 16 ms frames when set). Example:
`SB_ASSET_VIEW=scene:c2l2s1 SB_ASSET_ZOOM=2 SB_TEST_PAD="20:rx=0.9,ry=0.5;40:a=1;45:rx=-1;70:rx=0;75:a=0" OMT_WINDOW_SIZE=1920x1080`.
The game font has no `< > / ( ) :` glyphs, so labels use words.
