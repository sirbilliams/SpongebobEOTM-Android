# Hotspot reveal and the Voices gallery (port features)

Both are port-only (`// port:`), drawn in code with the game's own font and bamboo menu frame; no show art is added.

## Hotspot reveal
Code: `game/src/game/Reveal.{h,cpp}`, `GameScene::collectClickable` (Scene.cpp, next to the pick passes).

- What is shown: every object the click pick (0x43a310 passes: TRIG, CHAR/ITEM via World3D::pick, 2D/ANIM/SPOT) could return, with a soft glowing
  rounded outline around its click rectangle (0x415e00 `x1,y1,x2,y2`) and its Description as caption. `collectClickable` applies the same
  visibility tests as the picks (Visible Condition, "use Condition" triggers, other-height connectors, hidden 2D objects, Clickable and the model
  shown for CHAR/ITEM, rollover Condition for SPOTs), so nothing the game hides is revealed. The player's own character is skipped (clicks on it do nothing).
  Objects with no rectangle, or no description and no event, are skipped.
- Look: one 640x480 ARGB canvas element (z 10150) redrawn each frame while visible (outline = bright core + dark edge + cyan glow), captions as
  `OMediaCCaption` (z 10160, same as the hover text) nudged so labels do not pile up. Everything lives in the logical 640x480 world, so all widescreen
  modes (fit, panels, fill, stretch) map it through the existing transform. Fade: 400 ms in, hold, 400 ms out; captions appear once the outline is about a third in.
- Triggers: gamepad L1 held (`SDL_CONTROLLER_BUTTON_LEFTSHOULDER`; needs the controller to be opened, i.e. the pad cursor option on), Tab held (desktop),
  companion command `reveal` (`CompanionFeed.h`, one marked branch in `companionCommands`; a pulse of about 2 s). Hooks: one line in the StartupMain game event loop
  (`sbgame::revealEvent`), one in `Game::update` (`reveal_->update`).
- Off during cutscenes (letterbox / running script), dialogue, and scene changes.
- Tests: `SB_TEST_REVEAL=on` holds it; `SB_TEST_REVEAL=<n>` pulses at game frame n; `SB_TEST_COMPANION=<tick>:reveal` with `SB_COMPANION_DUMP` sends the command. Log lines `reveal: on/off` (`SB_GAME_LOG=1`).
- Verified: 1920x1080 PNGs in `.shots/` (not committed): c1l5s4 in `fit` and `panels`, c1l2s2 in `fill`.

## Voices gallery (View Movies, page 2)
Code: `game/src/ui/VoiceGallery.{h,cpp}`, `MainMenu.cpp` (`voicesPage`, ids 400-503, page 5), `StartupMain.cpp` (`menu_progress`), `GameState` (`heard_`, `dialogueEntries`), `DialogueUI::startVoice` (records a heard voice).

- The View Movies page (0x4246a0) gets a `Voices` button beside `Ok`; Esc / pad B steps back one level, then to the movies.
- Pages: chapters (with "n of m") -> speakers of the chapter -> that speaker's clips, 10 per page, arrows for pages. A clip not heard is `???` and cannot be clicked.
- Clips: every line of `dialogue/c%dl%ds%d.sb2` that has a wave in `sounds/c%dl%ds%d.omt` (same 7 case combinations as 0x412d00). Speaker = the line's speaker object name (small display table, else the raw name).
- Heard: the voices started in this run (GameState `heard_`, not saved: the save format is the game's) plus the saved dialogue progress (0x416d30 records, the THRD list):
  lines before the group's current line, or the whole group if finished. Source of the progress behind the menu: paused game, finished game, else the last used save slot (`menu_progress`).
- Playing: the voice channel (4) of the default sound manager, like `DialogueUI`; log `voice gallery: play 121AAA ...`.
- Test seed: `SB_TEST_VOICES_HEARD=all` or `c1l2s1,c1l2s2:AAB,...` (a scene = all its clips, `scene:KEY` = one). Menu clicks in a run: `OMT_MENU_CLICKS` (window pixels, small frame numbers; y must be strictly inside the caption box).
- Verified: chapter list, speaker clips with `???`, and a click playing `121AAA` (log), at 1920x1080 panels.
