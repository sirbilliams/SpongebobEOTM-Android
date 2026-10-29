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
is written but the HUD (item in hand) that calls it is not. `// 3D:` the grid walk, 3D picking (characters, items, triggers) and the player's start
point are lane A's.
**ANIM** (0x408cb0, simplified): frames advance at `1000/fps` ms, `Loop Count` passes (or continuous), `Reverse When Done` ping-pongs, `Stay on Last
Frame` or back to frame 1, `Play Interval` s restarts an idle one, `Click to Play` starts it on a click (event at the begin with `Event Play @
Begin`, else when the run ends), `Frame per Click` steps by clicks, `Allow Toggle` pauses. `anim2d` waits for the run to end. port: the frames are
swapped on one `OMediaCanvasElement` instead of an `OMedia2DAnim` + `OMediaAnim`; `SoundFrame` is read but its Sound Name is not played in the loop.
**Letterbox / transition** (0x43bd90.., 0x43b760): two 640x240 black boxes at z 9999 / 10300; state 1 letterbox open (bars to y -176 and 416),
2 close, 3 transition close, 4 transition open (bars to -240 and 480), 5 hold 250 ms; speed 0.25 px per ms; every state end wakes the script.
**Waits and wake-ups**: `step()` clears all waits, so a wake is `Game::wake()` = `runUntilWait` and deleting the runner when finished. Callers:
bar states, ANIM end (`anim2d`), dialogue end (0x411ba0 / 0x439890), comment end, sound channel idle, the runner's idle timer, the HUD pickup timer, and
the 3D stubs (a script waiting for a walk, rotate or character animation is woken after `sim3dMs` = 700 ms, `// 3D:`).
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
  world, characters, grid walking, triggers/doors, save games (0x416ed0), mini games, `shock` / `sparkle` effects, movies (hook only), scene switch
  "loading" cover, dialogue specials per scene id (0x411ba0 tail), the speaker camera/actor swapping in 0x412d00.

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
  (straight line, 220 / 380 units per second, walk anim, facing from the direction), `move`, `rotate` (absolute heading, shortest way, 500 ms), `show` /
  `hide` / Visible Condition on characters. The dialogue speaker loops `<name>_talk` while a line plays (`speakersIdle` on line end).
* Picking: `OMedia3DVPModel` records the screen rectangle of each element's visible polygons in the last render (`get_element_bounds`); `World3D::pick`
  returns the nearest clickable CHAR / ITEM whose rectangle holds the pointer. `GameScene::click` / `hover` ask it before the 2D objects (the exe's
  3D passes come first, 0x43a310). A click on a character runs its `Event` through the classifier: the dialogue starts (no `event AAA` stand-in).
* Simplified (port): walking is a straight line without the terrain grid (`walkgrid` finishes at once, grid cells are not decoded), no player walk
  input / click-to-walk (a click on an object with a grid cell runs its event at once), walk speed is fixed instead of root motion, `moveY` / `climb` /
  `attach` / `kick` / shadows / camera shake are not done (`attach`, `kick` only log), headings of walks assume heading 0 = facing -z.
* Fonts: `Game` uses lane C's `GameFont` (0x414820..0x414fe0; `GlyphFont` is deleted).

## Game binary
* `StartupMain.cpp` `menu_new_game()` ends the menu loop; `run_game` makes a `Game` on the 640x480 screen canvas, `switchTo("mintro c1l0s1")` (movie, then
  the chained scene: `Game::movieHook` plays the Bink movie and the loop then sends `game->chained`), runs `Game::update` + the 2D world update per frame (no `OMediaPeriodical::devote_time`: it updates the worlds a second time and shows black frames). F10 / Android Back return to the menu. Movies named by scripts (`M...`) go through the same hook.
* **One place for scaling:** scenes and menu are drawn into the 640x480 canvas; `omgl_present` (engine/sdl/OMediaSDLGfx.cpp) scales it to the
  drawable, 4:3 centred (the Thor's 1920x1080 gets bars); the pointer goes back through `OMediaEventManager::window_to_logical` (omt1/Input). A
  widescreen mode is added in those two functions (and the 640x480 canvas / viewports' size), nowhere else. Bink movies scale in `StartupApp::present`.
* Test env (desktop only): `SB_SKIP_MOVIES=1` (logos, menu movies, intro), `SB_TEST_NEWGAME=1` (New Game at menu frame 20), `SB_TEST_SCENE=<scene or
  event>` (first target instead of the intro), `SB_TEST_CLICKS` / `SB_TEST_GAME_CLICKS="frame:x:y,..."` (logical-coordinate clicks sent as window events, frame numbers of the menu / game loop), `SB_GAME_LOG=1` (the runtime's log on stdout).
