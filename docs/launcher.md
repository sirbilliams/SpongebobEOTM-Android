# Launcher (Android): the living underwater scene

The app's own menu before the game (`SetupActivity` + `LauncherView`), not the game's in-game menu. Everything is drawn
in code on a Canvas: no bitmaps, no fonts, no show art. Text is generic ("Employee of the Month", "The port launcher").

## Scene (top screen, 1920 x 1080 stage, letterboxed)
- **Dock overhead**: deck boards seen from below with light through the gaps, a crossbeam with bolts and a weed fringe,
  two pilings (the right one is a painted depth gauge that scrolls while diving).
- **Light shafts** from the deck gaps drift and breathe; they fade with depth.
- **Three depths**: far ridge, kelp and coral (blue-tinted); mid sand, kelp, coral and rocks; front sand with ripples,
  light dapples, bright coral and edge kelp. Kelp is a swaying ribbon (phase travels up the strand) bent by a slow current.
- **Bubbles**: ambient risers, a vent stream in the gap between signs and bottle, a 48-slot pool for taps / dives / select.
- **Jellyfish** (3): pulsing bells with halo and tentacles; a tap in the water scoots nearby ones away and dims them.
- **Menu**: wooden signs hanging on ropes from the title plaque, chained sign to sign. The focused sign glows like a
  jellyfish (pink/violet halo, pink rim, lit bottle icon), swings forward (scale + shift + a damped spring kick) and its
  ropes pull taut and lighten. Labels are cream on dark wood.
- **Bottle**: a corked glass bottle tethered to a stone holds the page's parchment: title, detail, status lines (word
  wrapped once per page), an install progress bar (a trailing `NN%` status line), and on Play the save card.
- **Dive**: each page has a depth (main 0; Play / Import / About 1; Saves / Settings 2; Engine room 3). Changing page
  eases (cubic in-out, 860 ms) the water column, gauge, bubbles and floor; the old signs and bottle leave up (diving) or
  down (rising) while the new ones are hauled in on their ropes. Re-showing the same page (a setting toggled) never dives.
- **Hint bar**: + Move, A Select, B Back, "Tap the water"; "Installing - please keep this screen open" while importing.

## Pages and functions (unchanged prefs / env, see SetupActivity)
Main: Play, Import game, Saves, Settings, Engine room, About, Quit. Play: Start game (or Import first) with the
**Continue card** (the save Resume loads: `Save=` from `settings.cfg`, else the newest save; its SHOT thumbnail, chapter,
place name from `SaveFiles.prettyName` = Java copy of `CompanionFeed.h companionPrettyName`, date) and the player's
**box art** in a driftwood frame. Settings: widescreen, bottom-screen panel, bottom screen touchpad/panel, panel
window focus, right-stick cursor, cursor theme, **Box art** (system picker; a JPEG copy, at most 1280 px, in the app's
files as `boxart.jpg`; never bundled; Pick another / Remove). Engine room: Share log, Touch test, App version, Reset
settings, Crash test, Reinstall. About: version, licences.

**Saves** ("Ship's locker", `SaveFiles`): Export all (one .zip via ACTION_CREATE_DOCUMENT), Export one (pick a save from
a list), Import (.zip via ACTION_OPEN_DOCUMENT). Import reads only `slotN.sb2` / `quick.sb2` entries (any folder, at most
128 entries, 4 MB each, `SHOT` magic checked), then previews every save (thumbnail, slot, chapter and place, date, "free
slot" / "replaces the save in this slot") before writing. "Replace" keeps the names; "Into free slots" puts each save
(quick save too) into an unused slot number. Files are written as `.tmp` + rename into `saved/` only.

## Bottom screen (`LauncherBottom`)
While SetupActivity is resumed and "Bottom-screen panel" is on, a Presentation on the Thor's built-in bottom screen
(`BottomScreen.find`, never a TV / cast display; FLAG_NOT_FOCUSABLE) shows `LauncherView` in bottom mode (1240 x 1080):
the same water at depth 1.4, a "Harbour office" plaque, the last-save card with a big thumbnail (or the box art), four
chapter pearls, and four brass dials (Widescreen, Right stick, Cursor, Bottom screen) that cycle the same prefs as the
Settings rows (`SetupActivity.applySetting`).

## Input
D-pad / left stick (latched at 0.6, released under 0.3) move, A / Enter / D-pad centre select (no repeats), B / Back go
back. Touch: down on a sign focuses it, up on the same sign selects; a touch in the water spawns bubbles and startles
jellyfish. Hit tests use the signs' resting rectangles (`LauncherView.rowTop/rowLeft/rowWidth`); none during a dive.

## Performance
Shapes (rays, ridge, sand, ripples, beam grain, fringe, jelly bell, bottle) and every shader are built once in the
constructor; per frame only scratch Paths are reset and refilled (kelp, ropes, tentacles), no objects are allocated.
Particles are capped (34 ambient bubbles, 44 snow specks, 48 pooled bubbles, 3 jellyfish, 21 kelp strands). Frames are
driven by `postInvalidateOnAnimation()` (vsync), dt clamped to 50 ms; a stopped Activity draws nothing.
**Reduced motion**: with the system animator duration scale at 0, the scene is drawn once at a fixed time, focus moves
instantly and pages switch without a dive.

## Sound (`LauncherSound`)
Synthesised in code at start-up (no recordings), played by SoundPool at low volume: a bubble blip on focus moves, a
rope-on-wood creak plus blip on select, a bubble cluster on water taps.

## Desktop renders
`tools/launcher/render.sh [dir]` (inside the omtbuild box; default `.shots/launcher2/`) compiles the real
`LauncherView.java` against Java2D shims (`tools/companion/shim` plus `tools/launcher/shim` overrides: paint alpha,
curves, multi-stop gradients, bitmaps, a stepped clock), runs `VerifyLauncher` (navigation, stick latch, A/B, touch at
three aspect ratios, import lock, bottom dials) and renders every page, three idle frames, a mid-dive and a mid-rise
frame, the tap reaction, an import in progress, reduced motion and the bottom screen (with and without a save).
Thumbnails and box art in the renders are made-up test pictures.
