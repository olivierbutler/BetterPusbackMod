# BetterPushback: SDK 4.4 modernization review

Prepared for Olivier | Contributor technical assessment | Updated 16 September 2026

## 1. Decision brief

Recommendation: complete the current owner-requested UI corrections on the existing supported renderer. Evaluate X-Plane's native ImGui renderer as a separate, optional modernization change, retaining compatibility with earlier supported simulator versions. Do not make the current approval package depend on a beta graphics migration.

This is an advisory proposal, not an approved upstream roadmap. The repository owner retains all decisions about supported X-Plane versions, platforms, release timing, architecture and who performs the work. Contributors can prepare a separately reviewable implementation if requested; the owner may instead implement it or defer it.

### Verified release status

Laminar published X-Plane 12.4.4 and SDK 4.4.0-b1 on 10 September 2026. Release notes rechecked on 16 September still describe 12.4.4 as an unstable public beta. The SDK package reviewed is XPSDK440b1.zip. The local simulator inspected during the original review reported 12.4.4-b1, build 124410. These are dated observations, not a prediction of final-release behavior. [1, 2, 3]

The major additions are Panel Graphics, native Dear ImGui rendering hooks, embedded browser interfaces through CEF, expanded avionics/object attachment facilities, and XLua 2.0 integration. Panel Graphics delegates supported 2-D drawing to X-Plane's native graphics backend instead of requiring a plugin-owned OpenGL renderer. [1, 2]

### What is required now - and what is not

Laminar expressly continues to allow OpenGL and other drawing systems. No removal deadline for the existing supported 2-D plugin window path was identified in the reviewed release materials. The new APIs therefore create an opportunity to reduce graphics integration work; they do not establish an immediate requirement to rewrite BetterPushback. This does not reverse older restrictions on legacy 3-D drawing callbacks. [1, 3]

Updating the SDK files alone does not modernize rendering, fix clipped copy, improve font coverage, or prove a performance improvement. Those outcomes require implementation and measurement. The release notes also list beta graphics/performance issues, which must be distinguished from plugin regressions during testing. [2]

Review scope: complete 12.4.4 release notes and SDK changelog; relevant Panel Graphics, font, texture, ImGui, window, interaction and instancing documentation/headers; comparison with the contributor branch at baseline 37cdc210608655851bfa2141d4dffbedf3f98834. This is not an audit of every dependency or a simulator certification.

<!-- page -->

## 2. How BetterPushback fits the new SDK

The Ground Operations interface already uses Dear ImGui. Its layout emits ImGui text, shape and clipping commands. A shared custom OpenGL renderer then draws them. Native ImGui is consequently the most direct first migration candidate: preserve the interface and replace its rendering backend, rather than rewrite every control. This is an engineering assessment based on the inspected source and SDK interfaces. [3, 4]

| Area | Current implementation | Proposed direction |
| --- | --- | --- |
| Ground Operations panel | src/ground_ops_ui.cpp creates ImGui draw lists and input targets. | Preserve layout and state rules; migrate drawing submission separately. |
| Shared window renderer | src/ImgWindow/ImgWindow.cpp uses the fixed-function OpenGL path. | Add a native Panel Graphics window/backend, with a tested legacy fallback. |
| Fonts and textures | ImgFontAtlas.cpp uploads a shared OpenGL font texture; ui_runtime.cpp owns shared UI lifetime. | Introduce backend-specific texture ownership and audit all sharing windows. |
| Pushback planner | src/bp_cam.c contains separate direct OpenGL drawing and XPLMDrawString calls. | Inventory its overlays and coordinates separately; panel migration will not remove this OpenGL use. |
| Tug and wing walker | src/tug.c and src/wing_walker.c use XPLMCreateInstance and XPLMInstanceSetPosition. | Retain the existing instanced-object path. New attachment APIs are optional, not a necessary replacement. |

### Text quality is a separate responsibility

The Panel Graphics font API provides font loading, metrics, width measurement and wrapping. It can be useful for a future direct native-text implementation. However, text still needs an adequately sized container, correct line height and supported glyphs. Word wrapping alone cannot guarantee that text fits vertically. [3, 5]

Native ImGui rendering retains the plugin's ImGui-generated font atlas. It does not automatically substitute the new Panel Graphics font engine or provide resolution-independent text. Font-quality work must therefore be evaluated separately from changing the ImGui backend. Use the same font and sizing rules for measuring and drawing each text region. [3, 4, 5]

The inspected unknown-value strings contain an em dash, while the UI builds a limited glyph range. A missing glyph is a plausible explanation for the displayed question marks. Using ASCII "--" removes that ambiguity without requiring SDK 4.4. Likewise, clipped messages and rail lines crossing labels are layout defects to correct now, not reasons to delay until a renderer migration.

<!-- page -->

## 3. Proposed sequence for owner review

### Track A - current UI corrections

Keep the current graphics backend and preserve the legacy tug motion, planner, normal disconnect/reconnect behavior, speed-based visibility and default-off telemetry policy. Make the following changes as a focused review package:

- Add explicit pilot acknowledgement once the legacy driver is displaying the pin/clear signal at the side of the aircraft. Retain "Verify the clear signal" and present the Acknowledge action in the existing PILOT ACTION style. Departure requires acknowledgement; preserve the legacy minimum signal-display time and reset the gate on every new sequence.
- Give command buttons clear hover and pressed states, immediate activation feedback and a quiet click. Hover-only collapse remains silent. Audio should respect mute/volume, reuse a prepared sound and release active resources on shutdown.
- Use drawn pop-out, minus and close controls. Collapse by clicking minus, dwelling over minus for about one second, or clicking the full-panel progress rail. Brief flyovers and drag gestures must not collapse the interface.
- Keep compact-to-expanded behavior click-only. On first collapse, select the nearest left/right edge of the current monitor. Allow subsequent repositioning and remember compact resting positions by display context. Constrain the visible UI to available bounds and recover saved positions after display changes.
- Replace unknown Speed/Remaining characters with "--", suppress compact tooltips, and leave space around rail labels so connector lines do not cross them.

These behavior and layout rules should remain independent of graphics-specific calls, allowing their tests and most interface code to survive a later native-rendering migration. This document does not claim that the current implementation has passed simulator acceptance; evidence belongs in the accompanying change checklist.

### Track B - optional SDK modernization

Stage 1: owner defines the supported simulator/platform matrix and approves an isolated prototype. Establish baseline screenshots, frame timings and resource measurements on the existing renderer.

Stage 2: implement a native ImGui backend for the Ground Operations window and audit shared font/window ownership. Retain the existing renderer on earlier supported X-Plane versions and provide a recovery path if native initialization fails. Do not simultaneously change operational behavior or redesign the planner.

Stage 3: validate the complete window and other affected ImGui windows across supported platforms and display modes. Prefer promotion after 12.4.4 reaches a stable release and the required evidence passes. A stable simulator release alone is not sufficient proof that the plugin implementation is correct.

Stage 4: assess remaining planner/OpenGL work, direct native fonts or retained-drawing optimizations on their own merits. Agree a separate scope and acceptance criteria before implementing them.

<!-- page -->

## 4. Engineering risks and release gates

### Compatibility and rendering boundaries

SDK 4.4 adds a window content type for Panel Graphics. The appropriate migration unit is the window's rendering layer, not scattered replacements of text calls inside an OpenGL drawing callback. Drawing must use the correct callback/context and coordinate conventions. [3, 4]

Optional native support requires more than an X-Plane version check. Avoid unconditional imports of new functions if older runtimes remain supported. Resolve optional functions safely and use the correct window-creation structure size/layout for each supported API version; otherwise a plugin may fail to load or a window may fail to initialize before fallback is possible. Laminar documents optional API lookup through XPLMFindSymbol. [3, 6]

### Fonts, coordinates and resource ownership

Native ImGui draws use opaque SDK texture handles, not existing OpenGL texture IDs. Font texture creation, lookup and destruction need backend-specific ownership. Audit shared atlases before converting one window while other windows remain on the old renderer. Prevent stale handles and duplicate destruction. [3, 4]

Validate vertex/index layouts, draw offsets, color packing, clipping rectangles, top-left ImGui coordinates and X-Plane window transforms. Test display scaling and pop-out transitions explicitly; a result that looks correct at one scale is not enough. Keep drawing in the allowed callbacks and follow the SDK's thread-safety restrictions. Do not introduce background drawing threads as an assumed optimization. [3, 4]

Retained drawing can cache static graphics, but it adds invalidation and resource-lifetime obligations. It is not needed to deliver the owner-requested behavior changes and should be introduced only if profiling justifies it. [3]

### Evidence required before native rendering becomes the default

- Functional/visual: all normal, aborted, reconnected and Emergency Tow states; pin acknowledgement; every caption and long message; hover, press and disabled states; no clipped or missing text.
- Window behavior: full and compact views; hide/show; pop-out/in; left/right and stacked displays; negative display coordinates; mixed scaling; saved-position recovery after resolution or monitor changes; VR where supported.
- Platform/compatibility: real Windows/Vulkan and macOS/Metal runs, Linux and other retained targets, plus an earlier supported X-Plane version exercising fallback. A Windows enlarged-UI preview is not a macOS rendering test.
- Resource/performance: repeated UI and pushback cycles, aircraft reloads, plugin unload and extended idle. Check for continuing CPU work while hidden, growing memory/graphics resources and recurring errors. Compare like-for-like frame times; do not promise an FPS gain without measurements.

Existing state tests remain useful, but cannot certify GPU rendering, audio behavior or font appearance. Record exact source revision, binary hashes, simulator version, platform, aircraft, display setup and test outcomes. Keep rollback artifacts until the owner accepts the result.

<!-- page -->

## 5. Decisions reserved for the owner

The immediate decision is whether to accept the focused UI correction package after its tests. A separate decision is whether to authorize SDK modernization. Neither a contributor push nor this assessment commits upstream to a new minimum simulator version or renderer.

Suggested choices to record before Track B begins:

- Minimum supported X-Plane version and supported operating systems/architectures.
- Whether native rendering begins as an opt-in prototype, an automatically selected path with fallback, or a later separate release.
- Who implements and reviews it: the owner, contributors, or another maintainer.
- Who can provide real macOS/Metal, Linux and display-scaling validation.
- Whether the first scope is limited to Ground Operations/shared ImGui windows, leaving the planner and font redesign for later.

CEF, XLua 2.0, new map/radar/synthetic-vision drawing, object attachments and other SDK additions should not be adopted merely because they are new. No requirement for them was identified in the current BetterPushback owner-feedback scope. Rebuilding this compact interface in a browser or Lua would materially expand the work and requires a separate product decision. [1, 2, 3]

### Source register

All references below are Laminar Research publications or its official SDK package. API details were reviewed on 12 September 2026; the release notes and announcement were rechecked on 16 September. Recheck the final SDK headers and release notes before implementing against a later beta or stable release.

[1] Laminar Research, "What's new in 12.4.4? | The SDK Update", 10 September 2026. https://www.x-plane.com/2026/09/whats-new-in-12-4-4-the-sdk-update/

[2] Laminar Research, "X-Plane 12.4.4 Release Notes", release date 10 September 2026. https://www.x-plane.com/kb/x-plane-12-4-4-release-notes/

[3] Official SDK 4.4.0-b1 package: README changelog, CHeaders/XPLM and bundled Documentation/xplm. Includes the window, Panel Graphics, font, texture, retained-drawing and instancing contracts reviewed for this assessment. https://www.x-plane.com/wp-content/uploads/2026/09/XPSDK440b1.zip

[4] Laminar's new API reference, XPLMPanelGraphics / ImGui Helpers. https://xpsdk-api.docs.staging.x-plane.com/docs-xplm/XPLMPanelGraphics/imgui_helpers.html

[5] Laminar's new API reference, XPLMPanelGraphics / Panel Graphics Fonts. https://xpsdk-api.docs.staging.x-plane.com/docs-xplm/XPLMPanelGraphics/panel_graphics_fonts.html

[6] Laminar Research, "Building and Installing Plugins", optional SDK API lookup and compatibility guidance. https://developer.x-plane.com/article/building-and-installing-plugins/

The online reference is currently hosted on Laminar's staging documentation site. If a page is temporarily unavailable or differs from a later revision, consult the headers and documentation bundled with the specific SDK package used for the build. Engineering recommendations in this document are the contributors' assessment, not statements of an approved Laminar or upstream BetterPushback roadmap.
