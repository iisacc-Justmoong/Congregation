# Congregation UI의 LVUI 토큰

타임라인의 두 슬라이더, 색상 불투명도와 브러시 팝오버의 일곱 슬라이더는 개별
손잡이·트랙 치수를 지정하지 않고 LVUI의 `SliderSize::mini` 또는 선언형
`size: .mini`를 사용한다. 크기와 포커스 표시는 LVUI가 소유하며 앱은 값 범위·
폭·접근성 이름 등을 전달한다. 브러시 팝오버는 LVUI `ScrollView`로 넘치는 압력
설정을 보존한다. 기준은 기존 LVUI 인스턴스로 구성한
[스크롤 뷰포트](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=281-4112)와
[전체 내용](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=281-4303)이다.
슬라이더의 트랙 색상도 제품이 지정하지 않고 원본과 일치하도록 수정한 LVUI의
`Panel/surfaceSolid` 기본값을 따른다. 브러시 팝오버의 흐림·방사형 악센트·
테두리·안쪽 광택·그림자는 LVUI의 공통 Popover 재질이 제공한다. 악센트는 원본의
여러 색상 정지점을 단일 강조색으로 근사한다. 불투명 폴백도 악센트와 안쪽 광택을
유지한다. 팝오버는 공통 창 강조색을 상속하고 경계 밖 악센트도 흐림 입력으로
제공한다. 실제 GPU 표시의 동등성은 확인하지 않았다.

Congregation의 활성 UI 색상과 컨트롤 표현은 LVUI가 소유하는 공유 토큰을 사용한다. LVUI 뷰는 `Color.surfaceSolid`, `Color.text.description`, `Color.blue.selected` 등을 직접 참조하고 네이티브 C++은 같은 생성 카탈로그의 `Color::surfaceSolid`, `Color::textDescription`, `Color::blueSelected` 등을 사용한다. 이 카탈로그는 LVRS/Figma에서 도입한 값을 포함한다. 앱 전용 Color 선언과 `Panel/Border` Figma 변수는 제거했다.

패널 경계는 두 제품과 Figma에서 공통 `Panel/canvas`이며 1 논리 픽셀이다. 타임라인의 눈금·플레이헤드·선택 영역은 각각 공통 텍스트 및 파랑 역할을 사용한다. 창과 팝오버·새 캔버스 창도 `Panel/window`, `Panel/surfaceSolid`를 사용한다. 타임라인 내부 선은 `Material/Surface/Dense/Edge`를 창 배경 위에 합성한 뒤 네이티브 렌더러에 전달한다. 메인 창 바탕은 악센트를 합성하지 않은 `Panel/window`다.

네이티브 컨트롤과 타임라인의 아이콘 이름은 LVUI 디자인 카탈로그에서 해석한다.
현재 Figma Main에서 캡처한 100종과 Avatar 기본 심벌은 해당 벡터를 사용하고, 나머지 이름은 LVUI가
소유한 이전 자산으로 폴백한다. 동일 이름의 이전 카탈로그는 호환용으로 유지한다.
사용되지 않던
Vincent 시절의 고정 색상 SVG와 이를 실행 파일에 포함하던 생성 코드는
제거했다. 공통 아이콘의 색상과 형태를 제품별 사본으로 고정하지 않는다.

`App/Native/views/Layout.lvui`는 창 크기와 작업 영역 비율 등을 포함한 제품별
`Metric` 선언 107개를 보유한다. 브러시 슬라이더를 공통 변형으로 바꾸며
더 이상 사용하지 않는 크기 선언 두 개를 제거했다. 두 열의 도구
레일은 별도 제품 크기 선언 없이 LVUI 아이콘 버튼의 공통 치수와 공통 간격으로
계산한다. 타임라인 레이어 행도 원본처럼 공통 아이콘 버튼의 고유 높이를 따른다.
키프레임은 원본과 같이 공통 18px 아이콘을 각 22px 행의 프레임 셀 중앙에 놓으며,
별도 5px 위치 토큰 대신 행·아이콘·셀의 크기에서 위치를 계산한다.
구분선 두께와 보조·본문 글자의 행 높이는 각각 LVUI의 스트로크 및 타이포그래피
토큰을 사용하며, 타임라인 손잡이의 중심은 손잡이 폭에서 계산한다.
타임라인과 확장 도구 패널의 전체 높이, 개별 파라미터 행의 높이는 원본의 자식
요소·공통 컨트롤 크기·여백에서 계산한다. 레이어 이름은 원본과 같은 본문 행
높이로 중앙 정렬한다. 둥근 표시의 반지름도 실제 표시 크기에서 계산한다.
전체 파라미터 높이는 현재 도구 사양의 그룹과 행에서 계산하며, 빠른 이동은
고정 좌표 대신 해당 파라미터의 현재 위치를 따른다.
도구와 색상 패널의 내부 폭은 Figma에서 확인한 패널 폭과 공통 여백으로,
팝오버와 Profile 입력 폭은 각 컨테이너 폭과 공통 안쪽 여백으로 계산한다.
도구 패널 하단 높이는 공통 아이콘 버튼·세그먼트의 고유 크기와 여백에서
계산한다. 이 관계로 중복된 제품 크기 선언 여섯 개를 제거했다.
압력 곡선의 안쪽 여백은 공통 `space6`을 사용하며, 격자 간격과 압력 좌표 범위는
뷰의 높이에서 안쪽 여백을 뺀 뒤 네 구간으로 나누어 계산한다. 이 관계로 별도
그래프 여백·높이·격자 간격 선언 세 개를 제거했다. 곡선 굵기는 Figma 기준과
같은 공통 `stroke2`를 사용한다. 메인과 설정 화면의 설명 글자는 동일한 크기와
색상을 반복 지정하지 않고 LVUI의 기본 Description 스타일을 따른다.
독립 아이콘의 기본 크기와 래스터 해상도는 LVUI 구성요소가 결정하며,
타임라인의 사용자 데이터 그리기도 같은 LVUI 아이콘 공급자를 사용한다.
아이콘형·레이블형 세그먼트의 기본 여백과 표면·테두리 색상도 제품 어댑터가
덮어쓰지 않고 각 LVUI 공통 구성을 사용한다.
환경설정 Profile의 [Figma 기준 화면](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=268-2635)은
LVUI Avatar와 둥근 TextField 인스턴스를 사용한다. 소스의 Profile 사진과 이름
입력도 공통 Avatar와 compact·rounded 입력 및 지우기 동작을 사용한다. 설정 탭은
공통 세그먼트의 바깥 여백과 테두리 없이 버튼에 맞춘 22px 배치를 따르며 `selectionStyle: .segmented`로
선택 항목의 기본 버튼 상태를 표현한다. 탭의 크기·여백·선택색은 제품에서
재정의하지 않는다. 다른 설정 화면의 개별 스타일, 창 전체의 시각 동등성과 실제
기기 표시는 아직 별도다.
CMake는 이 파일을 세 LVUI 뷰와
`lvui_generate_styles`에 전달한다. 현재 사용자 규칙은 앱 전용 스타일 토큰을
금지하므로, 이 선언들은 레이아웃 책임과 LVUI 공통 토큰으로 분리해야 하는 미완료
항목이다. 사용자 그림, 브러시 값, 선택한 문서 색상 및 미리보기 데이터는 디자인
토큰과 구분한다.

## 생성과 검증

C++용 상수는 `build/CongregationStyle.generated.h`로 생성하며 직접 편집하지 않는다. 이전 LVRS 호환 스냅샷과 생성기는 현재 빌드의 입력이 아니다. `Congregation.ui_style_tokens`는 앱 전용 Color 선언과 UI 리터럴의 재유입을 검사한다. LVUI는 필터가 필요한 기존 아이콘 한 종에 자체 대체 자산을 사용한다. 범용 SVG 필터의 지원을 뜻하지는 않는다. 사용자 문서 픽셀과 색상은 검사 대상 UI 팔레트가 아니다.

아래는 과거 전환 시점의 기록이며 현재 앱에 별도 Color 프로필이 있다는 의미가 아니다.

## 2026-10-25 Color·Metric 전환 시점의 검증

다음 수치와 보고서는 New canvas 서브윈도우 확장 전의 전환 스냅샷이다. 이후 추가한 `canvasChooser*` 토큰과 새 창의 검증은 [NEW_CANVAS_WINDOW.md](NEW_CANVAS_WINDOW.md)를 따른다.

macOS arm64 Release 빌드와 제품 전체 CTest **16/16**이 통과했다(279.31초). LVUI SDK의 전체 CTest도 **61/61**이 통과했다(30.95초). UTF-8 파일 읽기를 명시한 스타일 검사도 마지막에 별도 재실행하여 통과했다. 새 스타일 검사기 내부 사례는 5개이다.

활성 UI 소스 17개에서 Color·Metric을 691회 참조한다. 앱 프로필은 Color 20개·Metric 120개를 정의하며 기존 LVRS 기본 토큰을 함께 사용한다. 문서 엔진·Artwork·Shapes·ToolSettings·TimelineModel 등 29개 데이터 소스의 SHA-256은 작업 시작 시점과 같다.

Metal·Vulkan에서 창/패널 크기, 경계선·배경·타임라인 손잡이와 세로선의 픽셀, 22개 도구 파라미터, 드래그·스크롤·시간 이동·프레임 레이트 조절·재생·문서 리비전 무변경을 검증했다. 원래 경로를 벗어난 동일 번들도 두 렌더러에서 실행되었고 외부 SDK 라이브러리 없이 종료했다. 번들은 정식 개발 경로로 복원되었다.

`build/` 안의 `.app`은 `Congregation.app` 하나이며 버전 1.0.0 / 빌드 1이다. ad-hoc 서명 검증, Qt 동적 링크 부재, LVUI 런타임 라이브러리 심볼 부재를 확인했다. 네이티브 kit manifest는 설치 파일 802개의 해시 검사를 통과했다.

검증 산출물은 `build/ui-token-verification.json`, `build/ui-token-source-report.json`, `build/ui-token-tests.xml`, `build/ui-token-final-build.log`, `build/native-relocation-verification.json`이다. 실제 화면은 `build/ui-token-editor-metal.png`와 `build/ui-token-editor-vulkan.png`에 보관했다. SDK의 공통 프로필 구현·범위·검증은 `SDK/LVUI/docs/StyleProfiles.md`를 따른다.

## LVUI 단추 스타일 경계

제품의 단추 어댑터는 LVUI가 소유하는 기본 상태와 팔레트를 사용한다. 호스트의
`ButtonOptions`에 별도 스타일이 들어오면 무시하거나 다른 모양으로 변환하지 않고
지원하지 않는 입력으로 거절한다. 제품에서 단추 모양이 더 필요하면 먼저 LVUI
라이브러리에 해당 상태나 변형을 정의해야 한다.
