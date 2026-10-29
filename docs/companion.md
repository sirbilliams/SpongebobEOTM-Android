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
Fields: `game, chapter_title, scene_title, cut, hint, caption, sel, items[], names[], icons, visited[], dlg{choice, speaker, text, choices[]}`
(see `Companion.h`). Taps go back as `item:<id>`, `noitem`, `choice:<n>`, `advance`, `menu`, `skip`
(`Companion.command` -> queue -> `companionCommands` on the game thread).

## What the panel shows
Header plank (chapter, scene, MENU and SKIP portholes); hint ticket (when there is one); while a dialogue is open
either the choice buttons (big, numbered) or the current line with a Next button; the inventory as large cards (tap =
in hand, tap again = put away; pages when there are many); the trail of the last scenes entered.

## Item pictures and names
`items[]` is the HUD's inventory (what is carried; used items are gone), `names[]` the display name of each (the `add` command's second argument, e.g.
"Shovel and Pail"). An item is a 3D model in the game (`models/items/<id>_model`), so its picture is a render: `Hud::ensureIcon` (called from
`companionTick` while the panel is on) draws the model once into an offscreen surface (96x96, twice, on black and on white, then un-matted to get alpha),
and writes `<id>.png` to `<game folder>/companion_icons/` (stored-deflate PNG, no zlib). The art is read from the player's files at run time and
cached on their device only, never committed. `icons` in the snapshot is that folder (absolute); `CompanionView` loads `<icons>/<id>.png` with
`BitmapFactory` (cached; a missing file is looked for again after 1.5 s, since the game writes it a moment after the item appears) and draws it in the card's bubble; without a
picture the initials badge is drawn as before. Desktop: `tools/companion/render.sh tools/companion/samples/inventory.json out.png` renders the sample with
stand-in pictures drawn by `shim/.../SampleIcons.java` (the sample's `"icons":"@ICONS@"` is replaced by the folder it writes); a snapshot written by the game
(`SB_COMPANION_DUMP=f.json`) renders with the game's own pictures.

## Stubs / missing game state
* Item in hand: `item:<id>` goes to `Hud::holdFromPanel` (the game's HUD, docs/scene-runtime.md "HUD and cursor"); the next click in the scene uses it
  (`GameScene::useItem`) exactly like an item taken from the bar; `noitem` puts it back. `sel` is the HUD's held item.
* Scene names: the game files name no scene, so `CompanionFeed.h` has a short hand-written place table (`companionPrettyName`, `// port:`), made from the
  scenes' own clickable labels ("The Krusty Krab - Kitchen", "Goo Lagoon - Lifeguard Station"; unknown rooms are "<place> - Area N"). `scene_title` and `visited[]` carry
  these names (no ids). Chapter stays "Chapter N".
* Hint: first `help.sb2` entry of the chapter whose condition holds (`Hud::hintText`); its text may be another line's id (`F121A` -> `F000D`), which lives in the
  chapter-0 lines. `HelpTable` now keeps those apart (`common_`, `// port:`) so `find` resolves them; before, the panel showed the raw id `F000D`. The hint has no
  speaker and no voice here (`sounds/help.omt` is on the CD, not in the install; the game's own hint button plays none either), so no play button.
* `menu`: sends the F10 key (`CompanionKey`, SDL_PushEvent): the same path as F10 / Android Back, the game is paused behind the main menu (Resume Game).
* `skip`: a dialogue line -> `DialogueUI::onKey(' ')` (the click that closes a line); else in a cutscene (`GameScene::letterboxed()`: the cinematic bars are in,
  also `cut`:1) the exe's click handler 0x41ae00 -> `ScriptRunner::skipToLetterboxClose` (0x43df30) + `Game::wake` (scene vt+0xe0); during a Bink movie the game loop is
  blocked, so `CompanionPush("skip")` sends a key press, which ends the movie like any key (0x40bcc0). `StartupMain::game_movie` brackets the movie with
  `CompanionMovie`. The panel's SKIP porthole is lit only in a cutscene or on a dialogue line. `SceneScriptHost::gameSkip` (the script command) is still a stub.
  Taps older than 1.5 s are dropped (a tap made while paused or in a movie must not fire later).
* Test hook: `SB_TEST_COMPANION="60:switch=smeanwhile1,200:skip"` (tick:command list; `switch=` runs `Game::switchTo`).
* Map: the trail shows the names of the scenes visited; the chips are not tappable (the game's map is a go-to-hub button, no travel list).
