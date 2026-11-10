# Congregation 네이티브 전환

실행 진입점과 이벤트 루프는 iiSDKCore Application이 소유한다. LVUI의 직접 선언한
View 트리를 빌드 때 C++로 컴파일하며 LVUI 소유 위젯을 iiGUI의 네이티브 창과 GPU에
표시한다. LVRS, 브라우저, 웹뷰, Qt, QML 실행기와 JavaScript 런타임은 링크하지 않는다.
기존 Qt 화면 소스는 이전 작업 보존을 위해 남아 있지만 새 빌드 그래프에 포함하지 않는다.

Artwork는 Congregation의 편집 정책을 담당한다. 문서 저장과 래스터/벡터 처리 구현은
iiSharedCanvas 및 iiPaintEngine에 남는다. 작업 중인 파일은 별도의 복구 디렉터리에
저장하고 저장 명령은 정규 IISC 스냅샷을 FileProvider로 원자적으로 발행한다. 열기 실패는
기존 문서를 덮어쓰지 않는다. 펜 샘플은 스트로크를 처리한 후 픽셀로 확정하며 저장하지 않는다.

레이어 선택과 화면 확대·이동은 문서 리비전을 올리지 않는다. 스트로크 취소는 마지막
확정 상태로 복구한다. 실행 취소는 최대 32개 명령과 합계 64 MiB의 스냅샷 예산을 사용한다.
단일 명령이 예산을 넘으면 이전 기록을 비워 불연속적인 실행 취소로 최신 작업을 잃지 않게 한다.
벡터 도형은 벡터 자산으로 보존하고 PSD 출력에는 PDF Smart Object로 포함한다.

빌드 산출물은 저장소의 `build/` 안에 둔다. 이전 Qt 앱과 변경 전 빌드 설정은
`build/native-migration-backup/`에 보존하며 새 앱은 `build/Congregation.app` 한 개이다.
Windows와 Linux는 소스 구현 범위이며 현재 Mac의 결과로 두 플랫폼의 실제 실행을
검증한 것으로 간주하지 않는다.

## 현재 Main 화면

현재 기본 화면은 Figma의 `83:6974`와 Tool 풀버전 `130:1832`를 기준으로 한다. 세부 구성과 입력 계약은 [FIGMA_APPLICATION_WINDOW.md](FIGMA_APPLICATION_WINDOW.md)에 있다. LVUI ApplicationWindow의 옵션·활성화·팝업과 같은 스코프에서 LVUI·iiGUI C++ 위젯을 구성한다.

Vincent 복원 화면의 과거 구성·검증 기록은 VINCENT_UI.md에 보존한다. 현재 기본 레이아웃의 통과 증거로 사용하지 않는다.

타임라인의 미구현 버튼은 눌림·해제의 시각적 반응만 제공한다. 레이어 잠금,
그룹·마스크·클리핑·연결, 스냅, 어니언 스킨과 미구현 키프레임 버튼은 아직 문서나
레이어의 상태를 바꾸지 않는다. 공통 버튼 어댑터는 연결된 동작이 없는 경우 빈 동작을
호출하므로 콜백이 없어도 입력을 정상 처리한다. 기존에 기능이 연결된 버튼의 동작과
실제 사용 조건에 따라 비활성화되는 제어는 그대로 유지한다.

`Congregation.independent_workspace`는 레이어 잠금을 포함한 이 버튼들의 포인터 반복
입력과 프로그램 활성화에서 눌림·해제 및 모델 상태 불변을 확인한다.

## New canvas

새 캔버스 창은 Dreamscapes의 14개 카테고리·289개 프리셋과 상세 설정을 동일하게 제공하는 비모달 네이티브 서브윈도우이다. 구성·데이터 동기화·생성 규칙과 검증 방법은 [NEW_CANVAS_WINDOW.md](NEW_CANVAS_WINDOW.md)에 있다.

## 현재 프레임의 캔버스 입력

Fit은 문서가 표시 영역보다 작을 때도 비율을 유지하며 확대한다. 여백을 유지하고
일반 확대 조작의 최대 배율을 넘지 않으며, 창 크기가 바뀌면 Fit 상태를 다시 계산한다.

선택·스포이트와 채우기 영역 판정은 표시 중인 타임라인 프레임을 샘플링한다. 정적 비트맵 레이어에
그릴 때는 해당 프레임에서 평가한 레이어·아트보드 변환의 역변환을 획의 시작부터
종료까지 사용한다. 프레임이 바뀌면 진행 중인 콘텐츠 제스처를 취소하고 미리보기
캐시를 비운다. 선택이나 취소는 파일 리비전을 바꾸지 않는다. 유효하지 않거나 퇴화한
변환은 문서 편집 전에 오류로 거부한다.

`Congregation.canvas_frame_*` 검사는 창을 열지 않고 정적 비트맵과 모션이 포함된
레이어의 그리기·스포이트·선택·채우기·프레임 전환 취소를 확인한다. 채우기는 표시
프레임에서 영역을 찾지만 결과는 정적 비트맵 레이어에 저장한다. 이 범위는 동적
프레임의 자산 편집이나 물리 펜 입력 전체의 검증이 아니다.

## macOS 글로벌 메뉴

macOS가 제공하는 기본 앱 메뉴를 그대로 사용하며 앱 이름의 메뉴를 별도로 추가하지
않는다. 그 뒤의 메뉴 순서는 File, Edit, Select, View, Layer, Image, Filter,
Window, Help이다. 기존 Preferences 명령은 Edit 메뉴에 있고 종료는 macOS 기본 앱
메뉴에서 처리한다. Filter와 Help에는 아직 연결된 기능이 없어 비활성 안내 항목을
표시한다. `Congregation.app_menu` 검사는 메뉴 순서와 명령 연결을 확인한다.

## 우측 패널 세그먼트

Figma `84:9464`의 LVUI 아이콘 세그먼트로 Tools, Generative, Document,
Asset Library를 이 순서로 선택한다. 선택 상태는 작업 영역 뷰모델의 `option_tab`과
동기화된다. Document에 기존 문서·보기 설정을 유지한다. Asset Library는 Society가
등록한 저장소와 컨테이너 식별자를 확인하고 매니페스트의 asset-library 섹션을 읽는다.
명시적으로 선택한 로컬 폴더가 있으면 그 폴더를 사용한다. 하위 디렉터리를 포함해 최대
512개 파일을 이름순으로 표시하며, 패널 진입과 Refresh에서 다시 읽는다. PNG·JPEG·WebP
항목은 선택하면 이미지 레이어로 삽입한다. 다른 형식은 목록에 표시하되 이미지로 삽입할
수 없다는 상태를 알린다. 연결되지 않았거나 빈 라이브러리는 각각 안내한다. Society
라이브러리 열기와 폴더 연결도 제공한다. Generative는 연결된 생성 기능이 없어 안내만
표시한다. `Congregation.society_assets`는 등록 정보와 파일 목록을 검사하고,
`Congregation.inspector_segments`는 아이콘 자산과 순서를 확인한다.

## Transfer와 Smoothing 제어

Transfer의 Flow·Wetness·Mix는 각각 Jitter, Control과 Minimum을 브러시 엔진의
동역학 변형에 전달한다. Flow를 필압으로 제어할 때는 이전의 별도 필압 Flow 계수를
중복 적용하지 않는다. Wetness·Mix의 가시적인 효과는 믹서나 스머지 등 해당 물리
모델이 활성화된 도구에서 나타난다. 불투명도는 별도의 필압 응답과 최소·최대 값을
계속 사용한다.

Smoothing의 활성화, Exponential/Pulled String 방식 및 획 끝 따라잡기는 독립적으로
설정한다. Strength와 Radius는 기존 Brush 및 Smoothing 값을 공유한다. 추가 파라미터는
기존 설정 파일을 읽을 때 기본값으로 초기화하며, `Congregation.tool_settings`가
필압에 따른 Transfer 계산과 Smoothing 구성 전달을 검사한다.

## 브러시 질감과 입자 효과

Tool 패널의 Texture 선택 버튼은 이미지 파일을 읽어 밝기와 알파로 브러시 마스크를
구성한다. Invert, Scale, Brightness, Contrast, Texture Each Tip, Mode, Depth,
Minimum Depth, Depth Jitter와 Control은 엔진 텍스처 설정에 전달한다. 선택한 텍스처와
배경색은 현재 실행 세션에 보관한다. Color Dynamics의 `Use current color`로 현재 색을
배경색으로 지정한 뒤 전경색을 바꾸고 Foreground/Background Jitter 및 Control을
사용한다. Noise의 Grain Size는 문서 픽셀 단위이다. Wet Edges의 폭은 문서 픽셀,
Diffusion은 백분율이다. Build-up은 활성화, 누적률, 불투명도 상한, 시간 기반 분사를
독립적으로 설정한다. 시간 기반 분사는 획 입력과 기존 시계 진행 경로를 사용한다.

Tool 패널은 주요 고급 섹션을 Figma 순서로 배치하고 작은 슬라이더를 사용하며,
Brush 섹션에 절대 간격과 필압 곡선 편집을 제공한다. `Congregation.tool_settings`와
`Congregation.brush_tools_integration`은 설정 전달과 실제 픽셀 변화를 검사한다.
