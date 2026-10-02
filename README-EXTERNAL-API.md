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
| `bp/api_version` | int | Interface version (this document: 1) |
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

## Roadmap

Later parts of the interface, each a separate change and each raising
`bp/api_version`:

1. **Crew lines** - the text of every ground-crew line as it is spoken (full
   sentence and translated caption), with a sequence number, so another plugin
   can show or log it.
2. **Blockers** - why the operation is waiting (a door open, a GPU
   connected, the parking brake), as text and a number.
3. **External voice** - a mode in which BetterPushback stays silent and another
   plugin speaks the crew lines, with a handshake so the operation waits for
   that plugin's line to finish (time-limited, never blocking a stop).
4. **External routes** - another plugin supplies the push route as positions
   and headings; BetterPushback fits it to the aircraft, accepts or rejects it
   with a reason, and publishes the active route in the same format.
5. **Saved routes** - commands to save the active route to a stand's slot, and
   to load or clear one.

## Testing

`tests/run_ext_api_state_tests.sh` (part of `tests/run_all_tests.sh`) checks
that every controller step and Ground Operations action has a unique, fixed
public number and name, and that the published state follows the controller
through idle, pushing, pause and the Emergency Tow.
