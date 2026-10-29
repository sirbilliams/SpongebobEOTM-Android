# Script interpreter and dialogue tables (sb2.exe layer 5)

Code: `game/src/script/` (`Sb2File`, `ScriptRunner`, `ScriptHost` + `ScriptHostLog`, `ScriptCondition`, `Dialogue`).
Test: `build/sb_scripttest <game folder> [steps] [chapter/script]` (target in `engine/CMakeLists.txt`, source `engine/tests/sb_scripttest.cpp`).
All addresses are in the retail `sb2.exe` (image base 0x400000). Nothing from the files or the exe is stored here.

## Corrections to docs/formats.md found on the way
* The scene id (`scene+0x264`, `game` switch code 0x40ab60) is packed **scene<<16 | level<<8 | chapter**, chapter in the
  LOW nibble (`atoi(name+1)` is the chapter, `+3` the level, `+5` the scene of a name like `c1l2s6p1`). The sprintf of
  `dialogue\c%dl%ds%d.sb2` and `sounds\c%dl%ds%d.omt` takes `id & 0xf` first. `unlock` tests `0x403` = c3l4s0, the end
  routine tests `0x50203` = c3l2s5.
* The dispatcher compares against **57** command names (list below), not "about 60".

## Container (0x43dae0, 0x4122f0, 0x4106d0, 0x419210)
`u16 count; count x { i16 len; u8 text[len] }`, every byte XOR 0x30. The exe reads a record into a zeroed 512-byte buffer
(port: len > 511 is an error). A record is split with `strtok(line, "\t\n")` (delimiters at 0x454c0c): empty fields vanish and
later columns shift left. `Sb2File.cpp` is that reader plus `SbTok`.

## Script files (`scripts\c%d\%s.sb2`, chapter = `game+0x154`)
Loader 0x43dae0; if the `.sb2` is missing 0x43d750 tries `scripts\c%d\%s.txt`, then `scripts\%s.txt` (same tokenizing, one
`fgets(256)` per line). Each record becomes a 0x80-byte line at `runner+8+i*0x80`: command (+0), args at +0x20, +0x40, +0x60
(char[32] each, initial value ""). Blank lines still take a slot (empty command, a no-op). Max 50 lines (port: error beyond;
the exe would overrun the runner object). The data: 168 files, 1126 lines, longest 30 lines, longest field 28 chars.

Load tail (both loaders): count -> `+0x1908`, `cur` (`+0x190a`) = -1, `finished` (`+0x190c`) = 0, waits (+0x1954, +0x1956,
+0x1957) = 0, terrain flag (+0x1960) = 1, idle (+0x195c) = 0, actor (+0x1950) = 0. With no actor passed the scene's active
actor (`scene+0x248`) becomes the actor and `game+0x88 vt+0x38` is called (`scriptStarted`). If the first line is neither
`control ...` nor has `on` as first argument, the actor is reset (`char+0x37c = 0, +0x3cc = 0, +0x358 = 0`,
`actorBeginScript`). Then `do { r = step(); } while (r == 0)`, and the mouse position is stored (+0x1958/+0x195a, used by
`onClick`).

## State model (runner object, 0x1964 bytes, ctor 0x43d550, vtable 0x44e710)
| offset | meaning | member |
|---|---|---|
| +4 | sound handle of the last `sound` | `sound_` |
| +8 | 50 line records | `lines_` |
| +0x1908 / +0x190a | line count / current line | `count_`, `cur_` |
| +0x190c | script finished | `finished_` |
| +0x190d | script name (32) | `name_` |
| +0x192d | name of the command being run (copied before each dispatch) | `lastCmd_` |
| +0x194d | `control on` seen (the scene keeps the player's control after the script) | `control_` |
| +0x1950 | current actor (character) | `actor_` |
| +0x1954 / 55 / 56 / 57 | waiting for: UI (add/unlock/video), comment caption, dialogue (talk/narrate), sound | `wait*_` |
| +0x1958 / +0x195a | mouse x, y when the script started | `mouseX_/Y_` |
| +0x195c | idle timer in ms (float), only used when there is no actor | `idleMs_` |
| +0x1960 | terrain flag (`terrain on/off`), read by the scene | `terrain_` |
| +0x1961 | sound handle belongs to the shared bank (not freed) | `soundShared_` |
| +0x1962 | idle-script runner: plays on sound channel 9 instead of 5 | `idleRunner_` |

There is no separate variable store: flags/variables live in the game's *progress object* (0x457330: seen scripts list,
inventory list, flag list, dialogue progress list), reached through `ScriptHost` / `ConditionAtoms`. The script itself can
only change its own line list (`goal` rewrites lines, `loop`, `skipif`, `exitif`) and the progress object (`add`, `save`,
`undo`, `texture`, `block`).

### Step and wake-up
`step()` (0x43e110) runs ONE line: `cur++`, clear the four waits; `cur == count` -> end routine, return 1; copy the command to
`lastCmd`; strcmp chain. Return 1 = "the script waits now (or ended)", 0 = "run the next line". Unknown command: falls
through the chain, returns 0 (the line does nothing). Nothing polls a waiting script: the scene calls `runUntilWait()`
(scene vt+0xe0 = 0x43aa80 / char 0x406050: `do step() while (!waits)`; delete the runner when `finished`) from the event that
ends the wait: character arrived / animation or rotation done (character code), `transition`/`letterbox` finished, dialogue
ended (clears +0x1956), caption done (+0x1955), item UI closed (+0x1954), and from the frame update:
`if (+0x1957 == 0) { if (idle > 0 && idleElapsed(dt)) advance } else if (soundDone()) advance` (0x41bc..). `idleElapsed`
= 0x43d650, `soundDone` = 0x43d6e0. `skipToLetterboxClose` (0x43df30) is the cutscene skip: finds the next
`letterbox close`, positions before it, clears waits. `talkAhead` = 0x43e0b0. `onClick` = 0x43dea0 (a click at the start
position while the last command was `walk`/`walkgrid` calls `actorStopWalk`).

End routine (0x43dfd0): finished=1, waits cleared, terrain=1, idle=0, `progress.markSeen(name)` (0x416610; the `S<name>`
condition term reads it), then unless the scene is 0x50203 and the name contains neither `up` nor `down` and is not
`sinitslots`/`sspinslots`/`sreturn`/`swash`: scene vt+0x100 (`scriptEnded`).

### How a scene starts a script
`game vt+0xb0` (0x40a230, called by the `switch` and `movie` commands and by scene interaction code) classifies a name:
`unlock` -> item UI; `skip` -> skip; `H...` -> comment caption (`game+0xc4 vt+0x14`) and the actor plays "talk";
`C?L?S` (c1l2s6p1 style) -> scene switch (game vt+0x94); `M...` -> movie (`movies`: stop; else first space-separated
token is the movie, the second is stored at `game+0x186` as the target to go to afterwards; the `script` command stores into
the same place); `S...` -> **script**: refused if the scene already has one (`scene+0x1c7c`), otherwise runner = new (0x1964
bytes), `load(name, 0)`; if it ended at once it is deleted, otherwise, unless `control on` was seen, the actor loses control
(`char+0x32a = 0`, and for `sradio`/`sjukebox` plays idle1); anything else -> scene vt+0xb4 (dialogue key). Idle scripts
(`s%s_idle`, `s%s_idle%d%d%d`, scene update 0x42f1a0) use a runner with `+0x1962 = 1`. The first scene (c1l0s1) holds an
object named `sload1`.

## Commands (all 57; "wait?" = returns 1)
Host call names are `ScriptHost` virtuals. Numbers are parsed with atof (float) / atoi exactly as the exe does.

| command | args | effect (host call) | wait? | address |
|---|---|---|---|---|
| walk | x y | `actorWalk(x, y, 0, -1, -1)` | if it returns true | 0x43e19c |
| checkgrid | v | `v != actor y`: `actorReposition(x, z)` | yes, in that case | 0x43e23c |
| run | x y | `actorWalk(x, y, 0, -1, -1)`; if true `actorSetRunning` | if true | 0x43e2b5 |
| walkgrid | gx gy | `actorPrepareWalkGrid`; `actorWalk(0, 0, 0, gx, gy)` | if true | 0x43e347 |
| move | dx dy | `actorMove` | no | 0x43e3e4 |
| movey | y | `actorMoveY` | no | 0x43e44b |
| movegrid | gx gy | `actorMoveGrid` | no | 0x43e49b |
| skipif | expr | `cur += 2` if `condition(expr)` (the next 2 lines are skipped) | no | 0x43e504 |
| exitif | expr | if `len(expr) > 1 && condition(expr)`: end routine | yes, then | 0x43e557 |
| blending | on/off | `gameSetBlending` (`game+0x251`) | no | 0x43e5cf |
| shadow | on/off | `actorShadow` (0x434430 / 0x434410) | no | 0x43e656 |
| terrain | on/off | terrain flag (+0x1960) | no | 0x43e6da |
| hideattach | | `actorHideAttach` (attach object `char+0xad8`, vt+0x30(1)) | no | 0x43e755 |
| showattach | | `actorShowAttach` (only if the object's +0x4c is set) | no | 0x43e79d |
| attach | obj bone1 bone2 | bones copied to `char+0xa76`/`+0xa96`, `actorAttach` (vt+0x12c) | no | 0x43e7f7 |
| shake | | `sceneShake` (scene vt+0x15c) | yes | 0x43e8c7 |
| detach | | `actorDetach` (vt+0x130) | no | 0x43e90f |
| switch | target | `gameSwitch(target)` (game vt+0xb0, see above) | no | 0x43e942 |
| loop | | `cur = -1` (restart) | no | 0x43e984 |
| wait | | looks at the PREVIOUS line: add/unlock/video -> +0x1954 (UI), comment -> +0x1955, sound (with a handle) -> +0x1957, narrate -> +0x1956; always blocks | yes | 0x43e9b2 |
| idle | seconds | with actor `actorIdle(s)` (vt+0x168), without: idle timer = s*1000 ms | yes | 0x43eb19 |
| climb | h | `actorClimb` | no | 0x43ebb5 |
| movie | name [next] | `gameSwitch(name)` | no | 0x43ec05 |
| sparkle | x y | `sceneSparkle` (object 0x457334, 0x41ebe0) | no | 0x43ec47 |
| block / unblock | x y | `sceneBlock(x-1, y-1, true/false)`: grid cell + progress 0x416ba0 + game vt+0x84 | no | 0x43eca7 / 0x43ed67 |
| rotate | degrees | `actorRotate((deg<<14)/360, 500)` | if true | 0x43edf9 |
| setchar | name | actor = `findActor(name)` if found | no | 0x43ee7f |
| add | item arg | `uiAddItem(item, arg, flag)` (flag false for isp1/iem1/ikc1, else true + `uiAddItemRefresh`), `progressGiveItem` | no | 0x43eed4 |
| unlock | | scene 0x403 and script `snelson` seen: skip the next line; else `uiUnlock` | no | 0x43efcb |
| video | | `uiVideo` | no | 0x43f032 |
| skip | | `gameSkip` | no | 0x43f062 |
| script | name | `setChainedTarget` (`game+0x186`) | no | 0x43f095 |
| comment | id | `gameComment(id)`; actor plays "talk" | no | 0x43f0f1 |
| hide / show | name | `sceneHide` (vt+0xfc) / `sceneShow` (vt+0xf8); `show can` also loops `can` "idle1" | no | 0x43f15f / 0x43fc47 |
| save | | `progressSave` (0x416880: actor data, scene id, x y z, rotation) | no | 0x43f1a7 |
| undo | name | `progressUndo` (0x416590) | no | 0x43f213 |
| control | on/off | active actor `+0x32a` = 1/0, runner +0x194d | no | 0x43f252 |
| talk | key | `sceneSpeak(key)` (scene vt+0xb4), wait for dialogue (+0x1956) | yes | 0x43f306 |
| narrate | key | `sceneSpeak(key)`, no wait (a following `wait` waits) | no | 0x43f365 |
| transition | close/open | `sceneTransition` (close also sets `scene+0x1c9a=0, +0x1c98=3`) | yes | 0x43f3b3 |
| letterbox | init/open/close/out/in | `sceneLetterbox(...)` (vt+0x128/0x12c/0x130/0x134; `in`: `game+0x25c = 1`) | open, close | 0x43f496 |
| sound | name [left/right] | frees the old sound; skipped while `voiceBusy && !voiceDone`; `soundOpen` (scene bank `game+0x5c`, else shared `game+0x68`); `soundPlay` on channel 5 (9 for idle runners), pan 0/1/2 | no | 0x43f5f4 |
| texture | actor tex | `actorSetTexture` (vt+0xf0) + `progressTexture` (0x4166d0) | no | 0x43fb68 |
| setdoor | name | `sceneSetDoor` (`scene+0x1c80`) | no | 0x43fbed |
| swapchar | | `swapPlayerHelmet`: copies pose of `sb` to `sbhelm`, helm becomes active actor | no | 0x43fced |
| startanim | name [speed] | `actorPlayAnim(name, wait=false, speed)` (vt+0xb8, default speed 1) | no | 0x43fdb0 |
| anim | name [speed] | `actorPlayAnim(name, wait=true, speed)`; if true: `kickdrop`/`kickup` also run on `can`; | if true | 0x43fe31 |
| anim2d | name | 2D object's vt+0x9c | yes | 0x43ff4d |
| shock | | `shockEffect`: 2D object `elec` at the player's screen position (-(z+10)) | no | 0x43ffda |
| slots | init/spin | mini game object (`game+0x12c`, created on demand): 0x436030 / 0x436090 | spin | 0x4400b6 |
| maze | n | no mini game: `gameSwitch("c2l5s2p2")`; else 0x436510(n) | no | 0x4401a3 |
| kickit | | `actorKick` (vt+0x16c) | yes | 0x440213 |
| goal | | actor vt+0x174 true: cheer sound, `sb` dances, **lines cur+1/cur+2 are overwritten** with `setchar beachball` and `move 150 -200`, count += 2; else if vt+0x188: line cur+1 := `move 150 -200`, count += 1 | on success | 0x44027b |
| phone | 0..10 / 11 | mini game 0x436610 (0) / 0x436640(n); `11` only if it exists | no | 0x440774 |
| darts | 0 / 1 | mini game 0x436990 / 0x4369c0 | no | 0x4409de |

`idle3` (1 line, `scripts/c1/sgran_idle.sb2`) is not in the chain: the exe does nothing for it. `checkgrid`, `movey` and
`movegrid` occur in no shipped script.

## Conditions (`skipif`, `exitif`, help table Condition column)
`game vt+0xb8` (0x40a6b0) and per term `vt+0xbc` (0x40a7d0), `ScriptCondition.cpp`. If `game+0x24f` (debug) is set: true.
Terms are space separated; " or " anywhere makes the expression OR, otherwise AND; `or`/`and` tokens are skipped. A term:
`0`, empty or a single space = true; optional `!`; `touching` (true only when the extra argument, always 0 from scripts,
is set), `debug` and `zero` = false; `S<name>` script seen (0x416690); `I<name>` item held (0x416a20); `C` + digits at
[1][3][5] (length > 3) flag set (0x416ad0); six chars `ccl s` digits + 3-letter key (chapter, level, scene, key) or a bare
3-letter key (current scene): the dialogue progress entry exists and its first short is non-zero (0x416e50); `xxxx`: the
mini game object's shorts (+0x14 != 0, +0x12 == 0).

## Dialogue tables
52 files in `dialogue\`: 50 scene tables `c%dl%ds%d.sb2`, `comments.sb2`, `help.sb2` (1876 records).

**Scene table** (0x4122f0, `DialogueTable`). Row: `tag  branch  node  line  speaker  listener  text  n1  n2  n3  action`.
* column 1 (the `121` style tag, = chapter level scene) and column 6 (listener) are parsed and dropped by the exe
  (`listener` is kept here for tools); rows without a letter or digit are skipped (193 of the shipped rows).
* key = branch + node + line (`AAA`). Lines with the same branch (key[0]) and node (key[1]), compared upper-cased, form a
  *group* (0x24-byte node: first key, two progress shorts, current line, first line); the group key is the key of its first
  line. A line record is 0x160 bytes: key +0, n1 +0x10, n2 +0x12, n3 +0x14 (atoi), action +0x16 (32), speaker +0x36 (32),
  text +0x56 (256), text height +0x156 (init 0x12), next +0x158, prev +0x15c.
* afterwards, per group, the progress object is asked (`DialogueProgress::lookup`, 0x416e50, scene id + group key): the two
  shorts are stored in the group, the current line becomes the line whose key equals the stored key, and if the second
  short is non-zero there is no current line. The sound bank `sounds\c%dl%ds%d.omt` is opened by the caller
  (`game vt+0x64`, fallback `<data root>sounds\...`).
* voice for a line: Wave named `%d%d%d%c%c%c` = chapter, level, scene, then the three key letters upper-cased
  (0x412d00), e.g. `122AAH` in `c1l2s2.omt`.
* `action` is `0` or a command line for the script dispatcher (`anim sb emoexplain`, `unlock`).

**Comments** (0x4106d0 `CommentTable`, lookup 0x410a60). Header row skipped. `id  text  (empty columns)`; id `H<chapter><level>..<letter>`;
a load keeps rows with id[1] == chapter digit and id[2] == level digit. Entry 0x100: id +0, text +0x10 (240). Bank
`sounds\comments%d.omt` (chapter, fallback with data root); voice name `%c%c%c%c%c` = id[0..4], first and last upper-cased.
Played from `game vt+0xb0` for names starting with `H` and from the `comment` command.

**Help** (0x419210 `HelpTable`). Header row skipped; `id  text  condition`; id `F<chapter>...`; keeps id[1] == chapter digit.
Entry 0x130: id +0, text +0x10 (256), condition +0x110 (a condition expression above). Bank `sounds\help.omt`.

## sb_scripttest result (retail data)
168 scripts, 1126 lines, 0 parse errors, 55 of the 57 dispatcher commands used (checkgrid, movey, movegrid never), 1 line
the dispatcher ignores (`idle3`); 52 dialogue files, 1876 records, 0 errors (1149 scene lines in 283 groups, 383 comment
entries, 149 help entries). Every script is also run to its end (or 3000 steps) against the logging host; ASan/UBSan clean.

## Open questions
* Host semantics that are only known from call shape: vtable slots of the character class (0x144 walk, 0x178, 0x188, ...)
  and of the scene (0xdc, 0xd24+0x18 "voice" pair); `checkgrid` compares against `char+0x314`; naming is by command.
* `sound`: what the `voiceBusy`/`voiceDone` gate is exactly (scene+0xd24 vt+0x18, scene vt+0xdc); the exe skips the sound
  while the first is true and the second false.
* Conditions: layout of the progress object's lists (0x457330 +8 seen, +0x14 flags, +0x20 items, +0x38 dialogue) is only
  read through `ConditionAtoms`/`DialogueProgress`; the save-game writer (0x416ed0) will define it.
* `wait` at line 0 reads the memory before the line table in the exe; here it is treated as "previous line is empty".
* `goal`, `kickit` run on actor vt slots whose meaning (dart/ball game) belongs to the character reconstruction.
* The scene dialogue UI (0x4128d0, 0x4129e0, 0x412d00: choice list, voice, flags n1..n3, `action` dispatch) and the exact
  lookup of a `talk` key (scene vt+0xb4) are not reconstructed here: `DialogueTable::find` is an exact strcmp over all lines.
* `anim2d` with no 2D object of that name: the exe calls through a null pointer (crash); the host decides.
