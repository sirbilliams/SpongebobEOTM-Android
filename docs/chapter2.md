# Chapter 2 playthrough (lane f)

`tools/test/chapter2.route` plays chapter 2 of the real `spongebob` binary from `c2l0s1` to the chapter 3 hand-over
(`schap2end` -> `mChapter3` -> `c3l0s1` -> `sload3` -> `c3l2s1`) through the real input path (SDL clicks, dialogue keys, the item bar).
Driver: `SB_TEST_PLAYTHROUGH=<route>` (`game/src/port/Playthrough.{h,cpp}`, from lane b; steps in its header).

```
distrobox enter omtbuild -- bash -lc 'cd ~/Projects/sb-lanes/f/.game && DISPLAY=:86 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 \
  OMT_FRAMES=2000000 SB_SKIP_MOVIES=1 SB_GAME_LOG=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c2l0s1 SB_TEST_SHOTDIR=../.shots/ch2 \
  SB_TEST_PLAYTHROUGH=../tools/test/chapter2.route OMT_CONFIG=/tmp/sb-f.cfg ../build/spongebob'
```
`OMT_CONFIG` keeps the run off the user's `settings.cfg`. Result: `PLAYTHROUGH DONE (n steps, scene c3l2s1)`, one PNG per scene in `.shots/ch2/`.
The start needs no chapter 1 state: chapter 2 reads none of chapter 1's flags, items or dialogue keys (checked in every c2 condition,
script and dialogue table), so `SB_TEST_NEWGAME` + `SB_TEST_SCENE=c2l0s1` is the same start as the end of chapter 1 (`sload2` follows).
A save step (`save 0`) in the route lets a later part be run from a checkpoint (`SB_TEST_RESUME=1`, no `SB_TEST_NEWGAME`).

## The route (what the game needs)
1. `c2l0s1`: `sload2` -> `c2l2s7` (bus station outside).
2. Bus station `c2l2s1` -> window `c2l2s2`: Information Lady (`teller`), keys `AAA ADA AEA AXA` (she wants a Kelp Bar).
3. Bus stop `c2l2s5`: Leon (`sleon`, talk `AAA ABA ACA AXA`); while the script is still running (`control on`) the Kelpsi can on the
   floor (item `ikc1`) is clicked (`grab ikc1`: Leon looks away, then the script hides it again).
4. `c2l2s3`: "Enter Restrooms" (`sflush`) gives the Quarter `iqu1`. `c2l2s6`: quarter on "Candy Machine" (`scandy`) -> Kelp Bar `ikb1`.
5. Kelp Bar on `teller` (item event `BAA`, `unlock`): the map shows WSEA (`chap202`, visible when `222BAA`).
6. Map -> WSEA `c2l4s1` -> "Walk Behind the Scenes" (`stransition3`, `smeanwhile3`, `c2l4s9`) -> `c2l4s2`: Jojo `AAA ACA ACA AXA`
   (second `ACA` = lines after the `n1` pause, ends with `unlock`: the Rusty Anchor `chap203`).
7. Rusty Anchor `c2l3s1`: parking lot `c2l3s2` (`bb AAA ABA ACA AXA`, `mm BAA BBA BXA`), bar `c2l3s3` (`mack AAA ABA ACA ADA`: Mack goes away,
   `smack`, via the dialogue-end hook below), pictures `c2l3s4` (Empty Mug `iem1`), kegs `c2l3s5` (mug on the Diet Coral-Cola spout -> `ifm1`),
   `combine ifm1 ikc1` -> Kelpsi Can Full of Diet Cola `ikc2`.
8. WSEA `c2l4s2`: `ikc2` on Jojo (`sjojo`, `attach ikc3`/`detach`) -> `242CAA`, the toolbox `itb1` can be picked up.
9. `c2l4s3`: toolbox on Tiffany (`shavetool`, `attach itb2`) -> `BAA`: the doors open. News room `c2l4s4`: `garyg AAA ACA AXA`, then the
   "Mega Screen 2000 Controls" (`smegascreen`, `unlock`: Marlin's cave `chap204`).
10. Marlin's cave: `c2l5s8` -> `c2l5s1` -> hat `c2l5s2` -> slot machine `c2l5s3`: `spin` pulls the handle until a spin wins (`swinner`,
    `sdrapes`, `sreturn`); the three symbols of the winning spin are the maze clue. Curtain (needs the term `xxxx`) -> maze `c2l5s4`:
    `maze` x3 clicks the door `smaze<symbol-6>` of each step -> `c2l5s5`. Up the stairs (`s255up`) -> Marlin `c2l5s6`:
    `AAA ABA ACA ADA` -> Magic Wand `imw1`.
11. Map -> WSEA `c2l4s3`: wand on "Control Room" (`swand`, `attach imw2`, `sparkle`, `setdoor`) -> `c2l4s6` -> "Fiddle With Weather Controls"
    (`schap2end`) -> movie `mChapter3` -> `c3l0s1` -> `c3l2s1`.

## Playthrough steps added
`grab <target>` (click as soon as it can be picked, no idle wait), `snap <name>` (shot without idle wait), `spin`, `maze` (see 10; the clue is
read from the visible `symbol0N` pictures of `c2l5s3`, reel x = 143 / 289 / 430, N-6 = the door; no change to the mini game files),
`locate` prefers the authored click rectangle for CHAR / ITEM targets.

## Defects found and fixed (exe code reconstructed)
* **3D picking, 0x43a310 second pass**: the exe walks the scene's object list in order and a clickable, visible CHAR / ITEM is hit when the
  point is inside its authored click rectangle (+0x14..+0x1a, test 0x415e00, an empty rectangle is never hit; first object wins). The port
  picked by the drawn model bounds, nearest first, so Leon's bounding box hid the can lying at his feet (`World3D::pick`).
* **Dialogue end hooks, 0x411ba0 tail** (were missing): by scene id and the key of the group that was picked, case-insensitive:
  `c2l3s3` + `ADA` -> `switchTo("smack")` (Mack leaves the bar), `c2l5s6` + `ADA` -> Magic Wand `imw1` into the bar and the state (0x41f2d0, 0x4169a0),
  `c3l2s4` + `AAA` -> `sstellawalk`, `c4l4s8` + `ABA` -> `stransition2` (`DialogueUI::stop`).
* **Condition term `xxxx`** (0x40a7d0: mini game object short +0x14 != 0 and +0x12 == 0 = clue armed by 0x435e10 and not failed): nothing set it, so
  the curtain `c2l5s2` stayed shut. `Game::cond` now sets `GameState::miniFlag` from `MiniGame::flag()` (one accessor added to `MiniGame.h`,
  the only touch of the mini game files).
* Test aids: `SB_TEST_DUMPOBJS=1` logs every scene object with events / conditions / click rectangle (route planning); `World3D::setHidden` logs
  `3D: show/hide <name>`.

## Known limits
* `attach` / `detach` come from lane b (merged: `0c44361`); a stub hit would show as a `3D:` line in the log.
* smoke.sh `click-dialogue` expected `line AZA` (the "back again" opening) on the first meeting with Larry; lane b's case-insensitive progress keys correctly start at `AAA`, the expectation is now `line AAA`.
* The route plays one winning slot spin chosen by the game's own `rand()`; it needs 1 to 4 pulls.
