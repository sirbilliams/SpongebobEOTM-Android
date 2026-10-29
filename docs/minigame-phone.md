# Chapter 4 phone keypad (c4l2s2)

Code: `game/src/game/Phone.{h,cpp}`, owned by the shared `MiniGame` object (`MiniGame::phone()`); `SceneScriptHost::miniPhoneKey/Reset`
forward the script command `phone` (dispatcher 0x440774, already in `ScriptRunner`). Marked edits in MiniGame.{h,cpp} (member, one
`update` line) and SceneScriptHost.cpp (two stubs). Keys, coin slot, hang-up bar are scene objects clicked through the normal event path
(mouse / touch / pad cursor); art, sounds and the comment `h422a` come from the game's own files.

## Reconstruction
- 0x4354d0 (phone part): index (+0x26) = -1 (no coin), 0 when script `sphone1` was already seen; display (+0x28) empty.
- Scene c4l2s2: `4221..4229` ANIM objects fire `snum1..9` (script `phone n`), visible only after `sphone1` (else comment `h422b`);
  the coin slot TRIG (item `iqu1`) runs `sphone1` (sound `coinslot`, then `phone 0` = 0x436610: index 0, display empty);
  the hang-up bar TRIG runs `sphone4` (`dialtone`, `phone 10`).
- 0x436640 (n): ignored while index == -1. n == 10: index 0, display cleared. Else digit[index] = n, display gets `-` before the 4th
  digit, then `%d`, index++. After the 7th digit: digits 5,5,5,4,4,4,4 ("555-4444") and `sphone3` not yet seen -> `sphone3`
  (ring, clear, dialogue AAA, unlock, `c4l2s1p3`), anything else -> `sphone2` (busy tone, comment h422a, clear).
- 0x4355a0: while in c4l2s2 (scene id 0x20204) with a phone and a non-empty display the display is drawn centred at y=200, z 10100.
  (The other branch of 0x4355a0, scene 0x50203, is the kick/score text and is not part of this.)
- `phone 11` (only if the object exists) calls 0x436640(11); no shipped script uses it.

## Port notes
- The exe stores the selected script name in game+0x1a6 and I found no reader of that field (all `0x1a6` references in .text are
  the ctor clear, the two writes in 0x436640 and a flag test in 0x41c0fa). Port: `Phone::update` starts it with `Game::switchTo`
  as soon as no script runs. Marked `// port:`.
- More than 7 digits would overrun the exe's digit array; the port ignores them (not reachable: the script runs first).
- The display is a `TextBlock` (re-made on change, removed outside c4l2s2) instead of immediate text drawing.

## Verification (headless :87, 1920x1080, SB_SKIP_MOVIES=1, SB_TEST_SCENE=c4l2s2, SB_TEST_SCRIPT=60:sphone1, SB_TEST_GAME_CLICKS)
Run helper used: `.shots/phone/*.log` and `*.png`. Logical click centres: keys 1-3 y=266, 4-6 y=331, 7-9 y=380; x = 273 / 323 / 376.
- `ok-*.png`, `ok.log`: 5,5,5,4,4,4,4 -> display `555-4444`, `sphone3`, display cleared, dialogue AAA starts.
- `fail-*.png`, `fail.log`: 1,2,3,4,5,6,7 -> `123-4567`, `sphone2`, comment h422a, display cleared.
- `asan.log`: the success path under ASan/UBSan (leak detection off): no diagnostics.
- All targets build; `tools/test/smoke.sh`: SUMMARY: 16 passed, 0 failed.
