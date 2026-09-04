# Archived tug-physics experiments

> Historical record only. The steering profiles, tail/path corrections,
> acceleration ramp, jerk limiter, force-cap experiment, corrected tug
> wheelbase, and controller-matched preview described below are not active in
> the current build. Owner review restored the upstream legacy planner,
> `drive_segs` steering, turn/endpoint behavior, tug approach geometry, and
> breakaway behavior. The only retained motion customization is each tug's
> configured loaded towing-speed limit. The per-aircraft experimental plugin
> exclusion was also restored with its original disable/re-enable behavior.

This archive records experiments that separated four quantities the original implementation mixed
or ignored: unloaded speed, aircraft-carrying speed, acceleration/deceleration,
and available tractive effort.

## Steering geometry

`front_z` and `rear_z` are axle positions in model coordinates. The wheelbase
is therefore `front_z - rear_z`. The previous reversed subtraction made every
shipped tug fall through to a synthetic 5 m wheelbase.

The bicycle-model radius used by the simulation is the fixed-axle path radius:

`radius = wheelbase / tan(steering angle)`

At the configured 60-degree steering lock this gives:

| Tug | Wheelbase | Fixed-axle path radius |
| --- | ---: | ---: |
| LEKTRO AP88 | 2.345 m | 1.354 m |
| Goldhofer AST-3F | 2.990 m | 1.726 m |
| Goldhofer AST-1X | 4.390 m | 2.535 m |

These are not exterior clearance radii. Manufacturer turning-radius figures
usually describe the outer swept envelope, so they must not be substituted
directly into the axle-center kinematic equation. The route planner reserves
10 percent steering authority for path correction.

## Verified performance data

- The LEKTRO AP88/89 operating manual lists 9 mph (14.5 km/h) unloaded and
  4 mph (6.4 km/h) fully loaded. Those limits are represented separately in
  `AP88.tug/info.cfg`.
- Goldhofer's AST-1X data sheet lists up to 32 km/h and 212 kN tractive force.
  The configuration uses both manufacturer values.
- A Goldhofer airport-technology brochure lists the AST-3F family as capable
  of up to 32 km/h, so its unloaded forward speed uses that figure.

Sources:

- LEKTRO AP88/89 operation manual:
  https://www.hernandezanthony.com/samples/AnthonyHernandez_techwriting_lektro_1.pdf
- Goldhofer AST-1X technical data:
  https://www.goldhofer.com/fileadmin/downloads/BRO_AT/Data_Sheets/DSH_AST-1X_EN-met_2023-09.pdf
- Goldhofer airport-technology brochure:
  https://terbergaviation.co.uk/wp-content/uploads/2019/05/Goldhofer-Airport-Technology.pdf

## Further calibration

- Obtain a primary AST-3F drawbar-pull or tractive-effort specification. The
  pre-existing 480 kN setting remains unchanged until it can be distinguished
  from aircraft weight and nose-landing-gear load ratings.
- Record X-Plane telemetry for each tug and representative aircraft classes.
  Use those runs to tune `max_accel` and `max_decel` per tug without confusing
  simulator tire friction with vehicle capability.
- If exterior swept-envelope collision checking is added, store that radius as
  a separate configuration value from the fixed-axle path radius.

## Phase 2: startup acceleration

The first instrumented AST-3F/B737 test recorded an average acceleration of
0.796 m/s² during the first 0.105 seconds, despite the normal configured limit
of 0.25 m/s². The cause was a legacy breakaway workaround that multiplied the
acceleration command by 100 below 0.09 m/s.

Phase 2 removed that multiplier and initially raised the acceleration command
from zero to 0.25 m/s² over two seconds. In the follow-up run, first-interval
acceleration fell from 0.796 to 0.012 m/s², peak early force fell from 37.8 kN
to 23.0 kN, and the time to 1.0 m/s increased from 3.87 to 5.17 seconds.

Driver feedback rated the new breakaway much better but the remaining run-up
slightly quick. Phase 3 therefore extends the command rise to three seconds,
bounding command jerk to 0.0833 m/s³. The 0.25 m/s² maximum, route speed, and
steering logic remain unchanged for another isolated comparison. The ramp
resets after a brake interruption, light-warning pause, or direction change.

The same recording also exposed an uninitialized deceleration result in the
final crawl-speed branch. That branch now always returns a valid Boolean state.

The Phase 3 test reached 0.1 m/s in 1.53 seconds, 0.5 m/s in 3.50 seconds,
1.0 m/s in 5.73 seconds, and 95 percent of the 1.11 m/s target in 6.38
seconds. Peak force during the first two seconds fell to 19.1 kN and the
steady push speed remained unchanged. Driver feedback rated the initial push
as feeling right, so the three-second ramp is now the locked startup baseline.

## Phase 4: turn-exit steering unwind

The Phase 3 telemetry separated route geometry from steering transition
behavior. On the 33.38 m planned turn, the aircraft crossed the segment exit
only 0.005 degrees from the requested heading. The nosewheel was still
deflected 23.66 degrees, however, so the aircraft continued turning to a 7.23
degree overshoot before the straight-line controller reversed the steering.

Phase 4 starts neutralizing the articulation before a turn followed by a
same-direction straight. The lead distance is calculated from current speed,
current nosewheel angle, a measured 10 degree/second neutralization rate, and
a 0.35 m margin, clamped to 1.5--4.0 m. Once active, neutral steering is held
across the segment boundary until nosewheel deflection falls below 2.5
degrees. Only then can the straight-line controller make a correction. This
is intended to trade the large overshoot-and-reversal maneuver for a smooth
unwind followed, if necessary, by one small line correction.

The live test rejected this design. It changed the request from -23.5 degrees
to zero in one frame. Tug steering then moved from about -2.4 to -35.7 degrees
in 0.7 seconds, while aircraft yaw rate jumped from about 1.9 to 6.4
degrees/second. The aircraft first missed the requested heading by 6.7 degrees
and then crossed to a 9.3 degree overshoot, a nearly 16 degree reversal. The
Phase 4 build was removed from the installed plugin and is not a calibration
baseline.

## Phase 5: continuous turn profile

Phase 5 does not use the legacy line-chasing steering output during an
automatic push. Straight segments request zero steering. A turn uses one
feed-forward curvature profile with symmetric 10 m smootherstep transitions:
curvature begins at zero, rises continuously to a plateau, and returns to zero
with zero slope at both segment boundaries. Plateau curvature is increased by
the exact area lost in the entry and exit transitions, preserving the planned
total heading change. The final steering request is additionally limited to 6
degrees/second.

This removes the discrete turn-to-straight command change and prevents the
straight controller from chasing the line with alternating corrections. A
small final position or heading miss is accepted in preference to an
overshoot-and-countersteer maneuver.

## Phase 6: tail-aware endpoint capture

The Phase 5 live test was completely smooth, but its open-loop assumptions
were not position-convergent. The turn profile reached neutral steering about
9.4 degrees short of the requested heading, then traveled another 6.5 m before
the generic route logic changed segments. The final straight retained zero
steering, allowing lateral error to grow to roughly 10.4 m.

Phase 6 keeps the Phase 5 profile as feed-forward steering and adds bounded
closed-loop feedback from the aircraft tail. Straight segments track the tail
against the segment centerline. During a turn, the feedback weight rises with
turn progress and targets the next user-placed aircraft pose. Tail cross-track
and aircraft heading errors contribute no more than 8 degrees of correction,
and the combined request remains subject to the proven 6 degree/second limit.

Telemetry schema 5 records the planner endpoint, main-gear and tail positions,
target tail position, tail cross-track and along-track errors, heading error,
feedback weight, feedback steering, combined target, and applied request. This
allows the next live run to distinguish path geometry from X-Plane's actual
tug/nosewheel response without reconstructing the endpoint after the fact.

## Phase 6.1: aircraft systems safety

The first Phase 6 live run produced a smooth, nearly exact push, but the
aircraft lost electrical power and its landing gear retracted for the duration
of the push. The X-Plane log showed that BetterPushback explicitly disabled
`zibomod.by.Zibo` at push start and re-enabled it at push completion. A stale
per-aircraft preference had selected Zibo through the fork's experimental
"ACF Plugin Exclusion" feature.

Aircraft-local plugins commonly own electrical, hydraulic, landing-gear and
flight-control state, so suspending one during aircraft motion is unsafe. This
fork no longer disables any aircraft-local plugin during pushback. The
experimental aircraft-plugin exclusion control has been removed from the
preferences window, and stale saved selections are ignored with a warning.
The separate resource-plugin exclusion used for conflicting eye trackers is
unchanged.

## Phase 7: continuous planner-route fidelity

The Phase 6 compound-route test was smooth and stopped 0.259 m (0.85 ft) from
the selected main-gear endpoint, but endpoint accuracy concealed a clearance
problem. Reconstructing each straight from the planner data showed that the
aircraft reference point ran 2.805 m off the first intermediate straight and
7.329 m (24.0 ft) off the straight following the tight center turn. The
open-loop turn profile and endpoint-only tail capture could therefore cut well
inside the route drawn on the planning screen.

Phase 7 keeps the accepted feed-forward curve, tail endpoint capture, and
6-degree/second steering slew. The legacy route controller is used only as an
error sensor. Its difference from the feed-forward command passes through a
1-degree deadband and is capped at 12 degrees before being added to the smooth
target. This prevents the legacy controller's frequent 50-degree saturated
requests from taking control or causing abrupt countersteering. On the final
segment, route correction fades smoothly over the last 8 meters so the proven
tail/heading capture controls the exact stop.

Telemetry schema 6 records the closest point on the current straight or arc,
signed cross-track error, route-heading error, bounded path correction, and
terminal handoff weight. The next repeated compound-route test can therefore
compare maximum and 95th-percentile corridor error against Phase 6 while also
checking steering slew, reversal count, and final placement.

The Phase 7 test was smooth and stopped only 0.022 m (0.86 in) from the
selected main-gear endpoint with 0.31 degrees of heading error. Straight-route
cross-track peaked at 2.02 m, but the smooth path still differed from the
planner's ideal circular arcs by as much as 5.20 m. The path-correction command
was at its 12-degree limit for 46 percent of the push. Raising that limit would
risk returning to line-chasing and countersteering, so Phase 7 is retained as
the accepted motion-controller baseline.

## Phase 8: controller-matched planning preview

The planning screen previously drew geometric straight lines and
constant-radius arcs. Those shapes described the route generator, not the
motion controller, and therefore understated the space used by smooth
steering transitions and endpoint capture.

Phase 8 leaves the accepted pushback controller unchanged. For display only,
the planner copies the committed and cursor-preview segments, runs them through
the same turn profile, bounded route correction, tail capture, 6-degree/second
steering slew, and vehicle acceleration/deceleration limits, then advances a
rear-axle bicycle model at 0.1-second intervals. The blue line now shows that
predicted main-gear trajectory. A smooth translucent magenta band spans one
aircraft wingspan around that blue line. Segment rectangles and round turn
joins are unioned in the stencil buffer, then each covered pixel is shaded
exactly once. This prevents overlapping geometry from exposing triangle fans,
dark seams, contour loops, or intermediate span-wise lines. Samples are drawn
at roughly 1-meter spacing. If a route is invalid or too long to predict
completely, the planner safely falls back to the original geometric renderer.

This is a trajectory and wing-clearance preview, not scenery collision
detection. It does not yet identify buildings, parked aircraft, vehicles,
taxiway restrictions, runway flow, or ATC constraints. Those belong to a
later airport-aware planning phase.
