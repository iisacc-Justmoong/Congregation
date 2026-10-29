# Congregation Figma 기능 재현을 위한 iiSDK 역량 감사 보고서

기준일: 2026-10-26. 대상은 Congregation Main `83:6973`/`83:6978`, 전체 Tool `130:1832`와 같은 창의 탐색·시간·레이어·상세 패널이다. Photoshop/Clip Studio 전체 기능이나 예전 네 개 시안 전체의 동등성을 요구 목록으로 확장하지 않는다.

## 판단

**현재 iiSDK는 이 화면을 표시하고 기본 그림·레이어·파일 편집을 실행할 기반은 갖추고 있지만, Figma가 제시하는 전문 도구와 트랙을 모두 동작시키기에는 충분하지 않다.** 부족의 중심은 새 모듈의 개수보다 편집 세션 연결, 정규 문서 개념, 일부 공개 API의 정확성이다. 기존 마스크·클리핑·벡터·모션·필터 커널을 활용하는 작업과, 텍스트·계층 그룹·16비트 문서처럼 데이터 계약을 추가하는 작업을 분리해야 한다.

실제 레이어에서 도구 레일의 액션 슬롯 **49개**(색상·설정·보기 포함, 구분선 4개 제외), 상단 컨트롤 **26개**, Tool 파라미터 **22개**를 추출했다. 이름의 “44 controls”는 실제 슬롯 수와 맞지 않는다. +/-/슬라이더는 같은 파라미터의 조작 수단으로 묶었다. 아래 표는 위치별 추적을 위한 **130개 대조 행**이며, 같은 명령이 다른 영역에서 반복되기도 한다. 고유 엔진 기능 수나 완료율이 아니다. 부족·미흡 항목은 **34개**로 묶었다.

## 판정 기준과 검토 범위

- **기반 있음:** 공개 타입/구현에서 해당 작업을 구성할 수 있다. 도구 UX·파일 왕복·앱 연결·기기 실행까지 완료했다는 뜻은 아니다.
- **부분:** 커널이나 일부 데이터만 있고 현재 요구를 완결할 편집/저장/입력 계약이 부족하다.
- **정규 타입 부족 / API 미확인:** 검토한 공개 헤더·문서 모델·타깃에서 해당 의미를 직접 표현하는 계약을 확인하지 못했다. 다른 이름의 일반 수학·이미지 API가 없다는 뜻은 아니다.
- **미연결 / 비활성:** SDK에 기반이 있어도 앱이 연결하지 않았거나 명시적으로 버튼을 끈 상태이다.
- **결함:** 이번 설치 소비자 실행 또는 명확한 분기에서 선언된 값과 실제 동작의 불일치를 확인했다.
- **미정:** Figma의 아이콘/문구만으로 제품 동작을 결정할 수 없다. 구현 전에 수락 예제가 필요하다.

Figma는 수정하지 않고 현재 레이어·표시값·컨트롤을 읽었다. 도구·파라미터와 C++ 선택 맵, 공개 헤더, 실제 구현, CMake 설치/링크 경계와 관련 테스트 소스를 대조했다. 기존 macOS 22개 검사 및 iPad 창 검사의 과거 성공을 이번 전체 도구 감사의 성공으로 재사용하지 않았다. 이번에 새로 실행한 것은 아래의 작은 Qt-free 설치 소비자 진단이다. SDK/앱 구현은 변경하지 않았다.

## 실행으로 확인한 공개 API 문제

독립 C++23 프로그램을 `iiSharedCanvas::Native` exported target만으로 구성했다. Qt5/Qt6 탐색을 차단했고 환경의 include/library override를 제거했다. 동적 로더에서도 `congregation-web-account/prefix/lib/libiiPaintEngine.dylib`와 같은 키트의 FileProvider가 사용된 것을 확인했다. configure·build·실행은 모두 성공했으며, 성공은 진단 수행을 의미한다. SDK 기능이 올바르다는 판정과 구분한다.

| 진단 | 기대하는 의미 | 실제 결과 | 판정 |
|---|---|---|---|
| 반전 사각 선택 | 선택 안 0, 밖 1 | 안 1, 밖 0 | 반전 평가 결함 |
| Fill opacity=0 | 원본 검정 유지 | 완전한 빨강으로 교체 | 불투명도 무시 |
| Fill mask alpha=128 | 부분 합성 | 완전한 빨강으로 교체 | 부드러운 커버리지 무시 |
| Selection featherRadius=3 | 부드러운 경계 | 가장자리 1 그대로 | 필드가 이 경로에서 소비되지 않음 |
| legacy Radial Gradient | 거리별 색 변화 | 처음/끝 모두 빨강 | 방사형 분기가 단색 대체 |
| iiSharedCanvas.fillRaster Radial | 거리별 색 변화 | 실제 서로 다른 픽셀 | 정상 대조군; SDK 전체의 radial 부재 아님 |
| BitmapEditor.beginStroke(StrokePoint) | 기울기/시간을 담는 입력 경계 | 해당 오버로드 없음 | 좌표/압력 경계로 고급 입력 손실 |

진단 결과는 `build/sdk-capability-audit-20261026/probe-results.json`, 로더와 실행 검증은 `probe-verification.json`이다. D01은 현재 앱의 floodMask 기반 Fill이 위 legacy FillOperation을 호출한다는 주장이 아니라, 재사용 가능한 SDK 공개 면의 결함 보고이다.

## 모듈별 준비 상태

| 모듈 | 소스 버전 | Congregation macOS 키트 | iPad 키트 | 현재 판단 |
|---|---|---|---|---|
| iiPaintEngine | 0.5.1 | iiPaintEngine 0.5.1 | iiPaintEngine 0.5.1 | 브러시·입력·마스크 기반 풍부; 저수준 편집 정확성과 native bridge 보강 |
| iiSharedCanvas | 0.32.0 | iiSharedCanvasNative 0.32.0 | iiSharedCanvasNative 0.32.0 | 문서·레이어·마스크·모션 기반; 그룹/텍스트/정밀도/네이티브 미디어 부족 |
| iiGraphicAdjust | 0.6.1 | 없음 | 없음 | 복제·힐링·메시·보정 커널 존재; 앱용 키트와 도구 세션 필요 |
| iiGUI | 0.4.1 | iiGUI 0.4.1 | iiGUI 0.4.1 | 창/GPU 표시/입력 기반; 회전 viewport·플랫폼 입력/성능 범위 보강 |
| LVUI | 0.1.0 | LVUI 0.1.0 | LVUI 0.1.0 | 선언적 뷰·Color/Metric 기반; 동적 구성/전체 수명 일부 미완료 |
| LVRS | 1.0.1 | LVRS 1.0.1 | LVRS 1.0.1 | 현재 기본 컨트롤 사용 가능; 원본 대비 partial 상태 |
| iiImageCodec | 0.1.0 | iiImageCodec 0.1.0 | iiImageCodec 0.1.0 | 8비트 교환/ICC 정규화 기반; 16비트 작업 파이프라인과 다름 |
| iiColorManager | 0.1.1 | 없음 | 없음 | 색 선택·변환 있음; GUI 모델 통합과 모드 연결 필요 |
| iiDeviceMultimedia | 0.2.1 | 없음 | 없음 | 장치 PCM 기반; 오디오 편집/동기화 세션과 별개 |
| iiFileProvider | 0.7.3 | iiFileProviderNative 0.9.0 | iiFileProviderNative 0.9.0 | 파일/SQLite/원자성 기반; 데이터 형식의 상위 의미는 Canvas 소유 |
| iiSDKCore | 0.20.0 | iiSDKCore 0.18.1 | iiSDKCore 0.20.0 | 실행·비동기·프레임 기반; 회화 도메인을 Core로 옮길 이유 없음 |
| iiSocietyHelper | 0.11.0 | 없음 | 없음 | core facade Qt 의존; native 앱에 전체 facade를 그대로 연결 불가 |

버전은 source CMake와 실제 ConfigVersion 파일을 따로 읽었다. Native/FileProvider의 별도 버전과 Core의 플랫폼별 버전을 하나로 합치지 않았다. 여러 SDK와 앱에 기존 미커밋 변경이 있으므로 `module-snapshot.json`에 HEAD·변경 파일을 함께 기록했다. 동일한 버전 문자열이 바이너리/헤더 동일성을 보장하지 않는다.

### 같은 SDK 안의 서로 다른 모델

`iiSharedCanvas::Document`는 현재 앱이 저장하고 렌더링하는 회화 문서이다. 별도 `TimelineProject`에는 locked/syncLocked 트랙, 클립 링크 집합, crop/skew, 오디오 pan/fade/channelMatrix와 자막 텍스트까지 있다. 이 타입들의 존재를 누락된 개념 전체의 부재로 해석하지 않았다. 반대로 그 값 모델이 자동으로 현재 Document.layers·FrameRenderer·앱 편집기에 적용되지는 않는다. 통합 화면의 데이터 정본과 명시적인 어댑터·저장·실행 정책이 필요한 영역이다. [E59](#e59) [E60](#e60) [E61](#e61) [E62](#e62)

## 부족 영역 목록

P0는 실제 픽셀·문서 신뢰성 또는 현재 활성 파라미터의 의미를 먼저 바로잡아야 하는 항목이다. P1은 기능 완성에 필요한 도메인/연결 작업이며, P2는 사용 플랫폼과 실제 컨트롤 범위로 좁혀 검증할 항목이다.

| ID | 우선순위 | 종류 | 부족한 개념/영역 |
|---|---|---|---|
| D01 | P0 | 확인된 결함과 부분 구현 | 선택과 채우기의 저수준 API 동작 불일치 |
| D02 | P0 | 데이터 계약 부족 | Figma의 16비트 문서와 실제 8비트 sRGB 경로 |
| D03 | P0 | 공개 입력 경계 부족 | 기울기와 실제 시간 정보가 끊기는 브러시 경로 |
| D04 | P0 | 편집 브리지 부족 | Mixer와 Smudge의 배경 픽셀 샘플러 연결 |
| D05 | P0 | 시간 기반 세션 부족 | 정지 분사와 따라잡기의 실제 시간 진행 |
| D06 | P1 | 도구 세션과 표현 연결 부족 | 영역 선택과 Quick Mask의 일관된 편집 계약 |
| D07 | P1 | 명시적 알고리즘 API 미확인 | 자기 올가미의 경계 추종 |
| D08 | P1 | 문서 단위 작업 부족 | 자르기와 원근 자르기 |
| D09 | P1 | 상호작용과 뷰 변환 부족 | 자유 변형과 정렬 및 비파괴 뷰 회전 |
| D10 | P1 | 앱 도구 연결 부족 | 복제 도장과 힐링 및 패턴 도장 |
| D11 | P1 | 국부 도구 어댑터 부족 | Blur Sharpen Dodge Burn Sponge 브러시 |
| D12 | P1 | 대화형 편집 세션 부족 | Liquify와 Warp의 미리보기 및 비파괴 저장 |
| D13 | P1 | 벡터 도구 연결 부족 | Pen Curvature Pen 제어점 편집 |
| D14 | P1 | 정규 문서 개념 부재 | 편집 가능한 영역 텍스트와 경로 텍스트 |
| D15 | P1 | 정규 문서 개념 부재 | 계층형 레이어 그룹과 Pass Through |
| D16 | P1 | 정규 편집 정책 부족 | 레이어 잠금과 레이어 링크 |
| D17 | P1 | 기존 SDK 미연결 | 레이어 마스크와 클리핑 편집 워크플로우 |
| D18 | P1 | 기존 SDK 미연결 | 프레임 노출과 모션 키프레임 편집 |
| D19 | P1 | 정책과 UI 미연결 | 어니언 스킨 |
| D20 | P1 | 네이티브 미디어 경로 부족 | Audio 트랙의 가져오기 재생 및 동기화 |
| D21 | P1 | 의미 불명확과 도메인 부재 | Camera 트랙 |
| D22 | P1 | 정규 문서 개념 부족 | Color balance 조정 레이어 |
| D23 | P1 | SDK 미연결과 타입 혼재 | 혼합 모드 선택과 Pass Object 표시 |
| D24 | P0 | 파라미터 의미와 소비 부족 | 22개 값의 도구별 유효성 및 End taper |
| D25 | P1 | SDK 자원 모델 미연결 | 브러시 프리셋과 Textured oil의 자원 계약 |
| D26 | P1 | 컬러 모델과 UI 연결 부족 | HSB 선택기와 공유 색상 상태 |
| D27 | P1 | 좌표 정책과 도구 미연결 | 눈금자 가이드 원근 그리드와 스냅 |
| D28 | P0 | 대형 문서 편집 계약 미흡 | 전체 스냅샷 Undo와 조용한 이력 소실 |
| D29 | P1 | 실제 GPU 처리 경로 부족 | GPU 표시와 문서 합성 가속의 구분 |
| D30 | P1 | 성능과 비동기 작업 계약 미흡 | 대형 필터 작업의 ROI 취소와 화면 반영 |
| D31 | P1 | 배포 프로필과 Qt 경계 부족 | 존재하는 SDK를 Qt 없는 앱 키트로 공급하는 경로 |
| D32 | P2 | 부분 지원과 플랫폼 검증 부족 | LVRS 네이티브 동등성과 입력 접근성 |
| D33 | P1 | 요구 사양 불명확 | 아이콘과 표시값만으로 결정할 수 없는 동작 |
| D34 | P1 | 기능 수락 검증 부족 | 화면 검사와 알고리즘 검사의 간극 |

<a id="d01"></a>
### D01 선택과 채우기의 저수준 API 동작 불일치

**P0 · 확인된 결함과 부분 구현 · 담당: iiPaintEngine; iiSharedCanvas**

설치 키트의 iiPaintEngine 0.5.1을 실제 링크한 진단에서 반전 사각 선택의 안/밖 커버리지가 1/0으로 나왔다. 불투명도 0의 채우기와 50% 마스크 채우기는 모두 완전한 빨강이 되었고, Radial Gradient는 시작 색 단색이었다. featherRadius=3도 가장자리 커버리지를 바꾸지 않았다.

반전 선택의 이중 반전, FillOperation.opacity 미사용, 마스크의 양수 여부만 보는 채우기, 방사형 분기의 미구현, featherRadius의 평가 누락을 각각 수정하거나 해당 API의 지원 범위를 축소해야 한다. iiSharedCanvas.fillRaster/featherMask/combineMasks의 별도 정상 경로와 중복된 의미도 정리해야 한다.

수락 기준: 안/밖 0/1, opacity=0 무변경, alpha=128 부분 합성, 방사형 중심/외곽 차이, featherRadius별 경계 커버리지를 독립 픽셀 값으로 검사한다. 기존 선형 그라디언트 검사만으로 통과시키지 않는다.

근거: [E16](#e16) [E17](#e17) [E18](#e18) [E15](#e15) [E50](#e50)

<a id="d02"></a>
### D02 Figma의 16비트 문서와 실제 8비트 sRGB 경로

**P0 · 데이터 계약 부족 · 담당: iiPaintEngine; iiSharedCanvas; iiImageCodec; iiColorManager**

Figma 상단·문서 정보는 RGB / 16-bit이다. RasterLayer는 ARGB32, iiImageCodec의 PixelFormat은 rgb8/rgba8이고 Native PSD 입력은 8-bit RGB만 허용한다. iiGraphicAdjust에는 sampleAverage16/solarizeSamples16 등 명시적인 16비트 샘플 커널도 있지만 정규 작업 문서와 연결된 파이프라인은 아니다. 중간 계산용 float/double도 16비트 작업 문서의 증거가 아니다.

픽셀 형식·비트 깊이·작업 프로파일·알파 정책·정밀도 변환·파일 왕복을 문서 계약으로 정의해야 한다. 현재 앱의 sRGB/8-bit 표시는 실제 제한에 가깝다. PPI/단위는 CanvasDefinition의 일시 상태이고 네이티브 비트맵 경로는 인쇄 해상도 비보존 경고를 낸다. CMYK/HDR 전면 지원은 현재 Figma에 확정된 요구가 아니므로 별도 범위 결정이다.

수락 기준: 16비트 미세 계조를 편집·저장·재열기·PNG/TIFF/PSD 교환 후 보존하고, 명시적 8비트 변환만 양자화한다. 프로파일·PPI 왕복과 지원하지 않는 깊이의 정직한 거부를 검사한다.

근거: [E20](#e20) [E21](#e21) [E58](#e58) [E11](#e11) [E48](#e48)

<a id="d03"></a>
### D03 기울기와 실제 시간 정보가 끊기는 브러시 경로

**P0 · 공개 입력 경계 부족 · 담당: iiGUI; iiSharedCanvas; iiPaintEngine; Congregation**

iiGUI WidgetInput과 iiPaintEngine StrokePoint에는 기울기·회전·timestamp가 있다. 그러나 Congregation→Artwork→BitmapEditor/ChunkedBitmapEditor는 좌표와 pressure만 전달하고, BitmapEditor가 이벤트마다 시간을 1/120초씩 만들어 낸다.

네이티브 캔버스 편집 세션이 실제 시각·기울기·회전·장치 식별자를 받는 경계를 제공해야 한다. iiPaintEngine BitmapPaintSession의 processPointerEvent/advanceTime가 재사용 가능한 선례이며 엔진 자체에 입력 개념이 없는 것은 아니다.

수락 기준: 동일 궤적을 다른 이벤트 빈도와 실제 시간으로 넣어 결과를 비교한다. 기울기·회전 변화가 픽셀에 반영되는지, 캔슬·장치 전환·비유한 입력이 안전한지 검사하고 물리 펜 검증을 따로 기록한다.

근거: [E29](#e29) [E54](#e54) [E06](#e06) [E09](#e09)

<a id="d04"></a>
### D04 Mixer와 Smudge의 배경 픽셀 샘플러 연결

**P0 · 편집 브리지 부족 · 담당: iiSharedCanvas; iiPaintEngine**

앱은 mixer/smudge 설정을 켜지만 BitmapEditor와 ChunkedBitmapEditor는 RasterSourceSampler 없는 projectBrushDabs 오버로드를 사용한다. wetDabColorArgb는 소스가 없으면 원래 dab 색으로 돌아간다. BitmapPaintSession에는 픽셀 샘플러를 주는 별도 구현이 있다.

현재 레이어 또는 합성 배경을 읽는 정책, 스트로크 시작 스냅샷과 최신 미리보기 중 어느 것을 샘플할지, 청크 경계·피드백 순서를 정하여 캔버스 편집기에 연결해야 한다.

수락 기준: 서로 다른 두 색 위에서 Mix/Wetness/Pickup을 조절했을 때 실제 색이 이동해야 한다. 투명 경계, 청크 경계, 되돌리기와 캔슬도 같은 기준으로 검사한다.

근거: [E07](#e07) [E57](#e57) [E08](#e08) [E09](#e09) [E02](#e02)

<a id="d05"></a>
### D05 정지 분사와 따라잡기의 실제 시간 진행

**P0 · 시간 기반 세션 부족 · 담당: iiSharedCanvas; iiPaintEngine; iiSDKCore**

엔진에는 airbrushRate와 smoothingCatchUp이 있고 BitmapPaintSession.advanceTime도 있다. 현재 캔버스 경로는 포인터 이벤트가 와야만 고정 1/120초를 진행한다.

누른 채 정지한 에어브러시, 정지 중 안정화 따라잡기, 화면 비활성·중단·복귀의 시계와 종료 처리를 연결해야 한다. 단순히 airbrushEnabled=true로 바꾸는 것으로 충족되지 않는다.

수락 기준: 같은 위치에서 0.2/1/2초 누른 결과가 설정된 초당 분사량을 반영하고, 이벤트 수에 비례하지 않아야 한다. 백그라운드 복귀 시 대량 분사가 발생하지 않아야 한다.

근거: [E06](#e06) [E09](#e09) [E10](#e10) [E02](#e02)

<a id="d06"></a>
### D06 영역 선택과 Quick Mask의 일관된 편집 계약

**P1 · 도구 세션과 표현 연결 부족 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

사각 SelectionState, byte SelectionMask, float PixelMask, iiSharedCanvas RasterMask와 flood/feather/combine API가 있다. 앱의 select는 레이어 hit-test이며 픽셀 영역 선택이 아니다.

사각·타원·올가미·다각형의 마스크 생성, add/subtract/intersect, 안티앨리어싱·반전·feather·Quick Mask 편집, 모든 도구에 적용되는 활성 선택 상태와 좌표 변환이 필요하다. 타입별 byte/float·문서/장치 좌표 의미를 명시해야 한다.

수락 기준: 확대·이동·회전 후에도 같은 문서 픽셀을 선택하고, 브러시·필터·채우기가 같은 부드러운 마스크를 사용한다. D01의 저수준 결함 경로를 재사용하지 않도록 검사한다.

근거: [E16](#e16) [E15](#e15) [E01](#e01) [E45](#e45)

<a id="d07"></a>
### D07 자기 올가미의 경계 추종

**P1 · 명시적 알고리즘 API 미확인 · 담당: iiPaintEngine 또는 별도 편집 도메인 계층**

현재 검토한 공개 헤더에 Magnetic Lasso 또는 Live Wire의 앵커·경로 추종 API는 없다. Canny/LineArt와 자동 마스크는 경계 정보이며 대화형 경계 추종 도구와 같지 않다.

경계 비용·탐색 반경·앵커 고정/삭제·닫기·수동 전환·노이즈 대응을 정의한 재사용 가능한 선택 경로 생성기가 필요하다.

수락 기준: 알려진 윤곽을 가진 이미지에서 목표 경로 오차와 앵커 편집을 검사하고, 평탄·잡음·투명 경계에서 종료와 취소가 보장되어야 한다.

근거: [E16](#e16) [E15](#e15) [E14](#e14)

<a id="d08"></a>
### D08 자르기와 원근 자르기

**P1 · 문서 단위 작업 부족 · 담당: iiSharedCanvas; iiGraphicAdjust; Congregation**

래스터 crop, CanvasExtent 편집, iiGraphicAdjust의 원근·메시 변형 기반은 있다. 별도 TimelineProject의 TimelineCrop도 클립 자르기 값으로 존재하지만 전체 회화 문서의 crop 실행 계약과는 다르다. 앱의 crop/perspective-crop 아이콘은 실행 맵에 없다.

캔버스와 다중 레이어·마스크·모션·아트보드의 원점 이동, 잘린 픽셀 보존 여부, 회전/원근 사각형, 출력 크기/PPI, 하나의 편집 트랜잭션을 결합해야 한다.

수락 기준: 레이어·마스크가 있는 문서를 자르고 저장·재열기·undo한 뒤 좌표와 픽셀을 대조한다. 단일 이미지 crop만 통과한 상태로 문서 crop 완료를 선언하지 않는다.

근거: [E11](#e11) [E25](#e25) [E01](#e01)

<a id="d09"></a>
### D09 자유 변형과 정렬 및 비파괴 뷰 회전

**P1 · 상호작용과 뷰 변환 부족 · 담당: iiSharedCanvas; iiPaintEngine; iiGUI; Congregation**

AffineTransform·레이어 변환은 있고 앱은 한 레이어의 이동/배율/회전과 좌우/상하 반전·90도 회전을 수행한다. 정렬·분배·rotate-view는 완성 경로가 없다. RasterViewport와 iiGUI ContentTransform에는 회전 항목이 없다.

다중 선택·기준점·변형 핸들·스큐/원근·확정/취소·정렬 기준이 필요하다. 뷰 회전·미러는 입력 역변환과 실제 문서 변형을 분리해야 한다. 현재 캔버스 하단 flip은 레이어 변형 명령으로 연결되어 있어 뷰 미러를 의도한 것인지 결정해야 한다.

수락 기준: 미리보기 중 문서 리비전 유지, 확정 1회 커밋, Escape 롤백, 회전된 화면의 펜 hit-test, 다중 레이어의 정렬·분배를 검사한다.

근거: [E38](#e38) [E44](#e44) [E45](#e45) [E11](#e11)

<a id="d10"></a>
### D10 복제 도장과 힐링 및 패턴 도장

**P1 · 앱 도구 연결 부족 · 담당: iiGraphicAdjust; iiSharedCanvas; Congregation**

iiGraphicAdjust에 cloneSpot/healSpot/removeSpot/rankSpotSources가 구현되어 있다. 앱은 clone/heal/spot-heal/pattern-stamp를 실행하지 못한다. SpotOptions는 주어진 점들의 원형 dab 합집합이며 빠진 점 사이를 자동 보간하지 않는다.

복제 소스 선택, aligned/non-aligned, 현재/합성 레이어 샘플링, 연속 dab 보간, 패턴 자원·원점·배율, 미리보기와 부분 커밋을 연결해야 한다. 커널 부재로 분류하여 재구현할 필요는 없다.

수락 기준: 소스 이동·투명도·선택 마스크·반복 스트로크와 캔슬을 픽셀로 검사한다. Heal을 Poisson 또는 생성형 지우개와 같은 동작이라고 설명하지 않는다.

근거: [E24](#e24) [E55](#e55) [E01](#e01)

<a id="d11"></a>
### D11 Blur Sharpen Dodge Burn Sponge 브러시

**P1 · 국부 도구 어댑터 부족 · 담당: iiGraphicAdjust; iiPaintEngine; iiSharedCanvas**

blur/sharpen, 노출·채도 조정과 마스크 합성 커널은 있다. Figma의 해당 브러시 도구는 앱 실행 맵에서 빠져 있다. GalleryFilters.sponge는 질감 효과로 채도 조절 Sponge 브러시와 다르며 ColorDodge 혼합 모드도 Dodge 브러시가 아니다.

브러시 커버리지로 국부 필터 강도를 누적하는 편집 세션, 영역/톤 범위·노출·채도 모드·알파 정책이 필요하다.

수락 기준: 스트로크 밖 픽셀이 변하지 않고 연속 누적과 가장자리 혼합이 정의되어야 한다. 이름이 비슷한 전역 필터를 연결한 것으로 도구 완료를 판정하지 않는다.

근거: [E24](#e24) [E26](#e26) [E15](#e15) [E01](#e01)

<a id="d12"></a>
### D12 Liquify와 Warp의 미리보기 및 비파괴 저장

**P1 · 대화형 편집 세션 부족 · 담당: iiGraphicAdjust; iiSharedCanvas; iiSDKCore**

iiGraphicAdjust는 Liquify·Puppet Warp·Perspective Warp와 메시 제약을 이미 제공한다. API는 제한된 입력을 받아 새 이미지를 반환하며 앱에는 연결되지 않는다.

원본/메시 소유권·핸들 편집·Freeze/Reconstruct·선택 마스크·ROI·취소·작업 예산·확정 트랜잭션과, 편집 가능한 메시를 문서에 남길지 결정해야 한다.

수락 기준: 전체/부분 작업 결과의 동일성, 비동기 취소, 큰 이미지 제한, alpha 보존과 저장 후 재편집을 검증한다. 픽셀 베이크만 지원하면 그 범위를 명시한다.

근거: [E25](#e25) [E26](#e26) [E01](#e01)

<a id="d13"></a>
### D13 Pen Curvature Pen 제어점 편집

**P1 · 벡터 도구 연결 부족 · 담당: iiSharedCanvas; Congregation**

VectorEditor에 경로·Bézier·앵커·제어점 삽입/교체/삭제 API가 있다. 앱의 pen/curvature-pen/add-anchor/delete-anchor는 실행되지 않고 direct-select는 레이어 선택으로 연결된다.

세그먼트 hit-test, 삽입 위치, 곡률 자동 핸들, cusp/smooth 전환, 핸들 드래그·확정/취소와 path-vs-layer 선택 상태가 필요하다.

수락 기준: 곡선 위 삽입 전후 형상 보존, 제어점 제약, 변형 후 hit-test와 명령별 undo를 검사한다. SDK의 벡터 곡선 자체가 없는 것은 아니다.

근거: [E14](#e14) [E01](#e01)

<a id="d14"></a>
### D14 편집 가능한 영역 텍스트와 경로 텍스트

**P1 · 정규 문서 개념 부재 · 담당: iiSharedCanvas; iiGUI; Congregation**

iiGUI에는 글꼴 셰이핑·TextRun·텍스트 편집 모델이 있고 별도 TimelineProject에는 TimelineSubtitleClip/스타일도 있다. 그러나 iiSharedCanvas 회화 Document의 Layer/Asset 타입에 편집 가능한 캔버스 텍스트 개체는 없고 앱은 Pretendard TextRun을 비트맵 레이어로 삽입한다.

문자·스타일 범위·폰트 식별·문단·텍스트 박스·기준선·경로 참조·레이아웃 캐시를 가진 캔버스 텍스트 문서 타입과 파일 왕복이 필요하다. UI TextDocument나 iiGeneralDocument의 문서 읽기 기능만으로 회화 문서의 editable text/path text가 생기지는 않는다.

수락 기준: 한글/복합 문자·줄바꿈·폰트 대체·곡선 위 배치·저장 후 재편집·출력 시 정책을 검사한다. 원본 문자열을 잃는 래스터화와 편집 가능한 텍스트를 구분한다.

근거: [E04](#e04) [E11](#e11) [E61](#e61) [E31](#e31)

<a id="d15"></a>
### D15 계층형 레이어 그룹과 Pass Through

**P1 · 정규 문서 개념 부재 · 담당: iiSharedCanvas**

Document.layers는 평면 목록이며 Artboard는 독립적인 비중첩 캔버스 그룹이다. Figma의 Painting 폴더와 Pass 표시는 일반 레이어 그룹을 암시하지만 정규 LayerGroup/부모 관계·그룹 합성 계약은 없다.

그룹 ID·부모/자식 순서·중첩 검증·pass-through/isolated·그룹 opacity/mask·재부모화와 좌표 보존을 정의해야 한다. 아트보드를 일반 그룹으로 대체하면 자르기와 원점 의미가 달라진다.

수락 기준: 중첩 그룹을 저장·재열기하고 반투명 배경·혼합 모드·그룹 마스크·undo를 픽셀로 검사한다. ORA 그룹 평탄화 경고를 그룹 보존 지원으로 해석하지 않는다.

근거: [E11](#e11) [E12](#e12) [E41](#e41)

<a id="d16"></a>
### D16 레이어 잠금과 레이어 링크

**P1 · 정규 편집 정책 부족 · 담당: iiSharedCanvas; Congregation**

회화 Document의 LayerProperties에 잠금·잠금 종류·링크 집합이 없고 앱의 lock/link 버튼도 비활성이다. 별도 TimelineProject에는 locked/syncLocked와 TimelineLinkGroup이 이미 있다. 문서 저장의 파일 잠금은 레이어 편집 잠금과 별개이다.

픽셀·위치·알파·전체 잠금 중 지원할 의미, 링크한 회화 레이어의 선택/변환 동작, 외부 소비자와 협업 편집에도 적용할 거부 규칙을 정의해야 한다. 기존 타임라인 값 모델의 재사용/매핑을 결정하여 같은 이름의 필드를 중복 신설하지 않는다.

수락 기준: UI 우회 호출·파일 재열기·원격 편집에서도 잠금 정책을 지키고, 링크 재부모화·삭제·undo를 검증한다.

근거: [E11](#e11) [E59](#e59) [E62](#e62) [E05](#e05)

<a id="d17"></a>
### D17 레이어 마스크와 클리핑 편집 워크플로우

**P1 · 기존 SDK 미연결 · 담당: iiSharedCanvas; iiPaintEngine; Congregation**

LayerAppearance는 raster/vector/filter mask, ordered mask stack, density/feather·clippedToBelow를 제공하고 렌더링·직렬화·테스트도 있다. 앱에서는 mask/clipping 버튼이 비활성이다.

활성 편집 채널을 콘텐츠/마스크로 바꾸고 선택→마스크 생성, 브러시 편집, 링크/해제, enable/invert와 썸네일을 연결해야 한다. 핵심 마스크 모델을 새로 만들 필요는 없다.

수락 기준: 콘텐츠와 마스크를 독립적으로 편집·저장·undo하고 부분 알파·벡터 마스크·클리핑 체인을 검사한다.

근거: [E12](#e12) [E13](#e13) [E51](#e51) [E05](#e05)

<a id="d18"></a>
### D18 프레임 노출과 모션 키프레임 편집

**P1 · 기존 SDK 미연결 · 담당: iiSharedCanvas; Congregation**

Document.frames/KeyframedSource와 LayerProperties.motion, setLayerMotion이 있다. 앱은 현재 시각을 이동·재생하지만 키 추가/삭제는 비활성이고 animated layer 복제는 명시적으로 거부한다.

현재 프레임의 편집 자산 결정, 노출 유지/분할·복제·이동·삭제, 모션과 콘텐츠 키 구분, 시간 범위와 ripple 여부를 정해야 한다. TimelineProject와 Canvas Document 중 정본도 명확히 해야 한다.

수락 기준: 0/중간/마지막 프레임에서 그리기·노출·모션을 저장/재열기하고 리비전·undo를 검사한다. 탐색은 무변경이며 키 편집은 한 명령이어야 한다.

근거: [E49](#e49) [E11](#e11) [E40](#e40) [E05](#e05)

<a id="d19"></a>
### D19 어니언 스킨

**P1 · 정책과 UI 미연결 · 담당: iiSharedCanvas; iiGUI; Congregation**

다른 프레임을 읽는 renderFrameRegion/Tile 기반은 있다. 앱의 onion-skin 버튼은 비활성이다.

이전/다음 노출 개수, 프레임/키 기준 간격, 틴트·감쇠·제외 레이어·캐시 무효화와 선택 레이어 정책이 필요하다.

수락 기준: 현재 문서 픽셀과 리비전을 바꾸지 않으며, 프레임/레이어 변경 후 잔상이 갱신되는지 검사한다.

근거: [E42](#e42) [E05](#e05)

<a id="d20"></a>
### D20 Audio 트랙의 가져오기 재생 및 동기화

**P1 · 네이티브 미디어 경로 부족 · 담당: iiSharedCanvas; iiDeviceMultimedia; iiSDKCore; Congregation**

Document에 PCM16 AudioAsset·클립·트랙·gain과 편집 API가 있고 Native DocumentAudio 검사가 있다. 별도 TimelineProject에도 pan/fade/channelMatrix 등을 가진 TimelineAudioMix가 있다. 그러나 Native 타깃은 AudioCodec.cpp와 그 공개 헤더를 빌드/설치하지 않는다. 앱 타임라인은 doc.layers만 열거하여 audioTracks를 표시하지 않는다.

WAV 등 코덱의 Qt 없는 공개 경로, PCM16↔float32 변환·믹싱/리샘플링·seek·파형·동영상 시계와 오디오 장치의 동기화가 필요하다. iiDeviceMultimedia의 장치 PCM I/O는 활용할 기반이다.

수락 기준: 파일 왕복 외에 재생·스크럽·프레임 레이트 변경·장면 중단/복귀의 A/V 오차와 장치 수명을 실측한다. 모형 Audio 행과 움직이는 마커는 실제 재생의 증거가 아니다.

근거: [E23](#e23) [E60](#e60) [E62](#e62) [E22](#e22) [E53](#e53)

<a id="d21"></a>
### D21 Camera 트랙

**P1 · 의미 불명확과 도메인 부재 · 담당: iiSharedCanvas; Congregation**

Figma는 Camera 행과 Object 표시를 가지지만 시야·카메라 키·출력 구도 계약은 없다. iiSharedCanvas MotionValue는 일반 레이어 변환이며 전용 2D Camera 트랙은 아니다.

2D 촬영 구도인지 단순 뷰 북마크인지 정한 뒤 프레이밍·줌·회전·보간·출력에 대한 적용과 저장 계약을 정의해야 한다. iiCameraIntegration의 실제 장치 촬영이나 iiActorObject의 3D 모델로 자동 대체할 수 없다.

수락 기준: 카메라 적용 전/후 프레임과 출력 영역을 정해진 픽셀로 비교하고, 카메라 편집과 일반 뷰 탐색의 문서 변경 여부를 분리한다.

근거: [E11](#e11) [E49](#e49) [E53](#e53)

<a id="d22"></a>
### D22 Color balance 조정 레이어

**P1 · 정규 문서 개념 부족 · 담당: iiSharedCanvas; iiGraphicAdjust**

iiGraphicAdjust에는 Color Balance 등 보정 커널이 있고 LayerAppearance.filters는 개별 콘텐츠 레이어에 보정을 적용한다. 아래 합성 결과에 작용하는 독립 Adjustment Layer 타입은 없다.

보정 파라미터·아래 스택 적용 범위·마스크·클리핑·그룹 경계·캐시 무효화와 직렬화 노드를 정의해야 한다. 현재 Figma의 Color balance 이름만으로 조정 레이어가 구현된 것은 아니다.

수락 기준: 아래 레이어를 바꾸면 보정 결과가 따라 바뀌고, 보정 레이어 이동/마스크/비활성/저장 왕복이 정의된 결과를 만들어야 한다.

근거: [E12](#e12) [E26](#e26) [E43](#e43)

<a id="d23"></a>
### D23 혼합 모드 선택과 Pass Object 표시

**P1 · SDK 미연결과 타입 혼재 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

SDK는 다수의 픽셀 혼합 모드를 지원한다. 앱 타임라인은 Normal/Multiply/Screen만 구별하여 표시한다. Figma의 Pass는 그룹 정책이고 Object는 일반 픽셀 혼합 모드가 아니다.

레이어 종류별 혼합 모드 목록과 표시 타입을 분리하고 전체 SDK 모드를 연결해야 한다. 지원하지 않는 종류를 Normal로 보이게 하는 표시 손실을 피해야 한다.

수락 기준: 각 혼합 모드의 저장 값과 UI 선택이 일치하고, 투명 배경과 clipping에서 독립 기대 픽셀을 검사한다.

근거: [E11](#e11) [E12](#e12) [E53](#e53)

<a id="d24"></a>
### D24 22개 값의 도구별 유효성 및 End taper

**P0 · 파라미터 의미와 소비 부족 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

22개 값의 표시·조절·저장은 있으나 End는 ToolSettings.brush에서 읽지 않는다. Wet/Mix/Load/Dilution은 mixer/smudge, Bristle count는 bristle 모드일 때만 유효하다. Pressure는 주로 size/flow, opacity는 별도 곡선으로 제어한다.

End의 후단 처리와 Start/Taper의 % 기준, Post correction의 당김줄 방식과 사후 보정의 차이, Dilution의 pickup/deposit 매핑을 정의해야 한다. Figma Roundness 최소 0%와 앱/엔진의 최소 1%도 일치시켜야 한다. 도구별 미지원 파라미터의 활성 정책이 필요하다.

수락 기준: 각 파라미터를 단독으로 바꾼 비교 스트로크와 유효 도구별 결과를 검사한다. 값 왕복이나 모델에 기록되었다는 이유만으로 시각 효과를 완료 처리하지 않는다.

근거: [E03](#e03) [E46](#e46) [E02](#e02) [E10](#e10)

<a id="d25"></a>
### D25 브러시 프리셋과 Textured oil의 자원 계약

**P1 · SDK 자원 모델 미연결 · 담당: iiPaintEngine; iiFileProvider; Congregation**

iiPaintEngine에는 BrushPreset/Library/Serializer·ABR·텍스처/팁 자원이 있다. 앱의 프리셋 선택은 두 이름과 일부 값 전환이며 ToolSettings는 22개 수치만 저장한다. BrushTexture 기본값은 disabled이다.

팁·텍스처·재질·동역학·도구 종류·자원 ID를 소유하는 프리셋과 현재 수치의 우선순위를 연결해야 한다. Textured oil/Pencil/Dry brush가 어떤 엔진 설정인지 명시해야 한다.

수락 기준: 프리셋 저장/재열기 후 팁·텍스처를 포함한 결과가 같고, 자원 누락·보호 텍스처·ABR 제한을 명시적으로 처리한다.

근거: [E39](#e39) [E03](#e03) [E56](#e56)

<a id="d26"></a>
### D26 HSB 선택기와 공유 색상 상태

**P1 · 컬러 모델과 UI 연결 부족 · 담당: iiColorManager; iiGUI; LVRS; Congregation**

iiGUI ColorPickerModel과 iiColorManager의 별도 Picker/색 변환 모델이 존재한다. 앱의 HSB ComboBox는 클릭 동작이 없으며 iiColorManager를 직접 소비하지 않는다. generic CMYK는 ICC 인쇄 변환과 별개이다.

사용할 하나의 색 상태 정본, 모드 선택 목록·채널 단위·알파·취소/확정 동기화를 정하고 기존 SDK 어댑터로 연결해야 한다.

수락 기준: HSB/RGB 등 확정한 모드를 왕복해 색과 alpha를 보존하고, eyedropper·brush·picker가 같은 값을 사용해야 한다.

근거: [E27](#e27) [E28](#e28) [E52](#e52)

<a id="d27"></a>
### D27 눈금자 가이드 원근 그리드와 스냅

**P1 · 좌표 정책과 도구 미연결 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

앱 눈금자는 64 화면 간격에 128 문서 값을 그리는 고정 표시이며 현재 viewport의 원점·배율을 읽지 않는다. grid/rulers 토글은 있고 snap/perspective-grid 명령은 구현 경로가 없다.

문서 좌표·단위·PPI·줌/이동/회전과 연결한 눈금, 이동 가능한 가이드·원근 소실점·스냅 후보/허용 거리/우선순위와 일시 해제를 정의해야 한다.

수락 기준: 다른 줌/원점/회전에서도 알려진 문서 좌표가 같은 눈금과 일치해야 한다. 스냅 허용 거리는 화면/문서 단위를 명시하고 시간 스냅과 공간 스냅을 분리한다.

근거: [E37](#e37) [E38](#e38) [E45](#e45)

<a id="d28"></a>
### D28 전체 스냅샷 Undo와 조용한 이력 소실

**P0 · 대형 문서 편집 계약 미흡 · 담당: iiSharedCanvas; iiPaintEngine; iiFileProvider; Congregation**

Artwork는 명령마다 전체 문서를 직렬화하고 스냅샷당 32MiB, 전체 이력 64MiB·32개 한도를 둔다. after/before 캡처가 비면 편집 커밋 후 undo 이력을 비우는 경로가 있다.

타일/영역 차이와 구조 명령의 delta 이력, 저장소와의 트랜잭션 연계, 메모리 한도 초과 시 사전 판정과 사용자에게 보이는 정책이 필요하다. iiPaintEngine의 부분 픽셀 패치 세션은 재사용 가능한 선례이다.

수락 기준: 고엔트로피 8K·다중 레이어·필터·마스크의 연속 편집에서 이력 크기와 취소 지연을 측정하고, 이력 손실을 숨기지 않도록 검사한다.

근거: [E35](#e35) [E36](#e36) [E09](#e09)

<a id="d29"></a>
### D29 GPU 표시와 문서 합성 가속의 구분

**P1 · 실제 GPU 처리 경로 부족 · 담당: iiPaintEngine; iiSharedCanvas; iiGUI**

iiGUI는 Metal/D3D11/Vulkan 표시를 제공한다. 그러나 iiPaintEngine renderLayerStackGpu의 available=true 경로는 CPU 함수 compositeLayerStack를 호출하고 Gpu라는 backend 값을 기록한다. 캔버스의 브러시 투영과 프레임 합성도 CPU 코드이다.

실제 실행 백엔드 표기를 바로잡고, 타일·브러시·필터·마스크 합성의 어떤 작업을 어느 GPU API가 수행하는지 기능 계약과 CPU 폴백·장치 손실 정책을 정해야 한다.

수락 기준: GPU 타이밍/명령 제출과 결과 픽셀을 함께 검사하고, 창이 Metal로 표시되었다는 사실만으로 GPU 브러시·문서 합성 완료를 선언하지 않는다.

근거: [E19](#e19) [E07](#e07) [E42](#e42)

<a id="d30"></a>
### D30 대형 필터 작업의 ROI 취소와 화면 반영

**P1 · 성능과 비동기 작업 계약 미흡 · 담당: iiSDKCore; iiSharedCanvas; iiGraphicAdjust; iiGUI**

Core Runtime/TaskScope와 FrameRenderTileRequest는 있다. GraphicAdjust의 다수 API는 동기 ImageView→새 Image이고 일부 레이어 필터는 원본 해상도 전체를 처리한다.

작업별 ROI/메모리/시간 예산, 실행 중 취소·진행률·문서 세대 가드·부분 결과 게시와 캐시 무효화를 공통 편집 세션에서 결합해야 한다. 일반 스케줄러를 새로 만들 필요는 없다.

수락 기준: 8K·수십 레이어·스크러빙/필터 동시 작업에서 입력 지연·메모리 상한·취소 시간·오래된 결과 거부를 측정한다. 측정 전에는 성능 충족을 판정하지 않는다.

근거: [E26](#e26) [E42](#e42) [E43](#e43)

<a id="d31"></a>
### D31 존재하는 SDK를 Qt 없는 앱 키트로 공급하는 경로

**P1 · 배포 프로필과 Qt 경계 부족 · 담당: iiSDKBuilder; 각 SDK; Congregation**

iiGraphicAdjust·iiColorManager·iiDeviceMultimedia는 호스트 설치본에 있지만 검토한 macOS/iPad Congregation 키트에는 없다. iiSocietyHelper의 core facade는 Qt 필수이고 core를 끈 monitoring 경로는 동일 기능을 제공하지 않는다. Core 키트는 macOS 0.18.1, iPad 0.20.0으로 서로 다르다.

필요한 peer 패키지를 대상 플랫폼별로 빌드·설치하고 네이티브 앱 어댑터를 링크해야 한다. Society Asset Library 연동을 다시 확장한다면 Qt 없는 client facade/프로토콜 경계를 사용해야 한다. 버전 차이 자체를 결함으로 단정하지 않되 한 실행 파일에서 헤더/바이너리/ABI를 섞지 않는다.

수락 기준: Qt 탐색 차단 상태의 설치 소비자·재배치·동적 로더와 iPad/데스크톱 각 패키지 조합을 검사한다. 현재 제거한 에셋 사이드바를 다시 필수 요구로 넣지는 않는다.

근거: [E47](#e47) [E34](#e34) [E41](#e41)

<a id="d32"></a>
### D32 LVRS 네이티브 동등성과 입력 접근성

**P2 · 부분 지원과 플랫폼 검증 부족 · 담당: iiGUI; LVRS; LVUI**

기본 창·아이콘·슬라이더·스크롤·색상 필드와 스타일 토큰은 있다. LVRS parity 목록은 해당 위젯들을 partial로 기록하고, LVUI는 동적 구성/전체 수명 일부를 미완료로 기록한다. iPad는 UIKeyInput 확정 문자 경로이며 전체 UITextInput marked-text와 외부 DnD는 지원 범위 밖이다.

이 화면에 필요한 키보드 증감·포커스·캡처 해제·IME·복합 문자·스크롤 clipping·가상 행 접근성·정밀 수치 조작을 실제 대상 플랫폼에서 검증해야 한다. 오래된 문서의 포괄적 미지원 문구와 신규 API를 대조하여 지원표도 갱신해야 한다.

수락 기준: 현재 Figma에 쓰인 컴포넌트/상태만 먼저 묶어 입력·렌더링·접근성 검사를 실행한다. 기존 macOS/iPad 창 검사는 전문 도구 전체나 Windows/Linux 실행 증거로 확대하지 않는다.

근거: [E29](#e29) [E31](#e31) [E32](#e32) [E33](#e33)

<a id="d33"></a>
### D33 아이콘과 표시값만으로 결정할 수 없는 동작

**P1 · 요구 사양 불명확 · 담당: 제품 기능 명세; 편집 도메인 인터페이스**

툴 레일 이름은 44 controls이지만 실제 액션 슬롯은 구분선 제외 49개이다. Free transform, Camera/Object, Pass, Clone source, Tool/Doc/View/More 및 브러시 그룹의 동작 범위가 아이콘만으로 확정되지 않는다. Tool 파라미터의 prototype reaction 존재는 엔진 실행 계약이 아니다.

각 도구의 대상·입력 제스처·수치 단위·미리보기·저장 데이터·undo 단위·허용 레이어·오류/비활성 정책을 짧은 기능 명세로 고정해야 한다. 보이는 버튼과 실제 도구 수를 구분한다.

수락 기준: Figma 항목마다 성공/취소 예제와 독립 기대 결과를 지정하고 미정 상태를 지원 완료로 분류하지 않는다.

근거: [E01](#e01) [E03](#e03) [E05](#e05)

<a id="d34"></a>
### D34 화면 검사와 알고리즘 검사의 간극

**P1 · 기능 수락 검증 부족 · 담당: 각 SDK tests; Congregation tests**

기존 제품 검사는 창·색·값·기본 편집과 독립 실행을 검증하지만, 모든 Figma 도구의 의미를 증명하지 않는다. 현재 실행 진단에서도 기존 공개 API의 여러 문제 동작이 재현되었다.

모듈별 API 존재/Qt-free 소비/문서 왕복/픽셀·입력 의미/앱 연결/대상 기기 실행을 따로 기록하는 capability 표와 도구별 수락 테스트가 필요하다.

수락 기준: 마스크·혼합·압력·시간·프레임·텍스트·그룹의 독립 수치/픽셀 기준을 사용한다. 구현을 그대로 복사한 기대값이나 빈 no-op 성공을 통과 근거로 쓰지 않는다.

근거: [E50](#e50) [E51](#e51) [E32](#e32)

## Figma 항목별 기능 대조표

“연결”은 표에 적힌 기본 범위의 연결 상태이다. 관련 부족 ID가 있으면 해당 조건을 함께 읽어야 한다. CSV에는 같은 대조표와 직접 연결한 Figma 노드 ID를 담았다.

### 좌측 도구

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| move | 기반 있음 | 부분 연결 | 한 레이어 hit-test/변환. 다중 선택·정렬 세션은 별도이다. | [D09](#d09) |
| rect-marquee | 기반 있음 | 미연결 | 사각·마스크 기반은 있으나 활성 픽셀 선택 UI가 없다. | [D01](#d01) [D06](#d06) |
| ellipse-marquee | 부분 | 미연결 | 마스크와 벡터 기반은 있지만 제스처/마스크 생성 도구가 필요하다. | [D06](#d06) |
| lasso | 부분 | 미연결 | 마스크와 벡터 기반은 있지만 제스처/마스크 생성 도구가 필요하다. | [D06](#d06) |
| polygon-lasso | 부분 | 미연결 | 마스크와 벡터 기반은 있지만 제스처/마스크 생성 도구가 필요하다. | [D06](#d06) |
| magnetic-lasso | 공개 API 미확인 | 미연결 | 자동 경계 추종 세션이 필요하다. | [D07](#d07) |
| magic-wand | 기반 있음 | 미연결 | floodMask의 tolerance/contiguous 기반을 도구에 연결해야 한다. | [D01](#d01) [D06](#d06) |
| crop | 부분 | 미연결 | 픽셀/메시 커널과 문서 crop 작업을 구분한다. | [D08](#d08) |
| perspective-crop | 부분 | 미연결 | 픽셀/메시 커널과 문서 crop 작업을 구분한다. | [D08](#d08) |
| color-picker | 기반 있음 | 부분 연결 | 스포이트 경로는 있으나 현재 프레임·샘플 대상·프로파일 정책이 필요하다. | [D02](#d02) [D26](#d26) |
| brush | 기반 있음 | 부분 연결 | 기본 페인팅은 연결. 고급 입력·선택·프리셋 자원은 별도이다. | [D03](#d03) [D24](#d24) [D25](#d25) |
| pencil | 기반 있음 | 부분 연결 | 일반 brush action으로 연결되며 이름에 맞는 독립 프리셋 계약이 없다. | [D25](#d25) [D24](#d24) |
| airbrush | 기반 있음 | 부분 연결 | 분사 flag는 있으나 정지 중 실제 시간 진행 경로가 빠져 있다. | [D03](#d03) [D05](#d05) |
| mixer-brush | 기반 있음 | 부분 연결 | simulation flag는 켜지만 native 편집기의 배경 샘플러 연결이 부족하다. | [D04](#d04) [D24](#d24) |
| dry-brush | 기반 있음 | 부분 연결 | 일반 brush action으로 연결되며 이름에 맞는 독립 프리셋 계약이 없다. | [D25](#d25) [D24](#d24) |
| bristle-brush | 기반 있음 | 부분 연결 | bristle.enabled/count는 연결. 기울기 입력과 도구별 파라미터 검증이 부족하다. | [D03](#d03) [D24](#d24) |
| eraser | 기반 있음 | 연결 | DestinationOut 페인팅 경로. 활성 픽셀 선택과 잠금 정책은 추가해야 한다. | [D06](#d06) [D16](#d16) |
| gradient | 기반 있음과 중복 결함 | 미연결 | 정상 fillRaster와 불완전한 legacy GradientOperation이 함께 존재한다. | [D01](#d01) [D06](#d06) |
| paint-bucket | 기반 있음 | 부분 연결 | 합성 결과의 tolerance=0 floodMask로 새 Fill 레이어를 만든다. 대상/참조 정책이 제한된다. | [D06](#d06) [D33](#d33) |
| smudge | 기반 있음 | 부분 연결 | simulation flag는 켜지만 native 편집기의 배경 샘플러 연결이 부족하다. | [D04](#d04) [D24](#d24) |
| clone-stamp | 커널 또는 기반 있음 | 미연결 | 연속 도구·소스/패턴 상태·미리보기/커밋 연결이 필요하다. | [D10](#d10) [D31](#d31) |
| pattern-stamp | 커널 또는 기반 있음 | 미연결 | 연속 도구·소스/패턴 상태·미리보기/커밋 연결이 필요하다. | [D10](#d10) [D31](#d31) |
| healing-brush | 커널 또는 기반 있음 | 미연결 | 연속 도구·소스/패턴 상태·미리보기/커밋 연결이 필요하다. | [D10](#d10) [D31](#d31) |
| spot-heal | 커널 또는 기반 있음 | 미연결 | 연속 도구·소스/패턴 상태·미리보기/커밋 연결이 필요하다. | [D10](#d10) [D31](#d31) |
| blur | 커널 또는 기반 있음 | 미연결 | 국부 브러시 세션이 필요하다. Gallery Sponge/ColorDodge는 같은 도구가 아니다. | [D11](#d11) [D31](#d31) |
| sharpen | 커널 또는 기반 있음 | 미연결 | 국부 브러시 세션이 필요하다. Gallery Sponge/ColorDodge는 같은 도구가 아니다. | [D11](#d11) [D31](#d31) |
| dodge | 커널 또는 기반 있음 | 미연결 | 국부 브러시 세션이 필요하다. Gallery Sponge/ColorDodge는 같은 도구가 아니다. | [D11](#d11) [D31](#d31) |
| burn | 커널 또는 기반 있음 | 미연결 | 국부 브러시 세션이 필요하다. Gallery Sponge/ColorDodge는 같은 도구가 아니다. | [D11](#d11) [D31](#d31) |
| sponge | 커널 또는 기반 있음 | 미연결 | 국부 브러시 세션이 필요하다. Gallery Sponge/ColorDodge는 같은 도구가 아니다. | [D11](#d11) [D31](#d31) |
| liquify | 커널 있음 | 미연결 | 메시·국부 입력·비동기 미리보기·문서 저장 정책을 연결해야 한다. | [D12](#d12) [D31](#d31) |
| warp | 커널 있음 | 미연결 | 메시·국부 입력·비동기 미리보기·문서 저장 정책을 연결해야 한다. | [D12](#d12) [D31](#d31) |
| pen | 기반 있음 | 미연결 | Bézier/제어점 편집 API를 도구로 연결해야 한다. | [D13](#d13) |
| curvature-pen | 기반 있음 | 미연결 | Bézier/제어점 편집 API를 도구로 연결해야 한다. | [D13](#d13) |
| direct-select | 기반 있음 | 다른 의미로 연결 | 현재 레이어 선택으로 연결되어 벡터 노드 직접 선택이 아니다. | [D13](#d13) [D09](#d09) |
| add-anchor | 기반 있음 | 미연결 | Bézier/제어점 편집 API를 도구로 연결해야 한다. | [D13](#d13) |
| delete-anchor | 기반 있음 | 미연결 | Bézier/제어점 편집 API를 도구로 연결해야 한다. | [D13](#d13) |
| rectangle | 기반 있음 | 연결 | 현재 도형 생성 경로가 있다. | [D09](#d09) |
| ellipse | 기반 있음 | 연결 | 현재 도형 생성 경로가 있다. | [D09](#d09) |
| polygon | 기반 있음 | 미연결 | 벡터 명령으로 구성 가능하나 이 도구의 입력/변 수 옵션이 없다. | [D13](#d13) [D33](#d33) |
| area-text | 정규 문서 타입 부족 | 래스터 삽입 | 텍스트를 bitmap으로 만들며 저장 후 문자 재편집이 불가능하다. | [D14](#d14) |
| path-text | 정규 문서 타입 부족 | 미연결 | 문자와 벡터 경로의 결합 모델이 필요하다. | [D14](#d14) |
| hand | 기반 있음 | 연결 | 뷰 이동 경로가 있다. | [D27](#d27) |
| rotate-view | 부분 | 미연결 | 회전 viewport와 입력 역변환이 필요하다. | [D09](#d09) |
| Current color · LVRS default | 기반 있음 | 연결 | 공유 색상 선택을 사용한다. | [D26](#d26) |
| quick-mask | 부분 | 미연결 | 활성 선택/마스크 편집 채널 계약이 필요하다. | [D01](#d01) [D06](#d06) [D17](#d17) |
| imagezoomIn | 기반 있음 | 연결 | 뷰 확대 경로가 있다. | [D27](#d27) |
| rulers | 기반 있음 | 표시만 연결 | 토글은 되지만 숫자가 현재 viewport에 연결되지 않는다. | [D27](#d27) |
| settings | 기반 있음 | 연결 | 환경설정 창 진입. 모든 전문 도구 옵션이 정의된 것은 아니다. | [D33](#d33) |
| symmetry-paint | 기반 있음 | 부분 연결 | SDK는 여러 대칭 모드, 앱은 수직 대칭 토글만 제공한다. | [D24](#d24) [D33](#d33) |

### 상단 명령

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| generalopen | 기반 있음 | 연결 | 기본 파일·이력·환경설정 동작이 있다. | [D33](#d33) |
| generalsave | 기반 있음 | 연결 | 기본 파일·이력·환경설정 동작이 있다. | [D33](#d33) |
| generalundo | 기반 있음 | 연결 | 기본 파일·이력·환경설정 동작이 있다. | [D28](#d28) |
| generalredo | 기반 있음 | 연결 | 기본 파일·이력·환경설정 동작이 있다. | [D28](#d28) |
| Free transform | 부분 | 부분 연결 | 선택 도구로 전환하지만 자유 변형 핸들/원근/다중 선택은 미완료이다. | [D09](#d09) [D33](#d33) |
| align-left | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| align-center | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| align-right | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| align-top | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| align-middle | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| align-bottom | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| distribute-horizontal | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| distribute-vertical | 변환 기반 있음 | 미연결 | 다중 선택과 정렬 기준·분배 작업이 필요하다. | [D09](#d09) |
| flip-horizontal | 기반 있음 | 연결 | 선택 레이어의 문서 변형 명령이다. | [D09](#d09) |
| flip-vertical | 기반 있음 | 연결 | 선택 레이어의 문서 변형 명령이다. | [D09](#d09) |
| rotate-left | 기반 있음 | 연결 | 선택 레이어의 문서 변형 명령이다. | [D09](#d09) |
| rotate-right | 기반 있음 | 연결 | 선택 레이어의 문서 변형 명령이다. | [D09](#d09) |
| perspective-grid | 일부 기반 있음 | 미연결 | 문서 좌표·가이드·스냅 계약이 필요하다. | [D27](#d27) |
| symmetry-paint | 기반 있음 | 부분 연결 | SDK는 여러 대칭 모드, 앱은 수직 대칭 토글만 제공한다. | [D24](#d24) [D33](#d33) |
| grid | 일부 기반 있음 | 토글만 연결 | 문서 좌표·가이드·스냅 계약이 필요하다. | [D27](#d27) |
| rulers | 기반 있음 | 표시만 연결 | 토글은 되지만 숫자가 현재 viewport에 연결되지 않는다. | [D27](#d27) |
| snap | 일부 기반 있음 | 미연결 | 문서 좌표·가이드·스냅 계약이 필요하다. | [D27](#d27) |
| sRGB / 16-bit | 부족 | 표시 불일치 | Figma의 16비트를 현재 8비트 문서 경로로 재현할 수 없다. | [D02](#d02) |
| color-picker | 기반 있음 | 부분 연결 | 스포이트 경로는 있으나 현재 프레임·샘플 대상·프로파일 정책이 필요하다. | [D02](#d02) [D26](#d26) |
| generalexport | 부분 | 부분 연결 | 현재 정지 프레임/8비트 출력과 애니메이션·오디오 범위는 다르다. | [D02](#d02) [D18](#d18) [D20](#d20) |
| settings | 기반 있음 | 연결 | 기본 파일·이력·환경설정 동작이 있다. | [D33](#d33) |

### Tool 파라미터

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| Size | 기반 있음 | 연결 | Figma 64 px / 1 px..1024 px; brush.size; 로그 슬라이더와 1..1024px. SDK의 더 큰 상한과 제품 상한은 구분한다. | [D24](#d24) |
| Hardness | 기반 있음 | 연결 | Figma 85% / 0%..100%; brush.hardness; 0..1로 변환된다. 픽셀 기준의 가장자리 검증이 필요하다. | [D24](#d24) |
| Opacity | 기반 있음 | 연결 | Figma 100% / 0%..100%; brush.opacity; 브러시 누적 상한과 legacy Fill opacity는 다른 경로이다. | [D01](#d01) [D24](#d24) |
| Flow | 기반 있음 | 연결 | Figma 90% / 0%..100%; brush.flow; dab별 잉크량이다. | [D24](#d24) |
| Roundness | 기반 있음 | 연결하되 범위 불일치 | Figma 100% / 0%..100%; shape.roundness; Figma 최소 0%, 앱/엔진 최소 1%이다. | [D24](#d24) |
| Angle | 기반 있음 | 연결 | Figma 0° / −180°..180°; shape.angleRadians; 고정 각도. 펜 축 회전 입력 연결과 다르다. | [D03](#d03) [D24](#d24) |
| Spacing | 기반 있음 | 연결 | Figma 12% / 0%..100%; spacingRatio; 양의 절대 spacing 값이 있으면 ratio보다 우선한다. | [D24](#d24) |
| Scatter | 기반 있음 | 연결 | Figma 5% / 0%..100%; material.scatter.radius; 크기 대비 반경으로 매핑한다. 개수 변동과는 별개이다. | [D24](#d24) |
| Bristle count | 기반 있음 | 조건부 | Figma 1 / 1..64; material.bristle.count; bristle-brush 모드에서만 활성이다. | [D03](#d03) [D24](#d24) |
| Stabilization | 기반 있음 | 부분 연결 | Figma 18 / 0..100; rasterizer.smoothing; 지수/당김줄 방식과 실제 입력 시간의 의미를 맞춰야 한다. | [D03](#d03) [D05](#d05) [D24](#d24) |
| Post correction | 기반 있음 | 다른 의미로 연결 | Figma 4 / 0..20; smoothingRadius/catchUp; 사후 곡선 보정보다 당김줄 반경·따라잡기로 사용한다. | [D05](#d05) [D24](#d24) |
| Pressure sensitivity | 기반 있음 | 부분 연결 | Figma 85% / 0%..100%; dynamics.pressureToSize/Flow; Opacity는 별도의 압력 곡선 경로로 제어한다. | [D03](#d03) [D24](#d24) |
| Tilt sensitivity | 기반 있음 | 입력 단절 | Figma 30% / 0%..100%; dynamics.tiltToEllipse; 설정은 전달되지만 BitmapEditor 입력에 실제 기울기가 없다. | [D03](#d03) [D24](#d24) |
| Velocity response | 기반 있음 | 입력 시간 부정확 | Figma 10% / 0%..100%; dynamics.velocityToOpacity; 이벤트마다 1/120초를 만들므로 실제 속도와 다를 수 있다. | [D03](#d03) [D05](#d05) [D24](#d24) |
| Wetness | 기반 있음 | 조건부와 샘플러 부족 | Figma 20% / 0%..100%; simulation.wetness; mixer/smudge에 한정되며 소스 픽셀 샘플러가 빠져 있다. | [D04](#d04) [D24](#d24) |
| Mix | 기반 있음 | 조건부와 샘플러 부족 | Figma 45% / 0%..100%; simulation.mixStrength; 실제 배경색 혼합 경로가 필요하다. | [D04](#d04) [D24](#d24) |
| Load | 기반 있음 | 조건부 | Figma 80% / 0%..100%; simulation.load; 시뮬레이션의 자국 커버리지에 영향을 주며 물감 잔량 모델인지 명세가 필요하다. | [D04](#d04) [D24](#d24) |
| Dilution | 기반 있음 | 의미 결정 필요 | Figma 12% / 0%..100%; simulation.pickup/deposit; pickup=v, deposit=1-v로 매핑한다. | [D04](#d04) [D24](#d24) |
| Start | 기반 있음 | 연결하되 단위 불명확 | Figma 15% / 0%..100%; warmupDistance; brush.size×Start%×Taper%의 문서 픽셀 거리이다. | [D24](#d24) |
| End | 후단 계약 부족 | 미소비 | Figma 30% / 0%..100%; 해당 매핑 없음; 표시·저장되지만 brush()에서 읽지 않는다. | [D24](#d24) |
| Minimum size | 기반 있음 | 조건부 | Figma 8% / 0%..100%; taperMinimum; 현재 시작 taper에 적용된다. 후단 taper 구현과 별개이다. | [D24](#d24) |
| Taper strength | 기반 있음 | 부분 연결 | Figma 35% / 0%..100%; warmupDistance/warmupTaperShape; 현재 시작 거리와 SmoothStep에 반영한다. | [D24](#d24) |

### 타임라인/레이어

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| 레이어 추가·복제·삭제 | 기반 있음 | 정적 레이어 연결 | 정적 레이어 CRUD, animated 복제는 거부한다. | [D18](#d18) |
| 레이어 이름·가시성·순서 | 기반 있음 | 연결 | 현재 레이어 목록에 연결되어 있다. | [D16](#d16) |
| 레이어 opacity | 기반 있음 | 연결 | 문서 속성 편집과 저장 경로가 있다. | [D23](#d23) |
| 전체 혼합 모드 | 기반 있음 | 일부만 연결 | 현재 UI가 Normal/Multiply/Screen만 구별한다. | [D23](#d23) |
| 그룹과 Pass | 정규 타입 부족 | 비활성 | 비중첩 아트보드와 계층형 레이어 그룹은 다르다. | [D15](#d15) |
| 래스터·벡터·필터 마스크 | 기반 있음 | 비활성 | mask stack과 렌더링 기반을 활용할 수 있다. | [D17](#d17) |
| 클리핑 마스크 | 기반 있음 | 비활성 | clippedToBelow와 합성 테스트는 있다. | [D17](#d17) |
| 잠금 | 정규 정책 부족 | 비활성 | 잠금 종류·보존·우회 편집 방지를 정의해야 한다. | [D16](#d16) |
| 레이어 링크 | 정규 정책 부족 | 비활성 | 링크 집합과 변형/삭제 의미가 필요하다. | [D16](#d16) |
| 콘텐츠 키프레임 추가·삭제 | 기반 있음 | 비활성 | Frame/KeyframedSource는 있으나 편집 세션을 연결해야 한다. | [D18](#d18) |
| 모션 키프레임 추가·삭제 | 기반 있음 | 비활성 | MotionValue와 setLayerMotion은 존재한다. | [D18](#d18) |
| 어니언 스킨 | 렌더링 기반 있음 | 비활성 | 여러 프레임 렌더링의 틴트/캐시 정책이 필요하다. | [D19](#d19) |
| 이전/다음 프레임·시간 스크럽 | 기반 있음 | 연결 | 프레임 경계 이동과 시간 표시가 있다. | [D18](#d18) |
| 재생 | 기반 있음 | 기본 연결 | Core 프레임 루프 기반. 오디오 동기 재생과 별개이다. | [D20](#d20) |
| 초 단위 눈금·프레임 틱 | 기반 있음 | 연결 | 초 표시와 프레임 경계 이동은 구현되어 있다. | [D33](#d33) |
| 틱의 프레임 레이트 변경 | 유리수 기반 있음 | 정수 레이트만 연결 | 문서는 numerator/denominator, 앱은 1..240 정수로 제한한다. | [D18](#d18) [D33](#d33) |
| 타임라인 확대·위치 슬라이더 | 기반 있음 | 연결 | 서로 다른 zoom/position 동작을 제공한다. | [D33](#d33) |
| 타임라인 스냅 | 일부 기반 있음 | 비활성 | 기본 프레임 양자화 외 키/클립 스냅 정책은 미정이다. | [D18](#d18) [D27](#d27) |
| 패널 스택·더보기 | 기반 있음 | 제한 연결 | 현재 stack 버튼은 스크롤 복귀, menu는 Doc 탭 이동이다. | [D33](#d33) |
| Audio 행·음성 클립 | 모델 있음/네이티브 코덱 부족 | 미연결 | 실제 오디오 트랙 투영·파형·재생 시계가 필요하다. | [D20](#d20) [D31](#d31) |
| Camera 행·Object 표시 | 의미와 타입 부족 | 미연결 | 2D 카메라/뷰 북마크 의미부터 정해야 한다. | [D21](#d21) [D33](#d33) |
| Color balance 조정 레이어 | 커널 있음/정규 타입 부족 | 미연결 | 아래 스택에 작용하는 독립 보정 노드가 필요하다. | [D22](#d22) |

### 뷰/패널

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| 확대·축소·맞춤·팬 | 기반 있음 | 연결 | 뷰 탐색은 문서 리비전과 분리되어 있다. | [D27](#d27) |
| 뷰 각도·회전 | 부분 | 읽기 전용/미연결 | 현재 0.0° 입력은 read-only, rotate 아이콘은 fit/reset이다. | [D09](#d09) |
| 캔버스 하단 반전 | 기반 있음 | 의미 확인 필요 | 현재 선택 레이어의 문서 변형이다. 뷰 미러를 의도했는지 확정해야 한다. | [D09](#d09) [D33](#d33) |
| Tool Doc View More 세그먼트 | UI 기반 있음 | 기본 연결 | 전환은 가능하나 Figma의 나머지 탭 세부 동작 사양은 부족하다. | [D33](#d33) |
| HSB 필드·색조·알파 | 기반 있음 | 연결 | 기본 HSB와 투명도는 공유 모델을 사용한다. | [D02](#d02) [D26](#d26) |
| 색상 모드 선택 | 기반 있음 | 드롭다운 동작 미연결 | HSB ComboBox에 선택 콜백이 없다. | [D26](#d26) |
| Tool 그룹·압력 토글·복제 소스 | 일부 기반 있음 | 의미/연결 부분 | 일부 버튼은 스크롤 이동, clone source는 미지원 도구 선택을 시도한다. | [D10](#d10) [D24](#d24) [D33](#d33) |
| 파라미터 리셋·저장·프리셋 | 기반 있음 | 수치만 연결 | 22개 값의 INI 저장과 완전한 자원 소유 BrushPreset은 다르다. | [D25](#d25) |
| 스크롤·키보드·포커스·접근성 | 부분 지원 | 대상별 검증 필요 | 현재 컨트롤의 상태·가상 행·IME·터치 검증을 분리해야 한다. | [D32](#d32) [D34](#d34) |

### 기반 품질

| 항목 | SDK | 앱 연결 | 판정/남은 작업 | 부족 ID |
|---|---|---|---|---|
| GPU 기반 표시와 문서 처리 | 표시 있음/문서 GPU 처리 부족 | 표시 연결 | Metal 표시가 GPU 브러시/필터/합성을 뜻하지 않는다. | [D29](#d29) |
| 큰 문서 Undo와 비동기 미리보기 | 부분 | 제한 연결 | 이력·ROI·취소·세대 가드의 기능 수락 기준이 필요하다. | [D28](#d28) [D30](#d30) |

## 구현을 진행할 때의 소유권과 순서

| 책임 | 주 소유자 | 유지할 경계 |
|---|---|---|
| 픽셀 브러시·입력·dab·선택 커널 | iiPaintEngine | 벡터 문서와 상위 앱을 참조하지 않는다. 입력 처리와 문서 저장 정책을 분리한다. |
| 정규 레이어·마스크·그룹·텍스트·프레임·직렬화 | iiSharedCanvas | 검증 가능한 재사용 문서 의미를 소유하고 FileProvider에 파일 I/O를 위임한다. |
| 보정·힐링·워프 이미지 커널 | iiGraphicAdjust | 새 이미지를 반환하는 독립 커널과 제품 편집 세션을 구분한다. |
| 창·입력·GPU 표시·플랫폼 경계 | iiGUI | 문서 편집 의미를 위젯이나 운영체제 이벤트 객체에 저장하지 않는다. |
| 화면 구성·스타일·위젯 | LVUI/LVRS | 완료된 capability에 맞춰 컨트롤을 활성화한다. |
| 편집 도구 세션·명령·표시 정책 | 기존 앱 어댑터 또는 재사용 편집 계층 | 하위 모듈을 인터페이스로 조합하고 순환 의존성을 만들지 않는다. |
| 스케줄링·취소·수명 | iiSDKCore | 캔버스/브러시/파일 형식의 의미를 Core로 흡수하지 않는다. |

1. D01의 정확성과 D03/D04/D05의 입력·샘플러·시계를 먼저 정리하고, D24의 무효 파라미터를 지원/비지원으로 명시한다.
2. D02의 정밀도·색상·메타데이터와 D15/D14/D16/D22의 문서 타입을 확정한다. 파일 호환성에 영향을 주므로 도구별 임시 필드로 우회하지 않는다.
3. 이미 존재하는 선택·마스크·벡터·리터치·워프·키프레임 API를 가역 편집 세션으로 연결한다. 같은 알고리즘을 제품마다 복사하지 않는다.
4. Audio/Camera의 실제 의미와 네이티브 미디어 경계를 구현하고, D28/D29/D30의 이력·실제 가속·응답성 예산을 검증한다.
5. 각 단계마다 Qt 없는 installed consumer, 파일 왕복, 독립 픽셀/입력 검사를 통과한 기능만 앱에서 완료로 표시한다. 새 모듈이 필요하다면 제품별 명칭이 아닌 재사용 가능한 편집 책임을 기준으로 분리한다.

## 증거와 재현

Figma: [Main](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=83-6973), [전체 Tool](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=130-1832). Figma의 프로토타입 reaction은 UI 상호작용 정의이며 SDK 엔진·파일 저장·기기 실행 증거로 계산하지 않았다.

생성 기록은 `build/sdk-capability-audit-20261026/`에 있다. `figma-inventory.json`, `figma-parameters.json`, `figma-navigation.json`은 원본 기능 목록이고 `module-snapshot.json`, `source-evidence.json`은 코드/패키지 기준점이다. `probe/`는 실행 진단의 소스와 단일 일반 실행 파일이며 기존 앱 빌드 트리는 재구성하지 않았다.

```sh
python3 build/sdk-capability-audit-20261026/write_report.py
python3 build/sdk-capability-audit-20261026/validate_report.py
```

첫 명령은 캡처한 원본 목록과 현재 소스 앵커를 검사하여 보고서/대조표를 재생성한다. 두 번째는 대조표의 각 값, 내부 링크, 원본 해시, 파라미터 목록과 진단 결과의 일치를 검증한다. 엔진 진단의 구성은 `probe/CMakeLists.txt`, configure·build·실행 출력은 `probe-configure.log`, `probe-build.log`, `probe-run.log` 및 `probe-loader.log`에 있다. 플랫폼 전체 테스트를 이번에 다시 실행했다고 해석하지 않는다.

## 소스 근거 목록

<a id="e01"></a>
- **E01** [Product/Congregation/App/Native/Application.cpp:522](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Application.cpp:522) — `void select_native_tool`

<a id="e02"></a>
- **E02** [Product/Congregation/App/Native/Application.cpp:652](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Application.cpp:652) — `void apply_brush`

<a id="e03"></a>
- **E03** [Product/Congregation/App/Native/ToolSettings.cpp:152](/Volumes/Storage/Workspace/Product/Congregation/App/Native/ToolSettings.cpp:152) — `iiSharedCanvas::BitmapBrush ToolSettings::brush`

<a id="e04"></a>
- **E04** [Product/Congregation/App/Native/CanvasView.cpp:350](/Volumes/Storage/Workspace/Product/Congregation/App/Native/CanvasView.cpp:350) — `if (tool == "text")`

<a id="e05"></a>
- **E05** [Product/Congregation/App/Native/TimelineView.cpp:93](/Volumes/Storage/Workspace/Product/Congregation/App/Native/TimelineView.cpp:93) — `good(b.icon(transport, "onion-skin"`

<a id="e06"></a>
- **E06** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:645](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:645) — `m_strokeTime += 1.0 / 120.0`

<a id="e07"></a>
- **E07** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:662](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:662) — `const std::vector<RasterSample> samples = projectBrushDabs`

<a id="e08"></a>
- **E08** [SDK/iiPaintEngine/src/Stroke/Rasterizer.cpp:141](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Stroke/Rasterizer.cpp:141) — `std::uint32_t wetDabColorArgb`

<a id="e09"></a>
- **E09** [SDK/iiPaintEngine/src/Application/BitmapPaintSession.h:35](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Application/BitmapPaintSession.h:35) — `BitmapPaintUpdate processPointerEvent`

<a id="e10"></a>
- **E10** [SDK/iiPaintEngine/src/Brush/BrushSettings.h:19](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Brush/BrushSettings.h:19) — `struct BrushStrokeSettings`

<a id="e11"></a>
- **E11** [SDK/iiSharedCanvas/src/Document/Document.h:318](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Document/Document.h:318) — `struct LayerProperties`

<a id="e12"></a>
- **E12** [SDK/iiSharedCanvas/src/Document/LayerAppearance.h:53](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Document/LayerAppearance.h:53) — `struct LayerAppearance`

<a id="e13"></a>
- **E13** [SDK/iiSharedCanvas/src/Document/DocumentEditor.h:230](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Document/DocumentEditor.h:230) — `DocumentEditResult setLayerAppearance`

<a id="e14"></a>
- **E14** [SDK/iiSharedCanvas/src/Vector/VectorEditor.h:65](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Vector/VectorEditor.h:65) — `DocumentEditResult setAnchorPoint`

<a id="e15"></a>
- **E15** [SDK/iiSharedCanvas/src/Bitmap/BitmapProcessing.h:35](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapProcessing.h:35) — `RasterMaskResult floodMask`

<a id="e16"></a>
- **E16** [SDK/iiPaintEngine/src/Selection/Selection.cpp:54](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Selection/Selection.cpp:54) — `Types::Scalar selectionAlphaAt`

<a id="e17"></a>
- **E17** [SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:82](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:82) — `void applyFill`

<a id="e18"></a>
- **E18** [SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:93](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:93) — `void applyGradient`

<a id="e19"></a>
- **E19** [SDK/iiPaintEngine/src/Render/GpuRenderer.cpp:39](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Render/GpuRenderer.cpp:39) — `result.layer = compositeLayerStack`

<a id="e20"></a>
- **E20** [SDK/iiImageCodec/src/iiImageCodec/ImageCodec.h:13](/Volumes/Storage/Workspace/SDK/iiImageCodec/src/iiImageCodec/ImageCodec.h:13) — `enum class PixelFormat`

<a id="e21"></a>
- **E21** [SDK/iiSharedCanvas/src/Native/PsdParser.cpp:303](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Native/PsdParser.cpp:303) — `if (depth != 8 || mode != 3)`

<a id="e22"></a>
- **E22** [SDK/iiSharedCanvas/cmake/iiSharedCanvasNative.cmake:9](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/cmake/iiSharedCanvasNative.cmake:9) — `add_library(iiSharedCanvasNative STATIC`

<a id="e23"></a>
- **E23** [SDK/iiSharedCanvas/src/Document/Document.h:470](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Document/Document.h:470) — `struct AudioTrackLayer`

<a id="e24"></a>
- **E24** [SDK/iiGraphicAdjust/src/iiGraphicAdjust/RetouchFilters.h:51](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/src/iiGraphicAdjust/RetouchFilters.h:51) — `Result<Image> cloneSpot`

<a id="e25"></a>
- **E25** [SDK/iiGraphicAdjust/src/iiGraphicAdjust/MeshFilters.h:49](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/src/iiGraphicAdjust/MeshFilters.h:49) — `Result<Image> liquify`

<a id="e26"></a>
- **E26** [SDK/iiGraphicAdjust/src/iiGraphicAdjust/Image.h:19](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/src/iiGraphicAdjust/Image.h:19) — `struct ImageView`

<a id="e27"></a>
- **E27** [SDK/iiColorManager/src/iiColorManager/ColorManager.h:15](/Volumes/Storage/Workspace/SDK/iiColorManager/src/iiColorManager/ColorManager.h:15) — `struct Cmyk`

<a id="e28"></a>
- **E28** [SDK/iiGUI/src/iiGUI/ColorPicker.h:27](/Volumes/Storage/Workspace/SDK/iiGUI/src/iiGUI/ColorPicker.h:27) — `class IIGUI_EXPORT ColorPickerModel`

<a id="e29"></a>
- **E29** [SDK/iiGUI/src/iiGUI/NativeWidgets.h:315](/Volumes/Storage/Workspace/SDK/iiGUI/src/iiGUI/NativeWidgets.h:315) — `struct WidgetInput final`

<a id="e30"></a>
- **E30** [SDK/iiGUI/src/iiGUI/NativeWidgetsIOS.mm:51](/Volumes/Storage/Workspace/SDK/iiGUI/src/iiGUI/NativeWidgetsIOS.mm:51) — `- (void)sample:`

<a id="e31"></a>
- **E31** [SDK/iiGUI/docs/IPADOS.md:17](/Volumes/Storage/Workspace/SDK/iiGUI/docs/IPADOS.md:17) — `UIKeyInput 경로는`

<a id="e32"></a>
- **E32** [SDK/LVRS/docs/native/Parity.md:3](/Volumes/Storage/Workspace/SDK/LVRS/docs/native/Parity.md:3) — `기준은 현재 QML`

<a id="e33"></a>
- **E33** [SDK/LVUI/README.md:65](/Volumes/Storage/Workspace/SDK/LVUI/README.md:65) — `Dynamic composition and the full lifecycle remain incomplete`

<a id="e34"></a>
- **E34** [SDK/iiSocietyHelper/CMakeLists.txt:35](/Volumes/Storage/Workspace/SDK/iiSocietyHelper/CMakeLists.txt:35) — `find_package(Qt6`

<a id="e35"></a>
- **E35** [Product/Congregation/App/Native/Artwork.cpp:16](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Artwork.cpp:16) — `constexpr std::size_t HistoryBudget`

<a id="e36"></a>
- **E36** [Product/Congregation/App/Native/Artwork.cpp:60](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Artwork.cpp:60) — `limits.maximumContainerBytes = HistoryBudget / 2`

<a id="e37"></a>
- **E37** [Product/Congregation/App/Native/WorkspaceView.cpp:18](/Volumes/Storage/Workspace/Product/Congregation/App/Native/WorkspaceView.cpp:18) — `void ruler(`

<a id="e38"></a>
- **E38** [Product/Congregation/App/Native/Application.cpp:609](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Application.cpp:609) — `void workspace_command`

<a id="e39"></a>
- **E39** [Product/Congregation/App/Native/EditorBackend.cpp:66](/Volumes/Storage/Workspace/Product/Congregation/App/Native/EditorBackend.cpp:66) — `void EditorBackend::choose_preset`

<a id="e40"></a>
- **E40** [Product/Congregation/App/Native/Artwork.cpp:367](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Artwork.cpp:367) — `Animated layer duplication needs an explicit frame`

<a id="e41"></a>
- **E41** [SDK/iiSharedCanvas/docs/NATIVE.md:30](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/docs/NATIVE.md:30) — `네이티브와 기존 Qt 패키지는`

<a id="e42"></a>
- **E42** [SDK/iiSharedCanvas/src/Render/FrameRenderer.h:37](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Render/FrameRenderer.h:37) — `struct FrameRenderTileRequest`

<a id="e43"></a>
- **E43** [SDK/iiSharedCanvas/src/Render/FrameRenderer.cpp:776](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Render/FrameRenderer.cpp:776) — `RasterProcessResult filterLayer`

<a id="e44"></a>
- **E44** [SDK/iiGUI/src/iiGUI/Graphics.h:91](/Volumes/Storage/Workspace/SDK/iiGUI/src/iiGUI/Graphics.h:91) — `struct ContentTransform final`

<a id="e45"></a>
- **E45** [SDK/iiPaintEngine/src/Transform/ViewportTransform.h:6](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Transform/ViewportTransform.h:6) — `struct RasterViewport`

<a id="e46"></a>
- **E46** [Product/Congregation/App/Native/ToolSettings.cpp:13](/Volumes/Storage/Workspace/Product/Congregation/App/Native/ToolSettings.cpp:13) — `constexpr ParameterSpec parameters`

<a id="e47"></a>
- **E47** [Product/Congregation/CMakeLists.txt:47](/Volumes/Storage/Workspace/Product/Congregation/CMakeLists.txt:47) — `find_package(iiSharedCanvasNative`

<a id="e48"></a>
- **E48** [SDK/iiSharedCanvas/src/Native/BitmapCodec.cpp:29](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Native/BitmapCodec.cpp:29) — `Print resolution is not retained`

<a id="e49"></a>
- **E49** [SDK/iiSharedCanvas/src/Document/DocumentEditor.h:123](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Document/DocumentEditor.h:123) — `DocumentEditResult setLayerMotion`

<a id="e50"></a>
- **E50** [SDK/iiPaintEngine/tests/EditingToolPipelineContractTest.cpp:50](/Volumes/Storage/Workspace/SDK/iiPaintEngine/tests/EditingToolPipelineContractTest.cpp:50) — `GradientOperation gradient`

<a id="e51"></a>
- **E51** [SDK/iiSharedCanvas/tests/MaskStackTest.cpp:46](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/tests/MaskStackTest.cpp:46) — `setLayerAppearance(`

<a id="e52"></a>
- **E52** [Product/Congregation/App/Native/WorkspaceView.cpp:350](/Volumes/Storage/Workspace/Product/Congregation/App/Native/WorkspaceView.cpp:350) — `colorTitle, "color-space"`

<a id="e53"></a>
- **E53** [Product/Congregation/App/Native/TimelineView.cpp:274](/Volumes/Storage/Workspace/Product/Congregation/App/Native/TimelineView.cpp:274) — `void TimelineView::refresh_document`

<a id="e54"></a>
- **E54** [SDK/iiPaintEngine/src/Stroke/StrokePoint.h:13](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Stroke/StrokePoint.h:13) — `struct StrokePoint`

<a id="e55"></a>
- **E55** [SDK/iiGraphicAdjust/docs/RetouchFilters.md:29](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/docs/RetouchFilters.md:29) — `it does not interpolate missing brush points`

<a id="e56"></a>
- **E56** [SDK/iiPaintEngine/src/Brush/BrushMaterial.h:57](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Brush/BrushMaterial.h:57) — `struct BrushTexture {`

<a id="e57"></a>
- **E57** [SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.cpp:568](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.cpp:568) — `dabs, state.rasterizer, RasterProjection{}, state.material)`

<a id="e58"></a>
- **E58** [SDK/iiGraphicAdjust/src/iiGraphicAdjust/SampleFilters.h:48](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/src/iiGraphicAdjust/SampleFilters.h:48) — `sampleAverage16(`

<a id="e59"></a>
- **E59** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:733](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:733) — `struct TimelineTrackProperties`

<a id="e60"></a>
- **E60** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:585](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:585) — `struct TimelineAudioMix`

<a id="e61"></a>
- **E61** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:630](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:630) — `struct TimelineSubtitleClip`

<a id="e62"></a>
- **E62** [SDK/iiSharedCanvas/docs/AUDIO_TIMELINE.md:5](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/docs/AUDIO_TIMELINE.md:5) — ``별도의 `TimelineProject` 모델과 독립적인``

