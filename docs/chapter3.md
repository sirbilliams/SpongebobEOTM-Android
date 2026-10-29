# Chapter 3 playthrough (lane a)

`tools/test/chapter3.route` plays chapter 3 of the real `spongebob` binary from `c3l0s1` to the chapter 4 hand-over
(`schap3end` -> `mChapter4` -> `c4l0s1` -> `sload4` -> `c4l2s1`) through the real input path. Driver: `SB_TEST_PLAYTHROUGH`
(`game/src/port/Playthrough.*`, steps in its header; this route adds no step kind).

```
distrobox enter omtbuild -- bash -lc 'cd ~/Projects/sb-lanes/a/.game && DISPLAY=:81 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 \
  OMT_FRAMES=2000000 SB_SKIP_MOVIES=1 SB_GAME_LOG=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c3l0s1 SB_TEST_SHOTDIR=../.shots/ch3 \
  SB_TEST_PLAYTHROUGH=../tools/test/chapter3.route OMT_CONFIG=/tmp/sb-a.cfg ../build/spongebob'
```
Result: `PLAYTHROUGH DONE (187 steps, scene c4l2s1)`, one PNG per scene in `.shots/ch3/`. Chapter 3 reads no state of chapters 1 / 2, so
New Game + `SB_TEST_SCENE=c3l0s1` is the start. `save 0..3` steps are checkpoints (`SB_TEST_LOADSLOT=<n>` + a route from there).

## The route (what the game needs)
1. `c3l0s1` (`sload3`) -> `c3l2s1` road -> diner `c3l2s2` -> `c3l2s4`: Patrick `AAA` (dialogue end hook -> `sstellawalk`: Stella walks in with Sandy's
   lunch `ilb1`, `save`), Stella `BAA` (map unlocks Jellyfish Fields, `324BAA`), the lunch `ilb1` is picked up.
2. Map -> `c3l3s1` -> `c3l3s2` (right edge) -> `c3l3s4`: Sandy `AAA`, `ilb1` on her (`BAA`, `ssanwalk`; `334BAA` opens Home Lane).
3. Map -> Home Lane `c3l4s1` -> Squidward's house `c3l4s2` -> "Walk to TV" `c3l4s0`: `snelson` runs on entering the scene (4 TV shots, `340DAA` opens Sardine's).
4. Map -> Sardine's `c3l5s1` -> `c3l5s2`: Nelson `AAA ABA AXA` (`352ABA`: the Dream Glasses may be taken) -> `c3l5s3`: `igl1`.
5. Squidward's bed `c3l4s3`: `igl1` on Squidward (`spitcher`, `attach`) -> dream `c3l4s4` -> `s5` (`s345up`) -> `s6` -> `s7`: Squidward `sdream` (clarinet `icl1`) -> `c3l4s3`.
6. SpongeBob's house `c3l4s8` -> `c3l4s9`: `icl1` on the snake (`scharm`) -> water helmet `iwh1` (`spickupwh`); map -> Sandy's `c3l6s0`:
   `iwh1` on SpongeBob (`shelmetswitch` = `swapchar`) -> dome `c3l6s1`: Sandy `AAA` (`ssandydome`).
7. `c3l6s2` -> behind the picnic table `c3l6s3`: Oxygen Tanks `iot1` (a scene without a player: the click takes the item at once) -> back to `c3l6s4`:
   `iot1` on Sandy (`schap3end`: `BAA`, transition, movie `mChapter4`) -> `c4l0s1`.

## Defects found and fixed (exe code reconstructed)
* **Script `walkgrid` is one based** (0x434650: `dec` at 0x43483b / 0x43484b; `World3D::walkGrid` took the cell unchanged): every c3 walkgrid target (scharm,
  spitcher, sdream, svideo3) was a closed cell as zero based. The character walks to the right cell now.
* **The character's click rectangle follows the model** (0x42e560, the character's place, vtable 0x44e4b4: rect +0xf4..+0xfa moves by the screen delta of the
  projected previous (+0x404..) and new position (vt+0xec); x and z of the previous position non zero, rectangle not empty). The authored rectangle belongs to the
  start position, so SpongeBob's rectangle covered the water helmet in `c3l4s9` and nothing in the scene could be picked at the left (`World3D::place`).
* **Scene start script of `c3l4s0`** (scene init, "here 8": scene id 0x403 starts `snelson`): was never started.
* **ITEM click without a player** (0x4234d0: scene+0x248 == 0 calls the item's vt+0x104 = 0x423710 at once): `c3l6s3` (oxygen tanks) has no 3D player and the click did
  nothing (`World3D::takeItem`, also used by the pickup animation).

## Known limits / notes
* The scenes still log `walk <actor>: no route ... (target cell closed)` for a few clicks (sb `c3l4s2` door walk, Sandy's grid cell at `c3l3s4`, squidbob in the dream):
  the exe's own grid result for those targets, no stub.
* The kick mini game is not used by any c3 script.
* The retail closet door `svideo3` / TV-movie branch of `c3l4s8` is optional and not on the route.
