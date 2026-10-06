<a id="congregation-100-application-structure"></a>

# Congregation 1.0.0 애플리케이션 구조

이 문서는 기존 QML 세션 레이어 워크플로를 유지하면서 네이티브 페인팅 표면을 iiSharedCanvas로 이동한 후 Congregation 1.0.0를 캡처합니다.

<a id="top-level-layout"></a>

## 최상위 레이아웃

- `CMakeLists.txt`(루트) 부트스트랩 Qt, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, iiLicenseManager, 패키징 및 `src/App/` 하위 디렉터리.
- `src/App/`에는 애플리케이션 번들 소스가 포함되어 있습니다.
- `src/App/models/canvas/canvasdocumentviewmodel.*`는 브러시 색상, 브러시 크기, 활성 도구 및 캔버스 크기에 대한 LVRS 방향 문서 상태를 저장합니다.
- `src/App/models/canvas/canvasviewmodelbridge.*`는 LVRS 문서 보기 모델을 통해 그리기 변형을 게이트하고 캔버스 메타데이터를 동기화합니다.
- `src/App/models/brush/paletteutils.*`는 QML에 노출된 팔레트 주문 도우미를 제공합니다.
- `src/App/models/document/psdcompatibilitydocument.*`는 PSD 가져오기/내보내기의 경계로 사용되는 내부 Photoshop 스타일 문서/레이어 매니페스트를 정의합니다.
- `src/App/models/document/psdimagereader.*`는 `psd_sdk`를 래핑하므로 Congregation는 Qt 이미지 플러그인을 사용하지 않고도 PSD 병합 이미지 데이터를 읽을 수 있습니다.
- `src/App/models/document/psdimagewriter.*`는 `psd_sdk`를 래핑하므로 Congregation는 XMP 메타데이터를 사용하여 계층화된 PSD 파일을 작성할 수 있습니다.
- `src/App/models/document/recentcanvascontainer.*`는 버전이 지정된 SHA-256 확인 내부 최근 세션 컨테이너 코덱을 소유합니다.
- `src/App/models/license/accountmanager.*`는 제품 키를 QML에 노출하지 않고 기본 설정 화면에 `iiLicenseManager 0.2` 계정 ID를 적용합니다.
- `src/App/models/license/licensemanager.*`는 보유된 온라인 Congregation 제품 라이센스 요청, 안전하게 거부하는 런타임 결정, 자격 증명 수명 주기 및 명시적 애플리케이션 적용 모드를 소유합니다. Congregation는 현재 비활성화 모드를 선택합니다.
- `src/App/models/license/licensecredentialstore.*`는 보안 자격 증명 지속성을 격리하므로 검증 정책을 변경하지 않고도 QtKeychain 프로덕션 어댑터와 메모리 내 테스트 가짜를 교환할 수 있습니다.
- `src/App/models/network/nearbycongregationdiscovery.*`는 익명 로컬 네트워크 존재 검색을 소유하고 UDP 소켓, 타이머, 인터페이스 멤버십 및 피어 만료를 전용 작업자 스레드에서 격리합니다.
- `src/App/models/network/localcanvassession.*`는 명시적인 LAN 캔버스 호스트/입력 클라이언트 프로토콜, 참가자 상태, 한계가 설정된 TCP 프레이밍 및 편집 명령 검증, 호스트 권한 있는 스냅샷 개정을 소유합니다.
- `src/App/models/collaboration/memberprofilelistbuilder.*` 는 현재 로컬 프로필 및 활성 캔버스 협업자 기록에서 멤버 프레젠테이션 목록을 생성합니다. 상위 공급 측 `isMe` 행을 중복 제거하고, 호스트를 먼저 유지하며, 정확한 역할 접미사를 할당하며, 각 제거 가능한 협업자의 소스 기록과 인덱스를 세션 삭제를 위해 보존합니다.
- `src/App/models/painting/drawingsurfaceitem.*`는 Congregation의 QML 표면 계약을 `iiSharedCanvas::CanvasItem`에 적용하고, 래스터 편집은 iiPaintEngine에 위임합니다.
- `src/App/models/preferences/applicationpreferences.*`는 민감하지 않은 일반 설정, 단일 앱-로컬 최근 캔버스 경로, 레거시 외부 경로 정리 및 `QSettings` 지속성을 소유합니다.
- `src/App/models/update/congregationupdatemanager.*`는 설치된 `iiUpdateManager 0.2` 런타임를 Congregation의 자격 증명 및 배포 채널 정책에 맞게 조정합니다.
- `src/App/qml/`에는 기본 창, 기본 설정 창, 도구 모음 및 그리기 화면에 대한 LVRS UI가 포함되어 있습니다.
- `tests/` 는 문서 뷰 모델, 일반 설정 지속성, iiLicenseManager 계정-정체성 경계, 근접 발견, 로컬 캔버스 세션, iiUpdateManager 통합, iiPaintEngine 그리기 표면 통합, 및 Windows , macOS , 및 Linux build/package 계약에 대한 Qt 테스트 타겟을 포함합니다.

<a id="build-system-overview"></a>

## 빌드 시스템 개요

1. 루트 `CMakeLists.txt`는 Qt 6, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager 0.2, iiLicenseManager를 설정합니다. 0.2, `psd_sdk`, 설치 경로, 패키징 메타데이터 및 `Congregation` 실행 대상.
2. `src/App/CMakeLists.txt`는 C++ 소스와 헤더를 `Congregation` 대상에 연결합니다.
3. `qt_add_qml_module`는 `Congregation` QML 모듈을 등록하고 `Main.qml`, `PreferencesWindow.qml`, `PainterCanvasPage.qml`, `PresentationLaserPointer.qml`, `CanvasToolBar.qml`를 노출합니다. `HslTriangleColorPicker.qml` 및 `DrawingSurface.qml`.
4. 실행 가능 링크는 Qt 코어, 네트워크, QML , 퀵, 퀵 컨트롤 2, SVG , `iiSharedCanvas::iiSharedCanvas` , 그 iiPaintEngine 의존성, `iiUpdateManager::iiUpdateManager` , `iiLicenseManager::iiLicenseManager` , 및 정적 QtKeychain 0.17.0 를 macOS / Windows 에 연결하며, LVRS 는 런타임 QML 가져오기 처리를 구성합니다. QtKeychain 는 커밋 `875f77d9f61bd97fd84cca47ce3bc71186dfbd09` 에 고정되어 있으며, 번역 없이 또는 자체 테스트 없이 빌드되고, 불안전한 대체 경로 를 사용하지 않습니다.
5. Windows 에서, `Congregation` 대상은 GUI 서브시스템을 사용하므로 콘솔 창 없이 시작됩니다; 그 수명은 `QGuiApplication` 이벤트 루프 및 최상위 창에 의해 소유됩니다. 컴파일된 리소스는 아이콘, 파일/제품 버전, `asInvoker` 매니페스트, Windows 10/11 호환성, 모니터별 V2 DPI 인식, 및 긴 경로 인식을 제공합니다.
6. Windows 빌드 후 단계는 LVRS , iiPaintEngine , iiSharedCanvas , iiUpdateManager , iiLicenseManager , 및 런타임 DLL 을 선택된 MinGW 컴파일러 옆으로 `build/` 로 복사하여 직접 빌드 트리 실행이 일관된 툴체인 를 해결합니다. 릴리스 패키징은 LVRS QML 을 LVRS 바이너리에 내장하고 중복된 느슨한 `qml/LVRS` 배포 트리를 제거합니다.
7. Linux 는 GNU `bin/lib/qml/plugins/share` 레이아웃, `Terminal=false` 데스크톱 엔트리, 상대 ELF RPATH, 그리고 Qt 의 생성된 QML 배포 스크립트를 사용하며, macOS 은 앱 번들로 남고 Windows 는 GUI -서브시스템 실행 가능 파일로 런타임 에 평평하게 배치된 상태로 남습니다.
8. `BUILD_TESTING=ON`, `tests/CMakeLists.txt`는 Windows 실행 가능 리소스 및 Windows의 PE 계약 검사와 모든 호스트의 정적 macOS 및 Linux 패키징 계약을 포함하여 활성 단위 테스트 대상을 등록합니다.

<a id="runtime-entry-point-srcappmaincpp"></a>

## 런타임 진입점(`src/App/main.cpp`)

- 표준 `QGuiApplication`/`QQmlApplicationEngine` 진입점을 시작하여 Windows가 내보내지지 않은 LVRS C++ 대신 LVRS의 내보낸 QML 모듈 표면에 대해서만 연결되도록 합니다. 런타임 도우미.
- CMake의 `PROJECT_VERSION` 컴파일 정의를 `QGuiApplication::applicationVersion`로 게시하고 런타임 버전을 macOS plist 및 Windows 패키지 메타데이터와 동일한 소스에 유지합니다.
- QML를 로드하기 전에 LVRS의 내보낸 `qml_register_types_LVRS()` 등록 기능을 호출하므로 `WindowSafeAreaObserver` 및 `ViewModels` 싱글턴와 같은 LVRS 제공 유형은 전체 없이 사용할 수 있습니다. LVRS 앱 부트스트랩.
- 패키지된 `qml/` 디렉터리의 권한을 유지하고, 번들 LVRS 모듈이 없는 경우에만 명시적 `LVRS_HOST_PREFIX`를 허용하며, 패키지 실행 중에 사용자 홈 `.local/SDK/LVRS` 대체를 검색하지 않습니다.
- 완료된 숨겨진 루트 창의 LVRS 가 제공하는 가로비율을 측정하고, 1,280-로직 픽셀 너비를 요청한 후 해당 비율에서 높이를 유도하여, 선택된 화면이 이를 포함할 수 없을 때만 쌍을 축소한 다음 이벤트 루프 진입 전에 동기적으로 표시합니다. C++ 는 가시 창을 절대 크기 조절하지 않으므로, 시작 시 디스플레이 보정 대신 컴포저터 안전 기하학 하나를 최종적으로 갖게 됩니다.
- 일반적인 시작 로그 I/O를 방지합니다. `CONGREGATION_STARTUP_TRACE=1`를 설정하면 `%TEMP%\Congregation-startup.log`에서 타임스탬프가 지정된 마일스톤 레코드가 활성화됩니다. 치명적인 루트-QML 객체 생성 실패는 항상 기록되고 플러시됩니다.
- `DrawingSurfaceItem`를 QML 캔버스 항목으로 등록합니다.
- `PaletteUtils`를 QML 컨텍스트 도우미로 등록합니다.
- 하나의 `LicenseManager`를 `CongregationLicenseManager` QML 컨텍스트 서비스로 등록하고 `EnforcementMode::Disabled`를 명시적으로 선택합니다. 엔드포인트와 `congregation` 제품 ID는 사용자가 편집할 수 있는 입력이 아닌 애플리케이션 소유 상수로 유지됩니다.
- `AccountManager`를 `CongregationAccountManager`로 등록합니다. 시작 시 유휴 상태로 유지되며 표시된 계정 ID 소스로 `iiLicenseManager`를 사용하여 기본 설정이 열릴 때만 새로 고쳐집니다.
- `iisacc`/`iisacc.com` 설정 ID를 설정한 후 `ApplicationPreferences`를 `CongregationApplicationPreferences`로 등록합니다. 민감하지 않은 시작/검색 선택 사항과 검증된 로컬 최근 파일 URL만 저장합니다. 자격 증명은 플랫폼 보안 저장소에 남아 있습니다.
- `NearbyCongregationDiscovery`를 `CongregationNearbyDiscovery` QML 컨텍스트 서비스로 등록하고, 해당 시작/중지 수명 주기를 지속형 검색 설정에 연결하고, 기본 창을 표시한 다음, 기본 활성화된 시작만 대기열에 추가하므로 로컬 네트워크 권한 및 검색이 첫 번째 창 프레젠테이션을 지연할 수 없습니다.
- `LocalCanvasSession` 를 `CongregationLocalCanvasSession` 로 등록하여 발견 서비스의 일시적 시작 ID 를 공유하며, 호스팅 중에만 임시 TCP 포트를 광고하고 세션 상태, 발견된 호스트 엔드포인트, 참여자, 버전, 오류, 참여자 편집 명령, 그리고 권한 있는 스냅샷 신호를 QML 에 노출합니다.
- 레지스트리의 `QObject` 메타 객체 API를 통해 `CanvasDocument` 아래의 LVRS `ViewModels` 레지스트리에 공유 `CanvasDocumentViewModel`를 등록합니다.
- `Congregation` QML 모듈의 `Main` 구성 요소를 시작합니다.

<a id="qml-module-layout-srcappqml"></a>

## QML 모듈 레이아웃(`src/App/qml/`)

### `Main.qml`

- 기본 LVRS 응용 프로그램 창을 만듭니다.
- 도움말 메뉴 버전을 `Qt.application.version`를 통해 `Congregation 1.0.0`로 렌더링하므로 표시되는 값은 별도의 QML 리터럴 대신 실행 파일의 CMake 제공 런타임 버전을 따릅니다.
- 공급 LVRS 애플리케이션 창 기하학을 런타임 가로비율 소스로 사용하고, QML 너비, 높이, 또는 최소 크기 오버라이드 없이 최소 크기 정책을 유지합니다. C++ 는 한 번의 1,280-로직 픽셀 시작 너비 요청과 자동 유도된 비례 높이를 적용한 후 숨겨진 루트 창을 표시하기 전에 사용 가능한 화면에 맞춤하며, 비동기 캔버스 로더는 이후에 이를 크기 조절할 수 없으나 일반 사용자 크기 조절, 최대화, 그리고 풀스크린 컨트롤은 계속 사용 가능합니다.
- 시각적 제목 표시줄 스트립을 억제하기 위해 LVRS 단색 크롬을 사용하는 동안 네이티브 닫기, 최소화 및 최대화 컨트롤을 유지합니다.
- Windows 와 Linux 네이티브 제목 표시줄을 사용하여 LVRS 의 중복 논리적 드래그 핸들을 사용하지 않습니다. macOS 만 LVRS 의 전체 크기 콘텐츠 제목 표시줄을 일반 모드에서 드래그 표면으로 예약하고 풀 스크린 모드에서는 해당 표면을 비활성화하여 모든 플랫폼이 도구대 위의 중복 스트립을 피합니다.
- CMake 플랫폼 패키징 메타데이터를 사용하여 `resources/Appicon.icns`를 표준 macOS 번들 아이콘으로, `resources/Appicon.ico`를 Windows 실행 파일 아이콘 리소스로 만든다. macOS App Store/Xcode 자산 카탈로그는 `tools/sync_app_icon_assets.sh`로 같은 `.icns`에서 다시 생성하고, CMake 번들 빌드는 증분 번들에서 레거시 `Contents/Resources/icon.icns` 파일을 제거한다. Windows 리소스는 네이티브 매니페스트와 버전 메타데이터도 포함한다. Windows 패키징은 `build-windows.ps1`가 담당한다. 빌드 트리를 `build/`로 고정하고 병렬 빌드하며, 서명 릴리스에는 테스트를 요구하고, 영어·한국어 Qt 번역만 배포한다. LVRS/iiPaintEngine/iiSharedCanvas/iiUpdateManager/iiLicenseManager 런타임 DLL을 복사하고 중복된 느슨한 LVRS QML와 미사용 플러그인을 제거하며, Congregation 소유 MinGW Release 바이너리의 심볼을 제거한다. PE 리소스·스테이징의 전체 import closure·런타임 호환성을 검증한 후 Authenticode 서명 전에 마지막 바이너리 변경을 수행한다. 공개 모드는 Congregation 소유 바이너리를 선택한 정확한 Windows 인증서 저장소 지문에 결합하고 유효한 공급자 신원을 보존한다. ZIP 생성 전에 SHA-256 파일 다이제스트·RFC 3161 SHA-256 타임스탬프·스테이징된 PE 전체의 Authenticode 정책 검증 성공을 요구한다. WiX MSI는 임시 이름 아래에서 완전히 생성한 뒤에만 서명하고 검증한다. 패키징 전에 선택한 임시 파일만 제거한다. 요청한 ZIP/MSI 파일과 사이드카 해시는 검증되고 롤백 가능한 한 묶음으로 마지막 정상 버전을 교체한다. 준비된/커밋된 트랜잭션 저널은 각 쌍의 원래 존재를 기록하고, 중단 후 이전 세대를 복원하거나 첫 번째 공개 잔여물을 제거하며, 완전한 프로모션된 집합이 다시 검증될 때까지만 삭제됩니다. 명시적으로 서명되지 않은 로컬 스모크 패키지는 `-unsigned` 접미사를 가지며, 검증되지 않은 일반 CPack 진단은 `-unsigned-cpack-incomplete` 입니다.
- ICE105 검증된 Windows 설치자 5 듀얼 컨텍스트 WiX MSI 를 작성하여 `ALLUSERS=2` 와 `MSIINSTALLPERUSER=1` 와 함께 별도의 Congregation 업그레이드 가족을 시작하고, 네이티브 고급 UI 를 통해 현재 사용자 또는 Elevated 모든 사용자 범위를 제공합니다. 필수 HKMU 설치 컨텍스트 마커는 관련 제품 감지 전에 범위와 설치 위치를 기록하여, 나중에 업그레이드가 컨텍스트를 가로지르는 것을 방지하며, `WIX_UPGRADE_DETECTED` 도 마커리스 개인 사용자 업그레이드를 현재 사용자 범위로 잠깁니다. ProductCode 는 동일한 버전과 아키텍처에 대해 결정론적이며, 입력 중 하나가 변경될 때 변경되어, 동일한 버전 재빌드가 중복 제품을 등록하는 것을 방지합니다. 모호한 이중 등록은 유지보수 및 복구용으로 여전히 사용 가능한 새 또는 교차 컨텍스트 설치를 차단합니다. 필수적인 런타임 기능은 컨텍스트 리디렉션된 프로그램 메뉴에서 Windows 설치자가 정상적인 단축키로 구체화하는 실행 파일 기반의 광고된 시작 메뉴 항목을 소유하며, 이는 고정 범위의 단축키 구성 요소 및 그 ICE38/ICE43/ICE57 충돌을 피합니다. 동일한 UI 는 모든 사용자 대상지를 노출하고 루트 GNU AGPL `LICENSE` 에서 생성된 RTF 렌더링을 내장합니다. 제품 식별자가 등록되면 동일한 패키지는 Windows 설치자의 표준 변경/수리/제거 유지보수 경로를 따릅니다.
- 별도의 `build-windows-store.ps1` 오케스트레이션 경계를 통해 Microsoft Store 및 로컬 개발 MSIX 패키지를 빌드합니다. 두 가지 모두 테스트된 평평한 Windows 런타임 단계를 재사용하며, 표준 1024 px 아이콘에서 정확한 50/44/150 px 스토어 자산을 생성합니다. x64_MANIFEST_는 `Windows.Desktop` 10.0.19041.0를 대상으로 하고, `packagedClassicApp`, `mediumIL`, 근접 Congregation 발견에 사용되는 사설 네트워크 클라이언트/서버 기능, 제한된 `runFullTrust` 기능을 선언하며, 버전을 1.0.0 로 `1.0.0.0` 로 유지합니다. 개발 모드는 정확한 주체의 자체 서명 코드 서명 인증서를 필요로 하며, 패키지 애플리케이션 ID 를 통해 설치하고 활성화할 수 있고, 가시적인 창을 관찰하며 패키지를 제거할 수 있습니다. Store 모드는 Microsoft 인증 및 재서명용으로 의도적으로 서명되지 않은 단일 패키지 `.msixupload` 를 생성하며, 대신 공개 신뢰로 개인 인증서를 취급하지 않고, 정확한 파트너 센터 제품 신원 값, 완전한 iiPaintEngine / iiSharedCanvas / 해당 출처 법적 증거, 그리고 현재 테스트된 릴리스 단계를만듭니다.
- 플랫폼별 애플리케이션 메뉴 프레젠테이션을 제공하여 명령 소유권을 중복하지 않습니다. Qt 6.8 는 Qt 퀉 컨트롤 메뉴 바를 macOS 의 네이티브 글로벌 메뉴로 계속 홍보하고, 설정을 Congregation 애플리케이션 메뉴로 이동시키며, Linux 는 기존 컴팩트 LVRS 토큰 테마 Qt 컨트롤 메뉴 바를 유지합니다. Windows 는 대신 해당 컨트롤 메뉴 바를 억제하고, 파일, 편집, 창, 도움말 버튼이 스톡 `LV.ContextMenu` 팝업을 여는 전용 22-DIP 인 윈도우 헤더를 설치하며, 이는 스톡 `LV.MenuItem` 행과 `LV.MenuDivider` 구분선에 의해 지원됩니다. Windows 도구, 모양 종류, 단축키 참조 분기는 계층적 LVRS 컨텍스트 메뉴로 유지되며, 단축키 참조는 컨텍스트 메뉴의 마지막 키 열을 사용하고 긴 참조를 한계가 설정된 한계 설정된 카테고리 팝업으로 분할하여 최소 애플리케이션 창에 맞춥니다. 단일 동작 맵은 `Controls.Action` / macOS / Linux 에서 사용되는 동일한 객체와 동기화되어 활성화, 선택, 동적 풀 스크린 레이블, 단축키, 트리거 동작을 유지합니다. 비모달 팝업 체인은 하나의 바깥쪽 누름 경계를 공유하여 최상위 버튼이나 중첩 행 사이에서 마우스를 올리면 고아 팝업 없이 메뉴를 전환합니다. 파일은 기존 새/열기/저장/비우기 흐름으로, 편집은 설정, 되돌리기/다시 실행, 클립보드 이미지 붙여넣기, 레이어 생성/삭제, 도구 및 모양 선택, 붓 크기 변경으로, 창은 캔버스 맞춤/재설정 및 네이티브 창 컨트롤로, 도움말은 업데이트 확인 및 현재 키보드 단축키 참조를 노출합니다. 캔버스 잠금 해제 후 라이선스 잊음은 의도적으로 누락되어 진행 중인 문서를 파괴할 수 없습니다.
- 각 실행 가능한 메뉴 항목에 대한 이름 지정된 단축키 계약을 소유합니다. Qt 의 휴대용 `Ctrl` 토큰은 애플 플랫폼에서 물리적 Command 키에 의도적으로 매핑되며 Windows / Linux 에서는 Control 로 유지되고, `Meta` 는 애플 플랫폼에서 물리적 Control 로 매핑되어 `Ctrl` 와만 함께 네이티브 Control+Command+F 풀스크린 시퀀스에 사용됩니다. 설정은 Qt 의 `StandardKey.Preferences` 바인딩 ( `Command+,` 에서 macOS 와 `Ctrl+,` 에서 Windows / Linux ) 을, 파일은 플랫폼 Command/Ctrl N/O/S, Command/Ctrl+Shift+K, Command/Ctrl+Q 를, 편집은 플랫폼 되돌리기/다시 실행, Command/Ctrl+V 클립보드 이미지 붙여넣기, Command/Ctrl+Shift+N, Command/Ctrl+Shift+Delete, 도구 B/E/H/V/Z/U/G/T, 모양 종류 Command/Ctrl+Alt+1..7, 붓 크기 [/] 를, 창은 Command/Ctrl+0, Command/Ctrl+1, Command/Ctrl+M, 플랫폼 풀스크린을 사용합니다. 도움말은 macOS 에서 휴대용 토큰을 Command, Control, Option 이름으로 번역합니다. `Controls.Action` 는 각 휴대용 단축키의 유일한 소유자이며, 캔버스는 중복되지 않는 2-based 대안을, 로드된 페이지는 캔버스 텍스트 입력, 레이어 이름 변경, 또는 모달 파일 대화 중 편집 명령을 비활성화하여 네이티브 텍스트 붙여넣기를 사용할 수 있게 합니다.
- 활성 macOS 일반 창 드래그 핸들 높이를 `PainterCanvasPage`에 전달합니다. Windows, Linux 및 macOS 전체 화면에서는 0으로 확인되므로 도구 모음은 렌더링된 메뉴/콘텐츠 가장자리 바로 아래에서 시작됩니다.
- 쉘 창이 첫 번째 구성 턴을 완료한 후 `Qt.callLater`를 통해 비동기 `Loader`를 활성화하므로 더 무거운 `PainterCanvasPage` 개체 트리가 배양되기 전에 네이티브 창이 나타날 수 있습니다. 페이지가 준비될 때까지 가벼운 LVRS 로딩 라벨이 계속 표시됩니다.
- 관리자의 강제 정책에서 캔버스 접근을 유래시킵니다. 현재 비활성화된 정책인 경우 로더는 저장된 인증 정보를 읽거나 유효성 검사 요청을 발행하지 않고 즉시 진행하며, 강제가 의도적으로 다시 활성화되면 `CongregationLicenseManager.licensed` 동안 대기한 후 잠긴 상태에서는 `LicenseActivationPage` 를 제시합니다.
- 로딩된 `PainterCanvasPage` 를 단일 콘텐츠 뷰로 호스팅하고 페이지가 `pageReady` 를 방출한 후에만 `canvasPage` 에 할당합니다. 최근 캔버스가 선택되면 페이지 준비가 단일 앱 로컬 `recent-canvas.vrc` 를 디코딩하는 한 번의 시도를 트리거하며, 누락되거나 손상되거나 불완전한 컨테이너는 삭제되고 초기화된 빈 캔버스는 사용 가능하게 유지됩니다. 일반 창 닫기는 대기 중인 최근 세션 변형만 플러시합니다.

### `PreferencesWindow.qml`

- 기본 응용 프로그램 창에서 일시적으로 소유한 별도의 초기 숨겨진 `LV.Window`를 만듭니다.
- 기본 설정 작업을 통해 동일한 창 인스턴스를 열거나 복원하고 일반으로 재설정한 다음 중복을 구성하는 대신 활성화하고 활성화합니다. 첫 번째로 보이는 기하학은 기본 창 바로 중앙에 있습니다. 나중에 사용자 위치가 유지됩니다.
- 상단 중앙에 일반/프로필/멤버십 헤더로 `LabelSegmentedControl` -매칭된 LVRS Figma 를 사용합니다. 일반에는 고정 `Account` 라벨과 `iiLicenseManager` iisacc .com 이메일이 포함되며, 서버 권위 있는 Congregation 활성화 후 `Not connected` 로 유지된 이메일 또는 상호 배타적으로 지속된 새 캔버스 및 최근 캔버스 라디오 컨트롤과 지속된 근처 Congregation 사용자 확인란이 있습니다. 기본 설정을 여는 것은 그 이후에만 해당 신분을 새로고침하며, 기존 레거시 보안 인증 정보는 C++ 내부에서만 누락된 iiLicenseManager 활성화를 생성하는 데만 소비되고 그 키는 QML 에 절대 들어가지 않습니다.
- 구매 복원을 왼쪽 하단에 배치하고 업데이트 확인…을 오른쪽 하단에 배치합니다. 구매 복원은 인증된 iisacc 계정 대시보드를 여는 대신 미확인 로컬 이메일에서 소유권을 부여합니다. 업데이트 버튼은 기본 설정을 숨기고 메인 창을 재활성화하며 도움말과 같은 가시적인 `iiUpdateManager` 수동 업데이트 모달로 위임합니다.
- 원형 테두리 없는 `LV.IconButton` 를 사용하여 대칭 아바타 패딩을 적용하여 기본 `user` 아이콘이 64-DIP 프로필 프레임 내부에 중앙에 유지되도록 합니다. 버튼은 먼저 기본 LVRS `ContextMenu` 를 열어 `Select profile image` 와 `Delete profile image` 를 포함하며, 기본 `LV.MenuItem` 행은 모두 접근성을 위해 가시성 레이블을 노출하고, 선택은 메뉴가 닫힌 후 네이티브 이미지 선택기를 엽니다. 삭제는 이미지가 존재할 때까지 가시화되지만 비활성화되어 있습니다. `ProfileImageProcessor` 는 이미지 방향 메타데이터를 적용하고, 소스의 네이티브 픽셀 차원에서 가장 큰 중앙 정사각형을 가져와 원으로 잘린 후 스케일링 없이 손실 없는 임시 PNG 를 작성하며, 삭제 시 미리보기가 지워지고 해당 임시 파일이 제거되고, 실패한 교체 시 이전 유효한 자르기가 그대로 유지됩니다. `LV.InputField` 는 프로필 이름을 유지하고, `LV.CheckBox` 는 다른 사용자가 초대될 수 있는지 여부를 제어합니다. 값은 영속화되지 않은 윈도우 상태이며, 초대 기능을 활성화하면 오직 부울형 기능만 광고되고, 표준화된 프로필 이름은 명시적인 타겟팅 초대나 캔버스 연결 후에만 전송되며, 프로필 이미지는 로컬로 유지됩니다.
- 멤버십에 기본 `LV.List` 하나를 유지하고, Figma 프레임 `171:172` 에서 테마 확장 237-DIP 너비를 유지하여 선호도 창 수평 중앙선 아래 공유 상태 행 아래에 정렬합니다. **캔버스 공유**, **근처 참여…**, **공유 중지/캔버스 나가기** 는 활성 `LocalCanvasSession` 를 제어하며, 참여 메뉴는 캔버스 포트를 광고하는 발견 기록으로만 채워집니다. 목록은 항상 라이브 로컬 프로필을 포함하며, 캔버스 호스트를 첫 번째 행으로 승격하고 정확한 `(host)`, `(me)` 또는 `(host, me)` 접미사를 사용하며; 빈 로컬 프로필 이름은 `Unnamed member` 가 됩니다. 라이브 세션 기록은 `peerId`, `profileName`, `isHost`, `isMe` 및 `removable` 역할을 사용합니다. 호스트 및 현재 사용자 행은 제거할 수 없으며, 호스트는 푸터 슬롯 2를 통해 연결된 모든 원격 행을 제거할 수 있습니다. 푸터 슬롯 1 는 LVRS 의 스톡 `add` 별칭 ( `generaladd`, 파일 개요 없는 평범한 플러스) 을 사용하며, 비활성 중이거나 호스팅 중일 때 초대 기능이 있는 근처 Congregation 세션의 메뉴를 엽니다; 하나를 선택하면 필요에 따라 호스팅을 시작하고 해당 타겟에게 초대를 보냅니다. 슬롯 3 는 비활성화된 빈 글리프 상태를 유지합니다. 익명 심박 데이터는 결코 참여자 신원이 되지 않습니다.

### `LicenseActivationPage.qml`

- 향후 의도적인 재활성화를 위해 패키지된 상태로 유지되지만 라이센스 적용이 비활성화된 동안에는 표시되지 않습니다.
- 형상을 재정의하지 않고 유휴 첫 번째 활성화 표면에 스톡 LVRS `AppCard`, `InputField`, `Label` 및 `LabelButton` 구성 요소를 사용합니다.
- 구매자의 인증된 계정 이메일 (서버의 254문자 제한까지) 과 렌더링 중 Congregation 를 고정된 수정 불가 제품 데이터로 표시하며 마스킹된 버전 `IIL<version>_...` 키를 수락하고, 민감한/예측 불가 입력 힌트는 키보드 학습을 방지하며 두 필드 및 활성화 버튼은 접근 가능한 이름을 노출합니다.
- 권한 있는 무효 라이선스 결정과 일시적으로 사용할 수 없는 확인을 분리하고, 성공 후 두 자격 증명 필드를 모두 지우고, URL에 자격 증명을 넣지 않고 개인 계정 대시보드 및 Congregation 스토어에 연결합니다.
- 저장된 자격 증명을 일시적으로 검증할 수 없으면, 서비스나 네트워크를 사용할 수 없다는 이유만으로 키를 다시 입력하지 않도록 양식을 **저장된 라이선스 재시도**와 **다른 라이선스 사용** 동작으로 교체한다. Linux는 보안 저장소 제한을 설명하고 실행할 때마다 수동으로 입력하는 경로를 유지한다.

### `AccountManager`

- `iiLicenseManager::LicenseClient::loadLicense()`를 먼저 호출하므로 유효하게 서명된 Congregation 인증서는 시작 요청 없이 연결된 정규 계정 이메일을 오프라인으로 복원합니다.
- iiLicenseManager 라이선스가 없으면 사용자가 설정을 열고 직접 `LicenseClient::activate()` 에 전달하기 전까지 `LicenseManager` 에 대한 레거시 보안 인증만 요청합니다. 이메일은 iiLicenseManager 가 유효한 서버 서명 활성화 인증서를 수락하고 계정 이메일 사이드카를 지속한 후에만 노출되며, 인증 정보가 사용 불가능하거나 유효하지 않거나 부재인 경우 `Not connected` 로 렌더링된 빈 값이 생성됩니다.
- `accountEmail`, `accountEmailLoading` 및 `refresh()`만 QML에 노출합니다. 라이센스 키는 이동 전용 C++ 데이터로 유지되며 활성화 호출 후 지워집니다.

### `LicenseManager`

- 불변 `enforcementEnabled` 상태를 노출합니다. 비활성화 모드는 잠금 해제 상태로 시작하며, 활성화, 재시도, 잊음, 자동 보안 저장소 복원, 자동 유효성 검증을 비활성화합니다. C++ 전용 저장된 인증 정보 액세스기는 명시적인 사용자 작업인 **즉시 업데이트** 가 C++ 내부의 전체 인증 정보를 소비하는 동안에만 사용 가능하며, `AccountManager` 는 설정이 열릴 때 iiLicenseManager 로 신원을 마이그레이션하기 위해 한 번만 이를 소비할 수 있습니다.
- 다음 유효성 검사 계약은 활성화된 모드에 대해 유지되므로 재활성화는 라이센스 코드를 재구성하는 것이 아니라 한 줄의 응용 프로그램 정책 변경입니다.
- `POST https://iisacc.com/graphql` 를 `validateLicense` 변형으로 `variables.input = { email, licenseKey, productId: "congregation" }` , `Content-Type` / `Accept: application/json` , no-store 헤더, 리디렉션 추적을 따르지 않으며 10초 타임아웃으로 보냅니다. 클라이언트 사전 검사 는 Congregation 를 1버전으로 고정하는 대신 서버 키링의 1 에서 32767 버전들을 수락합니다.
- `valid` 멤버가 JSON 부울인 HTTP 200 `application/json`만 허용합니다. `valid: true`에는 추가로 `productId: "congregation"` 응답이 필요합니다. `valid: false`는 ​​서버에 의해 문서화된 구별할 수 없는 라이선스 무효 결정입니다.
- 리디렉션, TLS/네트워크 오류, 시간 초과, 429/5xx, 크기가 너무 크거나 잘못된 본문, 문자열 Y/N 값, 실제 결정에 없거나 예상치 못한 제품을 확인 불가능으로 처리하고 안전하게 거부하는로 유지됩니다.
- 공인된 성공 응답 후에만 표준화된 이메일, 라이선스 키, 스키마, 고정 제품 ID 를 직렬화합니다. macOS 는 JSON 를 키체인에 기록하고 Windows 는 QtKeychain 를 통해 `insecureFallback(false)` 를 Credential Manager 에 기록하며, Linux 는 영구적인 어댑터를 노출하지 않고 절대 평문 저장을 대체하지 않습니다.
- 시작 시 안전한 JSON 을 읽어서 로컬/오프라인 라이선스를 부여하지 않고 자동으로 동일한 온라인 POST 를 수행하며, 권한 있는 `valid: false` 와 잘못된 저장된 JSON 는 자격 증명을 삭제하고, 네트워크/ TLS /timeout/429/5xx 실패는 재시도를 위해 유지하며, 잠금된 활성화 화면의 **다른 라이선스 사용** 작업은 명시적으로 삭제하고 폼으로 되돌립니다.
- 비동기 보안 저장소 쓰기가 반환된 후에만 첫 번째 활성화를 완료합니다. 저장소 전용 오류는 여전히 현재 프로세스에 대한 신뢰할 수 있는 온라인 라이센스를 존중하지만 캔버스 위에 `secure_storage_unavailable`가 표시되도록 유지하므로 구매자는 다음 실행 시 키를 다시 입력해야 한다는 것을 알 수 있습니다.
- 요청 자격 증명은 URL 또는 로그에 기록되지 않으며 메모리 내 라이선스가 부여된 부울만 현재 프로세스에 대한 캔버스 구성을 제어합니다.

### `CongregationUpdateManager`

- `iiUpdateManager 0.2` 를 C++ 컨텍스트 서비스를 통해 QML 에 노출된 `CongregationUpdateManager` 로 감싸며, QML 는 상태, 진행 상황, 버전, 제목/메시지, 그리고 인수 없는 확인/업데이트/취소 방법만 받으며, 자격 증명, 로컬 패키지 경로, 아티팩트 URL, 및 다운로드 권한은 C++ 전용으로 남습니다.
- 공개 애플리케이션 및 패키지 마케팅 버전을 `1.0.0`에 유지하면서 업데이트 프로토콜 입력만 `1.0.0`로 정규화하여 표시되는 버전을 변경하지 않고 iiUpdateManager의 안정적인 `MAJOR.MINOR.PATCH` 비교 계약을 충족합니다.
- 타이머, 소켓, 폴링, 시작 확인 또는 원격 측정 없이 유휴 백엔드를 구성합니다. 도움말 또는 기본 설정 → 일반에서 명시적인 **업데이트 확인...** 작업만 하나의 매니페스트 확인을 호출하고 업데이트를 찾아도 자동으로 승인되거나 다운로드되지 않습니다.
- 명시적인 **지금 업데이트** 작업 후에만 `CongregationUpdateCredentialProvider` 를 사용하며, 제공자는 기존 `LicenseManager` / `LicenseCredentialStore` 인스턴스에 정통한 4-필드 안전한 JSON 를 요청하고, 누락된/접근할 수 없는 저장소를 잘못된 스키마 바이트와 별도로 분류하며, 유효한 이메일/키 데이터를 RAII 업데이트 자격 증명으로 이동시키되 두 번째 키체인/자격 증명 관리자 서비스를 생성하지 않습니다.
- 원시 엔드포인트, 서명된 URL 또는 파일 시스템 세부 정보를 전달하지 않고 백엔드 결과를 안정적인 사용자 대상 메시지에 매핑합니다. 확인된 설치 프로그램이 열리면 사용자에게 작업을 저장하고 설치 프로그램 지침을 따르도록 명시적으로 경고합니다. 설치 프로그램 신호에서 `Qt.quit()`를 호출하지 않습니다.
- 읽기 전용 `selfUpdateSupported` 채널 게이트를 노출합니다. macOS `IISACCDistributionChannel=appstore` 마커 또는 비어 있지 않은 `_MASReceipt/receipt` 및 Windows `GetCurrentPackageFullName` 패키지 컨텍스트는 백엔드 요청 전에 외부 설치 프로그램 흐름을 비활성화합니다. 예기치 않은 Windows 패키지 쿼리 오류도 안전하게 거부한다입니다.

### `NearbyCongregationDiscovery`

- 전용 `QThread`로 이동된 작업자 `QObject` 내에서 `QUdpSocket`, 하트비트 타이머, 네트워크 인터페이스 새로 고침, 수신 루프 및 피어 만료 타이머를 실행합니다. GUI 스레드는 시작/중지를 대기열에 추가하고 상태 변경 신호를 수신합니다.
- 관리적으로 범위가 지정된 IPv4 멀티캐스트 그룹 `239.255.86.67` , 개인 포트 `52683` , 멀티캐스트 TTL 1, 4초의 심박, 및 14초의 만료 창을 사용하며, 모든 활성 멀티캐스트 지원 IPv4 인터페이스에 참여하고 인터페이스가 변경될 때마다 멤버십을 새로고침하며, 종료 중 최선의 노력으로 오프라인 비콘을 보내고 인터페이스가 사용 가능해질 때 다음 새로고침을 통해 복구합니다.
- 서비스 이름, 프로토콜 버전 1, 각 애플리케이션 시작마다 재생성되는 일시적인 UUID , 온라인/오프라인 상태, 초대-기능 불리언, 및 사용자가 명시적으로 캔버스를 호스팅하는 동안 선택적인 임시 TCP 포트만 직렬화합니다. 잘못된 형식, 과도한 크기, 외국 버전, 및 자기 세션 데이터그램을 거부하며; 호스트 이름, 사용자 이름, 프로파일, 계정, 앱 문서, 및 장치 식별자는 절대 심박수에 들어가지 않습니다.
- 현재 로컬 인터페이스 주소인 송신자를 가진 데이터그램을 무시한 후, ID 세션에 따라 캔버스 엔드포인트와 초대 가능 타겟을 유지하면서 송신자 주소로 인접 장치 수를 중복 제거합니다. 멤버 `+` 선택 후만 타겟 지시된 초대가 발생하며, 정형화된 초대 UUID , 송신자/타겟 세션 ID, 임시 캔버스 포트, 및 정규화된 초대자 프로필 이름을 포함합니다. 워커는 알려진 송신자 주소와 대상 세션을 검증하고, 여러 인터페이스를 통해 수신된 중복 ID 를 억제합니다.
- macOS는 `NSLocalNetworkUsageDescription`를 선언합니다. App Store 샌드박스는 네트워크 클라이언트와 UDP 수신기 서버 권한을 모두 부여합니다. Store MSIX는 `privateNetworkClientServer`를 선언하는 반면 직접 데스크톱 패키지는 플랫폼 방화벽/로컬 네트워크 정책을 통해 계속됩니다.
- `ApplicationPreferences::discoverNearbyCongregationUsers`가 활성화된 경우(기본값)에만 기본 창 다음에 시작되고 일반 확인란이 선택 취소되면 즉시 중지되며 나중에 시작할 때 해당 선택 사항이 유지됩니다.

### `LocalCanvasSession`

- 일시적 포트에서 **캔버스 공유**후에만 IPv4 `QTcpServer` 를 시작하며, 공유가 중지되면 발견 비콘이 해당 포트를 회수합니다. 참여는 라이브 발견 기록에서만 엔드포인트를 해결하고, UUID /주소/포트를 검증하며, 현재 로컬 IPv4 서브넷 밖의 주소를 거부하고, 8초의 연결 시간 제한을 적용하며, 호스트는 불완전한 핸드셰이크에 동일한 LAN 주소 게이트와 시간 제한을 적용합니다.
- 버전화된 `VCAN` 프레임 헤더와 64비트 페이로드 크기를 사용하며, 할당 전에 잘못된 형식이나 과도한 크기의 프레임을 거부하고, 세션을 16 개의 원격 피어로 제한하며, 프로필 이름을 80 문자로 정규화하고, 중복되거나 변경된 피어 UUID 를 거부합니다. 참가자 프로필 이름과 캔버스 바이트는 TCP 핸드셰이크 이후에만 전송되며, 명시적으로 타겟팅된 초대에서 이전에 전송되는 유일한 프로필 필드는 초대자의 정규화된 프로필 이름입니다. 수송은 LAN TCP 없이 TLS 또는 공유 비밀 없이 직접적이므로, 공유는 신뢰할 수 있는 로컬 네트워크를 위한 것으로 의도되었습니다.
- 참여한 모든 표면을 실제 호스트 캔버스의 입력 클라이언트로 취급한다. 클라이언트는 엄격한 키 및 크기·값 한계가 설정된을 적용한 스트로크, 채우기, 텍스트, 도형, 객체 변환, 캔버스/레이어 연산, 실행 취소/다시 실행과 검증된 래스터/이미지 바이트의 의미 명령만 제출한다. `.vrc` 상태를 공개하거나 정규 클라이언트 문서를 변경할 수 없다. 로컬 포인터/태블릿 입력 중 스트로크는 iiSharedCanvas의 활성 스트로크 렌더러를 커밋하지 않는 미리보기로 사용한다. 입력을 놓으면 취소하여 이전 픽셀과 이력으로 정확히 돌아간 뒤 완료된 명령을 제출한다.
- 호스트는 각 명령을 자신의 `DrawingSurface` 에서 검증하고 적용한 후, 다음 단조 증가 버전과 함께 완전한 SHA-256 -검증된 `.vrc` 애플리케이션 세션 스냅샷을 단독으로 브로드캐스트하며, 모든 클라이언트는 명령 기원을 포함한 호스트 상태를 복원합니다. 명령은 호스트 도착 순서로 직렬화되며, 객체 수준의 동시 병합은 의도적으로 이 프로토콜 바깥에 있습니다. 호스트는 원격 참가자를 제거할 수 있으며, 호스트를 중지하면 명시적인 이유로 모든 사람이 연결이 끊깁니다.
- 검증된 초대를 최대 16 개 대기열에 쌓고, 같은 보낸 사람의 더 오래된 대기 중인 초대를 대체하며, 첫 번째 초대 및 개수를 QML 에 노출합니다. `respondToPendingInvitation(bool accepted, profileName)` 은 항상 현재 초대를 제거하고, `true` 에 한해 검증된 LAN 엔드포인트에만 참여하며, `false` 에는 세션을 건드리지 않고 그대로 두며, 초대 비활성화는 대기열을 지웁니다.

<a id="mainqml-manual-update-modal"></a>

### `Main.qml` 수동 업데이트 모달

- **Check for Updates…**를 도움말에 추가하고 확인, 최신, 오류, 사용 가능한 버전, 다운로드/확인 진행률, 설치 프로그램 열림 및 취소 상태에 대한 LVRS `Modal`를 엽니다.
- **업데이트를 지금** 두 번째 명시적 작업으로 유지하고 인증, 다운로드, 검증, 설치기 시작 전까지 취소 기능을 노출하며, 설치기 인수 성공 후만 취소가 종료됩니다. `Component.onCompleted` 는 캔버스 인큐베이팅 전용으로 남으며, 사용 가능/업데이트 신호 처리기는 다른 네트워크 작업을 시작하지 않습니다.
- `selfUpdateSupported`가 false인 경우 **업데이트 확인...**를 숨기고 비활성화합니다. 따라서 스토어 관리 설치는 모달을 열거나 QML에서 외부 PKG/MSI 업데이트 프로그램을 호출할 수 없지만 직접 웹 사이트 빌드는 수동 흐름을 유지합니다.

### `PainterCanvasPage.qml`

- 안정적인 `PainterCanvasPage` 뷰 ID를 사용하여 뷰를 LVRS `ViewModels`에 바인딩합니다.
- `CanvasDocumentViewModel`에서 브러시 및 캔버스 상태를 읽습니다.
- `collaboratorProfiles` 및 `currentUserIsCanvasHost`를 `LocalCanvasSession`에 바인딩합니다. 로컬로 공유된 캔버스는 현재 사용자를 호스트로 취급합니다. 참가한 세션은 원격 호스트 레코드를 `isHost`로 표시합니다. 구성원 발표자는 여전히 로컬 프로필을 삽입하며 익명 상태만으로는 참가자 행을 채우지 않습니다.
- 예약된 상단 크롬 높이가 0으로 확인되므로 Windows, Linux 및 macOS 전체 화면에서 메뉴 표시줄/콘텐츠 가장자리 바로 아래에 도구 모음을 배치합니다. macOS 일반 창 모드는 전체 크기 제목 표시줄 드래그 표면만 예약합니다.
- 툴바 바로 아래에 왼쪽에 도킹된 `LV.Hierarchy` 레이어 패널을 호스팅하여 창 왼쪽 및 하단 가장자리와 맞닿게 합니다. 패널은 현재 QML 세션 레이어를 위에서 아래로 나열하고, 존재할 경우 `Background` 래스터 행을 추가하며, 스택에 레이어가 없는 경우 목록 영역을 비우고, 각 행을 LVRS `iconSource` 비트맵 스냅샷 미니처를 통해 표시하며, 레이어 선택을 활성화하고, 반복된 행 활성화/더블 클릭에서 내선 레이어 이름 편집을 엽니다. 행 드래그 이동을 `DrawingSurface` 로 위임하고, 배경 레이어를 포함하여 레이어 생성 또는 제거를 위한 플러스/마이너스 푸터 버튼을 노출합니다. 빈 문서는 `Background` 및 선택된 투명 `Layer 1` 로 시작하여 수동 레이어 추가 단계를 거치지 않고 그림을 시작할 수 있습니다.
- 스톡 테마 규모 계약에 `LV.Hierarchy` 너비와 최소 기하학을 남겨 둡니다. Congregation는 배치, 모델/편집 동작 및 바닥글 작업만 제공합니다.
- 도구 모음 작업을 뷰 모델 속성 업데이트 또는 `DrawingSurface` 명령으로 라우팅합니다.
- `DrawingSurface.sessionChanged` 를 1.2 초 동안 디바운스하고, 현재 완전한 세션을 `ApplicationPreferences.recentCanvasStorageUrl` 에 기록하며, 원자적 작성이 성공한 후만 스냅샷을 사용 가능으로 표시합니다. 외부 파일 경로는 최근 캔버스 식별자가 결코 되지 않으며, 열기 또는 내보내기는 결과 라이브 세션의 새 내부 스냅샷을 예약합니다.
- 호스팅 중에는 호스트 사용자의 자체 편집을 350 밀리초마다 디바운스하고, 메모리 내 전체 `.vrc` 스냅샷을 게시합니다. 참가자가 누르는 동안 로컬 전용 라이브 브러시 미리보기를 보지만, 한계가 설정된 스트roke 명령은 릴리스 후에만 호스트에 동기적으로 적용되고 다음 이벤트 사이클에 대한 공식적인 게시를 대기열에 추가하여 새로 추가된 QML 래스터 레이어가 내보내기를 준비할 수 있습니다. 연결된 클라이언트는 첫 번째 공식적인 QML 복원이 성공할 때까지 명령 차단 상태에 남아 있으며, 복원된 원격 상태를 게시하거나 영구 저장하지 않고 연결 해제 또는 복원 실패 시 준비 상태를 지웁니다. 원격 스냅샷은 비동기 복원 동안 모든 로컬 재게시 를 억제합니다.

### `PresentationLaserPointer.qml`

- 휠 확대/축소를 전달하고 프레젠테이션 제스처로 인해 그리기 캔버스가 변경되는 것을 방지하는 동안 전체 프레젠테이션 화면에서 포인터 입력을 사용합니다.
- `currentPointX`/`currentPointY`에서만 하나의 3링 빨간색 레이저 포인트 항목을 유지합니다. 이 항목은 스무딩이나 지연 없이 활발하게 누른 커서에서 직접 업데이트됩니다. 기록 샘플은 포인트 항목을 인스턴스화하지 않으며 원이나 채우기를 생성하지 않습니다.
- 과거 마우스 샘플의 각 연속 실행을 모든 샘플링된 좌표를 통과하는 Catmull–Rom 파생 큐빅 베지어 세그먼트로 변환합니다. 이렇게 하면 포인터 궤적이 `lineTo` 폴리라인이 아닌 연속 곡선으로 유지됩니다.
- 평평한 캡과 경사 조인을 사용하여 곡선형 코어와 발광 레이어를 렌더링하여 샘플 노드 원과 둥근 조인 원을 모두 방지합니다. 길이가 0인 터미널 세그먼트가 다른 점을 형성할 수 없도록 정확한 중복 릴리스 좌표를 무시합니다.
- 모든 누르기의 첫 번째 샘플을 새 스트로크로 표시하므로 반복되는 제스처가 인위적인 커넥터를 얻지 못하는 동시에 세그먼트당 2초 페이드, 256샘플 바운드 및 16 ms 다시 그리기 흐름을 유지합니다.

### `CanvasToolBar.qml`

- flat-래스터 명령 모음을 제공합니다.
- 새로 만들기, 열기 및 저장을 위한 왼쪽 도구 모음 파일 작업을 표시하는 반면 최상위 응용 프로그램 작업은 해당 바로 가기를 소유하고 메뉴를 통해 명확하게 표시합니다.
- 플로팅 실린더 대신 전체 너비 사각형 모서리 툴바 배경 안에 명령 바를 렌더링합니다. 콘텐츠 레이아웃은 `LV.Theme.gap16` 를 16픽셀의 왼쪽 및 오른쪽 패딩으로 일치시켜 첫 번째 파일 컨트롤과 마지막 프로필 컨트롤이 윈도우 가장자리와 닿지 않습니다.
- 기존 `LV.IconButton`, `LV.IconMenuButton`, `LV.ToggleSwitch`, 및 `LV.ContextMenu` 기하학을 인스턴스화합니다. 14 컨트롤은 직접적인 기존 `LV.IconButton` 인스턴스이며, 현재 색상 컨트롤만 선택된 색상 원을 그리는 데 이를 유래시킵니다. 모든 15 결과 아이콘 버튼과 툴바 `IconMenuButton` 는 `LV.AbstractButton.Borderless` 를 사용하며, Congregation 는 의미론적 아이콘, 접근성, 및 이벤트 바인딩을 제공하지만 인스턴스별 아이콘 크기, 프레임 크기, 패딩, 메뉴 너비, 토글 차원, 또는 선택에 따른 톤 오버라이드를 제공하지 않습니다.
- LVRS `addFile`, `generalopen`, `generalsave`, `generalsearch`, `showCode`, `eraser`, 선택된 모양 종류, `fillbucket` 및 `typeAlias` 아이콘과 앱 로컬 `panHand` 및 `translateObject` 소스.
- 일치하는 실행 가능한 아이콘에 기존 동작을 유지합니다: add 는 새 캔버스 크기 모달을 엽니다, open 은 이미지 열기 대화 상자를 표시합니다, save 는 이미지 저장 대화 상자를 표시합니다, `panHand` 는 패닝 도구를 선택합니다, `translateObject` 는 이동 도구를 선택합니다, `generalsearch` 는 줌 도구를 선택합니다, 연필은 브러시 설정을 선택하거나 다시 엽니다, 지우개는 지우개 도구를 선택합니다, 스톡 모양 메뉴 버튼은 모양 도구를 선택하고 해당 메뉴를 엽니다, `fillbucket` 는 채우기 도구를 선택합니다, 그리고 `typeAlias` 는 텍스트 도구를 선택합니다.
- 새 캔버스 요청을 내보내기 전에 너비 및 높이 입력이 포함된 LVRS 스타일 모달을 사용합니다.
- 스톡 `LV.ContextMenu`를 통해 직사각형, 타원, 삼각형, 다이아몬드, 별, 직사각형 버블 및 타원 버블 삽입에 대한 모양 메뉴 항목을 제공하고 메뉴 항목 너비를 재정의하지 않고 `LV.IconMenuButton`에서 선택한 모양을 미러링합니다.
- 앱 전체 벡터 소스를 `panHand` 에 사용하며, 이는 LVRS 아이콘 세트에 없습니다, 그리고 `translateObject` 는 설치된 LVRS SVG 가 Qt 의 내장 `<use>` 렌더링 경고를 트리거합니다. 이들은 스톡 `LV.IconButton` 의 의미론적 아이콘 입력입니다; 이들은 구성 요소를 대체하거나 크기 조절하지 않습니다. `typeAlias` 는 스톡 LVRS 아이콘 이름 처리를 통해 해결됩니다.
- 고정 캔버스 뷰포트에서 팬 입력을 유지하고 앵커 중심 오프셋 대신 직접 항목 `x`/`y` 오프셋을 사용하여 팬 이동을 적용하여 모든 드래그 프레임 중에 앵커 릴레이아웃을 방지합니다.
- 도구를 브러시, 지우개, 이동, 이동, 확대/축소, 모양 만들기, 채우기 및 텍스트로 제한합니다.
- 최상위 애플리케이션 액션을 통해 `B/E/H/V/Z/U/G/T` 를 할당하고 캔버스에는 일치하는 2-beolsik 한국어 대안을만 남겨 모호한 중복 `QShortcut` 등록을 피합니다. 가벼운 앱 수준의 `TemporaryCameraInput` 필터는 캔버스 단축키가 활성화된 상태에서 Space plus Control/Command 를 추적하므로, 다른 비텍스트 컨트롤이 포커스를 잡은 후에도 임시 카메라 이동이 여전히 작동합니다. Space 만으로는 열림/닫힘 손 패닝을 노출하며, Control 또는 Command 를 추가하면 뷰포트 -wide 수평 드래깅 줌으로 전환하고 수정자를 해제하면 패닝으로 돌아오고, Space 를 해제하면 선택된 도구와 커서를 복원합니다. 캔버스 텍스트, 레이어 이름 변경, 및 도구대 대화 상자 편집기는 일반 Space 입력을 유지합니다.
- 현재 색상 컨트롤을 왼쪽 정렬 명령 흐름에서 브러시 컨트롤 바로 뒤에 배치하고, 테두리가 없는 HSL -derivative 색상 컨트롤에서 `LV.IconButton` 삼각형 색상 휠을 엽니다. 사용자 정의 채색 원은 현재 브러시 색상을 추적하며 검은색이 가시적으로 남도록 2픽셀의 흰색 테두리를 갖습니다. 뒤에 있는 프레젠테이션 모드 및 프로필 컨트롤은 `LV.IconButton` 인스턴스를 직접적으로 사용하여 LVRS 와 `screens` 및 `user` 아이콘을 사용합니다. 대기 중인 초대 메시지는 프로필에 LVRS 위험 색상 점 위에 겹쳐지며, 클릭하면 초대자의 표준화된 이름과 사용자 아이콘, `Accept` 및 `Decline` 를 보여주는 `LV.ContextMenu` 를 엽니다. 그런 다음 선택된 수락 상태를 부울 값으로 방출합니다.
- 브러시 크기 컨트롤을 감소 버튼, 슬라이더 및 증가 버튼으로 주문하여 컨트롤이 값 방향을 따르도록 합니다.
- 이미 선택된 브러시 도구를 다시 누르면 브러시 설정 메뉴가 열리며, iiPaintEngine 브러시 크기, 흐름, 불투명도, 경도, 간격 및 안정화 강도에 대한 슬라이더와 태블릿 압력이 브러시 불투명도 상한을 조절하는지 여부를 제어하는 압력-불투명도 토글이 있습니다. 압력 최소, 중앙 및 최대 매개변수는 별도의 슬라이더 대신 메뉴 하단에 3점 곡선 그래프로 표시됩니다.
- 열린 대화 상자를 지원되는 Qt 이미지 입력과 PSD 파일로 제한하고 저장 대화 상자를 Photoshop PSD로 기본 설정하면서 여전히 PNG, JPEG, BMP를 제공합니다. WebP 및 TIFF(선택한 필터별로 기본 확장 처리 포함)

### `HslTriangleColorPicker.qml`

- QML `Canvas`를 사용하여 색상 링과 내부 HSL 삼각형을 그립니다.
- 현재 선택된 색상의 내측 삼각형을 채우며, 순수 색상, 흰색 및 검은색을 3 꼭짓점으로 합니다.
- 삼각형 점을 순색, 흰색 및 검정색 가중치로 매핑한 다음 HSL 채도 및 밝기를 통해 브러시 색상을 파생합니다.
- 미리 정의된 팔레트 목록에 의존하지 않고 선택한 색상을 도구 모음에 직접 내보냅니다.

### `DrawingSurface.qml`

- 편집 가능한 래스터 표면으로 `DrawingSurfaceItem`를 호스트합니다.
- iiPaintEngine이 각 활성 래스터 표면에서 마우스·태블릿·실시간 프리뷰·획 커밋·지우개·실행 취소·다시 실행·래스터 저장 동작을 처리하도록 한다. 평면 이미지를 열 때마다 기본 래스터 캔버스를 원본 이미지 크기로 교체하므로 작업 공간 패딩이 문서에 포함되지 않으며, 이미지를 이전 캔버스에 맞추어 확대/축소하지 않는다.
- 모드에서 모양이 활성화되면 드래그하여 삽입하는 도구를 제시하며, QML 미리보기가 드래그 범위를 따르고, Shift 는 드래그 범위를 1:1 비율로 제한한 후 현재 브러시 색상으로 선택된 모양을 변환 가능한 단색 세션 객체와 별도의 레이어 행으로 저장합니다. 말풍선 꼬리는 몸체와 같은 경로로 추적되므로 직사각형과 타원형 말풍선은 별도의 삼각형 하위 모양을 연결하지 않습니다.
- 도형과 텍스트 사이에 채우기 도구를 제공합니다. 캔버스를 클릭하면 인접한 동일한 색상의 래스터 영역이 `DrawingSurfaceItem`를 통해 현재 브러시 색상으로 가득 채워집니다.
- 텍스트 도구가 활성화되면 페인트 스타일의 텍스트 편집기를 제시하며, 편집기는 남은 캔버스 너비 내의 가장 긴 텍스트 줄에서 프레임 크기를 설정하고, 현재 브러시 크기와 브러시 색상을 텍스트 크기와 색상으로 사용한 후 평문 텍스트를 변환 가능한 세션 객체와 별도의 레이어 행으로 저장합니다.
- 원시 시스템 클립보드 이미지, 복사된 로컬 이미지 파일 URL, 또는 캔버스에 드롭된 이미지를 콘텐츠 주소 기반 캐시된 PNG 로 변환하여 선택된 이미지 레이어 객체로 삽입합니다. 붙여넣기는 레이어를 캔버스 중앙에 배치하는 반면, `DropArea` 는 Finder/파일 관리자 및 브라우저 드래그를 캔버스 로컬 드롭 포인트로 매핑하고 허용된 드래그 중 캔버스 테두리를 강조합니다. 인코딩된 이미지 MIME 바이트가 우선순위를 가지며, 로컬 URL 은 소스 파일에 대한 의존성을 유지하지 않고 디코딩되고, 브라우저 HTML 이미지 소스와 HTTP (S) URL 은 비동기 네트워크 대체 경로 를 사용합니다. 캔버스보다 큰 이미지는 80% 캔버스 박스에 비율 맞춤되며, 작은 이미지는 픽셀 크기를 유지하고, 페이지는 기존 변환 핸들이 즉시 사용 가능하도록 이동 도구로 전환됩니다. 동일한 캐시된 소스의 반복 삽입은 정확한 중첩 아래 사라지는 대신 16픽셀 대각선 오프셋을 교대로 사용합니다. 결과 판독은 사용 불가, 누락, 형식 오류, 과도한 크기, 다운로드, 캐시 작성 실패를 구별하며, 실패한 시도는 대기 중인 텍스트, 객체 스택, 선택 및 도구를 보존하고 `Main.qml` 는 일시적인 LVRS 카드에 현지화된 원인을 제시합니다.
- 삽입한 이미지·텍스트·도형 레이어 객체에 이동 도구를 제공한다. 객체를 클릭하면 선택하고 본문을 드래그하면 이동하며, 본문을 Shift 드래그하면 우세한 직선 축으로 이동을 고정한다. 확대된 모서리나 가장자리 핸들을 드래그하면 해당 크기 변경 커서로 경계를 변경한다. Shift 크기 변경은 객체의 원래 종횡비를 보존한다. 변환은 Photoshop 레이어처럼 캔버스 밖으로 나갈 수 있지만 객체 픽셀과 변환 제어 UI는 캔버스의 시각 범위로 계속 클리핑한다. Delete 또는 Backspace는 내보내기 전에 선택한 객체를 제거한다. 이동 커서는 포인터 위치에서 결정한다. 빈 캔버스는 화살표, 객체 본문은 4방향 이동 커서를 사용하며 방향별 크기 변경 커서는 선택한 객체의 활성 핸들 클릭 대상에만 적용한다.
- `layerHierarchyRows`, `addEmptyLayer`, `addDefaultDrawingLayer`, `activateLayerByKey`, `deleteLayerByKey`, `renameLayerByKey` 및 `applyLayerHierarchyOrder`를 노출하여 LVRS 패널이 캔버스 상태 소유자를 우회하지 않고 현재 레이어 스택을 추가, 선택, 이름 바꾸기, 삭제 및 재정렬할 수 있도록 합니다. 초기 앱 로드와 빈 `newCanvas` / `clearCanvas` 흐름은 기본 레이어 헬퍼를 호출하여 `Background` 위에 선택된 투명 `Layer 1`를 생성합니다.
- 비트맵 썸네일 URL 을 `iconSource` 를 통해 계층 구조 행에 공급하며, 변환 가능한 객체는 작은 PNG 썸네일로 래스터화되고, 기본/ 래스터 레이어는 비동기 32px `grabToImage` 미리보기를 작은 파일로 캐싱하며, 가져오기가 사용 불가능할 때만 C++ 전체 래스터 PNG 캐시로 회귀합니다. 래스터 썸네일 새로고침은 `DrawingSurfaceItem` 보고서 래스터 콘텐츠 변경 후 지연 및 병합되며, 붓 입력이 레이어 미리보기를 동기적으로 인코딩하지 않도록 하고, 제거된 래스터 레이어는 대기 중인 새로고침 및 썸네일 URL 을 지웁니다.
- 추가된 래스터 레이어를 자체 투명 `DrawingSurfaceItem`로 백업하고, 채우기 및 활성 레이어 실행 취소/다시 실행을 선택한 래스터 표면으로 라우팅하고, QML 대리자 재구축 전체에서 레이어 표면의 스냅샷을 찍고, 레이어가 삭제되면 레이어 픽셀을 제거합니다.
- 증분 `drawableObjectVisualModel`를 통해 렌더링하는 동안 `drawableObjects`를 문서/세션 스택으로 유지하므로 많은 레이어를 추가하면 모든 기존 래스터 레이어 표면을 재설정하고 PNG 스냅샷 복원 변동을 강제하는 대신 새 대리자만 생성됩니다.
- 기본 캔버스와 추가된 래스터 레이어 모두에 대해 16 ms 프레임 간격으로 브러시 라이브 미리 보기 작업을 실행하여 표시되는 프레임 케이던스 위의 iiPaintEngine 미리 보기 작업을 피하면서 원시 입력 누적을 즉시 유지합니다.
- 붓 및 지우개 발자국 피드백을 포인터 중앙에 위치한 2 `QtQuick.Shapes` 벡터 경로로 표시합니다. 경로 지름은 `brushSize * canvasZoomScale` 논리 화면 픽셀이며, 실제 값의 붓은 6 DIP 미만에서 비례적으로 얇아지고, Qt는 모니터 DPI 를 적용하며, 정확한 발자국이 서브픽셀일 때만 표준 교차선이 사용 가능합니다. 포인터 좌표는 `mapToItem` 를 사용하며, 고-z 윤곽은 캔버스 가장자리에서 페인트 위에 완전히 남아있는 비자른 뷰포트 뷰포트에서 렌더링됩니다. 비차단 `HoverHandler` 뷰포트는 비활성 이동 처리를 담당하고, 수동 `PointHandler` 는 `Qt.NoButton` 를 사용하여 합성 마우스 이벤트를 통해 진정된 마우스/태블릿 입력을 유지하면서 iiPaintEngine 의 독점 그리기를 사용하지 않습니다.
- 줌 도구 드래그 입력을 뷰포트 -크기의 `MouseArea` 뷰포트에 유지하여 빈 작업 공간에서 캔버스 주변으로 가로 줌 드래그가 캔버스 자체에서뿐만 아니라 작동하며, Control/Command+Space 는 같은 오른쪽에서 확대, 왼쪽에서 축소 스크러비 제스처를 이 경로를 통해 임시로 라우팅하지만 `toolMode` 를 변경하지 않습니다. 커서는 관련 없는 가로 크기 조정 커서가 아닌 정밀 교차선으로 유지됩니다.
- 비례 작업 공간 삽입을 사용하여 창을 채우는 대신 어두운 여백이 보이는 도구 모음 아래에 초기 및 지워진 캔버스를 만듭니다.
- 툴바 모달에서 전달된 명시적인 너비와 높이 값으로 새 캔버스를 생성하여 지원되는 래스터 차원 범위로 제한한 후, 요청된 캔버스보다 작업 공간 뷰포트 가 더 클 때에만 초기 줌을 축소합니다. 모달의 LVRS 무한 캔버스 체크박스는 대신 iiSharedCanvas 1.1 희소 캔버스를 생성하여 256픽셀 크기의 블록을 가지며, 이러한 차원들을 초기 할당 영역으로 취급합니다.
- 이동 또는 줌으로 할당된 영역 바깥의 월드 공간이 드러나면 무한 캔버스를 확장합니다. 확장은 차원 경계를 따르며, 기본 및 추가된 래스터 표면을 동기화하고 좌/상 성장을 위한 로컬 세션 객체 좌표를 이동시키고, 중심 기반 이동 오프셋을 보정하여 기존 월드 콘텐츠가 화면에서 점프하지 않도록 합니다.
- 이미 생성된 캔버스를 정적으로 유지합니다. 나중에 창이나 뷰 모델 캔버스 크기를 변경해도 크기가 조정되지 않습니다.
- `Background` 가 존재하는 동안 흰색 종이 배경에 고정 캔버스 영역만 표시하고, `Background` 가 제거된 후에는 해당 영역을 타일식 PNG 스타일 투명성 격자로 전환하며, 리사이즈된 뷰포트 오버플로를 LVRS 작업 영역 색상으로 남겨두되, 잘린 캔버스 표면 아래 위계 순서로 기본 래스터 캔버스, 래스터 레이어 표면, 그리고 변환 가능한 객체 오버레이를 중첩합니다.
- 뷰포트 배치, 휠 포커스 처리, 키보드 단축키, 임시 Space 카메라 모드 바인딩 및 도구 모음 상태 바인딩을 담당하는 QML를 유지합니다.
- 모든 캔버스 커서를 하나의 유효한 도구 함수를 통해 해결합니다: 붓과 지우개는 크기 정확한 원형 윤곽 아래에 빈 시스템 커서를 사용하고, 십자선 대체 경로 는 하나의 논리 픽셀 아래에 있으며, 줌과 모양은 정밀 십자선을 사용하고, 채우기는 손가락을 가리키는 손, 텍스트는 I-빔, 이동은 열려진/닫힌 손이며, 이동은 현재 포인터 위치에서 화살표, 4방향 이동 또는 방향성 리사이즈를 해결합니다. 도구 변경 및 임시 카메라 모드 진입은 캐시된 변환 핸들 호버 상태를 명확히 하여 오래된 크기 조정 커서가 다른 도구로 누출되지 않도록 합니다; Space 는 즉시 브러시 윤곽선을 손가락 커서로 대체하고, Control/Command+Space 는 줌 교차선을 사용합니다.
- 보류 중인 텍스트/도형 배치를 커밋하고 `DrawingSurfaceItem`에게 래스터 캔버스와 현재 세션 개체를 Photoshop 스타일 레이어 레코드에 매핑하도록 요청하는 PSD 호환성 매니페스트 도우미를 노출합니다.
- 참가자 입력 명령 캡처/적용 및 메모리 내 공식 전체 세션 스냅샷/적용 도우미를 노출하며, LAN 세션의 QML 객체/ 래스터 레이어 재구성이 완료된 후 복원 완료 신호가 전달됩니다; 합쳐진 표면은 수신된 상태를 로컬 편집으로 되돌리거나 발표할 수 없습니다.

<a id="core-c-components"></a>

## 핵심 C++ 구성 요소

### `TemporaryCameraInput`

- 캔버스 단축키가 활성화된 상태에서는 애플리케이션 수준의 Space 와 Control/Meta 키 전환만 필터링하여 `pan`, `zoom` 또는 비활성 모드를 QML 에 노출시키되, 더 넓은 LVRS 런타임 이벤트 데몬을 활성화하지 않습니다. 두 Qt 수정자 매핑이 모두 허용되므로 물리적 Control 과 Command 는 macOS 에서뿐만 아니라 다른 플랫폼에서도 지원됩니다.
- 편집/모달 입력으로 인해 필터가 비활성화되거나 응용 프로그램/창이 비활성화될 때 보유된 카메라 키를 지워 초점 변경 후 손이나 확대/축소 커서가 멈추는 것을 방지합니다.

### `DrawingSurfaceItem`

- `iiSharedCanvas::CanvasItem` 를 상속받습니다; 하나의 항목은 정적  래스터 , 정적 벡터, 그리고 유지- 키프레임이 적용된   래스터 /벡터 레이어를 동일한 네이티브 문서에서 렌더링할 수 있으며  iiPaintEngine 는  비트맵 스트로크 래스터화 작업에 계속 책임을 집니다.
- 혼합 렌더링 프레임을 표시 및 플랫 내보내기에만 사용합니다. 플랫 내보출 및 내장된  래스터 -레이어 스냅샷 추출은 권한 있는 문서에서 결정론적인 동기식  `renderFrameRegion()` 를 호출하므로  iiSharedCanvas 의 비동기 디스플레이 타일 완료는 빈 파일이나 구식 파일을 생성할 수 없습니다. 래스터 텍스트, 형상, 및 채우기 작업은 선택된 래스터 자산, 맵 문서 좌표를 레이어의 역 아핀 변환을 통해 읽고 대체하며, 해당 자산에 가시적인 벡터 또는 자매 레이어를 평평하게 만들지 않습니다. 플랫 비트맵 를 여는 것은 항상 이전에 열린 혼합 문서와 차원이 일치하더라도 대체 단일 래스터 문서를 생성합니다.
- Congregation의 이전 QML 방향 명령(예: `newCanvas`, `openRaster`, `saveToFile`, `undo`, `redo` 및 호환 스트로크 방법)을 유지합니다.
- 유한/무한 모드 및 선택적 부호 있는 세계 원점으로 `newCanvas`를 확장하고, 일반 및 래스터 레이어 스냅샷 복원 중에 스파스 모드를 유지하고, 네이티브 문서나 최근 문서를 열 때 연속 또는 청크된 래스터 콘텐츠를 선택합니다.
- 검증된 iiSharedCanvas 코덱을 통해 정규 `.iisc` 문서를 열고, Congregation의 열기 대화 상자에 노출하며, C++ API를 통해 직접 네이티브 저장을 지원합니다. 'Save As'는 `.iisc`를 노출하지 않으며, QML 세션 레이어는 네이티브 문서 외부에 남아 있습니다.
- 사적 최근 세션 경계로 `saveRecentCanvas` 와 `openRecentCanvas` 를 노출합니다. 작성자는 정준 iiSharedCanvas 문서를 라이브 시각적 범위로 표준화한 후, 추가 래스터 레이어 및 삽입된 이미지 PNG 바이트와 JSON 안전 편집 가능 객체 메타데이터를 하나의 소유자 전용 원자적으로 대체 파일에 임베드합니다. 읽기는 버전, 크기, SHA-256 해시, 중첩 `.iisc`, 객체 식별자 및 모든 PNG 를 확인한 후 현재 문서를 대체하며, 복원된 PNG 는 표면의 소유권을 가진 단일 소유자 전용 임시 디렉토리를 공유하여 `.vrc` 를 유일한 영구 최근 아티팩트로 남깁니다.
- 파일 시스템 I/O 없이 `exportCanvasSession` 및 `importCanvasSession`를 동일한 코덱 및 유효성 검사 경계로 노출합니다. 따라서 LAN 공유는 두 번째 문서 형식을 유지하는 대신 최근 캔버스 복구에 사용되는 세션 형식을 정확하게 직렬화합니다.
- 네이티브 `.iisc` 와 개인 `.vrc` 읽기, 원자적 쓰기, 부모 디렉토리 생성, 최근 컨테이너 삭제를 `iiFileProvider::File` 에 위임합니다. 애플리케이션은 코덱 검증과 소유자 전용 최근 파일 권한을 유지합니다. 경쟁하는 제공자 작성기는 기존 바이트를 대체하거나 최근 캔버스 선호도를 지우지 않고 저장/삭제를 실패하게 만듭니다. `iiFileProvider` 는 Congregation 나 iiSharedCanvas 에 대한 의존성이 없으며, 제품은 저장 API 와 독립적인 도메인 코덱을 구성합니다. 네이티브 및 최근 컨테이너 왕복 변환 테스트는 이 잠금 충돌 동작을 실행하며, 선호도 테스트는 잠금이 해제된 후 삭제를 확인합니다.
- 참여자 입력 모드에서, 거울은 마우스/태블릿 샘플을 iiSharedCanvas 의 라이브 스트로크 렌더러로 캡처하여 즉시 로컬 피드백을 제공하고, 릴리스 시 미리보기를 취소하여 스트로크 수, 되돌리기 기록, 픽셀 및 세션 직렬화 이 미삭제 상태를 유지하며, 호스트 애플리케이션에 완료된 한계가 설정된 명령만 전송합니다.
- `imageObjectForFile` 를 PSD 미리보기/객체 호환성을 위해 노출하고, `clipboardImageObject` 를 원본 클립보드 픽셀 또는 복사된 로컬 파일용이며, `canImportDroppedImage` / `importDroppedImage` 를 Qt 퀵 드롭 이벤트용으로 사용하며, 주된 `openRaster` 경로는 기본 래스터 캔버스 대신 각 평면 이미지를 소스 픽셀 차원에 유지합니다. 삽입 결과는 안정적인 상태 코드 ( `ready` , `clipboard-unavailable` , `no-image` , `decode-failed` , `image-too-large` , `cache-write-failed` , `download-failed` , 또는 `download-too-large` ) 를 가지며, 해시하기 전에 픽셀 바이트를 처리하기 위해 한 변당 32,768 픽셀 이상 또는 64 메가픽셀 이상의 차원을 거부합니다. 구현은 의존성을 추가하는 대신 Qt 의 기존 `QClipboard` , `QMimeData` , `QImageReader` , `QSaveFile` , 및 `QNetworkAccessManager` API 를 재사용합니다. SHA-256 -해결된 PNG 캐시 히트는 읽을 수 있는 일치하는 이미지 크기가 있는지 확인되며, 대체는 원자적으로 커밋됩니다. 원격 대체 경로 는 HTTP (S) 을 허용하며, 안전하지 않은 리디렉트만 허용하고, 30 초 후 타임아웃되며, 인코딩된 전송을 64 MB 로 제한하며, 지속된 소스 메타데이터에서 인증 정보, 쿼리 매개변수, 조각, 및 데이터 URL 을 제거합니다.
- QML가 텍스트 경계, 내용, 텍스트 크기 및 텍스트 색상을 Qt 텍스트 레이아웃에 전달하고 결과를 iiPaintEngine의 래스터 대체 경로를 통해 다시 커밋할 수 있도록 `commitText`를 노출합니다.
- QML가 모양 경계, 선택한 모양 종류 및 채우기 색상을 Qt 페인터 경로에 전달하고 단색 결과를 iiPaintEngine의 래스터 대체 경로를 통해 다시 커밋할 수 있도록 `commitShape`를 노출합니다.
- `fillAt`를 노출하여 QML가 캔버스 점과 브러시 색상을 iiPaintEngine의 래스터 대체 경로를 통해 다시 커밋되는 정확한 색상 플러드 채우기로 전달할 수 있습니다.
- `saveToFileWithObjectsAndRasterLayers`를 노출하므로 QML는 해당 개체를 라이브 래스터 상태로 병합하지 않고도 현재 기본 래스터 캔버스와 라이브 래스터 레이어 표면, 변환 가능한 이미지, 텍스트 및 모양 세션 개체를 저장할 수 있습니다.
- `.psd` 경로 8비트 RGB Photoshop 문서로 작성하며, 여전히 존재하는 불투명 흰색 `Background` 캔버스 레이어, 현재 래스터 /이미지/텍스트/형상 객체마다 하나의 래스터화 레이어, 호환성 명세에 대한 Congregation XMP 메타데이터를 병합된 미리보기와 함께 작성하여, Photoshop 가 `Background` 가 있을 때 하단에 표시하는 하단에서 상단으로의 레이어 기록 순서를 유지합니다.
- QML가 현재 래스터 캔버스 및 변환 가능한 세션 개체에 대한 Photoshop 스타일 매니페스트를 검색할 수 있도록 `psdCompatibilityManifest`를 노출합니다.
- `.psd` 가져오기는 `PsdImageReader`를 거친다. 가져올 수 있는 레이어형 PSD 문서는 캐시된 전체 캔버스 PNG 스냅샷을 통해 Congregation 래스터 레이어가 된다. 평면 PSD 이미지 객체 가져오기도 캐시된 PNG 미리보기 소스를 받고 원본 PSD 소스 메타데이터를 유지한다.
- 브러시 상태, 도구 모드 및 캔버스 크기를 `CanvasDocumentViewModel`부터 `CanvasViewModelBridge`까지 동기화합니다.
- QML 기반 캔버스 표면 크기 업데이트를 원자적으로 적용하므로 시작 크기 조정이 문서 모델에 부분적인 1픽셀 크기를 남길 수 없습니다.
- 현재 플랫 래스터 문서 메타데이터에 대해 `backgroundSource` 및 `hasBackground`를 노출합니다.
- `rasterContentChanged` 를 래스터 픽셀이 변경될 때 방출하며, 붓 스트로크를 iiPaintEngine 의 커밋 후 `strokeCountChanged` 신호로 연결하고, 계층 패널이 `Background` 에 사용하는 썸네일 캐시 도구를 노출하며, 래스터 레이어와 래스터화 객체 행에 추가됩니다. QML 계층 레이어는 래스터 썸네일 작업을 비활성화 타이머 뒤에 배치하고, `grabToImage` 를 사용하여 32px 미리보기 파일을 캐싱하며, 래스터 전체 PNG 생성을 대체 경로 로만 호출합니다.

### `CanvasDocumentViewModel`

- 팔레트, 브러시 색상, 브러시 크기, iiPaintEngine 브러시 설정, 활성 도구, 선택한 모양 종류 및 캔버스 크기를 QML에 노출합니다.
- iiPaintEngine의 적용 범위 기반 원형 브러시에 대한 기본 브러시 경도를 앱의 최대 앤티앨리어싱 가장자리 설정으로 설정합니다.
- 브러시 크기, 브러시 역학, 압력 곡선, 압력 불투명도 활성화, 안정 장치 및 캔버스 크기를 안전한 범위로 고정합니다.
- 도구 모드를 flat-래스터 도구 세트(브러시, 지우개, 이동, 이동, 확대/축소, 모양, 채우기 및 텍스트)로 제한합니다.

### `PsdCompatibilityDocument`

- 앱의 래스터/세션 스택에 대한 내부 PSD 호환성 경계를 제공합니다.
- Photoshop 스타일 위쪽/왼쪽/아래쪽/오른쪽 레이어 경계, Photoshop 혼합 모드 키, 바이트 불투명도, 가시성 및 레이어별 페이로드를 사용하여 RGB, 8비트 문서 매니페스트를 저장합니다.
- 캔버스에 아직 레이어가 있는 경우 하단 `Background` 레이어를 생성하고 현재 래스터, 이미지, 텍스트 및 모양 세션 레이어를 그 위의 정렬된 레이어 레코드에 매핑합니다.
- 레이어 경계를 현재 캔버스에 고정하고 캔버스에 PSD의 30,000 px 가장자리 제한보다 큰 플래그를 PSD와 호환되지 않는 것으로 표시합니다.

### `PsdImageReader`

- BSD-2-Clause `psd_sdk` 파서를 사용하여 Congregation 내부에서 직접 Photoshop PSD 파일을 읽습니다.
- 8비트 RGB/RGBA 병합된 이미지 데이터를 `QImage`로 변환합니다.
- XMP 메타데이터에 대한 이미지 리소스를 구문 분석하고 Congregation의 base64 JSON 레이어 매니페스트가 있는 경우 이를 복원합니다.
- 레이어 마스크 섹션을 구문 분석하고, 가져올 수 있는 8비트 RGB/RGBA 래스터 레이어를 추출하고, 아래쪽에서 위쪽 레이어 이미지와 경계, 불투명도, 가시성, 혼합 모드 키 및 마스크 존재 플래그를 QML에 노출합니다.

### `PsdImageWriter`

- BSD-2-Clause `psd_sdk` 내보내기를 사용하여 Photoshop PSD 파일을 Congregation 내부에 직접 작성합니다.
- Congregation 의 하단에서 상단으로의 래스터 /세션 객체 스택을 PSD 레이어로 변환하여 해당 기록 순서를 뒤집지 않으며, 기본 래스터 캔버스를 Congregation 의 흰색 종이 색상 위에 작성하여 하단 불투명 `Background` 레이어로만 작성하고, 현재 래스터, 이미지, 텍스트, 형상 레이어는 그 위에 투명 가능 레이어로 작성합니다.
- Congregation 호환성 버전, 레이어 수 및 PSD 호환성 매니페스트의 base64인코딩 JSON 복사본에 대한 XMP 메타데이터 키를 추가하여 개체 경계, 소스/텍스트/모양 페이로드 및 레이어 순서가 PSD 내보내기에서 유지됩니다.

### `CanvasViewModelBridge`

- 활성 LVRS 문서 보기 모델을 해결합니다.
- 예상되는 문서/뷰 바인딩을 사용할 수 있을 때까지 캔버스 변형을 차단합니다.
- 모델의 캔버스 크기, 도구 상태 및 iiSharedCanvas 비트맵 브러시 속성을 렌더링된 표면과 정렬된 상태로 유지합니다.

<a id="data-flow-summary"></a>

## 데이터 흐름 요약

1. `main.cpp`는 Qt/LVRS 지원 QML 애플리케이션을 실행하고 공유 보기 모델과 비활성화된 적용 모드의 `LicenseManager`를 포함한 도우미 서비스를 등록합니다.
2. 표시되는 기본 창이 설정된 후 `NearbyCongregationDiscovery`는 작업자를 비동기식으로 시작하고 익명의 로컬 Congregation 존재와 선택적 호스트 포트를 유지합니다. 명시적인 공유/결합 작업은 별도의 `LocalCanvasSession` TCP 데이터 경로를 생성합니다.
3. `Main.qml`는 비활성화된 정책에 따라 즉시 `PainterCanvasPage`를 구성합니다. 적용이 다시 활성화되면 `LicenseActivationPage`는 확인된 이메일과 키를 제출하고 애플리케이션 소유 제품 `congregation`에 대한 권한 있는 `valid: true`만 페이지를 구성합니다.
4. `PainterCanvasPage`는 `CanvasDocumentViewModel`에 바인딩되고 브러시 상태를 `DrawingSurface`에 전달합니다.
5. `CanvasToolBar`는 파일 흐름, 도구 선택, 모양 선택, HSL 색상 선택기 변경, 브러시 크기 업데이트 및 브러시 재선택 설정에 대한 사용자 작업을 내보냅니다.
6. `DrawingSurface`는 `DrawingSurfaceItem`를 호스트합니다. 항목은 혼합 문서 작성에 iiSharedCanvas를 사용하고 비트맵 스트로크를 iiPaintEngine에 위임합니다.
7. `PainterCanvasPage` 는 `LV.Hierarchy` 를 왼쪽에 호스팅하여 `DrawingSurface.layerHierarchyRows` 에 바인딩하고, 행은 `iconSource` 를 통해 캐시된 레이어 비트맵 썸네일을 사용하며, 빈 문서에는 선택된 투명 `Layer 1` 로 시작하고, 활성화는 일치하는 세션 객체를 선택하며, 반복된 활성화/더블 클릭은 이름 변경을 위한 인라인 `TextInput` 을 엽니다, 푸터 추가는 또 다른 투명 래스터 레이어를 생성하고, 행 드래기는 `drawableObjects` 순서를 재작성하며, 푸터 삭제는 선택된 레이어를 포함하여 `Background` 를 제거한다.
8. iiSharedCanvas 는 네이티브 문서, 프레임 합성, 선택된 래스터 편집 이력, 및 `.iisc` 코덱 경계를 소유하며, iiPaintEngine 는 스트로크 래스터화와 합성 를 수행합니다. Congregation 는 추가된 래스터 레이어와 이미지/텍스트/형상 객체를 QML 세션 오버레이로 유지하며, 채우기 대체를 선택된 래스터 표면에 라우팅하고, 레거시 세션 스택을 래스터 / PSD 저장용으로 합성합니다.
9. 권위 있는 호스트 변형은 1.2초 로컬 최근 캔버스 타이머를 재시작합니다. 호스트 사용자의 직접 편집도 350 밀리초 LAN 발행 타이머를 재시작하며, 참여자 브러시 입력은 즉시 취소 시 릴리스 로컬 미리보기로만 렌더링된 후 하나의 한계가 설정된 명령으로 교차하고, 동일한 호스트 캔버스에서 실행되며, 다음 이벤트 사이클을 위한 호스트 스냅샷을 대기열에 추가합니다. 호스트만이 전체 `.vrc` 상태를 단조로운 수정 프로토콜을 통해 내보내고 전송합니다. 연결된 클라이언트는 해당 상태를 발행하거나 자신의 최근 캔버스로서 유지하지 않고 복원합니다.
10. PSD 작업이 문서 구조가 필요할 때 `PsdCompatibilityDocument` 는 동일한 선택적 기본 래스터 캔버스와 세션 객체를 PSD 스타일의 레이어 매니페스트로 변환하고, `.psd` 저장 시 Congregation 의 흰색 캔버스 종이 위에 래스터 를 합성하여 `Background` 가 존재하는 동안 래스터화된 레이어/객체 이미지 및 해당 매니페스트 메타데이터를 `PsdImageWriter` 에 전달한다.
11. PSD 를 열 때 `PsdImageReader` 는 XMP 메타데이터, 합쳐진 이미지 데이터, 그리고 `psd_sdk` 를 통해 가져올 수 있는 8비트 RGB / RGBA 래스터 레이어를 읽으며, QML 는 레이어화된 PSD 를 Congregation 래스터 레이어로 복원하고 평면 PSD 이미지 객체의 경우 캐싱된 합쳐진 PNG 미리보기로 회귀합니다.

<a id="testing-surface"></a>

## 테스트 표면

- `tests/tst_canvasdocumentviewmodel.cpp`는 iiPaintEngine 브러시 설정, 지원되는 도구 모드 및 지원되는 모양 종류를 포함하여 평면 래스터 문서 상태 및 값 클램핑을 검증합니다.
- `tests/tst_psdcompatibilitydocument.cpp`는 캔버스 메타데이터, 선택적 래스터 기본 레이어 생성, 세션 개체 레이어 매핑, PSD 경계, 불투명도, 가시성 및 PSD 캔버스 크기 제한을 포함한 PSD 호환성 매니페스트의 유효성을 검사합니다.
- `tests/tst_canvastoolbarqmlcontract.cpp` 는 스톡 LVRS 도구 모음, 메뉴, 토글 및 계층 구조 기하학 사용, 새 캔버스 크기 모달, 클립보드 이미지 삽입, 캔버스 `DropArea`, 드롭된 이미지 성공/실패 전파 및 이동 도구 인수 전달, 기본 빈 레이어 생성, 브러시 재선택 설정, 패닝/이동/확대/형상/채색 도구 선택, 전체 애플리케이션 공간 패닝 및 컨트롤/커맨드+스페이스 확대 라우팅, 중앙 집중식 유효 도구 커서 해결, 수동 태블릿 안전 포인터 추적과 확대 보정 실제 스트로크를 갖춘 벡터 이중 링 브러시/지우개 커서, 객체 변환, 잘린 캔버스 렌더링, 라이브 래스터 레이어 계층 구조 동작, 내선 이름 변경 편집, 푸터 동작, 왼쪽 클러스터 현재 색상 원의 2픽셀 흰색 테두리, 후미 프레젠테이션/프로필 컨트롤의 직접 스톡 `LV.IconButton` 계약, 그리고 프로필의 초대 배지/맥락 메뉴 불리언 응답 계약을 검증합니다.
- `tests/tst_mainqmlcontract.cpp` 는 네이티브 컨트롤, 플랫폼/풀스크린 드래그 영역 선택, 네이티브 macOS 와 콤팩트 인 윈도우 메뉴 계약, 단일 소유자 단축키 등록, 섹션화된 선호도 헤더, 일반 컨트롤/액션 와이어링, 프로필 이미지/이름/초대 컨트롤, 클립보드 붙여넣기 실패 메시징, 그리고 명시적 비활성화 강제 캔버스 정책을 포함하는 LVRS 애플리케이션 윈도우 크롬 계약을 유효성 검사합니다.
- `tests/tst_memberprofilelistbuilder.cpp`는 로컬 호스트 표시, 원격 호스트 우선 순서 지정, 정확한 역할 접미사, 소스 인덱스 보존, 상위 공급 측 자체 중복 제거 및 이름 없는 프로필 대체 경로를 검증합니다.
- `tests/tst_licensemanager.cpp` 속성을 검증하여 비활성화 모드가 인증 저장소 또는 네트워크 액세스 없이 잠금이 풀린 다음, 유지된 활성화 모드 POST 본문과 헤더, 표준화된 검증된 이메일, 애플리케이션 소유 제품 ID, 엄격한 불리언 응답 파싱, 유효하지 않은 라이선스 결정, 리디렉션, 잘못된 형식의 응답, 5xx, 연결 실패, 그리고 프로덕션 엔드포인트를 사용하지 않고 타임아웃을 수행합니다.
- `tests/tst_nearbycongregationdiscovery.cpp` 속성을 검증하여 익명 비콘 스키마, 선택적 초대 기능 및 공유 포트 경계, 대상 초대 스키마, 잘못된 형식/외부 거부, 중복 억제; 시작이 전용 작업자 스레드에 대기열에 등록되고 멀티캐스트를 통해 2 격리된 서비스를 실행하여 엔드포인트 발견, 초대 전달, 철회, 그리고 명시적인 오프라인 삭제를 증명합니다.
- `tests/tst_localcanvassession.cpp` 속성을 실행하여 초대 거절과 수락, 핸드셰이크, 초기 상태 전달, 참여자/프로필 업데이트, 한계가 설정된 한계가 설정된 편집 명령 전달, 호스트 애플리케이션 전에 명령이 리비전을 진행할 수 없음을 증명, 호스트 브로드캐스트, 클라이언트 스냅샷 업로드 거부, 잘못된 형식 명령 거부, 호스트 제거, 유효하지 않은 스냅샷 거부, 그리고 유효하지 않은 엔드포인트 거절을 수행합니다.
- `tests/tst_linuxbuildworkflowcontract.cpp`는 Linux 상대 RPATH, 생성된 Qt/QML 런타임 배포, 루트 상대 TGZ 레이아웃, 데스크탑 항목 및 문서 계약을 검증합니다.
- `tests/tst_windowsbinarycontract.cpp`는 실제 실행 파일을 실행하고 3초 동안 네이티브 외부 및 클라이언트 크기를 샘플링하여 비동기 시작 시 눈에 띄는 크기 전환이 발생하지 않도록 합니다.
- `tests/tst_drawingsurfaceitem.cpp` 는 Congregation -to- iiSharedCanvas 경로를 검증하며, 혼합된 래스터 /vector/ 키프레임 합성, `.iisc` 왕복 변환, 네이티브 문서의 전체 최근 컨테이너 왕복 변환, plus 래스터 /image/text/shape 세션 객체, 라이브 시각적 범위 보존, 세션 범위로 추출된 자산 정리, 그리고 손상된 컨테이너 무변형 동작, plus 그리기, 지우기, 채우기, 텍스트 및 모양 래스터 커밋, 참여자 측 라이브 스트로크 미리보기와 호스트 명령 발신 전 정확한 픽셀/이력 롤백, 시스템 클립보드 이미지 캐싱/중앙 정렬/선택 및 이후 객체 이동, 로컬 복사된 이미지 파일 대체 경로, 클립보드 소유권 상실, 끌려온 인코딩된 MIME 이미지, 파인더 스타일 로컬 파일 드롭, 브라우저 HTML / HTTP 이미지 드롭을 위한 격리된 로컬 테스트 서버, 드롭 포인트 배치, 잘못된 형식 및 과도한 입력, 차단된 캐시 복구, 무변형 실패 동작, 유효한 도구 커서 매핑, 네이티브 윈도우 브러시/지우기 커서 지름과 눌린 이동 중 핫스팟 추적, 컨텍스트 감응형 이동/바디/핸들 커서, 오래된 리사이즈 호버 정리, 변환 가능한 이미지/텍스트/모양 객체 이동/리사이즈/삭제 포함 확대된 핸들 타겟, Shift 제한 직선 이동 및 비율 잠금 리사이즈, 클립된 시각적 범위, 그리고 캔버스보다 초과할 수 있는 경계, 모양/텍스트 도구 생성을 별도의 레이어 행으로, 초기 빈 레이어 생성, 계층 래스터 -레이어 생성 시 변환 타격 테스트 없이, 래스터 -레이어 삭제 시 해당 픽셀을 합성 출력에서 제거, 배경 레이어 삭제, 투명성 그리드 가시성, PSD 출력에서 배경 생략, 많은 레이어 생성 시 스냅샷 변화 없음, 계층 레이어 이름 변경·계층 레이어 행 투영과 재정렬·손 도구 캔버스 이동·네이티브 누름/이동/해제 순서를 사용하는 뷰포트 전체의 선택 도구 및 임시 Control/Command+Space 가로 드래그 캔버스 확대/축소·메타데이터를 담은 레이어형 PSD 출력을 포함한 합성 객체 저장·`psd_sdk`를 통한 PSD 병합 프리뷰와 레이어 가져오기·실행 취소/다시 실행·저장·원본 해상도에서 평면 이미지 캔버스의 반복 교체·명시적 크기의 새 캔버스 생성·작업 공간 인셋 캔버스 생성을 다룬다. 네이티브 포인터 사례는 합성 마우스 입력과 프레임 캡처를 위해 실제로 노출된 창이 필요하며, 빌드 후 각 픽스처를 노출하도록 macOS compositor에 최대 15초를 허용한다.
- `-DBUILD_TESTING=ON`로 구성한 후 `ctest --test-dir build --output-on-failure`로 제품군을 실행합니다.

<a id="graphql-license-authority"></a>

## GraphQL 라이센스 권한

`validateLicense(input: LicenseValidationInput!)` 는 `data.validateLicense` 를 반환합니다. 클라이언트는 `valid`, `code`, `productId`, `checkedAt`, 및 `expiresAt` 를 요청하며 공유 GraphQL 엔벨로프 파서에는 iiAcountManager 0.2.8 가 필요합니다. HTTP 200 오류 및 부분 데이터는 사용 불가능한 결정입니다: 앱 잠금 해제 또는 저장된 라이선스 삭제를 할 수 없습니다. 거짓 결과는 null productId 를 가질 수 있으며, 참 결과는 이 제품과 일치해야 합니다. 활성화 및 업데이트 권한은 업데이트된 SDK 를 사용합니다. 새 제품 빌드를 배포하기 전에 서버 GraphQL 를 배포해야 합니다.

빌드에서는 사용 가능한 경우 iiAcountManager에서 내보낸 QtKeychain 대상을 재사용하여 중복 종속성 대상을 방지합니다. 제품 라이센스/업데이트 테스트에서는 애플리케이션과 동일한 GraphQL 파서 및 종속성 그래프를 사용합니다.
