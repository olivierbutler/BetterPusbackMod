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
| `bp/api_version` | int | Interface version (this document: 3) |
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

## Roadmap

Later parts of the interface, each a separate change and each raising
`bp/api_version`:

1. **External voice** - a mode in which BetterPushback stays silent and another
   plugin speaks the crew lines, with a handshake so the operation waits for
   that plugin's line to finish (time-limited, never blocking a stop).
2. **External routes** - another plugin supplies the push route as positions
   and headings; BetterPushback fits it to the aircraft, accepts or rejects it
   with a reason, and publishes the active route in the same format.
3. **Saved routes** - commands to save the active route to a stand's slot, and
   to load or clear one.

## Testing

Both run as part of `tests/run_all_tests.sh`:

- `tests/run_ext_api_state_tests.sh` checks that every controller step and
  Ground Operations action has a unique, fixed public number and name, and
  that the published state follows the controller through idle, pushing,
  pause and the Emergency Tow, and that blockers are published with the open
  item and its kind only while an operation runs.
- `tests/run_ext_api_msgs_tests.sh` checks that every crew line has a unique,
  fixed number, key, text and caption, and that the brake-set and
  no-engine-start variants leave out the part that does not apply, and that
  the X-Plane speech lines (16 to 18) follow the recordings.
