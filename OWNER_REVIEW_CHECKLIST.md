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
- Complete the operation. Confirm disconnect occurs automatically and no
  separate disconnect or reconnect windows appear.

## Repeated and emergency operation

- Complete normal pushback, **Call tow back**, Emergency Tow, and another
  normal pushback without reloading the plugin.
- Confirm Emergency Tow uses the same legacy motion engine and configured tug
  towing-speed limits.
- Confirm the wing-walker timing remains unchanged for a normal operation and
  remains omitted for Emergency Tow.

Record the tested commit, platform, X-Plane version, aircraft, tug, and any
screenshots or `Log.txt` evidence before release approval.
