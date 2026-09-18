# BetterPushback User Guide

BetterPushback provides a pilot-guided pushback and towing workflow for
X-Plane 11 and 12. The Ground Operations window follows the tug from dispatch
through connection, pushback, disconnect, and final clearance while preserving
BetterPushback's established route planning and steering behavior.

This guide describes the owner-review release candidate. Availability on a
specific operating system or X-Plane version depends on the final release
package and its published compatibility notes.

## 1. Install and open

1. Close X-Plane before replacing an existing plugin installation.
2. Back up the current `BetterPushback` plugin folder and any saved route data.
3. Copy the complete release `BetterPushback` folder into
   `X-Plane/Resources/plugins`.
4. Start X-Plane, load the aircraft at a stand, and use the X-Plane Plugins menu
   or `BetterPushback/ground_ops_show_hide` command to show Ground Operations.

Do not copy only the `.xpl` binary. Tug objects, sounds, configuration files,
the wing walker, and supporting resources are part of the plugin.

## 2. Read the progress rail

Ground Operations uses five stages:

| Stage | Meaning |
| --- | --- |
| Tug | Tug selection, dispatch, approach, and service-area checks |
| Connect | Final approach, nose-gear capture, and lift |
| Comms | Route planning and brake-release coordination |
| Push | Pushback or tow movement, Pause, Resume, and stopping |
| Clear | Lowering, disconnect, tug clearance, and the final hand signal |

The node colors are operational cues:

| Appearance | Meaning | Pilot response |
| --- | --- | --- |
| Bright green with a strong ring | The current stage is progressing normally | Monitor the operation |
| Amber/orange | Progress is blocked on a required pilot action | Expand the panel and follow the PILOT ACTION card |
| Green | The stage is complete | No action required |
| Cyan outline | The stage is still in the future | No action required |

Red is not used for normal operation. It is reserved for a future explicit
fault or safety-stop condition. A destructive alternative such as **End
operation** does not make the stage red.

Color is never the only status indicator. The stage name, status, CURRENT TASK
or PILOT ACTION heading, task wording, and button label describe what is
happening.

## 3. Window modes

- **Compact rail:** Shows only the five stages. Click the rail to expand it.
  Dragging moves it without expanding. Compact mode intentionally has no
  tooltips.
- **Expanded panel:** Shows airport and weather context, current status, task,
  speed, remaining distance, captions, and available controls.
- **Collapsed:** Click the minus control, hold the pointer over it for about one
  second, or click the expanded stage rail.
- **Pop-out:** Use the pop-out control to move Ground Operations into a native
  operating-system window, including another display.
- **Hidden:** Use the X control or the show/hide command. Hiding the window does
  not cancel the operation.

Positions are remembered by window mode, monitor, and side. If a saved display
is removed or resized, the window is recovered into a visible area.

## 4. Normal pushback

### Call the tug

The operation begins at an amber/orange **Tug** node and a **Call tug** PILOT
ACTION. Nothing is dispatched automatically. Press **Call tug** when the
aircraft and ramp are ready.

After the call is accepted, the Tug node changes to bright green. Tug selection,
dispatch, approach, and connection preparation proceed automatically. The panel
explains any door, GPU, ASU, or parking-brake condition that prevents progress.

### Plan the push

After the tug captures the nose gear, it stops before lifting. The **Comms** node
turns amber/orange and the panel presents **Plan push**. Open the overhead
planner, place the route manually, and accept it.

The blue line is the planned nosewheel trajectory. The translucent magenta band
is a wingspan-wide visual-clearance aid. It is not automatic collision
detection; the pilot remains responsible for verifying the route and clearance.

### Release the parking brake

When the tug is connected and the route is ready, the panel displays the next
required brake action. The Comms node remains amber/orange until the required
condition is satisfied. A **Change plan** control may be available while the
aircraft is connected and held by the parking brake; it is an optional control,
not the required action itself.

### Monitor, pause, or end

During a normal push the **Push** node is bright green. **Pause push** requests a
controlled deceleration and retains the accepted route and steering state.
After the tug reaches a stationary pause, **Resume push** becomes a required
pilot action and the Push node is amber/orange.

**End operation** is different from Pause. It discards the remaining route and
continues through a safe stop and disconnect sequence at the current position.
The confirmation step prevents accidental activation.

### Disconnect and clear

At the route end, set the parking brake when requested. The tug lowers and
releases the nose gear. At **Ready to disconnect**, choose **Disconnect tug** to
continue or **Reconnect** if the connection must be restored.

The wing walker presents STOP, STANDBY, and CLEAR signals during a normal
operation. When the clear signal is visible, **Acknowledge** it in the panel.
Acknowledgement does not shorten the established minimum signal-display time.
After the tug departs, every stage is green and the operation is complete.

## 5. Saved routes

At a published airport start, BetterPushback can identify a gate or stand only
when the live nosewheel position and heading uniquely match it within the
plugin's strict tolerance. A matching airport, stand, and compatible aircraft
profile can have two independent route slots.

- Select an existing slot to reuse a route.
- Select an explicit slot when replacing a saved route.
- Accepting an unchanged recalled route does not rewrite it.
- Arbitrary-position, off-anchor, saved-situation, and Emergency Tow routes are
  session-only and cannot list, load, replace, or save persistent slots.

If no unique published start is recognized, plan the route normally for the
current operation. The panel and log explain why persistent saving is disabled.

## 6. Emergency Tow

After a completed normal operation, **Call tow back** starts a guarded one-time
Emergency Tow. The tug returns, reconnects, and opens the manual planner at the
aircraft's live position.

Emergency Tow uses no saved routes and does not display the wing walker. Pause,
Resume, and End operation remain available. After final disconnect and tug
departure, BetterPushback returns to its normal cold-start state.

## 7. Commands and recovery

The most useful assignable commands are:

| Command | Purpose |
| --- | --- |
| `BetterPushback/ground_ops_show_hide` | Show or hide Ground Operations |
| `BetterPushback/ground_ops_expand_collapse` | Switch between compact and expanded views |
| `BetterPushback/start` | Start pushback or reopen Change plan during the connected hold |
| `BetterPushback/pause_resume` | Pause or resume without discarding the route |
| `BetterPushback/stop` | End pushback and disconnect |
| `BetterPushback/start_planner` | Open the manual planner |
| `BetterPushback/stop_planner` | Close the planner |
| `BetterPushback/acknowledge_clear` | Acknowledge the final clear signal |

Classic menu commands remain available as a recovery path. If the Ground
Operations window cannot initialize, the operational commands still remain
available.

## 8. Offline behavior, privacy, and diagnostics

The normal pushback workflow uses simulator-local aircraft, airport, and weather
context and remains usable offline. Ground Operations shows the source and
freshness of the displayed context.

Runtime telemetry recording is disabled in normal release builds. Diagnostic
builds can enable bounded per-operation CSV telemetry for troubleshooting. Such
files remain local unless the user deliberately shares them.

When reporting a problem, include the X-Plane version, operating system,
aircraft, airport and stand, display arrangement, the exact workflow step, and
the relevant `Log.txt`. Keep a backup of the previous working plugin until the
new release has passed a complete local pushback.

## 9. Current release scope

The current owner-review candidate includes:

- compact and expanded Ground Operations presentations, including pop-out and
  remembered multi-monitor placement;
- explicit Call tug, Plan push, brake, disconnect, and clear-signal gates;
- manual route planning with the legacy route-following behavior;
- two guarded saved-route slots per matching gate/stand and aircraft profile;
- Pause/Resume and safe End operation behavior;
- normal-operation wing-walker signals and one-time Emergency Tow;
- simulator-local flight and weather context;
- default-off runtime telemetry and classic-command recovery paths.

Release acceptance still depends on the owner's final review, supported-platform
builds, packaging checks, and live simulator validation.

## 10. Future roadmap

The following items are candidates for separate owner approval and validation,
not promises for the current release:

- user-selectable display language with an Automatic/X-Plane-language option,
  explicit overrides, English fallback, and locale-specific short rail labels;
- optional SDK 4.4 native ImGui rendering with an earlier-runtime fallback;
- a separately scoped projected-ImGui planner overlay prototype;
- broader Windows, macOS/Metal, Linux, X-Plane 11/12, multi-display, scaling,
  VR, and long-session validation;
- additional accessibility, recovery, and tug-driver presentation refinements
  supported by measurable acceptance criteria.

Operational behavior changes remain separate from renderer modernization so
each change can be reviewed, measured, and rolled back independently.

## 11. Safety notice and credits

BetterPushback is an entertainment add-on. It is not intended for flight
training, real-world aircraft operation, or safety-critical use.

Original Better Pushback by Saso Kiselkov. This maintained fork and its release
materials preserve the repository's existing copyright, license, attribution,
and third-party asset notices.
