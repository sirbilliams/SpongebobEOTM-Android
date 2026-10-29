# Scene runtime, 2D side (sb2.exe layers 5/6)

Code: `game/src/game/` (`GameState`, `Scene`, `DialogueUI`, `Game`, `SceneScriptHost`, `GlyphFont.h`), written on lane A's OMT 1.x layer
(`engine/omt1`: `OMedia2DWorld`, `OMediaCanvasElement`, `OMediaBoxElement`, `OMediaCCaption`, `OMediaEventManager`, `OMediaTextFont`) and the sound
manager (`omt1/OMediaSoundManager`, built here as library `omt1_sound`). Test: `build/sb_scenetest <game folder> [frames=1600] [shots dir]`
(see the end). All addresses are in the retail `sb2.exe` (image base 0x400000). Nothing from the exe is stored here, every value comes from the
player's files at run time. `// 3D:` marks every place that waits for lane A's 3D world and Granny characters.

`game/src/game` is part of the `spongebob` target (desktop and Android). One engine serves the game binary: the OMT 1.x layer
(`omt1` + `omt1_sound`; the OMT 2.5 `omt` library is no longer linked by `spongebob`, the old `sb_audio` library is gone, its role is
`omt1_sound`). See "Game binary" at the end.

## The classes of the exe and where they are
| exe | here | notes |
|---|---|---|
| game object, global 0x457310, vtable 0x44db24 | `Game` | 0x40a230 `switchTo`, 0x40ab60 `requestScene`, 0x40ac20 `exitScene`, DoSwitch 0x40b720 `doSwitch`, mouse |
| scene, vtable 0x44e5a0, ctor/dtor 0x436f30, loader 0x437210 | `GameScene` | scene vtable slots: see below |
| base object, vtable 0x44dd90 (listener) | `SceneObject` | one record type for 2DOB / ANIM / SPOT / ITEM / CHAR / TRIG |
| dialogue manager, vtable 0x44dcfc | `DialogueUI` | scene+0xd24 |
| progress object, global 0x457330 | `GameState` | `ConditionAtoms` + `DialogueProgress` |
| script runner (scene+0x1c7c) | `sbscript::ScriptRunner` + `SceneScriptHost` | `Game::wake` = scene vt+0xe0 (0x43aa80) |

Scene vtable (base 0x44e5a0) as used here: +0x94 load 0x437210, +0x9c save 0x437850 (writer, not needed), +0xa0 enter 0x437cf0 (the "here 1..17"
routine), +0xb4 speak 0x4397c0, +0xb8 notify 0x439890, +0xbc click 0x4398f0, +0xc4 pick 0x43a310, +0xd0/+0xd4/+0xd8 scene-kind tests (0x437140 level
1, 0x437160, 0x4371a0), +0xdc 0x4371c0 (the script's `voiceDone` gate), +0xe0 wake 0x43aa80, +0xe4 find character 0x43ab00, +0xf4 find 2D object
0x43ad10, +0xf8 show 0x438fa0, +0xfc hide 0x439110, +0x100 refresh visibility 0x438ad0, +0x118 timers 0x43ae70, +0x128/+0x12c/+0x130/+0x134/
+0x138 letterbox/transition 0x43bd90/0x43bf30/0x43bf40/0x43bf50/0x43c100, +0x13c bar state machine 0x43b760, +0x15c shake 0x43d130.
(docs/script.md quoted these slots from a base 0x44e53c: its numbers are right, they are the ones above.)

## Object fields (the exe's property sheets name them: 0x401d50 2DOB, 0x4078a0 ANIM, 0x41a4c0 SPOT, 0x4230a0 ITEM, 0x433120 CHAR, 0x441820 TRIG)
Offsets are relative to the serialisable part (docs/scene-loader.md `oXXXX`).
* common: `0x14,0x16,0x18,0x1a` click rectangle x1,y1,x2,y2 (empty = never hit, 0x415e00), `0x20` Event, `0x40` Condition, `0x60` Alt Event,
  `0x80` Accept this Item, `0xa0` Item Event, `0xc0` Wrong Item Event, `0xe0` Have Item Event, `0x100` Visible Condition, `0x120` Description (the
  hover text), `0x140` Sound Name, `0x16c/0x16e` Grid x/y (walk there first), runtime `0x161` is-3D, `0x163` used, `0x164` the tag.
* 2DOB: `0x174` OMT Name (canvas), `0x194/0x198/0x19c` Screen x, y, z-Depth, `0x1a0` Post Render (z = +10000).
* ANIM: as 2DOB plus `0x1b0` Number of Frames (canvases `"%s%02d"`), `0x1b2` Frames Per Sec, `0x1b4` SoundFrame, `0x1b6` Click to Play, `0x1b8` Play
  Interval, `0x1ba` Play Continuous, `0x1bc` Loop Count, `0x1be` Reverse When Done, `0x1c0` Frame per Click, `0x1ca` Stay on Last Frame, `0x1d0`
  Allow Toggle, `0x1d2` Start On/Off, `0x1d4` Event Play @ Begin. Placement z: 2DOB `+z`, ANIM `-z`, post render `10000`, background `-10000`.
* SPOT: `0x194/0x198` x, y, `0x1b0` Rollover uses Cond.
* ITEM / CHAR / TRIG (3D): `0x1e4` Granny Name, `0x224` Scale, `0x230..0x238` x y z, `0x23c` Radius, `0x240` rotation, ITEM `0x278/0x27a` Can Pickup /
  Add to Inventory, CHAR `0x27e` User Controlled, `0x246` Clickable, TRIG `0x278` Require Enter, `0x27c` Is a Door, `0x280` Door Anim Name.
* camera record (scene+0x268...): exits `0x192/0x1b2/0x1d2/0x1f2` (scene+0x3fa/0x41a/0x43a/0x45a, used by `exitScene(dir)`), `0x244` the scene's enter
  event (scene+0x4ac) and `0x264` its condition (scene+0x4cc).

## Flows
**Switch** (0x40a230 `Game::switchTo`): `unlock` -> HUD, `skip` -> skip, `H...` -> comment caption (comments table, `soundCI` voice on channel 8) and the
active actor talks, `C?L?S...` -> `requestScene` (id = scene*0x10000 + level*0x100 + chapter, `pN` = start point N-1; ignored while a switch is
pending, 0x40ab60), `M...` -> movie (first word; second word is the follow-up in `game+0x186`; port: a hook, without it the movie ends at once), `S...`
-> script (refused while the scene runs one; `ScriptRunner::load` runs to the first wait; `control on` keeps the player's control), anything else ->
dialogue key (8 chars: first 3 cut, 0x4397c0; no dialogue: comment `H000F`). `game+0x24e` is set after all but a refused script.
**DoSwitch** (0x40b720, `doSwitch`): old scene and its runner deleted (port: the exe keeps 10 loaded scenes), new `GameScene::load`, sound banks
(`sounds/cXlYsZ.omt` = game+0x5c, `sounds/cX.omt` = game+0x68, `sounds/comments%d.omt`, `sounds/items.omt`), comment/help tables, `enter`.
**Load** (0x437210): `sceneart/<scene>.omt`, canvas `background` at 0,0 z -10000, the `.scn` objects in file order; 2DOB canvases come from the scene's
database, then sb2.omt (0x407f90 order).
**Enter** (0x437cf0 "here 4..16"): refresh visibility, the scene event (camera `0x244` if its condition holds; or the pending `game+0x186`; an `H...`
event is parked in `scene+0x1cb4` and sent by the next update), hide `loading` for level-0 scenes, 500 ms click lock (`scene+0x1d4`).
**Visible Condition** (0x438ad0 `refreshVisibility`, also the script's `scriptEnded`): each object with a condition is hidden / shown by it.
`show` / `hide` (0x438fa0 / 0x439110) unhide / hide every 2DOB / ANIM of that name (show restarts an ANIM); the element's counted hide is used as the
exe does (`+0x4c`).
**Click** (0x4398f0): ignored while a script runs without control or during the click lock; `pick` (0x43a310, 2D pass in list order): hidden 2DOBs are
skipped (unless debug or the last clicked), ANIMs skipped when hidden and not toggleable or `Stay on Last Frame` while playing, SPOTs with
`Rollover uses Cond.` need their Condition, then the rectangle test. Hit: `pickEvent` (0x415a70): `Have Item Event` if used, else Event when the Condition
holds (plays Sound Name panned by screen x < 221 / > 419), else Alt Event; `" "` and `"0"` mean none; the result goes to `switchTo`. `useItem` (0x415c30)
is called by the HUD (`Hud::dropAt`, see "HUD and cursor" below). `// 3D:` the grid walk, 3D picking (characters, items, triggers) and the player's start
point are lane A's.
**ANIM** (0x408cb0, simplified): frames advance at `1000/fps` ms, `Loop Count` passes (or continuous), `Reverse When Done` ping-pongs, `Stay on Last
Frame` or back to frame 1, `Play Interval` s restarts an idle one, `Click to Play` starts it on a click (event at the begin with `Event Play @
Begin`, else when the run ends), `Frame per Click` steps by clicks, `Allow Toggle` pauses. `anim2d` waits for the run to end. port: the frames are
swapped on one `OMediaCanvasElement` instead of an `OMedia2DAnim` + `OMediaAnim`; `SoundFrame` is read but its Sound Name is not played in the loop.
**Letterbox / transition** (0x43bd90.., 0x43b760): two 640x240 black boxes at z 9999 / 10300; state 1 letterbox open (bars to y -176 and 416),
2 close, 3 transition close, 4 transition open (bars to -240 and 480), 5 hold 250 ms; speed 0.25 px per ms; every state end wakes the script.
**Waits and wake-ups**: `step()` clears all waits, so a wake is `Game::wake()` = `runUntilWait` and deleting the runner when finished. Callers:
bar states, ANIM end (`anim2d`), dialogue end (0x411ba0 / 0x439890), comment end, sound channel idle, the runner's idle timer, the HUD pickup timer, and
character completion (`World3D` calls `actorDone` when walking, turning or a one-shot animation ends). Remaining unimplemented character commands still use simulated waits.
**Sounds**: sound manager with 15 channels, 4 = voice, 5 = script `sound` / object sounds, 8 = comments, 9 = idle scripts. Bank lookups check
`object_exists` first (`get_object` by a missing name creates an empty object), exact / lower / upper case.

## Dialogue (`DialogueUI`, vtable 0x44dcfc)
* A conversation is `speak(key)` (0x411920): branch = `key[0]`; eligible groups (`eligible`, 0x411aa0) are the unfinished groups of that branch
  whose first line's `action` is an `ANIM...` marker or a true condition (`"0"` is true); `Z` groups are only eligible in monologue mode, `X`
  groups leave. A conversation with a single `X` group says nothing. One eligible group is taken at once; else a `Z` group of the branch is spoken
  first (`+0x2c`), then the choice list is shown.
* Choice list (0x4128d0): the choices stack upwards from y = 450 (`0x1c2`), x = 5, z = 10100, each `height + 12` apart, the selected one
  highlighted; hover 0x412af0 / click 0x4129e0 pick, Up/Down/PageUp/PageDown move (0x413e30 / 0x413e60), Enter / Space confirm (0x412bb0), Escape ends
  a list or skips a line (0x413e90).
* Playing a line (0x412d00): the voice is the Wave `%d%d%d%c%c%c` (chapter, level, scene, key letters) from the scene bank, tried in 7 case
  combinations (lower, UL U... order as in the exe); no voice: the line is shown for `max(1, len/14)` s and waits for a click. When the voice
  ends the scene notify (0x439890) advances. The speaker plays "talk" (`World3D::talk`).
* Advance (0x413c40): last line of a node: run its `action` (not `ANIM...`, longer than 3 chars) through `switchTo`; `n2` = 1 or an opening node:
  restart at the first line, 2: back to the last line with `n1 != 0`, else the node is finished (`stateB = 1`); progress is stored (0x416d30: group key,
  current key, two shorts, scene id); `n3 == 0` and not an exit: back to the list (a lone choice is taken, none left ends it), else the
  conversation ends (0x411ba0: `talk` waits are woken). Not the last line: step on; a next line with `n1 != 0` shows the list (a decision point).
* The meaning of `n1`..`n3` and of the `Z` / `X` second letters is read from these branches, not from a spec; the three are documented as
  "decision point / resumable line", "restart / back" and "ends the conversation" where they act.

## ScriptHost (`SceneScriptHost`)
2D calls are real (show, hide, anim2d, door, shake, transition, letterbox, talk/narrate, switch, comment, sounds, conditions, seen / items / undo /
texture, block cells, add / unlock / video as timed HUD stubs); 3D and character calls log `3D: ... (character vt+0x..)` and return what lets the
script go on. `voiceBusy` = dialogue active (+0x19), `voiceDone` = 0x4371c0 (true for levels 0, a few scene ids, or while a bar state runs).

## Deviations (port) and what is left
* stand-in font (`GlyphFont`, Font.omt glyph canvases) until lane C's font class (vtable 0x44dd78) lands; same `OMediaTextFont` interface.
* one scene at a time; text wrapping by `TextBlock` (one caption per row), highlight is a box; hover text placement is approximate (cursor + 14, 16).
* not done: the HUD / inventory bar (0x4201c0, 0x420b20, 0x420d50: item drag, "Show Inventory", map, hint), the cursor object (game+0x88), the 3D
  triggers/doors, save games (0x416ed0), mini games, `shock` / `sparkle` effects, movies (hook only), scene switch
* not done: the pause / main menu (HUD menu button logs only), the hint's voice, the 3D
  world, characters, grid walking, triggers/doors, save games (0x416ed0), mini games, `shock` / `sparkle` effects, movies (hook only), scene switch
  "loading" cover, dialogue specials per scene id (0x411ba0 tail), the speaker camera/actor swapping in 0x412d00.

## HUD and cursor (`Hud.{h,cpp}`, `Cursor.{h,cpp}`)
The exe's HUD object (global 0x457334) and the game cursor object (game+0x88). One `Hud` and one `Cursor` live in `Game` (`hud()`, `cursor()`); the
hooks in the shared files are the lines marked `// port: hud` / `// port: cursor` (Game.cpp / Game.h: create, mouse down / up / move, Esc, the
per-frame `update`, the caption override; SceneScriptHost.cpp: `uiAddItem` -> `Hud::addItem`).
| exe | here |
|---|---|
| 0x4201c0 pointer over the HUD (captions, "Show Inventory" / "Hide Inventory" / "Use Map" / "Get a Hint" / "Main Menu" / the item's name / "Empty"; with an item in hand "Use %s with %s" / "Use %s on %s" / "Give %s to %s" / "Swap %s with %s") | `Hud::caption` (feeds `Game::update`'s hover caption) |
| 0x420d50 mouse down (chest, map, menu, hint, slot -> pick the item up) | `Hud::onMouseDown` |
| 0x420b20 the item is let go (own / free slot: stays; other item: 0x421880 combine or comment `H000F`; a scene object: `GameScene::useItem` 0x415c30, the bar closes, an accepted item is removed) | `Hud::onMouseUp` / `dropAt`, `combine` (6 recipes: ikp1+ikp2 = ikp3, ikp2+ikp7 / ikp3+ikp5 = ikp6, ikp1+ikp5 = ikp7, iuc1+ict1 = ict2, ifm1+ikc1 = ikc2) |
| 0x420a30 / 0x420a70 / 0x420ab0 slot under a point (x 64 + 64 i, y < 64, six slots) / item in a slot / slots laid out again | `spotAt`, `itemInSlot` |
| 0x421390 open / close the bar, 0x4208f0 / 0x420990 button states (faint `...t` canvas, solid one under the pointer) | `setOpen`, `update` |
| 0x41f2d0 `add`: the item (name, description = the command's 2nd argument) appears at (320, 240) (special start points for ikp5, ivt1 in 0x20204, inc1 in 0x10304) and flies into its slot | `addItem`, `update` (`fly_`) |
| 0x421090 / 0x420fc0 the bar is shown / hidden | `wanted()`: scenes of a level (not level 0), no script without `control` |
Art (all from `sb2.omt`, nothing copied): `inv` / `chest1t` (closed chest, solid / faint), `inv2` / `chest2t` (open chest), `help` / `helpt`, `menu` / `menut`,
`map` / `mapt`, `bubble1` (slot with an item) / `bubblet` (empty slot). Layout (y 0): chest x 0, six slots x 64..448, hint 448, menu 512, map 576.
* **Items are 3D pictures.** An inventory item is the item's Granny model (`models/items/<name>_model`, from the player's files) shown through its own
  64x64 `OMediaVPElement` (own `OMedia3DWorld` + viewport, camera at the origin, model centred and scaled to fill the box, one light, slow turn
  under the pointer). The item in hand is the same element at pointer - 32 (box kept on screen), above the bar.
* **Input.** Mouse down on a slot picks the item up. A button release away from the slot lets it go there (drag); a click on the slot keeps it in hand
  and the next click uses it (also how the companion panel hands one over: `holdFromPanel`). Esc puts it back. On the Thor the touch is the pointer, so
  the same code serves a finger. The progress object's item list stays "ever found" (conditions `I<name>`), the HUD list is what is carried: used and
  combined items leave the HUD only (as 0x42a290, which does not touch the progress list).
* **Cursor.** sb2.exe has no pointer art: it uses the Windows arrow, and the hourglass (`OMediaMouseCursor::set_shape(Arrow / Clock)`) while loading;
  the held item is its own picture. The port hides the system cursor and draws a sprite (`Cursor`; four shapes drawn in code: Normal, Hotspot (a pointing hand
  over something clickable or a bar button), Hand (item in hand, gold arrow), Wait (a scene switch is pending)). The sprite is the last element of the world each
  frame, so it is on top. On Android it is the last touch position.
* **Engine finding.** `OMedia2DWorld` draws in link order (its sort mode is None, as the exe's), z only orders sub-elements. `Hud` and `Cursor` therefore build
  their elements with the first scene and link them again after every scene load (`Hud::relink`); elements linked at start-up would be under the scene.
* Port-only: the pickup flight is 1200 ms (the exe's +0x140 = 2000) so it ends with the script's wait; slot full = item not added; the hint button shows the
  first help line whose condition holds as a text block (no voice); the menu button logs only; the map button runs `c<chapter>l1s1p1` (0x455234).
* Test hooks (desktop only): `SB_TEST_ITEMS="isp1:Shovel and Pail,ict2:Urchin Chip Pie"` puts items in the bar when a scene runs, `SB_TEST_HOLD=<item>`,
  `SB_TEST_MOUSE=x:y`, `SB_TEST_CURSOR=<0..3>`, `SB_HUD_DEBUG=1`, `SB_NO_GAME_CURSOR=1`. Example (chapter 1, Larry accepts the pie, script `slarryjog`):
  `SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c1l5s4 SB_TEST_ITEMS=isp1:Shovel\ and\ Pail,ict2:Urchin\ Chip\ Pie SB_TEST_GAME_CLICKS=150:30:30,330:160:32,520:427:267`.

## sb_scenetest
`sb_scenetest <game folder> [frames] [shots dir]` (headless, `DISPLAY=:82 SDL_AUDIODRIVER=dummy`): opens the first scene `c1l0s1`, its script `sload1`
runs (letterbox, transition open, 3D waits simulated, sounds, `show loading`, `switch c1l2s6p1`), c1l2s6 loads, clicks on an ANIM and a SPOT
(synthetic SDL mouse events through the event manager) give their comments (text + voice), `scene c1l2s1`, a click-to-play ANIM, the dialogue with
the customer (`event AAA` stands in for 3D character picking): opening statement, choice list, hover, click, lines with voice, Escape. Frames go to
`.shots/f<frame>.bmp`. `SBT_STEPS="frame:click X Y,frame:move X Y,frame:key N,frame:scene NAME,frame:event NAME,frame:shot"` and `SBT_SCENE` replace
the default run. ASan/UBSan: clean (only the known shared-core `OMediaRLEPacker.cpp:81` misaligned load).


## 3D side (`World3D`, `game/src/game/World3D.{h,cpp}`)
`World3D` is made by `Game::doSwitch` for every scene (before `GameScene::enter`) and holds what the exe's scene init (0x4033f0) builds: an
`OMedia3DWorld` (camera focal / ambient / far / near from the camera record, no background, one GL rasterizer per process as 0x40ceb0), an
`OMediaViewPort` at the camera position and angles, an `OMediaVPElement` linking it into the 2D world at z 100, the LITE lights, and for every CHAR /
ITEM record an `OMedia3DGrannyModel` (`models/people|items/<name>_model`) + `OMedia3DShapeElement` at the record's x y z and rotation (`0x240`, units
0x4000 = 360). Characters start with `<name>_idle1`. `World3D::update` (from `Game::update`): Granny clock, walks, rotations, pose copy, 3D render.
* The scene's player start point (`pN`) moves the `User Controlled` character when the point is not all zero (c1l0s1's are zero: the CHAR position).
* SceneScriptHost: `play` (one-shot `<name>_<anim>.grn`, waits until the Granny control expires, then back to idle1), `loop`, `talk` anim, `walk` / `run`
  (grid route, 150 / 300 world units per second, walk/run animation), `walkgrid`, `movegrid`, `move`, `movey`, `rotate` (absolute heading, shortest way, 500 angle-unit tolerance), `show` /
  `hide` / Visible Condition on characters. The dialogue speaker loops `<name>_talk` while a line plays (`speakersIdle` on line end).
* Picking: `OMedia3DVPModel` records the screen rectangle of each element's visible polygons in the last render (`get_element_bounds`); `World3D::pick`
  returns the nearest clickable CHAR / ITEM whose rectangle holds the pointer. `GameScene::click` / `hover` ask it before the 2D objects (the exe's
  3D passes come first, 0x43a310). A click on a character runs its `Event` through the classifier: the dialogue starts (no `event AAA` stand-in).
* Walking/grid reconstruction and remaining differences are detailed below. `climb`, `attach`, `kick`, shadows and camera shake remain incomplete.
* Fonts: `Game` uses lane C's `GameFont` (0x414820..0x414fe0; `GlyphFont` is deleted).

## Game binary
* `StartupMain.cpp` `menu_new_game()` ends the menu loop; `run_game` makes a `Game` on the 640x480 screen canvas, `switchTo("mintro c1l0s1")` (movie, then
  the chained scene: `Game::movieHook` plays the Bink movie and the loop then sends `game->chained`), runs `Game::update` + the 2D world update per frame (no `OMediaPeriodical::devote_time`: it updates the worlds a second time and shows black frames). F10 / Android Back return to the menu. Movies named by scripts (`M...`) go through the same hook.
* **One place for scaling:** scenes and menu are drawn into the 640x480 canvas; `omgl_present` (engine/sdl/OMediaSDLGfx.cpp) scales it to the
  drawable, 4:3 centred (the Thor's 1920x1080 gets bars); the pointer goes back through `OMediaEventManager::window_to_logical` (omt1/Input). A
  widescreen mode is added in those two functions (and the 640x480 canvas / viewports' size), nowhere else. Bink movies scale in `StartupApp::present`.
* Test env (desktop only): `SB_SKIP_MOVIES=1` (logos, menu movies, intro), `SB_TEST_NEWGAME=1` (New Game at menu frame 20), `SB_TEST_SCENE=<scene or
  event>` (first target instead of the intro), `SB_TEST_CLICKS` / `SB_TEST_GAME_CLICKS="frame:x:y,..."` (logical-coordinate clicks sent as window events, frame numbers of the menu / game loop), `SB_GAME_LOG=1` (the runtime's log on stdout).

## Port widescreen presentation (lane C)

`SB_WIDESCREEN=fill|panels|stretch|off` selects presentation; the default is `fill`.
Android's launcher remembers **Widescreen: Fill (pan) / Side panels / Stretch** in
its settings preferences and exports `SB_WIDESCREEN` before SDL starts. Desktop
and Flatpak use the environment directly. `off` retains centred 4:3 black bars.

The mode belongs after scene composition, in `omgl_present`, rather than in the
scene camera: pre-drawn backgrounds, 3D characters, captions and hit rectangles
still use the authored 640x480 coordinates. Fill uses drawable width / 640 (3x
at 1920x1080), showing 360 logical vertical pixels. A bounded exponential pan
tracks the user-controlled character's projected `World3D::screenBounds` and the
mouse focus (window height mapped to the authored height) with a 12-pixel
character margin. Moving toward a window edge requests that authored edge. When both cannot fit,
it centres their combined span and clamps to the authored frame. Panels draws
original procedural planks and brass/blue circular portholes behind the centred
frame; stretch scales each axis independently. Narrow displays retain fit in
fill mode rather than cropping horizontally.

`omgl_window_to_logical` and its inverse share the last presented transform,
including the vertical crop and window/drawable pixel ratio. Both OMT mouse
input (including SDL's touch mouse events) and test click replay use these
functions. `StartupApp::run_game` enables the mode with current player bounds;
leaving it resets to fit. The menu is therefore always centred 4:3. Movie
`StartupApp::present` keeps its separate centred 4:3 path.

Validation in this sandbox: normal and ASan/UBSan builds succeed. At 1920x1080,
`SB_TEST_NEWGAME=1 SB_SKIP_MOVIES=1 SB_TEST_SCENE=c1l2s6`
with `SB_TEST_GAME_CLICKS=100:235:425 SB_GAME_LOG=1` reports
`click 235,425: CHAR 'SpongeBob'` in fill, panels, stretch and off.
Menu (fill selected) and startup movie (stretch selected) captures also retain
centred 4:3. Screenshots live under ignored `.shots/ws-<mode>.png`; additional scene captures
and sanitizer logs use `.shots/ws-<scene>-<mode>*`. Sanitizer runs disable leak
checking for the graphics/audio libraries (`ASAN_OPTIONS=detect_leaks=0`).

The required Xvfb/import check is still outstanding: Xvfb :83 fails to bind its
local/UNIX sockets under this session's sandbox, and `import -display :83
-window root` cannot connect. Captures here instead use `OMT_SCREENSHOT` at
1920x1080 with SDL's headless fallback, then ImageMagick BMP-to-PNG conversion.
The distrobox entry command is unavailable inside this already provisioned
build environment. No Android device/launcher execution was available.

## Lane D: walking and grid (2026-10-28)

`World3D.cpp` now consumes the loaded GRID records, selects the plane by the actor's exact Y, searches eight neighbours, follows cell-centre waypoints, and switches walk/run/rotate/idle animations. The player's floor clicks use the existing logical-coordinate mapping followed by inverse camera projection onto that plane. Object approach cells defer the event until arrival; CHAR/ITEM approaches turn to face the object before dispatch. No SDL coordinate mapping was changed.

Recovered meanings and routines:

| Retail address | Meaning / implementation |
|---|---|
| `0x418140`, `0x4181c0` | Cell address is `x*30+z`, zero is open; `block`/`unblock` modify the selected plane. The second 900 bytes at grid+0x39c are temporary character occupancy, not another terrain height. |
| `0x4183d0` | World -> cell: start at `(origin - halfSize*cellWidth)`, increment until the first boundary >= the coordinate. Exact boundaries belong to the lower cell; the outer lower boundary returns -1. |
| `0x418460` | Cell -> world: X=`o000c + (gx-halfSize)*o0008`, Y=`o0014`, Z=`o0010 + (gz-halfSize)*o0008`. Fractional coordinates are allowed. |
| `0x439e20`, `0x4181e0` | Select a grid by height; temporary occupancy of other visible characters uses the centre and four corners offset by 20 world units. |
| `0x434650`, `0x42af50` | Walk setup and eight-neighbour search. Cardinal edge cost 64, diagonal 90; waypoints are cell centres. CHAR `o0280` and runner `terrain` enable terrain routing. `terrain off` walks directly; grid targets then use `0x418460` directly. |
| `0x42f1a0`, `0x434ec0` | Speed is **0.15 units/ms walking, 0.30 running**, not root motion. Arrival threshold is max(radius/2,25). Heading derives from OMT vector yaw, converts to integer degrees, subtracts 90, then converts back to 16384 units/turn. |
| `0x42ed80`, `0x42ee80` | Rotate uses shortest signed difference. The second argument (scripts pass 500) is **angular tolerance**, not milliseconds. Turning advances eight elapsed-ms angle increments, sixteen when running. |
| `0x434390`, `0x4343b0`, `0x4342e0` | `move`, `movey`, `movegrid` place the character without a wait. `movegrid` uses the supplied coordinates unchanged. |
| `0x4398f0`, `0x4184f0`, `0x433a60` | Floor click selects a projected grid cell; object approach uses positive object grid coordinates minus one, then defers interaction. |

Important remaining reconstruction work (this lane is **not yet a fully faithful movement replacement**):

* The bounded A* implementation uses the recovered eight neighbours and edge costs, but does not reproduce every open-list tie or the retail curve/clearance postprocessor (`0x42c450` and `0x42cc50` onward). It retains cell-centre waypoints. Exact paths and corner turns can differ from retail.
* `checkgrid` is **not reconstructed**: its existing host still repositions at the same X/Z. The exe instead calls `0x434450`, chooses an up/down connector with `0x43a7a0`, and queues travel through it. Cross-height connectors, door animations, radius-based object approaches without an explicit grid cell, blocked interaction-cell neighbour fallback (`0x417f10`), and explicit object facing overrides are still required. Do not treat those as verified.
* Player floor input is verified on the current plane. This does not establish cross-height routing, scene-edge exits, or every interaction case.

Validation artifacts (local, ignored): `.shots/walk-*.png` are desktop captures of `c1l2s6` using `SB_TEST_GAME_CLICKS=100:450:395,700:500:435,1100:190:440`. The first destination routes around the blocked rectangle at cells X=8..9, Z=4..5. Xvfb could not bind :84; SDL offscreen OpenGL plus the existing `OMT_SHOTS` hook produced the captures. Use `OMT_DATA=$PWD/.game`, `SB_TEST_NEWGAME=1`, `SB_SKIP_MOVIES=1`, and `LD_LIBRARY_PATH=$HOME/Projects/sbeotm-private/ffmpeg/lib`.

The custom script test in ignored `.shots/testdata` symlinks the installed assets and adds a generated script exercising `movegrid`, two waiting `walkgrid` commands, `rotate`, `move`, `movey`, and `walk`. `.shots/asan-script.log` records both grid walks completing and the script ending with zero stub calls. `sb_scripttest`, `sb_scenetest`, and `spongebob` build. ASan/UBSan checks use `detect_leaks=0` (process/global engine caches are retained); movement runs report no memory/undefined-behaviour errors. The direct scene entry initially exposed a database eviction use-after-free; `World3D` now applies the same cache-disable initialization as desktop startup before loading Granny materials.


## Lane B follow-up: sky, world sort, save position (2026-10-28)
* **Black sky = the camera's sky** (found, not a clear colour). Camera `Sky Index` (+0x238), `Sky Direction` (+0x23a), `Sky Offset` (+0x23c, y), `Sky Speed` (+0x240, float) name canvas `sky0<index>` of `sceneart\c<chapter>skies.omt` (game+0x64, opened by the chapter loader 0x40b090; per-index cache at game+0x70, getter 0x40b590 = game vt+0xcc). The scene (0x43bb60) makes two 2DOB tiles of it at x 0 and 640, y = offset, z -10100, behind the `background` canvas (z -10000), whose sky area is transparent. 0x43ae70 scrolls: `scroll += dtMs * speed`; at >= 1.0 both tiles move by `ftol(scroll)` px (direction non-zero: right, else left), `scroll -= px`; each tile wraps inside +-640 (0x4022c0). Port: `Game::loadSkies/skyCanvas`, `GameScene::buildSky/updateSky`.
* **The game's 2D world sorts by z** (world class ctor 0x442010: `[world+0x88] = 3`, dirty). The port had left the default (None), so draw order was link order; `Game` now calls `set_sort_mode(omc2dsort_Z)`. Needed for anything behind the background.
* Checked c1l2s6, c1l4s1, c1l5s2 (sky visible), c1l2s1/c1l3s3 (no sky, unchanged).
* Save/load position: see docs/saves.md. Verified walk sequence `.shots/walk-seq.png` (click-to-walk at 1920x1080, fill mode), `.shots/load.png` (Resume lands at the saved spot), ASan/UBSan clean on a walk run.
* Still not reconstructed: `checkgrid` (0x434450/0x434520: scene vt+0x144 finds the object at the cell, walks to its +0x310/+0x318, runs its `0x128` check then queues the connector walk), cross-height connectors, 0x42c450 path post-processing.

## Lane f: character idle scripts and shadows (2026-10-28)

Code is marked `lane f` in `World3D.{h,cpp}` (Actor3D fields, `updateIdle`, `suspendIdle` / `resumeIdle`, `updateShadow`), `SceneScriptHost`
(`own_`) and one line each in `GameScene::click`, `ScriptRunner::backUp` and the GL rasterizer.

**Idle scripts (character update 0x42f1a0, ctor 0x42e150).** The ctor sets char `+0x2c4` (`idleWanted`). On the first update with `+0x32c` (the
idle runner) empty: clear `+0x2c4`, make a runner (0x43d550, `+0x1962` = 1: sound channel 9), load `s<name>_idle` (0x43dae0, actor = the character),
else `s<name>_idle%d%d%d` with the scene id's nibbles `id & 0xf`, `(id >> 8) & 0xf`, `(id >> 16) & 0xf` (c2l4s2 -> `sjojo_idle242`). A failed load or a
script that ends at once frees the runner; `+0x2c4` stays 0, so a character gets exactly one try per scene. There is no random delay and no timer at
the start: the script itself paces (`idle N`, `walk`, `rotate`, `anim`, `sound`, `loop`). A finished runner is freed the same way. Idle scripts run
alongside the scene script (the exe has no exclusion); `exitif` lines in them (sdude4: `141AAA`) end them. `idle N` (vt+0x168 = 0x42f170) also
plays idle1 and arms the char timer `+0x400`, which wakes the script (vt+0xf4 = 0x406050: the scene runner wakes if it runs on this character, else
the idle runner). Port: each idle runner has its own `SceneScriptHost` (the exe's runner object holds its own wait state); `World3D::finished`
routes an actor's end-of-walk / animation to the scene host if the scene runner's actor is that character, else to its idle host.
Sound waits poll `soundDone` (channel 9) per frame (0x43d6e0).
**Interruption.** 0x435080 (suspend, from the click handlers 0x433a60 / 0x433f00 and the bump check 0x431610): idle1, walk / rotate stopped, `+0x330` = 1;
the timer stops counting. 0x42f498 resumes (0x4350d0) when the player exists, no dialogue is busy, no scene runner exists and the player's target is not
this character: idle1, and if the last line was not an `anim` the runner steps back one line (`ScriptRunner::backUp`), then it runs. Port hooks: a
click on the character (`GameScene::click`) and `World3D::approach` call `suspendIdle`; the bump check (0x431610) is not reconstructed.
**No script: random idle2 / idle3.** Timer `+0x38c` accumulates seconds while the animation is idle1 and the model has `idle1..idleN` (N >= 2); at
`+0x390` (first and next values `7 + rand() % 8` s) it plays `idle2`, or `idle3` with chance 1/3 when N >= 3. The `idlet1..3` talk variants and the
per-animation sound (vt+0xf8 = 0x405040) are not reconstructed (the port's talk path differs).

**Shadows.** The `shadow on/off` flag is char `+0x30c`'s visibility (0x434430 / 0x434410: vt+0x30 hide). `+0x30c` is a second 0x354-byte model object that
0x422070 (the CHAR record's setup) makes from the record's "Shadow Name" (`o0204`, e.g. `shadow`, `shadow2`) as `models\items\%s_model`, only when the
Options 3D Shadows value (game+0x274) is set, and links to the character as a child at (0,0,0) (it follows position and heading). The model is a flat
quad (about 120 x 130 units at y = 0 of the character) with a 64x64 grey / alpha texture and a material with alpha 0.47. `World3D` loads it into the same
Granny/OMedia3D world, places it with the character each frame and hides it with `shadow off` or when the character is hidden. The GL rasterizer's
cut-out test (alpha > 0.5) dropped every fragment of a blended material, so a blended polygon now skips it (`OMedia3DGLRasterizer.cpp`). Not verified against
the exe: the D3D blend mode; the port alpha-blends the grey texture (light patch on dark floors).

Verified (1920x1080, in the box): c1l3s5 (`sgran_idle`: walks between points with no input), c2l4s2 (`sjojo_idle242`), c2l4s4 (`sgaryg_idle`); shadows under
SpongeBob and NPCs in all three; c2l2s3 `sflush` (`shadow off` -> no shadow, `shadow on` -> back), logs and PNGs in `.shots/lf/`.
