# BetterPushback next-session handoff

Prepared: 2026-08-06
Workspace: `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\BetterPusbackMod`  
Branch: `feature/realistic-tug-physics`

## Start here

The completed Ground Operations UI and Emergency Tow workflow are the
**simulator-accepted baseline** at current `HEAD`. The fork now presents only
the Ground Operations panel by default while preserving the original
operational UI behind a reversible build option.

Emergency Tow is recorded by the commit titled `Add guarded Emergency Tow
workflow` on `feature/realistic-tug-physics`. It is layered on accepted UI
cleanup commit `7221afb2efc711bf5a35f252a10b60fb8354af15`.

The exact pre-feature source state remains recoverable from branch
`backup/pre-emergency-tow-20260806` and the verified full-history bundle under
`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\pre-emergency-tow-20260806`.

The accepted implementation is recorded in commit
`84e40ca9d4e39c2a8f47752eb66f0f428fcd79ed` (`Add two saved routes per gate
and aircraft`) on `feature/realistic-tug-physics`. Both author and committer are
`EZSIMULATIONS <ezsimulations@localhost>`.

The accepted wing-walker phase is recorded in commit
`50b994b76660f8462bedb0ecfdf56f579f93dfd4` (`Add final pushback wing
walker`) on the same branch, also authored and committed by
`EZSIMULATIONS <ezsimulations@localhost>`.
Preserve the user's accumulated fork. Do not reset, clean, revert, or replace
unrelated work.

## Wing-walker phase accepted and committed

The user approved a first prototype using Jungle Jim's downloadable
"Low-Poly Construction workers (animated)" model. The source is licensed under
CC BY 4.0 and is retained with attribution and a reproducible Blender 5.1
generator under `objects/src/Wing Walker`. The unrelated downloaded people
library was not used.

The candidate adds one held-pose wing walker approximately 30 yards ahead of
the aircraft nose and 1.5 m toward the captain's side. The worker faces the
cockpit and follows terrain slope. It is isolated from the accepted planner,
gate-route cache, and tug physics.

Candidate 1 was rejected because all three pose meshes rendered together and
the worker was therefore visible while the runtime signal was hidden. Candidate
2 added an always-hidden baseline to each pose, but the worker then never
rendered. Its simulator log proved that the controller emitted the exact desired
hidden, STOP, STANDBY, CLEAR, hidden sequence at steps 3, 14, 17, 22, and 23.

The isolated Candidate 2 rendering defect was a missing X-Plane custom-dataref
registration. `bp/anim/wing_walker_signal` was passed to
`XPLMCreateInstance`, but X-Plane requires custom datarefs referenced by an OBJ
to be registered before the OBJ is loaded. Candidate 3 registers the dataref in
`XPluginStart`, before any push can load the worker, and removes it in
`XPluginStop`. The OBJ pose gating and state mapping are otherwise unchanged.
Runtime signal transitions remain logged for simulator verification.

Candidate 3 passed complete daytime and nighttime pushbacks. The user confirmed
that STOP, STANDBY, CLEAR, disappearance timing, and the LIT texture all worked.
The worker was slightly too close to the aircraft nose for reliable visibility
from larger-aircraft cockpits. Candidate 4 changes only the nose clearance from
20 yards (18.288 m) to 30 yards (27.432 m); all validated behavior and assets
are unchanged.

Candidate 4 passed its final simulator acceptance pushback on 2026-08-05. The
user confirmed that the 30-yard placement is perfect. Four left-captain-seat
screenshots show STOP, STANDBY, CLEAR, and disappearance at tug departure. The
log records hidden, STOP, STANDBY, CLEAR, hidden at steps 3, 14, 17, 22, and 23
and then a clean plugin unload. The matching telemetry contains a complete
departure sequence and no NaN/Inf values.

Signal mapping:

- STOP: crossed hands and orange wands above the head from `stopped`, when the
  parking-brake instruction begins, through `ungrabbing`, including the safe
  portion of a reconnect sequence.
- STANDBY: both arms and wands down at approximately 45 degrees from
  `waiting4ok2disco`, after the equipment is physically released, through the
  tug's move to its clearance position.
- CLEAR: the right arm raises a green lit wand during `clear_signal`.
- The worker is hidden when the tug enters `driving_away`, and on all earlier
  push states where aircraft motion can resume.

Automated checks passed:

- `tests/run_wing_walker_logic_tests.sh`
- `tests/wing_walker_asset_test.py`
- Windows and Linux warnings-as-errors release builds
- `git diff --check`

Accepted Candidate 4 hashes installed on 2026-08-05:

- Windows: `7B06CEE3DFD902BF77ABEF73D950A6AB8C17761A92F80D517CE1D22A46388AFF`
- Linux: `EACBB630CA78D86C787CD199B0D2C957BA9AA054CFEA84D76EF93F5A31BB4C26`
- OBJ: `ADE970250611AB228AA62C6041D01899C587AEA0EFD9CC9C480DB1DC256F4B7E`

The previously accepted binaries were archived before installation at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\wing-walker-candidate1-preinstall-20260805`

Candidate 1's rejected binaries, exact OBJ/textures, `Log.txt`, three
screenshots, and result notes are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\wing-walker-candidate1-rejected-20260805`

Candidate 2's rejected binaries, exact OBJ/textures, `Log.txt`, and result notes
are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\wing-walker-candidate2-rejected-20260805`

The four Candidate 2 screenshots remain attached to the Codex task; they were
no longer present at their reported simulator paths when the archive was made.

Candidate 3's behaviorally accepted day/night binaries, runtime assets,
`Log.txt`, and result notes are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\wing-walker-candidate3-accepted-20260805`

Candidate 4's final accepted binaries, runtime assets, `Log.txt`, telemetry,
four acceptance screenshots, and result notes are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\wing-walker-candidate4-accepted-20260805`

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

## Preserved accepted baseline

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

## UI cleanup pass 1 accepted

The first UI cleanup pass was simulator-accepted on 2026-08-06. The expanded
panel no longer displays EOBT, METAR, or ATIS. The header reads X-Plane's
assigned Flight ID when available and falls back to the aircraft ICAO type when
the field is blank or contains the aircraft-type placeholder. Airport,
simulator wind/temperature, and QNH remain visible.

The 737-700NG acceptance screenshot at KCOS shows `Flight B737` and the cleaned
briefing strip. The exact screenshot and Windows/Linux candidate binaries are
preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\ui-cleanup-pass1-accepted-20260806`

Accepted candidate hashes:

- Windows: `17E7FB7E447019844F0758F51304C6002931C2A1B24E0ED3CAA0FE70431CC885`
- Linux: `016A4B7C5C96852F3D4884EB51423D1771E490151F5208172708B6A759F7768E`
- Screenshot: `552EA2EA24A114FB53890881F6AB3778B458B486A59410CF412CB082A700FBE8`

## Emergency Tow accepted

The post-completion UI now offers **Call tow back**. That action starts an
isolated Emergency Tow session using the existing connect-first tug workflow.
After capture, the existing manual planner opens automatically at the live
nosewheel with a fresh one-time route.

Emergency Tow never lists, loads, replaces, or saves persistent gate routes and
does not allocate or render the wing walker. After disconnect and final tug
drive-away, the module clears itself and Ground Operations returns to the normal
**Tug available / Call tug** state. The next normal operation restores the wing
walker and normal cache policy; the existing strict published-start guard keeps
an off-anchor returned aircraft session-only.

The user completed a normal push, Emergency Tow back to the selected gate, and
a second normal push on 2026-08-06. The live five-file gate-route cache remained
byte-for-byte unchanged. The log confirms both hard cache guards, no emergency
wing-walker allocation, the cold-start reset, the off-anchor no-save decision,
and restoration of the wing walker on the next normal operation.

Accepted candidate hashes:

- Windows: `6BD26A4225A81DB0DD1EA44879CFE0B8981BA25BBCA40BDE0538DCC6B9FAEEDE`
- Linux: `FCD131A4F5FC5CF565AC06F548AD728CDDA6693488A6C19DCB3AE0336BA7CD87`

Exact accepted evidence is preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\emergency-tow-accepted-20260806`

## Final UI cleanup accepted

The original four BetterPushback "magic squares" operational windows no longer
render by default, eliminating the duplicate UI. Their complete source remains
in `bp.c`. CMake option `BP_ENABLE_LEGACY_MAGIC_SQUARES` defaults to `OFF` and
can be configured `ON` by an upstream maintainer to restore the original
presentation without a source revert.

The beacon-triggered tug-start check was separated from legacy window creation,
so the switch affects presentation only. The Ground Operations panel, planner,
preferences, commands, disconnect/reconnect prompts, cache, tug physics,
Emergency Tow, and wing walker remain active.

The user visually confirmed in X-Plane that only the Ground Operations panel
rendered and that normal operations remained functional. All ten regression
suites, an explicit legacy-UI-enabled Linux build, and default-off Windows/Linux
warnings-as-errors builds passed. The simulator log contains no BetterPushback
errors or assertions, and the five-file gate-route cache remained byte-for-byte
unchanged.

Accepted candidate hashes:

- Windows: `6D4E3A2ED9F6FACCAF88DE0B684414B835FD68AEF913CA5BFDE52A1252DE78F5`
- Linux: `22623D004DE21AA44CF4A059C612E4939FE681A9562D19AA7265FF2BA0F3F897`

Exact accepted evidence is preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\ui-cleanup-pass2-accepted-20260806`

## Next work - beta package preparation

The implementation phase is complete. Await the user's beta-distribution
requirements, then assemble a tester package from the accepted binaries and
create a PDF explaining the changed Ground Operations workflow, manual/saved
route behavior, wing-walker signals, Emergency Tow, test scenarios, telemetry,
known limitations, and feedback/reporting instructions.

Treat current `HEAD`, including Emergency Tow and the final UI cleanup, as the
accepted baseline. Do not alter the manual planner, two-slot gate-route cache,
Emergency Tow policy guards, tug physics, wing-walker timing, poses, placement,
lighting, or licensed assets while preparing and running the beta.

After beta testing is complete, the user intends to finalize the repository and
push the branch for upstream review/approval. Do not push, open a pull request,
or contact upstream maintainers until the user explicitly requests that action.

## Verification completed

- All ten current automated regression suites passed, including strict
  gate-anchor math, two-slot save policy, Emergency Tow, and wing-walker tests.
- Windows and Linux warnings-as-errors release builds passed.
- Source audit found no automatic proposal implementation references.
- Windows and Linux binary-string audits found no proposal messages.
- Planner-source `git diff --check` passed.
- The full five-operation, two-aircraft Corrective Slice 6 simulator matrix
  passed, including two slots per aircraft, recalled-route editing, cache
  isolation, clean push/clear completion, and final per-aircraft chooser reloads.
- Wing-walker logic and asset tests passed, as did fresh Windows and Linux
  warnings-as-errors release builds.
- Candidate 3 passed daytime/nighttime lighting and signal tests. Candidate 4
  passed final 30-yard placement and full STOP/STANDBY/CLEAR/disappearance
  validation from the left captain seat, with a clean log and finite telemetry.
- UI cleanup pass 1 passed flight-identity fallback tests, three focused Ground
  Operations suites, Windows/Linux warnings-as-errors builds, and simulator
  visual acceptance at KCOS.
- Emergency Tow passed all ten regression suites, Windows/Linux
  warnings-as-errors builds, the complete normal/emergency/normal simulator
  sequence, and a byte-for-byte five-file cache audit.
- Final UI cleanup passed all ten regression suites, both default-off release
  builds, an explicit legacy-UI-enabled build, simulator visual/operation
  validation, and another byte-for-byte cache audit.

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
- `src/wing_walker.c` and `.h`: asynchronous OBJ lifecycle, terrain placement,
  30-yard nose clearance, captain-side offset, and per-instance signal updates.
- `src/wing_walker_logic.c` and `.h`: isolated pushback-step-to-signal mapping.
- `src/emergency_tow.c` and `.h`: isolated lifecycle and hard cache/walker
  policy gates for the one-time post-completion tow.
- `src/xplane.c`, `src/bp.c`, and `src/bp_cam.c`: minimal Emergency Tow command,
  planner handoff, lifecycle reset, and double cache-write guard integrations.
- `src/CMakeLists.txt` and `src/bp.c`: reversible default-off legacy operational
  UI gate with beacon automation preserved outside rendering.
- `objects/wing_walker`: committed OBJ8 runtime mesh, diffuse/LIT textures, and
  CC BY 4.0 attribution.
- `objects/src/Wing Walker`: original licensed source asset plus the
  reproducible Blender generator.
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
