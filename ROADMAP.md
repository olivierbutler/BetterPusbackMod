# Realistic Better Pushback Roadmap

This document is the source of truth for the realism fork. A phase is complete
only after all of its exit gates have evidence. New ideas are added to the
traceability table before they are scheduled so agreed requirements are not
lost.

Status date: 2026-09-04

## Owner-review reconciliation

The 2026-09-04 owner review supersedes older experimental-controller text in
this roadmap. The release candidate must:

- use upstream's legacy route construction, `drive_segs` steering, turn
  tracking, planned-route stopping, and tug approach geometry;
- retain only configured loaded tug speed limits from the physics experiment;
- pause longitudinal motion without replacing legacy steering, resume the
  same accepted route, and let **Stop Here** disconnect without a planned-end
  correction;
- disconnect automatically and never create the old disconnect/reconnect
  windows, while keeping their source available;
- always enable Ground Operations and captions, with the obsolete preference
  rows removed and the original ACF Plugin Exclusion restored;
- offer **Change plan** only while connected and held by the parking brake;
- scale Ground Operations proportionally to 1.35 on macOS, with a matching
  Windows/Linux development-emulation build for review.

Automated source tests cover the default and emulated platform scales. Final
visual acceptance still requires the owner's macOS simulator check.

## Non-negotiable guardrails

1. Preserve the upstream legacy planner and push trajectory exactly; UI and
   telemetry must remain observational.
2. Preserve endpoint accuracy within the tested operational margin.
3. Do not add frame-rate-bound business logic, networking, parsing, or planner
   simulation.
4. Keep ordinary manual pushback fully functional offline.
5. Never call XPLM APIs from a worker thread.
6. Keep classic menu commands available as a recovery path.
7. Separate UI intent from aircraft/tug mutation.
8. Build and test each phase before adding the next source of risk.
9. The normal Ground Operations workflow shows airport context and waits for an
   explicit pilot **Call tug** action. It has no artificial dispatch timer and
   still holds before lift at an amber **Plan push** gate.

## Phase status

| Phase | Deliverable | Status | Depends on |
| ---: | --- | --- | --- |
| 0 | Approved UI design and complete roadmap | Complete | — |
| 1 | Planner prediction and terrain cache | Complete | Phase 0 |
| 2 | Inert hidden/compact-rail/panel window shell | Complete | Phase 1 |
| 3 | State progression, pilot-called connection, and captions | Complete | Phase 2 |
| 4 | Controlled Pause/Resume and End/Disconnect | Complete | Phase 3 |
| 5 | Simulator-local flight and weather context | Complete | Phase 3 |
| 6 | Ground-crew scheduling and communications workflow | Complete | Phases 3, 5 |
| 7 | Manual planner and legacy motion restoration | Complete | Phase 1 |
| 8 | Automatic disconnect and tug-driver presentation | In progress | Phases 3, 6 |
| 9 | Compatibility, performance, and long-session hardening | In progress | Phases 1-8 |
| 10 | Release candidate, documentation, and packaging | Pending | Phase 9 |

Detailed UI behavior is defined in `GROUND_OPS_UI_DESIGN.md`.

## Phase 0 — Design baseline

### Deliverables

- Approved three-state presentation: hidden, compact progress rail, expanded narrow
  panel.
- Existing overhead planner retained for route editing.
- Screen-space, monitor, pop-out, persistence, data-source, safety, and
  performance contracts recorded.
- Complete requirements traceability table.

### Exit gates

- [x] Pilot approves compact-rail-to-panel direction.
- [x] UI design specification is tracked in the repository.
- [x] Full roadmap is tracked in the repository.
- [x] README links to both documents.

## Phase 1 — Legacy planner caching and performance foundation

### Problem

The legacy planner must not allocate a replacement cursor route while the
cursor and accepted route are unchanged. Draw callbacks can run more than once
per frame, so unchanged geometry is reused without changing its construction.

### Implementation

- Reuse the last legacy `compute_segs` cursor result when its inputs are
  unchanged.
- Invalidate that cache on cursor, route, aircraft geometry, or local-reference
  changes.
- Draw the legacy segment centerline and clearance band without running a
  second motion controller.
- Keep all caching observational: accepted segment coordinates and live
  `drive_segs` inputs remain upstream-compatible.

### Tests

- Unit-test invalidation decisions and revision changes.
- Unit-test that an unchanged route does not rebuild.
- Compare cached and uncached legacy segment endpoints.
- Exercise add, rotate, delete, clear, reopen, and aircraft reload.
- Confirm no segment allocations occur during unchanged planner draws.

### Exit gates

- [x] Centerline and danger-zone appearance remain unchanged.
- [x] Cached and uncached legacy endpoints match.
- [x] An unchanged route and cursor produce zero prediction rebuilds during
      repeated draw callbacks.
- [x] Planner open/close and every edit operation remain stable.
- [x] Automated tests and Windows build pass.
- [x] Live planner FPS comparison shows no regression.

## Phase 2 — Inert compact rail and panel shell

### Implementation

- Add a dedicated `GroundOpsWindow` based on the included `XPImgWindow` layer.
- Centralize `XPImgWindowInit()` and cleanup so preferences and ground-ops
  windows share one safe lifetime.
- Implement hidden, five-stage compact rail, and expanded panel modes.
- Implement click-to-expand and collapse-to-compact-rail.
- Implement click-versus-drag discrimination for the compact rail.
- Expand toward the available screen area so a rail placed against either
  screen edge always opens a fully visible panel with an accessible drag grip.
- Implement floating, pop-out, return-to-sim, and monitor recovery.
- Persist floating and operating-system geometry separately.
- Add show/hide and expand/collapse X-Plane commands and menu entries.
- Render static placeholders only; do not mutate tug or aircraft state.
- Keep Ground Operations enabled while preserving classic menu commands.
- Apply one proportional 1.35 macOS scale to window geometry, text, and hit
  targets, plus a development emulation build on Windows/Linux.

### Tests

- 100, 125, 150, and 200 percent UI scale.
- Single monitor, multiple monitors, unplugged remembered monitor.
- Windowed, full-screen, popped-out, and return-to-sim transitions.
- Repeated expand/collapse and plugin reload.
- Preference window and ground-ops window open in either order.
- X-Plane 11 and 12 window behavior where supported.

### Exit gates

- [x] Compact five-stage rail stays within its size contract.
- [x] Expanded panel stays within its width contract.
- [x] No cockpit dataref or push state changes from the inert shell.
- [x] Hidden mode has effectively zero UI overhead.
- [x] Compact rail and panel stay below their measured CPU ceilings.
- [x] No plugin-owned per-frame allocation after warm-up.
- [x] Invalid saved monitor geometry always recovers visibly.

## Phase 3 — State progression, pilot-called connection, and captions

### Implementation

- Add a fixed-size `ground_ops_snapshot_t`.
- Map detailed `PB_STEP_*` values into Tug, Connect, Comms, Push, and Clear.
- Add a pre-push state for parsed airport data without overloading `bp.step`.
- Present an amber **Call tug** action immediately, with no artificial timer.
- Queue the button intent outside drawing and dispatch the existing
  connect-first controller only when the pilot presses it.
- Stop after nose-gear capture and before any lift, then expose the amber
  **Plan push** gate. Begin lift only after a route is accepted.
- Preformat status, source, speed, distance, and action-availability values only
  when the snapshot changes.
- Drive the five compact-rail nodes from the snapshot.
- Show the amber action marker only when pilot input is required.
- Mirror existing crew messages as always-enabled on-screen captions.
- Collapse and restore correctly around the overhead planner.
- Add bounded transition logging for telemetry and troubleshooting.

### Exit gates

- [x] Every existing `PB_STEP_*` maps to one documented UI state.
- [x] The draw path never changes pushback state; queued Call tug intent uses
      the existing command handler on the UI manager callback.
- [x] Captions match the active ground-crew message.
- [x] Waiting states do not show false timer-based progress.
- [x] Planner, push, stop, disconnect, and completion transitions are correct
      in logs and on screen. The legacy reconnect state retains exhaustive
      mapper-test coverage and was not invoked in the accepted run.

## Phase 4 — Pause, Resume, and End/Disconnect

### Implementation

- Add an explicit automatic-push command for pause/resume.
- Pause requests controlled deceleration using the existing acceleration and
  deceleration model.
- Hold preserves the active route, segment progress, steering state, and turn
  profile.
- Resume ramps from zero without a steering jump.
- Keep the legacy terminating Stop path as **End pushback and disconnect**.
- End/Disconnect remains separate from the operational hold and requires
  confirmation in the panel.
- Reject unsafe commands during connection, lowering, disconnect, or after
  completion.
- Extend telemetry with pause request and stationary-hold state changes.

### Tests

- Pause and resume on a straight segment.
- Pause and resume before, during, and after a turn.
- Repeated pause/resume requests.
- End/Disconnect while moving and while already held.
- Parking brake interactions while held.
- Verify no heading snap, speed discontinuity, route loss, or endpoint drift.

### Exit gates

- [x] Pause reaches a smooth stationary hold.
- [x] Resume has no steering jump and uses controlled acceleration.
- [x] Route and endpoint remain within the validated margin.
- [x] Pause and terminating Stop cannot be confused in UI or commands.
- [x] Moving End/Disconnect remains available, supports Keep pushing, and
      completes the legacy stopping and disconnect sequence.
- [x] End/Disconnect from an already-stationary pause hold remains at zero speed
      through the parking-brake request, including with approximately 37° of
      turn articulation. Accepted in simulator validation on 2026-08-04.

### Validation evidence

- Moving End/Disconnect, Keep pushing, repeated Pause/Resume, turning holds,
  and the parking-brake Resume interlock passed the first simulator matrix.
- The corrected stationary End handoff maintained a zero commanded target speed
  while neutralizing approximately `-37.4°` of nosewheel articulation, then
  completed the parking-brake and disconnect sequence without visible movement.
- Accepted Windows artifact SHA-256:
  `44942CFA17E5819D6FA51FF14CD5D1BEC8153BB23FB52417B08F193C03A205A3`
- Accepted evidence is preserved in
  `backups\phase4-validation2-accepted-20260804`.

## Phase 5 — Simulator-local flight and weather context

### Implementation

- [x] Define fixed-size provider interfaces for flight identity, schedule,
      weather, METAR, and ATIS.
- [x] Add simulator-local Flight ID and weather fallbacks first.
      An assigned Flight ID is preferred and aircraft ICAO is the display
      fallback. QNH is shown in hPa and inHg, and the fixed briefing layout
      does not rotate or depend on icon-font glyphs.
- [x] Keep the operational plugin independent of SimBrief. A pushback does not
      require an OFP, schedule download, account token, or flight-plan parser.
- [x] Do not start a METAR or ATIS network worker. EOBT, METAR, and ATIS are no
      longer displayed in the active Ground Operations panel.
- [x] Keep simulator reads on the main thread and at the accepted one-hertz UI
      manager rate. No network or parsing worker exists in this phase.
- [x] Publish only validated fixed-size results to the UI state mapper.
- [x] Display source and staleness for active simulator-local data; never
      fabricate missing data.

### Refresh policy

- Simulator weather is sampled at one hertz only while the Ground Operations
  presentation is visible.
- Hiding the presentation cancels its manager callback.
- No external response cache or recurring network poll exists.

### Failure tests

- Offline from startup.
- Missing or malformed simulator Flight ID with aircraft-ICAO fallback.
- Changing airport, aircraft reload, and plugin disable/stop.
- Stale local snapshot and unavailable weather datarefs.

### Exit gates

- [x] Manual pushback works identically with every external provider disabled.
- [x] No network operation can block or delay an X-Plane callback.
- [x] No XPLM call occurs off the main thread.
- [x] Source and freshness are visible for the simulator-local provider.
- [x] Provider memory remains fixed-size and bounded.

## Phase 6 — Ground-crew and communications workflow

### Implementation

- Add a ground-operations controller separate from the motion controller.
- Load aircraft and airport context locally.
- Represent tug unavailable, scheduled, approaching, staged, connecting,
  connected, ready for comms, disconnecting, and clear.
- Retain the explicit pilot **Call tug** action established in Phase 3 and
  enrich the surrounding workflow with sourced simulator context.
- Keep connection automatic only after that call. The next pilot interaction
  is the connected pre-lift planning/communications gate.
- Validate parking-brake release/set against simulator datarefs.
- Add captions for radio/crew exchanges without replacing existing sound packs.
- Make every waiting reason visible and recoverable.
- Provide an optional post-completion Emergency Tow workflow that reconnects
  the tug, opens a one-time manual route, omits the wing walker, and returns the
  plugin to its cold-start state after final tug departure.
- Hard-disable persistent route listing, loading, replacement, and saving for
  Emergency Tow. Preserve the strict published-start guard for the next normal
  operation so an off-anchor returned aircraft cannot modify the cache.

### Exit gates

- [ ] The workflow can be completed entirely offline.
- [ ] The workflow can be bypassed through classic commands.
- [ ] No automatic action occurs earlier than its configured readiness rule.
- [ ] Door/GPU/ASU and brake mismatches are explained without forcing aircraft
      systems.
- [ ] Emergency Tow and abort paths remain valid after automatic disconnect.
- [x] A completed operation can call an isolated Emergency Tow and return to a
      normal cold-start workflow without changing any gate-route cache file.

## Phase 7 — Manual planner restoration and route-cache correction

The automatic wind/runway proposal experiment was rejected after three
simulator candidates produced unusable planner geometry. That feature and its
dedicated implementation were removed on 2026-08-05.

### Implementation

- [x] Restore planner startup to the original manual-placement workflow.
- [x] Remove wind/runway inference, automatic segment seeding, proposal UI text,
      and proposal-specific input handling.
- [x] Keep the legacy blue planner route and magenta danger band as visual
      review tools for the pilot-drawn route.
- [x] Preserve a route explicitly drawn in the current planner session.
- [x] Keep persistent route loading and saving disabled while its alignment
      defect remains unresolved.
- [ ] Specify the expected persistent-cache coordinate and pose contract with
      the pilot before changing cache code.
- [ ] Correct and simulator-validate persistent route loading and saving.

### Exit gates

- [x] Windows and Linux release builds contain no automatic-proposal code or
      proposal strings.
- [x] Remaining automated regression suites pass.
- [x] Simulator validation confirms the planner opens with no prebuilt route and
      the pilot can place, edit, accept, and execute a manual route normally.
- [ ] Persistent route reuse aligns exactly with the current aircraft pose and
      loads only under the agreed conditions.

## Phase 8 — Disconnect and tug-driver presentation

### Implementation

- Add per-aircraft or global driver-position preference: left, ahead, or right.
- Validate the requested path against tug steering limits.
- Preserve the existing fallback drive-away route.
- Show lowering, release, moving clear, pin presentation, and driving-away
  progression.
- Add an ahead option suitable for cockpits where the side-window pillar hides
  the driver.
- Persist the preference through the existing configuration layer.

### Exit gates

- [ ] Driver is visible from representative left-seat cockpit positions.
- [ ] Tug does not cross the aircraft footprint during repositioning.
- [ ] Disconnect cannot hang if the preferred presentation route is invalid.
- [x] Final disconnection is automatic and creates no disconnect/reconnect
      decision windows.

## Phase 9 — Hardening and compatibility

### Test matrix

- X-Plane 12 current stable on Windows.
- X-Plane 11 compatibility build and smoke test.
- Linux and macOS builds and smoke tests before public release.
- Laminar default narrow-body and wide-body aircraft.
- Popular third-party aircraft when available, including custom steering and
  brake integrations.
- Towbar and towbarless tugs.
- Straight, single-turn, S-turn, tight-area, forward-tow, and abort scenarios.
- Online and fully offline operation.
- Single monitor, multi-monitor, pop-out, high DPI, and VR smoke test.

### Endurance and recovery

- Ten consecutive pushbacks without reloading the plugin.
- One long preflight with the compact rail visible.
- Aircraft reload before, during a safe waiting state, and after completion.
- Plugin reload and simulator shutdown with network worker active.
- Repeated planner editing and camera movement.
- Memory, handle, flight-loop, window, and thread leak checks.
- Log review for repeated errors or frame-rate-bound messages.

### Exit gates

- [ ] No known crash, hang, aircraft-power, gear, or brake regression.
- [ ] No measurable idle FPS regression beyond the documented budget.
- [ ] No unbounded memory growth.
- [ ] Every external failure has a tested fallback.
- [ ] Telemetry contains enough context to reproduce motion defects.

## Phase 10 — Release candidate

### Deliverables

- Versioned release notes describing physics, planner, UI, networking, and
  compatibility changes separately.
- Installation package containing the entire plugin folder.
- Upgrade notes and safe rollback instructions.
- User guide for compact-rail/panel controls, planner proposal, pause/resume, offline
  behavior, pop-out, and driver position.
- Privacy note listing optional external requests and how to disable them.
- Known-issues list and telemetry collection instructions.

### Exit gates

- [ ] Clean build from documented prerequisites.
- [ ] Packaged plugin loads on each supported platform.
- [ ] Fresh install and upgrade install both pass.
- [ ] Release candidate receives live pushback validation before publication.
- [ ] Git tag, source archive, binary hashes, and rollback package are recorded.

## Requirements traceability

| Agreed requirement | Delivery phase | Acceptance evidence |
| --- | ---: | --- |
| Cache planner prediction and terrain heights | 1 | Rebuild/probe counters and unchanged-route test |
| Preserve smooth blue trajectory and magenta danger band | 1, 7 | Geometry comparison and live manual-planner review |
| No repeated expensive work in draw callbacks | 1, 2, 3 | Instrumented callback timings and allocation audit |
| Narrow vertical panel | 2 | Size tests at supported UI scales |
| Clickable five-stage progress rail | 2 | Interaction and drag-threshold tests |
| Click compact rail to open the full panel | 2 | UI state test |
| Collapse panel back to compact rail | 2 | UI state test |
| Hidden/minimal/full presentation modes | 2 | Persistence and lifecycle tests |
| Move around X-Plane | 2 | Floating-window test |
| Pop out and move to another monitor | 2 | Multi-monitor geometry test |
| Remember position and monitor safely | 2 | Restart and missing-monitor recovery test |
| Flight identity and simulator weather | 5 | Provider and fallback tests |
| EOBT, METAR, and ATIS omitted from active UI | 5 | UI source and simulator screenshot |
| One default operational UI, with legacy UI recoverable | 2, 9 | OFF/ON builds and simulator visual/operation test |
| Tug approaching/staged/connected progression | 3, 6 | State-transition log and UI test |
| Airport context and explicit pilot Call tug action | 3 | Button dispatch and simulator workflow test |
| Hold after nose-gear capture and before lift | 3 | Amber Plan push screenshot and no-lift simulator observation |
| Pilot-triggered simulated communications | 6 | Interphone workflow test |
| Manual planner route placement | 2, 7 | Planner integration and simulator test |
| Persistent route-cache alignment | 7 | Agreed pose contract and simulator reuse test |
| UI remains engaged through disconnect | 3, 8 | End-to-end workflow test |
| One-time Emergency Tow after completion | 6, 8 | Full normal/emergency/normal simulator sequence |
| Emergency/off-anchor routes never modify cache | 6, 7 | Hard persistence gates and byte-for-byte cache audit |
| No wing walker during Emergency Tow | 6, 8 | Allocation guard, lifecycle test, simulator/log review |
| Pause and smooth resume | 4 | Telemetry and live motion test |
| Safety stop distinct from cancel | 4 | Command/UI state tests |
| Tug-driver position after disconnect | 8 | Cockpit visibility and drive-away tests |
| Lower-end-machine support | 1, 2, 3, 5, 9 | CPU, allocation, cache, and endurance budgets |
| No endless flight or network loops | All | Callback/thread lifecycle audit |
| Bounded memory allocation and caching | 1, 3, 5, 9 | Allocation counters and endurance test |
| Offline-safe operation | 5, 6, 9 | Network-disabled end-to-end test |
| Classic recovery path | 2, 6, 9 | Menu/command regression test |
| Full compatibility and stability research | 9 | Test matrix and known-issues review |

## Change-control rule

Any new feature request must be added to the requirements traceability table
with a delivery phase and acceptance evidence before implementation begins. A
phase may not be marked complete because it merely compiles; every exit gate
must be checked or explicitly deferred with a recorded reason.
