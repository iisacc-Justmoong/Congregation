<a id="congregation-100-packaging-guide"></a>

# Congregation 1.0.0 포장 가이드

이 문서는 `Congregation` 빌드 트리를 Congregation 1.0.0에 대한 앱 스토어 준수 macOS 패키지로 변환하는 데 필요한 엔드 투 엔드 단계를 캡처합니다. Windows 런타임 패키지를 포함하며, Linux TGZ 패키지를 더합니다. 이 문서는 리포지토리 README 에 참조된 Qt 의존성이 설치되어 있다고 가정합니다.

활성 프로젝트는 번들 식별자 `com.iisacc.congregation.painter` 와 정형 저장소 `https://github.com/iisacc-Justmoong/Congregation` 를 사용합니다. 릴리스 `1.0.0` 는 macOS 빌드 `1`, QML 모듈 `1.0`, Windows 파일/ MSIX 버전 `1.0.0.0` 로 시작합니다. 이전 이름 변경 프로젝트, 생성된 패키지, 서명된 프로비저닝 프로파일이 `Product/.Deprecated` 에 유지됩니다. 이 프로젝트에 대해 신선한 저장소 로컬 `build/` 를 구성합니다. 앱 스토어 배포는 새 식별자를 위해 발급된 프로파일이 필요합니다; 프로덕션 라이선싱 및 업데이트 서비스는 온라인 릴리스 검증 전에 새 제품 식별자를 등록해야 합니다.

Windows MSI 는 Congregation 업그레이드 패밀리와 UpgradeCode `31432BB7-5B11-55D5-8AE0-CD97E6B1D95F` 및 설치 컨텍스트 구성 요소 GUID `05DF7FB6-91BE-5013-AE84-E4A6496550E8` 를 시작합니다. 이 UUIDv5 값들은 URL 정형 저장소에 `#msi-upgrade` 및 `#msi-installation-context` 접미사를 사용하여 유도됩니다. 이후 Congregation 버전에서 새 `1.0.0` 시리즈가 보관된 제품의 설치를 대체하지 않도록 안정적으로 유지하세요.

<a id="1-prerequisites"></a>

## 1. 전제조건
- App Store Connect에 액세스할 수 있는 Apple 개발자 프로그램 멤버십입니다.
- 키체인 접근에서 다운로드한 인증서:
  - Mac App Store 외부 공증 배포를 위한 **Developer ID Application** 및 **Developer ID Installer**.
  - **Apple Distribution** (또는 레거시 *3rd Party Mac Developer Application*).
  - **Apple Installer** (또는 레거시 *3rd Party Mac Developer Installer*).
- 개발자 ID 공증을 위한 `notarytool` 자격 증명 프로필입니다.
- 번들 식별자와 일치하는 macOS App Store 프로비저닝 프로필.
- Xcode 명령줄 도구(`xcode-select --install`) 및 Mac App Store의 Transporter.
- Qt 툴체인(Core, Network, Qml, Quick, QuickControls2, Svg)를 `PATH`에서 사용할 수 있으므로 `macdeployqt`를 호출할 수 있으며 고정된 QtKeychain용 Git도 있습니다. FetchContent 결제.
- iiPaintEngine는 `$HOME/.local/SDK/iiPaintEngine` 아래에 설치되거나 `CMAKE_PREFIX_PATH`를 통해 `iiPaintEngine::iiPaintEngine`로 사용 가능합니다.
- iiSharedCanvas 0.10.1(정확한 패키지 버전)는 `$HOME/.local/SDK/iiSharedCanvas` 아래에 설치되거나 `IISHAREDCANVAS_PREFIX`와 함께 선택되어 `iiSharedCanvas::iiSharedCanvas`를 내보냅니다.
- iiFileProvider 0.5 이상, `iiFileProvider::iiFileProvider` 내보내기. 네이티브 `.iisc` 및 프라이빗 `.vrc` 콘텐츠 I/O는 이 패키지를 사용합니다. 형식 디코딩은 iiSharedCanvas 및 Congregation에 남아 있습니다. 일치하는 `iiSharedCanvas_DIR` 및 `iiPaintEngine_DIR`와 함께 `-DiiFileProvider_DIR=/absolute/prefix/lib/cmake/iiFileProvider`를 사용하여 단계별 패키지를 선택할 수 있습니다.
- iiUpdateManager 0.2는 `$HOME/.local/SDK/iiUpdateManager` 아래에 설치되거나 `IIUPDATEMANAGER_PREFIX`와 함께 선택되어 `iiUpdateManager::iiUpdateManager`를 내보냅니다.
- iiLicenseManager 0.2는 `$HOME/.local/SDK/iiLicenseManager` 아래에 설치되거나 `IILICENSEMANAGER_PREFIX`와 함께 선택되어 `iiLicenseManager::iiLicenseManager` 및 설치된 libsodium 타사 알림을 내보냅니다.

<a id="1a-automated-macos-build-script"></a>

## 1a. 자동화된 macOS 빌드 스크립트
`./build.sh` 는 개발자 ID 배포 흐름으로 기본값입니다. 이것은 ID 애플리케이션과 설치자 신원 및 인증서 자격 증명을 구성하기 전에 유효성 검사하고, `build/` 에서 빌드하며, `ctest --test-dir build --output-on-failure` 를 실행하고, Qt 런타임 를 배포하며, 강화된 런타임 와 신뢰할 수 있는 타임스탬프로 전체 앱 트리를 서명하고, `dist/Congregation.pkg` 를 생성하며, 해당 패키지를 애플의 인증 서비스에 제출하고, 승인된 티켓을 스탭플하며, `pkgutil`, `stapler` 및 게이트키퍼로 최종 패키지를 확인합니다. 정규 `dist/Congregation.pkg` 경로는 이 서명되고 인증된 흐름에서만 게시됩니다.

로컬 개발 유효성 검증을 위해 `./build.sh local` 를 사용하세요. 로컬 모드는 `build/Congregation.app` 를 `LOCAL_APP_CERT`, 첫 번째 유효한 `Apple Development` 신원 또는 임시 서명으로 서명하고 명시적으로 분배할 수 없는 `dist/Congregation-local-unsigned.pkg` 와 `dist/Congregation-appstore-local-unsigned.pkg` 를 생성합니다. 이것은 `dist/Congregation.pkg` 나 `dist/Congregation-appstore.pkg` 를 절대 덮어쓰지 않습니다. 컴포넌트 기반 로컬 및 앱 스토어 패키지 게이트는 요청된 식별자와 버전을 배포 `product` 에 요구하며, 오직 레거시 개발자 ID   `pkgbuild --package`  흐름만이 해당 식별자를 컴포넌트 `pkg-ref` 에서 검증하므로 관련 없는 컴포넌트가 현재 제품 식별자를 대신할 수 없습니다.

macOS  워크플로는 점진적이며 `build/` 를 보존하며, 연결되지 않은 `psd_sdk` 와 QtKeychain   FetchContent 체크아웃을 포함합니다. 도구 체인을 변경했거나 오래된 캐시를 복구한 후에만 `./build.sh --clean` 를 사용하여 깨끗한 개발자 ID 배포 빌드를 하거나, `./build.sh --clean local` 를 사용하여 깨끗한 로컬 검증을 하십시오. 로컬 패키지는 `macdeployqt -no-strip` 로 기호를 유지하며, 개발자 ID , 맥 앱 스토어 및 결합된 배포 모드는 `-no-strip` 를 생략하여 배포 도구에서 릴리스 번들이 스트립됩니다. `MACDEPLOYQT_NO_STRIP=0|1` 는 명시적인 오버라이드로 남습니다.

QtKeychain   0.17.0 는 불변 커밋 `875f77d9f61bd97fd84cca47ce3bc71186dfbd09` 에서만 macOS 와 Windows 에서만 가져옵니다. CMake 는 번역이 포함된 정적 빌드를 강제하며, 데모/테스트 애플리케이션과 QtKeychain 의 자체 CTest 트리를 비활성화합니다. Congregation 는 모든 읽기/쓰기/삭제 작업에 대해 `insecureFallback(false)` 를 명시적으로 설정하므로 지원되지 않거나 거부된 안전한 저장이 결코 `QSettings` 평문 시크릿이 되지 않습니다. macOS 는 키체인을 사용하며 Windows 빌드는 자격 증명 스토어를 활성화하고, Linux 는 의도적으로 매번 수동으로 시작됩니다. 상위 공급 측 상위 공급 측   BSD-3 -조항 `COPYING` 바이트는 `Contents/Resources/legal/QtKeychain/QtKeychain-BSD-3-Clause.txt` 에서 번들로 묶이고 macOS 에서 `legal/QtKeychain/COPYING.txt` 로 준비되어 Windows   ZIP / MSI / MSIX 콘텐츠에 포함됩니다.

`macdeployqt` 이후 스크립트는 `install_name_tool`로 번들 내 모든 Mach-O에서 빌드 머신의 절대 `LC_RPATH` 항목을 제거하고, 연결된 `libiiUpdateManager*.dylib`와 `libiiLicenseManager*.dylib`가 `Contents/Frameworks` 아래에 있어야 한다고 요구한다. 그런 다음 모든 Mach-O 파일을 `otool -L`로 검사하고 남은 모든 `LC_RPATH`를 `otool -l`로 검사한다. 둘 중 어느 런타임이라도 없거나 아키텍처/배포 대상이 잘못되었거나, 배포 바이너리가 여전히 비시스템 의존성 또는 RPATH의 절대 경로를 참조하면 패키징이 실패한다. 공개 전에 `.pkg` 페이로드에서 두 런타임을 모두 검사하며 앱 리소스에는 iiLicenseManager의 libsodium ISC 고지를 유지한다.

각 서명 앱 단계는 코드 서명 전에 `Info.plist` 에서 `IISACCDistributionChannel` 를 받으며, `direct` 는 로컬/Developer ID 단계에 대해, `appstore` 는 Mac App Store 단계에 대해 적용됩니다. Congregation 는 `appstore` 마커 또는 비어 있지 않은 App Store 수표를 스토어 관리로 취급하고 외부 업데이트기를 억제합니다. Windows 스토어/ MSIX 패키징 컨텍스트는 런타임 에서 `GetCurrentPackageFullName` 를 통해 감지되며 동등하게 스토어 관리되며, 직접 ZIP / MSI 빌드는 명시적 수동 업데이트기에 적합하게 유지됩니다.

RPATH 빌드는 로컬 설치 폴백 전에 실제 구성된 LVRS, iiPaintEngine, iiSharedCanvas, iiFileProvider, 업데이터, 라이선스 관리자 패키지 디렉토리를 배치합니다. 일반 유닉스 `$HOME/.local/SDK/<dependency>/lib` 대체 경로 디렉토리는 비 애플 유닉스 빌드에만 제한됩니다. 테스트는 `DYLD_LIBRARY_PATH`, `DYLD_FRAMEWORK_PATH`, 및 `DYLD_FALLBACK_LIBRARY_PATH` 를 상속된 것으로 명확히 하며, RPATH 를 오버라이드하고 `.local` 에서 오래된 ABI -호환 SDK 를 선택할 수 있기 때문입니다. macOS 에서 `tests_filesdkruntime` 는 dyld 진단과 함께 네이티브 및 최근 컨테이너 라운드 트립을 실행하며, iiFileProvider, iiSharedCanvas, 및 iiPaintEngine 가 선택된 패키지 바이너리에 해결되는지 확인합니다. 단계화된 빌드의 쉘 시작은 동일한 환경 정리 작업을 필요로 합니다. 배포는 여전히 절대 빌드 RPATHs 의 제거를 별도로 유효성 검사합니다.

가용성 감사에서는 `otool -L` 의 들여쓰기된 의존성 행만 허용하므로, 유니버설 Mach-O 아키텍처를 위한 반복된 파일명 헤더가 의존성으로 오인되지 않습니다. 또한 dylib 의 자체 `LC_ID_DYLIB` 값과 의존성 목록을 구별하므로 절대 자기 식별자가 외부 의존성으로 보고되지 않습니다. Congregation 은 Qt SQL 에 링크되지 않지만 `macdeployqt` 는 QML 임포트 를 탐색하는 동안 전체 Qt SQL 드라이버 세트를 복사할 수 있으므로 스크립트는 감사 및 서명 전에 `Contents/PlugIns/sqldrivers` 을 제거합니다. 이는 또한 사용하지 않는 ODBC, PostgreSQL, 또는 Mimer 플러그인이 기계별 클라이언트 라이브러리 경로를 유지하는 것을 방지합니다.

스크립트는 `set -u` 가 활성화되고 사용자 정의 추가 CMake 인수가 없는 macOS 시스템 Bash 3.2 에서 실행되도록 예상됩니다. 기본 배포 빌드나 명시적인 로컬 모드 모두 호출자가 `CMAKE_EXTRA_ARGS` 또는 기타 선택적 인수 배열을 정의할 필요가 없습니다. `build.sh` 는 공식 패키징 진입점이므로 추적 소스이며 `.gitignore` 에 의해 무시되어서는 안 됩니다. Congregation 의 마케팅 버전은 `1.0.0` 로 고정되어 있고, macOS `CFBundleVersion` 은 단조롭게 증가하는 앱 스토어 빌드 번호 `1` 이며, 앱 내 `Congregation 1.0.0` 라벨은 제품 패밀리 마케팅 이름으로 유지됩니다.

Congregation 는 의도적으로 저장소 로컬 `build/` CMake 바이너리 디렉토리만 지원합니다. 구성, 빌드, 테스트, 및 패키징 명령은 `-B build`, `cmake --build build`, 및 `ctest --test-dir build` 를 사용해야 하며, 대안 빌드 트리는 CMake 구성 단계에서 거부되므로 CLion 및 쉘 워크플로가 아무런 알림 없이 안전하게 거부하여 다른 곳에서 오래된 번들을 생성할 수 없습니다.

Darwin 호스트에서 CMake 는 `project()` 가 컴파일러 감지를 수행하기 전에 macOS 12 배포 타겟을 설정합니다. 이는 첫 번째 컴파일 시도 후에만 플래그를 추가하는 대신, 컴파일러 기능 검사, FetchContent 타겟 및 최종 번들을 동일한 최소 OS 계약에 유지합니다.

Windows 에서, CMake 는 Windows GUI 서브시스템과 `Congregation.exe` 를 연결하므로 실행하면 콘솔 창이 할당되지 않으며 `QGuiApplication` 이벤트 루프가 프로세스 수명을 소유합니다. CMake 도 `resources/windows/Congregation.manifest.in` 과 `resources/windows/Congregation.rc.in` 를 실행 가능 파일로 컴파일합니다. 이 리소스들은 애플리케이션 아이콘, 4-부분 파일/제품 버전, `asInvoker` 실행 수준, Windows 10/11 호환성, 모니터별 V2 DPI 인식, 및 긴 경로 인식을 포함합니다. CMake 빌드는 `LVRS.dll`, `libiiPaintEngine.dll`, iiFileProvider DLL, `libiiSharedCanvas.dll`, 설치된 `iiUpdateManager.dll` / `libiiUpdateManager.dll`, 및 설치된 `iiLicenseManager.dll` / `libiiLicenseManager.dll` 를 선택된 MinGW 컴파일러 옆에 발견된 런타임 DLLs 를 `build/` 에 복사하며, 관련 없는 시스템 `PATH` 에서 다른 컴파일러 런타임 를 검색하지 않습니다. Windows 단계화는 정확히 하나의 공유 캔버스 DLL, 하나의 업데이트러 DLL, 및 하나의 라이선스 관리자 DLL 를 필요로 하며, PE import-closure, strip, 및 Authenticode 게이트에 포함된 모든 3 를 포함하고, 하나라도 누락되면 ZIP / MSI 발행 전에 실패합니다. 이는 `build/` 트리의 직접적인 CLion 또는 쉘 시작을 선택된 Qt 키트에 정렬되도록 유지하며, Qt 자체를 부분적으로 다시 배포하지 않습니다; `build/Congregation.exe` 에서 누락된 의존성 대화상자는 빌드 트리가 구식이므로 `cmake --build build` 로 다시 빌드해야 함을 의미합니다.

모든 모드가 `CFBundleIconFile` 가 `Contents/Resources/Appicon.icns` 로 해결되는지, 레거시 `Contents/Resources/icon.icns` 파일이 존재하지 않는지, 그리고 번들 아이콘이 `resources/Appicon.icns` 와 일치하는지 확인합니다. 모든 생성된 `.pkg` 페이로드가 빌드가 종료되기 전에 검사되므로, 여전히 `icon.icns` 를 포함하고 있는 구식 설치자 패키지는 오래된 앱 아이콘을 설치하는 대신 빌드를 실패시킵니다.

`resources/Appicon.icns`는 정식 macOS 아이콘 소스입니다. 교체한 후 빌드하기 전에 App Store/Xcode 자산 카탈로그를 다시 생성하십시오.

```bash
tools/sync_app_icon_assets.sh
```

이 스크립트는 정식 `.icns`에서 `AppIcon-1024.png`를 포함하여 `packaging/macos/Congregation.xcassets/AppIcon.appiconset`를 다시 작성합니다. CMake 번들 대상은 또한 각 macOS 빌드 후에 오래된 `Contents/Resources/icon.icns`를 제거하므로 증분 `build/` 번들은 제거된 레거시 아이콘을 계속 광고할 수 없습니다.

기본 개발자 ID 흐름 및 명시적 대안은 다음과 같습니다.

```bash
./build.sh
CONGREGATION_BUILD_MODE=devid ./build.sh
./build.sh local
CONGREGATION_BUILD_MODE=mas ./build.sh
CONGREGATION_BUILD_MODE=all ./build.sh
```

`devid` 는 `Developer ID Application` 와 `Developer ID Installer` 인증서 및 공증 자격 증명을 필요로 합니다. `mas` 는 `Apple Distribution` 와 `3rd Party Mac Developer Installer` 인증서를 필요로 합니다. `all` 는 두 가지 배포 흐름을 모두 실행합니다. 개발자 ID 공증의 경우 `NOTARY_KEYCHAIN_PROFILE` 를 선호하며, Apple ID 모드가 사용되면 스크립트에 저장하는 대신 `NOTARY_APP_PASSWORD` 를 통해 앱별 비밀번호를 제공해야 합니다. 기본 `notary-main` 프로필은 앱별 비밀번호를 쉘 히스토리에 넣지 않고 생성할 수 있습니다.

```bash
xcrun notarytool store-credentials notary-main \
  --apple-id "<Apple ID>" \
  --team-id "5U49ST9XZH"
```

`--password` 가 생략되면 `notarytool` 가 이를 안전한 프롬프트를 통해 요청하고 자격 증명을 저장하기 전에 이를 검증합니다. 모든 개발자 ID 실행도 CMake 를 구성하기 전에 공증 자격 증명을 검증하여, 누락되거나 유효하지 않은 프로필로 인해 긴 서명된 빌드가 종료되는 것을 방지합니다.

<a id="1b-windows-build-package-and-current-user-install-script"></a>

## 1b. Windows 빌드, 패키지 및 현재 사용자 설치 스크립트
Windows 빌드를 Windows PowerShell 5.1 또는 그 이상의 버전에서 실행합니다. 스크립트는 Windows 로 빌드된 Qt, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, 및 iiLicenseManager 접두사를 기대하며, macOS `.local` 바이너리는 Windows 에서 재사용할 수 없습니다. 기본 SDK 설치 루트는 `$HOME\.local\SDK`, CMake 힌트, PowerShell 빌드 기본값, 및 Windows CI 워크플로우와 일치합니다. 명시적인 SDK 접두사 변수는 계속 지원됩니다. 각 쉘 세션마다 한 번 설정합니다.

```powershell
$env:QT_PREFIX = "C:\Qt\6.8.3\mingw_64"
$env:LVRS_PREFIX = "$HOME\.local\SDK\LVRS"
$env:IIPAINTENGINE_PREFIX = "$HOME\.local\SDK\iiPaintEngine"
$env:IISHAREDCANVAS_PREFIX = "$HOME\.local\SDK\iiSharedCanvas"
$env:IIUPDATEMANAGER_PREFIX = "$HOME\.local\SDK\iiUpdateManager"
$env:IILICENSEMANAGER_PREFIX = "$HOME\.local\SDK\iiLicenseManager"
```

지원되는 MinGW 설정은 Qt 의 번들 CMake, Ninja, 및 MinGW 도구를 `PATH` 에서 사용합니다. 포함된 `psd_sdk` 빌드는 `Psdminiz.c` 를 C++ 로 취급합니다. 이는 상위 공급 측 C 파일이 C++ `static_assert` 선언을 포함하는 헤더를 포함하기 때문입니다. 좁은 `-Wno-pragmas` 예외는 상위 공급 측 타겟에만 적용됩니다. 파일은 다른 SDK 번역 단위에도 포함되기 때문입니다. Windows MinGW 빌드는 릴리스 최적화, 함수/데이터 섹션, 링커 쓰레기 수집, 및 릴리스 심볼 스트리핑을 유지하지만 IPO 를 Congregation 와 PSD SDK 에 대해 비활성화합니다: MinGW 13 는 LTO 플러그인을 Ninja 의 아카이브 및 생성된 QML 단계를 통해 일관되게 전파하지 않습니다. 이는 일반적으로 플러그인 진단, 시리얼 LTRANS 대체 경로, 및 신뢰할 수 없는 링크를 생성합니다. 다른 툰체인은 계속 LVRS IPO 정책을 사용합니다. 현재 iiPaintEngine Qt 어댑터는 브러시 불투명도 토글을 `brushOpacityEnabled` 로 노출하므로, QML 및 브리지 코드는 이전 `pressureToOpacityEnabled` 속성 이름을 사용하지 않아야 합니다.

배포 가능한 패키지가 없는 로컬 빌드, 테스트 및 단계적 런타임의 경우 다음을 사용합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -SkipPackage
```

공공 Windows 패키지는 안전하게 거부하는 이며, Windows SDK SignTool 및 코드 서명 인증서가 필요합니다. 이 인증서의 개인 키는 Windows 인증서 저장소, 하드웨어 토큰, 또는 구성된 KSP 를 통해 사용할 수 있습니다. 정확한 40-hex 저장소 서명 지문을 통해 인증서를 선택합니다. 서명 지문은 인증서를 식별하며, 파일 및 타임스탬프 해시는 SHA-256 로 유지됩니다. 기본 저장소는 `CurrentUser\My` 입니다. `-SigningCertificateStoreLocation LocalMachine` 를 추가하려면 보호된 키가 `LocalMachine\My` 에 설치되어 있어야 합니다. Windows   PowerShell   5.1 는 코드 서명 EKU 객체 식별자를 문자열로 노출할 수 있지만 다른 인증서 제공자는 `Value` 속성을 가진 객체를 노출합니다; 서명 사전 검사 는 두 가지 표현을 모두 표준화한 후 EKU   `1.3.6.1.5.5.7.3.3` 를 강제합니다. 정책 계약은 Windows   PowerShell   5.1 와 PowerShell   7 하에 `pwsh.exe` 가 설치될 때 실행됩니다. 소비자 배포를 위해 릴리스 작업은 소비자 신뢰 코드 서명 루트 또는 신뢰할 수 있는 서명 서비스에 대한 인증서 체인을 사용해야 합니다. 중단된 게시 복구 후 부분 아티팩트 삭제 또는 새 패키지 출력 생성 전에, 서명 사전 검사 는 자체 서명된 신원을 거부하고 온라인 회수 코드 서명 체인을 구축하며 종단 인증서가 Windows '  `LocalMachine\AuthRoot` 공개 루트 저장소에 있어야 합니다. 이는 개발 PC 에서만 수동으로 신뢰된 개인 루트를 차단합니다. 깨끗한 스톡 Windows 설치에는 최종 소비자 신뢰 점검이 남아있으며, 루트 프로그램 상태와 SmartScreen 평판은 이미지 및 업데이트 수준에 따라 다를 수 있습니다.

```powershell
$env:CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT = "0123456789ABCDEF0123456789ABCDEF01234567"
$env:SIGNTOOL_PATH = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
$env:CONGREGATION_TIMESTAMP_URL = "http://timestamp.digicert.com"
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Sign
```

릴리스 경로는 Release 또는 MinSizeRel만 허용하며 `-SkipTests`는 허용하지 않는다. Code Signing EKU `1.3.6.1.5.5.7.3.3`, 접근 가능한 비공개 키, 현재 유효한 인증서 기간, Key Usage 확장이 있다면 `DigitalSignature` 권한, 접근 가능한 RFC 3161 타임스탬프 서비스가 필요하다. 서명 없는 출력으로 대체하지 않는다. 서명된 Windows Installer 패키지도 만들려면 `WIX_TOOLS_DIR`, `WIX` 또는 `PATH`로 WiX Toolset 3.x를 사용할 수 있게 한 뒤 `-CreateMsi`를 추가한다. 명령줄 비밀값은 다른 로컬 프로세스에서 관측할 수 있으므로 PFX 암호 인자는 의도적으로 지원하지 않는다. 명시적 덮어쓰기는 `SIGNTOOL_PATH`이다. 자동 탐색은 `PATH`로 주입한 임의 실행 파일 대신 보호된 Windows Kits 위치를 고려한다.

<a id="public-windows-signing-identity-onboarding"></a>

### 공개 Windows 서명 ID 온보딩

신원 증명 및 보호 키 활성화는 법인 또는 승인된 조직 담당자가 완료해야 합니다. 게시자 이름을 만들어내거나, 개인 키 또는 토큰 PIN를 관리자에게 보내거나, 단지 이 빌드를 자동화하기 위해 키를 PFX로 내보내지 마십시오.

1. 주문하기 전에 발행자 신원을 선택하세요. 개인의 OV 인증서는 검증된 법적 개인 이름을 표시합니다. 조직 OV 인증서는 검증된 등록 법적 또는 상호 이름을 표시하며, `Congregation` 는 제품 이름이기만 해서 발행자로 사용할 수 없습니다. EV 는 고객, 조달 정책 또는 다른 서명 프로그램이 명시적으로 요구하는 경우에만 적합하며, OV 를 통한 자동 Microsoft Defender SmartScreen 우회 기능을 더 이상 제공하지 않습니다.
2. 지원하는 신청자의 국가와 법적 형태를 지원하는 공개 CA 에서 OV 코드 서명 인증서를 주문하세요. 현재 Windows -스토어 SignTool 흐름의 경우 CA 발급 USB 하드웨어 토큰이 가장 간단한 옵션입니다. Windows 제공자가 인증서와 보호된 개인 키를 `CurrentUser\My` 또는 `LocalMachine\My` 를 통해 노출하는 CA 클라우드 HSM 또는 다른 준수 토큰/ KSP 도 허용됩니다.
3. CA의 신원 검증은 본인이 직접 완료한다. 개인은 정부 발급 사진 ID/영상 검증, 법적 이름과 거주지 주소 증빙, 독립적인 전화 또는 이메일 확인, 가입자 계약과 확인 전화를 예상해야 한다. 조직은 법적 실재와 주소 기록, 신청자의 사진 ID/영상 확인, 권한 증빙, 독립적으로 검증 가능한 업무용 전화/이메일, 가입자 계약과 승인 전화를 예상해야 한다. CA는 추가적인 최신 등록 기록이나 운영 문서를 요청할 수 있다.
4. 하드웨어 토큰을 수신하고 초기화하거나, CA 의 지시를 사용하여 클라우드 HSM 계정을 활성화합니다. CA /token 공급자의 서명된 미들웨어와 Windows 암호학 공급자만 설치합니다. 다중 요소 인증을 계속 활성화합니다. PIN , 복구 비밀, 개인 키, 또는 클라우드 서명 자격 증명을 절대 공개하지 마십시오.
5. 토큰을 연결하고 잠금을 해제한 후 Windows가 현재 유효한 코드 서명 인증서와 액세스 가능한 개인 키를 볼 수 있는지 확인하세요.

   ```powershell
   Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
     Where-Object { $_.HasPrivateKey } |
     Format-List Subject,Issuer,Thumbprint,NotBefore,NotAfter,HasPrivateKey
   ```

   공급자가 머신 전체에 설치하는 경우 `Cert:\LocalMachine\My`에 대해 명령을 반복하고 `-SigningCertificateStoreLocation LocalMachine`를 빌드에 전달합니다.
6. 실제 40-hex 지문을 고안하거나 단축하지 않고 복사합니다. 비밀이 아닌 선택기와 CA의 RFC 3161 URL만 설정한 다음 PowerShell를 닫았다가 다시 열어 영구 사용자 환경이 다시 로드되도록 합니다.

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

7. 토큰이 연결되고 잠금 해제되면 한 번의 트랜잭션으로 대체 ZIP 및 MSI를 생성합니다.

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Sign -CreateMsi
   ```

릴리스 운영자는 유지 관리자에게 인증서 엄인, 정확한 `Subject` /Publisher 텍스트, 저장 위치, 및 공개 타임스탬프 URL 만 제공할 수 있습니다. 이 값들은 개인 키 재료이 아닙니다. 최종 릴리스는 정확한 예상 Publisher 정체성과 SHA-256 사이드카를 게시한 후, 게시 전에 깨끗하고 완전히 업데이트된 스톡 Windows 기계에서 서명된 MSI 를 설치하고 시작해야 합니다. 또한 모든 번들 프로젝트 및 제 3 자 구성 요소가 명시적인 재배포 가능 라이선스와 필요한 통지를 가지고 있음을 확인하십시오; 공개 서명은 누락된 배포 권리를 구제하지 않습니다.

<a id="windows-distribution-licenses-and-corresponding-source"></a>

### Windows 배포 라이센스 및 해당 소스

모든 ZIP/MSI 패키지는 `LICENSE.txt`, `THIRD_PARTY_NOTICES.txt`, `SOURCE_OFFER.txt`와 `legal/` 트리를 스테이징한다. 트리에는 LVRS, iiPaintEngine 및 iiSharedCanvas의 AGPL 원문, iiLicenseManager에 설치된 libsodium ISC 고지, QtKeychain의 BSD-3-Clause 원문, psd_sdk의 BSD-2-Clause 및 내장 miniz의 Unlicense 원문, Pretendard 1.3.9의 OFL 원문, 배포된 Qt 모듈의 라이선스 모음과 SPDX 문서, 스테이징한 MinGW 런타임 DLL과 해시가 일치하는 정확한 3 GCC/MinGW-w64/winpthreads 고지가 포함된다. 패키징 검사는 각 원본 파일을 바이트 그대로 복사하고 스테이징 후 SHA-256을 검증한다. WiX가 최종 스테이지를 수집하므로 ZIP와 MSI에 같은 트리가 있어야 한다.

선택된 툴체인 Qt 의 패키징은 일치하는 Qt `Sources` 컴포넌트와 SBOM 파일이 필요합니다. Qt 6.8.3 에 대해 `C:\Qt\6.8.3\Src` 와 `C:\Qt\6.8.3\mingw_64\sbom` 아래에서 해결됩니다. 완전한 Qt 설치기는 `Src\LICENSES` 하위에 있는 공통 라이선스 텍스트 5 를 제공합니다; 선택적 `aqt install-src` 레이아웃은 해당 루트 디렉토리를 생략할 수 있으므로, 패커는 동일한 5 비어 있지 않은 공통 텍스트가 존재하는지 확인한 후에만 `Src\qtbase\LICENSES` 를 수락합니다. SignPath 워크플로우는 배포된 Qt 모듈 각각에 대한 소스 라이선스 아카이브를 다운로드하며, `qttranslations` 를 포함합니다. MinGW 패키지는 복사하기 전에 스테이지된 런타임 DLL 해시를 비교하여 정확한 `C:\Qt\Tools\mingw*` 디렉토리를 해결하며, 관련 없는 툴체인 의 라이선스 디렉토리는 거부됩니다.

iiPaintEngine 저작권 소유자가 `AGPL-3.0-only` 를 선택했습니다. 그 저장소는 루트 `LICENSE` 에 AGPL v3 텍스트를 포함하고, README 에서 SPDX 라이선스를 식별하며, 텍스트를 `share/licenses/iiPaintEngine/LICENSE` 로 설치하고 계약 테스트로 레이아웃을 보호합니다. Congregation 는 해당 정확한 파일을 `legal/iiPaintEngine/LICENSE.txt` 로 복사하고 스테이지된 바이트를 검증합니다. 임시 2026 `-AllowUnsignedPackage` 경로는 동일한 완전한 대응 소스와 법적 자료 게이트가 통과할 때만 공개 릴리스입니다.

첫 번째 공개 빌드 전에 `Congregation-1.0.0-Corresponding-Source.zip` 를 깨끗한 Congregation , LVRS , iiPaintEngine , iiSharedCanvas , iiUpdateManager , 및 iiLicenseManager 소스와 전달된 라이브러리에 필요한 정확한 Qt 대응 소스로부터 생성합니다. 출판자가 제어하는 위치에 게시하고, `Get-FileHash -Algorithm SHA256` 를 계산하며, `CONGREGATION_CORRESPONDING_SOURCE_URL` 와 `CONGREGATION_CORRESPONDING_SOURCE_SHA256` 를 그 정확한 값으로 설정합니다. 단독 상위 공급 측 Qt 다운로드 링크는 릴리스의 대응 원본 증거로 취급되지 않습니다. 서명 스크립트는 절대 HTTPS URL 와 64-hex SHA-256 만 수락하며, 둘 다 설치된 `SOURCE_OFFER.txt` 에 기록합니다.

불변 릴리스 태그를 생성하고 체크아웃한 뒤 `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\new-corresponding-source.ps1 -Version 1.0.0 -CongregationRevision v1.0.0 -IiSharedCanvasRevision $env:IISHAREDCANVAS_COMMIT -IiUpdateManagerRevision $env:IIUPDATEMANAGER_COMMIT -IiLicenseManagerRevision $env:IILICENSEMANAGER_COMMIT`로 아카이브를 생성한다. 도구는 태그와 iiSharedCanvas, iiUpdateManager, iiLicenseManager를 포함한 각 고정 의존성을 정확한 커밋으로 해석한다. 변경 가능한 작업 트리 바이트 대신 Git 리비전을 아카이브하고, Windows 런타임이 전달하는 Qt 6.8.3 원본 모듈만 복사하며, 파일별 SHA-256 값을 기록하고, 아카이브와 사이드카를 `build/release-source/`에 저장한다. 로컬 작업 트리 변경은 보고하고 제외한다. Git 아카이브는 `psd_sdk/build/VS2019`처럼 정당하게 추적되는 빌드 시스템 원본 디렉터리를 포함한 추적 바이트만 담는다. 저장소 메타데이터는 거부하며 Qt 파일 시스템 복사는 별도로 `.git`와 생성된 `build` 디렉터리를 제외한다. 생성한 `BUILD-SOURCE.md`는 PowerShell 이스케이프 문자 치환 없이 Markdown 경로를 그대로 유지한다. 출력 경로가 필수적인 저장소 로컬 `build/` 트리 밖에 있으면 거부한다. 워크플로우는 `IIUPDATEMANAGER_READ_TOKEN`라는 저장소 시크릿으로 고정된 비공개 iiUpdateManager와 iiLicenseManager 체크아웃을 인증하고, 체크아웃 자격 증명의 지속 저장을 비활성화하며, 사용 전에 해석된 두 커밋을 검증한다. 여전히 `IIUPDATEMANAGER_REPOSITORY`와 검토된 40자리 16진수 `IIUPDATEMANAGER_COMMIT` 및 `IILICENSEMANAGER_COMMIT`가 필요하다. 출처 정보나 토큰 상태가 누락되면 원본 설치 전에 실패한다.

웹사이트 릴리스 런너는 iiSharedCanvas CTest 를 실행하기 전에 iiSharedCanvas 빌드 디렉토리와 고정된 iiPaintEngine, LVRS, Qt 런타임 디렉토리를 앞에 추가합니다. Windows 에서 해당 위치를 생략하면 DLL 가 성공적으로 컴파일 및 링크되더라도 테스트 본문 시작 전에 로더 상태 `0xc0000135` 가 생깁니다.

독립적인 iiUpdateManager 빌드 및 테스트 게이트는 더 긴 LVRS 빌드 전에 실행되므로 사설 업데이트러 회귀 가 빠르게 실패합니다. 신규로 빌드된 DLL 와 Qt 런타임 디렉터리들은 `PATH` 에 앞에 추가되며, CTest 가 실패하면 런너는 테스트별 텍스트 출력과 함께 업데이트 흐름 실행 가능 파일을 반복 실행하고 그 진단을 출력한 후 중지합니다. 런너는 또한 vcpkg `x64-mingw-static` libsodium 설치에 대해 iiLicenseManager 를 빌드하고 CTest 수트를 실행하며 Congregation 를 구성하기 전에 설치된 제 3 자 공지사항을 요구합니다.

서명되지 않은 임시 공개 MSI는 `-unsigned` 파일 이름 접미사를 수신하므로 서명된 릴리스와 혼동될 수 없습니다. 릴리스 또는 MinSizeRel로 제한되며 전체 테스트 모음 및 공개 소스 증거가 필요하며 2027시작 시 자동으로 만료됩니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -BuildType Release -AllowUnsignedPackage -SkipPackage -CreateMsi
```

스크립트는 항상 `cmake -S . -B build`로 구성하고 증분 작업을 위해 저장소 내부의 해당 트리를 보존하며 CMake의 병렬 모드로 빌드한다. `-SkipTests`를 전달하지 않는 한 `ctest --test-dir build --output-on-failure`를 실행한다. 일반 CTest는 소스, 작성과 빌드 워크플로 계약을 검증한다. `build/`에 이미 존재하는 MSI는 이전 패키징 실행에서 생성했을 수 있으므로 의도적으로 찾거나 검사하지 않는다. `-CreateMsi`를 요청하면 패키징 워크플로는 새로 링크한 정확한 `.partial.msi` 경로와 3개 필드로 정규화한 Windows Installer ProductVersion을 MSI 데이터베이스 계약에 전달하며 명시적 데이터베이스 검사를 통과하지 않으면 해시 계산이나 공개를 거부한다. 툴체인을 바꾸거나 오래된 빌드 트리를 복구할 때 로컬 재빌드만 초기화하여 수행하려면 `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -Clean -SkipPackage`를 사용한다. 같은 초기화 실행으로 패키지도 공개하려면 적절한 서명 또는 명시적 미서명 패키징 플래그를 추가한다. `-SkipTests`를 전달하면 `BUILD_TESTING=OFF`로 구성하고 `Congregation` 타깃만 빌드한다. 런타임 배포는 `windeployqt --qmldir src/App/qml --translations en,ko`를 사용하며 벡터 브러시 커서가 사용하는 Qt Quick Shapes QML 플러그인이 필요하다. 릴리스에서만 제외하는 QML 디버거 플러그인 그룹과 사용하지 않는 generic/Insight Tracker, SQL, Quick3D 유틸리티, PDF, Qt Virtual Keyboard, 소프트웨어 OpenGL 및 런타임 D3D/DXC 컴파일러 페이로드를 제외한 뒤 LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager 및 iiLicenseManager 런타임 DLL을 복사한다. Congregation 는 Qt 의 Direct3D 백엔드를 사용하며 사전 컴파일된 셰이더를 사용하며 런타임 에서 해당 셰이더 컴파일러를 로드하지 않습니다. LVRS QML 는 LVRS 바이너리에 컴파일되므로 해당 단계를 통해 배포에서 방출된 느슨한 `qml/LVRS` 디렉토리를 복사하는 대신 수천 개의 중복 소스 파일이나 `qmldir` `prefer` 지시문을 제거합니다.

패키징 전에 스크립트는 스테이지된 실행 파일이 네이티브 AMD64 PE32 + Windows GUI 바이너리인지, 필요한 ASLR / DEP 플래그를 가지고 있는지, 그리고 예상되는 파일 버전, 제품 버전, 제품 이름을 노출하는지 확인합니다. MinGW 릴리스 및 MinSizeRel 단계는 `Congregation.exe`, `LVRS.dll`, `libiiPaintEngine.dll`, `libiiSharedCanvas.dll`, 업데이터 런타임, 및 라이선스 관리자 런타임 에서 완전한 COFF 심볼 테이블을 제거하며, Qt 의 배포된 바이너리는 그대로 둡니다. 단계는 또한 `objdump` 로 검사되며, `LVRS.dll` 와 같은 의존성이 `__cxa_thread_atexit` 를 가져오지만 스테이지된 `libstdc++-6.dll` 는 이를 내보내지 않으면 스크립트는 MinGW ABI 오류로 실패하며 의존성은 Qt 와 동일한 MinGW 키트로 다시 빌드되어야 합니다. 동일한 패스는 모든 준비된 실행 가능한 파일의 PE import 클로저와 DLL 를 준비된 페이로드 및 Windows 시스템 DLL 설정과 비교하여, 해결되지 않는 서드파티 import 는 ZIP 또는 MSI 생성 전에 실패합니다.

배포 후, 제거, 스트립, PE 검증 및 ABI /import 검증이 완료되면, 서명 모드는 `Congregation.exe`, `LVRS.dll`, `libiiPaintEngine.dll`, 공유 캔버스 런타임 (`iiSharedCanvas.dll` 또는 `libiiSharedCanvas.dll`), 업데이터 런타임 (`iiUpdateManager.dll` 또는 `libiiUpdateManager.dll`), 라이선스 관리자 런타임 (`iiLicenseManager.dll` 또는 `libiiLicenseManager.dll`) 를 다른 유효한 서명이 함께 도착했더라도 선택된 인증서에 대해 정확히 서명합니다. 그것은 다른 PE 파일에 유효한 타임스탬프된 벤더 서명을 유지하며, 나머지 서명되지 않은 준비된 `.exe` 및 `.dll` 파일을 `signtool sign /fd SHA256 /tr <url> /td SHA256` 로 Authenticode 서명합니다. 그런 다음 `signtool verify /pa /all /tw` 가 성공해야 모든 준비된 PE 에 대해 요구되며, 압축 전에 Congregation 소유 파일에서 선택된 인증서 지문 thumbprint 를 다시 확인합니다.

실행 전에 선택된 패키지의 임시 `.partial` 파일만 지워집니다. ZIP 및 MSI 바이트는 `.partial` 이름 아래 생성되어, 최종 기록된 파일명으로 검증되고 해시되며, 이전 표준 아티팩트는 그대로 유지됩니다. 게시물은 먼저 각 표준 아티팩트/체크섬 쌍이 유효하게 존재했는지 또는 완전히 결여되었는지를 포함하여 동일한 볼륨의 `prepared` 트랜잭션 저널을 원자적으로 작성합니다. 그런 다음 마지막 알려진 좋은 쌍을 `.previous` 로 이동하고, 완전한 검증된 세트를 승격하며, 모든 승격된 쌍을 다시 검증하고, 백업과 저널의 마지막을 삭제하기 전에 원자적으로 저널을 `committed` 로 변경합니다. 재시도는 서명 도구 발견, 정리 또는 `-Clean`: `prepared` 전에 해당 저널을 조정하며, `committed` 가 승인되려면 모든 최종 쌍이 유효하게 유지되어야 합니다. ZIP 와 MSI 가 함께 요청될 때, 두 개의 부분 패키지가 각각의 서명과 검증 게이트를 통과할 때까지 정통 파일은 변경되지 않습니다. 버전당 하나의 저널과 서명/미서명 맛은 ZIP 전용, MSI 전용, 그리고 결합된 출력을 직렬화하며, 이러한 모드는 정통 파일을 공유하기 때문입니다; 기존 저널은 고정된 ZIP / MSI 허용 목록에만 매칭되며, 저널 없는 실행은 해당 호출에 의해 요청된 패키지 유형만 검사합니다. 회복은 SHA-256 로 최종 파일과 백업 파일을 검사하며, 프로세스 중단이 한쪽 백업을 남긴 후에도 내부적으로 일관된 아티팩트/체크섬 쌍만 선택합니다. 서명과 미서명 출력은 서로 삭제하거나 혼합되지 않습니다. ZIP 는 최종 서명 단계에서 구축되며, WiX 는 해당 단계와 MSI 에서 구축되고, MSI 컨테이너 자체는 출판 전에 서명되고 검증됩니다. 연결된 부분 MSI 도 해싱 또는 승진 전에 데이터베이스 계약 테스트를 통과해야 하므로, UI , 기능, 라이선스, 그리고 트랜잭션 업그레이드 순서는 출시 게이트이며 빌드 후 관측 사항이 아닙니다. Heat, Candle, Light 는 경고를 오류로 취급하고 ICE 억제를 하지 않으며, Smoke 는 추가로 ICE105 를 단독으로 실행하여 경고를 오류로 승진시켜 도구 기본값으로 인해 이중 컨텍스트 계약을 건너뛰는 것을 방지합니다. 각 ZIP 와 MSI 는 인접한 `.sha256` 기록을 받으며, 체크섬만으로는 발행자 신원을 증명할 수 없으므로 인증된 릴리스 채널을 통해 해당 기록을 게시합니다. Authenticode 는 단계별 PE 파일과 서명한 MSI 컨테이너를 포함하지만 ZIP 컨테이너나 그 느슨한 QML /resources 는 포함하지 않으므로, 인증된 사이드카 해시는 전체 ZIP 를 결합합니다. 단계별 앱은 `dist/Congregation-Windows` 에 작성되고, 서명한 ZIP 는 `dist/Congregation-1.0.0-Windows.zip` 에, 서명한 MSI 는 `build/Congregation-1.0.0-Windows.msi` 에 작성됩니다. 명시적 서명 없는 연기 아티팩트는 `Congregation-1.0.0-Windows-unsigned.*` 를 사용합니다. 릴리스 노트에는 예상되는 발행자 주체 또는 인증서 엄밀한 지문을 명시해야 사용자가 Congregation 의 서명자를 관련 없는 유효한 서명자와 구별할 수 있습니다. Windows 설치 프로그램 5 MSI 는 공식 `ALLUSERS=2` 와 `MSIINSTALLPERUSER=1` 기본값을 작성하므로 현재 사용자에 대한 기본값으로 로컬 애플리케이션 데이터를 설정하고 별도의 Congregation 업그레이드 가족을 시작합니다. 고급 옵션은 또한 네이티브 64-비트 프로그램 파일 하의 모든 사용자 설치를 제공하며, 이는 권한 상승이 필요합니다. 필요한 컨텍스트 인식 `installation-context marker` 는 선택된 범위와 해결된 설치 위치를 기록합니다. 마커 발견은 `FindRelatedProducts` 전에 실행되어 후기 업그레이드를 등록된 범위에 잠그고 사용자 정의 모든 사용자 디렉토리를 보존합니다. `WIX_UPGRADE_DETECTED` 를 통해 감지된 마커리스 사용자별 업그레이드는 레거시 대체 경로 로 현재 사용자 범위에 잠깁니다. 비동기 업그레이드는 `ALLUSERS` 나 `MSIINSTALLPERUSER` 를 덮어써서는 안 되며, Windows Installer 는 활성 설치 컨텍스트에서 관련 제품을 열거하므로 명시적으로 강제된 반대 컨텍스트는 상호작용형 마커리스 대체 경로 를 사용할 수 없습니다. 모호한 동시 사용자별 및 머신별 등록은 새 설치와 크로스 컨텍스트 업그레이드를 차단하며, 복구 목적으로 유지 및 제거는 계속 사용 가능합니다. 동일한 버전과 아키텍처에는 결정론적인 MSI ProductCode 가 사용되고, 다른 버전이나 아키텍처는 다른 ProductCode 를 받아 동일 버전 재설치가 중복 제품 등록을 생성하는 것을 방지합니다. 별도의 `-InstallForCurrentUser` 패키징되지 않은 연기 경로는 로컬 애플리케이션 데이터 아래에 유지됩니다. 공정 수정은 첫 번째 3 ProductVersion 필드를 증가시켜야 하므로 나중에 Congregation 릴리스는 1.0.0 를 업그레이드하고 다른 1.0.0 패키지는 설치된 제품 식별자를 유지합니다. 모든 후기 업그레이드는 설치된 제품에 선택된 동일한 설치 컨텍스트를 사용해야 합니다. `RemoveExistingProducts` 는 `InstallInitialize` 이후 실행되어 Windows 설치자 트랜잭션 내부의 기존 제품 제거를 유지하므로 실패한 대체를 롤백할 수 있습니다.

시스템 전체 Congregation 패키징 뮤텍스는 접합, 기호 링크, 매핑된 드라이브 또는 UNC 별칭을 통해 동일한 저장소에 도달하는 경우를 포함하여 프로세스 중 하나가 빌드 트리 또는 게시 저널을 변경할 수 있기 전에 동시 스크립트 호출을 거부합니다.

<a id="windows-installer-ui-contract"></a>

### Windows 설치 프로그램 UI 계약

WiX MSI 는 Congregation 애플리케이션 파일을 필수 핵심 기능으로 노출하고 실행 파일과 함께 컨텍스트 인식 시작 메뉴 단축키 하나를 설치합니다. 기본값은 현재 사용자로 설정되므로 승격이 필요 없으며, 첫 번째 설치에서 **고급** 를 선택하여 현재 사용자 또는 모든 사용자 범위를 선택하고, 모든 사용자 범위의 경우 설치 디렉토리를 선택합니다. 현재 사용자 범위는 로컬 애플리케이션 데이터 아래에 루트되며, 모든 사용자 범위는 머신의 네이티브 `%ProgramFiles%\Congregation`, 공유 시작 메뉴를 사용하며 승격이 필요합니다. `Congregation.exe` 가 소유한 광고된 항목으로 단축키가 작성된 후 `DISABLEADVTSHORTCUTS=1` 로 일반 셸 단축키로 구체화되어 ICE38 , ICE43 , ICE57 , ICE105 에서 모두 실행 파일 기반 구성 요소가 유효하게 유지됩니다. 업그레이드 시 저장된 설치 컨텍스트 마커는 반대 범주 선택을 모두 상회하며 관련 제품 감지 전에 기록된 설치 위치를 재사용합니다. 필수적인 런타임 는 사용 불가능한 패키지로 선택 해제될 수 없습니다. 패키징은 저장소 루트 `LICENSE` 를 MSI 의 RTF 라이선스 제어에 Unicode 안전 이스케이프로 변환하여 마법사가 실제 GNU AGPL 조항을 제시하고 WiX 의 플레이스홀더 텍스트를 절대 표시하지 않으며 Windows RichEdit 왕복 변환 왕복 변환 테스트가 변환기를 보호합니다.

Windows 설치기는 등록된 제품 신원을 통해 설치된 제품을 식별합니다. 성공적인 설치 후 동일한 MSI 를 다시 열면 설계상 유지 관리 모드로 진입하며 다른 첫 번째 **설치** 흐름으로 기술되거나 테스트되어서는 안 됩니다. 유지 관리 마법사는 표준 **변경**, **복구**, **제거** 경로를 노출하며 **변경** 는 설치된 필수 기능 상태를 표시하고 **복구** 는 누락되거나 손상된 설치 파일과 단축키를 복원하며 **제거** 는 제품을 제거합니다. 첫 설치 범위와 목적지 선택은 유지 관리에서 의도적으로 다시 제공되지 않습니다. 해당 계약을 다시 실행하거나 현재 사용자 및 모든 사용자 범주 간에 변경하려면 먼저 등록된 제품을 제거해야 합니다. 3- 속성 ProductVersion 는 더 높은 값을 가질 때 동일한 설치 컨텍스트에서만 주요 업그레이드를 수행하며, 동일한 ProductVersion 로 다른 바이트를 다시 게시하는 것은 금지됩니다.

일반 Windows CPack 출력을 게시하지 마십시오. `windeployqt`, 단계적 PE 폐쇄 확인 또는 Authenticode 서명을 실행하지 않으며 의도적으로 `Congregation-1.0.0-Windows-unsigned-cpack-incomplete.zip`라는 이름이 지정되었습니다.

변경 가능한 스테이징 디렉터리가 아닌 변경 불가능한 릴리스 아카이브를 감사하세요. 사이드카 해시를 비교하고 표준 ZIP를 새로운 임시 디렉터리에 추출한 후 포함된 실행 파일을 검사합니다.

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

소비자 신뢰할 수 있는 Authenticode 체인은 게시자 신원이 알 수 없는 것으로 표시되지 않도록 방지하지만, Microsoft Defender SmartScreen 도 게시자 평판과 파일별 평판을 평가하므로 새 릴리스 해시에도 여전히 경고가 표시될 수 있습니다. 빌드 머신이 개인 루트를 신뢰하기 때문에 유효하다고 보고된 서명은 소비자 Windows 설치에서 이를 신뢰한다는 증거가 아니며, 깨끗한 스톡 Windows 머신이나 동등한 격리된 러너에서 릴리스를 확인해야 합니다. 일반 소비자 머신에서 자체 서명된 인증서는 신뢰되지 않으며 공개 게시자 신원을 해결하지 못합니다.

일반 Windows 시작은 진단 파일을 열거나 flush하지 않는다. `Main.qml`은 LVRS 기본 애플리케이션 창의 기하를 사용하며 너비·높이·최소 크기를 덮어쓰지 않는다. 처음 표시하기 전에 공통 Qt 진입점은 숨겨진 LVRS 창의 종횡비를 측정하고 너비 1,280 논리 픽셀을 요청하며, 해당 종횡비로 높이를 도출한다. 선택한 화면에 들어가지 않을 때만 두 크기를 함께 줄인다. 완성된 창을 표시한 뒤에는 보이는 상태에서 창 크기를 다시 변경하지 않는다. 같은 표시 전 규칙은 Cocoa와 Linux compositor가 너무 큰 첫 프레임을 눈에 보이게 수정하지 않도록 한다. 따라서 비동기 캔버스 `Loader`는 최상위 크기를 다시 협상할 수 없지만 사용자는 시작 후 창 크기를 변경하거나 최대화할 수 있다. 선택적 시작 시간 추적은 `Congregation.exe` 실행 전에 `$env:CONGREGATION_STARTUP_TRACE = "1"`을 설정한다. 시작 단계는 `%TEMP%\Congregation-startup.log`에 추가된다. 치명적인 루트 QML 객체 생성 실패는 추적을 비활성화해도 해당 파일에 기록하고 flush하지만 일반 Qt/QML 경고는 그곳으로 리디렉션하지 않는다. Windows에서 `LVRS.dll`의 프로시저 진입점 `__cxa_thread_atexit`를 찾지 못했다고 보고하면, 이는 Congregation의 애플리케이션 로깅 전에 발생한 실패이며 QML 로딩 실패가 아닌 플랫폼 런타임 불일치를 뜻한다. Congregation에 사용한 것과 같은 Qt MinGW kit으로 LVRS를 재빌드하고 `build-windows.ps1 -Clean -SkipPackage`를 다시 실행하며 갱신한 `dist/Congregation-Windows` 페이로드로 설치 프로그램을 다시 생성한다.

패키지를 만들지 않고 현재 사용자 스모크 설치를 수행하려면 다음을 실행하세요.

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -SkipPackage -InstallForCurrentUser
```

이는 기본적으로 런타임 를 `%LOCALAPPDATA%\Programs\Congregation` 로 복사하고 현재 사용자의 시작 메뉴에 단축 아이콘을 생성합니다. `-InstallDir <path>` 는 현재 사용자의 로컬 애플리케이션 데이터 디렉토리 아래에서만 위치를 오버라이드할 수 있습니다. 기존 비어 있지 않은 타겟은 Congregation 의 소유권 마커 또는 버전화된 `Congregation.exe` 를 포함해야 하며, 드라이브 루트, 사용자 데이터 루트, 재파스 포인트, 관련 없는 디렉토리 및 단계화된 소스는 재귀적 삭제 전에 거부됩니다. Qt 키트가 MSVC 기반이고 `cl.exe` 가 이미 `PATH` 에 있지 않다면, 스크립트는 `vswhere.exe` 를 통해 Visual Studio C++ 빌드 환경을 로드하려고 시도하며, 그렇지 않으면 VS 에 대한 개발자 PowerShell 에서 실행합니다.

<a id="prepared-website-release-path-through-signpath-foundation"></a>

### SignPath 재단을 통한 웹사이트 출시 경로 준비

비비용 웹사이트 배포 경로는 Foundation 이 명시적으로 Congregation 를 승인한 후 SignPath Foundation 오픈소스 스폰서십을 사용하도록 준비되어 있습니다. 프로젝트에 대해 현재 SignPath Foundation 인증서가 활성화되어 있지 않습니다. 12 월 31까지, 2026, `.github/workflows/windows-signpath-release.yml` 는 `submit-for-signing` 가 거짓일 때 명시적으로 이름이 지정된 서명되지 않은 웹사이트 MSI 를 빌드합니다. 서명되지 않은 경로는 전체 테스트, 대응 소스, 법적 자료, PE 종료, MSI 데이터베이스, 및 체크섬 게이트를 유지하지만 Authenticode 발행자 신원을 제공할 수 없으며 Windows 알 수 없는 발행자 또는 SmartScreen 경고가 표시될 수 있습니다. 승인 후 동일한 워크플로우는 별도로 격리된 SignPath 입력을 제출하고 반환된 MSI 및 중첩 실행 가능 서명을 모두 확인할 수 있습니다.

`build-windows.ps1 -ExternalSigning -SkipPackage -CreateMsi` 는 `-AllowUnsignedPackage` 의 의도적으로 분리된 상태로 남습니다. 두 모드 모두 릴리스 또는 MinSizeRel 만 허용하며, 완전한 테스트 스위트와 공개 대응 소스 증거를 필요로 하며 MSI 에서만 CI 전용입니다. 서명되지 않은 모드는 `build/Congregation-1.0.0-Windows-unsigned.msi` 를 생성하고 외부 서명은 배포 불가능한 `build/signpath-input/Congregation-1.0.0-Windows.msi` 를 생성합니다. 모드는 결합될 수 없습니다.

제출을 활성화하기 전에 저장소 소유자는 다음을 수행해야 합니다.

1. SignPath 재단 OSS 프로그램으로부터 Congregation에 대한 승인을 얻습니다.
2. 이 저장소에 대해 SignPath GitHub 앱을 설치하고 GitHub 및 SignPath에 MFA를 유지합니다.
3. MSI 내부의 Congregation 소유 `Congregation.exe`, iiUpdateManager 런타임, iiLicenseManager 런타임을 서명한 후 MSI 컨테이너를 서명하는 SignPath 산출물 구성을 설정한다. 상위 공급 측 Qt, LVRS, iiPaintEngine, MinGW 바이너리는 자신의 게시자 신원을 유지하거나 OSS 정책이 허용하는 서명 없는 상태로 둔다;
4. SignPath 릴리스 서명 정책에서 수동 승인이 필요합니다.
5. 저장소 변수 `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG`, `SIGNPATH_SIGNING_POLICY_SLUG`, `SIGNPATH_ARTIFACT_CONFIGURATION_SLUG`, `CONGREGATION_CORRESPONDING_SOURCE_URL`, `CONGREGATION_CORRESPONDING_SOURCE_SHA256` 및 검토된 `IILICENSEMANAGER_COMMIT`와 `SIGNPATH_API_TOKEN` 저장소 비밀을 구성합니다.
6. `submit-for-signing`가 활성화된 불변 릴리스 태그에서 워크플로를 전달합니다.

그때까지 `submit-for-signing` 를 비활성화한 상태에서 정확한 검토된 커밋에서 워크플로우를 발송하고 SHA-256 를 iisacc .com 에 기록한 후에만 `Congregation-website-release-unsigned` 아티팩트를 게시합니다. 별도의 `windows-corresponding-source.yml` 워크플로우는 인증된 GitHub 릴리스에 첨부해야 하는 아카이브 및 사이드카를 생성하며, MSI 빌드 변수가 업데이트되기 전에 완료됩니다.

워크플로우는 온라인 설치자가 별도로 노출하는 Qt 6.8.3 모듈만 요청하며, Qt SVG 는 기본 데스크톱 패키지의 일부로 남습니다. LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, 및 iiLicenseManager 는 공유 캔버스와 라이선스 관리자 테스트 수트를 Congregation 구성 전에 실행하는 별도의 관찰 가능한 단계에 설치됩니다. Congregation 1.0.0 는 포인터 이벤트 병합을 비활성화하고 터치된 샘플 라이브 미리보기 업데이트를 수행하는 iiPaintEngine 리비전을 고정하며, 일치하는 대형 캔버스 iiSharedCanvas 렌더러 리비전과 함께 Windows 패키지가 macOS 상호작용 동작과 일치하도록 합니다. 고정된 iiPaintEngine 설치기는 단일 구성 생성기를 Release 로 구성하여 Congregation Release 구성에 사용할 수 있는 `IMPORTED_IMPLIB_RELEASE` 를 내보냅니다. GitHub 호스트된 러너는 고정된 LVRS 의존성을 `MinSizeRel` 로 빌드하고 일반 IPO 스위치와 플랫폼 빌드 최적화를 모두 비활성화하며, 후자는 독립적으로 구성별 LTO 를 활성화하고 생성된 QML 리소스를 링크하는 동안 MinGW 13 내부 컴파일러 오류를 발생시킬 수 있습니다. Congregation 자체는 Release 로 빌드되며 완전한 기능 및 패키징 검증을 실행합니다. 워크플로 는 Congregation 에 대한 기존 중첩 Authenticode 검증과 모든 향후 `Congregation-website-release` 서명 아티팩트가 생성되기 전에 관리자 런타임 두 개를 포함하여 `Congregation-website-release-unsigned` 만 명시적 서명되지 않은 브랜치에서 업로드합니다.

<a id="prepared-no-cost-website-release-path-through-necessary"></a>

### Necessary를 통한 무료 웹사이트 출시 경로 준비

Congregation 는 [필수 코드 서명](https://sign.necessary.nu/) 에 2026-07-30에 신청서를 제출했습니다. 서비스 는 Congregation 를 자격 있는 실제 유틸리티로 인정하고 접근을 발급하기 전에 관리자 신원 및 주소 검증을 요청했습니다. 공중보건 엔드포인트는 HSM 와 인증서 가용성을 모두 보고했지만, 프로젝트 토큰은 아직 발급되지 않았습니다. 따라서 현재 Congregation MSI 는 이 서비스를 통해 공개적으로 서명된 것으로 설명될 수 없습니다. 프로젝트 자격 요건, 신청서 접수, 또는 HSM 의 건강한 상태는 어떤 Congregation 바이트가 소비자 신뢰할 수 있는 서명을 포함한다는 증거가 아닙니다.

이 통합은 상위 공급 측 [osslsigncode 2.14 릴리스](https://github.com/mtrojnar/osslsigncode/releases/tag/2.14)를 사용하며, 로컬에서 재구현된 Authenticode 인코더를 사용하지 않습니다. 프로젝트는 GPL-3.0 -or-later 라이선스로 적극적으로 유지 관리되며, 문서화된 OpenSSL 링크 예외를 지원하고, PE, MSI, 분리된 서명, 서명 첨부, 및 RFC 3161 타임스탬핑을 지원합니다. Windows x64 릴리스 아카이브는 SHA-256 `9a1722aaf62a27852c4eb9c35749a0248065052d0ae0a93d4ed6bb49def027f2` 에 고정되어 있으며, `.github/workflows/windows-necessary-release.yml` 는 다운로드된 바이트 수가 다르면 실행을 거부합니다. 설치된 애플리케이션에 라이브러리 또는 네트워크 의존성을 추가하지 않는 릴리스 전용 도구입니다. 필요한 것은 여전히 외부 서비스 의존성으로, 무료 OSS 액세스, 인증서, 가용성 및 회수 정책이 변경되거나 취소될 수 있습니다.

Congregation의 승인이 완료된 뒤 발급된 토큰은 GitHub Actions 저장소 비밀 `NECESSARY_SIGN_TOKEN`에만 저장한다. 명령줄로 전달하거나 저장소 변수에 넣거나 출력하거나 아티팩트에 첨부하거나 유지보수 문서에 복사하지 않는다. 변경 불가능한 `v<project-version>` 태그에서 `submit-for-signing`를 활성화하여 `Windows Necessary public release`를 수동으로 실행한다. 비밀이 없거나, 선택한 ref가 일치하는 태그가 아니거나, 대응 소스 근거가 없거나, build/test/package 검사 중 하나라도 실패하면 워크플로는 안전하게 거부하는 상태를 유지한다.

워크플로우는 먼저 `build-windows.ps1 -ExternalSigning -SkipPackage -CreateMsi`를 실행하여 Release 빌드·전체 CTest 실행·법적 자료 검사·PE closure 검사·WiX 작성·ICE105·MSI 데이터베이스 검증만 재사용한다. 결과 `build/signpath-input` 파일은 서명되지 않은 내부 입력으로 유지하며 Necessary 경로에서 게시하지 않는다. 이어 `tools/necessary-authenticode.ps1`로 각 Congregation 소유 또는 그 밖의 서명되지 않은 스테이징 PE에서 SHA-256 Authenticode 페이로드를 추출한다. 분리된 해당 페이로드만 HTTPS로 고정 `https://sign.necessary.nu/windows/sign` 엔드포인트에 게시하며, bearer 토큰은 요청 인증 헤더에만 전달한다. 보조 도구는 반환된 서명을 임시 복사본에 붙이고 독립적인 SHA-256 RFC 3161 타임스탬프를 추가한다. Windows가 `Necessary Innovations AB`의 유효한 공개 신뢰 서명을 보고하고 Windows SDK SignTool이 `/pa /all /tw`를 수락할 때까지 원본은 수정하지 않는다.

한 실행에서 Necessary로 서명하는 모든 파일은 같은 인증서 지문을 사용해야 한다. 유효한 기존 상위 공급 측 서명은 유지하고 `Congregation.exe`, `LVRS.dll`, `libiiPaintEngine.dll`는 승인된 릴리스 인증서에 다시 결합한다. `tools/necessary-windows-release.ps1`는 공통 로컬/CI 실행 절차를 담당한다. 기존에 검증한 WiX 소스를 다시 컴파일하고 서명된 스테이징에서 MSI를 다시 링크하며, Smoke ICE105를 재실행하고, 같은 분리 서명 흐름으로 임시 MSI 후보를 서명한 뒤 MSI 데이터베이스 계약을 재검사한다. 이어 관리 설치 이미지를 만들고 설치된 Congregation 소유 바이너리 3개 전체의 타임스탬프·Publisher·인증서 지문·SignTool 정책을 독립적으로 검증한다. 후보 검사 중 하나라도 실패하면 기존 표준 MSI와 체크섬은 그대로 두며, 완전히 검증한 후보만 일치하는 SHA-256 사이드카와 함께 `build/Congregation-<version>-Windows.msi`에 원자적으로 게시한다. 해당 파일 또는 동일한 압축 해제 상태의 `Congregation-website-release` 워크플로우 산출물만 새 기기 설치/실행/제거 검사와 후속 게시 후보이다. 예상하는 Windows Publisher는 `Necessary Innovations AB`이다. 후원 서비스가 미등록 제품명 Congregation을 인증서 가입자라고 주장하도록 인증서를 만들 수 없기 때문이다.

제공자가 토큰을 발행한 후, 해당 릴리스는 토큰을 명령어 히스토리에 기록하지 않고 로컬에서 완료할 수 있습니다. 먼저 일치하는 공개 대응 소스 URL 와 SHA-256 를 사용한 외부 서명 빌드를 실행한 다음, 토큰을 프로세스 범위의 `NECESSARY_SIGN_TOKEN` 로만 사용 가능하게 하고, 고정된 `osslsigncode.exe`, Windows, SDK, `signtool.exe`, WiX, 3.14 디렉토리를 해결한 다음 호출합니다.

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

쉘 명령어에 리터럴 토큰을 할당하거나 사용자/머신 환경 변수에 영구 저장하지 마십시오. 실행 직후 프로세스 범위의 값을 지우십시오. 로컬 명령어는 대체 build/stage 디렉토리를 거부하고, 유효한 타임스탬프 된 벤더 서명을 보존하며, Congregation 소유 또는 기타 서명되지 않은 PE 파일만 서명하고, 모든 새 서명을 첫 번째 반환된 인증서 엄밀한 지문과 연결하며, 모든 외부 및 중첩 확인이 통과될 때까지 정준 MSI 를 보유합니다.

<a id="1c-microsoft-store-msix"></a>

## 1c. 마이크로소프트 스토어 MSIX

Microsoft Store MSIX 는 Congregation 의 인증서 없는 공개 Windows 배포 경로입니다. Microsoft 는 인증 후 MSIX 를 다시 서명하므로 발행자는 공개 OV / EV 인증서를 구매하거나 갱신하거나 수출하거나 보호하지 않으며, 스토어 설치에 대해 SmartScreen 알 수 없는-발행자 경고가 표시되지 않습니다. 이는 실제 MSIX 제출에만 적용됩니다. Microsoft 는 별도의 Win32 설치 경로로 제출된 MSI 나 EXE 를 다시 서명하지 않습니다. Microsoft 의 현재 [Windows 코드 서명 옵션](https://learn.microsoft.com/windows/apps/package-and-deploy/code-signing-options) 과 [수동 데스크톱 MSIX 패키징 가이드](https://learn.microsoft.com/windows/msix/desktop/desktop-to-uwp-manual-conversion)를 참조하세요.

`build-windows-store.ps1` 는 스토어가 아닌 ZIP / MSI 서명 경로와 별도로 이 워크플로우를 소유합니다. 그것은 테스트된 `dist/Congregation-Windows` 런타임 를 재사용하고, 파트너 센터 신원 및 정확한 예약 앱 이름을 `AppxManifest.xml` 에 기록하며, 표준 1024 px 아이콘에서 정확한 스토어 PNG 자산을 생성하고, 설치된 x64 Windows SDK MakeAppx 로 패키징하며, `dist/` 하에 스토어 파일 및 SHA-256 사이드카를 게시합니다. 예약 이름은 `Package/Properties/DisplayName` 와 `uap:VisualElements/@DisplayName` 를 모두 구동하며, 패키지 신원과 발행자가 정확하더라도 패키지 수준의 예약되지 않은 이름은 파트너 센터가 거부합니다. 매니페스트는 x64이며, 패키지 버전 `1.0.0.0` 를 사용하고, `Windows.Desktop` 에서 19041빌드를 대상으로 하며, `uap10:RuntimeBehavior="packagedClassicApp"`, `uap10:TrustLevel="mediumIL"`, `privateNetworkClientServer` 를 인접한 Congregation 발견을 위해 선언하고, `runFullTrust` 를 선언합니다. 네 번째 버전 필드는 스토어 사용을 위해 예약되어 있으며 0로 유지되어야 합니다.

<a id="local-self-signed-msix-verification"></a>

### 로컬 자체 서명된 MSIX 확인

자체 서명은 개발 또는 중앙에서 관리되는 개인 장치에만 유효합니다. 다운로드를 공개적으로 신뢰할 수 있게 만들지는 않습니다. 따라서 개발 출력은 `build/development-only` 아래에 격리되고 개발 전용 ID를 전달하며 공개 릴리스에 첨부되어서는 안 됩니다.

개발 인증서 주제는 `CN=Congregation Development Local Only` 와 정확히 일치해야 하며, 코드 서명 EKU 를 가지고 `CurrentUser\My` 에 접근 가능한 개인 키를 가지고 있어야 하고, 정확한 40-hex 서명 지문으로 선택되어야 합니다. 자신 서명된 MSIX, Windows 앱 설치자를 설치하려면 로컬 컴퓨터의 `TrustedPeople` 스토어에 공개 인증서가 추가로 필요합니다. 공개 `.cer` 만 가져오고 PFX 는 가져오지 마십시오; 이 일회성 신뢰 작업은 Elevated PowerShell 가 필요합니다. 리프 서명 인증서를 신뢰할 수 있는 루트 스토어에 두지 마십시오.

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

그런 다음 상승된 PowerShell에서 패키지 테스트를 위해 해당 공개 인증서를 신뢰합니다.

```powershell
Import-Certificate `
  -FilePath .\build\development-only\Congregation-Development-Local-Only.cer `
  -CertStoreLocation Cert:\LocalMachine\TrustedPeople
```

표시되는 Congregation 창을 빌드, 서명, 타임스탬프, 설치, 활성화하고 관찰하고 하나의 명령으로 패키지를 제거합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows-store.ps1 -Mode Development -InstallDevelopment
```

SignTool가 패키지를 검증하고 설치 트리에 실행 파일과 법적 고지문이 있는 경우에만 명령이 성공한다. `shell:AppsFolder\<PackageFamilyName>!Congregation`로 실행하고 보이는 네이티브 창을 기다린 뒤, 자신이 실행한 프로세스만 중지하고 개발 패키지를 제거하며 등록 해제를 확인한다. `-SkipBuild`는 이미 테스트한 런타임 스테이징을 대상으로 개발 검사를 반복할 때만 허용한다. 출력은 `build/development-only/Congregation-1.0.0-Windows-Sideload-Development-x64.msix`이다. 옆에 있는 인증서는 공개 키 자료만 담지만 두 파일 모두 공개 릴리스 산출물은 아니다.

<a id="free-store-account-and-product-identity"></a>

### 무료 스토어 계정 및 제품 ID

법적 계정 소유자는 신원 민감 단계를 완료해야 합니다. [storedeveloper.microsoft.com](https://storedeveloper.microsoft.com/) 에서 시작하여 현재 무료 온보딩 흐름을 사용하도록 한 후, 해당 제품을 소유할 Microsoft 계정에 로그인하거나 계정을 생성합니다. 개별 계정은 계정 소유자의 정부 발급 사진 ID 와 셀카 인증이 필요하며, 해당 검증된 개인 신원 하에 게시됩니다. 회사 계정은 조직에 대한 권한과 Microsoft 가 요청하는 비즈니스 증거가 필요합니다. 계정 유형, 법적 계약, 신원 증명, 그리고 최종 게시 작업은 빌드 스크립트에 위임할 수 없으며, 개별 계정은 나중에 회사 계정으로 간단히 변환될 수 없습니다.

확인 후:

1. 파트너 센터를 열고 **앱 및 게임**, **새 제품**를 선택한 다음 **MSIX 또는 PWA 앱**를 선택합니다.
2. Congregation의 정확한 공개 표시 이름을 검색하고 예약하세요. 이전 제품 예약으로 인해 이 새 이름의 소유권이 설정되지는 않습니다. 파트너 센터에서 반환된 정확한 값을 사용하세요. 제출에 사용되지 않으면 예약이 만료될 수 있습니다.
3. **상품관리 > 상품아이덴티티**를 오픈합니다.
4. 이  4  값을 정확히 복사하여 대문자, 공백, 쉼표 및 문법을 그대로 유지하세요:
   - `Package/Properties/DisplayName`에 대해 예약된 앱 이름
   - `Package/Identity/Name`
   - `Package/Identity/Publisher`
   - `Package/Properties/PublisherDisplayName`
5. 현재 사용자 환경에서는 비밀이 아닌 값만 저장합니다.

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

고안된 ID 또는 게시자는 다른 패키지 제품군을 생성하는 반면 예약되지 않은 표시 이름은 패키지 유효성 검사에 실패합니다. 파트너 센터는 두 경우 모두 거부합니다. 로컬 개발 게시자 및 표시 이름은 의도적으로 Store 값과 관련이 없으며 대체되어서는 안 됩니다.

<a id="legal-release-gate"></a>

### 법적 릴리스 게이트

서명 저장은 발행인의 라이선스 의무를 대체하지 않습니다.  iiPaintEngine  저작권 소유자가  `AGPL-3.0-only` 를 승인했으며, 설치된 엔진 패키지는 이제 해당 정확한 라이선스를 제공합니다.  Congregation 의 스토어 단계를 위해서는 비어 있지 않은  `legal/iiPaintEngine/LICENSE.txt`, 모든 다른 단축된 제 3 자 공지 사항, 그리고 해당 릴리스에 대한 소스 제공이 필요합니다.

배포되는  Congregation,  LVRS,  iiPaintEngine, 패치/빌드 필수 의존성 소스, 빌드 스크립트, 라이선스 자재를 위한 완전한 소스 아카이브를 생성하고, 발행인이 제어하는  HTTPS 위치에 게시하며, 해당 바이트의  SHA-256 를 기록하세요. 증거를 다음과 같이 구성하세요:

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

iiPaintEngine 라이선스, HTTPS URL, 해시, 법적 고지, 정확한 제품 ID, 현재 릴리스 빌드 또는 테스트가 누락된 경우 MakeAppx 이전의 스토어 모드 안전하게 거부한다. 개발 패키지를 공개 패키지로 바꾸거나 자리 표시자로 돌아가지 않습니다.

<a id="create-and-submit-the-store-upload"></a>

### 스토어 업로드 생성 및 제출

제품 ID 및 법적 증거가 완료되면 다음을 실행합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\build-windows-store.ps1 -Mode Store
```

이는 항상 릴리스 모드에서 다시 빌드되고 전체 테스트 모음을 실행합니다. 다음을 생성합니다.

- `dist/Congregation-1.0.0-Windows-Store-x64.msix`, 스토어 수집을 위해 의도적으로 서명되지 않았습니다.
- `dist/Congregation-1.0.0-Windows-Store-x64.msixupload`, ZIP 컨테이너는 정확히 x64 MSIX를 보유하고 있으며 가짜 기호 아카이브는 없습니다.
- 각 파일에 대한 `.sha256` 사이드카.

MinGW 는 Microsoft  PDB 를 생성하지 않으므로  `.appxsym` 는 의도적으로 생략됩니다. 개인 개발 인증서로 스토어 업로드에 서명하지 마세요. 제출의  **Packages** 페이지에  `.msixupload` 를 업로드하세요. 파트너 센터는 공식 매니페스트, 악성 코드, 정책 및 기술 인증을 수행한 후 패키지 서명을 Microsoft 의 스토어 서명으로 대체합니다.

Congregation는 네이티브 Qt Win32 데스크톱 애플리케이션이므로 제출 옵션에서 제한된 `runFullTrust` 기능을 설명해야 합니다. 제출된 빌드에 대해 실제로 사실인지 확인한 후에만 다음을 사용하십시오.

> Congregation 는 네이티브  Qt 6 Win32 데스크톱 이미지 편집 응용 프로그램입니다. 주요 실행 파일은 중간 무결력으로 실행되며, 사용자가 명시적으로 선택한 파일을 여닫기 위해 표준 Win32 데스크톱 API 를 사용합니다. 드라이버나 서비스를 설치하거나, 권한 상승을 요청하거나,  HKLM 를 수정하거나, 전체 머신 구성을 수행하지 않습니다.  runFullTrust 기능은 패키지가 전통적인 풀 트러스트 Win32 데스크톱 실행 파일을 포함하고 있기 때문에 필요합니다.

계정 소유자는 또한 목록 설명과 카테고리, 가격 및 시장을 완료해야 하며, 애플리케이션의 실제 동작에 기반한 최소 하나의 데스크톱 스크린샷, 지원 연락처, 개인정보/데이터 처리 성명서를 완료해야 하며, IARC 연령 등급 질문지, 추가 오픈 소스 라이선스 고지, 그리고 최종 제출 동의를 완료해야 합니다. 추측된 개인정보 성명서를 게시하지 마십시오. Windows 앱 인증 키트는 선택형 로컬 사전 검사 사전 검사이며, 권한 상승이 필요하며, 그 결과는 파트너 센터 인증을 대체하지 않습니다.

<a id="2-configure-the-release-build"></a>

## 2. 릴리스 빌드 구성
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target Congregation
```
다른 번들 식별자가 필요한 경우 구성하기 전에 `CMakeLists.txt`에서 `BUNDLE_ID`를 업데이트하세요. 최신 또는 이전 macOS 릴리스를 지원해야 하는 경우 배포 대상을 조정합니다.

<a id="3-stage-the-app-bundle"></a>

## 3. App Bundle 준비
빌드된 앱은 `build/Congregation.app` 아래에 있습니다. 빌드 트리를 건드리지 않고도 배포 도구를 안전하게 실행할 수 있도록 준비 디렉터리(예: `build/Congregation.app`)에 복사하세요.

<a id="4-embed-qt-frameworks"></a>

## 4. Qt 프레임워크 포함
App Store 모드에서 `macdeployqt`를 실행하여 필수 Qt 프레임워크 및 QML 플러그인을 포함합니다.
```bash
macdeployqt "build/Congregation.app" \
  -appstore-compliant \
  -qmldir=src/App/qml \
  -always-overwrite
```
이제 모든 `.framework` 번들이 `build/Congregation.app/Contents/Frameworks` 내에 있고 `qt.conf`가 `Contents/Resources/`에 있는지 확인합니다.

<a id="5-prepare-metadata"></a>

## 5. 메타데이터 준비
생성된 `Info.plist`(`build/Congregation.app/Contents/` 내부)를 다음과 같이 업데이트합니다.
- `CFBundleIdentifier`는 번들 ID와 일치합니다.
- `CFBundleShortVersionString`는 마케팅 버전 `1.0.0`로 설정되고 `CFBundleVersion`는 새 번들 식별자에 대해 App Store 빌드 번호 `1`로 초기화되었습니다.
- `CFBundleIconFile`는 번들 `resources/Appicon.icns` 파일을 확인해야 합니다. Windows는 생성된 리소스 스크립트를 통해 `resources/Appicon.ico`를 포함하여 빌드합니다.
- `NSLocalNetworkUsageDescription`는 Congregation가 로컬 네트워크를 사용하여 근처의 Congregation 사용자를 찾고 사용자가 선택할 때 캔버스를 공유한다는 사실적인 설명을 포함합니다. 현재 macOS 버전에서 첫 번째 로컬 검색 작업 시 프롬프트가 예상됩니다.

<a id="6-sandbox-entitlements"></a>

## 6. 샌드박스 자격
`packaging/macos/Congregation.entitlements` 는 앱 샌드박스를 활성화하고, 사용자에게 선택된 파일 및 사진 라이브러리에 대한 읽기/쓰기 접근 권한을 부여하며, `com.apple.security.network.client` 와 `com.apple.security.network.server` 를 모두 부여합니다. UDP 멀티캐스트 발견은 데이터그램을 전송하고 수신 소켓을 바인딩하는 반면, 명시적 캔버스 공유는 로컬 TCP 연결을 수락하고 엽니다. 따라서 두 네트워크 방향 모두 의도적입니다. 향후 권한 추가는 최소한으로 유지하여 앱 검토 승인 확률을 높입니다.

<a id="7-codesign-the-bundle"></a>

## 7. 번들 공동 설계
```bash
codesign --force --options runtime \
  --entitlements packaging/macos/Congregation.entitlements \
  --sign "Apple Distribution: MUYEONG YUN (5U49ST9XZH)" \
  "build/Congregation.app"
```
그런 다음 서명을 검증합니다.
```bash
codesign --verify --deep --strict "build/Congregation.app"
spctl --assess --type execute "build/Congregation.app"
```
`spctl`가 강화된 런타임 누락에 대해 경고하는 경우 `--options runtime`가 전달되었는지 확인하세요.

<a id="8-create-the-installer-package"></a>

## 8. 설치 프로그램 패키지 생성
```bash
productbuild \
  --component "build/Congregation.app" /Applications \
  --sign "Apple Installer: MUYEONG YUN (5U49ST9XZH)" \
  "dist/Congregation.pkg"
```
그러면 App Store Connect에 필요한 설치 프로그램 페이로드가 생성됩니다. `.pkg`를 4 GB 아래에 유지합니다.

`dist/Congregation.pkg` 또는 `dist/Congregation-appstore.pkg`를 설치했을 때 이전 앱 아이콘이 보이면 패키지가 오래된 것으로 취급한다. 기본 `./build.sh`는 Developer ID 서명·공증·stapling·최종 검증에 성공한 뒤에만 `dist/Congregation.pkg`를 갱신한다. `CONGREGATION_BUILD_MODE=mas ./build.sh`는 App Store 패키지를 갱신하고 `./build.sh local`는 2개 `-local-unsigned.pkg` 산출물만 기록한다. 패키지 페이로드를 직접 검사할 수 있다:

```bash
pkgutil --payload-files dist/Congregation.pkg | grep 'Contents/Resources/.*icns'
```

페이로드는 `./Congregation.app/Contents/Resources/Appicon.icns`를 나열해야 하며 `./Congregation.app/Contents/Resources/icon.icns`를 나열해서는 안 됩니다.

이 페이로드 확인이 통과한 후에도 Transporter 의 Active 목록이 이전 아이콘을 계속 표시하면, 해당 큐 썸네일을 `.pkg` 가 여전히 이전 아이콘을 임베드한다는 증거로 간주하지 마십시오. Active 목록은 앱 이름과 Apple ID 를 통해 전달 전에 App Store Connect 기록을 식별할 수 있으므로 항목을 제거하고 다시 추가한 후 새로 재구성된 `dist/Congregation-appstore.pkg` 를 드래그했는지 확인하고 패키지 페이로드를 다시 검사하십시오.

```bash
pkgutil --expand-full dist/Congregation-appstore.pkg /tmp/congregation-appstore-payload
cmp resources/Appicon.icns /tmp/congregation-appstore-payload/com.iisacc.congregation.painter.pkg/Payload/Congregation.app/Contents/Resources/Appicon.icns
```

`cmp` 명령이 성공하면 업로드 패키지에 현재 앱 아이콘이 포함됩니다. Transporter가 배송 전에 이전 스토어 목록 아이콘을 계속 표시하는 경우 App Store Connect 앱 기록 아이콘을 별도로 업데이트하거나 새로 고칩니다.

Dock 또는 Launchpad 가 설치된 번들이 올바르게 된 후에도 이전 아이콘을 계속 표시하면, 오래된 `/Applications/Congregation.app` Dock 항목을 제거하고 LaunchServices 가 재구성된 앱을 다시 등록하도록 하십시오. 고정된 Dock 항목은 작업 공간 `build/Congregation.app` 가 새 아이콘을 갖는 경우에도 여전히 오래된 또는 제거된 번디 경로에 가리킬 수 있습니다.

<a id="9-upload-to-app-store-connect"></a>

## 9. App Store Connect에 업로드
1. 트랜스포터를 엽니다.
2. `dist/Congregation-appstore.pkg`를 대기열로 드래그합니다.
3. App Store Connect 자격 증명을 제공하고 업로드하세요.
4. Transporter가 보고하는 유효성 검사 문제(아이콘 누락, 자격 불일치 등)를 해결합니다.

<a id="10-post-upload-checklist"></a>

## 10. 업로드 후 체크리스트
- 스크린샷, 현지화된 설명, 가격이 포함된 App Store Connect 기록을 생성하세요.
- 업로드된 빌드를 새 버전 제출에 첨부하고 수출 규정 준수 설문지를 작성하세요.
- 검토를 위해 제출하세요.

<a id="troubleshooting-tips"></a>

## 문제 해결 팁
- 빌드 트리에 대한 절대 경로가 남아 있지 않도록 하려면 `otool -L build/Congregation.app/Contents/MacOS/Congregation`를 사용하세요.
- `macdeployqt` 실행 후 `plutil -p`를 활용하여 `Info.plist`를 검사합니다.
- `LC_VERSION_MIN_MACOSX` 누락으로 인해 Transporter가 업로드를 거부하는 경우 구성 시 `CMAKE_OSX_DEPLOYMENT_TARGET`가 설정되어 있는지 확인하세요.
- 매장 외부 배포에 대한 공증이 필요한 경우 동일한 권한으로 공동 설계를 다시 실행하고 `xcrun notarytool`를 통해 제출하세요. App Store 제출에는 별도의 공증이 필요하지 않습니다.
- CLion에서 `loading 'build.ninja': No such file or directory`를 보고하는 경우 빌드하기 전에 선택한 프로필에 대해 CMake 구성을 다시 실행하세요. 또한 이 프로젝트는 `$HOME/.local/SDK/LVRS` 및 `$HOME/.local/SDK/iiPaintEngine`에 대한 로컬 빌드 트리 rpath를 삽입하므로 추가 `DYLD_LIBRARY_PATH` 설정 없이 CLion 및 CTest를 실행할 수 있습니다.

<a id="linux-build-and-packaging"></a>

## Linux 빌드 및 패키징
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```
TGZ 아카이브는 `Congregation-<version>-Linux.tar.gz` 로 빌드 디렉토리에 작성됩니다. CMake 는 연결된 Qt, LVRS, iiPaintEngine 공유 라이브러리, QML 모듈, 번역 및 플랫폼 플러그인을 설치 이미지에 수집하기 위해 Qt 6.8 의 `qt_generate_deploy_qml_app_script` 를 사용합니다. Congregation 의 설치된 ELF RPATH 는 `$ORIGIN/../lib` (또는 구성된 GNU 설치 라이브러리 디렉토리) 이므로 빌드 트리나 `$HOME/.local` 런타임 의존성을 유지하지 않습니다. 아카이브는 `bin/`, `lib/`, `qml/`, `plugins/`, `share/` 레이아웃을 사용하며, 추출 후 `bin/Congregation` 로 실행합니다.

설치 이미지는 `share/applications/com.iisacc.congregation.painter.desktop` 와 `Terminal=false` 및 hicolor 애플리케이션 아이콘을 포함합니다. X11 와 Wayland 양쪽에서 릴리스 아카이브를 확인하세요: Xvfb/ XCB 스모크 실행과 headless Weston/Wayland 스모크 실행을 실행하고, `ldd bin/Congregation` 에서 미결 의존성을 검사하며, 해결된 경로가 소스 트리 `build/` 나 유지관리자의 홈 디렉토리를 가리키지 않는지 확인하세요. 설치된 트리를 선호하는 경우 `cmake --install build --prefix <path>` 를 실행하세요; 동일한 상대 레이아웃 및 배포 스크립트가 사용됩니다.

<a id="automated-tests"></a>

## 자동화된 테스트
현재 단위 테스트 도구 모음을 구성하고 실행하려면 다음 안내를 따르세요.

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target Congregation tests_canvasdocumentviewmodel tests_drawingsurfaceitem
ctest --test-dir build --output-on-failure
```

macOS 패키징 스크립트는 `src/App/qml`에서 가져온 QML를 검색합니다. 로컬 및 공증-사전 검사 회귀 픽스처는 동일한 소스 레이아웃을 사용합니다.
