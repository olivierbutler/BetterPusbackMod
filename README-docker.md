# Cross-compile to linux,windows from a mac

## Pre-requisites

- docker for mac

that's it

## Settings

in `build_xpl.sh` set the paths of the current project and the libacfutils lib

```
# Set here the correct paths
# docker's internal path /xpl_dev is mapped 1 level up from the current folder
# the libacfutils folder is expected to be at the same level of this projet, if not
# modify docket-compose.yml accordingly.

# Host folders         | Internal docker folders
# ---------------------|---------------------
# ../projets/          | /xpl_dev/
# ├─ libacfutils/      | ├── libacfutils/
# └─ BetterPusbackMod/ | └── BetterPusbackMod/

PROJECT_PATH='BetterPusbackMod'
```
