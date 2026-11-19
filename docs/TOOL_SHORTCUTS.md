# 도구 모음 단축키

이 문서는 Congregation 편집기의 기본 키 배치와 처리 계약이다. 단축키는 도구
아이콘을 누르는 것과 같은 도구 선택 경로를 사용한다. macOS에서는 `Command`,
Windows와 Linux에서는 `Ctrl`을 기본 명령 키로 표기한다. `Option`과 `Alt`는
같은 보조 키다.

## 공통 동작

- 다른 도구군을 사용 중이라면 문자 키로 아래 표의 첫 번째 도구를 선택한다.
- 같은 도구군의 단축키를 다시 누를 때마다 표의 다음 도구로 이동한다. 마지막
  도구 다음에는 첫 번째 도구로 돌아간다. 마우스 등으로 고른 도구도 순환의
  기준이 된다. 사용할 수 없는 도구는 안내를 표시하며 다음 전환을 막지 않는다.
- `Shift`나 `Option/Alt`를 누른 문자 키는 도구군을 전환하지 않는다.
- `Command/Ctrl + 위쪽 화살표`와 `Command/Ctrl + 아래쪽 화살표`는 브러시
  크기를 각각 1px씩 정밀하게 증감한다. 크기는 설정의 허용 범위에 묶인다.
- 텍스트를 입력하는 동안에는 이 단축키가 도구나 브러시 설정을 바꾸지 않는다.
- 키보드 도구 단축키는 `Command/Ctrl`과 `Option/Alt`를 함께 누른 키 입력을
  다른 명령에 넘긴다.

## 브러시 불투명도

브러시 도구군(`B`)에서는 키보드 상단 숫자열로 활성 브러시의 `Opacity`를 조작한다.
`7`을 한 번 누르면 즉시 70%가 된다. 첫 입력 후 300ms 이내에 두 번째 숫자를
누르면 두 숫자를 백분율로 적용한다. `2`를 두 번 누르면 20%에서 22%로 바뀐다.
`0` 한 번은 100%, 연속 `00`은 0%, 연속 `05`는 5%이다. 두 자리가 완성되면
다음 숫자는 새 입력을 시작한다. [Adobe Photoshop 브러시 불투명도](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/brushes-presets/paint-with-brush-or-pencil-tool.html)

숫자패드, 보조 키와 함께 누른 숫자, 키를 누른 채 유지할 때의 반복 신호는 이 단축키의
대상에서 제외한다. 텍스트 입력란은 숫자를 편집에 사용한다. 다른 키를 누르거나 활성
브러시가 달라지면 다음 숫자는 새 입력으로 처리한다.

키로 변경한 값은 활성 브러시의 기존 `Opacity` 설정·패널 표시·캔버스에 반영된다.
다른 브러시로 전환하면 그 브러시의 설정이 적용된다. 변경 값을 계속 사용하려면
Tool 패널의 기존 설정 저장 기능을 사용한다.

## 브러시 포인터 단축키

브러시가 활성화된 상태에서 캔버스 위의 마우스로 조작한다.

- `Option/Alt + 왼쪽 클릭`은 현재 표시 중인 프레임의 합성 픽셀을 샘플링해
  전경색으로 적용한다. 색을 집은 뒤 같은 브러시 프로필로 계속 그릴 수 있다.
  [Adobe Photoshop 페인팅 중 색상 선택](https://helpx.adobe.com/uk/photoshop/desktop/adjust-color/choose-colors/choose-a-color-while-painting.html)
- `Command + Option + 왼쪽 버튼 드래그`는 활성 브러시의 크기 또는 경도를
  바꾼다. Windows와 Linux에서는 `Ctrl + Alt`를 쓴다. 오른쪽으로 밀면 크기가
  커지고 왼쪽으로 밀면 작아진다. 아래로 밀면 경도가 높아지고 위로 밀면 낮아진다.
  [Adobe Photoshop 브러시 포인터 조정](https://helpx.adobe.com/au/photoshop/desktop/get-started/settings-and-preferences/change-tool-pointers.html)

드래그는 시작점에서 4 논리 픽셀 이상 움직였을 때 이동량이 큰 축으로 고정된다.
이후 이동량을 시작점 기준으로 계산한다. 수평 이동 1 논리 픽셀은 브러시 크기
1px, 수직 이동 1 논리 픽셀은 경도 1%p에 해당한다. 크기 범위는 1–1024px이고
경도 범위는 0–100%이다. 조작 중 바뀌는 값은 Tool 패널의 `Size` 또는
`Hardness`에 반영되며, 도구를 바꾸거나 포인터 조작이 취소되면 드래그가 끝난다.

네이티브 자체 검사는 열린 편집기의 캔버스에 네 방향의 수정키 드래그를 전달한다.
각 드래그에서 대응하는 설정 값과 캔버스 편집 버전을 확인한다.

마우스가 캔버스 위에 있을 때 브러시, 지우개, 복제 도장은 현재 팁의 윤곽을
커서로 표시한다. 윤곽은 문서 픽셀 단위의 크기에 뷰포트 확대율을 곱해 그린다.
활성 팁의 모양, 원형률, 각도가 반영된다. 펜 입력의 동역학은 획에 적용된다.

스포이드 도구 또는 브러시의 `Option/Alt + 왼쪽 버튼`으로 색을 채취할 때,
버튼을 누른 동안 채취 지점의 원형 링 위쪽에 채취한 색, 아래쪽에 직전 전경색을
표시한다. 열린 중앙으로 원래 픽셀이 보인다. 버튼을 떼거나 캔버스를 벗어나면
링이 사라진다. 채취한 색은 누르는 순간 적용된다.

두 커서는 캔버스의 표시 레이어에서 갱신한다. 포인터 이동은 문서 버전을 바꾸거나
캔버스 타일을 다시 합성하지 않는다.
[Figma 커서 시안](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=485-20746)

## 도구군

각 행의 순서는 문자 키를 반복해서 누를 때의 순서다. Photoshop과 Clip Studio
Paint의 주요 문자 할당을 우선하고, 두 제품의 기본값이 충돌하거나 대응 도구가
없는 경우 Congregation의 나머지 키를 사용한다.

| 키 | 도구 순서 |
| --- | --- |
| `V` | Move |
| `M` | Rectangular Marquee → Elliptical Marquee |
| `L` | Lasso → Polygonal Lasso → Magnetic Lasso |
| `W` | Magic Wand |
| `C` | Crop → Perspective Crop |
| `I` | Eyedropper |
| `B` | Brush → Pencil → Airbrush → Mixer Brush → Dry Brush → Bristle Brush |
| `E` | Eraser |
| `G` | Gradient → Paint Bucket |
| `K` | Smudge → Blur → Sharpen |
| `S` | Clone Stamp → Pattern Stamp |
| `J` | Spot Healing Brush → Healing Brush |
| `O` | Dodge → Burn → Sponge |
| `Y` | Liquify → Warp |
| `P` | Pen → Curvature Pen → Add Anchor Point → Delete Anchor Point |
| `A` | Direct Selection |
| `U` | Rectangle → Ellipse → Polygon |
| `T` | Area Text → Path Text |
| `H` | Hand |
| `R` | Rotate View |
| `Z` | Zoom |
| `Q` | Quick Mask 켜기/적용 |

## 도구 모음의 공통 기능

| 키 | 동작 |
| --- | --- |
| `Command/Ctrl + R` | Rulers 표시 전환 |
| `Command/Ctrl + Shift + R` | 수직 Symmetry Painting 켜기/끄기 |

환경설정은 기존 앱 메뉴의 `Command/Ctrl + ,`를 사용한다. 파일·편집 메뉴의
New, Open, Save, Undo/Redo, Copy/Paste 및 선택 명령은 기존 명령 키를 유지한다.

## 현재 구현 범위

단축키는 편집기의 기존 도구 선택 및 공통 기능으로 연결한다. 현재 편집기가
지원하지 않는 Pattern Stamp, Spot Healing Brush, Healing Brush, Blur,
Sharpen, Dodge, Burn, Sponge, Liquify, Warp, Pen, Curvature Pen, Add/Delete
Anchor Point, Path Text, Rotate View는 선택을 가장하지 않고 사용할 수 없다는
안내를 표시한다. 도구 기능 구현 여부는 단축키 할당과 별개다.

실제 플랫폼의 키 전달과 앱 설치 상태는 이 문서만으로 보장하지 않는다. iiGUI의
iPadOS 호스트에는 텍스트 편집 중이 아닐 때 외장 키보드의 문자 키를 전달하는
소스 수정이 포함되어 있다. 수정된 SDK를 링크한 iPad 앱과 실제 키보드·입력기
동작은 별도로 빌드하고 기기에서 확인해야 한다.

## 참고

- [Adobe Photoshop 도구 단축키](https://helpx.adobe.com/content/dam/help/en/photoshop/using/default-keyboard-shortcuts/photoshop-keyboard-shortcuts.pdf)
- [Clip Studio Paint 도구 단축키](https://help.clip-studio.com/en-us/manual_en/780_shortcuts/Tool_Shortcuts.htm)
- [Clip Studio Paint 조작 중 보조 키](https://help.clip-studio.com/en-us/manual_en/780_shortcuts/Shortcuts_usable_during_operation.htm)
