# Chapter 4 playthrough (lane e)

`tools/test/chapter4.route` plays chapter 4 of the real `spongebob` binary from `c4l0s1` through `schap4end`, `mEnd`, `mCredits` and the
continue into `c1l2s6p1` (`Game::movieFinished`, the exe's post-game), through the real input path. Driver: `SB_TEST_PLAYTHROUGH`
(see docs/chapter1.md, docs/chapter2.md).

```
distrobox enter omtbuild -- bash -lc 'cd ~/Projects/sb-lanes/e/.game && DISPLAY=:85 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 \
  OMT_FRAMES=2000000 SB_SKIP_MOVIES=1 SB_GAME_LOG=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c4l0s1 SB_TEST_SHOTDIR=../.shots/ch4 \
  SB_TEST_PLAYTHROUGH=../tools/test/chapter4.route OMT_CONFIG=/tmp/sb-e.cfg timeout 1500 ../build/spongebob'
```
Result: `PLAYTHROUGH DONE (n steps, scene c1l2s6)`, one PNG per scene in `.shots/ch4/`. Chapter 4 reads nothing from the earlier chapters.
`save 1` in the route is a checkpoint (`SB_TEST_LOADSLOT=1`, partial routes were developed that way).

## The route (what the game needs)
1. `c4l0s1` -> `sload4` -> `c4l2s1`. Sandy (`stanks`, `talk baa`) gives the Empty Oxygen Tanks `iot1` (map: Oxygen Springs `chap402`).
2. Oxygen Springs `c4l5s1`: intercom `AAA AAA ABA AXA` (no jacket); back at Sandy (`c4l2s3` Rocket Landing -> `stransition4`, `smeanwhile4`,
   `c4l2s1`): `AAA ADA AXA` (`421ADA`: Waverly Hills `chap403`).
3. Waverly Hills `c4l3s1`: Coconuts (`scoco1`), pick `inc1`. `c4l3s2`: Water Pump (`swateron`), fountain `c4l3s1` (`sgrow`), Gate Pressure Plate (`spplate`).
4. `c4l3s3` -> Carlton `c4l3s4` (`AAA AAA ACA AXA`), shack `c4l3s5`/`c4l3s7`: Cowfish `AAA ABA ACA AXA`; entering the back room `c4l3s8` runs `sindy`
   (the trap, ends in `c4l3s9`). Normal coconut `inc1` on the pedestal's "Golden Coconut" (`sswap`) -> `igc1`. Golden coconut on Carlton (`scarlton`) -> Quarter `iqu1`.
5. Phone booth `c4l2s2`: quarter on "Coin Slot" (`sphone1`), keys 5 5 5 4 4 4 4 (mini game docs/minigame-phone.md; `wait 90` after each key: the key animation), `sphone3`,
   dialogue `422AAA` (`chap404` Sublime Seafoods).
6. `c4l4s4`: bouncer (`stony`). Timmy `c4l4s3` `AAA ABA AXA`; binoculars `c4l4s2` -> `c4l4s9`: the pointer pans the big background until the sign is in view
   (`sbinoc`, `comment h449a`, back to `c4l4s2`); Timmy again (`sinvite`, `iin1`). Invitation on the bouncer (`sbouncer`).
7. `c4l4s5` -> `c4l4s6`: Shapiro `AAA ABA AXA`, Lisa (`scoatcheck`, jacket texture), Shapiro (`sjacket`), dining `c4l4s7` -> table `c4l4s8`: Frederique, ordering the Krabby Patty
   throws SpongeBob out (`skout`, `c4l4s5`). Jacket kept (`snojac` / `scoatcheck` texture swap, `undo`).
8. `c4l4s4` (upper level: `Walk Down Stairs` first, `s444down`) -> map -> Oxygen Springs: intercom `BAA` (`sintercom`), Patrick `c4l5s4` `AAA`, locker room `c4l5s5`:
   Leonard (`sleonard`), Patrick's shorts `ipp2` (`stakeshorts`, `ipp1`), shorts on Patrick (`sshorts`), Oxygen Timer (`stubon`), tanks `iot1` on the Oxygen Tub
   (`schap4end`) -> `mEnd` -> `mCredits` -> `c1l2s6p1`.

## Reconstructed (exe code)
* **0x43b200 (scene vt+0x11c) the pan of the big background**, only c4l4s9 (scene id 0x90404, binoculars) and c2l4s0 (0x402, `splank`; vt+0xd8 = 0x4371a0):
  start shift -320,-240 (c2: -50,-50) and the pointer put in the middle (0x437cf0 "here 13", input manager vt+0xc(0x140,0xf0));
  pointer outside the window x 190..450 / y 175..305 (c2: 160..480 / 125..358) or A/D/W/S/arrows moves the background (scene+0x1ca2/+0x1ca4)
  0.15 px/ms, limits x in [right-0x500, left], y in [bottom-0x3c0, top]; no pan while a script runs; the "turn" sound on channel 7 while it moves
  (0x4367a0, stopped 0x4368f0); when the sign rectangle is inside the window the script `sbinoc` (c2: `splank`) starts once. The cursor index the exe returns (2..9) is
  not used: the port draws its own pointer (`// port:`). Code: `GameScene::pan` (Scene.cpp), called from `Game::update` after the scene update.
* Test aid: `hover <x> <y> [n]` (Playthrough.cpp) holds the pointer.

## Observations
* c4l3s8/c4l3s9: the pink/purple SpongeBob and the orange golden coconut are the authored lighting (c4l3s9: ambient 0,50,100, light 0.8/0.3/0.6), not a colour bug.
* c4l5s5: the red wall outlines are in the sceneart.
* `undo` (`snojac` / `scoatcheck`) and `texture` work in the real run (the coat comes and goes; log `texture sb spongetux: bound`).
