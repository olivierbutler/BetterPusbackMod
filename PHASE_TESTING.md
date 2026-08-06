# Realism Fork Verification Record

This is the evidence log for the realism fork. A phase is not marked complete
until its code has passed automated checks, its Windows plugin has been built
and installed, and its acceptance checks have been confirmed in X-Plane by a
live simulator test. Simulator screenshots, `Log.txt`, and telemetry files are
retained as evidence when they apply to the phase.

## Status definitions

- **Implemented:** source work is present but has not passed all local checks.
- **Built:** automated checks and the required plugin build passed.
- **Simulator validation:** the exact recorded binary is installed and awaiting
  an X-Plane test.
- **Confirmed:** all phase exit gates have evidence and the pilot accepted the
  result.

## Phase 1 — Planner prediction and terrain cache

Status: **Confirmed**

Date prepared: 2026-08-02  
Date confirmed in X-Plane: 2026-08-03  
Branch: `feature/realistic-tug-physics`  
Starting commit: `d446978ca266`  
Windows artifact SHA-256:
`C72E497CAE2F308C21151260AF20949478B569A172BFE70F403C48F422050A06`  
Linux artifact SHA-256:
`2E05C2E34982156AEF9776D4C88112863B9E5B306CEFA8217F0A14C6B8E71697`

### Change under test

- The controller-matched path and its terrain heights are built once per route
  signature and then reused by subsequent draw callbacks.
- A monotonic cache revision changes when committed or predicted geometry,
  controller parameters, aircraft dimensions, or the aircraft reference point
  changes.
- An unchanged cursor pose reuses its predicted segment solution instead of
  freeing and reallocating that list on every camera callback.
- Failed predictions are cached for the current route revision, preventing an
  invalid route from entering a frame-rate retry loop.
- The live push controller and route geometry algorithms were not changed in
  this phase.

### Automated evidence

| Check | Result |
| --- | --- |
| Planner cache lifecycle, invalidation, failure-cache, hash, tolerance, and heading-wrap tests | Passed |
| Existing vehicle-physics and telemetry regression suite | Passed |
| Windows x86-64 MinGW plugin build | Passed |
| Linux x86-64 plugin build | Passed |
| Built and installed Windows artifact hashes match | Passed |
| `git diff --check` | Passed; line-ending conversion warnings only |

The test builds use `-std=c99 -Wall -Wextra -Werror`. The installed test binary
is located at:

`C:\X-Plane 12\Resources\plugins\BetterPushback\win_x64\BetterPushback.xpl`

The complete distribution copy is located at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\dist\BetterPushback-realistic-physics`

The pre-Phase-1 working binaries were preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase1-planner-cache-preinstall-20260802`

### Simulator validation procedure

1. Start X-Plane and confirm BetterPushback appears normally in the plugin
   menu.
2. Open the planner with the same aircraft used for the accepted S-turn tests.
3. Draw a route containing a straight section and at least two turns. Confirm
   the blue line and continuous magenta danger-zone band look identical to the
   accepted planner visual.
4. Stop moving the mouse for at least ten seconds. Move it once to change the
   proposed endpoint, then hold it still for another ten seconds. This creates
   a clear cache-reuse sample without asking the simulator to guess at timing.
5. Commit a segment, rotate a segment, delete it, and redraw it. Confirm every
   edit updates the preview immediately and no stale band remains.
6. Accept the route and complete one push. Confirm the motion remains smooth
   and the final aircraft position remains within the already accepted
   operational margin.
7. Exit X-Plane normally and retain `Log.txt`, the push telemetry CSV, and one
   planner or final-position screenshot.

### Expected log evidence

The log must contain:

- `Planner trajectory cache initialized`
- `Controller-matched cached planner preview active`
- `Planner cache summary:`

For the stationary-pointer portions, `path cache hits` should outnumber `path
builds`, and `cursor reuse` should outnumber `cursor solves`. `failures` should
be zero for a valid route. Terrain probes should occur only during path builds;
cache-hit draws do not call the terrain probe API.

### Acceptance record

- [x] Plugin loads and planner opens normally.
- [x] Blue centerline and magenta danger-zone appearance are unchanged.
- [x] Add, rotate, delete, redraw, accept, and close are stable.
- [x] Log counters demonstrate stationary-route cache reuse.
- [x] No cached-prediction failures occur on the valid test route.
- [x] Push motion remains smooth.
- [x] Final position remains within the accepted operational margin.
- [x] Pilot reports no visible planner FPS regression.
- [x] `Log.txt`, telemetry CSV, and screenshot paths are recorded below.

### Confirmed simulator results

- Planner session: 1,194 route revisions and 1,194 required path builds were
  followed by 15,790 cache-hit draws, a 93.0 percent cached-draw rate.
- Cursor prediction: 2,059 changed solutions and 14,925 reused solutions, an
  87.9 percent reuse rate.
- Prediction failures: zero.
- Cached path build time: 0.66 ms average and 2.39 ms maximum.
- First preview: 339 points, 2.15 ms build time, and 0.11 m predicted endpoint
  offset.
- Telemetry: 5,329 samples over 562.018 seconds, including 199.710 seconds in
  the pushing state.
- Final planned-route sample: 0.192 m (7.6 in) main-gear endpoint offset,
  within the previously accepted operational margin.
- Largest consecutive commanded steering change: 0.702 degrees at the
  10 Hz telemetry rate. Largest consecutive aircraft-speed change: 0.066 m/s.
- `Log.txt` contains no planner-cache warning, prediction failure, plugin error,
  or crash record for the validation session.

Simulator result: **Passed**

Pilot confirmation: “All went very smoothly. Test complete.”

### Evidence files

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `B7093D2EF9F8A0B261CBFF06E298237FF90849C0AFE149900FD8C2D31E02BE96`
- `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260803_003524_AST-3F_tug.csv`  
  SHA-256: `271162A2FCDE8C0B6EA7CCBE1CB21053C15EEB6511F6380631811A7780CA9AFB`
- `C:\Users\DARRON\OneDrive\Pictures\Screenshots\Screenshot 2026-08-03 003906.png`  
  SHA-256: `90BE83B098588D9AF276037F11691FD7207652DE6FC39F9EE6DC8557AFBEAA35`

Phase 1 disposition: **Accepted and closed. Phase 2 is next in the roadmap.**

## Phase 2 — Inert Ground Operations UI shell

Status: **Confirmed**

Date prepared: 2026-08-03  
Date confirmed in X-Plane: 2026-08-03  
Branch: `feature/realistic-tug-physics`  
Starting commit: `d446978ca266`  
Windows artifact SHA-256:
`E558BF5FC3EAE1BE4101077F5DD2D546997D325CA2C750CE8FD8769BC64903EA`  
Linux artifact SHA-256:
`919E5823E2DD8D2FBDE3A09C1C3DF39221B0F7450420B804A890B89BA770A0F2`

### Change under test

- A dedicated `XPImgWindow` Ground Operations shell now has three presentation
  states: hidden, 58-by-244 five-stage progress rail, and 292-by-380 narrow
  panel.
- The compact rail distinguishes a click from a drag using a five-boxel displacement
  threshold. Clicking expands it; dragging only moves it.
- Edge-aware expansion opens rightward from the left edge and leftward from the
  right edge, clamping vertically so the complete title bar remains accessible.
- The panel can collapse, hide, pop out to an operating-system window, and
  return to the simulator.
- Floating and operating-system geometry are stored separately. Missing saved
  monitor geometry recovers to a visible monitor with a fixed margin.
- `BetterPushback/ground_ops_show_hide` and
  `BetterPushback/ground_ops_expand_collapse` commands and matching menu entries
  provide keyboard/hardware and classic-menu access.
- The preferences window includes an **Enable Ground Operations UI** option.
  Disabling it leaves every classic BetterPushback command available.
- The overhead planner suspends the new window and its manager callback, then
  restores the prior presentation when the planner closes.
- This phase renders static placeholders only. It neither reads nor writes tug,
  aircraft, push-controller, route, terrain, weather, or network state.

### Automated evidence

| Check | Result |
| --- | --- |
| Presentation enum, exact-size, anchor, monitor recovery, and click/drag threshold tests | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Shared `XPImgWindow` runtime call-site audit | Passed; one centralized initializer and cleanup owner |
| Inert draw-path audit | Passed; no dataref writes, physics calls, terrain probes, networking, disk I/O, or logging in drawing |
| Warm draw-path allocation audit | Passed; no plugin-owned allocation in compact-rail or panel drawing |
| Windows x86-64 MinGW plugin build | Passed |
| Linux x86-64 plugin build | Passed |
| Repository, distribution, and installed artifact hashes match | Passed |
| `git diff --check` | Passed; line-ending conversion warnings only |

The installed test binary is located at:

`C:\X-Plane 12\Resources\plugins\BetterPushback\win_x64\BetterPushback.xpl`

The complete distribution copy is located at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\dist\BetterPushback-realistic-physics`

The accepted Phase 1 binaries were preserved before installation at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase2-inert-ui-preinstall-20260803`

### Simulator validation procedure

No pushback is required for this phase because the shell is intentionally
inert.

1. Start X-Plane and confirm BetterPushback loads normally.
2. Select **Plugins > Better Pushback > Ground Operations: Show/Hide**. Confirm
   the compact five-stage rail appears without changing any aircraft system or
   tug state.
3. Drag the rail. Confirm it moves but does not expand. Then click it without
   dragging and confirm the narrow Ground Operations panel opens.
4. Use the up-chevron to collapse the panel to the compact rail. Expand again,
   use `X` to hide it,
   and show it once more from the menu.
5. In the panel, use `OUT`, move the operating-system window—to another monitor
   if available—then use `IN` to return it to X-Plane.
6. Open the BetterPushback preferences window while the Ground Operations UI is
   visible, close/save normally, and confirm both windows remain stable after
   reload.
7. Leave the compact rail or panel visible and open the overhead pushback planner.
   Confirm the Ground Operations UI disappears while planning and returns when
   the planner closes.
8. Hide and show the interface repeatedly. Exit X-Plane normally and retain
   `Log.txt` plus one compact-rail and one panel screenshot.

If practical, repeat steps 2-5 at a second X-Plane UI scale. Full multi-scale,
multi-monitor, and X-Plane 11 coverage remains part of the release compatibility
matrix even if it is not available on this test system.

### Expected log evidence

The log must contain:

- `Ground Ops inert UI initialized hidden`
- `Shared XPImgWindow UI runtime initialized` after the UI is first shown
- `Ground Ops inert UI shown in compact rail mode`
- `Ground Ops UI moved to popout mode` and `floating mode` when those controls
  are exercised
- `Ground Ops UI suspended while the overhead planner owns the screen`
- `Ground Ops UI restored after planner close`
- `Ground Ops UI summary`

The summary records compact-rail and panel draw counts, average and maximum callback
time, click expansions, suppressed drag expansions, and monitor-geometry
recoveries. The average acceptance ceilings are 0.10 ms for the compact rail and 0.20 ms
for the panel. Hidden mode must produce no recurring Ground Operations callback
or frame log.

### Acceptance record

- [x] Plugin loads and both Ground Operations menu entries are available.
- [x] Five-stage rail remains compact and within its 58-by-244 contract.
- [x] Dragging moves the rail without expanding it; clicking expands it.
- [x] A rail placed against either screen edge opens a fully visible panel.
- [x] Panel remains narrow and within its 292-boxel width contract.
- [x] Collapse, hide, show, pop-out, and return-to-sim are stable.
- [x] Preference and Ground Operations windows can coexist and reload safely.
- [x] Planner suspension and restoration work in both compact-rail and panel modes.
- [x] The inert shell causes no aircraft-system, tug, or push-state change.
- [x] Hidden mode has no observable recurring UI work.
- [x] Logged average compact-rail and panel callback times remain within their ceilings.
- [x] Classic menu and planner commands remain available.
- [x] `Log.txt` and screenshots are recorded below.

Phase 2 disposition: **Accepted and closed. Phase 3 is next in the roadmap.**

### Simulator validation attempt 1 — Rejected and corrected

Date: 2026-08-03  
Artifact SHA-256:
`08643D25C2180B5C388462BDAF96DF73285F3F0357E9C82E82CB3A387C22C198`

Passed evidence:

- Plugin and shared UI runtime loaded and shut down cleanly.
- Click-versus-drag discrimination worked.
- Pop-out, movement to another monitor, and return-to-simulator worked.
- Compact-view average callback cost was 0.009-0.013 ms; maximum was
  0.061 ms, below the 0.10 ms ceiling.
- Panel average callback cost was 0.015 ms; maximum was 0.123 ms, below the
  0.20 ms ceiling.

Rejected evidence:

- The single progress circle did not communicate the five workflow stages.
- A circle placed at the left edge expanded leftward, leaving most of the
  panel and its title-bar drag area off-screen.
- The test panel did not preserve the approved visual design. Default 17-pixel
  rendering made the copy hierarchy and spacing crowded and unprofessional.

Corrective action:

- Replaced the circle with the same vertical five-node rail used in the panel.
- Added edge-aware horizontal expansion and full vertical clamping, covered by
  automated left-edge, right-edge, and bottom-edge tests.
- Rebuilt the panel from the approved concept's charcoal surfaces, blue status
  accent, green stage state, 10-16 pixel typography, dividers, title grip, and
  compact information hierarchy.
- Increased the title-bar drag region while keeping the three window controls
  outside it.

Attempt 1 evidence:

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `056BF51D94CFB5DE02E690611AD8867D51FC3E7B17E95ABC7BE2832D3EACB9F9`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-03 02.02.55.png`  
  SHA-256: `202AA6FA95E1BE1679E33B769FE431186B769D78C49E18EE8647547F4BBDA8DA`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-03 02.03.55.png`  
  SHA-256: `45C5315DE39133E1B947E4A2681288E83778A794485549A7BB09A7B0DE7DD2BD`

The rejected Windows and Linux binaries are preserved for audit at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase2-validation1-rejected-20260803`

Pilot disposition: visual and edge-expansion validation **failed**. Corrective
artifact installed for simulator re-validation.

### Simulator validation attempt 2 — Accepted

Date: 2026-08-03  
Windows artifact SHA-256:
`E558BF5FC3EAE1BE4101077F5DD2D546997D325CA2C750CE8FD8769BC64903EA`

Confirmed evidence:

- The corrected five-stage compact rail and the 292-boxel panel remained fully
  visible at the left screen edge and preserved the approved visual hierarchy.
- Click-versus-drag, expand/collapse, hide/show, pop-out, movement to another
  monitor, return-to-simulator, and preference-window coexistence passed in the
  shell-interaction session immediately preceding the planner test.
- The expanded panel disappeared when the overhead planner opened. The log
  recorded suspension at 08:25:54 and restoration at 08:26:10, and the compact
  rail remained functional after restoration.
- The planner remained stable while the Ground Operations manager was
  suspended: 111 required path builds were followed by 1,477 cache-hit draws,
  with zero prediction failures.
- Compact-rail callback cost was 0.011 ms average and 0.097 ms maximum, below
  the 0.10 ms average ceiling. Panel callback cost was 0.015 ms average and
  0.137 ms maximum, below the 0.20 ms average ceiling.
- The log contains no recurring hidden-mode Ground Operations work, plugin
  error, or crash record, and the shared UI runtime shut down cleanly.
- The inert shell did not initiate a push or mutate tug or aircraft state.

Simulator result: **Passed**

Pilot confirmation: "Completed in sim test of going to planner and then
suspending it. UI still functions as it should."

### Accepted evidence files

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `763C90B7E4B1D1EA3BEB1C533092A3C0822421818DE2085E7203AF156D6E798A`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-03 08.24.41.png`  
  SHA-256: `7DCE5A4D6AFB928CBCD133EC203C10CB20F05C943030DC987D1707D13AAD4FE9`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-03 08.26.06.png`  
  SHA-256: `124F85D07E5A6A2F2948D45A9B9748817B0A59EF8DF4E55D14EE0BA729A9696E`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-03 08.26.13.png`  
  SHA-256: `CCAE120AB781165D7075DF6C041E209311D9ECDF7431B6641C7634D93A28F7C6`

The accepted Windows and Linux binaries plus the final evidence set are
preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase2-validation2-accepted-20260803`

## Phase 3 — State progression and corrective workflow history

Status: **Complete — accepted in simulator on 2026-08-04**

Date prepared: 2026-08-03  
Branch: `feature/realistic-tug-physics`  
Starting commit: `d446978ca266`  
Initial validation Windows artifact SHA-256:
`C24B80E9D3CF2D95706609F0E6637EB392FD00357D8FD9C7CFB189D88E7049AE`  
Initial validation Linux artifact SHA-256:
`935F649E1FFA8730761F801E8348785042BF34B7EFA054CD070F234018FDBEB7`

### Change under test

- `ground_ops_state.[ch]` provides a fixed-size snapshot and a pure mapping for
  all 24 existing `PB_STEP_*` controller states.
- Parsed-airport idle, planner-review, and completed pre-push views are
  represented independently from `bp.step`.
- The compact rail and panel now render completed, current, and future stages
  from the immutable snapshot instead of a static preview.
- An amber marker and labeled **PILOT ACTION** task appear only for controller
  states that actually require pilot input.
- Status, detail, task, source, speed, distance, hover, and caption strings are
  preformatted only when their quantized inputs change. Speed changes at 0.1
  m/s resolution and distance at one-meter resolution.
- Active ground-crew messages are mirrored as optional captions for the audio
  message duration. The preference takes effect immediately and does not alter
  audio playback.
- Semantic state changes produce one bounded `Ground Ops state transition` log.
  Metric-only refreshes do not log.
- The Phase 3 mapper and draw callbacks remain read-only. **Call tug** and the
  Phase 4 control slice queue action intent to existing command handlers after
  drawing.

### Automated evidence

| Check | Result |
| --- | --- |
| Exhaustive 24-state controller-to-stage mapping | Passed |
| Pre-push state separation from `bp.step` | Passed |
| Completed/current/future rail progression | Passed |
| Pilot-action marker positive and negative cases | Passed |
| Caption enable/disable and message-text mapping | Passed |
| Quantized metric refresh without semantic log transition | Passed |
| Waiting-state stability with identical input and no timer progress | Passed |
| Pure mapper dependency/allocation audit | Passed; no XPLM, dataref, route, audio, networking, file, or allocation calls |
| Ground Operations draw-path mutation audit | Passed; snapshot reads and drawing only, with action intent deferred to the manager loop |
| Phase 2 window-state regression suite | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Windows x86-64 MinGW plugin build with warnings as errors | Passed |
| Linux x86-64 plugin build with warnings as errors | Passed |
| Repository, distribution, and installed Windows artifact hashes match | Passed |
| `git diff --check` | Passed |

The installed test binary is located at:

`C:\X-Plane 12\Resources\plugins\BetterPushback\win_x64\BetterPushback.xpl`

The complete distribution copy is located at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\dist\BetterPushback-realistic-physics`

The accepted Phase 2 binaries and evidence remain preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase2-validation2-accepted-20260803`

### Initial simulator validation procedure (superseded)

1. Start X-Plane, show the Ground Operations panel, and confirm the idle view
   reads **Ready to plan pushback** with Tug current and an amber pilot-action
   marker. No tug or aircraft state should change.
2. Open the overhead planner. Confirm the Ground Operations UI disappears.
   Create and accept a route, then confirm the UI returns with **Pushback plan
   ready**. The planner remains the only route editor.
3. Start pushback through the classic command and keep the expanded panel
   visible. Confirm the rail advances through Tug, Connect, Comms, Push, and
   Clear in response to the real controller steps.
4. At each waiting point, confirm the status remains stationary—without a fake
   timer or animated progress—and the amber marker appears only when the pilot
   must secure equipment, set/release the brake, or approve disconnection.
5. Confirm active crew audio prompts appear as matching captions. In
   Preferences, turn **Show Ground Operations captions** off and on and confirm
   it takes effect immediately without changing audio.
6. During `PB_STEP_PUSHING`, confirm speed is displayed to 0.1 m/s and remaining
   route distance to whole meters when the automatic controller provides it.
   Manual or unavailable distance must display an em dash rather than a guess.
7. Exercise the classic stop path and confirm Push progresses through stopping
   and stopped without any UI-initiated command.
8. At the disconnect prompt, choose reconnect once. Confirm the rail returns
   from Clear to Connect/Comms as the existing controller reconnects, then
   complete a normal disconnect and clear signal.
9. Exit X-Plane normally and retain `Log.txt`, screenshots from at least one
   action-required state and one Push/Clear state, and the push telemetry CSV.

### Expected log evidence

The log must contain bounded entries beginning with:

- `Ground Ops state transition`

Across the test they must include controller states and stages for:

- `tug_load` or `driving_up_close` / Tug;
- `waiting_for_parking_brake` or `grabbing` / Connect;
- `connected` / Comms with `action required`;
- `pushing` and `stopping` / Push;
- `waiting_to_disconnect`, reconnect progression, and `clear_signal` / Clear;
- completed pre-push state after the operation.

Repeated 0.1 m/s speed or one-meter distance changes must not emit transition
logs. The existing Ground Operations performance summary must remain below the
Phase 2 ceilings, and no plugin error, assertion, or crash record is acceptable.

### Acceptance record

- [x] Every existing controller step has a documented and unit-tested mapping.
- [x] Pre-push presentation is separate from `bp.step`.
- [x] The snapshot and draw paths contain no pushback mutation.
- [x] Waiting states have no timer-derived progress.
- [x] Caption mapping and immediate enable/disable behavior are implemented.
- [x] Idle, planner-return, and valid-plan views are correct in X-Plane.
- [x] Tug, Connect, Comms, Push, and Clear progression is correct on screen.
- [x] Amber action markers appear only for real pilot-input gates.
- [x] Captions visibly match the active crew messages.
- [x] Speed and available distance displays update correctly.
- [x] Stop, disconnect, clear-signal, and completion paths are correct. The
      legacy reconnect state remains covered by the exhaustive mapper tests;
      it was not invoked in the final accepted run.
- [x] Transition logging is bounded and contains the expected sequence.
- [x] Phase 2 UI performance ceilings remain satisfied.
- [x] Final `Log.txt` and schema 7 telemetry evidence are recorded and
      preserved with the accepted binaries.

### Simulator validation attempt 1 — Motion passed, workflow rejected

Date: 2026-08-03  
Windows artifact SHA-256:
`C24B80E9D3CF2D95706609F0E6637EB392FD00357D8FD9C7CFB189D88E7049AE`

Confirmed evidence:

- Tug approach, cradle preparation, connection, push motion, route following,
  controlled route-end stop, lowering, disconnect, clear signal, and final
  completion all succeeded.
- The five-stage UI continued to render and update throughout both operations.
- The second telemetry file completed normally and the pilot reported that the
  push itself was successful.
- Shutdown performance summary: compact rail averaged `0.013 ms`; expanded
  panel averaged `0.016 ms`.

Rejected behavior:

- After the physical lift completed in connect-first mode, the UI continued to
  display **Lifting the nose gear / Wait for lift completion**.
- Opening the planner caused the UI to advance, but the required amber
  **Plan push** gate was missing before that click.
- The only operational stop path terminated the operation, discarded the
  route, and proceeded to disconnect. The pilot requires both a temporary
  traffic/ATC hold and a distinct terminating End/Disconnect action.

Root cause:

`pb_step_lift()` intentionally retains the legacy `PB_STEP_LIFTING` enum after
the lift animation is complete while connect-first waits for a route. Mapping
the enum alone therefore cannot distinguish active lifting from connected and
awaiting planning.

Evidence hashes:

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `BFBD1933003CDDCFEF347C7E81F2B04E47E232C64A1F297E4DA8DE0706C6D272`
- `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260803_090250_AST-3F_tug.csv`  
  SHA-256: `C2BBB85DC82E0731FA0104BCCD163478F6E1E0F66ED9F61DB138E92ACA220FFC`
- Screenshots `08.56.29`, `08.56.35`, `08.57.11`, `08.57.17`, `09.00.13`,
  `09.05.56`, `09.07.45`, `09.07.55`, and `09.09.53` are retained in
  `C:\X-Plane 12\Output\screenshots`.

The rejected Windows binary, installed Linux recovery copy, log, and telemetry
are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase3-validation1-rejected-20260803`

Pilot disposition: **Rejected for workflow correction; motion was successful.**

### Corrective artifact attempt 2 — Rejected in design review

Date prepared: 2026-08-03  
Windows artifact SHA-256:
`E2AB3D8C3F5A56B9BF75C09582EFDF00329A2F29BCB38EB28D36F14584C4D252`  
Linux artifact SHA-256:
`2638AEE25BE2E2819DB93D6200E03490D7FC264A94659AC2EE4F12C1DD9BB05F`

This artifact compiled and passed its automated checks, but it was rejected
before another simulator run because its workflow violated the approved
operating sequence:

- Under the requirement in force on 2026-08-03, it added a pilot **Call tug**
  gate where automatic dispatch had then been requested.
- It exposed the planner gate only after physical lift. The required hold is
  after nose-gear capture and before any lift.
- Its Pause/Resume and confirmed End/Disconnect controls were retained for the
  next correction because those controls satisfy the distinct hold-versus-end
  requirement.
- `BetterPushback/pause_resume` requests controlled deceleration while the
  route driver continues updating segment progress, steering feedback, and turn
  profile. At zero speed it reports **Pushback paused**; Resume ramps from zero
  on the same accepted route. The stationary hold freezes steering, and Resume
  is rejected until the parking brake is released.
- The compatible `BetterPushback/stop` path is presented as **End pushback and
  disconnect**. The panel requires a second confirmation and clearly states
  that the route will be discarded.
- Telemetry schema 7 records `pause_requested` and `pause_held` and forces a row
  when either state changes.

The rejected Windows and Linux binaries and supporting evidence are preserved
at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase3-validation2-review-rejected-20260803`

Pilot disposition: **Rejected from the written workflow; no simulator attempt
was authorized.**

### Corrective artifact attempt 3 — Installed for simulator validation

Date prepared: 2026-08-03  
Windows artifact SHA-256:
`A7CE585CAF89C193E4F7D630678E21D4113A0370FBDFE702B0C4481255EC3AB7`  
Linux artifact SHA-256:
`3E30EF15F992C60944F8C08401404E32D9F260EF44E5500E7B82156CC872CD78`

Corrected workflow:

- The initial panel displays the parsed nearest-airport identifier. ATIS and
  METAR are visibly marked pending until the later data-provider phase.
- A low-rate workflow callback runs a real 120-second ground-crew hold even
  while the UI is hidden. It then invokes the existing connect-first command
  automatically. There is no pilot tug-call action.
- Automatic connection covers tug selection, approach, cradle preparation,
  final positioning, and nose-gear capture without an amber pilot gate.
- The controller stops at the beginning of `PB_STEP_LIFTING`, holds lift
  position at zero, and explicitly reports `awaiting_plan`.
- Only then does the UI complete Connect and show the amber **Tug connected;
  plan the push** action. Accepting a route clears the hold and starts the
  physical lift from zero.
- Ending from the pre-lift hold skips the lowering animation so the aircraft is
  never lifted merely to disconnect.
- `BetterPushback/pause_resume` retains the accepted route for a temporary
  traffic/ATC hold. Confirmed **End operation** remains the terminating,
  route-discarding disconnect path.
- Telemetry schema 7 continues to record `pause_requested` and `pause_held`.

Automated evidence:

| Check | Result |
| --- | --- |
| Airport-data and real countdown presentation | Passed |
| No pilot action before automatic nose-gear capture | Passed |
| Pre-lift awaiting-plan mapping and amber Plan action | Passed |
| Pause, pausing, held, resume, and terminating-action presentation | Passed |
| Exhaustive controller mapping regression | Passed |
| Phase 2 window/monitor/click regression | Passed with 292 x 420 interactive panel contract |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry schema 7 regression suite | Passed |
| Windows x86-64 MinGW warnings-as-errors build | Passed |
| Linux x86-64 warnings-as-errors build | Passed |
| Repository, distribution, and installed hashes | Passed; all copies match per platform |
| `git diff --check` | Passed |

Validation attempt 3 procedure:

1. With no plan active, expand Ground Operations. Confirm the panel shows the
   current airport, **ATIS pending | METAR pending**, and a 2:00 automatic
   connection countdown. There must be no amber marker and no tug-call button.
2. Do not invoke any pushback command. Let the timer expire and confirm the tug
   is selected, dispatched, approaches, prepares its cradle, and captures the
   nose gear automatically.
3. Confirm the aircraft nose is not lifted. At capture completion, Connect must
   become complete and Comms must show amber **Tug connected; plan the push**
   with a **Plan push** button.
4. Open the planner from that button and accept a route. Confirm physical lift
   begins only after planner acceptance, then release the parking brake when
   requested and begin the automatic push.
5. On a straight segment click **Pause push**. Confirm smooth deceleration to
   **Pushback paused**, speed `0.0 m/s`, and no disconnect sequence. Click
   **Resume push** and confirm the original route continues.
6. If practical, repeat Pause/Resume during a turn. Confirm no steering snap,
   heading jump, or lost route endpoint.
7. On a separate operation, click **End operation**, verify the two choices
   **Keep pushing** and **Confirm end**, then confirm End. Verify controlled stop
   followed by the existing parking-brake and disconnect sequence.
8. Exit normally and retain `Log.txt`, the schema 7 telemetry CSV, one connected
   amber Plan screenshot, one paused screenshot, and one End confirmation or
   disconnect screenshot.

Corrective acceptance checklist:

- [ ] Airport identifier and the real 2:00 countdown display correctly.
- [ ] Tug dispatch and connection begin automatically with no pilot tug call.
- [ ] Automatic connection reaches the amber Plan push gate before any lift.
- [ ] Lift begins only after the planner route is accepted.
- [ ] Pause reaches a smooth `0.0 m/s` stationary hold without disconnecting.
- [ ] Resume continues the same accepted route without a steering jump.
- [ ] End/Disconnect is visibly distinct, confirmed, and follows the legacy
      terminating sequence.
- [ ] Telemetry contains immediate `pause_requested` and `pause_held` changes.
- [ ] No assertion, plugin error, crash, route loss, or endpoint regression.

Phase 3 corrective disposition on 2026-08-03: **Attempt 2 rejected in design
review; attempt 3 built and installed.**

### Corrective artifact attempt 4 — Pilot-called tug workflow

Date prepared: 2026-08-04  
Windows artifact SHA-256:
`29E6BB47658CE4B378443423C4258F2534F63DCCFAC3B2408C3F333BEDEC05B8`  
Linux artifact SHA-256:
`6FF91E2E7F66F5B68C47E1887BD62552B2D9631417291DAB8105F93DB361E681`

Requirement change:

- User demand reversed the automatic-dispatch decision on 2026-08-04.
- The two-minute timer, automatic-dispatch callback, retry behavior, and
  countdown presentation were removed completely.
- The initial panel still displays parsed airport context and pending
  ATIS/METAR fields, but now immediately shows amber **Call tug**.
- Pressing **Call tug** queues the existing `BetterPushback/connect_first`
  command outside the draw callback. No connection state changes before that
  press.
- After the call, tug approach, cradle preparation, final positioning, and
  nose-gear capture remain automatic.
- The corrected pre-lift **Plan push** hold remains: no lift occurs before route
  acceptance.
- Pause/Resume and confirmed End/Disconnect behavior is unchanged.

The superseded automatic build is preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase3-validation3-automatic-reverted-20260804`

Automated evidence:

| Check | Result |
| --- | --- |
| Idle airport view exposes amber Call tug | Passed |
| No countdown or automatic workflow state remains | Passed |
| Called connection has no second gate before capture | Passed |
| Pre-lift awaiting-plan mapping and amber Plan push | Passed |
| Pause/Resume and terminating-action presentation | Passed |
| Phase 2 window/monitor/click regression | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Windows and Linux warnings-as-errors builds | Passed |
| Repository, distribution, and installed hashes | Passed |

Validation attempt 4 procedure:

1. Load at an airport and expand Ground Operations. Confirm the airport
   identifier and **ATIS pending | METAR pending** are shown with amber
   **Call tug**. No countdown should appear.
2. Wait without pressing the button. Confirm no tug is selected, dispatched,
   or moved toward the aircraft.
3. Press **Call tug**. Confirm the tug begins its approach and completes cradle
   preparation, final positioning, and nose-gear capture automatically.
4. Confirm the aircraft has not been lifted. The UI must show amber **Tug
   connected; plan the push** with **Plan push**.
5. Open the planner and accept a route. Confirm lift begins only after route
   acceptance, then complete the push.
6. Validate Pause/Resume as a temporary route-preserving hold and End operation
   as the distinct terminating disconnect path.

Attempt 4 disposition: **Failed startup validation and was replaced.** X-Plane
12.4.3-r2 crashed before pilot interaction because the new UI called
`find_nearest_airport()` during plugin enable, before `bp_init()` had populated
the operation-only `drs.lat` and `drs.lon` wrappers. The exact assertion was
`dataref "" has bad type 0 (bp.c1180: &drs.lat)`. The crashing binaries and log
are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase3-validation4-crash-20260804`

### Corrective artifact attempt 5 — Startup airport-context crash fix

Date prepared: 2026-08-04  
Windows artifact SHA-256:
`8562F61A3B565C460785BD38F9FD3170ADB6427AABC72A56B7789A1CA0591F23`  
Linux artifact SHA-256:
`23F90D438D25F31AA31120AED7C1875DF643A55A814DB0AF07183374A3547A5A`

Correction:

- UI initialization and aircraft/airport reset no longer call the operation
  version of `find_nearest_airport()`.
- Airport context is resolved on a deferred X-Plane flight-loop tick using
  X-Plane's always-registered core latitude/longitude datarefs.
- `find_nearest_airport_at()` accepts validated coordinates and contains no
  BetterPushback operation-dataref access. Invalid or not-yet-ready values are
  retried at one-second intervals; polling stops as soon as context loads.
- The context callback is destroyed on plugin shutdown and safely rescheduled
  after aircraft or airport changes.
- This is display-only work. It cannot select, dispatch, move, or connect a
  tug. The tug remains idle until the pilot presses **Call tug**.

Automated evidence:

| Check | Result |
| --- | --- |
| No synchronous airport lookup during UI init/reset | Passed |
| Ground Operations state regression suite | Passed |
| Phase 2 window/monitor/click regression suite | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Windows x86-64 MinGW warnings-as-errors build | Passed |
| Linux x86-64 warnings-as-errors build | Passed |
| Repository, distribution, and installed hashes | Passed; all copies match per platform |
| Crashing build and exact `Log.txt` preserved | Passed |

Validation attempt 5 procedure:

1. Start X-Plane and confirm BetterPushback loads without an assertion or
   plugin-attributed crash.
2. Expand Ground Operations and confirm the nearest-airport identifier appears
   with the amber **Call tug** action.
3. Wait without pressing **Call tug** and confirm no tug is selected or moves.
4. Press **Call tug**, then confirm the existing pilot-called connection,
   pre-lift **Plan push** hold, Pause/Resume, and End/Disconnect behavior.

### Simulator validation attempt 5 — Accepted

Date: 2026-08-04  
Airport: KCOS  
Aircraft: B737 (`737_70NG.acf`)  
Tug: `AST-3F.tug`

Pilot disposition: **Everything worked as it should. Phase accepted.**

Evidence:

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `FB65593E244585F9B05BADB1B550E4EF799312CCA5A378FAEEFCAEE22EECE516`
- `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260804_093221_AST-3F_tug.csv`  
  SHA-256: `F2C99646E24FA07580D771E1C37CB9F04192548D1A0FA8C91F71AC9EC183D55B`
- Accepted Windows artifact SHA-256:  
  `8562F61A3B565C460785BD38F9FD3170ADB6427AABC72A56B7789A1CA0591F23`
- Accepted Linux artifact SHA-256:  
  `23F90D438D25F31AA31120AED7C1875DF643A55A814DB0AF07183374A3547A5A`

The accepted binaries and evidence are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase3-validation5-accepted-20260804`

Log evidence:

- The corrected build loaded without a BetterPushback error, assertion, or
  plugin-attributed crash. Airport context resolved to KCOS.
- The tug operation began only after the pilot action at 09:32:21. The log then
  records approach, cradle preparation, final connection, and nose-gear
  capture.
- The UI reached the pre-lift amber planning gate at 09:33:12. It suspended for
  the overhead planner at 09:33:29, restored at planner close, and began lift
  only after route acceptance at 09:33:49.
- Automatic push began at 09:34:14. Pause was requested at 09:34:21, stationary
  hold was reached at 09:34:27, and Resume continued the operation at 09:34:33.
- The route completed through stopping, lowering, disconnect, clear signal,
  tug departure, and controller `off`/operation complete at 09:38:16.
- The planner cache reported 400 builds, 1,756 cache hits, zero failures,
  `0.39 ms` average build time, and `1.33 ms` maximum build time.
- UI shutdown performance remained below the Phase 2 ceilings: compact rail
  `0.012 ms` average / `0.060 ms` maximum and panel `0.016 ms` average /
  `0.188 ms` maximum. There were no geometry recoveries or drag suppressions.
- BetterPushback unloaded normally, the shared UI runtime was cleaned up, and
  X-Plane recorded a clean shutdown.

Telemetry evidence:

- Schema 7 contains 3,394 data samples across 67 columns over `354.982 s`, an
  effective `9.56 Hz` sample rate. There are no malformed rows, missing core
  numeric values, or frame intervals above `0.5 s`; maximum recorded frame
  interval is `0.0503 s`.
- The pause request lasted `11.674 s`; stationary `pause_held` was recorded for
  `5.716 s`. Held speed averaged `0.0053 m/s` and never exceeded the controller
  stationary threshold (`0.0782 m/s` maximum).
- In the two-second Resume window, applied steering and requested nosewheel
  steering had zero sample-to-sample jump. Maximum observed aircraft
  acceleration magnitude was `0.1863 m/s²`.
- At the last pushing sample, tail distance remaining was `0.0088 m`, tail
  cross-track error was `0.3113 m`, and final heading error was `0.5926°`.
  The disconnect sequence ended with the aircraft effectively stationary
  (`0.00002 m/s`) and the parking brake set.
- Telemetry closes as the tug finishes `driving_away`; the subsequent bounded
  state-transition log confirms controller `off` and operation completion.

Attempt 5 disposition: **Accepted. Phase 3 is complete. The run also validates
the Phase 4 Pause/Resume path. The explicit early End/Disconnect control and
remaining Phase 4 scenario matrix stay in simulator validation.**

## Phase 4 — Pause, Resume, and End/Disconnect validation

### Simulator validation attempt 1 — One stationary-handoff correction required

Date: 2026-08-04  
Airport: KCOS  
Aircraft: B737 (`737_70NG.acf`)  
Tug: `AST-3F.tug`

Accepted behavior:

- While moving, **End operation** displayed the distinct **Keep pushing** and
  **Confirm end** choices. **Keep pushing** resumed the same operation.
- A later moving **Confirm end** produced a controlled stop and completed the
  parking-brake, lowering, disconnect, clear, and tug-departure sequence.
- Repeated Pause/Resume worked on straight and turning portions of the route.
  Resuming from the turn hold was smooth and retained the accepted route.
- Resume was correctly rejected while the parking brake was set. After the
  brake was released, Resume continued normally.

Rejected behavior:

- On the final test, the aircraft was already in a confirmed stationary pause
  hold when **End operation** and **Confirm end** were selected. Instead of
  remaining stationary for the parking-brake request, the stopping controller
  briefly moved the aircraft before stopping again.
- Telemetry records the transition into `stopping` at `336.285 s` from a held
  aircraft speed of `0.0012 m/s`. The stopping controller then requested its
  normal steering-straightening creep speed. Aircraft speed peaked at
  `0.5624 m/s` and position changed by `2.021 m` during the first ten seconds.
- The operation still completed its disconnect sequence successfully, and the
  log contains no crash, assertion, or plugin error.

Root cause and correction:

- The End command discarded the route and entered the legacy stopping state,
  but it did not distinguish a moving push from an already-completed pause
  hold. With steering still non-neutral, the stopping state commanded a low
  translation speed to straighten the nose gear.
- End confirmation now records a dedicated stationary-handoff condition before
  clearing the pause state. In that condition the stopping controller holds
  commanded speed at exactly zero and neutralizes steering without translating
  the aircraft.
- The normal stopping state is intentionally retained so the UI and ground crew
  still request the parking brake before lowering and disconnecting. The change
  does not alter End behavior while the aircraft is moving.
- Pause flags are cleared when End is confirmed, so terminating telemetry no
  longer reports the operation as simultaneously paused.

Evidence:

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `BE59770D4D8CED81BD4B89F8BD78F443BADF44C6844FB67412A921134089F336`
- Moving End/Keep/End-confirm run:  
  `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260804_100720_AST-3F_tug.csv`  
  SHA-256: `FB4E6C875C2BB36E40BD90E0F4325EF417BA5EA70B2EA20FA8742CAEE3A15AEE`
- Pause/Resume and stationary-End run:  
  `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260804_101453_AST-3F_tug.csv`  
  SHA-256: `72DCA82FFF0AE4ED6424699B0578C010ABF84CFE98F94E90A730D4AAFF73F361`

The tested attempt-1 binaries and all three evidence files are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase4-validation1-hold-end-rejected-20260804`

Corrective artifact installed for focused simulator validation:

- Windows SHA-256:  
  `44942CFA17E5819D6FA51FF14CD5D1BEC8153BB23FB52417B08F193C03A205A3`
- Linux SHA-256:  
  `4C2CEC29D4B256073CEC3DBDA064AE92036CD517A170835ACD96AD104B1B1B13`

Automated evidence:

| Check | Result |
| --- | --- |
| Stationary-handoff classification regression | Passed |
| Ground Operations state regression suite | Passed |
| Phase 2 window/monitor/click regression suite | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Windows x86-64 MinGW warnings-as-errors build | Passed |
| Linux x86-64 warnings-as-errors build | Passed |
| Repository, distribution, and installed hashes | Passed; all copies match per platform |
| `git diff --check` | Passed |

Focused validation attempt 2 procedure:

1. Call the tug, plan a route, start the push, and release the parking brake.
2. While the aircraft is moving, press **Pause push** and wait until the panel
   says **Pushback paused** and shows speed `0.0 m/s`.
3. Leave the parking brake released. Press **End operation**, then **Confirm
   end**.
4. The aircraft and tug must remain completely stationary. The UI must request
   that the parking brake be set; there must be no forward creep or jerk.
5. Set the parking brake. Confirm lowering, disconnect, clear signal, and tug
   departure complete normally.
6. Exit X-Plane normally and retain `Log.txt` and the newest telemetry CSV.

Attempt 1 disposition: **All Phase 4 control behavior accepted except End from
an already-held pause. The correction is built and installed; Phase 4 remains
in simulator validation pending only the focused six-step check above.**

### Simulator validation attempt 2 — Accepted

Date: 2026-08-04  
Airport: KCOS  
Aircraft: B737 (`737_70NG.acf`)  
Tug: `AST-3F.tug`

Pilot disposition: **Worked exactly as intended. The push was deliberately
paused in the middle of a turn. Confirmed End introduced no tug or aircraft
movement, requested the parking brake, and disconnected successfully.**

Evidence:

- `C:\X-Plane 12\Log.txt`  
  SHA-256: `4A49A4B52A0EA493FF4895C11B63DB5A86A74C1377B7908CF610468FA084D58B`
- `C:\X-Plane 12\Output\BetterPushback\telemetry\push_20260804_104837_AST-3F_tug.csv`  
  SHA-256: `8B34A2BE20BD109017FDA8704D4769680194BDE7B03EEA269B4D63F14AABA266`
- `C:\X-Plane 12\Output\screenshots\737_70NG - 2026-08-04 10.53.29.png`  
  SHA-256: `95CDEFB809E908D30EB76C3BCA07A89A5A9A728664C7D4F691F56CB9705EAF9F`
- Accepted Windows artifact SHA-256:  
  `44942CFA17E5819D6FA51FF14CD5D1BEC8153BB23FB52417B08F193C03A205A3`
- Accepted Linux artifact SHA-256:  
  `4C2CEC29D4B256073CEC3DBDA064AE92036CD517A170835ACD96AD104B1B1B13`

Log evidence:

- Automatic push began normally. Pause was requested at `10:51:15`, and the
  stationary hold was reached at `10:51:20` while the tug was turning.
- At `10:51:36`, the corrected controller explicitly recorded **End operation
  confirmed from stationary pause hold; preserving zero speed through
  parking-brake request**.
- The UI immediately presented the stopped/action-required state. After the
  parking brake was set, the controller progressed through lowering,
  ungrabbing, disconnect, clear signal, tug departure, and `off` at `10:53:23`.
- BetterPushback recorded no error, assertion, or crash. The Ground Operations
  UI shut down cleanly at `10:53:34`; panel rendering averaged `0.017 ms` with a
  `0.143 ms` maximum and no geometry recoveries or drag suppressions.

Telemetry evidence:

- Schema 7 contains 2,756 data samples across 67 columns over `286.674 s`, with
  no invalid core telemetry rows.
- The final stationary hold was established with substantial turn articulation:
  nosewheel angle `-37.331°` and applied steering `-36.047°`. This is the exact
  case that previously triggered unwanted steering-straightening translation.
- At the End handoff, aircraft speed was `0.0013 m/s`, tug speed was
  `0.0025 m/s`, and nosewheel angle remained `-37.357°`. Requested steering was
  neutralized to `0°`, both pause flags cleared, and commanded target speed was
  exactly `0.000 m/s`.
- Target speed remained exactly zero for the entire `4.091 s` parking-brake
  request. Only sub-threshold simulator settling was measured: maximum aircraft
  speed `0.0587 m/s`, maximum tug speed `0.0427 m/s`, aircraft displacement
  `0.0275 m`, and tug displacement `0.0320 m`. There was no commanded creep,
  straightening run, or visible jerk.
- Parking-brake engagement was followed by the complete lowering and disconnect
  sequence. The final screenshot shows every workflow stage complete, **Ground
  operation complete**, **Aircraft and equipment are clear**, and **No pilot
  action required**.

The accepted binaries, log, telemetry, and screenshot are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase4-validation2-accepted-20260804`

Attempt 2 disposition: **Accepted and closed. Phase 4 is complete. Phase 5 is
next in the roadmap.**

## Phase 5 — Optional flight and weather context

### Slice 1 — Provider foundation and simulator-local fallback

Date prepared: 2026-08-04  
Status: **Accepted and closed**

Windows artifact SHA-256:  
`A3F20C7D8207D1057E1587AA20EE73E6D7F705CF03FFD28DB24DBD73E4EC00EE`  
Linux artifact SHA-256:  
`CC9FA601A761F20A1DC593BDDDED3A1700D284FDCA69556784C86F3AC3BC7AAE`

Change under test:

- `ground_ops_data.[ch]` defines fixed-size, source-aware provider contracts for
  flight identity, schedule, simulator weather, cached METAR, and online ATIS.
  Every value carries source, fetch time, expiry time, and derived freshness.
- The first active providers read X-Plane's standard Flight ID and current
  aircraft-location wind, temperature, and QNH datarefs. All XPLM calls occur
  on the main thread through the existing Ground Operations manager loop.
- Provider input is bounded and validated before publication. Flight tokens are
  normalized, invalid controls and oversized text are rejected, weather ranges
  are checked, and draw code receives only fixed-size presentation strings.
- The panel title displays `Flight <ID>` when the standard dataref is populated
  and `Flight --` when it is not. The briefing strip displays simulator weather
  as `SIM ddd/nnKT ttC`, QNH as `nnnn hPa / nn.nn inHg`, an explicit
  `METAR -- | ATIS --`, EOBT unavailable, and `Simulator | Current`
  provenance.
- Local weather is refreshed at one hertz only while the Ground Operations UI
  is visible. Hiding the UI cancels the manager callback, preserving the Phase
  2 zero-recurring-work contract. No network thread or request exists in this
  slice.
- Flight/weather values are display-only state changes. They cannot start,
  stop, pause, plan, connect, or otherwise mutate a pushback operation.

Automated evidence:

| Check | Result |
| --- | --- |
| Unavailable provider presentation | Passed |
| Bounded Flight ID normalization and malformed-token rejection | Passed |
| Simulator weather validation, formatting, and staleness | Passed |
| Oversized/malformed METAR rejection | Passed |
| Mixed-source and stale-source presentation | Passed |
| Provider data changes do not create workflow transitions | Passed |
| Ground Operations controller-state regression suite | Passed |
| Phase 2 window/monitor/click regression suite | Passed |
| Planner-cache regression suite | Passed |
| Vehicle-physics and telemetry regression suite | Passed |
| Windows x86-64 build | Passed cleanly |
| Linux x86-64 build | Passed cleanly |
| Repository, distribution, and installed hashes | Passed; all copies match per platform |
| `git diff --check` | Passed |

Simulator validation procedure:

1. Start X-Plane, expand Ground Operations, and take one screenshot before
   pressing **Call tug**.
2. Confirm the title shows either the aircraft's standard Flight ID as
   **Flight XXXXXXX** or the truthful fallback **Flight --**. Report which one
   appears; do not enter a made-up value solely for this test.
3. Confirm the briefing strip shows **Airport KCOS** (or the actual airport), a
   populated `SIM ddd/nnKT ttC Qnnnn` weather summary, **EOBT --**, and
   **METAR -- | ATIS --**. The footer must show **Simulator | Current** and
   **Interactive | Offline safe**. No field should say `pending`.
4. Leave the panel open for at least ten seconds. Confirm the display remains
   stable without flicker; ordinary simulator weather variation may update the
   values.
5. Press **Call tug**, complete the connection and planning gates, start the
   push, perform one Pause/Resume, then use **End operation** and **Confirm end**.
   Confirm the previously accepted workflow is unchanged and disconnects
   normally.
6. Exit X-Plane normally and retain `Log.txt`, the newest telemetry CSV, and the
   pre-call screenshot.

### Validation 1 — Complete workflow and local-data presentation

Date tested: 2026-08-04  
Operational disposition: **Accepted**  
Presentation disposition: **Corrections installed for focused validation**

Accepted artifact SHA-256 values:

- Windows: `A3F20C7D8207D1057E1587AA20EE73E6D7F705CF03FFD28DB24DBD73E4EC00EE`
- Linux: `CC9FA601A761F20A1DC593BDDDED3A1700D284FDCA69556784C86F3AC3BC7AAE`

Evidence:

- `Log.txt` records the complete workflow from tug load at 12:08:24 through
  connection, planner suspension/restoration, push, pause/resume, controlled
  stop, brake interaction, disconnect, clear signal, and controller off at
  12:14:38. X-Plane then unloaded the plugin normally at 12:17:55.
- Telemetry schema 7 contains 3,599 data rows (`A1:BO3600`) over 374.416
  seconds. It records 84 pause-requested samples, 26 pause-held samples, a
  maximum absolute aircraft speed of 0.0783 m/s while held, the complete
  controller sequence through `driving_away`, and a final aircraft speed of
  approximately zero.
- The panel remained stable and the airport, simulator wind, temperature, QNH,
  source, and freshness were populated. EOBT, METAR, and ATIS truthfully
  remained unavailable in this local-only slice.

Presentation findings:

- X-Plane's standard Flight ID contained `B737`, matching the aircraft ICAO,
  so it was an aircraft-type fallback rather than a real flight number.
- Font Awesome glyphs were not present in the active draw font and appeared as
  question marks in the header and current-stage tile.
- QNH needed both hPa and inHg, the simulator-weather row needed more readable
  spacing, and immediate hover helpers could obscure the panel.

Corrective artifact installed for focused validation:

- Windows SHA-256:
  `E284BA678428F7381B69F5946B52E0005C4AF63F6D5F334A0DF167529BBB3345`
- Linux SHA-256:
  `D2A7BCA4A7ADAE90A4968BD9FA319BB3D2F421AAF55F8E3F7AEA41DC8A11AD3E`

Corrections:

- A simulator Flight ID equal to the aircraft ICAO is rejected as a flight
  number and displays the truthful `Flight --` fallback.
- The briefing strip is static rather than rotating: wind and temperature are
  on one row and `QNH nnnn hPa / nn.nn inHg` is on a dedicated row.
- Unsupported icon-font glyphs were replaced with ASCII-safe control and stage
  labels.
- Helper text requires a stationary delayed hover with an item-specific timer
  and disappears immediately when the pointer leaves.

Focused validation procedure:

1. Start X-Plane and expand Ground Operations before calling the tug.
2. Confirm the Zibo displays `Flight --` unless an actual flight number has
   been entered, and confirm there are no question-mark glyphs in the header or
   current-stage tile.
3. Confirm wind and temperature are legible, and QNH displays both hPa and
   inHg on the line beneath them. EOBT, METAR, and ATIS should remain `--`.
4. Hover over a header or action control. Confirm no helper appears immediately,
   that it appears only after a stationary delay, and that it disappears as
   soon as the pointer leaves.
5. Press **Call tug** once and confirm the panel advances normally. A second
   full push is not required for this display-only correction.

### Validation 2 — Focused presentation correction

Date tested: 2026-08-04  
Disposition: **Accepted**

Accepted artifact SHA-256 values:

- Windows: `E284BA678428F7381B69F5946B52E0005C4AF63F6D5F334A0DF167529BBB3345`
- Linux: `D2A7BCA4A7ADAE90A4968BD9FA319BB3D2F421AAF55F8E3F7AEA41DC8A11AD3E`

Confirmed results:

- The aircraft-type placeholder was filtered and the panel displayed the
  truthful `Flight --` fallback.
- The fixed simulator-weather layout was easier to read and updated normally.
- QNH displayed both hPa and inHg.
- Header controls and the `TG` stage tile rendered without missing-glyph
  question marks.
- Hover help appeared only after intentional hover and disappeared correctly
  when the pointer moved away.
- **Call tug** advanced the workflow from the amber pilot gate to
  `Tug approaching aircraft` and `Monitor tug approach`.
- `Log.txt` records `Flight --`, airport KCOS, the panel opening, and normal
  transitions into `tug_load`, `start`, `driving_up_close`,
  `opening_cradle`, and the Connect stage. The UI shutdown summary reported
  14,162 panel draws at a 0.016 ms average and 0.111 ms maximum, with zero
  click expansions, drag suppressions, or geometry recoveries. The plugin then
  unloaded normally.

Validation 2 evidence and accepted binaries are preserved in:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase5-slice1-validation2-accepted-20260804`

Validation 1 evidence and the accepted binaries are preserved in:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase5-slice1-validation1-ui-findings-20260804`

Slice 1 disposition: **Accepted and closed. The provider path, simulator-local
fallbacks, presentation, and unchanged pushback workflow all passed simulator
validation. The product decision now excludes SimBrief and does not require a
network METAR or online ATIS provider; those fields remain truthful `--`
placeholders unless a later display-only design is approved.**

## Phase 7 — Deterministic proposed push

### Slice 1 — X-Plane wind soft seed

Date prepared: 2026-08-04  
Status: **Rejected in simulator validation; retained below as historical test evidence**

Change under test:

- A previously accepted route for the same gate remains first priority.
- With no cached route, the planner reads current X-Plane wind once and groups
  the loaded airport's paved runways by coarse direction. It never assigns a
  specific runway.
- A clear result opens the planner with one amber, uncommitted route and a
  message such as **Suggested SOUTH flow from X-Plane wind**.
- Moving the mouse or rotating the endpoint replaces the initial pose. Enter
  accepts the current amber pose; Delete removes the suggestion.
- Calm wind, an equal choice between intersecting runway directions,
  unsupported airport geometry, or an unreachable endpoint opens the unchanged
  manual planner.
- No airport file scan, external ATIS, SimBrief, network call, or continuous
  weather solver was added.

Automated results:

| Test | Result |
| --- | --- |
| KCOS-like southerly wind selects coarse SOUTH flow | Passed |
| KCOS-like northerly wind selects coarse NORTH flow | Passed |
| Parallel runways collapse into one direction | Passed |
| Calm wind produces no suggestion | Passed |
| Equal intersecting-runway choice produces no suggestion | Passed |
| Short secondary strip does not override the main runway family | Passed |
| All existing provider, workflow, window, planner-cache, and physics tests | Passed |
| Windows and Linux release builds with warnings as errors | Passed |

Installed candidate hashes:

- Windows: `B0E9C90DF9B9213A726A62F5F2C49D16221165E54CAA51F13FD411BA05F9E877`
- Linux: `2EBC075200162B14AF1EE36CDBEEA9882DBEFEF55A8F04689005B0371169AD16`

The immediately previous accepted binaries are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-soft-seed-preinstall-20260804`

### Simulator validation — exact procedure

Use two KCOS gates that have not previously had a BetterPushback route saved.
Changing to another gate is important because a cached route correctly wins and
will hide the new suggestion.

1. At the first unused gate, set X-Plane manual weather to wind **180 degrees at
   10 knots**. Open Ground Operations, call the tug, let it connect, and press
   **Plan push**.
2. When the overhead planner opens, do not move the mouse initially. Confirm an
   amber proposed route is already visible and the bottom message says
   **Suggested SOUTH flow from X-Plane wind**. It must not name `17L`, `17R`,
   or any other runway.
3. Move the mouse to reposition the amber aircraft, use the mouse wheel to
   rotate it, and confirm the route updates normally. Press **Enter without an
   extra placement click**. Confirm the planner closes and the accepted route
   advances the normal lift/push workflow.
4. Complete enough of the push for the accepted route to be saved. Reset the
   aircraft to the identical gate and heading, repeat the connection, and open
   the planner again. Confirm the saved route loads and there is no X-Plane-wind
   suggestion message. This proves cached-route priority.
5. At the second unused gate, set wind speed to **0 knots**. Repeat through
   **Plan push**. Confirm no pre-positioned amber suggestion or suggestion
   message appears; draw and accept a route manually as before.
6. Preserve `Log.txt` and screenshots for steps 2, 4, and 5. The log should
   contain one of these explicit decisions for each opening: `Planner offered
   uncommitted`, `Planner reused saved gate route`, or `Planner flow suggestion
   unavailable`.

Acceptance requires all three branches: offered/editable, cached-route wins,
and calm-wind manual fallback. A normal push must remain smooth after accepting
the edited suggestion.

### Validation 1 - cache priority and hover defect

Date tested: 2026-08-04  
Disposition: **Rejected**

The preceding cached-route-first procedure is obsolete and must not be
repeated. Its cache-priority acceptance criterion was rejected during this
validation.

Findings:

- A delayed helper remained visible after the mouse left the Ground Operations
  window and covered the action controls.
- The cached Gate 8 route won automatically, suppressing the current weather
  proposal. That priority is invalid because a gate may require opposite
  departure flow on different days.
- The legacy cached route was not aligned to the current aircraft pose. Its
  stored start was approximately 11 metres from the current Gate 8 WED pose.

Rejected artifacts and evidence are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice1-validation1-cache-priority-rejected-20260804`

### Corrective Slice 2 - weather proposal first, cache by pilot choice

Date prepared: 2026-08-04  
Status: **Rejected in simulator validation on 2026-08-05**

Corrected decision hierarchy:

1. An existing route created explicitly in the current session is preserved.
2. Otherwise, current X-Plane wind and the loaded airport's `apt.dat` runway
   geometry produce the first editable coarse-direction proposal.
3. A persistent saved route is never selected automatically. When one is
   available, the planner displays **Saved route / Optional review** as the
   final control. Only the pilot can load it.
4. If wind is calm or the runway direction is ambiguous, the planner opens in
   manual mode; the optional saved-route control remains available.

Implementation details:

- The proposed path starts with a guaranteed straight-back clearance segment.
  Clearance is the greater of half the aircraft length or its wheelbase,
  clamped to 12-30 metres. The turn toward the coarse departure direction is
  solved only after that segment.
- The proposal remains stationary while the pointer crosses the screen to the
  planner controls. **Enter** or **Accept plan** accepts it unchanged. Clicking
  the map or rotating the endpoint explicitly switches to manual editing.
- Optional cached geometry is stored separately from the proposed route. When
  selected, it is translated and rotated from its stored start pose to the
  current aircraft pose and remains editable before Enter.
- Cache matching is nearest-pose selection within 15 metres and 5 degrees,
  reduced from the original fuzzy 30-metre/10-degree AVL identity.
- The Ground Operations window polls X-Plane's global mouse position every
  frame. Leaving the XPLM window immediately clears ImGui hover state and any
  delayed tooltip.
- The current airport database provides ramp-start locations and runway
  geometry, but this slice does not infer pavement, stand obstruction, or
  one-direction-only ramp constraints. The editable pilot review remains the
  authority for those cases.

Automated evidence:

| Check | Result |
| --- | --- |
| KCOS-like NORTH/SOUTH coarse-flow inference | Passed |
| Guaranteed clearance-distance bounds | Passed |
| Gate 8 legacy cache remains eligible at its measured 11.01 m offset | Passed |
| Cached-route translation, clockwise rotation, and heading wrap | Passed |
| All provider, workflow, window, planner-cache, and physics regressions | Passed |
| Windows and Linux warnings-as-errors builds | Passed |
| `git diff --check` | Passed |

Installed candidate SHA-256 values:

- Windows: `8EB53E9F597F63EBC08FA71F7944B87E763E2B102CC00C5F28DCA7B936327EAD`
- Linux: `08F402E3FB3D88899A6910A249ABC39476DA39EB3C58AF893B84D3B193566CF5`

#### Focused simulator validation

1. Keep the aircraft at KCOS Gate 8 so the existing saved route is present.
   Set X-Plane wind to **180 degrees at 10 knots**. Call and connect the tug,
   then press **Plan push**.
2. Confirm the wind/`apt.dat` **SOUTH** proposal appears even though Gate 8 has
   a cache. The blue line must begin centered at the aircraft and run straight
   backward before the first turn. It must not name a specific runway.
3. Move the mouse across and outside Ground Operations. Confirm no helper stays
   on screen. Move the pointer to **Accept plan** without clicking the map and
   confirm the original proposal does not move.
4. Cancel and reopen the planner. Press **Saved route / Optional review**.
   Confirm the route changes only after this click, starts centered at the
   aircraft, and can be deleted, extended, or rotated before Enter.
5. Cancel and reopen with wind **360 degrees at 10 knots**. Confirm the initial
   proposal changes to coarse **NORTH** while the saved route still does not
   auto-load.
6. Accept either editable proposal and complete a normal push. Preserve
   `Log.txt`, telemetry, and screenshots of the SOUTH proposal, explicitly
   selected saved route, and NORTH proposal.

Expected log decisions include `Planner offered uncommitted`, `Saved route
available`, and - only after the button is pressed - `Pilot explicitly selected
the saved route`.

### Validation 2 - heading presentation, turn geometry, and cache transform

Date tested: 2026-08-05  
Disposition: **Rejected**

Findings:

- The planner log selected SOUTH at an aircraft heading of 180.1 degrees, but
  the planner did not make the aircraft-nose versus tail convention clear
  enough to prevent the displayed pose being interpreted in reverse.
- The proposal placed an exact target only 16.8 metres behind the stand and
  handed it to the generic endpoint solver. That produced a tight S-turn with
  an operationally impossible initial steering demand.
- The explicitly selected saved route still rendered left and below the
  aircraft despite a reported 0.02-metre / 0.0-degree match. Cached-route
  transformation is not reliable enough to remain in the workflow.

Rejected binaries and all simulator evidence are preserved at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice2-validation1-rejected-20260805`

### Corrective Slice 3 - broad tow arc and no persistent route reuse

Date prepared: 2026-08-05
Status: **Rejected in simulator and replaced**

Corrected behavior:

- Persistent route loading, presentation, and saving are disabled. The legacy
  cache file is left untouched for evidence but is ignored by operations.
- The proposal reports the aircraft **NOSE** heading and reciprocal **TAIL**
  heading explicitly. A KCOS SOUTH proposal is therefore shown as approximately
  `NOSE 180 / TAIL 000`.
- The proposal no longer gives an arbitrary endpoint to `compute_segs`. It
  builds one deterministic reverse-tow shape: straight back for the greater of
  80 percent of aircraft length or twice the wheelbase, one broad arc using
  only 40 percent of available steering, and a short final straight.
- Heading changes over 135 degrees fall back to manual planning because a safe
  turn side cannot be inferred without ramp-layout knowledge.
- The proposal remains amber, editable, and uncommitted until the pilot presses
  Enter. Calm or ambiguous flow still opens the manual planner.

Automated and build evidence:

| Check | Result |
| --- | --- |
| SOUTH nose 180.1 / tail 000.1 convention | Passed |
| Conservative 737 straight-back, turn-radius, and exit distances | Passed |
| Over-135-degree automatic maneuver rejection | Passed |
| All seven provider, workflow, window, planner, and physics test suites | Passed |
| Windows and Linux warnings-as-errors release builds | Passed |
| Persistent route load/save call-site audit | Passed; no operational call sites |
| `git diff --check` | Passed |

Installed candidate SHA-256 values:

- Windows: `69818CBC498BE29B0AC7AAD706D88A5F6FE0BBA30E535F3D822ECFE476A430D2`
- Linux: `8337180F29423CA85CC5480A6E4053127E9744D23AF362994A66F45B60BD0055`

Focused simulator validation result:

- Rejected at KCOS Gate 8 on 2026-08-05. The screenshot showed an operationally
  unusable route rather than a sensible push from the stand.
- The log recorded a SOUTH proposal with aircraft nose 180.1 degrees, tail 0.1
  degrees, wind 98.8 degrees at 3.6 m/s, 26.8 m straight back, 34.9 m turn
  radius, and 93.7 m total reverse path.
- The automatic proposal experiment was ended by product decision. Exact
  binaries, screenshot, log, telemetry, and evidence notes are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice3-validation1-rejected-20260805`.

### Corrective Slice 4 - restore manual planner

Date prepared: 2026-08-05  
Status: **Accepted in simulator**

Product decision:

- Opening **Plan push** must return to the original manual-placement workflow.
- No wind/runway inference, automatic route geometry, departure-direction
  message, or proposal accept/edit state remains.
- The pilot moves and rotates the cursor, clicks to place the desired aircraft
  pose, edits as needed, and explicitly accepts the route.
- The planner's controller-matched blue trajectory, magenta danger band, and
  in-session route retention remain.
- Persistent route loading and saving remain disabled. Cache alignment will be
  addressed separately after this manual workflow is validated.

Verification:

| Check | Result |
| --- | --- |
| Six remaining automated regression suites | Passed |
| Windows warnings-as-errors release build | Passed |
| Linux warnings-as-errors release build | Passed |
| Source audit for proposal implementation references | Passed; none remain |
| Windows/Linux binary-string audit for proposal text | Passed; none remain |
| `git diff --check` for planner source | Passed |

Installed candidate SHA-256 values:

- Windows: `0BAE410F37632C192AF1DF31710DC6272B6A0B7139C7AF877F8B12DF5636EC09`
- Linux: `71A2C20C80A66AB2704F4EF9CC94A24DCED8C6D7A5EECA106B005617C3241D18`

Focused simulator validation:

1. Call the tug, wait for nose-gear capture, and press **Plan push**.
2. Confirm the planner opens with no prebuilt route, no suggested-departure
   message, and no **Saved route** control.
3. Move and rotate the cursor, click to place the desired aircraft endpoint,
   and confirm the blue trajectory and magenta band update normally.
4. Edit or delete one placed endpoint, then create the intended route again.
5. Accept the manual route and complete one normal push while preserving the
   planner screenshot and `Log.txt`.

Simulator validation result:

- Passed on 2026-08-05 at KCOS with the AST-3F tug.
- The planner logged `Planner opened for manual route placement`; no automatic
  proposal or saved route was presented.
- The pilot manually placed and accepted the route, and the operation completed
  successfully.
- Planner prediction telemetry reported 393 builds, 2,154 cache hits, 519
  cursor solves, 2,029 cursor reuses, zero failures, and a maximum build time of
  1.09 ms.
- X-Plane exited cleanly. Exact accepted binaries, `Log.txt`, telemetry,
  screenshot, and evidence notes are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice4-validation1-accepted-20260805`.

### Corrective Slice 5 - published-start gate cache rewrite

Date prepared: 2026-08-05
Status: **Accepted after two-stage simulator validation**

Product decisions implemented:

- The active scenery-priority-resolved `apt.dat` row-1300 location is the
  persistent cache identity: airport, ramp name, latitude, longitude, and true
  heading.
- A route is cacheable only when the live nosewheel uniquely matches a
  published start within 1 m and 1 degree.
- An arbitrary or saved-situation start remains fully usable for the current
  manual pushback session, but persistent load and save are disabled.
- The cache uses a new file,
  `Output/caches/BetterPushback_gate_routes_v1.dat`; the legacy route file is
  ignored by the active planner.
- Controller segments are stored in an aircraft-heading-relative metre frame
  anchored at the nosewheel. No altitude-zero geographic round trip, nearest
  route shifting, or 15 m acceptance window is used.
- Internal steering remains main-gear referenced. The blue planner trajectory
  is drawn from the corresponding nosewheel positions, making its first point
  the published ramp anchor.

Verification:

| Check | Result |
| --- | --- |
| Strict published-start and reversible anchor-transform tests | Passed |
| Five existing ground-ops, planner-cache, and vehicle-physics suites | Passed |
| Windows warnings-as-errors release build | Passed |
| Linux warnings-as-errors release build | Passed |
| `git diff --check` | Passed |

Installed candidate SHA-256 values:

- Windows: `13BA30F5BCFA759B0B1BBE8FFB4BA0295819BEC0F203E8C19801BC3B796D3725`
- Linux: `D9D98FF7C593E818B4E8850D77A38EC08B8643F4BBAB1E520EDA3A98768DC089`

Focused simulator validation completed on 2026-08-05:

- The test began from a clean slate with both `BetterPushback_routes.dat` and
  `BetterPushback_gate_routes_v1.dat` absent.
- Test 1 loaded the 737-700NG at KCOS Gate 8. The blue trajectory began at the
  aircraft nosewheel, the pilot drew and accepted the manual route, the new
  cache file was created, and pushback/ground-operations completion succeeded.
- Test 2 reloaded the same aircraft at the same published start. The log reports
  `Gate route cache recalled for KCOS Gate 8; published anchor 38.79933100,
  -104.70027200 at 269.20 degrees`. The recalled blue route began at the same
  nosewheel location and retained the intended endpoint.
- The second planner summary reports 217 recalled preview points, 76 successful
  path builds, 2,691 path-cache hits, and zero prediction failures. The second
  operation reached controller `off`, preparation `complete`, and a clean plugin
  unload.
- The two saved cache records have the same gate identity, aircraft geometry,
  anchor, heading, and route. Segment-coordinate differences are only
  floating-point serialization noise of approximately 4e-12 m.
- Both telemetry files contain the complete operational sequence through
  `driving_away`, with 3,526 and 3,422 samples respectively and no NaN or
  infinity values.
- Screenshots confirm the initial and recalled blue trajectories begin at the
  nosewheel and the ground-operations UI reports successful completion in both
  runs.
- Accepted binaries, screenshots, `Log.txt`, cache data, and both telemetry
  files are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice5-validation1-accepted-20260805`.

### Corrective Slice 6 - two selectable routes per gate and aircraft

Date prepared: 2026-08-05
Status: **Confirmed**

Product decisions implemented:

- Persistent routes now live exclusively under
  `Output/caches/BetterPushback_Gate_Routes`.
- The folder hierarchy separates airport, published gate identity, and aircraft
  filename/landing-gear profile. Each compatible profile contains exactly
  `slot_1.route` and `slot_2.route` initially.
- The accepted root-level `BetterPushback_gate_routes_v1.dat` is not read,
  migrated, modified, or deleted by the candidate.
- With one or two compatible slots, the overhead planner displays a modal route
  chooser before loading any route. The pilot may select Route 1, Route 2, or a
  new manual plan.
- A new route fills the first empty slot. If both slots are occupied, Enter
  requires an explicit choice to replace Route 1, replace Route 2, use the route
  once without saving, or return to planning.
- An unchanged recalled route is not rewritten. Editing a recalled route saves
  back to that selected slot.
- Slot files retain the accepted published nosewheel anchor, aircraft-relative
  metre geometry, exact aircraft/gear validation, first-pose validation, and
  automatically calculated tail-direction/final-heading metadata.
- Each slot is written to a temporary file and atomically replaces only its own
  destination after a complete successful write.

Verification completed:

| Check | Result |
| --- | --- |
| Strict anchor/transform/tail-direction math | Passed |
| Empty, one-slot, full, unchanged, edited, replacement and use-once policy | Passed |
| Five existing ground-ops, planner-cache, and vehicle-physics suites | Passed |
| Windows warnings-as-errors release build | Passed |
| Linux warnings-as-errors release build | Passed |
| `git diff --check` | Passed |

Installed and accepted SHA-256 values:

- Windows: `67C6A63380A6AFE9140A84F596CF93C30A8F3442AC068224A65555CC92A708E3`
- Linux: `A81481AC9042AD3B75463550783C7ED134842DD998E740B7F815AB1414CB4673`

Simulator validation completed on 2026-08-05:

- Log 1 recognized the published KCOS Gate 8 start with no compatible slots and
  saved 737-700NG Slot 1 as `Tail S`, final aircraft heading 359.20 degrees.
- Log 2 found one compatible 737 slot, selected a new manual route, and saved
  Slot 2 as `Tail N`, final aircraft heading 180.70 degrees.
- Log 3 found both 737 slots, recalled Slot 1, edited it, and saved only Slot 1
  with the updated final heading 359.70 degrees. The final 737 chooser displayed
  Route 1 at 360 degrees and Route 2 at 181 degrees.
- Log 4 loaded the Felis B742 at the same published gate and found no compatible
  slots, demonstrating aircraft-profile isolation. It saved B742 Slot 1 as
  `Tail S`, final aircraft heading 0.70 degrees.
- Log 5 found only the B742 Slot 1, selected a new manual route, and saved B742
  Slot 2 as `Tail E`, final aircraft heading 268.20 degrees. The final B742
  chooser displayed both aircraft-specific routes while the final 737 chooser
  still displayed only the 737 pair.
- The final cache tree contains exactly four route files: two under the
  `737_70NG.acf` profile and two under the `B742_Cargo_Felis_XP12.acf` profile.
  All four use the same published anchor `38.79933100, -104.70027200` at 269.20
  degrees, and no temporary files remain.
- Every planner cache summary reports zero failures. Every Ground Ops session
  reached controller `off`, preparation `complete`, and clean plugin unload.
- The five telemetry recordings contain 3,340, 3,489, 3,404, 4,137, and 4,392
  samples respectively. Each contains the complete operational state sequence
  through `driving_away`, and none contains NaN or infinity values.
- Accepted binaries, five logs, five telemetry files, three chooser screenshots,
  and the complete four-file cache tree are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice6-validation1-accepted-20260805`.

Commit criterion: **Passed**.

## UI cleanup

### Pass 1 - briefing strip and flight identity

Date prepared: 2026-08-06
Status: **Accepted after simulator visual validation**

Product decisions implemented:

- The expanded Ground Operations panel no longer draws EOBT, METAR, or ATIS.
- Airport, simulator wind/temperature, and QNH remain in the briefing strip.
- The header prefers X-Plane's assigned Flight ID. When the Flight ID is blank
  or contains the aircraft type placeholder, the aircraft ICAO type is shown as
  the fallback identity.
- The change is isolated from the manual planner, saved gate routes, tug
  physics, and wing-walker behavior and assets.

Verification completed:

| Check | Result |
| --- | --- |
| Flight-ID precedence and aircraft-type fallback test | Passed |
| Ground Operations state and window-state suites | Passed |
| Windows warnings-as-errors release build | Passed |
| Linux warnings-as-errors release build | Passed |
| `git diff --check` | Passed |

Installed and accepted SHA-256 values:

- Windows: `17E7FB7E447019844F0758F51304C6002931C2A1B24E0ED3CAA0FE70431CC885`
- Linux: `016A4B7C5C96852F3D4884EB51423D1771E490151F5208172708B6A759F7768E`

Simulator validation completed on 2026-08-06:

- The 737-700NG was loaded at KCOS with no assigned flight number.
- The header displayed `Flight B737`, proving the aircraft-type fallback.
- EOBT, METAR, and ATIS were absent. Airport KCOS, simulator wind/temperature,
  and QNH remained visible with the existing panel layout intact.
- The user accepted the visual result from screenshot
  `737_70NG - 2026-08-06 09.28.20.png` (SHA-256
  `552EA2EA24A114FB53890881F6AB3778B458B486A59410CF412CB082A700FBE8`).
- The accepted screenshot and exact binaries are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\ui-cleanup-pass1-accepted-20260806`.

Commit criterion: **Passed**.

### Pass 2 - disable duplicate legacy operational UI

Date prepared: 2026-08-06
Status: **Accepted after simulator visual and operational validation**

Product decisions implemented:

- The Ground Operations panel is the fork's only default operational UI.
- The original four "magic squares" windows and their click handlers remain in
  `bp.c`; no legacy implementation was deleted.
- CMake option `BP_ENABLE_LEGACY_MAGIC_SQUARES` defaults to `OFF`. An upstream
  maintainer can set it to `ON` to restore the original windows without
  reverting source.
- The historical beacon-triggered tug check was separated from legacy window
  creation so the compatibility option changes presentation only.
- The overhead planner, preferences UI, X-Plane commands, disconnect/reconnect
  prompts, Ground Operations panel, cache, physics, Emergency Tow, and wing
  walker remain unchanged.

Verification completed:

| Check | Result |
| --- | --- |
| Four focused Ground Operations/Emergency Tow suites | Passed |
| Complete ten-suite regression matrix | Passed |
| Linux build with legacy windows explicitly enabled | Passed |
| Windows default-off warnings-as-errors release build | Passed |
| Linux default-off warnings-as-errors release build | Passed |
| `git diff --check` | Passed |

Installed and accepted SHA-256 values:

- Windows: `6D4E3A2ED9F6FACCAF88DE0B684414B835FD68AEF913CA5BFDE52A1252DE78F5`
- Linux: `22623D004DE21AA44CF4A059C612E4939FE681A9562D19AA7265FF2BA0F3F897`

Simulator validation completed on 2026-08-06:

- The user confirmed that none of the original legacy windows rendered and the
  Ground Operations panel remained present.
- A normal KCOS operation used the Ground Operations controls, opened and
  restored the accepted overhead planner, recalled an unchanged saved route,
  progressed through normal push and disconnect stages, and retained the wing
  walker behavior.
- BetterPushback logged no errors or assertions. Panel rendering averaged
  0.016 ms and peaked at 0.172 ms during the captured session.
- The five saved gate-route files remained byte-for-byte unchanged.
- Captured telemetry contains 1,556 lines and no NaN or infinity values.
- Exact binaries, `Log.txt`, and telemetry are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\ui-cleanup-pass2-accepted-20260806`.

Commit criterion: **Passed**.

## Emergency Tow Module

Date prepared: 2026-08-06
Status: **Accepted after complete simulator lifecycle validation**

Product decisions implemented:

- The normal completed Ground Operations view exposes an optional blue
  **Call tow back** action after the tug has finished its departure.
- `emergency_tow.[ch]` is a small session coordinator. It reuses the accepted
  connect-first workflow, manual planner, route/controller geometry, tug
  physics, and disconnect sequence instead of duplicating those modules.
- Nose-gear capture opens the existing planner automatically at the live
  aircraft position with a fresh one-time route.
- Emergency Tow bypasses saved-route enumeration and loading at planner entry
  and independently blocks every cache write at planner exit.
- The wing-walker object is not allocated or rendered during the emergency
  session.
- Final tug drive-away ends the submodule and restores the normal cold-start
  **Tug available / Call tug** state.
- A following normal operation uses the existing unique published-start guard;
  an aircraft returned off-anchor cannot load, save, or modify a cached route.

Verification completed:

| Check | Result |
| --- | --- |
| Emergency Tow lifecycle and hard policy tests | Passed |
| Gate-route math and two-slot policy suites | Passed |
| Ground Operations data, state, and window suites | Passed |
| Planner-cache, vehicle-physics, wing-walker logic, and asset suites | Passed |
| Windows warnings-as-errors release build | Passed |
| Linux warnings-as-errors release build | Passed |
| `git diff --check` | Passed |

Installed and accepted SHA-256 values:

- Windows: `6BD26A4225A81DB0DD1EA44879CFE0B8981BA25BBCA40BDE0538DCC6B9FAEEDE`
- Linux: `FCD131A4F5FC5CF565AC06F548AD728CDDA6693488A6C19DCB3AE0336BA7CD87`

Simulator validation completed on 2026-08-06 at KCOS with the 737-700NG and
AST-3F tug:

- A normal cached push completed first with the wing walker and the established
  disconnect/clear sequence intact. The completed UI then displayed
  **Call tow back**.
- Emergency Tow dispatched at 10:27:04. The log confirms that persistent-route
  access and wing-walker rendering were disabled before tug load.
- After capture, the planner opened automatically at the live nosewheel. It
  displayed `Emergency Tow - manual route only; saved routes are disabled` and
  presented no saved-route chooser.
- The return route was accepted once. The planner logged that its hard
  persistence guard skipped every gate-route cache write.
- The aircraft was towed back to the selected gate position, the tug completed
  disconnect and drive-away, and the coordinator restored the cold-start Ground
  Operations state at 10:31:44.
- A second normal push immediately followed. Because the returned aircraft did
  not uniquely match the published anchor, the planner listed no saved routes
  and logged that the route would not be saved. The wing walker was restored
  for this normal operation and completed its normal signal sequence.
- All five pre-existing route files remained byte-for-byte identical to the
  pre-test snapshot. No cache route was created, replaced, or rewritten.
- Three telemetry files contain 3,336, 2,697, and 3,263 lines respectively,
  cover the normal/emergency/normal sequence through final tug departure, and
  contain no NaN or infinity values.
- BetterPushback logged no errors or assertions. Two benign pre-existing
  `stop_planner`-disabled warnings occurred while invoking the normal late-plan
  workflow and did not affect either operation.
- Accepted binaries, four screenshots, `Log.txt`, all three telemetry files,
  and the complete unchanged cache tree are preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\emergency-tow-accepted-20260806`.

Commit criterion: **Passed**.

## Open release-engineering requirements

### RE-001 — Reproducible dependency bootstrap

The source-tree build correctly reached the new cache files but the local
sibling `libacfutils` dependency checkout does not currently contain all of the
Windows cross-compiled headers. The dependency-complete staging checkout at
`/opt/betterpushback-build` produced both plugins successfully. Before release,
the fork must document or automate the exact dependency/submodule bootstrap so
a clean machine can reproduce the release artifacts without relying on that
staging checkout. This is tracked for release hardening and does not block the
Phase 1 simulator test.
