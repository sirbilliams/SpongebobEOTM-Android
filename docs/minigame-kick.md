# Beachball kick

The supplied task calls this chapter 3, but the installed game and executable identify it as
**chapter 1, c1l5s1 (Goo Lagoon)**. Decoding every shipped script finds `kickit` only in
`scripts/c1/skick.sb2`. Chapter 3 has darts. The shared 0x4354d0 mini-game object has no kick state:
`kickit` calls the active CHAR's vtable +0x16c. `MiniGame.{h,cpp}` is unchanged.

Implementation: `game/src/game/Kick.{h,cpp}`; small marked hooks in World3D, SceneScriptHost and
ScriptRunner. No art is added. The scene background, beachball, goal, SpongeBob, shadows,
`kick`, `bounce`, `dance01`, `ball2` and `cheer` come from the player's files.

## Executable map and rules

- Dispatcher 0x43e110, `kickit` at 0x440213: call actor +0x16c, replace the waiting command name
  with `walk`, return blocked. The port now also makes that command-name change.
- CHAR vtable is **0x44e358**, not the base class vtable 0x44d83c. Its +0x16c is 0x435160.
  Set the special movement flag (+0x410), temporarily disable control/grid routing, and target
  the current ball position plus **400** times the normalized vector away from the player.
  Normalization uses OMT.dll 0x10032cd0 `quick_magnitude`, checked against the shared table-based
  implementation. There is no charge, random outcome, score counter or aim overlay.
- 0x434650 sets arrival tolerance to max(25, radius/2), skips the walk/rotate animation changes
  for special movement. 0x42f1a0 advances this running movement at **0.3 units/ms**. A blocked
  candidate position reverses the remaining travel and moves backward by this frame's step;
  if already touching the matching goal, it instead sets the target to the current position.
  Completion plays idle1 and wakes the waiting walk. Animation expiry cannot wake kick early.
- Collision 0x431940 / grid conversion 0x418170 uses the walk grid at the actor's elevation;
  a boundary belongs to the lower cell. Door animation and `terrain off` bypass grid checks.
- +0x174 = 0x431870: find a TRIG in the contact list whose Accept Item equals the actor name,
  case-insensitively. The port computes these contacts directly using 0x4313e0's strict XZ
  overlap test, equal elevation, and sum of radii. Scene c1l5s1's goal TRIG accepts `beachball`.
- +0x188 = 0x431a90: project the actor centre and test outside x=0..639 or y=0..479.
- `goal`, 0x44027b: on success play cheer, select sb and wait for dance01; append `setchar
  beachball`, `move 150 -200`. On off-screen failure append just `move 150 -200` for the ball.
  A non-scoring on-screen shot stays where it finishes, allowing another kick. Existing script
  rewriting supplies these callbacks; the false-returning ScriptHost stubs are replaced.

The standard mouse/touch/controller cursor event path picks the beachball and sends `skick`.
Move SpongeBob to the desired side of the ball first. No new input handler is used. Mouse replay
below goes through SDL events, the same Game::mouseDown / scene picking path as touch/pad clicks.
Touch/pad regressions are also covered by smoke.sh; direct hardware interaction was not tested.

Port guards are marked: coincident centres decline a direction; a long frame cannot overshoot
its target; a missing model releases its script; projection at the camera plane is treated as off-screen.
The generic renderer/model fidelity limitations remain those in scene-runtime.md.

## Verification and reproduction

All game runs: Xvfb **:84**, 1920x1080, dummy audio, SB_SKIP_MOVIES=1, SB_WIDESCREEN=fit.
`distrobox` is unavailable here; builds and runs used the host's SDL/FFmpeg dependencies.
All captures/logs are in `.shots/kick/`, untracked. BMPs are converted to PNG with a host Python
standard-library converter (`bmp_to_png.py` in that directory).

**Untouched-start win:** `natural-win.log` and `natural-win-*.png` play from the original
scene start with no setup script. SpongeBob walks around the ball, then two mouse kicks score.
Use the common environment described above with OMT_FRAMES=1550, OMT_SHOTS=100,750,900,1050,1200,1400,1500,
OMT_SCREENSHOT=../.shots/kick/natural-win.bmp, no SB_TEST_SCRIPT, and
`SB_TEST_GAME_CLICKS=150:186:292,850:367:260,1100:477:255`.
The log ends with the ball at approximately (914,-107), `goal=1 missed=0`, cheer and reset.

The focused callback tests used temporary developer-format scripts in the installed copy only
(removed after testing; preserved in `.shots/kick/fixtures/` for reproduction):
`skicksetup.txt` sets beachball to (700,-110), sb to (580,-110); `skickloss.txt` sets beachball to
(-1250,200), sb to (-1130,200). Each uses `setchar` and `move`; neither changes rules, terrain,
callbacks, random state or outcomes. These positions shorten the replay and isolate both callbacks.
The original `skick` is still dispatched by the mouse click on the ball.

From `.game`, run `../build/spongebob` with:

```
LD_LIBRARY_PATH=$HOME/Projects/sbeotm-private/ffmpeg/lib:/tmp/c-ffmpeg/lib
DISPLAY=:84 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=1920x1080
SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c1l5s1 SB_GAME_LOG=1 SB_WIDESCREEN=fit
OMT_FRAMES=600 OMT_SHOTS=140,210,270,330,450,570 OMT_SCREENSHOT=../.shots/kick/win.bmp
SB_TEST_SCRIPT=80:skicksetup SB_TEST_GAME_CLICKS=170:514:250
```

Win logs `click ... Beach Ball`, event `skick`, kick target (1100,-110), `goal=1 missed=0`,
and the cheer channel. Later PNGs show the ball reset at (150,-200) after the dance.
For loss: OMT_FRAMES=500, `SB_TEST_SCRIPT=80:skickloss`,
`SB_TEST_GAME_CLICKS=170:12:200`, output `loss-reset2.bmp`. This logs a terrain bounce,
then `goal=0 missed=1`; the ball resets. `play`, `full-play`, and `court-play` additionally record ordinary floor walking and non-scoring kicks
from the untouched scene start (including a terrain rebound); the focused win/loss fixtures
above establish both script callbacks. `initial` records a non-scoring shot from the original
scene start; `loss` records an on-screen bounce and retry position.

ASan/UBSan builds use both C and C++ sanitizers and -fno-omit-frame-pointer. Leak detection is
disabled for the existing graphics/audio stack. `asan-win.log` and `asan-loss.log` replay the
same mouse-driven outcomes without sanitizer diagnostics. All-target build passes; `sb_unittest`: 72 checks, 0 failed; `git diff --check` passes.

Regression: `SUMMARY: 16 passed, 0 failed  out=/home/bill/Projects/sb-lanes/d/.shots/kick/smoke`.
No commit: the lane brief reserves git metadata/commits for the lead.
