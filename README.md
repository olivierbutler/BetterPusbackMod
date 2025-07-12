# About BetterPushback Mod X-Plane 11/12

This is a pushback plugin for the X-Plane 11/12 flight simulator.
It provides an overhead view to plan a pushback route and
accomplishes a fully automated "hands-off" pushback, letting the user
focus on aircraft startup and other pilot duties during pushback. It can
of course also tow you forward, or perform any arbitrarily complicated
pushback operation. To increase immersion, it speaks to you in a variety
of languages and accents, simulating ground staff at various places
around the world.

### About this Fork and Copyright

Better Pushback is developed by "Saso Kiselkov". So if you see this project or else, just contact me.
I just did it, to "keep it a life on X-Plane 12". I will always respect that this is your code,
and you are the father of this application. Hope you accept this as there was no answer from your side.

There is no idea to steal it from you. If you don't like this effort. Just say and I will stop it, no question.

Thanks

## Downloading BetterPushback

You can get the last binary release from here:

[https://forums.x-plane.org/files/file/90556-better-pushback-for-x-plane-1112]()

Some Beta / Pre-Releases can be found here:

[https://github.com/olivierbutler/BetterPusbackMod/releases]()

(Please be aware that pre-releases maybe still has some issues inside and maybe is not final tested.)

## Building BetterPushback

To build BetterPushback, check to see you have the pre-requisites installed. The
Linux and Windows versions are built in one step on an Ubuntu 16.04 (or
compatible) machine and the Mac version is obviously built on macOS (10.9
or later).

> Note: **on macOS only** , by using the option `-f`, the script will build also the linux and windows versions. see `README-docker.md`.

For the Linux and Mac build pre-requisites, see `build_xpl.sh`

The global build script is located here and is called '`build_release`'.
Once you have the pre-requisite build packages installed, simply run:

---

```
$ ./build_release [-f]
```

This builds the dependencies and then proceeds to build BetterPushback for the appropriate target platforms. Please note that this builds a
stand-alone version of the plugin that is to be installed into the global
Resources/plugins directory in X-Plane.

---

```
$ ./build_xpl_sh [-f]
```

This build only the .xpl file. (option described above can be used)

---

```
$ ./install_xplane.sh
```

Copy the .xpl files to the x-plane and change the quarantine attribute of the `mac.xpl` file.
In the script, just set `XPLANE_PLUGIN_DIR` accordingly.

---

For details on how to add tug liveries, see
`objects/tugs/LIVERIES_HOWTO.txt`.

To add a voice set, see `data/msgs/README.txt` for the information.

### libacfutils Library Required

I removed from the project. It need to by downloaded separatly. To make sure you have a matched version, take the fork in my repository.
To connect with the library setup the Library in the `CMakeLists.txt` File in the "src" directory.

`file(GLOB LIBACFUTILS "../../../libs/libacfutils")`

As I found out in the last view days the relation to this library are very hard and many issues come from here ... it is not possible to splitup the library.

The library can be found here:
[https://github.com/olivierbutler/libacfutils]()

### CREDIT

Original BetterPushback version by skiselkov: [https://github.com/skiselkov/BetterPushbackC]()

### DISCLAIMER

BetterPushback is _NOT_ meant for flight training or use in real avionics. Its
performance can seriously deviate from the real world system, so _DO NOT_
rely on it for anything critical. It was created solely for entertainment
use. This project has _no_ ties to Honeywell or Laminar Research.
