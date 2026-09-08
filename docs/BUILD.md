# Congregation 1.0.0 Packaging Guide

This document captures the end-to-end steps needed to turn the `Congregation` build tree into an App Store–compliant macOS package for Congregation 1.0.0, a Windows runtime package, plus the Linux TGZ package. It assumes you have the Qt dependencies referenced in the repository README installed.

The active project uses the bundle identifier `com.iisacc.congregation.painter` and the canonical repository `https://github.com/iisacc-Justmoong/Congregation`. Release `1.0.0` starts at macOS build `1`, QML module `1.0`, and Windows file/MSIX version `1.0.0.0`. The pre-rename project, its generated packages, and its signed provisioning profile are retained in `Product/.Deprecated`. Configure a fresh repository-local `build/` for this project. App Store distribution requires a profile issued for the new identifier; production licensing and update services must register the new product identifier before online release validation.

Windows MSI starts a separate Congregation upgrade family with UpgradeCode `31432BB7-5B11-55D5-8AE0-CD97E6B1D95F` and installation-context component GUID `05DF7FB6-91BE-5013-AE84-E4A6496550E8`. These UUIDv5 values are derived from the canonical repository URL with the `#msi-upgrade` and `#msi-installation-context` suffixes. Keep them stable across subsequent Congregation versions so that the new `1.0.0` series does not replace the archived product's installation.

## 1. Prerequisites
- Apple Developer Program membership with access to App Store Connect.
- Certificates downloaded in Keychain Access:
  - **Developer ID Application** and **Developer ID Installer** for notarized distribution outside the Mac App Store.
  - **Apple Distribution** (or legacy *3rd Party Mac Developer Application*).
  - **Apple Installer** (or legacy *3rd Party Mac Developer Installer*).
- A `notarytool` credential profile for Developer ID notarization.
- A macOS App Store provisioning profile that matches your bundle identifier.
- Xcode command-line tools (`xcode-select --install`) and Transporter from the Mac App Store.
- Qt toolchain (Core, Network, Qml, Quick, QuickControls2, Svg) available in your `PATH` so that `macdeployqt` is callable, plus Git for the pinned QtKeychain FetchContent checkout.
- iiPaintEngine installed under `$HOME/.local/SDK/iiPaintEngine` or available through `CMAKE_PREFIX_PATH` as `iiPaintEngine::iiPaintEngine`.
- iiSharedCanvas 0.10.0 (exact package version) installed under `$HOME/.local/SDK/iiSharedCanvas` or selected with `IISHAREDCANVAS_PREFIX`, exporting `iiSharedCanvas::iiSharedCanvas`.
- iiUpdateManager 0.2 installed under `$HOME/.local/SDK/iiUpdateManager` or selected with `IIUPDATEMANAGER_PREFIX`, exporting `iiUpdateManager::iiUpdateManager`.
- iiLicenseManager 0.2 installed under `$HOME/.local/SDK/iiLicenseManager` or selected with `IILICENSEMANAGER_PREFIX`, exporting `iiLicenseManager::iiLicenseManager` and its installed libsodium third-party notice.

## 1a. Automated macOS Build Script
`./build.sh` defaults to the Developer ID distribution flow. It validates the Developer ID application and installer identities plus notarization credentials before configuring, builds in `build/`, runs `ctest --test-dir build --output-on-failure`, deploys the Qt runtime, signs the complete app tree with hardened runtime and a trusted timestamp, creates `dist/Congregation.pkg`, submits that package to Apple's notary service, staples the accepted ticket, and verifies the final package with `pkgutil`, `stapler`, and Gatekeeper. The canonical `dist/Congregation.pkg` path is published only by this signed and notarized flow.

Use `./build.sh local` for local development validation. Local mode signs `dist/Congregation.app` with `LOCAL_APP_CERT`, the first valid `Apple Development` identity, or an ad-hoc signature, then creates the explicitly non-distributable `dist/Congregation-local-unsigned.pkg` and `dist/Congregation-appstore-local-unsigned.pkg`. It never overwrites `dist/Congregation.pkg` or `dist/Congregation-appstore.pkg`. The component-built local and App Store package gates require the requested identifier and version on the Distribution `product`; only the legacy Developer ID `pkgbuild --package` flow validates that identity on its component `pkg-ref`, so an unrelated component cannot stand in for the current product identity.

The macOS workflow is incremental and preserves `build/`, including the disconnected `psd_sdk` and QtKeychain FetchContent checkouts. Use `./build.sh --clean` for a clean Developer ID distribution build, or `./build.sh --clean local` for clean local validation, only after changing toolchains or when recovering a stale cache. Local packages keep symbols with `macdeployqt -no-strip`; Developer ID, Mac App Store, and combined distribution modes omit `-no-strip` so release bundles are stripped by the deployment tool. `MACDEPLOYQT_NO_STRIP=0|1` remains an explicit override.

QtKeychain 0.17.0 is fetched at immutable commit `875f77d9f61bd97fd84cca47ce3bc71186dfbd09` only on macOS and Windows. CMake forces a static build with translations, demo/test applications, and QtKeychain's own CTest tree disabled. Congregation explicitly sets `insecureFallback(false)` for every read/write/delete job, so unsupported or denied secure storage never becomes a `QSettings` plaintext secret. macOS uses Keychain and Windows builds enable Credential Store; Linux deliberately stays manual-per-launch. The upstream BSD-3-Clause `COPYING` bytes are bundled at `Contents/Resources/legal/QtKeychain/QtKeychain-BSD-3-Clause.txt` on macOS and staged as `legal/QtKeychain/COPYING.txt` in Windows ZIP/MSI/MSIX content.

After `macdeployqt`, the script removes absolute build-machine `LC_RPATH` entries from every bundled Mach-O with `install_name_tool`, requires the linked `libiiUpdateManager*.dylib` and `libiiLicenseManager*.dylib` under `Contents/Frameworks`, then inspects every Mach-O file with `otool -L` and every remaining `LC_RPATH` with `otool -l`. Packaging fails if either runtime is absent, has the wrong architecture/deployment target, or a deployed binary still references an absolute non-system dependency or RPATH. The `.pkg` payload is inspected for both runtimes before publication, and the app resources retain iiLicenseManager's libsodium ISC notice.

Each signed app stage also receives `IISACCDistributionChannel` in `Info.plist` before code signing: `direct` for local/Developer ID stages and `appstore` for the Mac App Store stage. Congregation treats the `appstore` marker or a non-empty App Store receipt as store-managed and suppresses the external updater. Windows Store/MSIX packaged context is detected at runtime through `GetCurrentPackageFullName` and is equally store-managed; direct ZIP/MSI builds remain eligible for the explicit manual updater.

The macOS build RPATH contains only the platform-specific LVRS and iiPaintEngine runtime directories. Generic Unix `$HOME/.local/SDK/<dependency>/lib` fallback directories are limited to non-Apple Unix builds because adding both layouts on macOS leaves an unused absolute RPATH behind after `macdeployqt` rewrites the paths that actually resolved linked libraries.

The portability audit accepts only the indented dependency rows from `otool -L`, so repeated filename headers for universal Mach-O architectures are not mistaken for dependencies. It also distinguishes a dylib's own `LC_ID_DYLIB` value from its dependency list, so an absolute self-identifier is not reported as an external dependency. Congregation does not link Qt SQL; `macdeployqt` may nevertheless copy the complete Qt SQL driver set while traversing QML imports, so the script removes `Contents/PlugIns/sqldrivers` before auditing and signing. This also prevents unused ODBC, PostgreSQL, or Mimer plugins from retaining machine-specific client-library paths.

The script is expected to run on the macOS system Bash 3.2 with `set -u` enabled and no custom extra CMake arguments. Neither the default distribution build nor explicit local mode requires callers to define `CMAKE_EXTRA_ARGS` or any other optional argument array. Because `build.sh` is the official packaging entry point, it is tracked source and must not be ignored by `.gitignore`. Congregation's marketing version is fixed to `1.0.0`, while macOS `CFBundleVersion` is the monotonically increasing App Store build number `1`; the in-app `Congregation 1.0.0` label remains the product-family marketing name.

Congregation intentionally supports only the repository-local `build/` CMake binary directory. Configure, build, test, and package commands must use `-B build`, `cmake --build build`, and `ctest --test-dir build`; alternate build trees are rejected during CMake configure so CLion and shell workflows cannot silently produce stale bundles elsewhere.

On a Darwin host, CMake sets the macOS 12 deployment target before `project()` performs compiler detection. This keeps compiler feature checks, FetchContent targets, and the final bundle on the same minimum-OS contract instead of adding the flag only after the first try-compile.

On Windows, CMake links `Congregation.exe` with the Windows GUI subsystem, so launching it does not allocate a console window and the `QGuiApplication` event loop owns the process lifetime. CMake also compiles `resources/windows/Congregation.manifest.in` and `resources/windows/Congregation.rc.in` into the executable. These resources carry the application icon, four-part file/product version, `asInvoker` execution level, Windows 10/11 compatibility, Per-Monitor V2 DPI awareness, and long-path awareness. The build copies `LVRS.dll`, `libiiPaintEngine.dll`, `libiiSharedCanvas.dll`, the installed `iiUpdateManager.dll`/`libiiUpdateManager.dll`, and the installed `iiLicenseManager.dll`/`libiiLicenseManager.dll` plus the runtime DLLs found next to the selected MinGW compiler into `build/`; it does not search an unrelated system `PATH` for a different compiler runtime. Windows staging requires exactly one shared-canvas DLL, one updater DLL, and one license-manager DLL, includes all three in PE import-closure, strip, and Authenticode gates, and fails before ZIP/MSI publication if any is absent. This keeps direct CLion or shell launches from the `build/` tree aligned with the selected Qt kit without partially redeploying Qt itself; a missing dependency dialog from `build/Congregation.exe` means the build tree is stale and should be rebuilt with `cmake --build build`.

Every mode verifies that `CFBundleIconFile` resolves to `Contents/Resources/Appicon.icns`, that the legacy `Contents/Resources/icon.icns` file is not present, and that the bundled icon matches `resources/Appicon.icns`. All generated `.pkg` payloads are inspected before the build exits, so a stale installer package that still contains `icon.icns` fails the build instead of installing the old app icon.

`resources/Appicon.icns` is the canonical macOS icon source. After replacing it, regenerate the App Store/Xcode asset catalog before building:

```bash
tools/sync_app_icon_assets.sh
```

The script rewrites `packaging/macos/Congregation.xcassets/AppIcon.appiconset`, including `AppIcon-1024.png`, from the canonical `.icns`. The CMake bundle target also removes stale `Contents/Resources/icon.icns` after each macOS build, so an incremental `build/` bundle cannot keep advertising the removed legacy icon.

The default Developer ID flow and explicit alternatives are:

```bash
./build.sh
CONGREGATION_BUILD_MODE=devid ./build.sh
./build.sh local
CONGREGATION_BUILD_MODE=mas ./build.sh
CONGREGATION_BUILD_MODE=all ./build.sh
```

`devid` requires `Developer ID Application` and `Developer ID Installer` certificates plus notarization credentials. `mas` requires `Apple Distribution` and `3rd Party Mac Developer Installer` certificates. `all` runs both distribution flows. For Developer ID notarization, prefer `NOTARY_KEYCHAIN_PROFILE`; if Apple ID mode is used, provide the app-specific password through `NOTARY_APP_PASSWORD` rather than storing it in the script. The default `notary-main` profile can be created without placing the app-specific password in shell history:

```bash
xcrun notarytool store-credentials notary-main \
  --apple-id "<Apple ID>" \
  --team-id "5U49ST9XZH"
```

Because `--password` is omitted, `notarytool` requests it through a secure prompt and validates the credentials before storing them in Keychain. Every Developer ID run also validates notarization credentials before configuring CMake, preventing a long signed build from ending at a missing or invalid profile.

## 1b. Windows Build, Package, and Current-User Install Script
Run the Windows build from Windows PowerShell 5.1 or newer. The script expects Windows-built Qt, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager prefixes; macOS `.local` binaries cannot be reused on Windows. The default SDK installation root is `$HOME\.local\SDK`, matching the CMake hints, PowerShell build defaults, and Windows CI workflows. Explicit per-SDK prefix variables remain supported. Set them once per shell session:

```powershell
$env:QT_PREFIX = "C:\Qt\6.8.3\mingw_64"
$env:LVRS_PREFIX = "$HOME\.local\SDK\LVRS"
$env:IIPAINTENGINE_PREFIX = "$HOME\.local\SDK\iiPaintEngine"
$env:IISHAREDCANVAS_PREFIX = "$HOME\.local\SDK\iiSharedCanvas"
$env:IIUPDATEMANAGER_PREFIX = "$HOME\.local\SDK\iiUpdateManager"
$env:IILICENSEMANAGER_PREFIX = "$HOME\.local\SDK\iiLicenseManager"
```

The supported MinGW setup uses Qt's bundled CMake, Ninja, and MinGW tools on `PATH`. The vendored `psd_sdk` build treats `Psdminiz.c` as C++ because that upstream C file includes headers with C++ `static_assert` declarations; the narrow `-Wno-pragmas` exception applies only to that upstream target because the file is also included by other SDK translation units. Windows MinGW builds retain Release optimization, function/data sections, linker garbage collection, and release symbol stripping but disable IPO for Congregation and the PSD SDK: MinGW 13 does not propagate its LTO plugin consistently through Ninja's archive and generated-QML steps, which otherwise produces plugin diagnostics, serial LTRANS fallback, and unreliable links. Other toolchains continue to use the LVRS IPO policy. The current iiPaintEngine Qt adapter exposes the brush opacity toggle as `brushOpacityEnabled`, so QML and bridge code should not use the older `pressureToOpacityEnabled` property name.

For a local build, test, and staged runtime without a distributable package, use:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -SkipPackage
```

Public Windows packages are fail-closed and require Windows SDK SignTool plus a Code Signing certificate whose private key is available through the Windows certificate store, hardware token, or configured KSP. Select the certificate by its exact 40-hex store thumbprint; the thumbprint identifies the certificate while file and timestamp digests remain SHA-256. The default store is `CurrentUser\My`; add `-SigningCertificateStoreLocation LocalMachine` only when the protected key is installed in `LocalMachine\My`. Windows PowerShell 5.1 can expose the Code Signing EKU object identifier as a string while other certificate providers expose an object with a `Value` property; the signing preflight normalizes both representations before enforcing EKU `1.3.6.1.5.5.7.3.3`. The policy contract runs under both Windows PowerShell 5.1 and PowerShell 7 when `pwsh.exe` is installed. For consumer distribution, release operations must use a certificate chaining to a consumer-trusted code-signing root or a trusted signing service. After any interrupted-publication recovery, but before clearing partial artifacts or producing new package output, the signing preflight rejects self-signed identities, builds an online-revocation Code Signing chain, and requires its terminal certificate to be in Windows' `LocalMachine\AuthRoot` public-root store. This blocks a private root manually trusted only on the development PC. A clean stock Windows installation remains the final consumer-trust check because root-program state and SmartScreen reputation can differ by image and update level.

```powershell
$env:CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT = "0123456789ABCDEF0123456789ABCDEF01234567"
$env:SIGNTOOL_PATH = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
$env:CONGREGATION_TIMESTAMP_URL = "http://timestamp.digicert.com"
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Sign
```

The release path accepts only Release or MinSizeRel, does not allow `-SkipTests`, requires the Code Signing EKU `1.3.6.1.5.5.7.3.3`, an accessible private key, a current certificate validity period, `DigitalSignature` permission when the Key Usage extension is present, and a reachable RFC 3161 timestamp service. It never falls back to unsigned output. To also create a signed Windows Installer package, make WiX Toolset 3.x available through `WIX_TOOLS_DIR`, `WIX`, or `PATH`, then add `-CreateMsi`. PFX password arguments are deliberately unsupported because command-line secrets are observable by other local processes. `SIGNTOOL_PATH` is the explicit override; automatic discovery considers protected Windows Kits locations rather than an arbitrary executable injected through `PATH`.

### Public Windows signing identity onboarding

The identity proof and protected-key activation must be completed by the legal person or authorized organization representative. Do not invent a Publisher name, send a private key or token PIN to a maintainer, or export the key into a PFX merely to automate this build.

1. Choose the Publisher identity before ordering. An individual OV certificate displays the verified legal personal name. An organization OV certificate displays a verified registered legal or trade name; `Congregation` cannot be used as the Publisher merely because it is the product name. EV is appropriate only when a customer, procurement policy, or another signing program explicitly requires it; it no longer provides an automatic Microsoft Defender SmartScreen bypass over OV.
2. Order an OV Code Signing certificate from a public CA that supports the applicant's country and legal form. For the current Windows-store SignTool flow, a CA-issued USB hardware token is the simplest option. A CA cloud HSM or another compliant token/KSP is also acceptable when its Windows provider exposes the certificate and protected private key through `CurrentUser\My` or `LocalMachine\My`.
3. Complete the CA's identity validation personally. An individual should expect government-photo-ID/video verification, legal-name and residential-address evidence, independent phone or email verification, the subscriber agreement, and a confirmation callback. An organization should expect legal existence and address records, the applicant's photo-ID/video check, proof of authority, an independently verifiable business phone/email, the subscriber agreement, and an approval callback. The CA may request additional current registry or operational documents.
4. Receive and initialize the hardware token, or activate the cloud HSM account, using the CA's instructions. Install only the CA/token vendor's signed middleware and Windows cryptographic provider. Keep multi-factor authentication enabled. Never disclose the PIN, recovery secret, private key, or cloud-signing credential.
5. Connect and unlock the token, then confirm that Windows can see a currently valid Code Signing certificate and accessible private key:

   ```powershell
   Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
     Where-Object { $_.HasPrivateKey } |
     Format-List Subject,Issuer,Thumbprint,NotBefore,NotAfter,HasPrivateKey
   ```

   If the provider installs it machine-wide, repeat the command against `Cert:\LocalMachine\My` and pass `-SigningCertificateStoreLocation LocalMachine` to the build.
6. Copy the actual 40-hex thumbprint without inventing or shortening it. Set only non-secret selectors and the CA's RFC 3161 URL, then close and reopen PowerShell so the persistent user environment is reloaded:

   ```powershell
   [Environment]::SetEnvironmentVariable(
     "CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT",
     "<actual-40-hex-thumbprint>",
     "User"
   )
   [Environment]::SetEnvironmentVariable(
     "CONGREGATION_TIMESTAMP_URL",
     "<CA-RFC3161-timestamp-URL>",
     "User"
   )
   [Environment]::SetEnvironmentVariable(
     "CONGREGATION_CORRESPONDING_SOURCE_URL",
     "https://<publisher-controlled-location>/Congregation-1.0.0-Corresponding-Source.zip",
     "User"
   )
   [Environment]::SetEnvironmentVariable(
     "CONGREGATION_CORRESPONDING_SOURCE_SHA256",
     "<actual-64-hex-source-archive-sha256>",
     "User"
   )
   ```

7. With the token connected and unlocked, create the replacement ZIP and MSI in one transaction:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Sign -CreateMsi
   ```

The release operator may provide a maintainer only the certificate thumbprint, exact `Subject`/Publisher text, store location, and public timestamp URL. Those values are not private-key material. The final release must publish the exact expected Publisher identity and SHA-256 sidecars, then install and launch the signed MSI on a clean, fully updated stock Windows machine before publication. Also confirm that every bundled project and third-party component has an explicit redistributable license and required notices; a public signature does not cure missing distribution rights.

### Windows distribution licenses and corresponding source

Every ZIP/MSI package stages `LICENSE.txt`, `THIRD_PARTY_NOTICES.txt`, `SOURCE_OFFER.txt`, and a `legal/` tree. The tree contains the LVRS, iiPaintEngine, and iiSharedCanvas AGPL texts, iiLicenseManager's installed libsodium ISC notice, QtKeychain BSD-3-Clause text, psd_sdk BSD-2-Clause and embedded-miniz Unlicense texts, Pretendard 1.3.9 OFL text, the deployed Qt module license collections and SPDX documents, and the exact GCC/MinGW-w64/winpthreads notices matched by hash to the three staged MinGW runtime DLLs. The packaging gate copies each source file byte-for-byte, verifies its SHA-256 after staging, and requires the same tree in the ZIP and MSI because WiX harvests the final stage.

Qt packaging requires the matching Qt `Sources` component and SBOM files from the selected kit. For Qt 6.8.3 they are resolved below `C:\Qt\6.8.3\Src` and `C:\Qt\6.8.3\mingw_64\sbom`. A complete Qt installer provides the five common license texts under `Src\LICENSES`; a selective `aqt install-src` layout may omit that root directory, so the packager accepts `Src\qtbase\LICENSES` only after verifying that the same five non-empty common texts are present. The SignPath workflow downloads source-license archives for every deployed Qt module, including `qttranslations`. A MinGW package also resolves the exact `C:\Qt\Tools\mingw*` directory by comparing the staged runtime DLL hashes before copying its notices; an unrelated toolchain's license directory is rejected.

The iiPaintEngine copyright holder has selected `AGPL-3.0-only`. Its repository now contains the AGPL v3 text at root `LICENSE`, identifies the SPDX license in its README, installs the text as `share/licenses/iiPaintEngine/LICENSE`, and protects the layout with a contract test. Congregation copies that exact file to `legal/iiPaintEngine/LICENSE.txt` and verifies the staged bytes. The temporary 2026 `-AllowUnsignedPackage` path is a public release only when the same complete corresponding-source and legal-material gates pass.

Before the first public build, create `Congregation-1.0.0-Corresponding-Source.zip` from clean, tagged Congregation, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager sources plus the exact Qt corresponding source required for the conveyed libraries. Publish it at a location controlled by the publisher, calculate `Get-FileHash -Algorithm SHA256`, and set `CONGREGATION_CORRESPONDING_SOURCE_URL` and `CONGREGATION_CORRESPONDING_SOURCE_SHA256` to those exact values. An upstream Qt download link alone is not treated as the release's corresponding-source evidence. The signing script accepts only an absolute HTTPS URL and a 64-hex SHA-256 and writes both into the installed `SOURCE_OFFER.txt`.

After creating and checking out the immutable release tag, generate that archive with `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\new-corresponding-source.ps1 -Version 1.0.0 -CongregationRevision v1.0.0 -IiSharedCanvasRevision $env:IISHAREDCANVAS_COMMIT -IiUpdateManagerRevision $env:IIUPDATEMANAGER_COMMIT -IiLicenseManagerRevision $env:IILICENSEMANAGER_COMMIT`. The tool resolves the tag and each pinned dependency, including iiSharedCanvas, iiUpdateManager, and iiLicenseManager, to an exact commit, archives Git revisions rather than mutable working-tree bytes, copies only the Qt 6.8.3 source modules conveyed by the Windows runtime, records per-file SHA-256 values, and writes the archive plus sidecar under `build/release-source/`. Local working-tree changes are reported and excluded. Git archives contain only tracked bytes, including legitimate tracked build-system source directories such as `psd_sdk/build/VS2019`; repository metadata is rejected, while the Qt filesystem copy separately excludes `.git` and generated `build` directories. The generated `BUILD-SOURCE.md` preserves its literal Markdown paths without PowerShell escape-character substitution. Output paths are rejected unless they remain under the required repository-local `build/` tree. The workflows authenticate the pinned private iiUpdateManager and iiLicenseManager checkouts through the repository secret named `IIUPDATEMANAGER_READ_TOKEN`, disable persisted checkout credentials, and verify both resolved commits before using them. They still require `IIUPDATEMANAGER_REPOSITORY` plus the reviewed 40-hex `IIUPDATEMANAGER_COMMIT` and `IILICENSEMANAGER_COMMIT`. Missing provenance or token state fails before source installation.

The website-release runner prepends the iiSharedCanvas build directory and the pinned iiPaintEngine, LVRS, and Qt runtime directories before running iiSharedCanvas CTest. On Windows, omitting those locations produces loader status `0xc0000135` before a test body starts even when the DLL compiled and linked successfully.

The independent iiUpdateManager build and test gate runs before the longer LVRS build so a private updater regression fails fast. Its freshly built DLL and Qt runtime directories are prepended to `PATH`; if CTest fails, the runner repeats the update-flow executable with per-test text output and prints that diagnostic before stopping. The runner also builds iiLicenseManager against a vcpkg `x64-mingw-static` libsodium installation, runs its CTest suite, and requires its installed third-party notice before configuring Congregation.

The temporary public unsigned MSI receives the `-unsigned` filename suffix so it cannot be confused with a signed release. It is restricted to Release or MinSizeRel, requires the full test suite and public source evidence, and expires automatically at the beginning of 2027:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -BuildType Release -AllowUnsignedPackage -SkipPackage -CreateMsi
```

The script always configures with `cmake -S . -B build`, preserves that repository-local tree for incremental work, builds with CMake's parallel mode, and runs `ctest --test-dir build --output-on-failure` unless `-SkipTests` is passed. General CTest validates source, authoring, and build-workflow contracts; it deliberately does not discover or inspect an MSI already present in `build/`, because such a file may belong to an earlier packaging run. When `-CreateMsi` is requested, the packaging workflow instead passes the exact newly linked `.partial.msi` path and the normalized three-field Windows Installer ProductVersion to the MSI database contract, then refuses to hash or publish it unless that explicit database gate passes. Use `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Clean -SkipPackage` for a clean-only local rebuild after changing toolchains or when recovering a stale build tree; add the appropriate signed or explicitly unsigned packaging flags when the same clean run should publish packages. Passing `-SkipTests` configures `BUILD_TESTING=OFF` and builds only the `Congregation` target. Runtime deployment uses `windeployqt --qmldir App/qml --translations en,ko`, requires the Qt Quick Shapes QML plugin used by the vector brush cursor, and omits the release-only QML debugger plugin group plus unused generic/Insight Tracker, SQL, Quick3D utility, PDF, Qt Virtual Keyboard, software OpenGL, and runtime D3D/DXC compiler payloads before it copies the LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager runtime DLLs. Congregation uses Qt's Direct3D backend with precompiled shaders and does not load those shader compilers at runtime. LVRS QML is compiled into the LVRS binary, so the stage deliberately removes any loose `qml/LVRS` directory emitted by deployment instead of copying thousands of duplicate source files or rewriting `qmldir` `prefer` directives.

Before packaging, the script verifies that the staged executable is a native AMD64 PE32+ Windows GUI binary, has the required ASLR/DEP flags, and exposes the expected file version, product version, and product name. MinGW Release and MinSizeRel stages strip the complete COFF symbol tables from `Congregation.exe`, `LVRS.dll`, `libiiPaintEngine.dll`, `libiiSharedCanvas.dll`, the updater runtime, and the license-manager runtime; Qt's deployed binaries are left untouched. The stage is also inspected with `objdump`; if a dependency such as `LVRS.dll` imports `__cxa_thread_atexit` while the staged `libstdc++-6.dll` does not export it, the script fails with a MinGW ABI error and the dependency must be rebuilt with the same MinGW kit as Qt. The same pass checks the PE import closure of every staged executable and DLL against the staged payload and Windows system DLL set, so unresolved third-party imports fail before ZIP or MSI creation.

After deployment, removal, strip, PE validation, and ABI/import validation have completed, signed mode signs `Congregation.exe`, `LVRS.dll`, `libiiPaintEngine.dll`, the shared-canvas runtime (`iiSharedCanvas.dll` or `libiiSharedCanvas.dll`), the updater runtime (`iiUpdateManager.dll` or `libiiUpdateManager.dll`), and the license-manager runtime (`iiLicenseManager.dll` or `libiiLicenseManager.dll`) with the exact selected certificate even if those files arrived with another valid signature. It preserves valid timestamped vendor signatures on other PE files and Authenticode-signs the remaining unsigned staged `.exe` and `.dll` files with `signtool sign /fd SHA256 /tr <url> /td SHA256`. It then requires `signtool verify /pa /all /tw` to succeed for every staged PE and rechecks the selected certificate thumbprint on Congregation-owned files before compression.

Only the selected package's temporary `.partial` files are cleared before a run. ZIP and MSI bytes are generated under `.partial` names, verified, and hashed with their final recorded filename while the previous canonical artifact remains untouched. Publication first writes a same-volume `prepared` transaction journal atomically, including whether each canonical artifact/checksum pair was validly present or completely absent. It then moves every last-known-good pair to `.previous`, promotes the complete verified set, verifies every promoted pair again, and atomically changes the journal to `committed` before deleting backups and the journal last. A retry reconciles that journal before signing-tool discovery, cleanup, or `-Clean`: `prepared` restores every old pair and removes newly introduced first-publication files, while `committed` is accepted only when every final pair remains valid. When ZIP and MSI are requested together, neither canonical file changes until both partial packages have passed their respective signing and validation gates. One journal per version and signed/unsigned flavor serializes ZIP-only, MSI-only, and combined publication because those modes share canonical files; an existing journal is matched only against the fixed ZIP/MSI allowlist, while a journal-free run examines only the package types requested by that invocation. Recovery examines final and backup files by SHA-256 and selects only an internally consistent artifact/checksum pair, including after a process interruption leaves a one-sided backup. Signed and unsigned outputs therefore do not delete or mix with one another. The ZIP is built from the final signed stage, while WiX builds the MSI from that stage and the MSI container itself is signed and verified before publication. The linked partial MSI must also pass the database contract test before hashing or promotion, so its UI, features, license, and transactional upgrade order are release gates rather than post-build observations. Heat, Candle, and Light run with warnings treated as errors and no ICE suppression; Smoke additionally runs ICE105 alone with warnings promoted to errors so the dual-context contract cannot be skipped by a tool default. Every ZIP and MSI receives an adjacent `.sha256` record; publish that record through the authenticated release channel because a checksum alone proves equality, not publisher identity. Authenticode covers the staged PE files and the signed MSI container, not the ZIP container or its loose QML/resources, so the authenticated sidecar hash binds the complete ZIP. The staged app is written to `dist/Congregation-Windows`, the signed ZIP to `dist/Congregation-1.0.0-Windows.zip`, and the signed MSI to `build/Congregation-1.0.0-Windows.msi`. Explicit unsigned smoke artifacts use `Congregation-1.0.0-Windows-unsigned.*` instead. Release notes must state the expected Publisher subject or certificate thumbprint so users can distinguish Congregation's signer from an unrelated valid signer. The Windows Installer 5 MSI authors the official `ALLUSERS=2` and `MSIINSTALLPERUSER=1` defaults, so it defaults to the current user under Local Application Data and starts a separate Congregation upgrade family; its advanced options also offer an all-users installation under native 64-bit Program Files, which requires elevation. A required context-aware `installation-context marker` records the selected scope and resolved install location. Marker discovery runs before `FindRelatedProducts`, locks later upgrades to the registered scope, and preserves a custom all-users directory. A markerless per-user upgrade detected through `WIX_UPGRADE_DETECTED` is locked to current-user scope as a legacy fallback. Unattended upgrades must not override `ALLUSERS` or `MSIINSTALLPERUSER`: Windows Installer enumerates related products in the active installation context, so an explicitly forced opposite context cannot use the interactive markerless fallback. Ambiguous simultaneous per-user and per-machine registrations block new installation and cross-context upgrade, while maintenance and removal remain available for recovery. The MSI uses a deterministic ProductCode for the same version and architecture; a different version or architecture receives a different ProductCode, preventing same-version rebuilds from creating duplicate product registrations. The separate `-InstallForCurrentUser` unpackaged smoke path remains under Local Application Data. Public corrections must increment the first three ProductVersion fields, so a later Congregation release upgrades 1.0.0 while another 1.0.0 package retains the installed product identity. Every later upgrade must use the same installation context selected for the installed product. `RemoveExistingProducts` runs after `InstallInitialize`, keeping removal of the old product inside the Windows Installer transaction so a failed replacement can roll it back.

A machine-wide Congregation packaging mutex rejects a concurrent script invocation before either process can mutate a build tree or publication journal, including when the same repository is reached through a junction, symbolic link, mapped drive, or UNC alias.

### Windows Installer UI contract

The WiX MSI exposes the Congregation application files as a required core feature and installs one context-aware Start Menu shortcut with the executable. It defaults to the current user and therefore requires no elevation; choose **Advanced** on the first installation to select current-user or all-users scope and, for all-users scope, choose the installation directory. Current-user scope is rooted below Local Application Data, while all-users scope uses the machine's native `%ProgramFiles%\Congregation`, a shared Start Menu, and requires elevation. The shortcut is authored as an advertised entry owned by `Congregation.exe`, then materialized as a normal shell shortcut with `DISABLEADVTSHORTCUTS=1`; this keeps one executable-backed component valid under ICE38, ICE43, ICE57, and ICE105 in both contexts. On an upgrade, the stored installation-context marker overrides any opposite scope choice and reuses the recorded install location before related-product detection. The required core runtime cannot be deselected into an unusable package. Packaging converts the repository root `LICENSE` into the MSI's RTF license control with Unicode-safe escaping, so the wizard presents the actual GNU AGPL terms and never WiX's placeholder text; a Windows RichEdit round-trip test protects the converter.

Windows Installer identifies an installed product through its registered product identity. Reopening the same MSI after a successful installation therefore enters maintenance mode by design; it must not be described or tested as another first-time **Install** flow. The maintenance wizard exposes the standard **Change**, **Repair**, and **Remove** paths: **Change** displays the installed required feature state, **Repair** restores missing or damaged installed files and the shortcut, and **Remove** uninstalls the product. First-install scope and destination choices are intentionally not offered again in maintenance. To exercise that contract again or change between current-user and all-users scope, remove the registered product first. A higher three-field ProductVersion performs the major upgrade only in the same installation context; republishing different bytes with the same ProductVersion is prohibited.

Do not publish the generic Windows CPack output. It does not run `windeployqt`, staged PE closure checks, or Authenticode signing and is deliberately named `Congregation-1.0.0-Windows-unsigned-cpack-incomplete.zip`.

Audit the immutable release archive rather than the mutable staging directory. Compare the sidecar hash, extract the canonical ZIP into a fresh temporary directory, and inspect the embedded executable:

```powershell
$zip = (Resolve-Path .\dist\Congregation-1.0.0-Windows.zip).Path
$expectedHash = ((Get-Content "$zip.sha256" -Raw).Trim() -split '\s+')[0]
$actualHash = (Get-FileHash -Algorithm SHA256 $zip).Hash
if ($actualHash -ne $expectedHash) { throw "Release ZIP checksum mismatch" }
$auditDir = Join-Path $env:TEMP ("Congregation-1.0.0-release-audit-" + [Guid]::NewGuid().ToString("N"))
Expand-Archive -LiteralPath $zip -DestinationPath $auditDir -Force
Get-AuthenticodeSignature "$auditDir\Congregation.exe" | Format-List Status,SignerCertificate,TimeStamperCertificate
& $env:SIGNTOOL_PATH verify /pa /all /tw /v "$auditDir\Congregation.exe"
```

A consumer-trusted Authenticode chain prevents the publisher identity from being shown as unknown, but Microsoft Defender SmartScreen also evaluates publisher reputation and per-file reputation, so a new release hash may still warn. A signature reported as valid only because the build machine trusts a private root is not evidence that consumer Windows installations trust it; validate the release on a clean stock Windows machine or equivalent isolated runner. Self-signed certificates remain untrusted on ordinary consumer machines and do not solve public publisher identity.

Normal Windows startup does not open or flush a diagnostic file. `Main.qml` uses LVRS stock application-window geometry and does not override width, height, or minimum dimensions. Before the first show, the common Qt entry point measures the hidden LVRS window's aspect ratio, requests a width of 1,280 logical pixels, derives the height from that aspect ratio, and scales both dimensions down together only if the selected screen cannot contain them. It then shows the completed window and does not resize the window after it becomes visible. The same pre-show rule prevents Cocoa and Linux compositors from visibly correcting an oversized first frame. The asynchronous canvas `Loader` therefore cannot renegotiate the top-level size; users can still resize or maximize the window after launch. For an opt-in startup timing trace, set `$env:CONGREGATION_STARTUP_TRACE = "1"` before launching `Congregation.exe`; startup milestones are then appended to `%TEMP%\Congregation-startup.log`. A fatal root-QML object creation failure is written to that file and flushed even when tracing is disabled, but ordinary Qt/QML warnings are not redirected there. If Windows reports that the procedure entry point `__cxa_thread_atexit` cannot be found in `LVRS.dll`, that failure happens before Congregation reaches application logging and indicates a platform runtime mismatch, not a QML load failure. Rebuild LVRS with the same Qt MinGW kit used for Congregation, rerun `build-windows.ps1 -Clean -SkipPackage`, and regenerate the installer from the refreshed `dist/Congregation-Windows` payload.

For a current-user smoke install without creating a package, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -SkipPackage -InstallForCurrentUser
```

That copies the staged runtime to `%LOCALAPPDATA%\Programs\Congregation` by default and creates a shortcut in the current user's Start Menu. `-InstallDir <path>` may override the location only below the current user's Local Application Data directory. An existing non-empty target must contain Congregation's ownership marker or a versioned `Congregation.exe`; drive roots, user data roots, reparse points, unrelated directories, and the staged source are rejected before recursive deletion. If the Qt kit is MSVC-based and `cl.exe` is not already in `PATH`, the script tries to load the Visual Studio C++ build environment through `vswhere.exe`; otherwise, launch it from a Developer PowerShell for VS.

### Prepared website release path through SignPath Foundation

The no-cost website distribution path is prepared to use SignPath Foundation open-source sponsorship after the Foundation explicitly approves Congregation. No SignPath Foundation certificate is currently active for the project. Through December 31, 2026, `.github/workflows/windows-signpath-release.yml` therefore builds an explicitly named unsigned website MSI when `submit-for-signing` is false. The unsigned path retains the full test, corresponding-source, legal-material, PE closure, MSI database, and checksum gates, but it cannot provide Authenticode publisher identity and Windows may show Unknown publisher or SmartScreen warnings. After approval, the same workflow can submit the separately isolated SignPath input and verify both the returned MSI and nested executable signatures.

`build-windows.ps1 -ExternalSigning -SkipPackage -CreateMsi` remains deliberately separate from `-AllowUnsignedPackage`. Both modes accept only Release or MinSizeRel, require the complete test suite and public corresponding-source evidence, and are MSI-only in CI. The unsigned mode creates `build/Congregation-1.0.0-Windows-unsigned.msi`; external signing creates the non-distributable `build/signpath-input/Congregation-1.0.0-Windows.msi`. The modes cannot be combined.

Before enabling submission, the repository owner must:

1. obtain approval for Congregation from the SignPath Foundation OSS program;
2. install the SignPath GitHub App for this repository and retain MFA on GitHub and SignPath;
3. configure a SignPath artifact configuration that signs the Congregation-owned `Congregation.exe`, iiUpdateManager runtime, and iiLicenseManager runtime inside the MSI and then signs the MSI container, while leaving upstream Qt, LVRS, iiPaintEngine, and MinGW binaries under their own publisher identities or unsigned as permitted by the OSS policy;
4. require manual approval in the SignPath release signing policy;
5. configure the repository variables `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG`, `SIGNPATH_SIGNING_POLICY_SLUG`, `SIGNPATH_ARTIFACT_CONFIGURATION_SLUG`, `CONGREGATION_CORRESPONDING_SOURCE_URL`, `CONGREGATION_CORRESPONDING_SOURCE_SHA256`, and the reviewed `IILICENSEMANAGER_COMMIT`, plus the `SIGNPATH_API_TOKEN` repository secret;
6. dispatch the workflow from an immutable release tag with `submit-for-signing` enabled.

Until then, dispatch the workflow from the exact reviewed commit with `submit-for-signing` disabled and publish only the `Congregation-website-release-unsigned` artifact after recording its SHA-256 in iisacc.com. The separate `windows-corresponding-source.yml` workflow produces the archive and sidecar that must be attached to the authenticated GitHub release before the MSI build variables are updated.

The workflow requests only Qt 6.8.3 modules that the online installer exposes separately; Qt SVG remains part of the base desktop package. LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager are installed in separate observable steps, with the shared-canvas and license-manager test suites run before Congregation configuration. Congregation 1.0.0 pins the iiPaintEngine revision that disables pointer-event coalescing and performs touched-sample live-preview updates, together with the matching large-canvas iiSharedCanvas renderer revision, so the Windows package matches the macOS interaction behavior. The pinned iiPaintEngine installer configures single-configuration generators as Release so its exported target contains a usable `IMPORTED_IMPLIB_RELEASE` for the Congregation Release configure. The GitHub-hosted runner builds the pinned LVRS dependency as `MinSizeRel` and disables both its general IPO switch and its platform build optimizations, because the latter independently enables per-configuration LTO and can trigger a MinGW 13 internal compiler error while linking the generated QML resources. Congregation itself is built as Release and runs the complete functional and packaging validation. The workflow uploads `Congregation-website-release-unsigned` only from the explicit unsigned branch and preserves the existing nested Authenticode verification for Congregation plus both manager runtimes before any future `Congregation-website-release` signed artifact is produced.

### Prepared no-cost website release path through Necessary

Congregation submitted an application to [Necessary Code Signing](https://sign.necessary.nu/) on 2026-07-30. The service accepted Congregation as an eligible real utility and requested maintainer identity and address verification before issuing access. Its public health endpoint reported both HSM and certificate availability, but the project token has not yet been issued. Therefore no current Congregation MSI may be described as publicly signed through this service. Project eligibility, an application receipt, or a healthy HSM is not evidence that any Congregation bytes carry a consumer-trusted signature.

This integration uses the upstream [osslsigncode 2.14 release](https://github.com/mtrojnar/osslsigncode/releases/tag/2.14), not a locally reimplemented Authenticode encoder. The project is actively maintained, licensed under GPL-3.0-or-later with its documented OpenSSL linking exception, and supports PE, MSI, detached signing, signature attachment, and RFC 3161 timestamping. The Windows x64 release archive is pinned to SHA-256 `9a1722aaf62a27852c4eb9c35749a0248065052d0ae0a93d4ed6bb49def027f2`; `.github/workflows/windows-necessary-release.yml` refuses to execute it when the downloaded bytes differ. It is release-only tooling and adds no library or network dependency to the installed application. Necessary remains an external service dependency whose free OSS access, certificate, availability, and revocation policy can change or be withdrawn.

After Congregation is approved, store the issued token only as the GitHub Actions repository secret `NECESSARY_SIGN_TOKEN`. Do not pass it on a command line, place it in a repository variable, print it, attach it to an artifact, or copy it into a maintainer document. Dispatch `Windows Necessary public release` manually from an immutable `v<project-version>` tag with `submit-for-signing` enabled. The workflow remains fail-closed when the secret is absent, the selected ref is not a matching tag, the corresponding-source evidence is missing, or any build/test/package check fails.

The workflow first runs `build-windows.ps1 -ExternalSigning -SkipPackage -CreateMsi` only to reuse its Release build, complete CTest run, legal-material gate, PE closure checks, WiX authoring, ICE105, and MSI database validation. The resulting `build/signpath-input` file remains unsigned internal input and is never published by the Necessary path. The workflow then uses `tools/necessary-authenticode.ps1` to extract an SHA-256 Authenticode payload from each Congregation-owned or otherwise unsigned staged PE. Only that detached payload is posted over HTTPS to the fixed `https://sign.necessary.nu/windows/sign` endpoint; the bearer token is supplied only as the request authorization header. The helper attaches the returned signature to a temporary copy, adds an independent SHA-256 RFC 3161 timestamp, and leaves the original untouched unless Windows reports a valid public-trust signature from `Necessary Innovations AB` and Windows SDK SignTool accepts `/pa /all /tw`.

All Necessary-signed files in one run must use the same certificate thumbprint. Existing valid upstream signatures are retained, while `Congregation.exe`, `LVRS.dll`, and `libiiPaintEngine.dll` are rebound to the approved release certificate. `tools/necessary-windows-release.ps1` owns the shared local/CI orchestration: it recompiles the existing verified WiX sources and relinks the MSI from the signed stage, reruns Smoke ICE105, signs a temporary MSI candidate through the same detached flow, and reruns the MSI database contract. It then creates an administrative image and independently verifies the timestamp, Publisher, certificate thumbprint, and SignTool policy for all three Congregation-owned installed binaries. A prior canonical MSI and checksum remain untouched when any candidate check fails; only a completely verified candidate is atomically published to `build/Congregation-<version>-Windows.msi` with its matching SHA-256 sidecar. Only that file, or the identical unarchived `Congregation-website-release` workflow artifact, is a candidate for clean-machine install/launch/remove testing and later publication. The expected Windows Publisher is `Necessary Innovations AB`, because a sponsoring service cannot legitimately make its certificate claim that the unregistered product name Congregation is the certificate subscriber.

After the provider issues a token, the same release can be completed locally without placing the token in command history. First run the external-signing build above with the matching public corresponding-source URL and SHA-256. Make the token available only as a process-scoped `NECESSARY_SIGN_TOKEN`, resolve the pinned `osslsigncode.exe`, Windows SDK `signtool.exe`, and WiX 3.14 directory, then invoke:

```powershell
. .\tools\necessary-authenticode.ps1
. .\tools\necessary-windows-release.ps1
$result = Invoke-CongregationNecessaryWindowsRelease `
    -RepositoryRoot $PWD.Path `
    -BuildDirectory (Join-Path $PWD.Path "build") `
    -StageDirectory (Join-Path $PWD.Path "dist\Congregation-Windows") `
    -WixToolsDirectory $wixToolsDirectory `
    -OsslSignCodePath $osslSignCodePath `
    -SignToolPath $signToolPath `
    -SigningToken $env:NECESSARY_SIGN_TOKEN `
    -TimestampUrl "http://timestamp.digicert.com"
$result | Format-List
```

Do not assign a literal token in the shell command or persist it in a user/machine environment variable. Clear the process-scoped value immediately after the run. The local command refuses alternate build/stage directories, preserves valid timestamped vendor signatures, signs only Congregation-owned or otherwise unsigned PE files, binds every new signature to the first returned certificate thumbprint, and withholds the canonical MSI until all outer and nested checks pass.

## 1c. Microsoft Store MSIX

Microsoft Store MSIX is Congregation's certificate-free public Windows distribution route. Microsoft re-signs an MSIX after certification, so the publisher does not buy, renew, export, or protect a public OV/EV certificate and customers do not receive a SmartScreen unknown-publisher warning for the Store installation. This applies to an actual MSIX submission only. Microsoft does not re-sign an MSI or EXE submitted through the separate Win32 installer path. See Microsoft's current [Windows code-signing options](https://learn.microsoft.com/windows/apps/package-and-deploy/code-signing-options) and [manual desktop MSIX packaging guide](https://learn.microsoft.com/windows/msix/desktop/desktop-to-uwp-manual-conversion).

`build-windows-store.ps1` owns this workflow separately from the non-Store ZIP/MSI signing path. It reuses the tested `dist/Congregation-Windows` runtime, writes the Partner Center identity and exact reserved app name into `AppxManifest.xml`, generates exact Store PNG assets from the canonical 1024 px icon, packages with the installed x64 Windows SDK MakeAppx, and publishes the Store files and SHA-256 sidecars under `dist/`. The reserved name drives both `Package/Properties/DisplayName` and `uap:VisualElements/@DisplayName`; Partner Center rejects an unreserved package-level name even when the package identity and publisher are correct. The manifest is x64, uses package version `1.0.0.0`, targets `Windows.Desktop` from build 19041, and declares `uap10:RuntimeBehavior="packagedClassicApp"`, `uap10:TrustLevel="mediumIL"`, `privateNetworkClientServer` for nearby Congregation discovery, and `runFullTrust`. The fourth version field is reserved for Store use and must remain zero.

### Local self-signed MSIX verification

Self-signing is valid only for development or centrally managed private devices. It does not make a download publicly trusted. Development output is therefore isolated under `build/development-only`, carries a development-only identity, and must never be attached to a public release.

The development certificate Subject must exactly equal `CN=Congregation Development Local Only`, have the Code Signing EKU and an accessible private key in `CurrentUser\My`, and be selected by its exact 40-hex thumbprint. To install a self-signed MSIX, Windows App Installer additionally requires its public certificate in the local computer's `TrustedPeople` store. Import only the public `.cer`, not a PFX; this one-time trust operation requires an elevated PowerShell and should be removed when the development identity is retired. Do not place a leaf signing certificate in a Trusted Root store.

```powershell
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_DEVELOPMENT_SIGNING_CERTIFICATE_THUMBPRINT",
  "<actual-40-hex-development-thumbprint>",
  "User"
)

$thumbprint = [Environment]::GetEnvironmentVariable(
  "CONGREGATION_DEVELOPMENT_SIGNING_CERTIFICATE_THUMBPRINT",
  "User"
)
$certificate = Get-Item "Cert:\CurrentUser\My\$thumbprint"
New-Item -ItemType Directory -Path .\build\development-only -Force | Out-Null
Export-Certificate `
  -Cert $certificate `
  -FilePath .\build\development-only\Congregation-Development-Local-Only.cer `
  -Force
```

Then, from an elevated PowerShell, trust that public certificate for package testing:

```powershell
Import-Certificate `
  -FilePath .\build\development-only\Congregation-Development-Local-Only.cer `
  -CertStoreLocation Cert:\LocalMachine\TrustedPeople
```

Build, sign, timestamp, install, activate, observe a visible Congregation window, and remove the package with one command:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows-store.ps1 -Mode Development -InstallDevelopment
```

The command fails unless SignTool verifies the package and the installed tree contains the executable and legal notices. It launches through `shell:AppsFolder\<PackageFamilyName>!Congregation`, waits for a visible native window, stops only the process it launched, removes the development package, and confirms that registration is gone. `-SkipBuild` is allowed only for a repeated development check against an already tested runtime stage. The output is `build/development-only/Congregation-1.0.0-Windows-Sideload-Development-x64.msix`; its adjacent certificate is public-key material only, but neither file is a public release artifact.

### Free Store account and Product identity

The legal account holder must complete the identity-sensitive steps. Start at [storedeveloper.microsoft.com](https://storedeveloper.microsoft.com/) so the current free onboarding flow is used, then sign in or create the Microsoft account that will own the product. An individual account requires the account holder's government-issued photo ID and selfie verification and publishes under that verified individual identity. A company account requires authority over the organization and the business evidence Microsoft requests. Account type, legal contracts, identity proof, and the final Publish action cannot be delegated to a build script; an individual account cannot simply be converted into a company account later.

After verification:

1. Open Partner Center, choose **Apps and games**, **New product**, then **MSIX or PWA app**.
2. Search for and reserve the exact public display name for Congregation. No prior product's reservation establishes ownership of this new name; use the exact value returned by Partner Center. A reservation can expire if it is not used for a submission.
3. Open **Product management > Product identity**.
4. Copy these four values exactly, preserving case, spaces, commas, and punctuation:
   - Reserved app name for `Package/Properties/DisplayName`
   - `Package/Identity/Name`
   - `Package/Identity/Publisher`
   - `Package/Properties/PublisherDisplayName`
5. Store only those non-secret values in the current user's environment:

```powershell
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_STORE_DISPLAY_NAME",
  "<exact-reserved-app-name>",
  "User"
)
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_STORE_IDENTITY_NAME",
  "<exact-Package-Identity-Name>",
  "User"
)
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_STORE_PUBLISHER",
  "<exact-Package-Identity-Publisher>",
  "User"
)
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_STORE_PUBLISHER_DISPLAY_NAME",
  "<exact-PublisherDisplayName>",
  "User"
)
```

An invented identity or publisher produces a different package family, while an unreserved display name fails package validation. Partner Center rejects either case. The local development Publisher and display name are intentionally unrelated to the Store values and must not be substituted.

### Legal release gate

Store signing does not replace the publisher's license obligations. The iiPaintEngine copyright holder has authorized `AGPL-3.0-only`, and the installed engine package now supplies that exact license. Congregation's Store stage requires non-empty `legal/iiPaintEngine/LICENSE.txt`, all other staged third-party notices, and a corresponding-source offer for the exact release.

Create a complete source archive for the shipped Congregation, LVRS, iiPaintEngine, patched/build-required dependency sources, build scripts, and license material; publish it at an HTTPS location controlled by the publisher; and record the SHA-256 of those exact bytes. Configure the evidence as follows:

```powershell
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_CORRESPONDING_SOURCE_URL",
  "https://<publisher-controlled-host>/<exact-source-archive>",
  "User"
)
[Environment]::SetEnvironmentVariable(
  "CONGREGATION_CORRESPONDING_SOURCE_SHA256",
  "<exact-64-hex-SHA-256>",
  "User"
)
```

The Store mode fails closed before MakeAppx when the iiPaintEngine license, HTTPS URL, hash, legal notices, exact Product identity, current Release build, or tests are missing. It never turns a development package into a public package and never falls back to a placeholder.

### Create and submit the Store upload

After the Product identity and legal evidence are complete, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows-store.ps1 -Mode Store
```

This always rebuilds in Release mode and runs the complete test suite. It creates:

- `dist/Congregation-1.0.0-Windows-Store-x64.msix`, intentionally unsigned for Store ingestion;
- `dist/Congregation-1.0.0-Windows-Store-x64.msixupload`, a ZIP container holding exactly that x64 MSIX and no fake symbol archive;
- a `.sha256` sidecar for each file.

MinGW does not produce a Microsoft PDB, so `.appxsym` is deliberately omitted. Do not sign the Store upload with the private development certificate. Upload the `.msixupload` on the submission's **Packages** page; Partner Center performs the authoritative manifest, malware, policy, and technical certification and then replaces the package signature with Microsoft's Store signature.

Because Congregation is a native Qt Win32 desktop application, the restricted `runFullTrust` capability must be explained under submission options. Use the following only after confirming it remains factually true for the submitted build:

> Congregation is a native Qt 6 Win32 desktop image-editing application. Its primary executable runs at medium integrity and uses standard Win32 desktop APIs to open and save files explicitly selected by the user. It does not install drivers or services, request elevation, modify HKLM, or perform machine-wide configuration. The runFullTrust capability is required because the package contains a traditional full-trust Win32 desktop executable.

The account holder must also complete the listing description and category, price and markets, at least one desktop screenshot, support contact, privacy/data-handling declarations based on the application's actual behavior, the IARC age-rating questionnaire, any additional open-source license notice, and the final submission consent. Do not publish a guessed privacy statement. Windows App Certification Kit is an optional local preflight and requires elevation; its result does not replace Partner Center certification.

## 2. Configure the Release Build
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target Congregation
```
If you need a different bundle identifier, update `BUNDLE_ID` in `CMakeLists.txt` before configuring. Adjust the deployment target if you need to support newer or older macOS releases.

## 3. Stage the App Bundle
The built app lives under `build/Congregation.app`. Copy it to a staging directory (for example, `dist/Congregation.app`) so you can safely run deployment tools without touching your build tree.

## 4. Embed Qt Frameworks
Run `macdeployqt` in App Store mode to embed the required Qt frameworks and QML plugins:
```bash
macdeployqt "dist/Congregation.app" \
  -appstore-compliant \
  -qmldir=App/qml \
  -always-overwrite
```
Verify that all `.framework` bundles now sit inside `dist/Congregation.app/Contents/Frameworks` and that `qt.conf` exists in `Contents/Resources/`.

## 5. Prepare Metadata
Update the generated `Info.plist` (inside `dist/Congregation.app/Contents/`) with:
- `CFBundleIdentifier` matching your bundle ID.
- `CFBundleShortVersionString` set to marketing version `1.0.0` and `CFBundleVersion` initialized to App Store build number `1` for the new bundle identifier.
- `CFBundleIconFile` should resolve to the bundled `resources/Appicon.icns` file. Windows builds embed `resources/Appicon.ico` through the generated resource script.
- `NSLocalNetworkUsageDescription` with the factual explanation that Congregation uses the local network to find nearby Congregation users and share a canvas when the user chooses. The prompt is expected on the first local discovery operation on current macOS versions.

## 6. Sandbox Entitlements
`packaging/macos/Congregation.entitlements` enables the App Sandbox, grants read/write access to user-selected files and picture libraries, and grants both `com.apple.security.network.client` and `com.apple.security.network.server`. UDP multicast discovery sends datagrams and binds a listening socket, while explicit canvas sharing accepts and opens local TCP connections, so both network directions are intentional. Keep any future entitlement additions minimal to improve App Review approval chances.

## 7. Codesign the Bundle
```bash
codesign --force --options runtime \
  --entitlements packaging/macos/Congregation.entitlements \
  --sign "Apple Distribution: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Congregation.app"
```
Then validate the signature:
```bash
codesign --verify --deep --strict "dist/Congregation.app"
spctl --assess --type execute "dist/Congregation.app"
```
If `spctl` warns about missing the hardened runtime, make sure `--options runtime` was passed.

## 8. Create the Installer Package
```bash
productbuild \
  --component "dist/Congregation.app" /Applications \
  --sign "Apple Installer: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Congregation.pkg"
```
This produces the installer payload required by App Store Connect. Keep the `.pkg` under 4 GB.

If installing `dist/Congregation.pkg` or `dist/Congregation-appstore.pkg` shows an older app icon, treat the package as stale. The default `./build.sh` refreshes `dist/Congregation.pkg` only after Developer ID signing, notarization, stapling, and final verification succeed. `CONGREGATION_BUILD_MODE=mas ./build.sh` refreshes the App Store package, while `./build.sh local` writes only the two `-local-unsigned.pkg` artifacts. You can inspect the package payload directly:

```bash
pkgutil --payload-files dist/Congregation.pkg | grep 'Contents/Resources/.*icns'
```

The payload should list `./Congregation.app/Contents/Resources/Appicon.icns` and should not list `./Congregation.app/Contents/Resources/icon.icns`.

If Transporter's Active list still shows the previous icon after this payload check passes, do not treat that queue thumbnail as proof that the `.pkg` still embeds the old icon. The Active list can identify the App Store Connect record by app name and Apple ID before delivery, so remove and re-add the item, confirm you dragged the freshly rebuilt `dist/Congregation-appstore.pkg`, and inspect the package payload again:

```bash
pkgutil --expand-full dist/Congregation-appstore.pkg /tmp/congregation-appstore-payload
cmp resources/Appicon.icns /tmp/congregation-appstore-payload/com.iisacc.congregation.painter.pkg/Payload/Congregation.app/Contents/Resources/Appicon.icns
```

If the `cmp` command succeeds, the upload package contains the current app icon. Update or refresh the App Store Connect app record icon separately if Transporter continues to show the old store listing icon before delivery.

If Dock or Launchpad still shows the old icon after the installed bundle is correct, remove stale `/Applications/Congregation.app` Dock entries and let LaunchServices re-register the rebuilt app. A pinned Dock item can continue pointing at an old or removed bundle path even after the workspace `dist/Congregation.app` has the new icon.

## 9. Upload to App Store Connect
1. Open Transporter.
2. Drag `dist/Congregation-appstore.pkg` into the queue.
3. Provide your App Store Connect credentials and upload.
4. Resolve any validation issues that Transporter reports (missing icons, entitlement mismatches, etc.).

## 10. Post-Upload Checklist
- Create an App Store Connect record with screenshots, localized descriptions, and pricing.
- Attach the uploaded build to a new version submission and complete the export compliance questionnaire.
- Submit for review.

## Troubleshooting Tips
- Use `otool -L dist/Congregation.app/Contents/MacOS/Congregation` to ensure no absolute paths to your build tree remain.
- Leverage `plutil -p` to inspect `Info.plist` after `macdeployqt` runs.
- If Transporter rejects the upload due to missing `LC_VERSION_MIN_MACOSX`, make sure `CMAKE_OSX_DEPLOYMENT_TARGET` is set at configure time.
- Should you require notarization for outside-the-store distribution, rerun codesigning with the same entitlements and submit via `xcrun notarytool`; App Store submissions do not need separate notarization.
- If CLion reports `loading 'build.ninja': No such file or directory`, rerun CMake configure for the selected profile before building. The project also injects local build-tree rpaths for `$HOME/.local/SDK/LVRS` and `$HOME/.local/SDK/iiPaintEngine` so CLion and CTest can run without extra `DYLD_LIBRARY_PATH` setup.

## Linux Build and Packaging
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```
The TGZ archive is written to the build directory as `Congregation-<version>-Linux.tar.gz`. CMake uses Qt 6.8's `qt_generate_deploy_qml_app_script` to collect the linked Qt, LVRS, and iiPaintEngine shared libraries, QML modules, translations, and platform plugins into the install image. Congregation's installed ELF RPATH is `$ORIGIN/../lib` (or the configured GNU install lib directory), so it does not retain a build-tree or `$HOME/.local` runtime dependency. The archive uses a root-relative `bin/`, `lib/`, `qml/`, `plugins/`, and `share/` layout; launch it with `bin/Congregation` after extraction.

The install image includes `share/applications/com.iisacc.congregation.painter.desktop` with `Terminal=false` and a hicolor application icon. Validate release archives on both X11 and Wayland: run an Xvfb/XCB smoke launch and a headless Weston/Wayland smoke launch, inspect `ldd bin/Congregation` for unresolved dependencies, and confirm no resolved path points into the source tree, `build/`, or the maintainer's home directory. If you prefer an installed tree, run `cmake --install build --prefix <path>`; the same relative layout and deployment script are used.

## Automated Tests
To configure and run the current unit test suite:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Congregation tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```
