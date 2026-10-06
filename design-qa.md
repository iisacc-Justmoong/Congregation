<a id="design-qa-2px-white-color-picker-border"></a>

# 디자인 QA: 2px 흰색 색상 선택기 테두리

<a id="comparison-target"></a>

## 비교대상

- 소스 시각적 진실: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`, 현재 색상 원에 2px 흰색 테두리가 있어 검정색이 계속 표시되어야 한다는 사용자의 명시적인 요구 사항에 따라 개선되었습니다.
- 구성 요소: `CanvasToolBar.qml`의 맨 오른쪽에 있는 현재 색상의 `LV.IconButton`입니다.
- 필수 상태:
  - 최소 대비 가시성을 확인하기 위해 검은색 현재 색상, 선택기가 닫혀 있습니다.
  - 제공된 시각적 개체와 비교하기 위해 자홍색 현재 색상, 선택기가 닫혀 있습니다.
  - 기본 상호작용을 확인하기 위해 현재 색상이 검은색이고 선택기가 열려 있습니다.

<a id="evidence-and-normalization"></a>

## 증거 및 정규화

- 소스 영상:
  - 경로: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`
  - 픽셀 크기: 1x에서 57 x 83.
- 접두사 검정색 구현:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-borderless-full.png`
  - 논리 뷰포트: 1280 x 853.
  - 픽셀 크기: 2x Retina에서 2560 x 1706.
- 사후 수정 검정 구현:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-full.png`
  - 논리 뷰포트: 1280 x 853.
  - 픽셀 크기: 2x Retina에서 2560 x 1706.
- 사후 수정 마젠타색 구현:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-magenta-full.png`
  - 논리 뷰포트: 1280 x 853.
  - 픽셀 크기: 2x Retina에서 2560 x 1706.
- 선택기 개방형 구현:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-picker-open.png`
- 밀도 정규화 집중 캡처:
  - 이전 검정색: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-borderless-black-normalized.png`
  - 검정색 이후: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-normalized.png`
  - 마젠타색 이후: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-magenta-normalized.png`
  - 각 초점 캡처는 동일한 2x Retina 크롭에서 1x의 57 x 83로 다운샘플링되었습니다.
- 결합된 비교 입력:
  - 검정색 가시성, 왼쪽 앞과 오른쪽: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-black-before-vs-white-border-after.png`
  - 제공된 소스(왼쪽) 및 자홍색 구현(오른쪽): `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-source-vs-white-border-implementation.png`
  - 두 입력 모두 4x 검사 크기 조정 전에 동일한 57 x 83 자르기를 함께 배치합니다.

<a id="findings"></a>

## 조사 결과

- 실행 가능한 P0, P1 또는 P2 결과가 남아 있지 않습니다.
- 이제 검은색 현재 색상 채우기가 어두운 도구 모음에 대해 요청된 흰색 링으로 한계가 설정된로 표시됩니다.
- 컨트롤은 하나의 색상 원과 하나의 가시성 테두리를 유지합니다. 이전의 외부 단추 프레임과 장식 고리는 반환되지 않습니다.
- 흰색 테두리는 `LV.Theme.scaleMetric(2)`로 지정되며 확인된 데스크톱 프로필에서 정확히 2 논리 픽셀로 확인됩니다.

<a id="required-fidelity-surfaces"></a>

## 필수 충실도 표면

- 글꼴 및 타이포그래피: 통과; 이 구성 요소에는 텍스트 콘텐츠가 없으며 인접한 도구 모음 서체는 변경되지 않습니다.
- 간격 및 레이아웃 리듬: 통과; 스톡 22 x 22 LVRS 버튼 프레임과 도구 모음 정렬은 변경되지 않은 반면 테두리는 스톡 아이콘 슬롯 안쪽으로 그려집니다.
- 색상 및 시각적 토큰: 통과; 채우기는 `toolbar.currentColor`에 바인딩된 상태로 유지되며 가시성 테두리는 요청한 대로 `#ffffff`로 불투명합니다. 검정색과 마젠타색 상태가 모두 관찰되었습니다.
- 이미지 품질 및 자산 충실도: 통과; 동적 색상 표시기는 래스터 스케일링 아티팩트나 교체 아이콘 자산 없이 Retina의 원래 픽셀 밀도에서 앤티앨리어싱된 QML 제어 표면으로 유지됩니다.
- 사본 및 내용: 통과; `Brush color` 액세스 가능한 이름과 도구 설명은 변경되지 않았습니다.

<a id="interaction-and-accessibility"></a>

## 상호 작용 및 접근성

- 노출된 `Brush color` AX 버튼을 활성화하면 HSL 삼각형 선택기가 열립니다.
- HSL 선택은 2px 흰색 테두리를 유지하면서 도구 모음 표시기를 즉시 업데이트합니다.
- Escape는 선택기를 닫고 스톡 LVRS `Borderless` 호버, 누르기, 초점 및 대상 적중 동작은 계속 사용할 수 있습니다.

<a id="comparison-history"></a>

## 비교 이력

1. 이전의 마젠타색 전용 패스는 모든 윤곽선을 제거하고 집중된 비교를 통과했습니다.
2. 새로 요구되는 검은색 상태 비교에서는 P1 어포던스 문제가 드러났습니다. 테두리 없는 검은색 원이 어두운 도구 모음에서 거의 사라졌습니다.
3. 현재 색상의 원은 제거된 외부 프레임이나 장식 링을 복원하지 않고 내부 2px 불투명 흰색 테두리를 받았습니다.
4. post-fix 블랙 비교에서는 명확하게 보이는 원이 표시되고 post-fix 마젠타/소스 비교에서는 단순화된 단일 링 구조가 확인됩니다. P0/P1/P2 문제가 남아 있지 않습니다.

<a id="implementation-checklist"></a>

## 구현 체크리스트

- [x] 스톡 `LV.IconButton` 형상과 `Borderless` 톤을 유지합니다.
- [x] `toolbar.currentColor` 채우기를 유지합니다.
- [x] 색상환에만 2px `#ffffff` 테두리를 추가합니다.
- [x] 외부 프레임과 장식 링을 제거한 상태로 유지하세요.
- [x] 도구 설명, 접근성 이름 및 HSL 선택기 상호 작용을 유지합니다.
- [x] 실행 중인 1280 x 853 앱에서 검정색 상태와 검정색이 아닌 상태를 확인합니다.

<a id="toolbar-horizontal-padding-follow-up"></a>

## 툴바 수평 패딩 후속 조치

- 전체 너비 도구 모음 배경과 하단 구분 기호는 1280픽셀 창과 동일한 높이를 유지합니다.
- 도구 모음 콘텐츠 레이아웃은 `anchors.leftMargin` 및 `anchors.rightMargin` 모두에 대해 `LV.Theme.gap16`를 사용합니다. 재고 없음 LVRS 버튼 치수가 재정의되었습니다.
- 재구성된 실행 중인 앱에서 윈도우 프레임은 `100,80,1280,853` 였고, 22 x 22 `New canvas` 버튼 프레임은 `116,115` 에서 시작되었으며, 22 x 22 `Brush color` 버튼 프레임은 `1342,115` 에서 시작되었습니다. 결과적인 왼쪽과 오른쪽 가장자리 간격은 모두 정확히 16 논리 픽셀입니다.
- 폐쇄 상태 엣지 증거: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-toolbar-horizontal-padding-16-edges.png`.
- 개방형 상호작용 증거: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-toolbar-horizontal-padding-16-color-picker-open-crop.png`; 선택기는 오른쪽 삽입 후 창 내에서 표시되고 정렬된 상태로 유지됩니다.

최종 결과: 합격

<a id="design-qa-profile-avatar-icon-centering"></a>

# 디자인 QA: 프로필 아바타 아이콘 센터링

<a id="comparison-target-1"></a>

## 비교대상

- 소스 시각적 진실: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-0aecfb71-e282-4600-90d9-54c04cbda3fa.png`.
- 구성 요소: `PreferencesWindow.qml`의 빈 프로필 `LV.IconButton`.
- 필수 상태: 64 x 64 원형 프레임이 표시되도록 프로필 이미지 버튼을 가리킨 어두운 기본 설정 창.

<a id="evidence-and-normalization-1"></a>

## 증거 및 정규화

- 소스 영상:
  - 픽셀 크기: 1x에서 86 x 107.
  - 사용자 아이콘 중심은 제공된 자르기 내에서 x=51이고 원형 프레임 중심은 x=59입니다.
- 수정 후 전체 보기:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-after-hover.png`.
  - 논리적 뷰포트 및 픽셀 크기: 1x에서 480 x 360.
- 수정 후 초점 보기:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-after-hover-focused.png`.
  - 픽셀 크기: 1x에서 86 x 107; 밀도 정규화가 필요하지 않았습니다.
- 소스 왼쪽/구현 오른쪽 결합 비교:
  - 경로: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-source-vs-after.png`.
  - 4x 가장 가까운 이웃 검사: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-source-vs-after-4x.png`.

<a id="findings-1"></a>

## 조사 결과

- 접두사 구현에는 P2 수평 정렬 드리프트가 있었습니다. 스톡 사용자 아이콘은 원형 버튼 프레임 중앙 왼쪽에 있는 8 논리 픽셀이었습니다.
- 대칭형 10픽셀 콘텐츠 패딩은 이제 64 x 64 프레임 내에서 44 x 44 아이콘을 중앙에 배치합니다.
- 사후 수정 아이콘 중심과 원형 프레임 중심은 모두 집중 비교에서 x=59로 확인됩니다. 실행 가능한 P0, P1 또는 P2 결과가 남아 있지 않습니다.

<a id="required-fidelity-surfaces-1"></a>

## 필수 충실도 표면

- 글꼴 및 타이포그래피: 통과; 프로필 레이블과 텍스트 필드 타이포그래피는 변경되지 않았습니다.
- 간격 및 레이아웃 리듬: 통과; 64 x 64 프로필 프레임과 주변 기본 설정 레이아웃은 변경되지 않고 아이콘의 콘텐츠 삽입만 변경됩니다.
- 색상 및 시각적 토큰: 통과; 기존 LVRS 테마 색상, 호버 처리, 테두리 없는 버튼 톤은 변경되지 않습니다.
- 이미지 품질 및 자산 충실도: 통과; 재고 LVRS `user.svg`는 래스터 교체 또는 크기 조정 아티팩트 없이 44 x 44에서 계속 사용됩니다.
- 사본 및 내용: 통과; 보이는 섹션 사본은 변경되지 않은 반면, 프로필 버튼의 액세스 가능한 이름은 이제 옵션 메뉴 역할을 반영합니다.

<a id="interaction-and-accessibility-1"></a>

## 상호 작용 및 접근성

- 노출된 `Profile image options` AX 버튼을 활성화하면 LVRS 프로필 이미지 컨텍스트 메뉴가 열립니다.
- `Select profile image`는 다음 이벤트 루프 차례에 네이티브 `Choose profile image` 파일 대화 상자를 연다. Escape를 누르면 두 화면 중 어느 쪽이든 닫히고 Preferences로 포커스가 돌아간다.
- 프로필 버튼은 기본 LVRS 호버/포커스 처리 외부에 경계선 없이 유지됩니다.

<a id="comparison-history-1"></a>

## 비교 이력

1. 소스 검사에서는 사용자 아이콘과 원형 프레임 중심 사이에 8픽셀 왼쪽 이동이 설정되었습니다.
2. 유지된 LVRS 기본 2픽셀 가로 패딩은 60픽셀 콘텐츠 영역을 떠나 해당 영역의 앞쪽 가장자리에 44픽셀 아이콘을 배치했습니다.
3. 이제 구현에서는 `(avatarSize - iconSize) / 2`에서 10픽셀 대칭 삽입을 파생하여 크기와 스톡 자산을 변경하지 않고 유지합니다.
4. 다시 작성되고 마우스를 올린 컨트롤은 아이콘과 프레임 중심이 일치하며 파일 대화 상자 상호 작용은 그대로 유지됩니다.

최종 결과: 합격

<a id="design-qa-native-resolution-circular-profile-crop"></a>

# 디자인 QA: 원본 해상도 원형 프로필 자르기

<a id="comparison-target-2"></a>

## 비교대상

- 구성 요소: `PreferencesWindow.qml`에 있는 경계선 없는 프로필 `LV.IconButton`의 선택된 이미지 상태입니다.
- 입력: `/Volumes/Storage/Workspace/Product/Congregation/docs/marketing/congregation-windows-editor.png`.
- 필수 상태: 선택한 가로 이미지는 중앙에 있고, 1:1 정사각형으로 자르고, 원으로 마스크되고, 기존 64-DIP 버튼 형상을 변경하지 않고 표시되어야 합니다.

<a id="evidence-and-pixel-contract"></a>

## 증거 및 픽셀 계약

- 입력 크기: 1403 x 911 픽셀.
- 처리된 이미지 증거: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-center-crop-output-911.png`.
- 처리된 크기: 911 x 911 픽셀, 추가 크기 조정 없이 입력의 짧은 면과 일치합니다.
- 처리된 형식: 알파가 포함된 무손실 PNG; 투명한 모서리와 앤티앨리어싱된 원형 가장자리가 있습니다.
- 실행 환경 설정 증거: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-center-crop-live.jpeg`.

<a id="findings-2"></a>

## 조사 결과

- 이전 일반 LVRS 아이콘 경로는 `Image.PreserveAspectFit`를 사용하여 전체 사진을 아이콘 상자로 축소하고 자르지 않았습니다.
- `ProfileImageProcessor`는 이제 방향 메타데이터를 적용하고, 중심에 있는 가장 큰 소스 사각형을 선택하고, 이를 크기 조정 없이 동일한 크기의 출력으로 그리고 앤티앨리어싱된 원형 알파 클립을 적용합니다.
- 가로 및 세로 회귀 픽스처는 출력 쪽이 소스의 짧은 쪽과 동일하고 출력 중앙 픽셀이 해당 소스 중앙 픽셀에 매핑된다는 것을 증명합니다.
- 실행 가능한 P0, P1 또는 P2 결과가 남아 있지 않습니다.

<a id="interaction-and-accessibility-2"></a>

## 상호 작용 및 접근성

- `Profile image options`를 활성화한 다음 `Select profile image`를 활성화하면 네이티브 이미지 선택기가 열립니다.
- 1403 x 911 이미지를 선택하면 기본 설정으로 돌아가고 즉시 스톡 사용자 아이콘이 원형 미리보기로 대체됩니다.
- 버튼의 경계선 없는 톤, 64-DIP 프레임 및 중앙 44-DIP 콘텐츠 영역은 변경되지 않습니다. 이제 액세스 가능한 이름이 중간 옵션 메뉴를 설명합니다.
- 실패한 후속 이미지 디코딩은 이전의 유효한 자르기를 유지합니다.

최종 결과: 합격

<a id="design-qa-profile-image-context-menu"></a>

# 디자인 QA: 프로필 이미지 컨텍스트 메뉴

<a id="required-flow"></a>

## 필요한 흐름

- 프로필 이미지를 누르면 네이티브 파일 선택기를 바로 여는 대신 기본 제공 LVRS 컨텍스트 메뉴를 열어야 한다.
- 메뉴에는 항상 `Select profile image` 및 `Delete profile image`가 표시되어야 합니다.
- Select는 상황에 맞는 메뉴가 닫힌 후에만 네이티브 선택기를 열어야 합니다.
- 삭제하려면 미리보기를 지우고 임시 원형 PNG를 제거해야 합니다. 계속 표시되지만 이미지가 없으면 비활성화됩니다.

<a id="evidence"></a>

## 증거

- 등록된 이미지 메뉴: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-image-context-menu-final.jpeg`.
- 삭제된 이미지 상태: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-image-after-delete.jpeg`.
- 최종 실행 접근성 트리는 `Profile image options`, `Select profile image` 및 `Delete profile image`를 이름별로 노출했습니다. 빈 이미지 상태에서는 삭제가 비활성화되었으며 선택 후 활성화되었습니다.

<a id="findings-3"></a>

## 조사 결과

- 직접 `onClicked: profileImageDialog.open()` 경로가 제거되었습니다.
- `ContextMenu.openFor()`는 다른 프레임을 추가하지 않고 기존 테두리 없는 프로필 버튼에 팝업을 고정합니다.
- 기본 `LV.MenuItem` 대리자는 LVRS 스타일을 유지하면서 표시되는 각 레이블을 접근성 이름에 바인딩합니다.
- 선택 경로가 `Choose profile image`에 도달했습니다. 삭제 경로는 중앙에 있는 스톡 사용자 아이콘을 복원하고 `Congregation-profile-image-*.png` 임시 파일을 남기지 않았습니다.
- 실행 가능한 P0, P1 또는 P2 결과가 남아 있지 않습니다.

최종 결과: 합격

<a id="design-qa-presentation-laser-single-dot-and-smooth-trajectory-correction"></a>

# 디자인 QA: 프리젠테이션 레이저 단일 도트 및 부드러운 궤적 교정

<a id="evidence-and-normalization-2"></a>

## 증거 및 정규화

- 소스 시각적 진실: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-fcb57daa-3e04-4f94-b5f2-cb22826fba5b.png`와 함께 커서 위치는 정확히 하나의 점을 소유하고 궤적은 점을 소유하지 않는다는 명시적 요구 사항을 함께 적용합니다.
- 소스 크기: 1588 × 1026 px.
- 접두사 결정론적 재현: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-before-fix.png`, 1366 × 768 px.
- 수정된 포인트 소유권을 사용하여 사전 평활화 결정론적 재현을 수행하지만 `lineTo` 궤적: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-before-smoothing.png`, 1366 × 768 px.
- 사후 수정 활성 포인터 구현: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-after-fix-active.png`, 1366 × 768 px.
- 수정 후 릴리스 포인터 구현: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-after-fix-released.png`, 1366 × 768 px.
- 컴파일된 `build/Congregation.app` 릴리스 포인터 증거: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/compiled-app-released-no-dots.jpg`, 1366 × 768 px.
- 전체 보기 3상태 비교: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/laser-pointer-single-dot-comparison.png`, 4098 × 768 px.
- 제어된 폴리선/활성 곡선/릴리즈 곡선 비교: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/laser-pointer-smooth-curve-comparison.png`, 4098 × 768 px.
- 뷰포트 및 밀도: 구현 컴포넌트는 1366 x 768 논리 및 물리 픽셀로 렌더링되었으며, Qt 오프스크린 소프트웨어 백엔드를 사용했습니다. 1588 x 1026 소스는 비율에 맞게 스케일링되고 중앙에서 잘려 1366 x 768 로 조정된 후 비교되었습니다.
- 비교 상태: 프리젠테이션 모드에서 최근 여러 교차 레이저 제스처; 가운데 패널은 활성 커서를 유지하고 오른쪽 패널은 놓은 직후 정확히 동일한 트레일을 나타냅니다.
- 집중된 증거: 활성 포인터 구현은 별도의 자르기 없이 모든 조인, 캡, 교차점 및 유일한 커서 끝점을 검사할 수 있을 만큼 충분히 큽니다.

<a id="findings-4"></a>

## 조사 결과

- [Resolved P1] 이전 구현에서는 24 불투명 패스에서 라운드 조인을 사용했습니다. 따라서 곡선 또는 빠르게 변화하는 경로는 명시적인 `arc()` 호출이 제거되었음에도 불구하고 샘플링된 많은 정점에서 원형 코어와 빛을 재구성했습니다.
- [해결됨 P1] 샘플 도트를 제거하면 별도의 `lineTo` 폴리라인 결함이 노출됩니다. 희소 이벤트 샘플이 결합된 직선 섹션으로 나타나며 연속 곡선 마우스 궤적을 표현할 수 없습니다.
- 이제 모든 연속 실행은 끝점이 샘플링된 마우스 좌표인 Catmull–Rom 파생 큐빅 베지어 세그먼트를 사용합니다. 제어된 비교는 각도 조인에서 연속 곡선으로 변경되는 동일한 입력 좌표를 보여줍니다.
- 트레일에는 맞대기 캡과 베벨 결합이 유지됩니다. 원형 캡, 결합, 호, 채우기 또는 반복되는 점 항목이 궤적에 남아 있지 않습니다.
- `activeLaserPoint`는 한 번 인스턴스화되고 `currentPointX`/`currentPointY`에서만 배치됩니다. 활성 캡처에는 해당 커서 좌표에 정확히 하나의 합성 레이저 점이 포함되어 있습니다.
- 릴리스된 캡처에는 0 개의 점이 포함되어 있습니다. 0길이의 터미널 섹먼트를 생성하기 전에 정확한 중복 릴리스 좌표는 거부됩니다.
- 다시 빌드된 네이티브 앱이 다시 실행되고 프레젠테이션 모드로 전환되었으며 4개의 독립적인 전체 캔버스 드래그가 수신되었습니다. 릴리스 상태 캡처에서는 빨간색 끝점이나 샘플 도트 없이 페이딩 스트로크만 유지되었습니다.
- 타이포그래피 및 카피: 이 포인터 전용 오버레이에는 적용할 수 없습니다. 텍스트 표면이 변경되지 않았습니다.
- 간격 및 레이아웃 리듬: 전체 캔버스 오버레이 형상 및 6 px 코어/16 px 글로우 너비는 변경되지 않습니다.
- 색상 및 토큰: 기존 `#ff2b2b` 레이저 색상 및 불투명도 봉투는 변경되지 않습니다.
- 이미지 품질: 1×에서 앤티앨리어싱된 코어 및 글로우 스트로크가 선명하게 유지됩니다. 교차점은 자연스럽게 획 색상을 누적할 수 있지만 점 프리미티브로 표시되는 교차점은 없습니다.

<a id="comparison-history-2"></a>

## 비교 이력

1. 이전 QA 패스는 직선 드래그를 사용했는데, 이로 인해 라운드 조인 결함이 숨겨지고 합격으로 잘못 표시되었습니다.
2. 현재 사용자가 제공한 바쁜 경로 소스는 나머지 P1 반복 도트 불일치를 노출했습니다.
3. 결정적 접두사 재현을 통해 경로 반전 시 원형 조인 형상이 확인되었습니다.
4. 둥근 조인은 베벨 조인으로 대체되었고 플랫 캡은 유지되었으며 길이가 0인 중복 릴리스 샘플은 거부되었습니다.
5. 사후 수정 활성 및 릴리스 캡처는 정확히 하나의 커서 소유 도트를 표시한 다음 0개의 도트를 표시하며 궤적 소유 도트는 없습니다. 이 단계에서는 궤적 충실도가 아직 해결되지 않았습니다.
6. 추가 보고서에서는 남은 각도 폴리라인 효과가 확인되었으므로 점 수정 렌더링이 제어된 사전 스무딩 기준선으로 유지되었습니다.
7. 동일한 입력 샘플이 3차 베지어 보간을 통해 다시 렌더링되었습니다. 이제 굽힘은 연속 곡선을 따르며 활성 점이 수정되지 않은 현재 커서 좌표에 연결된 상태로 유지됩니다.
8. 릴리스된 곡선 캡처는 0개의 점을 유지합니다. 실행 가능한 P0/P1/P2 결과가 남아 있지 않습니다.

최종 결과: 합격
