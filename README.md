# Congregation 1.0.2

Congregation is a local canvas editor built with the native iiSDK platform. Its application lifecycle and asynchronous services use iiSDKCore; its views use LVUI-owned native widgets hosted by iiGUI. The active application does not link LVRS, Qt, QML, a browser engine, or a WebView.

LVUI views are HTML-like markup with inline styles. They compile to C++23 during the build and are not interpreted as web pages at runtime. macOS uses Cocoa and Metal; Vulkan is available through MoltenVK. Windows Win32/Direct3D and Linux X11/Vulkan adapters are implemented as source ports. Their execution is not verified by the macOS build.

The current Main window uses a 54 px vertical icon toolbar at the left window edge, a centered canvas with pixel rulers, a bottom timeline/layer dock, and a 352 px segmented inspector. The former 224 px asset sidebar is removed; the canvas and timeline share the recovered space. The rest of the layout follows the Congregation Figma design. The Tool panel exposes all 22 parameters through sliders and step buttons with a scrolling body and fixed preset/actions. See [the native Figma contract](docs/FIGMA_APPLICATION_WINDOW.md). The earlier [Vincent restoration](docs/VINCENT_UI.md) remains historical reference.

## Editing

- Finite canvases, pressure-aware painting and erasing, flow, opacity, hardness, spacing, stabilization, and pressure response.
- Native layer lists, selection, renaming, visibility, reordering, duplication, and deletion.
- Vector lines and shapes, native font text placement, imported images, undo/redo, and tile-based viewport rendering.
- Canonical `.iisc` documents, independent recovery files, and migration of earlier `.vrc` canvas containers.
- PNG/JPEG/BMP/GIF/TGA/WebP/TIFF, layered PSD, and OpenRaster input. PNG/JPEG/BMP/TGA/WebP/TIFF and layered PSD output.
- Native file dialogs, clipboard images, file/image drops, application menus, shortcuts, preferences, and collaboration windows.
- Browser sign-in at iisacc.com and online product-license checks use a device-scoped iiAccountManager session; the app never asks for a password or license key. The web-account migration and its update path are still being validated. LAN discovery and shared-canvas services remain native.

The document SDK retains vector paths and exports them as PDF smart objects in PSD files. Unsupported foreign-document features fail explicitly. Migrated legacy text is rendered into an individual native bitmap layer. Undo history is bounded to 32 commands and 64 MiB of serialized snapshots.

UI colors and control styles use LVUI `Color` and `Metric` tokens, including the shared Figma-derived values migrated from LVRS. `App/Native/views/Layout.lvui` contains product layout dimensions for all three compiled views and the generated native C++ header; it defines no color palette. See [the styling contract](docs/UI_STYLE_TOKENS.md) for token selection, regeneration, data boundaries, and regression checks.

The editor's native view model exposes LVUI-compatible, revisioned list sources and a shared observable color alongside its existing commands. The current screens use those sources directly. See [the view-model contract](docs/LVUI_VIEWMODELS.md).

## Build

A C++23 compiler, CMake 3.31+, Python 3.9+, and an installed native iiSDK kit are required. Build the SDK packages before the app; the staged kit used by this workspace is `../../SDK/build/congregation-native/prefix`.

```sh
./build.sh local
```

`CONGREGATION_SDK_PREFIX` selects another installed kit. All product build output stays in `build/`. The canonical macOS application is `build/Congregation.app`. Its post-build packaging step closes the runtime dependency graph, includes third-party notices, removes absolute runtime search paths, checks the macOS 12.0 deployment declarations, and verifies an ad-hoc code signature. Notarization and Store publication are separate release steps.

On Windows, use `build-windows.ps1 -SdkPrefix <native-kit>`. The source requires an appropriate native kit for the target architecture. Existing Qt-era Store/MSI authoring material remains historical and is not a verified release path for this native migration.

For a connected iPad, build the isolated iPhoneOS SDK kit with `python3 -B tools/build_ipados_sdk.py all`, then use `python3 -B tools/build_ipados.py --device <iPad-UDID> --install --launch`. The UIKit/Metal source shares the C++ document engine and LVUI views. The independent LVUI migration has not yet been validated on an iPad. The same canonical `build/Congregation.app` output slot is used; the previous macOS bundle is preserved in a verified ZIP before switching platforms. See [the iPadOS build and device checks](docs/IPADOS.md).

See [the build guide](docs/BUILD.md), [native runtime contracts](docs/NATIVE_RUNTIME.md), and [migration verification](docs/NATIVE_VERIFICATION.md).

For a macOS installer, run `cmake --build build --target CongregationInstaller` after building and testing. It signs the canonical native app and creates `build/Congregation.pkg` for `/Applications/Congregation.app`, with a checksum and payload verification report. See [installer signing and notarization](docs/MACOS_INSTALLER.md); a signed package is ready for public distribution only after notarization and Gatekeeper validation pass.

## Test

```sh
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
```

The suite covers the Core engine, native services, document editing and file compatibility, and real native-window runs with Metal and forced Vulkan. GUI self-tests use an isolated directory under `resources/` (also linked from `build/native-tests` when available), close automatically, and do not use the user's saved credentials or canvases. Runtime library override variables are removed from the test environment.

The window tests verify the absence of the asset sidebar, the left-edge toolbar and tool selection, canvas/timeline expansion at full and minimum window sizes, 22 parameter controls, scroll clipping, time-based timeline navigation, and inspector tabs. Run `ctest --test-dir build -R figma_ui --output-on-failure` to reproduce the native-window captures. Tests use the original Figma artwork as document content; the whole-screen reference screenshot is never used as an implementation asset.

## Controls

The New canvas child window shares Dreamscapes’ catalogue, layout, search, units, PPI, and background controls while keeping the main editor interactive. See [the native chooser contract](docs/NEW_CANVAS_WINDOW.md).

Use B for brush, E for eraser, T for text, V for selection, and H for pan. Space or the middle pointer button temporarily pans the view. Selection drag moves a layer; Shift drag scales it and Alt drag rotates it. Text editors retain their own keyboard input instead of invoking canvas shortcuts.

Focus a timeline layer and press Enter to rename it; drag its name to reorder it. Drag the timeline playhead to scrub elapsed time, or hold Alt and drag its frame ticks vertically to change the document rate.

Navigation and selection do not change document revisions. Each finished stroke is one durable edit; focus loss or Escape cancels its preview. The recent-canvas preference records only regular recovery files inside the app's own managed directory.

## Source organization

`App/Engine` owns product services and policies. `App/Native` owns native editor integration, and `App/Native/views` contains declarative LVUI views. `tests/` contains executable contracts. `src/App` is preserved migration reference material and is excluded from every active build and runtime resource set.

The current source declares version 1.0.2 and bundle build 3. A successful local build does not establish Windows/Linux runtime compatibility, physical-tablet validation, execution on an older macOS machine, or production account/update-server behavior.

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
