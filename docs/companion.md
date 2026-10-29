# Dual-screen companion panel (AYN Thor)

The game runs on the top screen (1920x1080). The bottom screen (1240x1080, touch) shows a companion panel, like the
Jimmy Neutron ports. Everything is drawn in code (wood planks, rope, bubbles, kelp, no show art).

## Pieces
* Android (`android/app/src/main/java/com/sirbilliams/spongebob/`): `BottomScreen` (picks the Thor's built-in second
  display, never a TV / cast display), `CompanionPanel` (a `Presentation` on it, polls the snapshot 10x/s, never takes
  focus), `CompanionView` (the drawing and the touch targets), `Companion` (JNI). `SpongeActivity` starts it in
  `onResume`. The launcher's main page has "Bottom screen panel: ON/OFF" (pref `spongebob` / `bottom_panel`; default
  on when a second display exists).
* Game (`game/src/port/`): `Companion.{h,cpp}` is game-independent (snapshot JSON, command queue, JNI
  `Companion_nativeSnapshot / nativeSetEnabled / nativeCommand`). `CompanionFeed.h` (header-only, needs `game/src/game`)
  builds the snapshot from `Game` / `GameState` / `DialogueUI` / `Help` and executes the panel's taps.
* `DialogueUI` got two small `// port: companion` methods: `choiceTexts()` and `pickChoice(n)`.
* Desktop render: `tools/companion/render.sh <snap.json|-> <out.png>` compiles the real `CompanionView.java` against a
  Java2D shim (`tools/companion/shim`) inside omtbuild; samples in `tools/companion/samples/`.
  `SB_COMPANION_DUMP=<file>` makes the game write its snapshot to a file (desktop), usable as input to render.sh.

## Wiring left to do (lane b owns game/src/game, so not done here)
In `Game::update` add one line: `sbport::companionTick(this, dt);  // port: companion` (include `../port/CompanionFeed.h`),
and link `port/Companion.cpp` into any target that uses it (`spongebob` globs it already). The panel stays empty
("Waiting for the game") until then.

## Snapshot (JSON) and commands
Fields: `game, chapter_title, scene_title, cut, hint, caption, sel, items[], visited[], dlg{choice, speaker, text, choices[]}`
(see `Companion.h`). Taps go back as `item:<id>`, `noitem`, `choice:<n>`, `advance`, `menu`, `skip`
(`Companion.command` -> queue -> `companionCommands` on the game thread).

## What the panel shows
Header plank (chapter, scene, MENU and SKIP portholes); hint ticket (when there is one); while a dialogue is open
either the choice buttons (big, numbered) or the current line with a Next button; the inventory as large cards (tap =
in hand, tap again = put away; pages when there are many); the trail of the last scenes entered.

## Stubs / missing game state
* Item in hand: stored (`companionSelectedItem()`), but the game's inventory bar / cursor (0x4201c0, game+0x88) is not
  reconstructed, so nothing uses it yet. It should call `GameScene::useItem(obj, item)` on the next click.
* Scene and chapter names: no name table known; shows "Chapter N" / "Level L, Scene S". Item names: the id prettified
  ("golden_key" -> "Golden key"); item art is in sounds/items.omt-adjacent art, not read; badges show initials.
* Hint: first `help.sb2` entry of the chapter whose condition holds; how the exe picks one is unknown.
* `skip`: goes to `Game::switchTo("skip")` -> `SceneScriptHost::gameSkip` (itself a stub). `menu`: logs only (no menu).
* `cut` (cutscene flag) and `caption` (comment text) are never set: `Game` has no getters yet.
* Map: only the list of scenes visited (no scene-name table, no layout); chips are not tappable.
