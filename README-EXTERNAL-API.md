# BetterPushback external interface

Other plugins (ATC and crew-voice add-ons, copilot tools, cockpit-sharing
tools) can follow and drive a BetterPushback operation through datarefs and
commands. This file describes that interface: what is published, what each
value means, and the rules that keep it stable for the plugins that use it.

The interface is additive. Nothing here changes how BetterPushback behaves for
a pilot who uses it on its own.

## Conventions

- **Feature detection.** Read `bp/api_version` first. It is raised whenever
  the interface gains something and never lowered; a plugin that needs a
  feature checks for the version that introduced it. If the dataref does not
  exist, the installed BetterPushback predates the interface.
- **Stable numbers.** Every number published here (steps, stages, actions)
  keeps its meaning forever. New values are added at the end; none are reused
  or reordered, even if BetterPushback's internal enums change.
- **Names next to numbers.** Each number has a `*_name` byte dataref holding a
  lower-case machine name (`pushing`, `call_tug`). Names are never translated
  and are meant for logs and debugging; decide on the numbers.
- **Change detection.** `bp/state_seq` changes whenever any value of the
  operation state does. Poll it (it is cheap) and read the rest only when it
  moved.
- **Refresh rate.** The published state is refreshed about ten times a second,
  whether or not the Ground Operations panel is visible. It is the same state
  the panel draws, so the two always agree.
- **Byte datarefs** hold a zero-terminated string; read them with
  `XPLMGetDatab` into a buffer of at least 32 bytes.

## Version 1: operation state

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/api_version` | int | Interface version (this document: 5) |
| `bp/state_seq` | int | Changes whenever a value below changes |
| `bp/started` | int | An operation is running (existing dataref) |
| `bp/emergency_tow` | int | The running operation is the Emergency Tow |
| `bp/step` | int | Controller step, see the table below |
| `bp/step_name` | byte[32] | Its machine name |
| `bp/stage` | int | Ground Operations stage: 0 tug, 1 connect, 2 comms, 3 push, 4 clear; -1 when no operation is running |
| `bp/stage_name` | byte[32] | `tug`, `connect`, `comms`, `push`, `clear` or `idle` |
| `bp/action` | int | What the pilot must do next, see below; 0 when nothing is required |
| `bp/action_name` | byte[32] | Its machine name |
| `bp/paused` | int | A pause is requested or held |

Also, outside `bp/state_seq` (they change continuously or never):

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/push_speed` | float | The aircraft's speed during the operation, m/s; -1 when no operation is running |
| `bp/push_distance_remaining` | float | Metres of the route left while pushing or towing; -1 otherwise |
| `bp/plugin_version` | byte[32] | BetterPushback's version (its release tag, e.g. `v1.15`) |

The speed and distance are the figures the Ground Operations panel shows.
The existing datarefs `bp/connected`, `bp/plan_complete`, `bp/planner_open`,
`bp/op_complete` and `bp/tug_name` keep their meaning.

### Steps (`bp/step`)

| Value | Name | | Value | Name |
| --- | --- | --- | --- | --- |
| 0 | `off` | | 12 | `pushing` |
| 1 | `tug_load` | | 13 | `stopping` |
| 2 | `start` | | 14 | `stopped` |
| 3 | `driving_up_close` | | 15 | `lowering` |
| 4 | `waiting_for_doors` | | 16 | `ungrabbing` |
| 5 | `opening_cradle` | | 17 | `waiting_to_disconnect` |
| 6 | `waiting_for_parking_brake` | | 18 | `moving_away` |
| 7 | `driving_up_connect` | | 19 | `closing_cradle` |
| 8 | `grabbing` | | 20 | `starting_to_clear` |
| 9 | `lifting` | | 21 | `moving_to_clear` |
| 10 | `connected` | | 22 | `clear_signal` |
| 11 | `starting` | | 23 | `driving_away` |

### Pilot actions (`bp/action`)

The action the Ground Operations panel shows as required, with the command
that performs it.

| Value | Name | Command |
| --- | --- | --- |
| 0 | `none` | |
| 1 | `call_tug` | `BetterPushback/connect_first` |
| 2 | `call_emergency_tow` | `BetterPushback/call_emergency_tow` |
| 3 | `open_planner` | `BetterPushback/start` |
| 4 | `change_plan` | `BetterPushback/start` (during the connected hold) |
| 5 | `pause` | `BetterPushback/pause_resume` |
| 6 | `resume` | `BetterPushback/pause_resume` |
| 7 | `disconnect_tug` | `BetterPushback/disconnect` |
| 8 | `reconnect_tug` | `BetterPushback/reconnect` |
| 9 | `end_disconnect` | `BetterPushback/stop` (the panel asks for confirmation first) |
| 10 | `acknowledge_clear` | `BetterPushback/acknowledge_clear` |

Actions that are a brake setting rather than a button (set or release the
parking brake) are not listed: follow `bp/step` (6 `waiting_for_parking_brake`,
10 `connected`) for those.

### Example: following an operation

```c
static XPLMDataRef seq_dr, step_dr, step_name_dr;
static int last_seq = -1;

void init(void) {
    XPLMDataRef ver = XPLMFindDataRef("bp/api_version");
    if (ver == NULL || XPLMGetDatai(ver) < 1)
        return;                         /* no interface: fall back */
    seq_dr = XPLMFindDataRef("bp/state_seq");
    step_dr = XPLMFindDataRef("bp/step");
    step_name_dr = XPLMFindDataRef("bp/step_name");
}

void poll(void) {                       /* from a flight loop */
    char name[32] = "";
    int seq = XPLMGetDatai(seq_dr);
    if (seq == last_seq)
        return;
    last_seq = seq;
    XPLMGetDatab(step_name_dr, name, 0, sizeof (name) - 1);
    printf("pushback step %d (%s)\n", XPLMGetDatai(step_dr), name);
}
```

## Version 2: crew lines

Every ground-crew line BetterPushback starts is published as text, so another
plugin can show it, log it or (with a later part of the interface) speak it.

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/msg_seq` | int | BetterPushback's line counter: goes up by one each time a line starts (a reader polling slowly may see it jump by two); 0 before the first |
| `bp/msg` | int | The line last started, see below; 0 before the first |
| `bp/msg_key` | byte[32] | Its key, e.g. `driving_up` (the voice file's name) |
| `bp/msg_text` | byte[192] | The nominal English sentence, e.g. "Ground to cockpit. Tow is driving up." |
| `bp/msg_caption` | byte[96] | The Ground Operations caption, in BetterPushback's language, e.g. "Ground crew: Tug approaching." |
| `bp/msg_voice` | byte[64] | The voice pack speaking, e.g. `en_GB` or `de`; its language is the part before `_`, `-` or `(` |
| `bp/msg_duration` | float | Length of the line's recording, seconds |
| `bp/msg_playing` | int | The line is being spoken now |

A line is published when it starts, even with the simulator's sound off.
`bp/msg_seq` moves once per line; read the other values when it changes. The
text is the nominal wording: the recorded voices, especially the other
languages, may say it slightly differently.

| Value | Key | Text |
| --- | --- | --- |
| 1 | `plan_start` | Ground to cockpit. Please show me where you want to go. |
| 2 | `plan_end` | Ground to cockpit. Plan acknowledged, call me through the menu when you are ready. |
| 3 | `driving_up` | Ground to cockpit. Tow is driving up. |
| 4 | `ready2conn` | Ok, all doors and hatches are closed, ready to connect. Set parking brake. |
| 5 | `ready2conn_nopark` | Ok, all doors and hatches are closed, ready to connect. (the brake is already set) |
| 6 | `winch` | Winching strap and adapter in position. Release parking brake when ready to start pushback. |
| 7 | `connected` | Tow connected and bypass pin inserted. Release parking brake. |
| 8 | `start_pb` | Starting pushback and you may start engines. |
| 9 | `start_tow` | Starting tow and you may start engines. |
| 10 | `start_pb_nostart` | Starting pushback. (engines cannot or should not be started now) |
| 11 | `start_tow_nostart` | Starting tow. (likewise) |
| 12 | `op_complete` | Operation complete, set parking brake. |
| 13 | `disco` | Disconnecting tow. Stand by. |
| 14 | `done_right` | Tow is disconnected and bypass pin has been removed, hand signal on the right, we'll see you next time and have a safe flight. |
| 15 | `done_left` | The same, hand signal on the left. |

Some messages are not recordings: BetterPushback says them through X-Plane's
speech. They are published the same way, with `bp/msg_text` and
`bp/msg_caption` holding the text as spoken (in BetterPushback's language),
`bp/msg_voice` = `xplane`, and `bp/msg_duration` an estimate (X-Plane does not
report how long it speaks).

| Value | Key | What |
| --- | --- | --- |
| 16 | `doors_gpu_open` | "Some doors are still opened or the GPU or the ASU are still connected. I'm waiting for all of them closed and disconnected then I will proceed." |
| 17 | `lights_warning` | "Hey! Quit blinding me with your landing lights! Turn them off!" (or taxi lights), during the push |
| 18 | `system` | A failure or warning about BetterPushback itself, e.g. "Pushback failure: no suitable tug for your aircraft." |

## Version 3: blockers

Why a running operation is waiting, so another plugin can say so ("we can't
connect, the GPU is still on") instead of the pilot guessing. These values are
part of the operation state: `bp/state_seq` moves when they change.

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/blocker` | int | What the operation waits for, see below; 0 when nothing |
| `bp/blocker_name` | byte[32] | Its machine name |
| `bp/blocker_item` | byte[64] | For `aircraft_not_ready`: the door-check dataref still open, from `BetterPushback_doors.cfg` (e.g. `laminar/B738/gpu_available`); empty otherwise |
| `bp/blocker_item_kind` | byte[32] | What that dataref stands for, from its name: `door`, `cargo_door`, `gpu`, `asu` or `other` |
| `bp/status_text` | byte[96] | BetterPushback's status line for the current step, in its language (e.g. "Waiting for the parking brakes release") |

| Value | Name | The operation waits until |
| --- | --- | --- |
| 0 | `none` | Nothing is blocking |
| 1 | `aircraft_not_ready` | Every configured door is closed and the GPU/ASU are disconnected |
| 2 | `set_parking_brake` | The parking brake is set (before connecting, or after the push) |
| 3 | `release_parking_brake` | The parking brake is released (connected, ready to push) |
| 4 | `plan_required` | A push route is planned |

The item kind is a best guess from the dataref's name; aircraft without a
`BetterPushback_doors.cfg` entry report no item.

## Version 4: external voice

Another plugin can speak the ground-crew lines itself (its own voices, the
pilot's language, its own audio routing) while BetterPushback stays silent.
The operation still waits for each line to be spoken before moving on, as it
does with its own recordings, through a small handshake.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/voice_mode` | int | the other plugin | 0: BetterPushback speaks (default); 1: the other plugin speaks |
| `bp/voice_heartbeat` | int | the other plugin | Count it up at least once a second while you are ready to speak |
| `bp/voice_done_seq` | int | the other plugin | After you finish speaking a line, write its `bp/msg_seq` here |
| `bp/voice_external` | int | BetterPushback | 1 while lines are being handed to the other plugin |

How it works:

- Lines are handed over while `bp/voice_mode` is 1 **and** the heartbeat has
  moved in the last 3 seconds (`bp/voice_external` says which). A line handed
  over is not played by BetterPushback; it is published as usual
  (`bp/msg_seq`, `bp/msg_text`, ...) for the other plugin to speak. The same
  goes for the messages BetterPushback would say through X-Plane's speech
  (lines 16 to 18): published, not spoken. The operation never waits for
  those, so reporting them in `bp/voice_done_seq` is optional.
- Where the operation waits for a line to finish (before connecting, before
  starting the push after "Release parking brake", before the tug lowers and
  before it drives clear), it waits until `bp/voice_done_seq` reaches that
  line's `bp/msg_seq`: the line lasts as long as the other plugin takes to say
  it, longer or shorter than the recording.
- Safeguards: a line is never waited for longer than its recording plus 8
  seconds. If the heartbeat stops, BetterPushback stops waiting (back to the
  recording's length), speaks the following lines itself and logs the change.
  The handshake only delays the next step of the normal sequence; Stop, Pause
  and End act at once, as before.
- `bp/msg_playing` follows the other plugin's speech for lines it speaks.
- `bp/ground_crew_audio_volume` still sets the volume of BetterPushback's own
  recordings.

```c
/* In a flight loop, while voice_mode is 1: */
XPLMSetDatai(heartbeat_dr, ++heartbeat);
int seq = XPLMGetDatai(msg_seq_dr);
if (seq != last_seq && XPLMGetDatai(voice_external_dr)) {
    last_seq = seq;
    XPLMGetDatab(msg_text_dr, text, 0, sizeof (text) - 1);
    start_speaking(text, seq);          /* your TTS or recordings */
}
/* ... and when the line for `finished_seq` has been spoken: */
XPLMSetDatai(voice_done_seq_dr, finished_seq);
```

## Version 5: external routes

In real operations the push is planned on the ground: each stand has its push
procedure, and the ground controller may name the direction ("push-back
approved, facing west"). Another plugin that knows these (an ATC add-on, a
ground-handling add-on) can supply the route, and BetterPushback does what it
is good at: it fits the legs to the aircraft and drives them.

A route is text: a header, then one line per position the aircraft should
reach, in order.

```
BPROUTE 1
# push back onto taxiway B facing west
P 47.79312345 12.99751234 180.0
P 47.79240000 12.99690000 270.0
```

- Each `P` line is `latitude longitude heading`: the aircraft's reference
  point (the position X-Plane reports for the aircraft) and its true heading
  there. Up to 16 positions; lines starting with `#` are comments.
- BetterPushback fits each leg from the previous position (the first from
  where the aircraft stands) within the aircraft's turning limits: a position
  behind the aircraft is reached by pushing, one ahead by towing.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/route_in` | byte[2048], writable | Write the route text here, zero-terminated |
| `BetterPushback/load_route` | command | Load `bp/route_in` as the current route |
| `BetterPushback/check_route` | command | Check whether `bp/route_in` fits, changing nothing (see below) |
| `BetterPushback/clear_route` | command | Remove the current route |
| `bp/route_seq` | int | Goes up by one with every load, check or clear, accepted or not |
| `bp/route_status` | int | Answer to the last load, check or clear: 1 accepted (fits), 2 rejected (0 before any) |
| `bp/route_reason` | byte[128] | Why it was rejected, e.g. "position 2 cannot be reached from the one before it within this aircraft's turning limits" |
| `bp/route_current` | byte[2048] | The current route, in the same text form (empty when there is none) |
| `bp/route_source` | int | Where the current route came from: 0 none, 1 planner (drawn, changed or accepted there), 2 saved slot (taken unchanged), 3 external (`load_route`) |
| `bp/route_source_name` | byte[16] | `none`, `planner`, `saved` or `external` |

When a route is accepted:

- A route can be loaded or cleared **before the tug is called** (the classic
  plan-first flow), **while the connected tug waits for a plan**
  (`bp/blocker` = `plan_required`), and **during the connected hold** with the
  parking brake set (as *Change plan* in the panel). It is refused while the
  planner is open, during a manual push, in shared-cockpit slave mode, and
  once the tug is connecting or the push is under way.
- *Call tug* (`BetterPushback/connect_first`) starts every operation with an
  empty route, so in that workflow load the route once `bp/blocker` reads
  `plan_required`: the operation continues on its own, as when the pilot
  accepts a plan in the planner.
- A rejected route leaves the current route unchanged.
- `BetterPushback/check_route` fits the route exactly as `load_route` would,
  from where the aircraft stands now, and answers the same way (1 fits, 2 does
  not, with the reason), but changes nothing and works at any time. An ATC
  plugin can so check that a push direction suits the aircraft before it
  approves it, and before the tug is called.

`bp/route_current` lists each leg's end: the positions given to `load_route`,
or the points clicked in the planner, with `push` or `tow` after each. It can
be fed back to `load_route` unchanged. `bp/route_source` tells a plugin
whether the route is still the one it loaded: it changes to `planner` or
`saved` as soon as the pilot replaces or edits it, and to `none` when it is
cleared.

```c
XPLMSetDatab(route_in_dr, (void *)text, 0, (int)strlen(text) + 1);
int before = XPLMGetDatai(route_seq_dr);
XPLMCommandOnce(XPLMFindCommand("BetterPushback/load_route"));
/* The answer is ready when the command returns: */
if (XPLMGetDatai(route_seq_dr) != before && XPLMGetDatai(route_status_dr) == 2)
    XPLMGetDatab(route_reason_dr, reason, 0, sizeof (reason) - 1);
```

## Roadmap

The next part of the interface, a separate change raising `bp/api_version`:

1. **Saved routes** - commands to save the current route to a stand's slot,
   and to load or clear one.

## Testing

All run as part of `tests/run_all_tests.sh`:

- `tests/run_ext_api_state_tests.sh` checks that every controller step and
  Ground Operations action has a unique, fixed public number and name, and
  that the published state follows the controller through idle, pushing,
  pause and the Emergency Tow, and that blockers are published with the open
  item and its kind only while an operation runs.
- `tests/run_ext_api_msgs_tests.sh` checks that every crew line has a unique,
  fixed number, key, text and caption, and that the brake-set and
  no-engine-start variants leave out the part that does not apply, and that
  the X-Plane speech lines (16 to 18) follow the recordings.
- `tests/run_ext_api_voice_tests.sh` checks the external-voice timing: an
  external line lasts until it is reported finished, never longer than its
  recording plus the grace, and falls back to the recording when the
  heartbeat stops.
- `tests/run_ext_api_route_tests.sh` checks reading and writing route text:
  comments, line endings, every kind of bad input refused with a reason that
  names the line, the position limit, and that a written route reads back;
  and the rule for when a route may change (before an operation, while the
  tug waits for a plan, during the connected hold; never with the planner
  open, during a manual push or in slave mode).
