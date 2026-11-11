# Character talk control (sb2.exe)

Verified with r2 on the retail executable, 2026-11-11.

- CHAR constructor `0x42e150` installs vtable `0x44e358` at `0x42e19f`.
  Slot `0x30` (byte offset `0xc0`, entry `0x44e418`) points to `0x404d50`.
- `0x404d50` takes an overlay name, an optional base-animation name, and a
  double loop count. Its callers pass FLT_MAX as a double for endless talk.
  Base `idle1` loops; base `idlet1` preserves an unexpired idlet1/2/3,
  otherwise starts idlet1 looping. Other bases play once via `0x404f20`.
- Secondary control storage is the three-word Granny handle at `+0x2a8`:
  version, sequence id, control id. **`+0x2b0` is its third word**, not a
  separate pointer or dialogue flag. `0x404e42` tests it, removes the old
  control with GrannyRemoveControlledAnimation, and zeros that word.
  The slot then finds the named animation, adds its control to the same
  sequence, sets parameter 3 (loop count), and stores the handle.
  A missing animation still removes the old overlay and returns false.
- Base animation control is separate, at `+0x27c` (id at `+0x284`).
  `0x404b80` restarts an active talk control after a base-animation change.
- `0x42f1a0` checks `+0x2b0` before an idle1 fidget. The idlet branches at
  `0x42f3c2` and `0x42f446` both call the slot with **talk / idlet3** when
  that handle is active and the scene dialogue is busy. Even the random
  idlet2 choice uses base idlet3; its sound remains idlet2. The slot itself
  does not start that sound. Otherwise the selected idlet2/3 plays normally.

The port keeps the secondary handle in OMedia3DGrannyModel and leaves the
base animation name/expiration independent. The boolean speaker API uses
idlet1 when available; full authored dialogue pose arguments remain outside
that existing API.

Chapter-bank regression: `tools/test/smoke.sh` starts c1l2s6 and uses the
existing `SB_TEST_SCRIPT=150:c2l2s7` hook to exercise switchTo, pendingId_,
DoSwitch and loadSceneBanks in one process. It requires both scene entries
and the c2 shared-bank opening after c1. Save-format smoke writes are isolated
under the output directory so a read-only installed game works too.
