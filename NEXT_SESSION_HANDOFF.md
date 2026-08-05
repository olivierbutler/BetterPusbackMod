# BetterPushback next-session handoff

Prepared: 2026-08-05  
Workspace: `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\BetterPusbackMod`  
Branch: `feature/realistic-tug-physics`

## Start here

Phase 7 Corrective Slice 4 is **simulator accepted**. It restores the original
manual planner after the automatic wind/runway proposal experiment was rejected.
The entire accepted fork baseline is committed. The next work is diagnostic
analysis of the persistent route-cache failure; do not change cache code until
the user explains the required behavior.

The worktree is intentionally dirty and contains the user's accumulated fork.
Preserve every existing change and untracked file. Do not reset, clean, revert,
or replace unrelated work.

## Fixed product decisions

- The pilot calls the tug. There is no automatic hookup or two-minute timer.
- After connection, **Plan push** opens the original manual-placement planner.
- The plugin must not infer a departure direction or generate route geometry
  from wind, runways, airport flow, or a generic automatic proposal.
- The pilot moves and rotates the cursor, clicks to place the desired aircraft
  pose, edits the route, and explicitly accepts it.
- A route explicitly drawn during the current planner session may be retained.
- Persistent route loading and saving remain disabled for this candidate.
- The persistent-cache alignment defect is the next separate design/fix topic,
  but only after the manual planner is simulator validated with the user.

## Rejected automatic proposal

The 2026-08-05 KCOS Gate 8 Slice 3 test showed an operationally unusable 93.7 m
automatic path. The user ended the proposal experiment. Exact rejected Windows
and Linux binaries, screenshot, `Log.txt`, telemetry, and evidence notes are at:

`C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice3-validation1-rejected-20260805`

The automatic proposal implementation was removed from `src/bp_cam.c`. Its
dedicated `src/airport_flow.c`, `src/airport_flow.h`, unit test, and runner were
deleted. Source and binary-string audits found no remaining operational proposal
references.

## Currently installed manual-planner candidate

- Windows SHA-256:
  `0BAE410F37632C192AF1DF31710DC6272B6A0B7139C7AF877F8B12DF5636EC09`
- Linux SHA-256:
  `71A2C20C80A66AB2704F4EF9CC94A24DCED8C6D7A5EECA106B005617C3241D18`

Installed at:

- `C:\X-Plane 12\Resources\plugins\BetterPushback\win_x64\BetterPushback.xpl`
- `C:\X-Plane 12\Resources\plugins\BetterPushback\lin_x64\BetterPushback.xpl`

## Accepted simulator validation

- **Plan push** opened with no prebuilt route, no suggested-departure message,
  and no **Saved route** control.
- The pilot manually positioned and accepted the aircraft route and completed
  the operation successfully.
- The planner reported zero prediction failures and X-Plane exited cleanly.
- Exact evidence is preserved at
  `C:\Users\DARRON\OneDrive\Documents\BetterPushBack\backups\phase7-slice4-validation1-accepted-20260805`.

## Next work: persistent route-cache diagnosis

Remain in diagnostic mode. Read the dormant `route_save`/`route_load` code, the
rejected Slice 2 evidence, and the saved cache file. Establish how coordinates,
aircraft pose, segment headings, matching, and re-anchoring currently work.
Present the diagnosis to the user and let the user explain the intended flawless
workflow before implementing a correction. Do not re-enable route loading or
saving during diagnosis.

## Verification completed

- Six remaining automated regression scripts passed.
- Windows and Linux warnings-as-errors release builds passed.
- Source audit found no automatic proposal implementation references.
- Windows and Linux binary-string audits found no proposal messages.
- Planner-source `git diff --check` passed.

## Relevant files

- `src/bp_cam.c`: restored manual planner startup and interaction.
- `src/driving.c` and `src/driving.h`: dormant legacy route-cache definitions;
  do not change until the user explains the required behavior.
- `src/planner_cache.c` and `src/planner_cache.h`: trajectory-render cache, not
  the persistent saved gate-route workflow.
- `PHASE_TESTING.md`: full history and current Corrective Slice 4 test.
- `GROUND_OPS_UI_DESIGN.md` and `ROADMAP.md`: automatic proposal removed from
  the active product design.

## Build/install notes

The dependency-complete build staging tree is:

`/opt/betterpushback-build/BetterPusbackMod`

When a new build is genuinely required, copy only changed source files into the
staging tree, run `./build_xpl.sh`, and copy the resulting Windows/Linux binaries
back into this workspace. Never overwrite installed binaries while X-Plane is
running. Archive the installed candidate and simulator evidence before replacing
a rejected build.
