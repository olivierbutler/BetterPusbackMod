# Ground Operations UI Design

Status: approved product direction; Phases 3 and 4 accepted  
Last updated: 2026-08-04

## Purpose

The ground operations interface must add immersion and control without covering
the cockpit. It is a compact operational companion to the existing overhead
planner, not a replacement for the planner and not a continuously running AI
dashboard.

The approved interaction is:

1. Hidden when no ground operation is relevant.
2. A narrow five-stage progress rail while the workflow is active.
3. A narrow vertical panel when the pilot clicks the rail.
4. Back to the rail when the pilot collapses the panel.
5. The existing overhead planner opens only when route review or editing is
   requested.

The startup workflow is fixed: display parsed airport context and an amber
**Call tug** action immediately. There is no artificial dispatch timer. When
the pilot presses the button, the ground crew approaches and captures the nose
gear automatically, then stops before lift at the amber **Plan push** gate.
Accepting a plan releases the controller into the lift and push sequence.

The pilot always controls whether the information panel is expanded. Normal
state changes must update the compact rail but must not unexpectedly cover the
cockpit.

## Design principles

- **Screen discipline:** the persistent presentation is the compact rail, not
  the panel.
- **One current task:** the expanded panel shows the action required now rather
  than a dashboard of every possible value.
- **Deterministic behavior:** proposed routes and status changes must explain
  their source; no opaque AI decision controls the aircraft.
- **Manual escape path:** the existing planner remains available at every
  planning decision.
- **Safety before convenience:** pause, safety hold, cancel, and abort are
  distinct operations.
- **Offline first:** network connectivity must never prevent an ordinary
  manual pushback.
- **No physics regression:** the tested push controller remains separate from
  the UI and ground-operations workflow.

## Window modes

### Hidden

- No visible X-Plane window and no UI draw callback.
- Available before a ground operation through the plugin menu and a command.
- Automatically returns after the completed-operation timeout unless the pilot
  has selected "keep compact rail visible."

### Compact progress rail

- Default active mode.
- Nominal size at 100 percent UI scale: 58 by 244 boxels.
- Contains the same Tug, Connect, Comms, Push, and Clear nodes used by the
  expanded panel, without any additional dashboard content.
- Completed stages are filled, the current stage is highlighted, and future
  stages remain outlined.
- Entire rail is clickable.
- A click expands the narrow panel.
- Dragging moves the rail without expanding it. The implementation must use a
  small movement threshold to distinguish a click from a drag.
- Hover text gives the full current status for pilots who do not want to expand
  the panel.
- A red safety treatment may replace the normal current-stage highlight only
  while the tug is holding because of a safety stop or blocking condition.
- No timer-based animation may imply progress while the operation is waiting.

### Expanded panel

- Nominal width at 100 percent UI scale: 292 boxels.
- Allowed width after scaling: 280 to 340 boxels.
- Content-driven height, normally 320 to 410 boxels.
- Narrow vertical layout with a five-stage rail on the left.
- The upper-left grip/title region moves the window.
- The upper-right control collapses directly to the compact rail.
- The panel and compact rail share one saved anchor position.
- When the rail touches the left edge, the panel expands rightward. When it
  touches the right edge, the panel expands leftward. The complete title-bar
  drag area must always remain visible.
- Only the content for the current state is rendered.

The panel is divided into:

1. **Title:** Ground operations, flight identity, and current status.
2. **Briefing strip:** airport, simulator wind and temperature, and QNH.
3. **Stage rail:** Tug, Connect, Comms, Push, Clear.
4. **Current-task area:** status, explanation, and valid actions.
5. **Source footer:** data provenance and offline state.

### Planner transition

- Clicking Manual planner or Edit opens the existing overhead planner.
- The panel collapses to the compact rail before the planner camera opens.
- The compact rail is hidden while the overhead planner owns the screen.
- Closing the planner restores the previous panel mode unless the workflow has
  advanced to a state that requires a different presentation.
- The planner remains the authoritative manual route editor.

## Positioning and monitors

The interface will use a modern `XPImgWindow` window. The included window layer
already supports floating, pop-out, and VR modes through `WndMode`.

Required behavior:

- Free movement inside X-Plane.
- Pop out into a first-class operating-system window.
- Move the popped-out window to another monitor.
- Return the window to X-Plane without losing the workflow state.
- Remember floating and popped-out geometry separately.
- Validate saved coordinates against the currently connected monitors.
- Recover to a visible default position if a remembered monitor no longer
  exists.
- Respect X-Plane UI scaling and high-DPI coordinates.

Initial preference keys are expected to cover:

- presentation mode: hidden, compact rail, or panel;
- floating geometry;
- popped-out operating-system geometry;
- preferred monitor;
- UI scale;
- completed-operation auto-hide delay;
- keep-compact-rail-visible option.

Exact key names are an implementation detail, but configuration migration must
be backward compatible.

## Workflow stages and existing pushback states

The UI uses a stable five-stage model and maps the more detailed controller
states into it. The UI must never infer motion merely from a caption string.

| Existing state | UI stage | Live status | Pilot action/control |
| --- | --- | --- | --- |
| `PB_STEP_OFF` | Tug/pre-push | Controller idle or the active pre-push state | Defined by the pre-push state |
| `PB_STEP_TUG_LOAD` | Tug | Selecting a compatible tug | No |
| `PB_STEP_START` | Tug | Ground crew dispatching tug, or Tug standing by | No |
| `PB_STEP_DRIVING_UP_CLOSE` | Tug | Tug approaching aircraft | No |
| `PB_STEP_WAITING_FOR_DOORS` | Tug | Ground crew clearing aircraft service | No amber gate in the automatic connection workflow |
| `PB_STEP_OPENING_CRADLE` | Tug | Preparing the tug cradle | No |
| `PB_STEP_WAITING_FOR_PBRAKE` | Connect | Ground crew connection checks | Automatically secured; no pilot gate in connect-first workflow |
| `PB_STEP_DRIVING_UP_CONNECT` | Connect | Positioning tug to connect | No |
| `PB_STEP_GRABBING` | Connect | Securing the nose gear | No |
| `PB_STEP_LIFTING` | Comms while awaiting plan, then Connect while lifting | Tug connected; plan the push, then Lifting the nose gear | Amber Plan push after capture and before lift |
| `PB_STEP_CONNECTED` | Comms | Ready for brake release, or late plan required | Yes: release brake or complete plan |
| `PB_STEP_STARTING` | Push | Starting pushback | No |
| `PB_STEP_PUSHING` | Push | Pushback in progress, Pausing, or Pushback paused | Pause/Resume plus confirmed End operation |
| `PB_STEP_STOPPING` | Push | Stopping the aircraft | No |
| `PB_STEP_STOPPED` | Push | Aircraft stopped | Yes: set parking brake |
| `PB_STEP_LOWERING` | Clear | Lowering the nose gear | No |
| `PB_STEP_UNGRABBING` | Clear | Releasing the nose gear | No |
| `PB_STEP_WAITING4OK2DISCO` | Clear | Ready to disconnect | Yes: confirm tug disconnection |
| `PB_STEP_MOVING_AWAY` | Clear | Tug moving clear | No |
| `PB_STEP_CLOSING_CRADLE` | Clear | Closing the tug cradle | No |
| `PB_STEP_STARTING2CLEAR` | Clear | Driver moving to clear | No |
| `PB_STEP_MOVING2CLEAR` | Clear | Driver moving to clear | No |
| `PB_STEP_CLEAR_SIGNAL` | Clear | Clear signal displayed | No |
| `PB_STEP_DRIVING_AWAY` | Clear | Tug returning to station | No |

Pre-push presentation remains separate from `bp.step`:

| Pre-push state | UI stage | Live status |
| --- | --- | --- |
| Airport data | Tug | Parsed airport identifier; amber Call tug action |
| Planner review | Comms | Reviewing pushback plan |
| Operation complete | Clear | All five stages complete |

States before `bp_start()`—airport data loaded and ground crew scheduled—
belong to the new ground-operations controller and must not be forced into
`bp.step`.

## Current-task views

### Airport context and automatic connection

Displays only active, simulator-local information:

- Assigned flight number, falling back to aircraft type.
- Departure airport.
- Simulator wind and temperature.
- QNH in hPa and inHg.

The Phase 5 local-provider slice parses and displays the nearest airport,
standard simulator Flight ID when available, and current simulator wind,
temperature, and QNH. It labels the source and freshness explicitly. EOBT,
METAR, and ATIS are not part of the active Ground Operations presentation. The
UI immediately turns amber for the pilot's **Call tug** action. After that call,
approach and capture proceed with no second pilot gate until the controller is
holding before lift.

The standard Flight ID is preferred when it contains a valid assigned value.
When it is blank or identical to the aircraft ICAO code, the aircraft ICAO type
(for example, `B737`) is displayed as the fallback identity. Simulator weather
uses a fixed, non-rotating three-line briefing layout: airport,
wind-temperature, and QNH in both hPa and inHg. The panel uses ASCII-safe
control and stage labels so it does not depend on an icon font. Helper text
requires a stationary delayed hover, uses no shared hover delay, and is removed
on the first frame after the pointer leaves the control.

Unavailable active data uses an em dash or an explicit "Unavailable" label.
The UI must not fabricate a flight identity or weather value.

### Tug staged

- Tug approaching or standing by.
- Door/GPU/ASU readiness when supported.
- Towbar or towbarless connection progression.
- No pilot action until a real action is required.

### Ready for communications

- Primary action: Open interphone.
- Towbar, bypass-pin, and safety-area status.
- Captions mirror the relevant ground-crew message.
- Manual planner remains available.

### Manual push planning

- The overhead planner opens in the original manual-placement workflow. It does
  not infer a departure direction or seed route geometry from wind or runways.
- The pilot moves and rotates the planner cursor, clicks to place the desired
  aircraft pose, and explicitly accepts the completed route.
- A route already drawn in the current planner session may be preserved.
- Persistent gate-route loading and saving remain disabled until the route-cache
  alignment defect is separately specified, corrected, and simulator validated.
- The planner's blue controller trajectory and magenta danger band remain visual
  review tools and do not claim automatic obstacle clearance.

### Brake release

- Communication action acknowledges the pilot call.
- The plugin separately verifies the actual parking-brake dataref.
- Movement cannot begin while the simulator still reports the brake set.
- The UI explains the mismatch instead of forcing the brake state.

### Push in progress

- Current instruction or tail direction.
- Actual tug/aircraft speed.
- Route distance remaining.
- Overall progress.
- Pause/Resume and Safety stop.

Pause is a controlled deceleration to a hold. Resume retains the route and
steering-controller state and uses the proven acceleration ramp. Safety stop is
a hold with a prominent reason. Cancel/abort is separate and requires explicit
confirmation because it can terminate the operation.

### Disconnect

- Parking-brake verification.
- Lowering, towbar/cradle release, and tug movement status.
- Driver presentation setting: left, ahead, or right.
- Equipment-clear completion.

After completion, all five rail stages are filled for the configured delay and
then the rail hides by default.

## Data sources and precedence

Each external value carries source, fetched time, and expiry time.

### Flight identity

1. Simulator/aircraft flight-number datarefs when available.
2. Aircraft ICAO type.
3. Unavailable.

### Weather

1. Simulator weather for operational calculations.
2. Unavailable display, while manual planning remains usable.

### Departure-flow context

1. Route explicitly drawn in the current planner session.
2. One-time coarse flow inferred from X-Plane wind and loaded paved-runway
   geometry.
3. No suggestion; open the unchanged manual planner.

Parallel runways are grouped by direction, so the inference can say NORTH or
SOUTH without choosing a runway. Calm wind, crossing-runway ties, unsupported
geometry, or an unreachable planner endpoint produce no suggestion.

## UI architecture

The first implementation will add separate ground-operations UI files instead
of expanding `cfg.cpp`.

Proposed boundaries:

- `ground_ops_state.[ch]`: fixed-size snapshot and pure mapping from plugin
  state to UI state.
- `ground_ops_ui.cpp/.h`: `XPImgWindow` compact-rail/panel rendering and local
  UI intent.
- Operational button intent is queued out of the draw callback and dispatched
  through the existing X-Plane command handlers on the manager flight loop.
- `ground_ops_data.*`: optional cached flight/weather providers added after the
  inert UI is stable.

The UI reads one immutable `ground_ops_snapshot_t` containing fixed-size or
preformatted values. It does not walk route lists, parse JSON, probe terrain,
or read arbitrary datarefs during drawing.

The initial shell was read-only through early Phase 3. The corrected workflow
adds **Call tug**, **Plan push**, **Pause/Resume**, and confirmed **End
operation** buttons. Call tug starts the existing connect-first workflow only
when pressed. Pause commands zero
speed through the normal deceleration model while retaining route and steering
state. The stationary hold freezes steering and cannot resume through a set
parking brake. End operation uses the compatible terminating stop path and
discards the route only after confirmation.

## Performance contract

- Hidden mode schedules no UI update loop and receives no draw calls.
- Compact rail and panel use X-Plane's normal window draw callback only while
  visible.
- State strings, icons, ring percentage, and button availability update only on
  snapshot changes.
- No route prediction, terrain probing, network I/O, JSON parsing, disk I/O, or
  logging occurs in a draw callback.
- No recurring network poll is tied to frame rate.
- After UI warm-up, the draw path performs no plugin-owned heap allocation.
- The ground-operations coordinator is event-driven and may use a low-rate
  scheduled tick only while an active workflow requires one.

The Phase 3 implementation quantizes speed to 0.1 m/s and remaining distance to
whole meters before comparing inputs. Metric-only changes update the snapshot
without producing semantic transition logs. The optional
`ground_ops_captions_enabled` preference mirrors an issued crew message only for
that message's audio duration, including when simulator sound is disabled.

The late-plan pre-lift hold is exposed explicitly rather than inferred from
`PB_STEP_LIFTING`: once nose-gear capture is complete, the UI advances to Comms
and shows the amber **Plan push** gate while lift position remains zero. After
plan acceptance, the same legacy enum continues into the physical lift.
- Any worker thread blocks on a condition variable or timed job and never busy
  waits.
- Worker threads never call XPLM APIs. Results cross to the main thread through
  a bounded queue or immutable snapshot.

Target measured overhead on the reference test system:

| Mode | Average plugin UI CPU target |
| --- | ---: |
| Hidden | effectively zero |
| Compact rail | no more than 0.10 ms per rendered frame |
| Panel | no more than 0.20 ms per rendered frame |

These are acceptance ceilings, not targets to consume. Lower is expected.

## Failure and recovery behavior

- Network timeout: show cached data if valid, otherwise Unavailable.
- Malformed response: discard it and retain the last valid snapshot.
- Missing flight plan: retain full manual functionality.
- Missing monitor: restore on the primary X-Plane monitor.
- Aircraft reload: close unsafe actions, rebuild aircraft context, and retain
  only safe window preferences.
- Plugin reload: do not resume a partially connected or moving tug.
- UI exception or failed allocation: hide the new UI and leave classic menu
  commands available.
- Planner prediction failure: retain the existing geometric fallback and log a
  bounded diagnostic once per route revision.

## Accessibility and usability

- High-DPI and X-Plane UI scaling supported.
- Status never depends on color alone; icons and text provide the meaning.
- Essential controls remain labeled in the panel.
- Compact-rail hover text provides the current status.
- Menu commands exist for show/hide, expand/collapse, pause/resume, and safety
  stop so pilots can bind hardware or keyboard shortcuts.
- The panel avoids automatic expansion; amber status and voice/caption prompts
  request attention without stealing screen space.
- Text localization uses the existing translation system.

## Explicitly out of scope for the first UI phases

- Large-language-model inference inside the plugin.
- Continuous AI monitoring.
- Automatic obstacle detection for buildings, aircraft, or vehicles.
- Rewriting aircraft FMOD packages.
- Replacing the tested push controller.
- Silently issuing or interpreting real ATC clearance.

These items require separate feasibility, licensing, performance, and safety
work and are not prerequisites for the approved ground-operations experience.
