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
- `Command/Ctrl`과 `Option/Alt`를 함께 누른 입력은 도구 단축키가 가로채지 않는다.

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
