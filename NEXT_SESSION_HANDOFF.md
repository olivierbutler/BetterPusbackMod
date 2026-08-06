# BetterPushback next-session handoff

Prepared: 2026-08-05  
Workspace: `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\BetterPusbackMod`  
Branch: `feature/realistic-tug-physics`

## Start here

Phase 7 Corrective Slice 6 is the **simulator-accepted baseline**. It adds two
pilot-selectable saved routes per published gate and compatible aircraft
profile, stored as separate files under
`Output/caches/BetterPushback_Gate_Routes`. The implementation preserves the
manual planner and does not restore any automatic wind/runway proposal
behavior.

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
- Each gate/aircraft profile has two persistent route slots. Existing slots are
  selected explicitly before the planner loads a route; a third plan requires
  explicit replacement or **Use once without saving**.

## Rejected automatic proposal

The 2026-08-05 KCOS Gate 8 Slice 3 test showed an operationally unusable 93.7 m
automatic path. The user ended the proposal experiment. Exact rejected Windows
and Linux binaries, screenshot, `Log.txt`, telemetry, and evidence notes are at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice3-validation1-rejected-20260805`

The automatic proposal implementation was removed from `src/bp_cam.c`. Its
dedicated `src/airport_flow.c`, `src/airport_flow.h`, unit test, and runner were
deleted. Source and binary-string audits found no remaining operational proposal
references.

## Currently installed accepted baseline

- Windows SHA-256:
  `67C6A63380A6AFE9140A84F596CF93C30A8F3442AC068224A65555CC92A708E3`
- Linux SHA-256:
  `A81481AC9042AD3B75463550783C7ED134842DD998E740B7F815AB1414CB4673`

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

Corrective Slice 6 passed the full two-aircraft slot matrix at KCOS Gate 8 on
2026-08-05:

1. The 737-700NG created Route 1 (`Tail S`, 359.20 degrees) from an empty
   compatible profile, then created Route 2 (`Tail N`, 180.70 degrees) without
   changing Route 1.
2. A third 737 session offered both saved routes. Route 1 was recalled, edited,
   and saved back to Slot 1 at 359.70 degrees. The final chooser showed the
   edited Route 1 at 360 degrees and unchanged Route 2 at 181 degrees.
3. The Felis B742 at the same gate found no compatible 737 slots, proving
   aircraft-profile isolation. It created its own Route 1 (`Tail S`, 0.70
   degrees) and Route 2 (`Tail E`, 268.20 degrees).
4. Final chooser screenshots showed exactly the correct two routes for each
   aircraft. The cache tree contains four version-2 route files in two aircraft
   profile directories, all anchored to `38.79933100, -104.70027200` at 269.20
   degrees, with no temporary files left behind.
5. All five planner summaries reported zero failures. All five telemetry files
   contain the complete sequence through `driving_away` with no NaN or infinity
   values, and all five Ground Ops sessions ended at controller `off`, prep
   `complete`, followed by clean plugin unload.
6. The accepted logs, telemetry, screenshots, complete cache directory, and
   exact installed binaries are preserved at
   `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice6-validation1-accepted-20260805`.

The old root-level `BetterPushback_gate_routes_v1.dat` remains ignored by the
accepted implementation.

## Next work

Await the user's next requested phase. Do not change the accepted manual planner
or gate-route behavior without a new explicit requirement.

## Verification completed

- Seven current automated regression scripts passed, including strict
  gate-anchor math and the new two-slot save-policy test.
- Windows and Linux warnings-as-errors release builds passed.
- Source audit found no automatic proposal implementation references.
- Windows and Linux binary-string audits found no proposal messages.
- Planner-source `git diff --check` passed.
- The full five-operation, two-aircraft Corrective Slice 6 simulator matrix
  passed, including two slots per aircraft, recalled-route editing, cache
  isolation, clean push/clear completion, and final per-aircraft chooser reloads.

## Relevant files

- `src/bp_cam.c`: restored manual planner startup and interaction.
- `src/gate_route_cache.c` and `.h`: published-start recognition plus isolated,
  atomic version-2 slot files in the dedicated gate-route folder.
- `src/gate_route_math.c` and `.h`: strict start-pose guard and reversible
  anchor-relative transforms plus tail-direction labels.
- `src/gate_route_slots.c` and `.h`: deterministic empty-slot and save-policy
  decisions shared by the planner and focused tests.
- `src/driving.c` and `src/driving.h`: dormant legacy route-table tooling; the
  active planner does not call its old persistent load/save interface.
- `src/planner_cache.c` and `src/planner_cache.h`: trajectory-render cache, not
  the persistent saved gate-route workflow.
- `PHASE_TESTING.md`: full history and accepted Corrective Slice 6 evidence.
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
