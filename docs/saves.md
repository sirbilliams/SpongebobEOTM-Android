# Save games

Reconstructed from `sb2.exe`: the progress object's writer `0x416ed0` (`fcn.00416ed0`), its loader `0x417460`, the Save Game and Load Game
screens `0x427ef0` / `0x428bb0` / `0x4295d0`, the slot helpers `0x428740` (save button), `0x429120` (load button), `0x428950` (slot exists),
`0x428a70` (delete), the arrows `0x4291b0` / `0x4293c0`, the quick save `0x41db50` / `0x41dd30`. Code: `game/src/game/SaveGame.{h,cpp}`,
the screens in `game/src/ui/MainMenu.cpp`, the glue in `game/src/port/StartupMain.cpp`.

## Files

`saved/slot%d.sb2` (`0x455350`, slots 0..98) and `saved/quick.sb2` (`0x455340`) in the game folder. The installed game ships 100 empty
(0 byte) files there; a file with no bytes is not a save (`0x40e120` needs a length), so they count as free slots. The exe writes the slot
number it last used to the registry value `Save` (game+0x26c); the port keeps it as `Save=` in `settings.cfg` and uses it for Load Game's
first slot and for Resume.

Little endian, strings are fixed zero padded fields. The tags are the dwords below written as `u32` (so `'ITEM'` is the bytes `METI`).

| Part | Bytes |
|---|---|
| name | `char[0x20]` typed on the Save Game screen (`Quick Save` for the quick save) |
| magic | `u32 0x53484f54` ('SHOT') |
| thumbnail | `s32` byte count (-1: none), then the bytes: 320x240, 16 bits per pixel (port: RGB565, rows of 640 bytes) |
| scene | `u32` scene id (`scene+0x264`: scene<<16 \| level<<8 \| chapter) |
| player | `f32 x, y, z`, `s16 facing` of the controlled character (all 0 when none) |
| item bar | `s16 n`, then n * { `char[0x20]` name, `char[0x20]` the item's Description (ITEM +0x200), `s16` slot } (`0x421c40`); port: `Hud::toBar` before every capture, `Hud::fromBar` after a load |
| records, to the end of the file | each a `u32` tag and its fields |

Records (the list of the progress object they come from; loader function that stores each):

| Tag | Fields | Meaning |
|---|---|---|
| `ITEM` 0x4954454d | `char[0x20]` | items given (+0x20, `0x4169a0`) |
| `CHAR` 0x43484152 | `char[0x20]` | characters (+0x2c, `0x416c70`) |
| `THRD` 0x54485244 | `char[0x10]` group, `char[0x10]` current line, `s16`, `s16`, `u32` scene | dialogue progress (+0x38, `0x416d30`) |
| `SCRP` 0x53435250 | `char[0x20]` | scripts seen (+8, `0x416610`) |
| `SCEN` 0x5343454e | `u32` | flag ids (+0x14, `0x416a60`; a `C` condition term maps to one in `0x416ad0`) |
| `TEXT` 0x54455854 | `char[0x20]` object, `char[0x20]` texture | texture changes (+0x44, `0x4166d0`) |
| `POSI` 0x504f5349 | `char[0x20]`, `u32` x4, `s16` | character positions (+0x50, `0x416880`) |
| `BLOK` 0x424c4f4b | `u32` scene, `s16` x4 | blocked cells (+0x5c, `0x416ba0`) |
| `PUZZ` 0x50555a5a | 42 bytes (+0xc 6, +0x12 2, +0x14 2, +0x18 14, +0x26 2, +0x28 16 of the puzzle object) | mini game state; `HIGH` 0x48494748 (`s16`, the high score) follows when it is above 0 |
| `LAST` 0x4c415354 | `u32` | game+0x15c |

The writer order is ITEM, CHAR, THRD, SCRP, SCEN, TEXT, POSI, BLOK, PUZZ(+HIGH), LAST. The loader reads in file order and ignores the order.

Loading (`0x417460`): every list is cleared first (`0x4163b0`, item bar `0x41e160`), the records are added, then the chapter is set to 0xffff (so
the chapter's sound banks load), `game+0x252` is set, and the classifier (`vt+0xb0`) is called with `c%dl%ds%dp1` (`0x455164`): the saved scene at
start point 1. When the saved position is not (0,0,0,0) the progress object remembers it (`+0x64`) for the scene to place the player.

## Port differences

- Written to `<name>.tmp`, then renamed: a crash never leaves a half save. Only the `SHOT` format is read; an unknown tag stops the load.
- The thumbnail is read back from the 640x480 screen when the game is left for the menu (F10 / Back), scaled 2:1 by averaging. The exe saves the
  menu's snapshot canvas in the display's own 16-bit format; the port writes RGB565.
- The port's progress object (`game/src/game/GameState`) keeps containers, not the exe's lists. What the port sets today (scripts seen, items,
  dialogue progress, textures, blocked cells, flags) is saved and restored; the lists nothing in the port fills yet (item bar, `CHAR`, `POSI`,
  `PUZZ`, `HIGH`, flag ids set by id) are kept and written back unchanged. A flag set by name is saved as its 24-bit id.
  `BLOK` holds one cell, written as x, y, x, y.
- The player position: `World3D::update` copies the controlled character's x/y/z and heading into `GameState::hasPlayer / playerX/Y/Z / playerFacing` every frame (the writer 0x416ed0 reads them); `resumePos` + the same fields hold the position of a loaded save and `World3D::enter` puts the player there (heading in the same 0x4000 units). Verified: walk, F10 dump, Resume -> `sb restored from save: 283.448 -400 210.693 heading 10012`.

## Screens and flow

- **Save Game** (main menu item 2; only while a game is paused behind the menu, else nothing happens): the first free slot is selected
  (all 99 used: slot 0, an overwrite). The black 322x242 box holds the slot's picture (`empty` for a free slot), the typed name (letters,
  backspace; at most 30 characters and 250 pixels wide), the `line` / `cursor` pictures, arrows to browse (left: slot - 1; right: while the slot
  is used, up to the first free one), Ok (or Enter, or a click on the picture) and Cancel. Saving an used slot asks `Overwrite?`. After a
  save the menu closes and the game goes on (`0x4248b0`).
- **Load Game** (item 3): the slot of `Save=` (else the first used one) with its picture and name, arrows that wrap between the saved games,
  Ok / Enter / a click on the picture loads it, Delete asks `Delete Saved Game?` (`0x428a70`: the later slots move down one). No saves:
  `No Saved Games`.
- **Resume Game** (item 6): enabled (no `noresume` overlay) while a game is paused behind the menu, or when the last used slot exists; it returns
  to the paused game, or loads that slot. F10 / Back in a game opens the menu with the game kept alive; Esc on the top menu resumes.

## Android: export and import

Saves live in `saved/` of the app's game folder (`<game folder>/saved/slot0.sb2` ... `slot98.sb2`, `quick.sb2`; the folder the importer fills
with `sb2.exe`, `scenes/`, ...). To export a save a user copies that folder (or single slot files) out with a file manager or `adb pull`; to import,
copies the files back over the slot files while the game is closed. The files are self contained and not tied to the device. Slots are plain
numbered files: a new slot needs no index. Bill wants an export/import UI later: it is a copy of these files (or one zip of `saved/`); the
only rule for the UI is to import over `slotN.sb2` of an unused number, or to ask before replacing a used one.

## Tests (desktop, inside the omtbuild box)

- `SB_TEST_SAVEFMT=1 spongebob`: writes a populated state, reads it, applies it to a fresh state, writes again and compares the two files
  byte for byte (`SaveGame::selfTest`).
- Hooks in the game binary (tests only): `SB_TEST_SEED=1` (some progress at game start), `SB_TEST_SCENE=c1l2s6`, `SB_TEST_F10=<frame>` (leave to
  the menu once), `SB_TEST_PCLICKS=frame:x:y,...` (clicks in the menu over the paused game), `SB_TEST_MENUKEYS=frame:key,...` (a single
  character is typed), `SB_TEST_RESUME=1`, `SB_TEST_GAMEQUIT=<frame>`, `SB_TEST_DUMP_PAUSE` / `SB_TEST_DUMP_QUIT=<file>` (the state as a save file,
  to `cmp`), `OMT_CONFIG=<file>` (the settings, with `Save=`).
- Session run: new game at c1l2s6 with seed -> F10 -> Save Game -> Ok (slot 0) -> quit; new process -> Resume -> loaded `c1l2s6p1`;
  `SB_TEST_DUMP_PAUSE` of the first and `SB_TEST_DUMP_QUIT` of the second compare equal (`cmp`).
