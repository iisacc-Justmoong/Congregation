# Congregation 1.0.0

Congregation 1.0.0 is a private, local-first raster drawing app built with Qt 6 and
QML. It provides a focused desktop canvas for drawing, handwriting, layered
raster editing, and PSD-compatible import and export without accounts,
telemetry, advertising, or automatic network access.

Version **1.0.0** starts Congregation's release series while preserving the inherited Git history. This is a source release; signed installers, Store listings, and production licensing/update services for the new product identifier are separate rollout steps. The installation instructions below apply when matching binaries have been published. Build the current source using the commands in [Testing](#testing).

![Raster-canvas interface preview with a sample artwork](docs/marketing/congregation-windows-editor.png)

_Interface preview with a sample raster artwork. These screenshot assets predate the 1.0.0 release._

## System Requirements
- macOS 12 or later (Apple Silicon or Intel)
- Windows 10 or later when using the Windows package
- Linux builds are available when provided by the release page

## Install (macOS)
1. Download `Congregation-1.0.0-macOS.pkg` from the latest release.
2. Double-click the installer and follow the prompts to install Congregation 1.0.0 in Applications.
3. Open Congregation 1.0.0 from Launchpad or the Applications folder.

## Install (macOS .app bundle)
1. Download `Congregation 6.0.app` from the release assets.
2. Drag it into the Applications folder.
3. Open Congregation 1.0.0 and allow any prompts to access files you select.

## Install (Linux)
1. Download the `Congregation-1.0.0-Linux.tar.gz` release archive.
2. Extract it and run `bin/Congregation` from the extracted folder.
3. The archive carries the detected Qt, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager shared runtimes, QML modules, and Qt platform plugins. An installed tree also provides a `Terminal=false` desktop entry for launching only the GUI on X11 and Wayland.

## Install (Windows)
The preferred public installation route is the Microsoft Store. A Store-delivered MSIX is certified and re-signed by Microsoft, so Windows users do not need Congregation's private development certificate and do not receive the unknown-publisher or SmartScreen warning associated with a self-signed download. The ZIP/MSI instructions below apply only when a separately Authenticode-signed non-Store release is offered.

1. Download `Congregation-1.0.0-Windows.zip` from the release assets.
2. Confirm that the release also provides `Congregation-1.0.0-Windows.zip.sha256`, verify the archive hash, and check that the extracted `Congregation.exe` has a valid Authenticode signature whose Publisher subject or certificate thumbprint matches the identity stated in the authenticated release notes.
3. Extract it to a writable folder such as `%LOCALAPPDATA%\Programs\Congregation`.
4. Run `Congregation.exe`. It opens only the Congregation GUI; no terminal or console window is required.

When the release also provides `Congregation-1.0.0-Windows.msi`, verify the adjacent checksum and the MSI's Authenticode Publisher before opening it. The Windows Installer 5 package uses the official `ALLUSERS=2` and `MSIINSTALLPERUSER=1` dual-context defaults, so 1.0.0 installs for the current user and starts a separate Congregation upgrade family without replacing the archived application. Choose **Advanced** on a first installation to select current-user or all-users scope; all-users scope also exposes the destination directory, installs under native 64-bit Program Files by default, creates a shared shortcut, and requires elevation, while current-user scope stays under Local Application Data. The application files and context-aware Start Menu shortcut are installed together. A required installation-context marker keeps later upgrades in the registered scope and preserves the recorded install location; when Windows Installer detects a markerless per-user upgrade, it also locks that legacy upgrade to current-user scope. Packaging explicitly validates this model with ICE105. ProductCode is deterministic for the same version and architecture, so rebuilding unchanged 1.0.0 cannot register a duplicate product, while a different version or architecture receives a distinct identity. If both scope registrations are discovered, new installation and cross-context upgrade are rejected, but maintenance and removal remain available so the machine can be recovered. After that product is installed, opening the same MSI intentionally enters the standard Windows Installer maintenance flow instead of presenting another **Install** action. Use **Repair** to restore missing or damaged installed files and **Remove** to uninstall Congregation; the standard **Change** path displays the required installed feature state. Remove the installed product first only when changing installation scope or exercising a genuinely fresh first-installation flow.

Windows maintainers create a public package with `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Sign -SigningCertificateThumbprint <40-hex-thumbprint>`. Public packaging fails closed unless `-Sign` is supplied, and requires Windows SDK SignTool plus a currently valid Code Signing certificate with an accessible private key in `CurrentUser\My` or the explicitly selected `LocalMachine\My` store; when the certificate contains a Key Usage extension it must permit `DigitalSignature`. The release operator must obtain that certificate from a consumer-trusted code-signing CA or trusted signing service and publish its expected Publisher identity. Before any build work, the script rejects self-signed identities, builds the Code Signing chain with online revocation checking, and requires its terminal root to be present in Windows' `LocalMachine\AuthRoot` public-root store. It signs Congregation-owned binaries with the selected identity, preserves valid timestamped vendor signatures, signs the remaining unsigned staged PE files after all binary mutations, and requires RFC 3161 timestamps plus Authenticode verification before promoting `dist/Congregation-1.0.0-Windows.zip` and its `.sha256` record from temporary names. `-CreateMsi` builds, validates without WiX warnings, and signs and verifies the MSI from a temporary path; when ZIP and MSI are requested together, the complete verified set replaces the last-known-good files in one rollback-capable publication step. A prepared/committed transaction journal records whether each canonical artifact existed before publication, so a retry removes half-published first-release files or restores the complete previous generation before any new build work starts. A machine-wide Congregation packaging mutex rejects concurrent invocations before they can compete for a build tree or journal, including when the same repository is reached through a path alias. Every public correction increments the release version; a local test package must opt in with `-AllowUnsignedPackage` and is named `Congregation-1.0.0-Windows-unsigned.*`. It is not a release asset. Use `-SkipPackage` for an unpackaged local build; use `-Clean -SkipPackage` for a clean-only local rebuild, or combine `-Clean` with the appropriate signed/explicitly unsigned package mode. Signed public builds cannot use `-SkipTests`.

For the certificate-free public Store route, maintainers reserve the app in Partner Center, copy its exact reserved display name plus the three Product identity values, provide the corresponding-source URL/hash, and run `build-windows-store.ps1 -Mode Store`. The reserved name must populate both package and application display names; an unreserved value fails Partner Center validation. iiPaintEngine is licensed by its copyright holder as `AGPL-3.0-only`, and that exact text is staged with the package. The command creates the intentionally unsigned `dist/Congregation-1.0.0-Windows-Store-x64.msixupload`; Microsoft replaces the package signature after certification. The Store does not re-sign MSI or EXE submissions. See [docs/BUILD.md](docs/BUILD.md) for account onboarding, local self-signed MSIX validation, restricted-capability justification, and submission steps.

## Code signing policy

Congregation is seeking free code signing provided by [SignPath.io](https://signpath.io/), with a certificate sponsored by [SignPath Foundation](https://signpath.org/). No SignPath Foundation certificate is currently active for Congregation. The prepared GitHub workflow must not submit a release signing request until the Foundation has explicitly approved the project and supplied the required identifiers. Windows website releases are built from this public repository on GitHub-hosted runners and require an authorized maintainer's manual signing approval. The unsigned `build/signpath-input` artifact and every self-signed trial artifact are development-only files; only a returned MSI whose container and nested `Congregation.exe` both have valid, timestamped SignPath Foundation Authenticode signatures may be published.

- Committer and reviewer: [iisacc-Justmoong](https://github.com/iisacc-Justmoong)
- Approver: [iisacc-Justmoong](https://github.com/iisacc-Justmoong)
- Privacy: This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it. Congregation currently performs local document editing and file selection only; it contains no telemetry, analytics, advertising, account login, or automatic network update client.
- System changes: the MSI announces installation scope and destination, installs the application and Start Menu shortcut together, and provides standard Windows Installer repair and removal.

The externally signed website release is the single file `Congregation-1.0.0-Windows.msi`. Its embedded Authenticode signature supplies publisher identity and integrity, so users do not need a separate certificate or checksum file to install it.

A valid trusted Authenticode signature replaces the Windows unknown-publisher identity with the certificate publisher, but Microsoft Defender SmartScreen also evaluates publisher reputation and per-file reputation, so a new release hash may still warn. A checksum proves file equality only when obtained through an authenticated release channel; it does not prove publisher identity by itself. Self-signed certificates are not suitable for public distribution. ZIP and MSI packaging also stages Congregation/LVRS and iiPaintEngine AGPL texts, psd_sdk/miniz, Pretendard, Qt, and MinGW legal materials plus Qt SPDX documents. Public signing still requires a publisher-controlled corresponding-source URL and SHA-256.

The generic Windows CPack target does not run Qt deployment, PE closure validation, or Authenticode signing. Its deliberately named `Congregation-1.0.0-Windows-unsigned-cpack-incomplete.zip` output is build-system diagnostics only and must never be published.

The Windows executable is a native AMD64 PE32+ GUI-subsystem application, so its lifetime is owned by the Qt GUI event loop rather than a companion terminal. QML retains LVRS stock application-window geometry; before the first show, C++ measures that hidden framework size as the aspect-ratio source, requests a 1,280-logical-pixel width, derives the height from the measured ratio, and scales the pair down only when required by the screen's available area. No post-display resize runs while the heavier canvas page is activated through an asynchronous QML `Loader`, and the window remains user-resizable after launch. Executable resources include the application icon, Windows file/product version information, an `asInvoker` manifest, Windows 10/11 compatibility, Per-Monitor V2 DPI awareness, and long-path awareness. The Release staging pass verifies those native properties and the required runtime DLLs, rejects incompatible MinGW runtimes, strips only Congregation-owned MinGW binaries, omits unused Qt generic, Insight Tracker, SQL, Quick3D utility, software OpenGL, and runtime shader-compiler payloads, and removes a loose `qml/LVRS` copy because LVRS QML is already embedded in `LVRS.dll`.

Normal startup performs no startup-log file I/O. To diagnose launch timing from a GUI-subsystem build, set `CONGREGATION_STARTUP_TRACE=1` before running `Congregation.exe`; milestone records are then appended to `%TEMP%\Congregation-startup.log`. A fatal failure to create the root QML object is recorded there even when tracing is disabled.

## Features at a Glance
- iiPaintEngine-backed brush and eraser strokes with native pointer/tablet event handling and pressure-controlled brush opacity
- Layered raster document model with a base raster canvas plus selectable transparent raster layers; blank documents start with `Background` plus a selected transparent `Layer 1`, each backed by its own iiPaintEngine surface and undo/redo state
- Image open flow through Qt image formats such as PNG, JPEG, BMP, GIF, WebP, and TIFF plus PSD files read through `psd_sdk`; importable 8-bit RGB/RGBA PSD layers reopen as Congregation raster layers, and every flat image open replaces the raster canvas at the source image dimensions without fitting the image into the previous canvas
- Raster save flow that composites the base canvas, current raster layers, and current image, text, and shape objects
- Internal PSD compatibility document layer that maps the raster canvas and current session objects into Photoshop-style layer records and XMP metadata
- LVRS-backed MVVM document state with a compact C++ canvas document view model
- Toolbar file actions expose new canvas, open image, and save image buttons; new canvas opens a width/height modal before creating the raster
- The application window exposes File, Edit, Window, and Help menus. Qt 6.8 uses the native macOS global menu bar, while Windows and Linux use a compact dark in-window menu bar whose 22-DIP height and background match the application; all variants preserve the existing canvas dialogs, undo/redo, layer actions, tool and shape selection, brush-size changes, canvas view controls, window controls, and complete shortcut reference entries
- Platform builds use `resources/Appicon.icns` as the canonical macOS icon source and `resources/Appicon.ico` for Windows executable resources; run `tools/sync_app_icon_assets.sh` after replacing the macOS icon so the App Store/Xcode asset catalog stays in sync
- Save image defaults to Photoshop PSD and also offers PNG, JPEG, BMP, WebP, and TIFF; PSD export writes a merged preview plus the current `Background` canvas layer when present, current raster layers, and current image/text/shape objects in bottom-to-top order so Photoshop presents `Background` at the bottom, with Congregation layer metadata embedded as XMP
- A docked left-side LVRS hierarchy panel lists the background raster canvas and current image, shape, text, and raster layers; new blank canvases select the initial transparent `Layer 1` so brush strokes land on an editable layer immediately, double-clicking a layer name edits it inline, dragging rows changes layer order, and the plus/minus footer buttons create or remove layers, including the background layer; when `Background` is removed, the canvas shows a tiled PNG-style transparency grid
- Layer creation uses an incremental visual model so adding many raster layers does not recreate existing layer surfaces or write PNG snapshots for unchanged layers
- Pan, move, zoom, brush, eraser, shape, fill bucket, and paint-style text tools with a dense full-width top toolbar placed directly below the menu bar on Windows, toolbar-frame and button x-axis padding set to half the y-axis padding, a current-color swatch with 4x right-edge padding that opens the HSL color picker, and brush controls that also set text size and text color
- Canvas cursors follow the effective tool: brush and eraser hide the system pointer and show a scene-graph vector circle whose diameter matches the current tool size at the active canvas zoom; subpixel footprints retain the exact circle and fall back to the system crosshair so the pointer cannot disappear; zoom and shape use a precision crosshair; text uses an I-beam; fill uses a pointing hand; pan uses open/closed hands; and move uses the arrow over empty canvas, four-way movement over object bodies, and directional resize cursors only over the selected object's resize handles
- Pan mode moves the canvas in the workspace by grabbing it with the hand tool, using viewport-relative movement offsets for smoother dragging; holding Space temporarily enters pan input without changing the selected tool
- Zoom mode scales the canvas by dragging horizontally anywhere in the canvas workspace: right to zoom in, left to zoom out
- Drag-to-insert solid shapes for rectangle, ellipse, triangle, diamond, star, rectangle bubble, and ellipse bubble, with Shift-constrained 1:1 bounds
- Inserted image, shape, and text objects are created as separate layer rows, can be selected with the move tool, dragged or resized past the canvas bounds from enlarged corner and edge transform handles while their visible pixels stay clipped to the canvas, Shift-dragged along the dominant straight movement axis, Shift-resized with their original aspect ratio, and deleted with Delete or Backspace before raster export
- Global menu shortcuts cover every actionable menu item: Command/Ctrl+N/O/S for new/open/save-as, Command/Ctrl+Z and platform redo for history, Command/Ctrl+Shift+N/Delete for layer creation/deletion, B/E/H/V/Z/U/G/T for tools, Command/Ctrl+Alt+1..7 for shape kinds, [/] for brush size, and Command/Ctrl+0/1/M plus platform fullscreen for window controls. Tool shortcuts also support matching two-beolsik Korean key positions on the canvas.
- Default brush hardness keeps iiPaintEngine's coverage-based edge anti-aliasing at its maximum app setting
- LVRS solid chrome keeps native window controls. Windows and Linux use their native title bars without a redundant logical drag region or toolbar gap; macOS reserves the LVRS drag surface only for its full-size content title bar in normal-window mode and removes it in full screen
- Initial canvases are created inside the workspace with proportional top, side, and bottom margins so the dark workspace remains visible, while new canvases use the dimensions entered in the toolbar modal
- Large new canvases are automatically zoomed down on creation so the full canvas remains visible inside the current workspace viewport

## Community and Contributing

- Join [GitHub Discussions](https://github.com/iisacc-Justmoong/Congregation/discussions) for general feedback, questions, and ideas.
- Report reproducible defects through [GitHub Issues](https://github.com/iisacc-Justmoong/Congregation/issues).
- Help validate Congregation 1.0.0 through the [Windows 10/11 testing issue](https://github.com/iisacc-Justmoong/Congregation/issues/new).
- Read [CONTRIBUTING.md](CONTRIBUTING.md) before proposing code, QML, documentation, or release-workflow changes.

## Testing
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Congregation tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```

## Known Limitations
- PSD import reads 8-bit RGB/RGBA raster layers, layer names, bounds, opacity, visibility, Photoshop blend-mode keys, and Congregation XMP manifests when present; Photoshop smart objects, adjustment layers, layer effects, and editable text/vector reconstruction still fall back to raster compatibility data
- Transformable image objects remain session overlays when present, while added paint layers own their own raster pixels for deletion and export
- Palette is fixed to the built-in colors
- Canvas dimensions are clamped to the supported raster size range when entered through the new-canvas modal

### 로컬 SDK 경로

SDK 소스는 `Workspace/SDK`, 설치본은 `~/.local/SDK/<패키지명>`을 사용한다.
CMake 의존성 힌트와 실행 RPATH, `build.sh`의 라이선스·업데이트 라이브러리
기본 prefix도 이 경로를 따른다. 설치 위치가 바뀌면 `build/`에서 CMake를
재구성한 뒤 전체 빌드와 CTest를 실행한다.
