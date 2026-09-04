# About BetterPushback Mod X-Plane 11/12

This is a pushback plugin for the X-Plane 11/12 flight simulator.
It provides an overhead view to plan a pushback route and
accomplishes a fully automated "hands-off" pushback, letting the user
focus on aircraft startup and other pilot duties during pushback. It can
of course also tow you forward, or perform any arbitrarily complicated
pushback operation. To increase immersion, it speaks to you in a variety
of languages and accents, simulating ground staff at various places
around the world.

In this fork, the planner and live tug use BetterPushback's original route
construction, segment steering, turn tracking, stopping, and approach
geometry. The planner's blue line therefore describes the legacy planned path.
A smooth, uniformly shaded magenta band marks a wingspan-wide visual-clearance
area around it, but does not perform automatic collision detection.

Development of the compact ground-operations experience is governed by the
tracked [UI design](GROUND_OPS_UI_DESIGN.md) and the fork's complete
[implementation roadmap](ROADMAP.md). These documents define the approved
five-stage compact rail, live workflow panel, performance guardrails,
phased delivery, and acceptance gates. Active crew audio prompts are mirrored
as panel captions. Build and simulator evidence for every phase is retained in the
[verification record](PHASE_TESTING.md).

The Ground Operations workflow first displays the parsed departure-airport
identifier and an amber **Call tug** action. There is no artificial timer or
automatic dispatch: connection begins only when the pilot presses the button.
The ground crew then performs approach and nose-gear capture automatically.
Capture ends at an amber **Plan push** gate with no lift performed; lift starts
only after the pilot opens the planner and accepts the push route.

The planner opens for manual route placement and does not generate a route from
wind or runway data. When the live nosewheel uniquely matches a published
`apt.dat` start within 1 metre and 1 degree, an accepted route can be stored by
airport, gate or stand, and compatible aircraft profile. Each matching profile
has two independent saved-route slots with explicit selection and replacement.
Saved-situation, arbitrary-position, off-anchor, and Emergency Tow routes remain
session-only and cannot load, replace, or save persistent routes.

Ground Operations can be shown as a compact five-orb stage rail, expanded into
the complete status and action panel, or popped out as a native X-Plane window
and moved to another monitor. During an automatic push, **Pause/Resume** retains
the accepted route and steering state, while **End operation** stops safely and
continues through the normal disconnect sequence at the current position.

After a completed normal operation, **Call tow back** starts a guarded one-time
Emergency Tow. The tug returns and connects, the manual planner opens at the live aircraft
position, and the plugin returns to its normal start state after towing and
disconnect are complete. Emergency Tow does not use the saved-route cache or
render the wing walker. Normal pushbacks retain the accepted STOP, STANDBY, and
CLEAR wing-walker sequence.

### About this Fork and Copyright

Better Pushback is developed by "Saso Kiselkov". So if you see this project or else, just contact me.
I just did it, to "keep it a life on X-Plane 12". I will always respect that this is your code,
and you are the father of this application. Hope you accept this as there was no answer from your side.

There is no idea to steal it from you. If you don't like this effort. Just say and I will stop it, no question.

Thanks

## Downloading BetterPushback

You can get the last binary release from here:

https://forums.x-plane.org/files/file/90556-better-pushback-for-x-plane-1112

Some Beta / Pre-Releases can be found here:

https://github.com/olivierbutler/BetterPusbackMod/releases

(Please be aware that pre-releases maybe still has some issues inside and maybe is not final tested.)


## Building BetterPushback

To build BetterPushback, check to see you have the pre-requisites installed. The
Linux and Windows versions are built in one step on an Ubuntu 16.04 (or
compatible) machine and the Mac version is obviously built on macOS (10.9
or later).

>Note: __on macOS only__ , by using the option ```-f```, the script will build also the linux and windows versions. see ```README-docker.md```. Use ```-m``` on a development build to emulate the enlarged macOS Ground Operations layout on Windows or Linux.

For the Linux and Mac build pre-requisites, see ```build_xpl.sh```

### Legacy operational UI compatibility

This fork uses the Ground Operations panel as its operational interface. The
original BetterPushback "magic squares" windows remain in the source tree but
are disabled by default so both interfaces are not displayed together. An
upstream maintainer can restore the original windows without reverting source
by configuring the build with:

```
cmake -DBP_ENABLE_LEGACY_MAGIC_SQUARES=ON ...
```

This switch controls only the four original shortcut windows. Ground
Operations remains enabled, and the overhead planner and classic menu commands
remain available. The end sequence disconnects automatically; the original
disconnect/reconnect window implementation is retained as dormant source and
is not created.

The global build script is located here and is called '```build_release```'.
Once you have the pre-requisite build packages installed, simply run:
***
```
$ ./build_release [-f]
```
This builds the dependencies and then proceeds to build BetterPushback for the appropriate target platforms. Please note that this builds a
stand-alone version of the plugin that is to be installed into the global
Resources/plugins directory in X-Plane.
***
```
$ ./build_xpl.sh [-f]
```
This builds only the `.xpl` files. The option described above can also be used.
***
```
$ ./install_xplane.sh
```
Copy the .xpl files to the x-plane and change the quarantine attribute of the ```mac.xpl``` file.
In the script, just set ```XPLANE_PLUGIN_DIR``` accordingly.
***

For details on how to add tug liveries, see
`objects/tugs/LIVERIES_HOWTO.txt`.

To add a voice set, see `data/msgs/README.txt` for the information.

## Running the regression tests

The focused regression matrix requires a POSIX shell, a C compiler, Python 3,
and the same sibling `libacfutils` dependency tree used by the plugin build.
Run all current suites with:

```
$ ./tests/run_all_tests.sh
```

## Commands

BetterPushback registers these X-Plane commands:

- `BetterPushback/start`: Start pushback (or reopen the planner as **Change
  plan** during the connected parking-brake hold).
- `BetterPushback/pause_resume`: Pause or resume automatic pushback
  without discarding the accepted route.
- `BetterPushback/stop`: End pushback and disconnect. This retains the legacy
  command path for compatibility but is presented distinctly from Pause.
- `BetterPushback/start_planner`: Open the pushback planner.
- `BetterPushback/stop_planner`: Close the pushback planner.
- `BetterPushback/connect_first`: Connect the tug before planning/pushback.
- `BetterPushback/call_emergency_tow`: Call the tug back after a completed
  operation for a one-time Emergency Tow.
- `BetterPushback/ground_ops_show_hide`: Show or hide Ground Operations.
- `BetterPushback/ground_ops_expand_collapse`: Expand or collapse Ground
  Operations.
- `BetterPushback/disconnect`: Internal compatibility command used by the
  automatic physical-disconnect sequence.
- `BetterPushback/cab_camera`: View from the tug cab.
- `BetterPushback/recreate_scenery_routes`: Recreate scenery routes from WED files.
- `BetterPushback/preference`: Open the preference window.
- `BetterPushback/reload`: Reload BetterPushback (disabled if pushback is running,
  the planner is open, or the preferences window is active).
- `BetterPushback/abort_push`: Abort pushback during coupled push.
- `BetterPushback/manual_push_left`: Turn the tug left (manual push).
- `BetterPushback/manual_push_right`: Turn the tug right (manual push).
- `BetterPushback/manual_push_toggle`: Toggle manual push direction.
- `BetterPushback/manual_push_start`: Start/pause manual push (yoke used).
- `BetterPushback/manual_push_start_no_yoke`: Start/pause manual push (no yoke).

Note: For coupling add-ons, only `BetterPushback/start` is intended to be
mirrored to the slave. Other commands remain local.

The Ground Operations **Call tug** button dispatches
`BetterPushback/connect_first` through the normal command handler.

### libacfutils Library Required

I removed from the project. It need to by downloaded separatly. To make sure you have a matched version, take the fork in my repository.
To connect with the library setup the Library in the "CMakeLists.txt" File in the "src" directory.

file(GLOB LIBACFUTILS "../../../libs/libacfutils")

As I found out in the last view days the relation to this library are very hard and many issues come from here ... it is not possible to splitup the library.

The library can be found here:
https://github.com/olivierbutler/libacfutils

### CREDIT

Original version by skiselkov: https://github.com/skiselkov/BetterPushbackC

### DISCLAIMER

BetterPushback is *NOT* meant for flight training or use in real avionics. Its
performance can seriously deviate from the real world system, so *DO NOT*
rely on it for anything critical. It was created solely for entertainment
use. This project has *no* ties to Honeywell or Laminar Research.
