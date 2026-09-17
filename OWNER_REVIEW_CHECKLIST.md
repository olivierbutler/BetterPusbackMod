# Owner review checklist

## September 16 final visual follow-up - local Windows acceptance passed

The tester reported all final live checks passed and approved this version for
local commit. Owner review/acceptance is a separate step. This top section is
the final status; the earlier candidate notes below are historical.

Final evidence: September 16, 18:12-18:24 local time, X-Plane 12.4.4-b1
(build 124410), Windows/Vulkan, ToLiss A320 NEO V1.3.3, MMUN T3 Gate 43.
The installed binary matched the Windows SHA-256 below, and all changed
source files matched the build staging. The tester confirmed the final task
card styling, button appearance, click controls and revised checklist wording.

Reviewed final Log.txt SHA-256:
`55F1C783C08EF7BEF2849AFA0BD203E90993730D610CB709C4B8559092C1D12A`

- Gate 43 recognized; Route 1 recalled at 18:18:48 and accepted unchanged at
  18:18:55, without rewriting the saved route.
- Normal pushback completed, including wing-walker stop/standby/clear signals.
- Clear signal displayed at 18:23:11; pilot acknowledged at 18:23:16; tug
  departure began at 18:23:26, preserving the 15-second minimum.
- Ground operation completed at 18:23:50. UI cleanup and plugin unload were
  logged; X-Plane shut down normally at 18:24:12.
- Full-panel draw CPU average 0.019 ms, maximum 0.207 ms (68,501 draws);
  compact average 0.012 ms, maximum 0.034 ms (763 draws). These are UI draw
  timings, not whole-plugin/frame-time or memory-leak measurements.
- No BetterPushback [E] entries. Three guard warnings: aircraft not on ground
  during loading, and two disabled stop-planner commands. No corresponding
  failure of the completed operation was observed.
- No telemetry-recording log entry or new CSV; latest existing CSV remains
  September 4. Click volume persisted as 1.0 with clicks enabled.

The log also contains simulator/other-add-on diagnostics (audio device,
controller calibration, scenery frequencies and ToLiss teardown). They are
not BetterPushback error entries. Raw logs remain local, not in the repository.
Native macOS/Metal, live Linux, shared-cockpit and all monitor configurations
are not certified by this Windows test; no leak-free guarantee is claimed.

The user confirmed saved-route recall after restarting at a matching airport,
gate and aircraft. No route-cache change is needed or included. The cache and
planner source files remain unchanged by this round of UI corrections.

This candidate makes only two runtime changes beyond the earlier September 16
pilot-action/volume candidate:

- All required PILOT ACTION cards, including disconnect approval, use the
  existing amber background, yellow heading and yellow left accent. This
  supersedes the neutral disconnect-card styling recorded below. Disconnect
  tug and Acknowledge remain yellow; Reconnect retains its secondary style.
- During "Tug returning to station", CURRENT TASK reads exactly "Finalize
  cockpit checklist". It remains informational, with no new button or gate.

Full regression and both 884,736 text-fit runs passed, including a new test
that the cockpit-checklist reminder is not a required action. Windows/Linux
normal Release and enlarged-scale Linux builds passed. All 14 changed source
files matched build staging. X-Plane was closed for installation; previous
binary/configuration/log were backed up and configuration was preserved.
The final appearance was accepted by the tester in the live pass recorded above.

Current candidate SHA-256:

- Windows (installed): `D98B5F0027C4083CC303C1A8D8F37A712E22C71C24B61765426928F2DF872F5E`
- Linux (staged only): `94F3990B3D6806130CE779E4BA52C18BA2DC20E48A81BD68BB22308A5956280D`

Local acceptance supports submission for owner review. No SDK modernization
or change to the supported platform/version policy is included.

## Earlier September 16 candidate - historical implementation notes

The user reported the September 12 candidate passed their simulator checklist,
apart from the requested button emphasis, disconnect-card wording and click
volume adjustments below. This is user-reported Windows acceptance, not a
claim of macOS, Linux, shared-cockpit or leak-free validation. The revised
September 16 candidate was still awaiting its own simulator acceptance when
the following notes were written; final acceptance is recorded above.

Implemented changes (no motion, acknowledgement timing or SDK migration changes):

- Disconnect card: neutral background, PILOT ACTION heading and exact task
  text "Cleared to disconnect". Disconnect tug is yellow; Reconnect remains
  secondary blue. The existing "Disconnect confirmed" feedback remains.
- Acknowledge is yellow; the existing "PILOT ACTION / Verify the clear signal"
  task card and 15-second minimum display/explicit acknowledgement gate remain.
- Required Call tug, Plan push/tow, Resume (when held), Disconnect tug and
  Acknowledge use yellow/dark text with separate hover and pressed colors.
  Pause, Change plan, Reconnect and Call tow back remain optional/secondary;
  End operation and Confirm end retain their destructive-action treatment.
- Preferences adds Button click volume, 0-100%, beside crew volume. Zero mutes;
  releasing the slider previews the selected level. Changes affect clicks
  immediately, and Save preferences persists them through the existing flow.
  The initial level is 35% only when unset; saved levels and legacy mute are
  retained. Moving the slider above zero unmutes. Crew/tug audio and the
  in-operation Preferences restriction are unchanged.

### New candidate checks completed

- Full regression suite passed, including button-role tests and both 884,736
  text-fit runs (portable font and installed X-Plane DejaVuSans font).
- Audio mocks passed 10,000 activations and 10,000 live gain changes, mute,
  saved-setting reloads, range/invalid input, missing APIs and failure cleanup.
  Address/undefined-behavior sanitizer audio run passed. This does not certify
  actual simulator audio or total-process memory behavior.
- Windows/Linux normal Release builds and the enlarged-scale Linux build
  passed. Native macOS/Metal remains untested. Normal test binaries retain
  telemetry disabled. Windows audio functions remain dynamically resolved.
- All 14 changed source files matched build staging; installed Windows file
  was hash-verified, with X-Plane closed. Configuration/assets were preserved.

September 16 candidate SHA-256:

- Windows (installed): `16B63F029DA23CF1F3CE1B4B643BFEAAF06447F9E553C099C3361552998D92C5`
- Linux (staged only): `8B9C54ED9C24874C2A81173B5EDCE2A33388DF4A59E3957B6568312AB8902044`

### Repeat live acceptance before commit/push

- Inspect the new slider and surrounding Preferences layout; test 0%, low,
  middle and maximum values. Confirm release-preview and subsequent panel
  clicks match, 0% is silent, simulator mute still works and unrelated audio
  is unchanged. Save preferences, reopen, then restart to check persistence.
- Inspect the disconnect card's exact two lines and neutral background. Check
  yellow Disconnect vs secondary Reconnect, hover/hold/activation feedback,
  and press-drag-out cancellation. Inspect Acknowledge styling separately.
- Confirm required vs optional colors throughout planning, brake-release,
  pause/resume and completion. All labels must remain contained/readable.
- Complete a normal operation and another fresh acknowledgement sequence.
  Hold without acknowledging beyond 15 seconds; departure must remain gated.
  Check minimize/expand, docking, optional Reconnect and clean shutdown again.
- Review the next Log.txt and screenshots before approval. No commit or push
  is authorized by the implementation request alone.

Prior-session evidence archived with the pre-install backup: clean shutdown,
panel draw average about 0.020 ms in logged summaries; no new telemetry CSV
since September 4. Log guard warnings include aircraft not on ground and
disabled stop-planner commands; no BpB [E] entries were found. These observations
do not establish absence of leaks or acceptance of the revised binary.

Audio API references for channel gain and playback lifetime:
https://developer.x-plane.com/sdk/XPLMSetAudioVolume/
https://developer.x-plane.com/sdk/XPLMPlayPCMOnBus/

## September 12 follow-up - historical candidate checklist

The following was the original pre-acceptance checklist; final status is above.
SDK 4.4 adoption is NOT included. See `SDK_440_REVIEW.md` and its companion
owner-facing PDF for the separate advisory proposal.

- At the pin/clear signal, verify "Verify the clear signal" remains displayed
  in the PILOT ACTION card, with one full-width **Acknowledge** button. Wait
  longer than 15 seconds without clicking: the driver must keep showing the
  signal and must not leave. Acknowledge once, confirm visible acknowledgement,
  and observe normal departure. Also acknowledge early: the original minimum
  15-second display time must still be respected.
- Try holding the acknowledgement command before the signal, repeated clicks,
  closing/reopening the panel while waiting, normal/end/reconnect sequences,
  Emergency Tow, and a second normal operation. No earlier acknowledgement
  may leak into a new sequence. The keyboard/joystick command is
  `BetterPushback/acknowledge_clear`.
- Confirm **Disconnect tug** immediately changes to "Disconnect confirmed"
  before clearance begins. Buttons must visibly brighten on hover, depress on
  hold, and give a short activation flash/click. Dragging off a button cancels
  activation. Confirm mute and the simulator UI-volume control silence the
  click as expected. Optional configuration: `ground_ops_click_sound=false`
  or `ground_ops_click_volume=0.0` (default volume 0.20, range 0..1).
- Check the drawn pop-out/in icon, minus and X at both normal and enlarged
  scale. Confirm no text-glyph-dependent window-control symbols remain.
- Click minus, hover over minus continuously for one second, and click the
  full-panel stage rail: each must collapse. A brief flyover or any drag must
  not collapse. Hover collapse makes no click sound. Expand only by clicking
  the compact rail, never by hovering. Drag out and back before releasing:
  it must remain a drag, not an expansion.
- With no remembered position, collapse a full panel near each side of the
  screen and verify it docks to that side. Drag the compact rail to a chosen
  resting point; expand/collapse, close/reopen and restart to verify recall.
  Move the full panel to the other side or monitor: it must not recall an
  unrelated side/monitor's saved location.
- Drag floating full and compact views against all four edges. The whole UI
  must remain visible; a drag across to another monitor must remain possible.
  Native OS pop-outs recover after geometry settles (up to two seconds), so
  OS-title-bar dragging can cross monitors. Test negative coordinates, stacked
  monitors and removed/resized displays. VR resizing stays local; desktop edge
  docking does not apply to a VR window.
- Hover all compact orbs: no tooltip may appear. Expand and verify readable
  tooltips return. Check every label: no connector line crosses its lettering.
- Confirm unknown metrics use ASCII `--`, and real zero values remain numeric.
  Inspect all messages at both scales; text-measurement tests do not replace
  screenshots or actual graphics validation.
- Repeat open/close, normal/emergency/normal pushback and aircraft reloads.
  Inspect resource usage, clean shutdown and the simulator log; ensure no new
  telemetry CSV files appear. Keep a pre-install binary backup until accepted.
- If shared-cockpit operation is supported, test the new acknowledgement command
  on both controller copies and review the third-party synchronization profile.
  A single-pilot acceptance test does not establish shared-cockpit compatibility.

Automated policy tests cover acknowledgement gating, state reset, placeholders,
hover timing, docking/rest offsets, bounds and enlarged scale. The additional
`tests/run_ground_ops_text_fit_tests.sh` checks the UI text with ImGui font
measurements; pass X-Plane's `Resources/fonts/DejaVuSans.ttf` as its argument
for the actual simulator-font check. The default runner uses ImGui's built-in
font for portable CI. Neither path validates GPU output or native mouse/audio
behavior. Do not push for owner approval until these live checks are recorded.

### Automated checks completed for this candidate

September 12 automated evidence (uncommitted candidate based on
`37cdc210608655851bfa2141d4dffbedf3f98834`): all regression runners passed,
including 884,736 text-fit cases with the portable font and another 884,736
with the installed X-Plane DejaVuSans font; audio mocks passed 10,000 repeated
activations, bounded channel ownership, shutdown, mute and missing-API tests.
Normal Windows and Linux release builds and an enlarged-scale Linux build
passed. Enlarged-scale Linux is NOT a macOS/Metal test. `git diff --check`
passed. No native simulator render, audible-click or memory-soak acceptance is
claimed. No SDK 4.4 rendering APIs were adopted.

Staged, stripped binary SHA-256 values:

- Windows: `C87D7AE994DE6F8406E13580F1A4B8007AAADCFC06B123FB398BEFBCE36A614C`
- Linux: `5B2A1B6B54DEB4B458033A46A7171420B34EE0DB7F2F691B165DBF2BC6A3C2DC`

Before the September 16 test installations, the Windows baseline was
`5CCFEE273150A87EA0C95B8CF713097B63FDB5DA603962E9C83EEAD8F3BDE7A5`.
The staged binaries are for testing, not owner-approved releases.

## Previous accepted reconciliation checks

This checklist covers the reconciliation requested on 2026-09-04. Automated
tests validate state mapping, route helpers, and both UI scale modes; the final
visual and motion checks require X-Plane.

## Build and UI

- Build macOS normally. The Ground Operations rail should be 78 by 329 and the
  panel 394 by 567, with fonts, spacing, controls, hit targets, and drag
  threshold all scaled together.
- For a Windows/Linux visual preview, run `./build_xpl.sh -m`. The normal build
  without `-m` must remain at 58 by 244 and 292 by 420.
- Hover every Ground Operations control. Panel tooltips must wrap and remain
  inside the panel.
- Confirm every CURRENT TASK message remains inside its card. The completed
  operation message must render on exactly two lines.
- Visit every normal and Emergency Tow panel state at both normal and enlarged
  UI scale. Confirm status, detail, caption, task, metric, and button text wraps
  or reduces in size without clipping or crossing a panel/card boundary.
- Confirm the stage rail uses green for completed stages, yellow for the current
  stage, and cyan for future stages. No separate yellow exclamation badge may
  appear.
- Confirm the installed package contains the wing-walker OBJ, diffuse texture,
  LIT texture, and attribution file under `objects/wing_walker`.
- Confirm a normal build does not log `Recording pushback telemetry` and does
  not create a new CSV in `Output/BetterPushback/telemetry`. Telemetry must be
  present only in an explicit `./build_xpl.sh -t` diagnostic build.
- Confirm the Preferences window does not show Auto disconnect when done,
  either Hide magic squares control, Magic squares position, Enable Ground
  Operations UI, or Show Ground Operations captions.
- Confirm ACF Plugin Exclusion (Experimental), Eye Tracker Plugin Exclusion,
  and all other retained upstream preferences are present.

## Normal operation

- Draw and accept straight, single-turn, multi-turn, and forward-tow routes.
  Compare planner construction, steering, turn transitions, tug approach, and
  final stopping with the upstream legacy behavior.
- While connected with the parking brake set, confirm **Change plan** appears
  and reopens the planner. Release the brake and confirm the action disappears
  before motion.
- Pause during a straight and a turn. Confirm only longitudinal motion stops,
  then resume and verify the same route continues.
- Use **End operation** while moving and while held. Confirm the aircraft stops
  and disconnects at the current position without returning to the planned
  endpoint.
- At **Ready to disconnect**, confirm the task card uses the normal neutral
  styling and offers **Disconnect tug** and **Reconnect** inside the Ground
  Operations panel. Confirm no separate disconnect or reconnect windows appear.
- Choose **Reconnect** once and confirm the controller returns to the connection
  sequence. Return to the disconnect gate, choose **Disconnect tug**, and
  confirm the normal clear-signal and tug-departure sequence completes.
- Confirm Ground Operations remains visible throughout an active operation.
  After completion, taxi at or above the legacy 1 m/s threshold and confirm
  the UI hides; slow below 1 m/s while on the ground and confirm the same
  expanded or collapsed view returns automatically.

## Repeated and emergency operation

- Complete normal pushback, **Call tow back**, Emergency Tow, and another
  normal pushback without reloading the plugin.
- Confirm Emergency Tow uses the same legacy motion engine and configured tug
  towing-speed limits.
- Confirm the wing-walker timing remains unchanged for a normal operation and
  remains omitted for Emergency Tow.

Record the tested commit, platform, X-Plane version, aircraft, tug, and any
screenshots or `Log.txt` evidence before release approval.
