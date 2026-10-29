# Congregation 네이티브 빌드와 검증

현재 진입점은 `build.sh`와 `build-windows.ps1`이다. CMake는 `App/Engine`, `App/Native`와 LVUI에서 생성한 C++23을 빌드한다. Qt5·Qt6 탐색과 Qt 코드 생성은 끈다. 실행 중에는 LVUI 컴파일러, 브라우저, WebView, QML 엔진이 필요하지 않다.

iPadOS의 SDK 교차 빌드·Xcode 개발 서명·연결 기기 설치와 실행은 [IPADOS.md](IPADOS.md)를 따른다. 활성 `.app`는 macOS와 iPadOS 모두 같은 `build/Congregation.app`을 사용한다.

## 설치 키트

웹 로그인 구현 이후 macOS 검증 키트는 `SDK/build/congregation-web-account/prefix`이다. 기존 `SDK/build/congregation-native/prefix`의 SDK/ABI 구성을 보존한 독립 키트에 iiAccountManager Native와 iiUpdateManager Native를 다시 빌드하여 설치한다. 새 키트가 있으면 기본으로 선택하며, 다른 환경에서는 iiSDKBuilder로 새 계정 API를 포함한 키트를 구성한다. CMake가 BrowserSession 지원 여부를 확인하고 오래된 계정 SDK를 거부한다. 개발 소스와 기존 `~/.local` 설치본을 혼합하지 않는다. `CONGREGATION_SDK_PREFIX`로 다른 네이티브 키트를 선택할 수 있으며 CMake는 해당 키트의 각 패키지 디렉터리를 캐시에 다시 고정한다. LVRS는 iiGUI 공개 헤더의 ABI 식별자를 검사하므로 같은 버전 번호라도 서로 다른 위젯 구조를 사용한 설치본은 구성 단계에서 거부한다. 앱의 편집·화면 타깃에도 이 ABI 식별자를 컴파일 인수로 전달하여 식별자가 바뀌면 기존 목적 파일을 다시 빌드한다.

키트의 재구성 순서와 고정된 외부 의존성은 `../../SDK/iiSDKBuilder/docs/congregation-native.md`에 있다. 각 수정 SDK의 네이티브 설치 스크립트는 구성·빌드·검사·설치를 수행한다. iiNetwork는 `install.sh`, 나머지 수정 모듈은 `install-native.sh`를 사용한다. macOS 키트는 모든 실행 의존성을 최소 macOS 12.0으로 빌드한다. OpenSSL과 이미지 코덱은 키트 안의 정적 라이브러리이며 시스템 CURL·XML2를 사용한다. MoltenVK와 필요한 동적 SDK는 앱 번들 안에 수집한다.

## macOS

저장소 루트에서 실행한다.

```bash
./build.sh local
```

기본 호출 `./build.sh`도 같은 로컬 네이티브 빌드와 전체 검사를 수행한다. 빌드 결과는 `build/Congregation.app` 하나이다. 제품 버전은 1.0.0, 번들 빌드 번호는 1이다. 빌드 위치는 저장소의 `build/`로 제한한다.

```bash
CONGREGATION_SDK_PREFIX=/absolute/path/to/native/prefix ./build.sh local
./build.sh --configure-only
./build.sh --no-tests
ctest --test-dir build --output-on-failure
```

`--no-tests`는 반복 개발을 위한 옵션이며 완료 검증을 대신하지 않는다. 오래된 Qt 빌드 캐시에서 처음 전환할 때에는 기존 `build/CMakeCache.txt`와 `build/CMakeFiles`를 `build/native-migration-backup/` 아래에 보존한 뒤 다시 구성한다. 기존 앱과 사용자 파일을 삭제하거나 다른 위치에 활성 `.app` 복사본을 만들지 않는다.

빌드 후 `cmake/package_native_macos.py`가 실행 파일의 전체 동적 의존성을 따라가며 번들 안으로 수집한다. Qt, 선택 키트 밖의 비시스템 라이브러리, macOS 12.0보다 높은 최소 OS를 선언한 이미지를 거부한다. 절대 RPATH를 제거하고 번들 상대 경로로 연결하며 외부 라이선스 고지를 포함한 뒤 로컬 임시 서명을 검증한다. 이는 Developer ID 서명·공증·App Store 배포를 수행한 결과가 아니다.

직접 배포용 `Congregation.pkg`의 Developer ID 서명·설치 대상·페이로드 검증과 공증 절차는 [MACOS_INSTALLER.md](MACOS_INSTALLER.md)를 따른다. `CongregationInstaller` 타깃은 정식 앱에서 직접 패키지를 생성한다.

## 검사 계약

- `Congregation.engine`: Core 루프 소유·차용, 작업 수명, 읽기와 변경의 구분이다.
- `Congregation.services`: 저장 설정, 라이선스 정책, 네이티브 협업·오래된 리비전 거부와 서비스 경계이다.
- `Congregation.artwork`: 정규 IISC 저장·재열기, 레이어·벡터·이미지·텍스트·실행 취소, 이전 문서 이행, 무한 캔버스와 읽기 불변식이다.
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


웹 계정 변경부터 `build.sh`는 전역 CPATH·CPLUS_INCLUDE_PATH·CMAKE 경로와 dyld 라이브러리 덮어쓰기를 자식 빌드에서 제거한다. 선택한 키트의 새 헤더와 다른 설치본의 이전 라이브러리를 섞지 않기 위함이다. iPad 앱과 SDK 빌더도 같은 경로 오염을 제거한다. SDK를 직접 구성할 때도 이 환경 분리를 적용하고 macOS 배포 목표 12.0을 명시한다.
