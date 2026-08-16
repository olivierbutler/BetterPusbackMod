# Pushback telemetry

This build records each pushback so tug behavior can be measured while the
motion model is tuned.

## Output

One CSV file is created per pushback in:

```text
<X-Plane folder>\Output\BetterPushback\telemetry
```

The filename contains the start time and selected tug, for example:

```text
push_20260801_143205_AST-3F_tug.csv
```

The beginning of the file contains metadata for the plugin, aircraft, loaded
weight, wheelbase, steering limits, startup acceleration ramp, and the exact
tug specification. Samples are then written at 10 Hz and whenever
BetterPushback changes state.

The samples include:

- aircraft and tug position, heading, and speed;
- aircraft acceleration, jerk, and yaw rate;
- tug steering angle and calculated turn radius;
- route steering command and nosewheel steering request;
- turn-profile progress and total distance, the continuous profile target,
  the combined controller target, and the rate-limited steering command
  actually applied;
- the closest point on the active planner segment, signed path cross-track
  and route-heading errors, bounded path-steering correction, and terminal
  handoff weight;
- the planned endpoint, live main-gear and tail positions, target tail
  position, tail cross-track and along-track errors, final heading error,
  and the bounded tail-feedback steering contribution;
- raw and limited target speed;
- applied force, force limit, and force fraction;
- active route segment, remaining distance, planned radius, and heading error;
- brake inputs, parking brake, surface friction, and frame time.

Schema 5 added tail-aware endpoint tracking. The controller retains the smooth
turn profile as feed-forward steering, then adds at most 8 degrees of
rate-limited correction from tail cross-track and final-heading error. On
straight segments the tail tracks the segment centerline. During a turn the
feedback weight rises smoothly toward the next user-placed aircraft pose.
The measured aircraft tail arm, transition distance, and steering-rate limit
are written in the metadata at the top of every CSV.

Schema 6 adds continuous planner-route fidelity. The original route
controller is retained as an error sensor, but it cannot directly command the
nosewheel: only the difference from the smooth feed-forward profile is used,
with a 1-degree deadband and a hard 12-degree correction limit. During the
last 8 meters, that correction fades smoothly to zero so tail-aware endpoint
capture has sole authority at the stop. The final command remains limited to
6 degrees per second. Direct route-reference, cross-track, heading-error,
correction, and handoff-weight fields make intermediate path accuracy
measurable without reconstructing the route from the endpoint.

Schema 7 adds `pause_requested` and `pause_held`. A row is emitted immediately
when either value changes, so a simulator test can measure controlled
deceleration, the stationary hold, and the resumed original route without
inferring those events from speed alone.

The CSV is flushed at least once per second and closed when the pushback is
cancelled or the tug completes its departure. The X-Plane log also prints the
full output path when recording begins.

## First controlled test

1. Load the aircraft and airport normally and leave weather and aircraft
   loading unchanged for the test.
2. Plan a reverse push containing at least two opposing turns followed by a
   short straight segment. For a controlled comparison, reuse the compound
   route from the previous test.
3. Complete the entire push through tug disconnect and departure. Avoid brake,
   steering, or manual-push input unless needed for safety.
4. Send the newest CSV from the telemetry folder for analysis.

For useful comparisons after a tuning change, repeat the same route with the
same aircraft loading, airport surface, and weather.
