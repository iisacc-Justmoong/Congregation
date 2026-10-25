# New canvas 서브윈도우

Congregation의 새 캔버스 창은 Dreamscapes의 `NewCanvasDialog.qml`과
`CanvasSizePreview.qml`을 기준으로 내용·레이아웃·입력 동작을 공유한다.
Dreamscapes는 편집 화면 위의 모달 팝업이며 Congregation은
`ii::gui::Host::show(parent, callbacks)`로 여는 비모달 네이티브 서브윈도우이다.
메인 문서에는 어두운 덮개를 올리지 않으며 서브윈도우를 연 상태에서도 편집할 수 있다.
이미 열린 창에서 New 명령을 다시 실행하면 같은 창을 앞으로 가져온다.

## 구성과 크기

초기 클라이언트 영역은 1280 × 880이며 최소 크기는 320 × 480이다.
바깥 여백은 12, 테두리는 1이다. 배경 `#202020`, 상·하단과 카드·상세 패널
`#242424`, 창 테두리 `#5A5D63`, 선택한 카테고리 `#283653`, 미선택 미리보기
`#E8E8E8`을 공유 스타일 프로필의 `Color.canvasChooser*`로 정의한다.
크기와 간격은 `Metric`을 사용하며 Pretendard와 LVRS 위젯을 사용한다.

- 너비 1000 이상: 높이 96인 헤더, 200 너비의 카테고리 목록, 최대 3열 카드 갤러리,
  268 너비의 상세 패널, 높이 68인 푸터를 배치한다. 본문 여백과 열 간격은 24이다.
- 너비 1000 미만: 헤더 높이 132, 푸터 높이 94, 본문 여백 12로 전환한다.
  카테고리는 헤더의 메뉴에서 선택하며 갤러리와 상세 설정을 하나의 본문에서 스크롤한다.
- 카드 높이는 144, 미리보기 높이는 62, 정적인 이름·치수 영역은 44이다.
  카드 열 수는 `clamp(floor((galleryWidth + 12) / 220), 1, 3)`이다.
- 상세 미리보기 높이는 124, 사용자 지정 미리보기 높이는 일반 창에서 320,
  좁은 창에서 180이다. 직사각형은 픽셀 종횡비를 유지한다.

검색·Custom size·Close, 카테고리 목록, 카드, Canvas details,
선택 요약·배경 요약·Cancel·Create canvas의 문구와 순서를 원본에 맞춘다.
네이티브 창의 제목 표시줄과 운영체제 창 조작은 Congregation의 서브윈도우 방식에 따른다.

## 프리셋과 생성 규칙

`resources/canvas-presets.json`은 Dreamscapes revision 3의 원본과 바이트 단위로 같은
14개 카테고리·289개 프리셋 스냅샷이다. 출처·해시는
`resources/canvas-presets-source.json`에 기록한다. 빌드 때 C++ 상수로 포함하므로
실행 시 Dreamscapes 설치본이나 소스 경로를 참조하지 않는다.
데이터는 274개의 starter 항목과 15개의 출처가 있는 documented-recommendation 항목을 유지한다.

```sh
python3 -B tools/sync_canvas_presets.py --check
# Dreamscapes 원본을 검토하고 갱신하는 경우
python3 -B tools/sync_canvas_presets.py
```

검색은 모든 카테고리를 대상으로 플랫폼·용도·크기·단위를 찾는다.
`1920 × 1080`과 `1920x1080`을 같은 치수 검색으로 처리하며 독립된 검색어 `X`는
해당 플랫폼으로 한정한다. 검색 결과나 카테고리를 바꾸면 첫 프리셋을 선택한다.
결과가 없으면 생성 버튼을 비활성화하고 Clear search를 제공한다.

사용자 지정은 1800 × 1200 px, 300 PPI로 시작하며 1:1, 16:9, 9:16 단축 크기를 제공한다.
단위는 px·mm·in이다. 단위 변경은 현재 물리적 크기를 변환하며 인쇄 단위에서 PPI와
변환된 픽셀 치수를 표시한다. px는 정수만 허용한다. 각 축은 최대 32768 px,
전체 면적은 268435456 px이며 PPI는 양수이면서 2400 이하이다.
인쇄 단위에서 정확히 반 픽셀이 발생하면 짝수 쪽으로 반올림하여 원본과 같은 값을 낸다.

배경은 White·Transparent·Black이다. White와 Black은 캔버스 전체의 벡터 배경 레이어,
그 위에는 투명한 Paint layer를 만든다. Transparent는 Paint layer만 만든다.
문서 크기와 배경은 IISC 저장·재열기 뒤에도 유지한다. 프리셋 이름·분류·물리 단위·PPI는
Dreamscapes와 같이 생성 세션의 정보이며 기존 IISC 파일 형식에 새 메타데이터를 추가하지 않는다.

Cancel·Close·Escape·창 닫기는 작업 중인 문서를 교체하지 않는다.
검증에 실패하거나 생성에 실패하면 입력 창에 오류를 표시하고 기존 문서를 유지한다.
생성에 성공하면 편집 대상과 문서 제목을 갱신하고 서브윈도우를 닫는다.
Find 단축키는 검색창에 초점을 주고 Control/Command + Return은 유효한 문서를 생성한다.
카드는 키보드로 선택할 수 있고 화면 밖의 카드에 초점을 주면 해당 항목을 스크롤해 보여 준다.

## 구조와 검증

`CanvasCatalogue`는 불변 데이터·검색·변환·검증을, `CanvasSetupModel`은 선택과 입력 상태를,
`CanvasSetupWindow`는 LVRS·iiGUI 컨트롤 배치와 입력 전달을 맡는다.
`CanvasDefinition`은 편집 정책에 전달하는 데이터 계약이다.
기존 `EditorBackend`의 별도 크기 입력 모델은 제거해 중복된 검증을 없앴다.
LVUI는 창 옵션과 모델 수명을 빌드 때 생성하며 Qt·QML 실행기를 포함하지 않는다.

갤러리의 배경·선택선·종횡비 미리보기는 보이는 영역에 한해 하나의 그리기 목록으로 묶는다.
각 카드는 별도의 네이티브 동작·접근성·키보드 포커스를 유지한다.
카테고리·갤러리 변경은 해당 본문만 교체하며 검색 입력과 상세 입력의 초점은 보존한다.

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'canvas_|artwork|ui_style_tokens' --output-on-failure
ctest --test-dir build --output-on-failure
```

`Congregation.canvas_reference`는 원본 데이터와 디자인 치수의 추적성을,
`canvas_setup`은 289개 항목·전역 검색·단위 변환·경계값을 검사한다.
`canvas_window_native`와 `canvas_window_vulkan`은 실제 부모·자식 창에서 레이아웃,
입력·포커스·생성 제한·리사이즈·스크롤·독립 닫기를 검사하고
`build/new-canvas/{native,vulkan}/`에 화면을 저장한다.
`artwork`는 배경 렌더링·저장·재열기·실패 시 문서 보존을 검사한다.
전체 standalone·bundle relocation 검사는 실제 앱에서 같은 생성 흐름과
Qt 없는 배포 번들의 독립 실행을 검사한다.
