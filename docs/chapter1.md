# Chapter 1 playthrough (lane b, 2026-10-28)

Goal: from New Game, chapter 1 plays to its end (the chapter 2 movie `mChapter2`, then `c2l0s1`) in the real `spongebob`
binary, driven through the real input path. Route file: `tools/test/chapter1.route`; driver: `SB_TEST_PLAYTHROUGH`
(`game/src/port/Playthrough.{h,cpp}`, steps listed in the header). All addresses are in the retail `sb2.exe`.

Run (inside the omtbuild box, own Xvfb display):
```
cd .game && DISPLAY=:82 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 OMT_FRAMES=3000000 SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 \
  SB_GAME_LOG=1 SB_TEST_PLAYTHROUGH=../tools/test/chapter1.route SB_TEST_SHOTDIR=../.shots/ch1 ../build/spongebob
```
It prints `PLAYTHROUGH DONE` or `PLAYTHROUGH FAIL line N (...)` with the game's state. Screenshots: `.shots/ch1/NN-<scene>-*.bmp`.

## The route (worked out from the scene files, the 34 c1 scripts and the c1 dialogue tables)
Map `c1l1s1`: Downtown is shown after `121caa` (the customer told about the bus), Goo Lagoon after `164caa` (Patrick and the
tokens), Grandma's after `154aca` (Larry's pies), the Barg-N-Mart after `135aba` (Grandma's urchin chips).

| # | where | what | data |
|---|---|---|---|
| 1 | c1l0s1 | `sload1`, then `c1l2s6p1` | scene enter event |
| 2 | c1l2s6 -> c1l2s1 | door trigger "Enter The Krusty Krab" | TRIG door `126Door` |
| 3 | c1l2s1 | talk to the customer (AA, AB, exit), pick up the Dirty Plate `ikp4` | dialogue c1l2s1 AA/AB |
| 4 | c1l2s2 / c1l2s3 | talk to Squidward; walk-in trigger "Walk to Salad Bar", pick up Fixin's `ikp2` | |
| 5 | c1l2s4 / c1l2s5 | kitchen door; Plain Patty `ikp1`; plate on the Sink -> `ssink` -> Clean Plate `ikp5` | TRIG accepts ikp4 |
| 6 | bar | `ikp1+ikp2 = ikp3`, `ikp3+ikp5 = ikp6` | HUD recipes 0x421880 |
| 7 | c1l2s1 | give `ikp6` to the customer -> `scustomer` -> CAA ... `unlock` | CHAR accepts ikp6 |
| 8 | map -> c1l6s1 -> c1l6s4 | talk to Patrick (CA, CB: buried treasure, `unlock`) | marty is hidden until `164DAA` |
| 9 | map -> Goo Lagoon c1l5s1..s4 | Larry (AA, AB, AC: Grandma's pies, `unlock`) | |
| 10 | c1l5s5, s6, s7 | Dame Mackerel (AA), Ray (AA, AB: cooking oil) | |
| 11 | map -> Grandma c1l3s1 | house door = `stransition1` (first time) -> `smeanwhile1` (c1l3s6) -> c1l3s4 -> c1l3s5 | |
| 12 | c1l3s5 | Grandma (AA, AB: urchin chips, `unlock`, AC) | |
| 13 | map -> c1l4s1..s3 | Eugene (AA: wash my car) | |
| 14 | c1l4s1 -> c1l4s7 | the car `boat1` -> `swalktocar` -> car wash `scarwash`; rub the car clean -> `swash` | 0x43d270 |
| 15 | c1l4s3, s4, s6 | Eugene (AC: chips); Urchin Chips `iuc1`; Cooking Oil `ico1` (`soilpickup`) | |
| 16 | c1l3s5 | chips on the Chipless Pie trigger -> `schips`; pick up `ict2` | |
| 17 | c1l5s4 | pie to Larry -> `slarryjog`; Shovel & Pail `isp1` | |
| 18 | c1l5s7 | oil to Ray -> BAA; Sunscreen `isl1` | |
| 19 | c1l5s8 / s9 | the shovel on the four Beach triggers (`sdig1..4`, attach `isp2`), treasure chest -> `stoken` -> `ibt1` | |
| 20 | c1l5s5 | sunscreen to the Dame -> `sdame` -> Umbrella `iub1`; **save slot 5** | |
| 21 | c1l6s4 | tokens to Patrick -> `spattoken` (Marty appears), talk to Marty (AA, AB), umbrella to Marty -> `schap1end` | |
| 22 | | `movie mChapter2 c2l0S1` -> c2l0s1 (`sload2`) -> c2l2s7 | chapter 2 starts |

## What was missing and is reconstructed now
| exe | what | port |
|---|---|---|
| 0x43a310 | pick: TRIG pass (Condition when +0x27e, hidden, connectors of another height), CHAR/ITEM pass, 2D pass | `GameScene::pick/pickAll/hover` |
| 0x4398f0 | scene click: `touching` = player vt+0x128, the object's vt+0x40(touching, 1); the floor walk only when nothing took the click and the object is none / a SPOT without grid / a TRIG without events; the car wash squeeze | `GameScene::click` |
| 0x415a70 / 0x41a700 / 0x402240 | base event (Condition with `touching`), SPOT and 2DOB walk to their grid cell first | `baseEvent`, `pickEvent` |
| 0x441cb0 | trigger: walk to it, turn to its heading (Require Enter), a door plays "door" + its 2D door ANIM (0x43adb0), the event | `triggerEvent`, `playDoor` |
| 0x433a60 | character: comments / nothing to say go straight; walk, turn (fixed heading +0x2a4), the character turns to the player, event | `charEvent` |
| 0x4234d0, 0x433910, 0x433980, 0x423710, 0x433a00 | item pickup: walk, turn, "pickup"/"pickup2", the item into the bar (giveItem, HUD add), hidden, its sound, "pickupb", control back, the item's Event | `itemEvent`, `World3D::startPickup/animEnded` |
| 0x415c30 / 0x433f00 / 0x441ed0 | item use: base (accept / have item / wrong / refusal / H000F-G-N), character (walk there first, deferred use), trigger | `useItem` and kinds; refusal lines dialogue vt+0x64 (0x414030); CHAR record 0x416c70 |
| 0x40de30 | game vt+0xac: the interaction left until the player has walked / turned | `Game::setPending`, `World3D::runPending` (port: the exe's consumer was not located; it runs when the player stops) |
| 0x4313e0 / 0x431610 | the touch list; a TRIG without Require Enter fires when walked into (not during the 500 ms walk-in, not during a script) | `updateTouches/addTouch` |
| 0x434650 | arrival distance by target kind (CHAR/ITEM radii sum, TRIG radius, else radius/2, min 25); blocked target cell -> nearest open neighbour (0x417f10) | `walkTo`, `gridWalk` |
| 0x405620 tail | what follows a one-shot animation (pickup/pickupb, door -> walk through, walk, srun, idle2/3, kickdrop...) | `animEnded` |
| 0x40b720 | DoSwitch waits for the door / walk animation; the player walks into the new scene (game+0x258 = 500) on open cells (vt+0x114) | `Game::doSwitch`, `walkIn` |
| 0x411920 / 0x413c40 / 0x4117f0 | dialogue opening (the first group of a branch with a decision point is spoken at once, else the Z group), node actions only the first time, progress (current, stateA, finished) | `DialogueUI::speak/advance/hasDecision` |
| 0x411820 | conversation has something to say | `DialogueUI::hasSomething` |
| 0x416e50 | dialogue progress lookup is `_stricmp` (the `121caa` style terms) | `GameState` keys |
| 0x416a60 | the scene id is remembered on enter (`cXlYsZ` terms: first-visit comments) | `GameState::markScene` |
| 0x410450 / 0x410a60 | the global `H000x` comments (`sounds/comments.omt`) | `CommentTable::loadGlobal` |
| 0x43d270, 0x43a020 | the car wash: the pointer over nothing rubs `cardirty` clean, "wash" / "squeeze" sounds, > 99% -> `swash` | `GameScene::carWash` |
| 0x433780 / 0x433730 / 0x438ad0 | attach / detach (item model by bone), hide/show attach, record attachments by condition (hat `ikk1`, the Dame's parasol `iub2`) | `World3D::attach/detach/hideAttach`, `OMedia3DGrannyModel::attach_to` |
| 0x4033f0 | camera angles are signed degrees (c1l2s1 tilts -25: everything drew 16 degrees off) | `World3D` ctor, `clickFloor` |
| CHAR/ITEM +0x224 | the record's Scale | `World3D` ctor |

## Port notes / still not reconstructed
* The cross-height connector walk (`checkgrid`, vt+0x17c 0x434520, vt+0x178 0x434450) and the path post-processing (0x42c450):
  no chapter 1 step needs them (every c1 scene the route uses has one walking height).
* A walk-in trigger can fire on the way to somewhere else when the cell route crosses its radius (the exe smooths routes,
  0x42c450); the route file walks around "Walk to Ray" (`floor 300 600`).
* The pending interaction (game+0x228) runs when the player stops walking and turning; its consumer in the exe was not found.
* Random choices of the exe (refusal line, "can't" comment) go in turn in the port (deterministic tests).
