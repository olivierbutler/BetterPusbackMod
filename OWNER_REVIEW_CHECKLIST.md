# Owner review checklist

This checklist covers the reconciliation requested on 2026-09-04. Automated
tests validate state mapping, route helpers, and both UI scale modes; the final
visual and motion checks require X-Plane.

## Build and UI

- Build macOS normally. The Ground Operations rail should be 78 by 329 and the
  panel 394 by 567, with fonts, spacing, controls, hit targets, and drag
  threshold all scaled together.
- For a Windows/Linux visual preview, run `./build_xpl.sh -m`. The normal build
  without `-m` must remain at 58 by 244 and 292 by 420.
- Hover every Ground Operations control. Panel tooltips must wrap and remain
  inside the panel.
- Confirm every CURRENT TASK message remains inside its card. The completed
  operation message must render on exactly two lines.
- Visit every normal and Emergency Tow panel state at both normal and enlarged
  UI scale. Confirm status, detail, caption, task, metric, and button text wraps
  or reduces in size without clipping or crossing a panel/card boundary.
- Confirm the stage rail uses green for completed stages, yellow for the current
  stage, and cyan for future stages. No separate yellow exclamation badge may
  appear.
- Confirm the installed package contains the wing-walker OBJ, diffuse texture,
  LIT texture, and attribution file under `objects/wing_walker`.
- Confirm a normal build does not log `Recording pushback telemetry` and does
  not create a new CSV in `Output/BetterPushback/telemetry`. Telemetry must be
  present only in an explicit `./build_xpl.sh -t` diagnostic build.
- Confirm the Preferences window does not show Auto disconnect when done,
  either Hide magic squares control, Magic squares position, Enable Ground
  Operations UI, or Show Ground Operations captions.
- Confirm ACF Plugin Exclusion (Experimental), Eye Tracker Plugin Exclusion,
  and all other retained upstream preferences are present.

## Normal operation

- Draw and accept straight, single-turn, multi-turn, and forward-tow routes.
  Compare planner construction, steering, turn transitions, tug approach, and
  final stopping with the upstream legacy behavior.
- While connected with the parking brake set, confirm **Change plan** appears
  and reopens the planner. Release the brake and confirm the action disappears
  before motion.
- Pause during a straight and a turn. Confirm only longitudinal motion stops,
  then resume and verify the same route continues.
- Use **End operation** while moving and while held. Confirm the aircraft stops
  and disconnects at the current position without returning to the planned
  endpoint.
- At **Ready to disconnect**, confirm the task card uses the normal neutral
  styling and offers **Disconnect tug** and **Reconnect** inside the Ground
  Operations panel. Confirm no separate disconnect or reconnect windows appear.
- Choose **Reconnect** once and confirm the controller returns to the connection
  sequence. Return to the disconnect gate, choose **Disconnect tug**, and
  confirm the normal clear-signal and tug-departure sequence completes.
- Confirm Ground Operations remains visible throughout an active operation.
  After completion, taxi at or above the legacy 1 m/s threshold and confirm
  the UI hides; slow below 1 m/s while on the ground and confirm the same
  expanded or collapsed view returns automatically.

## Repeated and emergency operation

- Complete normal pushback, **Call tow back**, Emergency Tow, and another
  normal pushback without reloading the plugin.
- Confirm Emergency Tow uses the same legacy motion engine and configured tug
  towing-speed limits.
- Confirm the wing-walker timing remains unchanged for a normal operation and
  remains omitted for Emergency Tow.

Record the tested commit, platform, X-Plane version, aircraft, tug, and any
screenshots or `Log.txt` evidence before release approval.
