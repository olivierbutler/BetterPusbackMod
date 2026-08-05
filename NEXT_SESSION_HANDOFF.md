# BetterPushback next-session handoff

Prepared: 2026-08-05  
Workspace: `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\BetterPusbackMod`  
Branch: `feature/realistic-tug-physics`

## Start here

Phase 7 Corrective Slice 5 is the **simulator-accepted baseline**. The
gate-anchored persistent-cache rewrite is built, installed, and validated. It
preserves the manual planner and does not restore any automatic wind/runway
proposal behavior.

The accepted implementation is committed on `feature/realistic-tug-physics`.
Preserve the user's accumulated fork. Do not reset, clean, revert, or replace
unrelated work.

## Fixed product decisions

- The pilot calls the tug. There is no automatic hookup or two-minute timer.
- After connection, **Plan push** opens the original manual-placement planner.
- The plugin must not infer a departure direction or generate route geometry
  from wind, runways, airport flow, or a generic automatic proposal.
- The pilot moves and rotates the cursor, clicks to place the desired aircraft
  pose, edits the route, and explicitly accepts it.
- A route explicitly drawn during the current planner session may be retained.
- Persistent routes may load and save only when the live nosewheel uniquely
  matches an active `apt.dat` row-1300 start within 1 m and 1 degree.
- Arbitrary/saved-situation starts remain fully usable for manual planning but
  are neither loaded from nor written to the persistent cache.
- The published ramp latitude, longitude, heading, airport, and ramp name are
  the cache identity. Route geometry is stored as aircraft-frame metre offsets
  from the live nosewheel anchor.
- The internal controller continues to use the main-gear path. The pilot-facing
  blue trajectory is the corresponding nosewheel path, so its first point is
  the published start without changing accepted steering physics.

## Rejected automatic proposal

The 2026-08-05 KCOS Gate 8 Slice 3 test showed an operationally unusable 93.7 m
automatic path. The user ended the proposal experiment. Exact rejected Windows
and Linux binaries, screenshot, `Log.txt`, telemetry, and evidence notes are at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice3-validation1-rejected-20260805`

The automatic proposal implementation was removed from `src/bp_cam.c`. Its
dedicated `src/airport_flow.c`, `src/airport_flow.h`, unit test, and runner were
deleted. Source and binary-string audits found no remaining operational proposal
references.

## Currently installed and accepted gate-cache rewrite

- Windows SHA-256:
  `13BA30F5BCFA759B0B1BBE8FFB4BA0295819BEC0F203E8C19801BC3B796D3725`
- Linux SHA-256:
  `D9D98FF7C593E818B4E8850D77A38EC08B8643F4BBAB1E520EDA3A98768DC089`

Installed at:

- `C:\X-Plane 12\Resources\plugins\BetterPushback\win_x64\BetterPushback.xpl`
- `C:\X-Plane 12\Resources\plugins\BetterPushback\lin_x64\BetterPushback.xpl`

## Accepted simulator validation

- Test 1 began with both BetterPushback route-cache files absent. At KCOS Gate
  8, the blue trajectory began at the aircraft nosewheel. The pilot drew and
  accepted the manual route, the new versioned cache was created, and the full
  pushback completed successfully.
- Test 2 reloaded the same 737-700NG at KCOS Gate 8. The planner logged `Gate
  route cache recalled for KCOS Gate 8` at the published anchor, and the cached
  blue trajectory began at the same nosewheel location and retained the same
  endpoint. The second pushback also completed successfully.
- The second planner session reported 217 recalled preview points and zero
  prediction failures. Both telemetry recordings contain the complete sequence
  through tug clear/departure with no non-finite numeric values. X-Plane and the
  plugin exited cleanly.
- Exact evidence is preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice5-validation1-accepted-20260805`.

## Next work

The published-start cache-route problem is resolved and accepted. Await the
user's next requested diagnostic or implementation scope. Do not alter the
manual planner or gate-anchor cache contract without new simulator evidence and
explicit direction.

## Verification completed

- Six current automated regression scripts passed, including the new strict
  gate-anchor and relative-coordinate math test.
- Windows and Linux warnings-as-errors release builds passed.
- Source audit found no automatic proposal implementation references.
- Windows and Linux binary-string audits found no proposal messages.
- Planner-source `git diff --check` passed.
- Two-stage simulator validation passed from a clean cache slate: initial save,
  exact Gate 8 recall, successful pushback, and successful ground-operations
  completion in both runs.

## Relevant files

- `src/bp_cam.c`: restored manual planner startup and interaction.
- `src/gate_route_cache.c` and `.h`: active published-start recognition and the
  new append-only, versioned persistent cache.
- `src/gate_route_math.c` and `.h`: strict start-pose guard and reversible
  anchor-relative transforms.
- `src/driving.c` and `src/driving.h`: dormant legacy route-table tooling; the
  active planner does not call its old persistent load/save interface.
- `src/planner_cache.c` and `src/planner_cache.h`: trajectory-render cache, not
  the persistent saved gate-route workflow.
- `PHASE_TESTING.md`: full history and accepted Corrective Slice 5 test.
- `GROUND_OPS_UI_DESIGN.md` and `ROADMAP.md`: automatic proposal removed from
  the active product design.

## Build/install notes

The dependency-complete build staging tree is:

`/opt/betterpushback-build/BetterPusbackMod`

When a new build is genuinely required, copy only changed source files into the
staging tree, run `./build_xpl.sh`, and copy the resulting Windows/Linux binaries
back into this workspace. Never overwrite installed binaries while X-Plane is
running. Archive the installed candidate and simulator evidence before replacing
a rejected build.
