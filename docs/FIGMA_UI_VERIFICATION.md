# Figma ApplicationWindow 검증 · 2026-10-25

## 패널 색상 후속 보정

같은 날 원본 paint·변수와 최신 PNG를 다시 대조하여 패널 대비를 보정했다. 아래 14/14 기록은 최초 창 구현의 전체 검사이며, 색상 보정 후 관련 검사 **5/5**가 한 번의 실행에서 통과했다(314.26초). Metal·Vulkan의 실제 픽셀 검사와 두 렌더러의 전체 화면·조작·재생을 포함한다. 이후 색상 보정의 별도 결과는 `build/figma-colors/verification.json`과 `build/figma-color-tests.xml`을 따른다. 색상 검사 전에는 (100,100,100)의 과도한 경계선이 실제 GPU 회귀 검사에서 실패했다. 색상·합성·개별 경계선 소유권은 FIGMA_APPLICATION_WINDOW.md의 패널 색 항목에 기록했다. 후속 검사에서 화면 렌더링이 지연될 때 750ms의 고정 재생 판정이 실패하는 경우를 확인했다. 이 검사는 실제 프레임 이동을 확인할 때 완료하고 최대 15초까지만 기다리도록 바꾸었다. 재생 여부·문서 무변경 조건은 유지한다.

## 결과

arm64 macOS 그래픽 세션에서 최종 빌드와 단일 CTest 일괄 실행을 완료했다. 제품 검사 **14/14**가 통과했으며 총 시간은 **244.41초**이다. Main `83:6974`와 Tool 전체 항목 `130:1832`를 실제 소스에 반영했고, Figma 파일 자체는 수정하지 않았다.

정식 개발 산출물은 `build/Congregation.app`이다. 버전 **1.0.0 / 빌드 1**을 유지하며 Qt5·Qt6 탐색, AUTOMOC·AUTORCC·AUTOUIC를 사용하지 않는다. LVUI는 빌드 때 C++23으로 생성되고, iiSDKCore가 앱 수명·이벤트·프레임 루프를 소유한다. LVRS와 iiGUI가 네이티브 콘텐츠를 그린다.

## 실행 검사

| 검사 | 결과와 확인 범위 |
|---|---|
| native_build_surface | 활성 빌드 진입점의 Qt 획득·배포 경로 없음 |
| native_build_surface_contract | Qt 유입 거부와 이행 기록 구분 |
| engine | Core 앱 엔진·수명·비동기 계약 |
| services | 기존 네이티브 제품 서비스 |
| artwork | 브러시·취소·실행 취소·파일 입출력·리비전·레이어 불투명도·혼합·타이밍·행 순서·아핀 변형 |
| tool_settings | 22개 값·로그 축·증감·유효 범위·설정 왕복·엔진 상태 연결·시간 눈금 |
| editor_layout | 캔버스 맞춤·확대 금지·좌표·sRGB |
| native_installer_contract | 기존 패키징 도구의 회귀 계약; 새 PKG 생성 증거는 아님 |
| native_window_contract | 1999×1080·패널 폭·32px 슬라이더·증감·세그먼트·스크롤·Saved 필터·투명도·최소 창·위젯 세대 퇴역 |
| standalone_native | Metal에서 기존 펜·텍스트·레이어·저장·열기·팝업·단축키 회귀 검사 |
| figma_ui_native | Metal에서 새 Main 배치·드래그·전체 Tool 스크롤·우측 탭·시간·레이트·재생·이름 변경·레이어 이동 |
| standalone_vulkan | Vulkan/MoltenVK에서 기존 편집 기능 회귀 검사 |
| figma_ui_vulkan | Vulkan/MoltenVK에서 같은 새 Main 화면·입력·레이어·재생 검사 |
| bundle_relocation | 번들을 다른 경로로 이동하고 빈 작업 폴더에서 두 렌더러 실행·로드 이미지 감사·정식 경로 복원 |

문서 편집을 수반하지 않는 도구 설정·탭·스크롤·스크러빙·재생은 저장 데이터와 문서 리비전을 바꾸지 않는다. 프레임 레이트를 손잡이의 Alt 드래그로 바꾸면 한 번의 문서 편집을 기록한다. 레이어 이름·순서 변경도 실제 문서에 적용한 뒤 실행 취소로 원복함을 확인했다.

## 배치와 화면

두 렌더러에서 클라이언트 **1999×1080**, 상단 **28px**, 라이브러리 **224px**, 도구 레일 **54px**, 우측 **352px**, 타임라인 **296px**을 확인했다. 그리기 영역은 `(306,46,1341,692)`, 1536×1024 원본 그림의 표시 원점은 그 영역 안 `(235,66)`, 배율은 `0.548828125`이다. 이에 따라 원본 그림은 창 좌표 `(541,112)`에 843×562로 배치된다. 화면에 확대율은 54.9%로 표시한다.

원본 DPR 2 캡처는 다음 위치에 있다.

- `build/figma-application/native/editor.png`
- `build/figma-application/vulkan/editor.png`
- 각 렌더러 폴더의 `tool-dynamics.png`, `tool-taper.png`, `options-1.png`부터 `options-3.png`
- 각 폴더의 `layout.json`

Figma 기준 스크린샷은 비교용이며 앱에 포함하지 않는다. 콘텐츠 PNG·플레이헤드 SVG는 `resources/figma/sources.json`에 원본 파일의 SHA-256을 기록했다. 자동 픽셀 일치 검사가 아니라 실제 배치·입력 계약과 캡처 검토의 통과이다. 문서 데이터와 지원 상태를 표시하므로 그림의 8-bit 표기 및 비활성 도구 표시는 Figma의 정적 예시와 차이가 있다.

## 번들과 SDK

번들의 실행 코드 5개를 검사하여 Qt 동적 의존성 0개와 QML 파일 0개를 확인했다. 재배치 실행에서도 번들 밖 SDK·Homebrew 코드를 불러오지 않았고 정식 앱 경로가 복원되었다. ad-hoc 서명을 `codesign --verify --deep --strict`로 확인했다.

이번 화면에 필요한 최신 iiGUI·LVRS의 설치물을 전용 스테이징 키트에 반영했다. `native-kit-manifest.json`을 다시 고정하고 헤더·설정·라이브러리·도구·라이선스 **801개 파일**과 상대 링크의 일치를 확인했다. Qt 공개 헤더·Qt 패키지 탐색·키트 밖 심볼릭 링크는 이 검사에서 거부한다. 다른 작업에서 편집 중인 SDK 소스는 보존했으며 전체 SDK 테스트가 이번에 모두 재실행되었다고 주장하지 않는다.

기존 배포 서명 앱은 `build/figma-application/previous-signed-release.bundle`에 보존했다. 이번 앱은 개발 검증용이며 새 설치 패키지·공증·배포는 수행하지 않았다. 이전 `Congregation.pkg`에 이번 UI가 반영된 것으로 취급해서는 안 된다.

## 기록과 재실행

- 전체 검사: `build/figma-window-tests.xml`, `build/Testing/Temporary/LastTest.log`
- 화면·의존성·재배치 통합 기록: `build/figma-application/verification.json`
- Mach-O 의존성: `build/figma-application/bundle-audit.json`
- 실제 로드 이미지: `build/native-relocation-verification.json`, `build/congregation-relocated-native.log`, `build/congregation-relocated-vulkan.log`
- 키트: `build/figma-application/native-kit-check.log`

```sh
./build.sh local
ctest --test-dir build -R figma_ui --output-on-failure
```

Windows·Linux는 기존 합의대로 소스 구현 범위이다. macOS 실행을 이들 플랫폼이나 실제 펜 장치·원격 Society Storage·운영 계정의 검증으로 확대하지 않는다. 지원하지 않는 도구·End 테이퍼·세션 내 에셋 상태의 경계는 FIGMA_APPLICATION_WINDOW.md에 명시했다.

## Color·Metric 후속 전환

같은 날 [Color·Metric 공통 스타일 프로필](UI_STYLE_TOKENS.md)로 전환한 최종 빌드의 제품 전체 검사 **16/16**이 통과했다(279.31초). 메인 창·환경설정·새 캔버스·직접 그리는 패널이 동일한 값 소스를 공유한다. 타임라인 손잡이/세로선의 독립 픽셀 검사를 추가했으며 기존 배치·색상·조작 기준도 통과했다. 결과는 `build/ui-token-verification.json`과 `build/ui-token-tests.xml`에 기록했다.
