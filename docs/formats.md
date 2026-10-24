# Game file formats (from sb2.exe's own code)

Addresses are in the retail `sb2.exe` (Aug 2002, MSVC 6, image base 0x400000). Everything here was read
from the code that loads the files; nothing from the files themselves is stored in this repo.

## Paths

| What | Path (relative to the install folder) | Read by |
|---|---|---|
| Script | `scripts\c%d\%s.sb2` (dev fallback `scripts\c%d\%s.txt`, `scripts\%s.txt`) | 0x43dae0 (text fallback 0x43d750) |
| Scene | `scenes\c%dl%ds%d.scn` | 0x437210 |
| Scene art | `sceneart\c%dl%ds%d.omt` (OMT database, object "background"), `sceneart\c%dskies.omt` | 0x437210 |
| Dialogue | `dialogue\c%dl%ds%d.sb2` (dev fallback `.txt`), `dialogue\comments.sb2`, `dialogue\help.sb2` | |
| Sounds | `sounds\c%dl%ds%d.omt`, `sounds\c%d.omt`, `sounds\items.omt`, `sounds\comments%d.omt`, `<cd>\sounds\help.omt` | |
| Music | `music\c%d.omt` | |
| Models | `models\people\%s_%s.grn` and other `models\...\*.grn` (Granny 1.2b) | |
| Movies | `movies\%s.bik`, `Movies\mSpongeBob.bik` (on the CD; the importer copies them) | |
| Saves | `Saved\slot%d.sb2`, `Saved\quick.sb2` (the shipped ones are empty files) | 0x40e160 |
| Settings | registry `Software\THQ\SpongeBob 2\Settings` (port: a settings file) | |

Several paths start with `%s` = the CD/data root (global at 0x457314).

## Scene ids

A scene is named `cXlYsZ` (chapter, level, scene; e.g. `c1l0s1`, `c1l2s6p1`). The name is parsed
(0x40bcc0: checks the letters C, L, S) into a packed id at `scene+0x264` (`param_1[0x99]`): chapter in
bits 16-19, level in bits 8-11, scene in bits 0-3. Movies are switched to like scenes, by name
(`mlegal`, `mthq`, `mawe`, `mnick`, `mspongebob`, `mintro`, `mchapter2..4`, `mend`, `mcredits`).

## .sb2 scripts and dialogue (0x43dae0)

```
u16 count
count x { u16 len; u8 text[len] }     each text byte XOR 0x30
```

Each decoded record is one line, tab-separated, ending in `\n`. The loader splits it with `strtok`
(delimiters at 0x454c0c) into up to 4 fields of 32 bytes, stored as 128-byte records at
`runner+8 + i*0x80` (command at +0, args at +0x20, +0x40, +0x60); `runner+0x1908` = count,
`+0x190a` = current line (starts at -1). The runner then steps (0x43e110) until a command blocks.

Script files hold commands; the dispatcher 0x43e110 is a `strcmp` chain (0x44b2d0) over about 60
commands: walk checkgrid walkgrid move movey movegrid skipif exitif blending shadow terrain hideattach
showattach attach shake detach switch loop wait unlock video comment sound narrate idle climb movie
sparkle block unblock rotate setchar skip script hide save undo control talk transition close open
letterbox init texture setdoor show swapchar startanim anim kickdrop kickup anim2d shock slots spin
maze kickit goal darts, plus a few short keywords (on/off etc.). Order and exact semantics come from
the dispatcher itself, not from this list.

Dialogue files use the same container; each line is a table row:
`scene  branch  node  line  speaker  listener  text  flag  flag  flag  action` (action is `0` or a
script command such as `anim sb emoexplain`).

## .scn scenes (0x437210)

An OMT file stream (`OMediaFileStream`), header `RMAC` (= FourCC `CAMR` stored little-endian),
versions 22 and 23 occur. Read order:

1. the scene camera object (streamed in by `scene[0x9a]->vtbl+0x20`);
2. up to 5 element records (`scene[0x13c + i*0x69]->vtbl+0x20`), FourCC `TILE`;
3. optional `STPT` chunk: 10 start points of 16 bytes (`scene+0x6f7*4`);
4. two optional `GRID` chunks (walk grid at `scene+0x367*4`, second grid at `+0x52f*4`): 5 floats
   (size, ...), then a (2*size) x (2*size) byte grid stored with 30-byte rows;
5. element records until end of file (0x42a480): FourCCs `CHAR` (characters), `2DOB` (2D objects), ...

A chunk with an unexpected FourCC is skipped back (`seek(-4, cur)`).

## Startup (0x40bcc0, 0x40e470)

A named-state chain: when a movie ends the game switches (virtual `+0xb0`, logged as "Switch to scene
%s") to the next name: legal, THQ, AWE, Nick, SpongeBob logo, then the main menu. A new game plays
`mintro` and switches to `c1l0s1`. `-sw` on the command line (`sb2safe.bat`) selects the software
rasterizer.
