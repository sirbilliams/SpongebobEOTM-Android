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
(`Companion.command` -> queue -> `companionCommands` on the game thread), plus `hint`, `reveal`, `travel:<target>`, `voice:<n>`, `combine:<a>:<b>`; the snapshot also has `map, mapbusy, talk, combine, goals` (see Pages).

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
  speaker. The voice bank `cd/sounds/help.omt` (the CD folder; also tried: `sounds/help.omt`) is played by the game itself: `Hud::hint` (hint button, and the panel command `hint`) plays the read line's voice on channel 4 (0x419210). The panel has no play button.
* `menu`: sends the F10 key (`CompanionKey`, SDL_PushEvent): the same path as F10 / Android Back, the game is paused behind the main menu (Resume Game).
* `skip`: a dialogue line -> `DialogueUI::onKey(' ')` (the click that closes a line); else in a cutscene (`GameScene::letterboxed()`: the cinematic bars are in,
  also `cut`:1) the exe's click handler 0x41ae00 -> `ScriptRunner::skipToLetterboxClose` (0x43df30) + `Game::wake` (scene vt+0xe0); during a Bink movie the game loop is
  blocked, so `CompanionPush("skip")` sends a key press, which ends the movie like any key (0x40bcc0). `StartupMain::game_movie` brackets the movie with
  `CompanionMovie`. The panel's SKIP porthole is lit only in a cutscene or on a dialogue line. `SceneScriptHost::gameSkip` (the script command) is still a stub.
  Taps older than 1.5 s are dropped (a tap made while paused or in a movie must not fire later).
* Test hook: `SB_TEST_COMPANION="60:switch=smeanwhile1,200:skip"` (tick:command list; `switch=` runs `Game::switchTo`).
* Map: the trail on the Pocket page shows the names of the scenes visited (not tappable); the real map is the Map page, below.

## Pages (Pocket, Map, Talk, Goals)
A tab row under the header plank (`tab:<n>`, panel-local) switches pages; it is there in Panel and in Touchpad mode (Touchpad: the Pocket tab is the pad with its slim pocket row,
the other tabs show their page in full). A question to answer (dialogue choices) always shows the Pocket page. The row ends with a REVEAL button that sends `reveal` (the
game side, which shows the hidden hot spots, is lane e's; until it lands the command is ignored). All pages are drawn in the same wood / rope / bubble style, no show art.
* **Map** (`map[]`, `mapbusy`): the in-game map is the scene `c<N>l1s1` (the HUD's map button switches to `c<N>l1s1p1`). `companionMapAreas` (CompanionFeed.h) reads that scene
  file from the player's folder (`sb::scene::load`) and takes every 2DOB / SPOT with a scene as its Event (`c1l6s1p1`), exactly the objects the map shows. Same rules as the game:
  a 2DOB is clickable only when its Visible Condition holds (`121caa` etc., `Game::cond`); the base event (0x415a70) takes Event or Alt Event by the Condition; the CHEAT spots
  (condition `debug`) are left out. The panel places each place at its click rectangle on the 640x480 map. Name = the map object's own Description ("Go to Downtown" ->
  "Downtown"); a place that is not open and not visited stays a grey "?" (no spoilers). Visited = a scene of that level is in GameState's entered-scenes list (also kept in saves).
  Tap an open place -> `travel:<target>`; the game re-checks the same rule (`companionTravel`) and refuses (log `companion: travel ... refused`) when the map would not offer it,
  when the target is not on the map, or while a dialogue / cutscene / script runs or a scene switch waits (the map button is dead then); else `Game::switchTo(target)`, which is what clicking the
  place on the map scene does. A not-open place only shows a note ("Not open yet").
* **Talk** (`talk[]`): every spoken dialogue line of the session (speaker + text, oldest first, 60 kept, newest at the bottom; it keeps to the bottom unless scrolled up, drag to scroll).
  `DialogueUI` has two small `// port: companion` getters (`currentKey`, `lineSerial`); `companionRecordTalk` records a line when it starts. The round button sends `voice:<serial>`:
  `companionReplayVoice` finds the line's voice in the scene's sound bank with the same seven case-combinations as `DialogueUI::startVoice` (0x412d00; the bank of the line's scene is opened
  for a line from another scene) and plays it on channel 4, the dialogue's own channel; refused while a line is being said. Lines without a voice file get a dimmed button.
* **Combine**: on the Pocket page, drag a card onto another (a drag starts after 26 px; the target lights up) -> `combine:<a>:<b>` -> `Hud::combineFromPanel`, which is `dropAt` onto a
  slot: `Hud::combine` (0x421880, the six recipes) or, if it fails, `switchTo("H000F")`, the game's own refusal comment. The snapshot's `combine{n,ok,t}` carries "Made: <name>" or the
  comment text (`Game::lastCommentText`, set in `showComment`), shown for 6 s in a banner. Works on the big cards and on the Touchpad layout's slim row. A short tap still takes the item in hand.
* **Goals** (`goals{sv,st,sc,places[]}`, `hint`): the chapter's scenes found in the player's `scenes/` folder (levels >= 2) against the entered scenes (GameState), per place with a bar, the number of
  scripts seen (`GameState::scriptsSeenCount`), and WHAT NEXT = the hint the HINT button resolves (`Hud::hintText`, first help line of the chapter whose condition holds), or "Nothing to suggest yet".
* GameState got two read-only `// port: companion` getters (`scenesEntered`, `scriptsSeenCount`), Game one (`lastCommentText`, `commentSerial`), Hud `combineFromPanel`.

## Tests
* `tools/companion/test.sh <build> <game> [out]` (inside the box, own Xvfb :84): runs the game headless with `SB_COMPANION_DUMP` and `SB_TEST_COMPANION`: dialogue history after a conversation, travel
  refused for a locked place / a target not on the map / while a conversation is open, travel allowed to an open place (and the scene is entered), combine of two items (`SB_TEST_ITEMS`) and the refusal comment.
* `tools/companion/panel-test.sh` (inside the box): the real `CompanionView.java` on the Java2D shim: tabs, tap on an open place sends `travel:`, a place that is not open sends nothing, a drag of a card onto another sends `combine:a:b`.
* Renders: `tools/companion/render.sh <snap> <png> page:map|page:talk|page:goals[:pad]|combine|combine:mid|banner` (`samples/pages.json` has invented place names and lines; a snapshot written by the game renders with its own data).
