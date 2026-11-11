# Path rounding (sb2.exe)

Reconstructed from `r2` disassembly (`af; pdf`) at 0x42c450, 0x42cc50,
0x42cd70, 0x42cbb0, 0x42c920, and their callees 0x42d5f0, 0x42d870,
0x42cec0, 0x42dc90. Struct offsets are recorded in `WalkRoute.h`.

- The 0x18-byte search record is three doubles: x, z, heading in radians;
  heading -1 means unconstrained. Coordinates are fractional grid cells.
- 0x42c450 reverses the destination-first search output into a temporary
  doubly linked list, computes arrival headings, then tries skipping successive
  nodes. A collision-free candidate removes the current node; a failed candidate
  commits that node as the next anchor. The final array is reversed back for
  the executable's stack consumer. The port keeps vectors in walking order.
- 0x42cc50 tries CW then CCW arc/line candidates (0x42d5f0), choosing strictly
  shorter distance. 0x42cd70 tries CW/CW, CW/CCW, CCW/CW, CCW/CCW arc/line/arc
  candidates (0x42d870). Equal scores keep the earlier candidate. Candidate
  selection happens before collision checking; no alternate-turn retry follows.
- Four 0x48-byte motion pieces form each 0x120-byte candidate. Unused lengths
  are zero. The default radius is 0.1 cell (0x42a670). The solver returns the
  last positive-length piece index; the pruning pass copies that piece's +30
  angle verbatim, including the odd zero-tangent/last-arc case.

## x87 stack notes (top first)

- 0x42cbb9..0x42cc45: subtract endpoints -> `[dz,dx]`; squares/add/sqrt ->
  `[distance,dz,dx]`; pop distance, `fxch; fpatan` -> `atan2(dz,dx)`.
- 0x42d6e5..0x42d6f7: `[r,d]`, duplicate/multiply -> `[d*d,r,d]`,
  duplicate/multiply -> `[r*r,d*d,r,d]`, `fsubp; fsqrt` -> tangent length
  `sqrt(d*d-r*r)`. The call 0x443a80 is CRT `acos`, verified by its error
  branch's function name. 0x42d710..0x42d724 computes `atan2(dz,dx) +/- acos(r/d)`.
- 0x42cec3..0x42ceff: `[dz,dx]` -> direction `atan2(dz,dx)` plus centre
  separation. Same-direction circles use direction +/- pi/2. Opposite circles
  use direction +/- acos(2*r/d), rejecting `abs(2*r/d)>1`; line length is
  measured from `C1+2*r*(cos(a),sin(a))` to C2, preserving the operation order.
- 0x42dcdd..0x42dd20: arc radial angle is `initial +/- distance/r`; clockwise
  subtracts, counterclockwise adds. Tangent heading is radial -/+ pi/2.
- 0x42c920 samples distance 0 through strictly less than total, every 0.2 cell.
  Checks centre and four clearance-square corners (width 0.1). Integer conversion
  truncates toward zero. Out-of-grid footprint corners are ignored when +3b4 is
  enabled (constructor default); in-grid blocked samples still reject the path.

## Integration and limits

`World3D::gridWalk` passes grid and character occupancy to the pruning pass,
then stores analytic pieces scaled to world units. Movement evaluates them by
travelled distance, carrying excess distance across nodes; intermediate arrival
tolerances cannot skip curves. The existing final arrival tolerance is applied
to remaining travel distance. The port explicitly clamps evaluation at/beyond
the endpoint and rejects invalid radius/sample-step inputs or missing candidates
instead of copying the executable's invalid candidate index.

TODO(0x42d090): optional eight-direction tangent splitting/quantization (+39a,
initialized false at 0x42a6c0) is not ported. The existing A* search remains the
port's bounded search, rather than the executable's optional heading-state search.

`sb_unittest` checks the L route's start/corner/end, a circular turn contained in
the corner cell, tangent continuity, the closed-form symmetric two-arc solution,
mirrored/opposite turns, pruning, clearance, and outside-grid policy. Smoke runs
use private runtime directories so their saves cannot modify the shared `.game`.
