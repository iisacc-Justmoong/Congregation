# Congregation 네이티브 빌드와 검증

현재 진입점은 `build.sh`와 `build-windows.ps1`이다. CMake는 `App/Engine`, `App/Native`와 LVUI에서 생성한 C++23을 빌드한다. Qt5·Qt6 탐색과 Qt 코드 생성은 끈다. 실행 중에는 LVUI 컴파일러, 브라우저, WebView, QML 엔진이 필요하지 않다.

iPadOS의 SDK 교차 빌드·Xcode 개발 서명·연결 기기 설치와 실행은 [IPADOS.md](IPADOS.md)를 따른다. 활성 `.app`는 macOS와 iPadOS 모두 같은 `build/Congregation.app`을 사용한다.

## 설치 키트

`CONGREGATION_SDK_PREFIX`는 기본 네이티브 키트를 지정하며 CMake는 그 안의 패키지 디렉터리를 우선 선택한다. 계정 API와 함께 독립 `LVUI::Native`, `LVUI::Color` 및 iiGUI 0.5.0 이상의 `iiGUI::iiGUI`가 필요하다. 0.5.0은 창 appearance의 글꼴 정책을 포함하는 새 C++ ABI이므로 선택한 LVUI·iiGUI의 생성기, 헤더와 라이브러리를 함께 다시 빌드해 일치시킨다. 각 패키지의 CMake 디렉터리와 `LVUI_HOST_COMPILER`를 별도로 지정할 수 있다. 앱은 Qt와 LVRS 패키지 탐색을 끈다. 앱의 편집·화면 타깃에는 iiGUI ABI 식별자를 컴파일 인수로 전달한다.

개발 중 서로 다른 설치 위치에 있는 호환 SDK를 선택할 때는 `CONGREGATION_LVUI_PREFIX`, `CONGREGATION_IIGUI_PREFIX`, `CONGREGATION_COLOR_MANAGER_PREFIX`, `CONGREGATION_CORE_PREFIX`를 설정한다. 각각 LVUI, iiGUI, iiColorManager, iiSDKCore의 **설치 접두 경로**이며 지정한 패키지가 없으면 구성에 실패한다. 명시한 패키지는 집계 키트보다 우선하고, iiGUI 설치본에 포함된 lunasvg·plutovg 패키지도 함께 선택한다. `build.sh`와 `build-windows.ps1`은 이 환경 변수를 CMake에 전달한다. iPad 등 교차 빌드에는 대상용 라이브러리와 별개로 호스트에서 실행되는 일치하는 `LVUI_HOST_COMPILER`가 필요하다. 선택한 설치본은 플랫폼·ABI와 헤더 및 라이브러리 버전이 서로 호환되어야 한다.

이 설정은 CMake 캐시에 유지되므로 이후 셸에서 환경 변수를 생략해도 이전 선택이 남는다. 선택을 바꿀 때는 새 경로를 지정하고, 집계 키트의 패키지로 돌아갈 때는 해당 `CONGREGATION_*_PREFIX`를 CMake 인수 `-D이름=`으로 비운 뒤 다시 구성한다. 집계 키트에 그 패키지가 없으면 이전 패키지의 `*_DIR` 캐시도 비우거나 새 경로를 지정한다.

현재 앱에는 실시간 획 미리보기와 유한 청크 문서를 지원하는 `iiSharedCanvasNative` 0.35.38
이상이 필요하다. 오래된 키트에는 필요한 API 또는 문서 지원이 없으므로
네이티브 SDK와 소비 키트를 갱신한 뒤 빌드한다. 별도 스테이징한 Society 계정 SDK를 사용해야
한다면 `CONGREGATION_ACCOUNT_MANAGER_PREFIX`로 그 설치 접두 경로를 지정한다.
해당 패키지는 나머지 네이티브 키트와 ABI 및 대상 플랫폼이 호환되어야 한다.

키트의 재구성 순서와 고정된 외부 의존성은 `../../SDK/iiSDKBuilder/docs/congregation-native.md`에 있다. 각 수정 SDK의 네이티브 설치 스크립트는 구성·빌드·검사·설치를 수행한다. iiNetwork는 `install.sh`, 나머지 수정 모듈은 `install-native.sh`를 사용한다. macOS 키트는 모든 실행 의존성을 최소 macOS 12.0으로 빌드한다. OpenSSL과 이미지 코덱은 키트 안의 정적 라이브러리이며 시스템 CURL·XML2를 사용한다. MoltenVK와 필요한 동적 SDK는 앱 번들 안에 수집한다.

Figma 색상과 이식된 원본 아이콘은 LVUI 설치본에서 제공한다. 과거
`prepare_legacy_style.py`와 LVRS 호환 패키지는 현재 빌드의 입력이 아니다.
호환 범위와 검사는 [UI 스타일 토큰](UI_STYLE_TOKENS.md)에 설명한다.

## macOS

저장소 루트에서 실행한다.

```bash
./build.sh local
```

기본 호출 `./build.sh`도 같은 로컬 네이티브 빌드와 전체 검사를 수행한다. 빌드 결과는 `build/Congregation.app` 하나이다. 현재 소스의 제품 버전은 1.0.2, 번들 빌드 번호는 3이다. 빌드 위치는 저장소의 `build/`로 제한한다.

```bash
CONGREGATION_SDK_PREFIX=/absolute/path/to/native/prefix ./build.sh local
./build.sh --configure-only
./build.sh --no-tests
ctest --test-dir build --output-on-failure
```

`--no-tests`는 반복 개발을 위한 옵션이며 완료 검증을 대신하지 않는다. 오래된 Qt 빌드 캐시에서 처음 전환할 때에는 기존 `build/CMakeCache.txt`와 `build/CMakeFiles`를 `build/native-migration-backup/` 아래에 보존한 뒤 다시 구성한다. 기존 앱과 사용자 파일을 삭제하거나 다른 위치에 활성 `.app` 복사본을 만들지 않는다.

빌드 후 `cmake/package_native_macos.py`가 실행 파일의 전체 동적 의존성을 따라가며 번들 안으로 수집한다. 명시적으로 링크한 iiSDKCore, iiFileProvider, iiPaintEngine 라이브러리 디렉터리도 선택 키트와 함께 허용하고 심볼릭 링크의 실제 위치를 확인한다. Qt·LVRS 설치 이름, 허용된 디렉터리 밖의 비시스템 라이브러리, macOS 12.0보다 높은 최소 OS를 선언한 이미지를 거부한다. `Congregation.native_package_directories`는 금지 이름과 정상 라이브러리 이름을 함께 검사한다. 절대 RPATH를 제거하고 번들 상대 경로로 연결하며 외부 라이선스 고지를 포함한 뒤 로컬 임시 서명을 검증한다. 이는 Developer ID 서명·공증·App Store 배포를 수행한 결과가 아니다.

직접 배포용 `Congregation.pkg`의 Developer ID 서명·설치 대상·페이로드 검증과 공증 절차는 [MACOS_INSTALLER.md](MACOS_INSTALLER.md)를 따른다. `CongregationInstaller` 타깃은 정식 앱에서 직접 패키지를 생성한다.

## 검사 계약

- `Congregation.engine`: Core 루프 소유·차용, 작업 수명, 읽기와 변경의 구분이다.
- `Congregation.services`: 저장 설정, 라이선스 정책, 네이티브 협업·오래된 리비전 거부와 서비스 경계이다.
- `Congregation.artwork`: 정규 IISC 저장·재열기, 레이어·벡터·이미지·텍스트·실행 취소, 이전 문서 이행과 읽기 불변식이다.
- `Congregation.lvui_native_boundary`와 `Congregation.independent_*`: 활성 화면의 Qt·LVRS 경계와 LVUI 화면·입력·모델·색상 동기화를 창 없는 위젯 트리에서 검사한다. `independent_*` 대상은 해당 CMake 옵션과 일치하는 iiGUI 테스트 헤더를 지정하면 켤 수 있다.
- `Congregation.canvas_reference`, `Congregation.canvas_setup`: Dreamscapes 원본 289개 프리셋·검색·단위 변환·크기 검증을 검사한다.
- `Congregation.canvas_window_native`, `Congregation.canvas_window_vulkan`: 비모달 New canvas 서브윈도우의 실제 메뉴·텍스트 입력·리사이즈·스크롤·키보드 동작을 검사한다. [상세 계약](NEW_CANVAS_WINDOW.md)을 참고한다.
- `Congregation.standalone_native`, `Congregation.standalone_vulkan`: 실제 네이티브 창에서 편집·한글 텍스트·드롭·색상 선택·새 캔버스·환경설정·프로필·내보내기를 실행하고 GPU 결과를 캡처한다.
- macOS의 `Congregation.bundle_relocation`: 정식 앱을 `build/` 안의 임시 위치로 **이동**하여 빈 작업 디렉터리에서 두 렌더러를 실행한다. 동적 로딩 목록이 시스템 또는 옮긴 번들 안에만 있는지 검사하고 `finally`에서 정식 경로로 복원한다. 임시 `.app` 복사본을 남기지 않는다. macOS의 `/System/Volumes/Preboot/Cryptexes/OS/usr/lib/`와 그 시스템 프레임워크 경로는 운영체제 라이브러리로 인정한다. SDK·Homebrew 등 임의 경로는 이 예외에 포함하지 않는다.

네이티브 테스트 파일과 임시 디렉터리는 `resources/` 아래 빌드별 격리 경로에 둔다. `CONGREGATION_TEST_RESOURCES`로 같은 자원 트리 안의 별도 경로를 지정할 수 있으며, 지정 경로의 상위 디렉터리는 미리 존재해야 한다. 자원 트리 밖을 가리키는 링크는 거부한다. 가능한 경우 `build/native-tests`는 그곳을 가리키는 링크이며, 링크 생성 권한이 없는 환경에서도 테스트는 실제 자원 경로를 사용한다. 번들 재배치 검사의 빌드 출력은 `build/standalone-relocation-*`을 사용한다. 로더 경로 덮어쓰기를 제거한다. 네이티브 UI 검사는 그래픽 세션이 있는 macOS 사용자 환경에서 수행한다. 검사 로그와 최소 OS·의존성·재배치 결과는 `build/`에 남긴다. 확정된 수행 결과는 [NATIVE_VERIFICATION.md](NATIVE_VERIFICATION.md)에 기록한다.

Developer ID hardened runtime으로 서명한 앱의 재배치 검사도 지원한다. 이 경우 제한될 수 있는 dyld 환경변수 진단에 의존하지 않고, 실행 중 `lsof` Mach-O 파일 매핑에서 앱과 모든 번들 dylib를 확인한다. 외부 경로와 관측되지 않은 필수 이미지는 실패이다.

수동 실행은 다음과 같다.

```bash
build/Congregation.app/Contents/MacOS/Congregation
build/Congregation.app/Contents/MacOS/Congregation --renderer vulkan
```

## Windows와 Linux

Windows는 네이티브 Windows용 SDK 키트를 준비한 뒤 `build-windows.ps1 -SdkPrefix <prefix>`로 구성·빌드·검사한다. Win32·Direct3D11, 네이티브 입력·IME·클립보드·드롭과 대화상자 소스가 연결되어 있다. DLL 수집·서명·설치 패키지 배포의 실제 검증은 별도이다.

Linux는 X11/XInput2·XIM, Vulkan, XDG portal/GIO 기반 소스이다. 같은 설치 키트 구조를 준비해 `./build.sh local`을 사용한다. 직접 Wayland 호스트는 포함하지 않는다. Linux OS 접근성 객체 노출에는 미구현 영역이 남아 있다.

Windows·Linux는 사용자의 지시에 따라 이번 작업에서 소스 구현만 수행한다. macOS의 빌드·입력·GPU 실행 결과로 다른 OS의 실행 성공을 주장하지 않는다.

이전 Qt·Developer ID·App Store 배포 절차는 [legacy/BUILD_QT.md](legacy/BUILD_QT.md)에 이행 당시 자료로 보존한다. 현재 네이티브 빌드 절차로 사용하지 않는다.

## 네이티브 빌드 자동화

`.github/workflows/native-build.yml`은 설치된 iiSDK 키트의 절대 경로와 운영체제별 자체 실행기 레이블을 받아 수동으로 빌드·검사한다. 그래픽 세션이 필요하며 Qt 설치·서명·배포는 호출하지 않는다. Windows·Linux의 실제 원격 실행은 이번 검증에 포함하지 않았다. 이전 Qt 기반 Windows 배포·대응 소스 워크플로는 `docs/legacy/automation/`에 원본으로 보존한다.

`Congregation.native_build_surface`는 활성 CMake·빌드 스크립트·워크플로에 Qt 탐색·설치·배포가 다시 들어오는지 검사한다. `Congregation.native_build_surface_contract`는 Qt 설치/탐색 거부와 정상 비활성화 플래그·이행 기록 허용을 별도의 build/ 내부 fixture로 검증한다.

## 네이티브 전이 의존성 검사 계약

구성 디렉터리의 마지막 단계에서 앱과 네이티브 아트워크 대상의 직접·전이 링크를
검사하므로 검사 파일을 포함한 뒤에 추가된 링크도 포함한다. Qt와
LVRS는 CMake 대상 이름, 직접 지정한 라이브러리·링커 옵션, 가져온 대상의 실제
라이브러리 경로 중 어느 경로로도 도달할 수 없어야 한다. 별칭과 링크 전용 래퍼,
설정별 분기 안에 있는 대상도 검사한다. 사용하지 않는 분기라도 Qt·LVRS 참조를
명시하면 플랫폼별 잠재 의존성으로 거절한다. 존재하는 절대 라이브러리 경로의
심볼릭 링크도 실제 파일까지 해석한다. 실제 LVRS가 아닌 `LVRSCompatible`과
이식 자산의 라이선스 고지는 금지하지 않는다.

`Congregation.native_dependency_guard`는 독립적인 CMake fixture로 금지된 직접·전이
대상, 별칭, 경로, 링크 옵션, 조건식과 정상 이름을 각각 검증한다. 이 검사는 구성 시
관찰할 수 있는 CMake 연결과 표현식에 명시된 대상 이름을 대상으로 한다. 외부
아카이브에 숨은 모든 정적 코드의 출처나 사용자 지정 빌드 스크립트가 실행 중에
만드는 임의의 연결까지 증명하지는 않는다. 실행 결과는
[네이티브 검증 기록](NATIVE_VERIFICATION.md)에 따로 기록한다.

macOS 번들 수집기, 설치 패키지의 원본 번들 검사와 실제 로드 이미지 검사는 공통
이름 정책을 사용한다. Qt와 LVRS 본체뿐 아니라 `LVRSCore`, 네이티브 라이브러리와
플러그인 이름도 거절하고, 독립적인 `LVRSCompatible` 같은 유사 이름은 허용한다.
심볼릭 링크는 실제 파일까지 해석해 검사하며, 설치 원본에서 번들 밖을 가리키는
라이브러리 링크는 거절한다. 이 정책은 검사한 파일·설치 이름에 대한 경계이며 임의
이름으로 숨긴 정적 코드의 출처를 식별하는 기능은 아니다.


웹 계정 변경부터 `build.sh`는 전역 CPATH·CPLUS_INCLUDE_PATH·CMAKE 경로와 dyld 라이브러리 덮어쓰기를 자식 빌드에서 제거한다. 선택한 키트의 새 헤더와 다른 설치본의 이전 라이브러리를 섞지 않기 위함이다. iPad 앱과 SDK 빌더도 같은 경로 오염을 제거한다. SDK를 직접 구성할 때도 이 환경 분리를 적용하고 macOS 배포 목표 12.0을 명시한다.

앱 대상은 선택된 `iiUpdateManagerNative`와 `iiAccountManagerNative` 패키지의 헤더를 다른 SDK 키트에 중복된 이전 헤더보다 먼저 탐색한다. 따라서 해당 패키지의 라이브러리와 같은 버전의 API로 컴파일한다.

강모 팁과 전체 획 테이퍼에는 iiPaintEngine의 `iiBrushTools` 패키지 0.1.2 이상이 필요하며, 현재 앱은 실시간 획 미리보기를 위해 iiSharedCanvas Native 0.35.38 이상을 요구한다. 선택한 iiSDK 키트에 BrushTools 구성요소와 일치하는 iiPaintEngine 라이브러리를 함께 설치한다. 개발 중에는 `CONGREGATION_PAINT_ENGINE_PREFIX`와 `CONGREGATION_SHARED_CANVAS_PREFIX`로 각각의 검증된 별도 설치본을 지정할 수 있다. macOS 12를 지원하는 앱에 연결할 SDK도 `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`으로 빌드해야 한다.

자석 올가미 경계 추적에는 iiSharedCanvas Native 0.35.18 이상이 필요하다.

비파괴 사각 자르기의 문서 재구성에는 iiSharedCanvas Native 0.35.19 이상이 필요하다.

원근 자르기에는 Qt 없는 iiGraphicAdjust 0.6.2 이상이 필요하다. 개발용 설치본은
`CONGREGATION_GRAPHIC_ADJUST_PREFIX`로 지정할 수 있다. SDK 설치본은
`share/licenses/iiGraphicAdjust`에 자체 MIT 및 Photocraft·Lightcraft 고지를 제공해야 하며,
macOS와 iPadOS 번들에 이 고지를 포함한다.

`Congregation.ui_style_tokens`는 UI 스타일에 지정된 하드코딩 색상과 크기를 검사한다.
그라디언트 정지점처럼 라벨에 표시하는 16진수 색상값은 사용자 데이터로 취급하며,
스타일 필드나 다른 표현의 하드코딩 색상은 계속 거부한다.


iPadOS 빌더 계약 테스트는 장치 연결이나 서명 없이 실행한다. 임시 입력과
패키지 픽스처는 `resources/`에 두고, CMake 시험 출력은 `build/`에 두며
테스트 종료 시 각각 정리한다.
