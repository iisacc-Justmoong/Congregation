# Congregation Qt 독립 실행 검증

검증일은 2026-10-23이며 현재 arm64 macOS 그래픽 세션에서 수행했다. Windows·Linux는 사용자의 지시에 따라 소스 구현 범위이다. 최신 결과는 아래 Vincent UI 복원 후 최종 검증이다. 선행 Qt 독립화 단계의 재실행·로그는 `build/native-verification.json`에, 이번 UI 변경의 최종 결과는 `build/vincent-ui-verification.json`에 보존한다.

## 제품과 설치 키트

- 실행 소스: `App/Engine`, `App/Native`, HTML과 인라인 스타일의 `App/Native/views/*.lvui`이다.
- 정식 앱: `build/Congregation.app`이다. 버전 1.0.0과 번들 빌드 번호 1을 유지한다.
- 설치 SDK: `../../SDK/build/congregation-native/prefix`이다. 기존 사용자 설치본은 변경하지 않았다.
- `native-kit-manifest.json`은 헤더·설정·라이브러리·도구·라이선스 777개 파일과 상대 심볼릭 링크를 고정한다. Qt 공개 헤더·Qt 패키지 탐색·키트 밖의 링크를 거부한다. UI 계약 추가 후 키트를 다시 고정하고 `SDK/iiSDKBuilder/build/vincent-ui-kit-check.log`에서 일치를 확인했다.
- LVRS/iiGUI ABI 일치, 불일치, 식별자 누락을 구성 소비자 검사로 확인했다. 앱 편집·화면 목적 파일도 위젯 ABI 식별자를 컴파일 인수로 받아 SDK 교체 시 다시 빌드한다.

## Vincent UI 복원 후 최종 검증

`build/vincent-ui-final-build.log`의 최종 빌드가 완료됐으며, `build/vincent-ui-final-tests.log`의 단일 일괄 실행에서 제품 검사 **11/11**이 통과했다. 전체 시간은 151.56초이다. 원본 기준 배치와 팔레트, 실제 입력, 두 렌더러의 화면 캡처, 번들 재배치를 포함한다.

| 검사 | 최종 결과 |
| --- | --- |
| Congregation.native_build_surface | 통과 |
| Congregation.native_build_surface_contract | 통과 |
| Congregation.engine | 통과 |
| Congregation.services | 통과 |
| Congregation.artwork | 통과 |
| Congregation.editor_layout | 통과 · 맞춤 여백·배율·확대 금지·sRGB·압력 곡선 |
| Congregation.standalone_native | 통과 · Metal 실제 편집·저장·팝업·단축키 |
| Congregation.vincent_ui_native | 통과 · Metal 9개 화면·배치·압력 그래프 입력 |
| Congregation.standalone_vulkan | 통과 · Vulkan 실제 편집·저장·팝업·단축키 |
| Congregation.vincent_ui_vulkan | 통과 · Vulkan 9개 화면·배치·압력 그래프 입력 |
| Congregation.bundle_relocation | 통과 · 이동한 앱에서 두 렌더러 실행·동적 로드 감사·원위치 복원 |

이번 변경과 관련된 SDK 검사도 통과했다. 아래 수치는 전체 SDK의 완전한 기능 이식률을 뜻하지 않는다.

| 모듈 | 결과 | 로그 |
| --- | --- | --- |
| iiGUI | 21/21 | `SDK/iiGUI/build/vincent-ui-slider-tests.log` |
| LVUI | 40/40 | `SDK/LVUI/build/vincent-ui-source-final-tests.log` |
| LVRS | 9/9 | `SDK/LVRS/build/congregation/vincent-ui-final-scope-tests.log` |
| 설치 키트 | 777개 파일 일치 | `SDK/iiSDKBuilder/build/vincent-ui-kit-check.log` |

iiGUI는 원형 색상 버튼·동적 아이콘·18px 버튼·12px 슬라이더와 입력/수명 계약을, LVUI는 해당 속성의 타입 검사와 AOT 생성을 검증했다. LVRS는 목록 모델·외형·가상화·숨김 AppCard 그리기·색상 버튼·ABI·소스 경계·선언 대응표를 검사했다. SDK 로그의 경로는 Workspace 기준이다.

화면 비교는 저장소 루트 `design-qa.md`를 따른다. 원본과 같은 1400×858 클라이언트 크기로 맞추고 DPR 2 캡처를 정규화했다. 최신 18개 원본 캡처와 비교 이미지는 `build/vincent-ui/`에 보존한다. 기존 임시 Undo/Redo 버튼을 찾던 초기 실행 검사는 네이티브 단축키 입력으로 갱신했으며 최종 일괄 실행에서 모두 통과했다.

## Qt 독립화 단계의 선행 빌드·제품 검사

`./build.sh local`로 Qt5/Qt6 탐색을 비활성화하여 구성·빌드하고 실행 검사를 수행했다. 최종 재배치 검사 상태는 아래 확정 결과와 구조화된 기록을 따른다.

Qt 독립화 당시 제품 검사 8개 모두 통과했다. 엔진·서비스·편집·Metal·Vulkan은 `build/congregation-abi-rebuilt-tests.log`의 빌드에 대한 통과 기록이며, 경로 분류를 수정한 번들 재배치 검사는 `build/congregation-relocation-final-tests.log`에서 추가 통과했다. 재배치 실행에서 개발 SDK·Homebrew 로딩은 0건이며 정식 앱 경로의 복원을 확인했다. 이후 활성 빌드 진입점과 그 회귀 계약 검사 2개를 `build/congregation-build-surface-tests.log`에서 추가 통과했다.

| 검사 | 결과 |
| --- | --- |
| Congregation.native_build_surface | 통과 · 활성 CMake·빌드 스크립트·워크플로의 Qt 획득·배포 차단 |
| Congregation.native_build_surface_contract | 통과 · Qt 유입 거부·정상 비활성화·이행 기록 허용 |
| Congregation.engine | 통과 |
| Congregation.services | 통과 |
| Congregation.artwork | 통과 |
| Congregation.standalone_native | 통과 · Metal |
| Congregation.standalone_vulkan | 통과 · Vulkan/MoltenVK |
| Congregation.bundle_relocation | 통과 · 이동한 앱에서 두 렌더러 실행·동적 로드 감사·복원 |


엔진·서비스·편집 검사 외에 실제 네이티브 창에서 새 캔버스 입력 검증·생성, 압력 스트로크와 취소, 화면 이동·확대·임시 확대, 레이어 추가·이름·선택·복제·삭제·가시성, 실행 취소·재실행, 이미지 드롭, 다중행 한글 텍스트, 실제 ColorPicker 팝업과 읽기 전용 스포이트, 정규 IISC 저장·재열기, PNG·PSD 내보내기, 비동기 환경설정 창, 프로필 이미지·브러시 설정을 실행한다. 순수 조회·화면 이동·선택은 문서 리비전을 바꾸지 않음을 확인한다.

`tests/verify_standalone_bundle.py`는 정식 앱을 `build/` 안의 임시 경로로 이동하고 빈 작업 디렉터리에서 Metal과 Vulkan을 실행한다. 실행 중 실제 로드된 라이브러리 경로를 검사하고 성공·실패 모두 정식 경로로 복원한다. macOS Cryptex 시스템 볼륨의 OS 라이브러리도 시스템 경로로 구분하며 개발 SDK·Homebrew·임의 외부 경로는 허용하지 않는다.

## Qt 독립화 단계의 선행 SDK 검사

| 모듈 | 통과 기록 | 검증 범위 |
| --- | --- | --- |
| iiSDKCore | 58/58 | 작업 수명·관측 값·십진 변환·기반 경계 |
| iiGUI | 176/176 | 네이티브 입력·IME·보안/다중행 입력·OS 서비스·보조창·GPU 효과·폴백 |
| LVUI | 37/37 | HTML/인라인 구문·AOT·타입 바인딩·스코프 수명·목록·라우터 생성·예약어 조사 |
| LVRS Native | 117/117, 재실행 포함 | 제품 위젯·모델·팝업·설치 ABI·선언 대응표와 병행 추가된 표·라우터 |
| iiSharedCanvas Native | 20/20 | 문서·청크·타일·레이어·파일 형식·무한 영역의 읽기 불변식 |
| iiImageCodec | 3/3 | 형식·방향·ICC·프로필 썸네일·알파 경계 |
| iiNetwork | 32/32, 재실행 포함 | 네이티브 전송·HTTP/TLS·종료·OS 신뢰 저장소 |
| 계정·라이선스·업데이트 Native | 각 1/1 | 정규화·GraphQL·서명/플랫폼·버전/대기 계약 |
| iiSDKBuilder | 16/16, 재실행 포함 | JSON 숫자·명령·실제 SDK 빌드·리소스·설치 소비자·키트 고정 |
| iiFileProvider | 37/38 | 로컬·SQL·Native·잠금·원자적 저장·CoreIO 통과; 작업 공간 전체 감사는 아래 별도 기록 |

일부 초기 실행은 제한 시간을 초과했으며 단독 재실행에서 통과했다. LVRS의 Sheet 모션 검사도 일괄 실행에서 한 번 실패하고 단독 재실행에서 통과했다. 단일 일괄 실행의 전 항목이 항상 성공했다고 표현하지 않는다. 최종 앱의 기능 검사와 SDK 모듈별 통과 기록을 구분한다.

## 설치·의존성 증거

`build/native-bundle-dependencies.json`은 앱 실행 파일과 동적 라이브러리 총 5개의 폐쇄된 의존성, 상대 RPATH, 최소 macOS 12.0 선언을 기록한다. Qt·Homebrew·개발 소스의 동적 런타임 의존성은 없다. 5개 이미지의 undefined symbol 검사에서도 Qt 클래스·실행 엔진 참조가 없으며 로컬 임시 서명의 엄격한 검증을 통과했다. 공개 배포용 서명·공증·스토어 제출은 수행한 범위가 아니다.

위치를 바꾼 SDK 소비자는 Qt 탐색을 끈 상태에서 정규 IISC 생성·편집·재열기·픽셀 델타·타일 렌더링·PNG 왕복·PSD 내보내기/가져오기를 통과했다. `../../SDK/iiSharedCanvas/build/native-consumer/loaded-images-verification.json`은 실제 SDK 라이브러리가 원래 설치 위치가 아닌 `relocated-prefix`에서 로드되었음을 기록한다.

## 작업 범위 밖의 전체 감사

`iiFileProvider.workspace_creation`은 Congregation만 검사하는 테스트가 아니다. SDK와 Product 전체를 조사하여 다음 Dreamscapes 경로 2건을 보고했다. 읽을 수 없는 파일은 0건이며 이번 Congregation·SDK 이행에서 발생한 위반은 없다. 이 검사를 통과로 바꾸거나 다른 제품의 변경을 덮어쓰지 않았다.

- `Product/Dreamscapes/src/App/Generation/FailureReportStorage.h:14`
- `Product/Dreamscapes/src/App/Publishing/PublishReview.cpp:185`

원본 보고서는 `../../SDK/iiFileProvider/build/congregation-final-creation-audit.json`이다. 경로와 줄 번호는 조사 당시 소스 기준이다.

## 플랫폼과 남은 검증 경계

macOS는 Cocoa 창과 네이티브 입력, Metal 및 MoltenVK를 통한 강제 Vulkan의 실제 실행 결과이다. macOS의 Vulkan은 Metal 위의 구현이며 독립된 하드웨어 폴백을 뜻하지 않는다. Vulkan과 Direct3D11의 블러·그림자·변환 구현은 GPU 경로이며 Vulkan의 실제 픽셀 검사를 포함한다.

Windows의 Win32/Direct3D11·Vulkan, 펜·IME·클립보드·드롭·메뉴·대화상자와 Linux의 X11/XInput2/XIM·Vulkan·XDG portal/GIO 소스를 구현했다. 해당 OS에서 컴파일·실행한 증거는 없다. Linux OS 접근성 객체 노출에는 미구현 영역이 남아 있고 직접 Wayland 호스트는 없다. LVRS 전체 149개 선언을 완전히 이식한 결과도 아니다. 기준은 Congregation 자체 실행에 필요한 계약이다.

물리 펜 장비, macOS 12 실기기, 실운영 계정·라이선스·업데이트 서버, 공개 배포 설치자는 이번 로컬 자동 검사와 별개의 검증이다. 이전 Qt 소스는 기존 변경과 이행 비교를 위해 보존했으며 현재 구성·컴파일·링크·리소스·배포에는 포함하지 않는다.
