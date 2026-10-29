# Music (sb2.exe, reconstructed)

Before this the port never started a song (the exe's music code had not been reconstructed and logged no stub). Code:
`Game::playMusic / sceneMusic / skipMusic / stopMusic / pauseMusic / loadMusicBank` (`game/src/game/Game.cpp`), the scene's
"Music Track" string (`GameScene::musicTrack`), the pause hook `omf_SDLPauseChannel` (`engine/sdl/OMediaSDLSound.cpp`).

## What the exe does
| address | what |
|---|---|
| 0x40b1c4 (in 0x40af40, the chapter-change block) | the old music bank (game+0x60) is released, `music\c%d.omt` is opened (relative path, else `%smusic\c%d.omt` on the CD). Only on a chapter change. |
| 0x40ca60, game vt+0xc4 `PlayMusic(name)` | same name as the current song (game+0x1c6): return true. No bank, or the bank has no `Wave` called `name`: return false (the old song keeps playing). Else channel 3: stop, flush; old song object (game+0x6c) freed; new song object loaded from the bank; name stored; `play(3, song, queue=1, loop=1)`. Returns true. |
| 0x4385c0, scene vt+0x108 | the scene's song, called by the scene enter routine 0x437cf0 at "here 5" (after the visibility refresh, before the enter event). Tries in order, stops at the first that `PlayMusic` accepts: (1) c1l5s5..s7 (ids 0x50501/0x60501/0x70501) with game+0x206 set, that name; (2) c3l2s3..s5 (0x30203/0x40203/0x50203) with game+0x1e6 set, that name; (3) the camera's "Music Track" (camera +0x218, scene +0x480) when not empty; (4) `c%dl%ds%d`; (5) `c%dl%d`; (6) `c%d`. |
| 0x40c860, game vt+0xfc | script command `skip` (jukebox in c3l2s3, radio in c1l5s7): finds the current song among `c<chapter>l1..6`, takes the next level (6 wraps to 1; not found: level 1), keeps the name in game+0x206 (c1l5s7) or game+0x1e6 (c3l2s3), and plays it. |
| 0x40af40 / 0x40ae40 (scene switch, reset) | stop every channel except channel 3: the song runs on across scene changes. |
| 0x40c620 (Game dtor 0x409d52) | stop all 15 channels (music too), free the song, clear the three names (+0x1c6/+0x1e6/+0x206). |
| 0x40c730 / 0x40c780 (game vt+0x7c / +0x80) | pause / resume the buffer of channel 3. The window message handler uses them on deactivate / activate and for the game's pause state (state +0x164 = 3). The sound effects have their own pair (vt+0x74 / +0x78, channels other than 0 and 3). |
| 0x40c3b0 | Options volumes: channel 3 = Music, `6*level+201`, 0 for level 0 (already in `StartupMain.cpp`). |

Movies (Bink, 0x429b..) have their own sound path and do not touch channel 3. The main menu / Options screen play no song (PlayMusic needs
a Game and its bank, the exe's menu is a separate screen).

Songs per bank (names in `music/c1..c4.omt`, type `Wave`): the levels `c<n>l<k>`, some scene songs (`c1l5s5`, `c2l5s2`, `c2l5s4`, `c3l2s3`,
`c3l4s4`, `c4l3s5`, `c4l4s6`), `tempo`, `machine`, `c2`, `c2l3`.  Scenes whose Music Track names a song: c1l3s6 `c1l4`, c1l5s6/s7 `c1l5s5`, c1l5s8/s9
and c3l4s9, c4l3s8/s9 `tempo`, c2l2s3..s7 / c2l4s1,s2 `c2l3`, c2l3s3..s5 / c2l4s9 `c2`, c2l4s6 `machine`, ...

## Port notes
* The song object belongs to the bank (a database object, locked); `music_` is just a pointer, cleared before the bank closes.
* `Game::loadMusicBank` uses its own chapter tracker: `state_.chapter` is set before `loadSceneBanks`, so it cannot detect a change.
* Pause behind the menu (port): `StartupMain` pauses the song when the game is left for the menu and resumes it on Resume Game (a
  `paused` flag on the SDL voice; the sample position is kept, like DirectSound's Stop / Play).
* New Game / ending: the old `Game` is deleted, its destructor stops channel 3.
* Music volume 0: channel 3 volume 0 (silent but still running, like the exe).
* Log lines (`SB_GAME_LOG=1`): `music: bank ... open`, `music: bank cN song 'x' channel 3 volume V loop Ns playing`, `music: '...' already plays`,
  `music: bank cN has no song '...'`, `music: pause|resume|stop '...'`.

## Verified (desktop, dummy audio; SDL `disk` audio shows a non-silent mix)
* New Game -> c1l0s1: no song (none of c1l0s1 / c1l0 / c1 exists) -> c1l2s6: `c1l2`; c1l3s6: `c1l4` (Music Track); c1l5s8: `tempo`; c1l3s4 after a click: `c1l3`.
* c2l3s3: `c2`; c3l2s3: `c3l2s3`; c4l3s5: `c4l3s5`; c4l1s1: `c4l1` (bank change per chapter).
* Pause behind the menu and resume (F10, Resume Game): `pause` / `resume` logged; Music=0 in the settings: volume 0.
* chapter 1 route, ASan/UBSan and smoke: see the lane report.
