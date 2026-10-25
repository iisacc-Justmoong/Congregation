# Congregation UI의 Color·Metric

Congregation의 활성 UI는 LVUI 기본 LVRS 토큰과 앱 전용 `App/Native/views/Theme.lvui`를 함께 사용한다. 색상은 Color, 간격·크기·테두리·글자 크기·굵기는 Metric으로 선택한다. Figma의 현재 배치와 색상을 유지하며, 기본 토큰이 없는 앱 고유 값만 프로필에 선언한다.

## 변경 위치

```text
// 앱 전용 프로필의 예
let window: Color = Color("#141719")
let inspectorWidth: Metric = 352
let panelEdgeResolved: Color = composite(.material.surface.dense.edge, .window)
```

`Color.window` 변경은 해당 앱의 기본 appearance에도 적용된다. 패널 경계선은 LVRS의 Dense/Edge 흰색 12%를 앱의 window 위에 encoded sRGB로 합성한 값이다. 실제 렌더링에서는 그 결과를 선형 색으로 변환한다. 원본 Figma 알파의 소수 정밀도를 보존하며, 경계선 두께는 `Metric.stroke1`이다.

| 대상 | 선택 방법 |
|---|---|
| LVUI 색상 | `color={Color.text.body}`, `surfaceColor={Color.popup}` |
| LVUI 수치 | `height={Metric.size22}`, `style="gap:Metric.space8"` |
| C++ 색상 | `Color::textBody`, `Color::panelEdgeResolved` |
| C++ 수치 | `Metric::size22`, `Metric::inspectorWidth` |
| 앱 창 배치 | `Metric.windowWidth/windowHeight`, `toolbarHeight`, `sidebarWidth`, `toolRailWidth`, `inspectorWidth`, `timelineHeight` |

기본 Color/Metric 목록은 LVUI의 `docs/StyleTokenMembers.md`를 따른다. C++에서는 점으로 구분한 멤버를 lowerCamelCase로 합친다. 예를 들어 `Color.material.surface.dense.edge`는 `Color::materialSurfaceDenseEdge`이다. 앱에 필요한 이름을 프로필에 추가하면 두 언어에서 같은 이름으로 사용할 수 있다. 프로필 내 수치를 바꿀 때 화면의 고정 기준을 검증하는 독립 테스트의 예상값도 함께 검토한다.

타임라인 손잡이는 원본 SVG의 모양·알파 마스크를 사용하되 최종 RGBA는 `Color.playhead`로 지정한다. 따라서 손잡이 이미지와 본문의 세로선 색상도 프로필 하나를 따른다. 원본 SVG paint는 실행 화면의 색상 결정에 사용하지 않는다.

## 생성과 의존성

CMake가 세 LVUI 뷰(Main, Preferences, CanvasSetup)에 동일한 `STYLE_FILE`을 전달한다. `lvui_generate_styles`는 같은 프로필을 `build/CongregationStyle.generated.h`로 생성한다. C++ 코드는 `App/Native/Style.h`를 통해 그 헤더의 Color·Metric을 사용한다. `CongregationStyle` 대상이 헤더 생성 순서와 include 경로를 전달하므로 병렬 빌드에서도 먼저 생성된다.

프로필을 수정하면 세 뷰와 C++ 상수를 다시 생성한다. 잘못된 타입·정의·색상 범위·이름 충돌은 빌드 오류이며 이전 생성 결과를 덮어쓰지 않는다. 생성 헤더를 직접 편집하지 않는다. 생성된 앱은 네이티브 C++ 상수와 iiGUI만 사용하고 LVUI 스타일 파일·파서·카탈로그를 실행 중에 읽지 않는다.

대상은 메인 창·활성화·메뉴·팝오버·환경설정·새 캔버스 화면과 네이티브 에셋 라이브러리·도구 패널·타임라인·캔버스 안내선·아바타이다. 문서 픽셀·브러시 색/크기·파라미터 범위·영상 시간/프레임·배율 계산·색공간 변환 계수는 문서 또는 알고리즘의 값으로 유지한다. `0`이 자동 크기나 좌표 원점인 경우도 수치 모델의 의미를 유지한다. 테스트의 고정 색상·배치 예상값은 구현 토큰과 독립적으로 두어 시각 회귀를 감지한다.

## 재현

LVUI SDK에서 빌드와 테스트를 완료하고 앱의 네이티브 kit에 설치한 후 manifest를 갱신한다. 활성 kit는 `SDK/build/congregation-native/prefix`이며 `~/.local`의 다른 SDK 패키지와 혼합하지 않는다.

```sh
./build.sh local --configure-only
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

`Congregation.ui_style_tokens`는 LVUI 속성/인라인 스타일, 네이티브 스타일 필드·빌더 인자·직접 그리기의 리터럴 재유입과 공통 프로필 연결을 검사한다. 검사기의 거부/허용 사례도 함께 실행한다. `editor_layout`은 독립된 좌표·색공간 기준을 유지하고, `native_window_contract`와 `native_window_colors_vulkan`은 실제 GPU 픽셀·폭·입력 높이·스크롤·파라미터 조작을 검사한다. 두 `figma_ui` 실행과 bundle relocation은 실제 앱과 이동된 단일 번들의 실행을 검증한다.


## 2026-10-25 Color·Metric 전환 시점의 검증

다음 수치와 보고서는 New canvas 서브윈도우 확장 전의 전환 스냅샷이다. 이후 추가한 `canvasChooser*` 토큰과 새 창의 검증은 [NEW_CANVAS_WINDOW.md](NEW_CANVAS_WINDOW.md)를 따른다.

macOS arm64 Release 빌드와 제품 전체 CTest **16/16**이 통과했다(279.31초). LVUI SDK의 전체 CTest도 **61/61**이 통과했다(30.95초). UTF-8 파일 읽기를 명시한 스타일 검사도 마지막에 별도 재실행하여 통과했다. 새 스타일 검사기 내부 사례는 5개이다.

활성 UI 소스 17개에서 Color·Metric을 691회 참조한다. 앱 프로필은 Color 20개·Metric 120개를 정의하며 기존 LVRS 기본 토큰을 함께 사용한다. 문서 엔진·Artwork·Shapes·ToolSettings·TimelineModel 등 29개 데이터 소스의 SHA-256은 작업 시작 시점과 같다.

Metal·Vulkan에서 창/패널 크기, 경계선·배경·타임라인 손잡이와 세로선의 픽셀, 22개 도구 파라미터, 드래그·스크롤·시간 이동·프레임 레이트 조절·재생·문서 리비전 무변경을 검증했다. 원래 경로를 벗어난 동일 번들도 두 렌더러에서 실행되었고 외부 SDK 라이브러리 없이 종료했다. 번들은 정식 개발 경로로 복원되었다.

`build/` 안의 `.app`은 `Congregation.app` 하나이며 버전 1.0.0 / 빌드 1이다. ad-hoc 서명 검증, Qt 동적 링크 부재, LVUI 런타임 라이브러리 심볼 부재를 확인했다. 네이티브 kit manifest는 설치 파일 802개의 해시 검사를 통과했다.

검증 산출물은 `build/ui-token-verification.json`, `build/ui-token-source-report.json`, `build/ui-token-tests.xml`, `build/ui-token-final-build.log`, `build/native-relocation-verification.json`이다. 실제 화면은 `build/ui-token-editor-metal.png`와 `build/ui-token-editor-vulkan.png`에 보관했다. SDK의 공통 프로필 구현·범위·검증은 `SDK/LVUI/docs/StyleProfiles.md`를 따른다.
