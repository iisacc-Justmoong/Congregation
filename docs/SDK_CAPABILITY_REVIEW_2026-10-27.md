# Congregation 현행 Figma 재현을 위한 iiSDK 역량 재검토 보고서

기준일은 **2026-10-27**이다. [Main](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=83-6973)과 [전체 Tool](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=130-1832)을 실시간으로 다시 읽고, SDK 공개 API·구현·설치 패키지·현재 App 연결을 대조한 결과이다. [2026-10-26 감사](SDK_CAPABILITY_AUDIT_2026-10-26.md)는 당시의 기록으로 보존한다.

## 결론

**현재 iiSDK의 모듈 구성은 창과 기본 회화 편집을 만들기에는 충분하지만, 현재 Figma의 모든 도구와 기능을 재현하기에는 아직 충분하지 않다.** 부족의 중심은 모듈의 개수보다 정규 회화 문서 개념, 기존 커널을 묶는 가역 편집 세션, 새 SDK 계약의 앱 채택과 플랫폼별 공급이다.

기존 **34개 추적 항목**을 유지하여 재평가했다. 그중 **D01·D03·D04·D05·D24의 5개는 모듈 및 설치 소비자 검증을 완료한 기록이 있으며**, 이를 SDK에 아직 없는 기능이라고 다시 나열하지 않았다. **D28은 공통 영역 이력 기반만 완료**되었고 Native 편집기·파일·구조 명령·앱 이력 전환은 남아 있다. 다른 28개 항목과 위 항목들의 제품 채택·수락 검증도 남아 있으므로 이를 제품 완료율로 환산하지 않는다.

실시간 Figma에서 좌측 도구 슬롯 **49개**, 상단 명령 **26개**, 확장 Tool 파라미터 **22개**를 재확인했다. 도구 레일의 이름은 여전히 “44 original native LVRS controls”이지만 실제 액션 슬롯은 49개이며 구분선 4개는 제외했다. 상세 대조표는 **130행**, 직접 참조하는 고유 Figma 노드는 **101개**이다. 101개 모두 현재 파일에 존재하며 도구/명령 이름과 파라미터 라벨을 대조했다. 반복 위치를 포함한 행 수이므로 고유 알고리즘 수나 기능 완료율이 아니다.

## 검토 범위와 판단 규칙

- 실제 창은 `83:6978`의 **1999 × 1080**, 외곽 설명 프레임은 `83:6973`의 **1999 × 1120**이다. 전체 Tool은 `130:1832`의 **352 × 2110**이다.
- Main의 세로 도구 레일, 상단 아이콘 명령, 시간 단위 눈금/프레임 틱/레이어 통합 뷰, 우측 Tool·Doc·View·More와 확장 22개 파라미터가 범위이다. Photoshop/Clip Studio 전체 기능을 추가 요구로 확대하지 않는다.
- **모듈 기반 있음**은 공개 타입·커널을 확인한 상태이다. **세션 부족**은 입력·미리보기·확정/취소·Undo·문서 저장으로 연결되지 않은 상태이다. **정규 타입 부재**는 검토한 회화 `Document`의 값 모델·직렬화·렌더러에서 해당 의미를 직접 표현하지 못한다는 뜻이다.
- **앱 미연결**과 **키트 미공급**을 SDK 알고리즘 부재와 분리했다. 선언/주석/모듈 이름만으로 지원 완료를 판단하지 않았다. **API 미확인**은 명시한 공개 범위에서 계약을 확인하지 못한 상태이며 모든 내부 수학 코드의 부재를 주장하지 않는다.
- Figma가 정하지 않은 Camera/Object·Pass·도구 모드 등의 의미는 **미정**으로 분류했다. 보이는 아이콘이나 prototype reaction을 실제 기능 검증으로 계산하지 않았다.
- 이번 작업은 검토와 보고서 작성이다. SDK·앱 소스, Figma 레이어, 기존 앱 빌드 구성은 수정하지 않았다. 전체 SDK 재빌드와 앱/기기 재설치는 수행하지 않았다.

## 현재 모듈·키트의 준비 상태

소스 CMake, 앱 키트의 실제 ConfigVersion, 별도 보강 스테이지를 분리해 읽었다. “없음”은 검토한 키트 안에 해당 CMake 패키지가 없다는 뜻이며 전체 호스트의 모듈 부재가 아니다. 주 앱의 `build/CMakeCache.txt`는 현재 iOS와 `congregation-ipados/prefix`를 가리킨다.

| 모듈 | 소스 버전 | macOS 앱 키트 | iPad 앱 키트 | 현행 판단 |
|---|---:|---:|---:|---|
| iiPaintEngine | 0.5.4 | 0.5.1 | 0.5.1 | 선택·채우기·혼색 수정과 공통 영역 이력은 새 스테이지에 있음; 앱 키트 구버전 |
| iiSharedCanvas | 0.33.2 | 0.32.0 | 0.32.0 | 입력·샘플러·시계는 Native 0.33.2에 있음; 앱은 0.32.0, 정규 문서/이력 보강 남음 |
| iiBrushTools | 0.1.0 | 없음 | 없음 | 22개 값·전체 획 taper 작성 계약 있음; 앱 링크·미리보기·다중 타일 커밋 미채택 |
| iiGraphicAdjust | 0.6.1 | 없음 | 없음 | 리터치·Liquify·Warp·보정 커널 있음; 앱 세션/키트 연결 부족 |
| iiGUI | 0.4.1 | 0.4.1 | 0.4.1 | 네이티브 창·GPU 표시·펜 입력·텍스트 기반; 뷰 회전·대상별 입력 검증 필요 |
| LVUI | 0.1.0 | 0.1.0 | 0.1.0 | 선언적 뷰·Color/Metric 있음; 동적 구성·전체 수명 일부 미완료 |
| LVRS | 1.0.1 | 1.0.1 | 1.0.1 | 현재 기본 컨트롤 사용 가능; 네이티브 parity는 partial |
| iiVisualCodec | 0.4.0 | 없음 | 없음 | iiImageCodec 후속; 호환 target 제공. Image는 8비트; 새 부가 기능을 회화 문서 지원으로 혼동하지 않음 |
| iiColorManager | 0.1.1 | 없음 | 없음 | HSB/RGB/CMYK/Lab 등 색 선택·변환 있음; 앱 색 상태/모드 연결 필요 |
| iiDeviceMultimedia | 0.2.1 | 없음 | 없음 | 장치 PCM I/O 기반; 회화 타임라인의 오디오 편집·재생 세션은 별도 |
| iiFileProvider | 0.8.0 | 0.9.0 | 0.9.0 | 표의 공급 버전은 Native 패키지 기준 0.9.0; 저장소/SQLite 기반이며 회화 의미는 Canvas 책임 |
| iiSDKCore | 0.20.0 | 0.18.1 | 0.20.0 | 실행·작업·취소·프레임 기반; 플랫폼 키트 버전 차와 ABI 일관성 확인 필요 |
| iiSocietyHelper | 0.11.0 | 없음 | 없음 | core facade에 Qt6 필수; 현재 네이티브 앱에 전체 facade를 연결할 수 없음 |

소스 `SDK/iiImageCodec/`이 없는 것은 이미지 코덱 전체의 소실이 아니다. 현행 소스는 `SDK/iiVisualCodec/`이며 `iiImageCodec::iiImageCodec`·기존 include·`ii::image`를 전달하는 호환 면이 있다. 현재 앱 키트의 `iiImageCodec 0.1.0` 표기만으로 새 iiVisualCodec 0.4.0과 바이너리·헤더가 같다고 판단하지 않는다. [E20](#e20) [E68](#e68) [E70](#e70)

별도 설치 스테이지는 Engine **0.5.4**, Native Canvas **0.33.2**, BrushTools **0.1.0**이다. 이는 현재 앱 번들이 이 버전을 사용한다는 뜻이 아니다. 같은 버전 문자열도 소스·헤더·바이너리 동일성을 보장하지 않으므로 증거 JSON에 경로·해시·HEAD·기존 변경 상태를 함께 담았다.

## 우선순위별 추적 목록

P0는 픽셀/문서 신뢰성과 현재 활성 파라미터의 실제 의미를 우선하는 분류이다. P1은 현재 화면 기능을 완성하는 도메인·세션·공급 작업이며 P2는 사용 컴포넌트와 대상 플랫폼으로 좁힐 입력/접근성 검증이다. 기존 ID와 우선순위는 보존했다.

| ID | 우선순위 | 부족 또는 남은 작업 | 현행 상태 |
|---|---|---|---|
| [D01](#d01) | P0 | 선택·채우기 공개 API의 수정본과 앱 키트의 버전 차이 | 모듈·설치 검증 완료 / 제품 채택 남음 |
| [D02](#d02) | P0 | Figma의 16비트 문서와 실제 8비트 sRGB 경로 | 미해결: 도메인 또는 연결 보강 필요 |
| [D03](#d03) | P0 | 고급 펜 입력 API의 Congregation 채택 | 모듈·설치 검증 완료 / 제품 채택 남음 |
| [D04](#d04) | P0 | Mixer·Smudge 샘플러 수정본의 앱 키트 채택 | 모듈·설치 검증 완료 / 제품 채택 남음 |
| [D05](#d05) | P0 | 정지 분사·안정화 시계의 앱 수명 연결 | 모듈·설치 검증 완료 / 제품 채택 남음 |
| [D06](#d06) | P1 | 영역 선택과 Quick Mask의 일관된 편집 계약 | 미해결: 도메인 또는 연결 보강 필요 |
| [D07](#d07) | P1 | 자기 올가미의 경계 추종 | 미해결: 도메인 또는 연결 보강 필요 |
| [D08](#d08) | P1 | 자르기와 원근 자르기 | 미해결: 도메인 또는 연결 보강 필요 |
| [D09](#d09) | P1 | 자유 변형과 정렬 및 비파괴 뷰 회전 | 미해결: 도메인 또는 연결 보강 필요 |
| [D10](#d10) | P1 | 복제 도장과 힐링 및 패턴 도장 | 미해결: 도메인 또는 연결 보강 필요 |
| [D11](#d11) | P1 | Blur Sharpen Dodge Burn Sponge 브러시 | 미해결: 도메인 또는 연결 보강 필요 |
| [D12](#d12) | P1 | Liquify와 Warp의 미리보기 및 비파괴 저장 | 미해결: 도메인 또는 연결 보강 필요 |
| [D13](#d13) | P1 | Pen Curvature Pen 제어점 편집 | 미해결: 도메인 또는 연결 보강 필요 |
| [D14](#d14) | P1 | 편집 가능한 영역 텍스트와 경로 텍스트 | 미해결: 도메인 또는 연결 보강 필요 |
| [D15](#d15) | P1 | 계층형 레이어 그룹과 Pass Through | 미해결: 도메인 또는 연결 보강 필요 |
| [D16](#d16) | P1 | 레이어 잠금과 레이어 링크 | 미해결: 도메인 또는 연결 보강 필요 |
| [D17](#d17) | P1 | 레이어 마스크와 클리핑 편집 워크플로우 | 미해결: 도메인 또는 연결 보강 필요 |
| [D18](#d18) | P1 | 프레임 노출과 모션 키프레임 편집 | 미해결: 도메인 또는 연결 보강 필요 |
| [D19](#d19) | P1 | 어니언 스킨 | 미해결: 도메인 또는 연결 보강 필요 |
| [D20](#d20) | P1 | Audio 트랙의 가져오기 재생 및 동기화 | 미해결: 도메인 또는 연결 보강 필요 |
| [D21](#d21) | P1 | Camera 트랙 | 제품 의미 확정 필요 |
| [D22](#d22) | P1 | Color balance 조정 레이어 | 미해결: 도메인 또는 연결 보강 필요 |
| [D23](#d23) | P1 | 혼합 모드 선택과 Pass Object 표시 | 미해결: 도메인 또는 연결 보강 필요 |
| [D24](#d24) | P0 | 22개 브러시 파라미터의 작성 계약과 앱 경로 불일치 | 모듈·설치 검증 완료 / 제품 채택 남음 |
| [D25](#d25) | P1 | 브러시 프리셋과 Textured oil의 자원 계약 | 미해결: 도메인 또는 연결 보강 필요 |
| [D26](#d26) | P1 | HSB 선택기와 공유 색상 상태 | 미해결: 도메인 또는 연결 보강 필요 |
| [D27](#d27) | P1 | 눈금자 가이드 원근 그리드와 스냅 | 미해결: 도메인 또는 연결 보강 필요 |
| [D28](#d28) | P0 | 공통 영역 이력 이후 남은 Native·파일·앱의 전체 복제 | 공통 이력만 완료 / Native·파일·앱 남음 |
| [D29](#d29) | P1 | GPU 표시와 문서 합성 가속의 구분 | 미해결: 도메인 또는 연결 보강 필요 |
| [D30](#d30) | P1 | 대형 필터 작업의 ROI 취소와 화면 반영 | 미해결: 도메인 또는 연결 보강 필요 |
| [D31](#d31) | P1 | Qt 없는 앱 키트·코덱 후속 패키지·ABI 공급 | 미해결: 도메인 또는 연결 보강 필요 |
| [D32](#d32) | P2 | LVRS 네이티브 동등성과 입력 접근성 | 사용 컴포넌트와 대상 플랫폼별 검증 필요 |
| [D33](#d33) | P1 | 아이콘과 표시값만으로 결정할 수 없는 동작 | 제품 의미 확정 필요 |
| [D34](#d34) | P1 | 화면 검사와 알고리즘 검사의 간극 | 기능 수락 범위 확장 필요 |

## 개념별 상세 검토

각 항목은 현재 확인한 기반, 부족한 책임, 담당 모듈과 수락 기준을 함께 제시한다. SDK 커널이 있는 경우 제품 안에서 같은 알고리즘을 복제하는 신규 모듈을 요구하지 않는다.

<a id="d01"></a>
### D01 · 선택·채우기 공개 API의 수정본과 앱 키트의 버전 차이

**P0 · 모듈 결함 해결 / 제품 키트 갱신 필요 · 담당: iiPaintEngine; iiSharedCanvas**

현재: iiPaintEngine 0.5.2에서 반전·feather·Fill opacity·부드러운 마스크 커버리지·Radial Gradient를 수정했으며 현행 소스 0.5.4에도 유지된다. 수정 단계의 전체 50/50·설치 소비자 검사와 후속 0.5.4 전체 52/52 기록이 있다. 반면 Congregation의 macOS/iPad 키트에서 읽은 버전은 여전히 0.5.1이다.

부족·남은 작업: 기존 결함을 새 SDK의 미구현으로 다시 분류해서는 안 된다. 수정본을 플랫폼별 앱 키트에 공급하고 영역 선택·Gradient를 앱에 연결해야 한다. 현재 앱의 floodMask 기반 Fill이 legacy FillOperation을 사용한다는 의미도 아니다.

수락 기준: 안/밖 0/1, opacity=0 무변경, alpha=128 부분 합성, 방사형 중심/외곽 차이, featherRadius별 경계 커버리지를 독립 픽셀 값으로 검사한다. 기존 선형 그라디언트 검사만으로 통과시키지 않는다.

근거: [E16](#e16) [E17](#e17) [E18](#e18) [E15](#e15) [E50](#e50)

<a id="d02"></a>
### D02 · Figma의 16비트 문서와 실제 8비트 sRGB 경로

**P0 · 데이터 계약 부족 · 담당: iiPaintEngine; iiSharedCanvas; iiVisualCodec::Image; iiColorManager**

현재: 현행 Figma는 sRGB / 16-bit와 300 dpi를 표시한다. 회화 RasterLayer는 uint32 ARGB32이고 Native PSD 파서는 depth != 8 또는 RGB 이외를 거부한다. iiImageCodec의 후속인 iiVisualCodec 0.4.0도 공개 image::PixelFormat이 rgb8/rgba8이다. GraphicAdjust의 float 계산과 sampleAverage16 같은 개별 커널은 16비트 정규 작업 문서가 아니다.

부족·남은 작업: Document에 픽셀 형식·비트 깊이·작업 프로파일·알파·PPI/단위·명시적 정밀도 변환을 정의하고 편집·합성·저장·교환 전체에 전달해야 한다. iiVisualCodec에는 ICC·dots_per_meter 메타데이터가 이미 있으므로 해당 개념의 SDK 전체 부재로 나열하지 않는다. Native BitmapCodec의 인쇄 해상도 비보존과 회화 문서 연결이 남은 문제이다. CMYK/HDR 전면 지원은 현재 Figma만으로 확정된 범위가 아니다.

수락 기준: 16비트 미세 계조를 편집·저장·재열기·PNG/TIFF/PSD 교환 후 보존하고, 명시적 8비트 변환만 양자화한다. 프로파일·PPI 왕복과 지원하지 않는 깊이의 정직한 거부를 검사한다.

근거: [E20](#e20) [E21](#e21) [E58](#e58) [E11](#e11) [E48](#e48) [E70](#e70)

<a id="d03"></a>
### D03 · 고급 펜 입력 API의 Congregation 채택

**P0 · 모듈 경계 해결 / 앱 입력 전달 부족 · 담당: iiGUI; iiSharedCanvas; iiPaintEngine; Congregation**

현재: iiSharedCanvas 0.33.0부터 유한·청크 편집기에 begin/continue/endStrokeInput(StrokePoint)가 있고, 현행 0.33.2는 실제 시각·기울기·회전·custom 채널을 검증하여 전달한다. 현재 App CanvasView→Artwork는 여전히 좌표와 pressure를 전달하고 레거시 beginStroke(point, pressure)를 호출한다.

부족·남은 작업: 동결한 Native API를 앱에서 채택하여 WidgetInput의 센서·실제 timestamp를 끝까지 전달해야 한다. 포인터 장치·캡처·취소는 UI/세션이 소유하고 문서에는 원시 궤적을 저장하지 않는 계약을 유지한다. 새 모듈 API가 없다는 과거 판정은 해소되었지만 앱의 입력 단절은 남아 있다.

수락 기준: 동일 궤적을 다른 이벤트 빈도와 실제 시간으로 넣어 결과를 비교한다. 기울기·회전 변화가 픽셀에 반영되는지, 캔슬·장치 전환·비유한 입력이 안전한지 검사하고 물리 펜 검증을 따로 기록한다.

근거: [E29](#e29) [E30](#e30) [E54](#e54) [E66](#e66) [E73](#e73)

<a id="d04"></a>
### D04 · Mixer·Smudge 샘플러 수정본의 앱 키트 채택

**P0 · 모듈 브리지 해결 / 앱 키트 갱신 필요 · 담당: iiSharedCanvas; iiPaintEngine**

현재: iiSharedCanvas 0.33.1에서 유한·청크·음수 원점의 획 시작 자산 픽셀을 샘플러에 연결했다. iiPaintEngine 0.5.3은 Smudge의 전경색 혼입·alpha 전달과 큰 좌표 차의 overflow를 보강했다. 현재 0.33.2/0.5.4 소스와 스테이지에 포함되지만 앱 키트는 0.32.0/0.5.1이다.

부족·남은 작업: 새 키트로 재빌드하고 앱에서 배경 색 이동·투명 경계·청크 경계·취소를 검사해야 한다. 구현된 기준은 현재 자산의 획 시작 픽셀이다. 합성 전체 샘플링 옵션을 제공할 경우에는 별도 소스 정책을 정의해야 하며, 기존 수정이 자동으로 그 옵션까지 제공한다고 해석하지 않는다.

수락 기준: 서로 다른 두 색 위에서 Mix/Wetness/Pickup을 조절했을 때 실제 색이 이동해야 한다. 투명 경계, 청크 경계, 되돌리기와 캔슬도 같은 기준으로 검사한다.

근거: [E07](#e07) [E57](#e57) [E08](#e08) [E73](#e73)

<a id="d05"></a>
### D05 · 정지 분사·안정화 시계의 앱 수명 연결

**P0 · 모듈 시계 해결 / 앱 시계 채택 부족 · 담당: iiSharedCanvas; iiPaintEngine; iiSDKCore**

현재: iiSharedCanvas 0.33.2에 advanceStrokeTime/rebaseStrokeTime이 있다. 정지 Airbrush, 따라잡기, 중단 시간 제외, 역행/비유한 시각 거부를 모듈·설치 소비자가 검증한다. Congregation은 현재 좌표·압력 API와 포인터 이벤트 중심 경로를 사용한다.

부족·남은 작업: 앱의 실제 프레임/단조 시계에서 활성 획을 진행시키고 장면 중단·복귀·포인터 캡처 해제에 재기준화 또는 취소를 연결해야 한다. 타이머를 하위 문서/브러시 모듈에 숨겨 넣을 필요는 없다. 새 시계 API가 없는 상태는 해소되었지만 제품의 정지 분사 동작은 미완료이다.

수락 기준: 같은 위치에서 0.2/1/2초 누른 결과가 설정된 초당 분사량을 반영하고, 이벤트 수에 비례하지 않아야 한다. 백그라운드 복귀 시 대량 분사가 발생하지 않아야 한다.

근거: [E06](#e06) [E66](#e66) [E73](#e73) [E10](#e10)

<a id="d06"></a>
### D06 · 영역 선택과 Quick Mask의 일관된 편집 계약

**P1 · 도구 세션과 표현 연결 부족 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

현재: 사각 SelectionState, byte SelectionMask, float PixelMask, iiSharedCanvas RasterMask와 flood/feather/combine API가 있다. 앱의 select는 레이어 hit-test이며 픽셀 영역 선택이 아니다.

부족·남은 작업: 사각·타원·올가미·다각형의 마스크 생성, add/subtract/intersect, 안티앨리어싱·반전·feather·Quick Mask 편집, 모든 도구에 적용되는 활성 선택 상태와 좌표 변환이 필요하다. byte/float·문서/장치 좌표 의미도 명시해야 한다. 저수준 선택/채우기 결함은 D01의 새 모듈에서 수정되었으므로 그 결함의 재구현과 도구 세션 연결을 구분한다.

수락 기준: 확대·이동·회전 후에도 같은 문서 픽셀을 선택하고 브러시·필터·채우기가 같은 부드러운 마스크를 사용해야 한다. 새 SDK 선택 계약을 앱 키트에서도 확인한다.

근거: [E16](#e16) [E15](#e15) [E01](#e01) [E45](#e45)

<a id="d07"></a>
### D07 · 자기 올가미의 경계 추종

**P1 · 명시적 알고리즘 API 미확인 · 담당: iiPaintEngine 또는 별도 편집 도메인 계층**

현재: 현재 검토한 공개 헤더에 Magnetic Lasso 또는 Live Wire의 앵커·경로 추종 API는 없다. Canny/LineArt와 자동 마스크는 경계 정보이며 대화형 경계 추종 도구와 같지 않다.

부족·남은 작업: 경계 비용·탐색 반경·앵커 고정/삭제·닫기·수동 전환·노이즈 대응을 정의한 재사용 가능한 선택 경로 생성기가 필요하다.

수락 기준: 알려진 윤곽을 가진 이미지에서 목표 경로 오차와 앵커 편집을 검사하고, 평탄·잡음·투명 경계에서 종료와 취소가 보장되어야 한다.

근거: [E16](#e16) [E15](#e15) [E14](#e14)

<a id="d08"></a>
### D08 · 자르기와 원근 자르기

**P1 · 문서 단위 작업 부족 · 담당: iiSharedCanvas; iiGraphicAdjust; Congregation**

현재: 래스터 crop, CanvasExtent 편집, iiGraphicAdjust의 원근·메시 변형 기반은 있다. 별도 TimelineProject의 TimelineCrop도 클립 자르기 값으로 존재하지만 전체 회화 문서의 crop 실행 계약과는 다르다. 앱의 crop/perspective-crop 아이콘은 실행 맵에 없다.

부족·남은 작업: 캔버스와 다중 레이어·마스크·모션·아트보드의 원점 이동, 잘린 픽셀 보존 여부, 회전/원근 사각형, 출력 크기/PPI, 하나의 편집 트랜잭션을 결합해야 한다.

수락 기준: 레이어·마스크가 있는 문서를 자르고 저장·재열기·undo한 뒤 좌표와 픽셀을 대조한다. 단일 이미지 crop만 통과한 상태로 문서 crop 완료를 선언하지 않는다.

근거: [E11](#e11) [E25](#e25) [E01](#e01)

<a id="d09"></a>
### D09 · 자유 변형과 정렬 및 비파괴 뷰 회전

**P1 · 상호작용과 뷰 변환 부족 · 담당: iiSharedCanvas; iiPaintEngine; iiGUI; Congregation**

현재: AffineTransform·레이어 변환은 있고 앱은 한 레이어의 이동/배율/회전과 좌우/상하 반전·90도 회전을 수행한다. 정렬·분배·rotate-view는 완성 경로가 없다. RasterViewport와 iiGUI ContentTransform에는 회전 항목이 없다.

부족·남은 작업: 다중 선택·기준점·변형 핸들·스큐/원근·확정/취소·정렬 기준이 필요하다. 뷰 회전·미러는 입력 역변환과 실제 문서 변형을 분리해야 한다. 현재 캔버스 하단 flip은 레이어 변형 명령으로 연결되어 있어 뷰 미러를 의도한 것인지 결정해야 한다.

수락 기준: 미리보기 중 문서 리비전 유지, 확정 1회 커밋, Escape 롤백, 회전된 화면의 펜 hit-test, 다중 레이어의 정렬·분배를 검사한다.

근거: [E38](#e38) [E44](#e44) [E45](#e45) [E11](#e11)

<a id="d10"></a>
### D10 · 복제 도장과 힐링 및 패턴 도장

**P1 · 앱 도구 연결 부족 · 담당: iiGraphicAdjust; iiSharedCanvas; Congregation**

현재: iiGraphicAdjust에 cloneSpot/healSpot/removeSpot/rankSpotSources가 구현되어 있다. 앱은 clone/heal/spot-heal/pattern-stamp를 실행하지 못한다. SpotOptions는 주어진 점들의 원형 dab 합집합이며 빠진 점 사이를 자동 보간하지 않는다.

부족·남은 작업: 복제 소스 선택, aligned/non-aligned, 현재/합성 레이어 샘플링, 연속 dab 보간, 패턴 자원·원점·배율, 미리보기와 부분 커밋을 연결해야 한다. 커널 부재로 분류하여 재구현할 필요는 없다.

수락 기준: 소스 이동·투명도·선택 마스크·반복 스트로크와 캔슬을 픽셀로 검사한다. Heal을 Poisson 또는 생성형 지우개와 같은 동작이라고 설명하지 않는다.

근거: [E24](#e24) [E55](#e55) [E01](#e01)

<a id="d11"></a>
### D11 · Blur Sharpen Dodge Burn Sponge 브러시

**P1 · 국부 도구 어댑터 부족 · 담당: iiGraphicAdjust; iiPaintEngine; iiSharedCanvas**

현재: blur/sharpen, 노출·채도 조정과 마스크 합성 커널은 있다. Figma의 해당 브러시 도구는 앱 실행 맵에서 빠져 있다. GalleryFilters.sponge는 질감 효과로 채도 조절 Sponge 브러시와 다르며 ColorDodge 혼합 모드도 Dodge 브러시가 아니다.

부족·남은 작업: 브러시 커버리지로 국부 필터 강도를 누적하는 편집 세션, 영역/톤 범위·노출·채도 모드·알파 정책이 필요하다.

수락 기준: 스트로크 밖 픽셀이 변하지 않고 연속 누적과 가장자리 혼합이 정의되어야 한다. 이름이 비슷한 전역 필터를 연결한 것으로 도구 완료를 판정하지 않는다.

근거: [E24](#e24) [E26](#e26) [E15](#e15) [E01](#e01)

<a id="d12"></a>
### D12 · Liquify와 Warp의 미리보기 및 비파괴 저장

**P1 · 대화형 편집 세션 부족 · 담당: iiGraphicAdjust; iiSharedCanvas; iiSDKCore**

현재: iiGraphicAdjust는 Liquify·Puppet Warp·Perspective Warp와 메시 제약을 이미 제공한다. API는 제한된 입력을 받아 새 이미지를 반환하며 앱에는 연결되지 않는다.

부족·남은 작업: 원본/메시 소유권·핸들 편집·Freeze/Reconstruct·선택 마스크·ROI·취소·작업 예산·확정 트랜잭션과, 편집 가능한 메시를 문서에 남길지 결정해야 한다.

수락 기준: 전체/부분 작업 결과의 동일성, 비동기 취소, 큰 이미지 제한, alpha 보존과 저장 후 재편집을 검증한다. 픽셀 베이크만 지원하면 그 범위를 명시한다.

근거: [E25](#e25) [E26](#e26) [E01](#e01)

<a id="d13"></a>
### D13 · Pen Curvature Pen 제어점 편집

**P1 · 벡터 도구 연결 부족 · 담당: iiSharedCanvas; Congregation**

현재: VectorEditor에 경로·Bézier·앵커·제어점 삽입/교체/삭제 API가 있다. 앱의 pen/curvature-pen/add-anchor/delete-anchor는 실행되지 않고 direct-select는 레이어 선택으로 연결된다.

부족·남은 작업: 세그먼트 hit-test, 삽입 위치, 곡률 자동 핸들, cusp/smooth 전환, 핸들 드래그·확정/취소와 path-vs-layer 선택 상태가 필요하다.

수락 기준: 곡선 위 삽입 전후 형상 보존, 제어점 제약, 변형 후 hit-test와 명령별 undo를 검사한다. SDK의 벡터 곡선 자체가 없는 것은 아니다.

근거: [E14](#e14) [E01](#e01)

<a id="d14"></a>
### D14 · 편집 가능한 영역 텍스트와 경로 텍스트

**P1 · 정규 문서 개념 부재 · 담당: iiSharedCanvas; iiGUI; Congregation**

현재: iiGUI에는 글꼴 셰이핑·TextRun·텍스트 편집 모델이 있고 별도 TimelineProject에는 TimelineSubtitleClip/스타일도 있다. 그러나 iiSharedCanvas 회화 Document의 Layer/Asset 타입에 편집 가능한 캔버스 텍스트 개체는 없고 앱은 Pretendard TextRun을 비트맵 레이어로 삽입한다.

부족·남은 작업: 문자·스타일 범위·폰트 식별·문단·텍스트 박스·기준선·경로 참조·레이아웃 캐시를 가진 캔버스 텍스트 문서 타입과 파일 왕복이 필요하다. UI TextDocument나 iiGeneralDocument의 문서 읽기 기능만으로 회화 문서의 editable text/path text가 생기지는 않는다.

수락 기준: 한글/복합 문자·줄바꿈·폰트 대체·곡선 위 배치·저장 후 재편집·출력 시 정책을 검사한다. 원본 문자열을 잃는 래스터화와 편집 가능한 텍스트를 구분한다.

근거: [E04](#e04) [E11](#e11) [E61](#e61) [E31](#e31)

<a id="d15"></a>
### D15 · 계층형 레이어 그룹과 Pass Through

**P1 · 정규 문서 개념 부재 · 담당: iiSharedCanvas**

현재: Document.layers는 평면 목록이며 Artboard는 독립적인 비중첩 캔버스 그룹이다. Figma의 Painting 폴더와 Pass 표시는 일반 레이어 그룹을 암시하지만 정규 LayerGroup/부모 관계·그룹 합성 계약은 없다.

부족·남은 작업: 그룹 ID·부모/자식 순서·중첩 검증·pass-through/isolated·그룹 opacity/mask·재부모화와 좌표 보존을 정의해야 한다. 아트보드를 일반 그룹으로 대체하면 자르기와 원점 의미가 달라진다.

수락 기준: 중첩 그룹을 저장·재열기하고 반투명 배경·혼합 모드·그룹 마스크·undo를 픽셀로 검사한다. ORA 그룹 평탄화 경고를 그룹 보존 지원으로 해석하지 않는다.

근거: [E11](#e11) [E12](#e12) [E41](#e41)

<a id="d16"></a>
### D16 · 레이어 잠금과 레이어 링크

**P1 · 정규 편집 정책 부족 · 담당: iiSharedCanvas; Congregation**

현재: 회화 Document의 LayerProperties에 잠금·잠금 종류·링크 집합이 없고 앱의 lock/link 버튼도 비활성이다. 별도 TimelineProject에는 locked/syncLocked와 TimelineLinkGroup이 이미 있다. 문서 저장의 파일 잠금은 레이어 편집 잠금과 별개이다.

부족·남은 작업: 픽셀·위치·알파·전체 잠금 중 지원할 의미, 링크한 회화 레이어의 선택/변환 동작, 외부 소비자와 협업 편집에도 적용할 거부 규칙을 정의해야 한다. 기존 타임라인 값 모델의 재사용/매핑을 결정하여 같은 이름의 필드를 중복 신설하지 않는다.

수락 기준: UI 우회 호출·파일 재열기·원격 편집에서도 잠금 정책을 지키고, 링크 재부모화·삭제·undo를 검증한다.

근거: [E11](#e11) [E59](#e59) [E62](#e62) [E05](#e05)

<a id="d17"></a>
### D17 · 레이어 마스크와 클리핑 편집 워크플로우

**P1 · 기존 SDK 미연결 · 담당: iiSharedCanvas; iiPaintEngine; Congregation**

현재: LayerAppearance는 raster/vector/filter mask, ordered mask stack, density/feather·clippedToBelow를 제공하고 렌더링·직렬화·테스트도 있다. 앱에서는 mask/clipping 버튼이 비활성이다.

부족·남은 작업: 활성 편집 채널을 콘텐츠/마스크로 바꾸고 선택→마스크 생성, 브러시 편집, 링크/해제, enable/invert와 썸네일을 연결해야 한다. 핵심 마스크 모델을 새로 만들 필요는 없다.

수락 기준: 콘텐츠와 마스크를 독립적으로 편집·저장·undo하고 부분 알파·벡터 마스크·클리핑 체인을 검사한다.

근거: [E12](#e12) [E13](#e13) [E51](#e51) [E05](#e05)

<a id="d18"></a>
### D18 · 프레임 노출과 모션 키프레임 편집

**P1 · 기존 SDK 미연결 · 담당: iiSharedCanvas; Congregation**

현재: Document.frames/KeyframedSource와 LayerProperties.motion, setLayerMotion이 있다. 앱은 현재 시각을 이동·재생하지만 키 추가/삭제는 비활성이고 animated layer 복제는 명시적으로 거부한다.

부족·남은 작업: 현재 프레임의 편집 자산 결정, 노출 유지/분할·복제·이동·삭제, 모션과 콘텐츠 키 구분, 시간 범위와 ripple 여부를 정해야 한다. TimelineProject와 Canvas Document 중 정본도 명확히 해야 한다.

수락 기준: 0/중간/마지막 프레임에서 그리기·노출·모션을 저장/재열기하고 리비전·undo를 검사한다. 탐색은 무변경이며 키 편집은 한 명령이어야 한다.

근거: [E49](#e49) [E11](#e11) [E40](#e40) [E05](#e05)

<a id="d19"></a>
### D19 · 어니언 스킨

**P1 · 정책과 UI 미연결 · 담당: iiSharedCanvas; iiGUI; Congregation**

현재: 다른 프레임을 읽는 renderFrameRegion/Tile 기반은 있다. 앱의 onion-skin 버튼은 비활성이다.

부족·남은 작업: 이전/다음 노출 개수, 프레임/키 기준 간격, 틴트·감쇠·제외 레이어·캐시 무효화와 선택 레이어 정책이 필요하다.

수락 기준: 현재 문서 픽셀과 리비전을 바꾸지 않으며, 프레임/레이어 변경 후 잔상이 갱신되는지 검사한다.

근거: [E42](#e42) [E05](#e05)

<a id="d20"></a>
### D20 · Audio 트랙의 가져오기 재생 및 동기화

**P1 · 네이티브 미디어 경로 부족 · 담당: iiSharedCanvas; iiDeviceMultimedia; iiSDKCore; Congregation**

현재: Document에 PCM16 AudioAsset·클립·트랙·gain과 편집 API가 있고 Native DocumentAudio 검사가 있다. 별도 TimelineProject에도 pan/fade/channelMatrix 등을 가진 TimelineAudioMix가 있다. 그러나 Native 타깃은 AudioCodec.cpp와 그 공개 헤더를 빌드/설치하지 않는다. 앱 타임라인은 doc.layers만 열거하여 audioTracks를 표시하지 않는다.

부족·남은 작업: WAV 등 코덱의 Qt 없는 공개 경로, PCM16↔float32 변환·믹싱/리샘플링·seek·파형·동영상 시계와 오디오 장치의 동기화가 필요하다. iiDeviceMultimedia의 장치 PCM I/O는 활용할 기반이다.

수락 기준: 파일 왕복 외에 재생·스크럽·프레임 레이트 변경·장면 중단/복귀의 A/V 오차와 장치 수명을 실측한다. 모형 Audio 행과 움직이는 마커는 실제 재생의 증거가 아니다.

근거: [E23](#e23) [E60](#e60) [E62](#e62) [E22](#e22) [E53](#e53)

<a id="d21"></a>
### D21 · Camera 트랙

**P1 · 의미 불명확과 도메인 부재 · 담당: iiSharedCanvas; Congregation**

현재: Figma는 Camera 행과 Object 표시를 가지지만 시야·카메라 키·출력 구도 계약은 없다. iiSharedCanvas MotionValue는 일반 레이어 변환이며 전용 2D Camera 트랙은 아니다.

부족·남은 작업: 2D 촬영 구도인지 단순 뷰 북마크인지 정한 뒤 프레이밍·줌·회전·보간·출력에 대한 적용과 저장 계약을 정의해야 한다. iiCameraIntegration의 실제 장치 촬영이나 iiActorObject의 3D 모델로 자동 대체할 수 없다.

수락 기준: 카메라 적용 전/후 프레임과 출력 영역을 정해진 픽셀로 비교하고, 카메라 편집과 일반 뷰 탐색의 문서 변경 여부를 분리한다.

근거: [E11](#e11) [E49](#e49) [E53](#e53)

<a id="d22"></a>
### D22 · Color balance 조정 레이어

**P1 · 정규 문서 개념 부족 · 담당: iiSharedCanvas; iiGraphicAdjust**

현재: iiGraphicAdjust에는 Color Balance 등 보정 커널이 있고 LayerAppearance.filters는 개별 콘텐츠 레이어에 보정을 적용한다. 아래 합성 결과에 작용하는 독립 Adjustment Layer 타입은 없다.

부족·남은 작업: 보정 파라미터·아래 스택 적용 범위·마스크·클리핑·그룹 경계·캐시 무효화와 직렬화 노드를 정의해야 한다. 현재 Figma의 Color balance 이름만으로 조정 레이어가 구현된 것은 아니다.

수락 기준: 아래 레이어를 바꾸면 보정 결과가 따라 바뀌고, 보정 레이어 이동/마스크/비활성/저장 왕복이 정의된 결과를 만들어야 한다.

근거: [E12](#e12) [E26](#e26) [E43](#e43)

<a id="d23"></a>
### D23 · 혼합 모드 선택과 Pass Object 표시

**P1 · SDK 미연결과 타입 혼재 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

현재: SDK는 다수의 픽셀 혼합 모드를 지원한다. 앱 타임라인은 Normal/Multiply/Screen만 구별하여 표시한다. Figma의 Pass는 그룹 정책이고 Object는 일반 픽셀 혼합 모드가 아니다.

부족·남은 작업: 레이어 종류별 혼합 모드 목록과 표시 타입을 분리하고 전체 SDK 모드를 연결해야 한다. 지원하지 않는 종류를 Normal로 보이게 하는 표시 손실을 피해야 한다.

수락 기준: 각 혼합 모드의 저장 값과 UI 선택이 일치하고, 투명 배경과 clipping에서 독립 기대 픽셀을 검사한다.

근거: [E11](#e11) [E12](#e12) [E53](#e53)

<a id="d24"></a>
### D24 · 22개 브러시 파라미터의 작성 계약과 앱 경로 불일치

**P0 · 작성 모듈 구현·검증 완료 / 앱 채택 부족 · 담당: iiBrushTools; iiPaintEngine; iiSharedCanvas; Congregation**

현재: iiBrushTools 0.1.0은 22개 파라미터의 단위·범위·도구별 유효성, 전체 보정 획 길이에 대한 Start/End 직경 taper, 사후 보정, 정규화 Velocity, 생성 Bristle 팁과 Roundness 0%를 제공한다. 전체·ASan/UBSan·설치 소비자 각각 6/6 기록이 있다. 앱의 ToolSettings.brush()는 이 모듈을 사용하지 않고 End를 읽지 않으며 Post correction을 PulledString에 매핑한다.

부족·남은 작업: App의 임시 입력 표본 소유·작성 미리보기 교체·최종 픽셀 타일 커밋을 새 계약에 연결해야 한다. 같은 원본에 대해 미리보기를 교체하고 여러 결과 타일을 한 편집·Undo로 묶어야 한다. 타일마다 기존 전체 스냅샷 편집을 호출하는 임시 연결은 D28 비용을 악화한다. 하위 엔진/문서에는 입력 궤적을 보존하지 않는다.

수락 기준: 새 키트의 실제 앱에서 22개 값의 개별 효과·비활성 정책·전후 taper 단면 폭·끝점·기울기·실제 속도를 검사한다. 클릭/드래그와 미세 증감이 같은 파라미터를 조절하고 취소·Redo·다중 타일이 하나의 명령으로 복원되어야 한다.

근거: [E03](#e03) [E46](#e46) [E64](#e64) [E65](#e65) [E71](#e71) [E73](#e73)

<a id="d25"></a>
### D25 · 브러시 프리셋과 Textured oil의 자원 계약

**P1 · SDK 자원 모델 미연결 · 담당: iiPaintEngine; iiFileProvider; Congregation**

현재: iiPaintEngine에는 BrushPreset/Library/Serializer·ABR·텍스처/팁 자원이 있다. 앱의 프리셋 선택은 두 이름과 일부 값 전환이며 ToolSettings는 22개 수치만 저장한다. BrushTexture 기본값은 disabled이다.

부족·남은 작업: 팁·텍스처·재질·동역학·도구 종류·자원 ID를 소유하는 프리셋과 현재 수치의 우선순위를 연결해야 한다. Textured oil/Pencil/Dry brush가 어떤 엔진 설정인지 명시해야 한다.

수락 기준: 프리셋 저장/재열기 후 팁·텍스처를 포함한 결과가 같고, 자원 누락·보호 텍스처·ABR 제한을 명시적으로 처리한다.

근거: [E39](#e39) [E03](#e03) [E56](#e56)

<a id="d26"></a>
### D26 · HSB 선택기와 공유 색상 상태

**P1 · 컬러 모델과 UI 연결 부족 · 담당: iiColorManager; iiGUI; LVRS; Congregation**

현재: iiGUI ColorPickerModel과 iiColorManager의 별도 Picker/색 변환 모델이 존재한다. 앱의 HSB ComboBox는 클릭 동작이 없으며 iiColorManager를 직접 소비하지 않는다. generic CMYK는 ICC 인쇄 변환과 별개이다.

부족·남은 작업: 사용할 하나의 색 상태 정본, 모드 선택 목록·채널 단위·알파·취소/확정 동기화를 정하고 기존 SDK 어댑터로 연결해야 한다.

수락 기준: HSB/RGB 등 확정한 모드를 왕복해 색과 alpha를 보존하고, eyedropper·brush·picker가 같은 값을 사용해야 한다.

근거: [E27](#e27) [E28](#e28) [E52](#e52)

<a id="d27"></a>
### D27 · 눈금자 가이드 원근 그리드와 스냅

**P1 · 좌표 정책과 도구 미연결 · 담당: iiPaintEngine; iiSharedCanvas; Congregation**

현재: 앱 눈금자는 64 화면 간격에 128 문서 값을 그리는 고정 표시이며 현재 viewport의 원점·배율을 읽지 않는다. grid/rulers 토글은 있고 snap/perspective-grid 명령은 구현 경로가 없다.

부족·남은 작업: 문서 좌표·단위·PPI·줌/이동/회전과 연결한 눈금, 이동 가능한 가이드·원근 소실점·스냅 후보/허용 거리/우선순위와 일시 해제를 정의해야 한다.

수락 기준: 다른 줌/원점/회전에서도 알려진 문서 좌표가 같은 눈금과 일치해야 한다. 스냅 허용 거리는 화면/문서 단위를 명시하고 시간 스냅과 공간 스냅을 분리한다.

근거: [E37](#e37) [E38](#e38) [E45](#e45)

<a id="d28"></a>
### D28 · 공통 영역 이력 이후 남은 Native·파일·앱의 전체 복제

**P0 · 공통 기반 구현 / 실제 편집 경로 미전환 · 담당: iiSharedCanvas; iiPaintEngine; iiFileProvider; Congregation**

현재: iiPaintEngine 0.5.4 RasterEditHistory는 여러 plane·영역을 한 명령으로 묶고 바뀐 행 구간의 이전/이후 표본만 보관한다. 기본 Reject와 명시적인 EvictOldest, 사전 예산 판정·충돌 검출·원자적 Writer 포트가 있다. 그러나 BitmapEditor/ChunkedBitmapEditor는 전체 자산 스냅샷, DocumentFile은 Document draft 전체 복사, Artwork는 전체 IISC 직렬화 before/after를 계속 사용한다.

부족·남은 작업: Native 다중 영역 편집·브러시 미리보기·취소를 새 이력으로 옮기고 파일 트랜잭션·바인딩 세대·구조 delta·앱 전역 Undo 순서를 결합해야 한다. Writer의 원자성은 호출자 구현 책임이다. 앱은 캡처가 비면 커밋 후 이력을 비우며 이 경로는 아직 수정되지 않았다. 사전 거부/명시적 오래된 이력 제거를 사용자에게 보이는 정책과 연결해야 한다.

수락 기준: 8K 고엔트로피 다중 레이어·실제 필터·마스크·연속 획·취소·파일 실패에서 픽셀/리비전/Undo의 원자성, 메모리 한도와 취소 지연을 검사한다. 기존 공통 이력의 1 GiB 메모리 콜백 검사는 실제 필터·파일 I/O·앱 취소 성능의 증거가 아니다.

근거: [E35](#e35) [E36](#e36) [E63](#e63) [E67](#e67) [E69](#e69) [E71](#e71) [E72](#e72) [E75](#e75)

<a id="d29"></a>
### D29 · GPU 표시와 문서 합성 가속의 구분

**P1 · 실제 GPU 처리 경로 부족 · 담당: iiPaintEngine; iiSharedCanvas; iiGUI**

현재: iiGUI는 Metal/D3D11/Vulkan 표시를 제공한다. 그러나 iiPaintEngine renderLayerStackGpu의 available=true 경로는 CPU 함수 compositeLayerStack를 호출하고 Gpu라는 backend 값을 기록한다. 캔버스의 브러시 투영과 프레임 합성도 CPU 코드이다.

부족·남은 작업: 실제 실행 백엔드 표기를 바로잡고, 타일·브러시·필터·마스크 합성의 어떤 작업을 어느 GPU API가 수행하는지 기능 계약과 CPU 폴백·장치 손실 정책을 정해야 한다.

수락 기준: GPU 타이밍/명령 제출과 결과 픽셀을 함께 검사하고, 창이 Metal로 표시되었다는 사실만으로 GPU 브러시·문서 합성 완료를 선언하지 않는다.

근거: [E19](#e19) [E07](#e07) [E42](#e42)

<a id="d30"></a>
### D30 · 대형 필터 작업의 ROI 취소와 화면 반영

**P1 · 성능과 비동기 작업 계약 미흡 · 담당: iiSDKCore; iiSharedCanvas; iiGraphicAdjust; iiGUI**

현재: Core Runtime/TaskScope와 FrameRenderTileRequest는 있다. GraphicAdjust의 다수 API는 동기 ImageView→새 Image이고 일부 레이어 필터는 원본 해상도 전체를 처리한다.

부족·남은 작업: 작업별 ROI/메모리/시간 예산, 실행 중 취소·진행률·문서 세대 가드·부분 결과 게시와 캐시 무효화를 공통 편집 세션에서 결합해야 한다. 일반 스케줄러를 새로 만들 필요는 없다.

수락 기준: 8K·수십 레이어·스크러빙/필터 동시 작업에서 입력 지연·메모리 상한·취소 시간·오래된 결과 거부를 측정한다. 측정 전에는 성능 충족을 판정하지 않는다.

근거: [E26](#e26) [E42](#e42) [E43](#e43)

<a id="d31"></a>
### D31 · Qt 없는 앱 키트·코덱 후속 패키지·ABI 공급

**P1 · 배포 프로필과 패키지 채택 부족 · 담당: iiSDKBuilder; iiVisualCodec; 각 SDK; Congregation**

현재: 검토한 Congregation macOS/iPad 키트에는 iiGraphicAdjust·iiColorManager·iiDeviceMultimedia·iiBrushTools가 없고 Engine/Native Canvas도 이전 버전이다. 소스의 iiImageCodec은 iiVisualCodec 0.4.0으로 통합되었으며 CMake target/include/namespace 호환 면을 제공한다. 앱과 Native Canvas는 여전히 그 호환 패키지를 요구한다. iiSocietyHelper core facade는 Qt6 필수이다.

부족·남은 작업: 대상별 필요한 컴포넌트만 빌드·설치하고 앱 헤더/바이너리/ABI를 하나의 키트로 맞춰야 한다. iiVisualCodec::Image의 직접 채택 또는 검증한 호환 패키지 공급 정책을 정한다. Image만 쓰는 앱에 CAD/USD/OpenCV/Video 전체를 불필요하게 링크할 이유는 없다. Society 연동을 재확장한다면 Qt 없는 프로토콜 경계를 사용한다. 현재 Figma에서 없앤 좌측 Asset Library를 이번 감사 범위에 다시 추가하지 않는다.

수락 기준: Qt 탐색 차단 상태의 설치 소비자·재배치·동적 로더와 iPad/데스크톱 각 패키지 조합을 검사한다. 현재 제거한 에셋 사이드바를 다시 필수 요구로 넣지는 않는다.

근거: [E22](#e22) [E34](#e34) [E47](#e47) [E68](#e68) [E70](#e70) [E74](#e74)

<a id="d32"></a>
### D32 · LVRS 네이티브 동등성과 입력 접근성

**P2 · 부분 지원과 플랫폼 검증 부족 · 담당: iiGUI; LVRS; LVUI**

현재: 기본 창·아이콘·슬라이더·스크롤·색상 필드와 스타일 토큰은 있다. LVRS parity 목록은 해당 위젯들을 partial로 기록하고, LVUI는 동적 구성/전체 수명 일부를 미완료로 기록한다. iPad는 UIKeyInput 확정 문자 경로이며 전체 UITextInput marked-text와 외부 DnD는 지원 범위 밖이다.

부족·남은 작업: 이 화면에 필요한 키보드 증감·포커스·캡처 해제·IME·복합 문자·스크롤 clipping·가상 행 접근성·정밀 수치 조작을 실제 대상 플랫폼에서 검증해야 한다. 오래된 문서의 포괄적 미지원 문구와 신규 API를 대조하여 지원표도 갱신해야 한다.

수락 기준: 현재 Figma에 쓰인 컴포넌트/상태만 먼저 묶어 입력·렌더링·접근성 검사를 실행한다. 기존 macOS/iPad 창 검사는 전문 도구 전체나 Windows/Linux 실행 증거로 확대하지 않는다.

근거: [E29](#e29) [E31](#e31) [E32](#e32) [E33](#e33)

<a id="d33"></a>
### D33 · 아이콘과 표시값만으로 결정할 수 없는 동작

**P1 · 요구 사양 불명확 · 담당: 제품 기능 명세; 편집 도메인 인터페이스**

현재: 툴 레일 이름은 44 controls이지만 실제 액션 슬롯은 구분선 제외 49개이다. Free transform, Camera/Object, Pass, Clone source, Tool/Doc/View/More 및 브러시 그룹의 동작 범위가 아이콘만으로 확정되지 않는다. Tool 파라미터의 prototype reaction 존재는 엔진 실행 계약이 아니다.

부족·남은 작업: 각 도구의 대상·입력 제스처·수치 단위·미리보기·저장 데이터·undo 단위·허용 레이어·오류/비활성 정책을 짧은 기능 명세로 고정해야 한다. 보이는 버튼과 실제 도구 수를 구분한다.

수락 기준: Figma 항목마다 성공/취소 예제와 독립 기대 결과를 지정하고 미정 상태를 지원 완료로 분류하지 않는다.

근거: [E01](#e01) [E03](#e03) [E05](#e05)

<a id="d34"></a>
### D34 · 화면 검사와 알고리즘 검사의 간극

**P1 · 기능 수락 검증 부족 · 담당: 각 SDK tests; Congregation tests**

현재: 모듈 보강 단계에는 선택/채우기·펜 입력·혼색·시계·브러시 작성·공통 이력의 독립 검사가 추가되었다. 이번 재검토에서도 설치 소비자의 브러시 작성 6개와 Native Canvas 4개를 별도로 실행한다. 이 검사들은 앱의 모든 Figma 도구 채택이나 실제 펜·모든 플랫폼 실행의 증거가 아니다.

부족·남은 작업: 각 도구에 API 존재·Qt 없는 설치 소비·문서 왕복·픽셀/입력 의미·앱 채택·플랫폼 실행을 분리한 수락 표가 필요하다. 미연결 버튼을 클릭했을 때 상태 문자열만 바뀌거나 no-op인 것을 성공으로 처리하지 않는다. SDK 개선 기록과 앱 키트가 같은 리비전인지도 확인해야 한다.

수락 기준: 마스크·혼합·압력·시간·프레임·텍스트·그룹의 독립 수치/픽셀 기준을 사용한다. 구현을 그대로 복사한 기대값이나 빈 no-op 성공을 통과 근거로 쓰지 않는다.

근거: [E50](#e50) [E51](#e51) [E32](#e32) [E64](#e64) [E66](#e66) [E74](#e74)

## 중복 구현을 피해야 하는 경계

| 영역 | 이미 있는 기반 | 필요한 보강 책임 |
|---|---|---|
| 선택·마스크 | Selection/Mask·flood/feather/combine·LayerAppearance mask stack | 활성 선택·Quick Mask·콘텐츠/마스크 편집 채널과 도구 공통 적용 |
| 리터치·변형 | GraphicAdjust clone/heal·국부 보정·Liquify/Puppet/Perspective Warp | 제스처·소스 정책·preview/commit/cancel·ROI·한 번의 Undo |
| 벡터·모션 | VectorEditor·Bézier·Document.frames·LayerProperties.motion | 경로 제어점 도구·노출·키 편집과 문서 저장 연결 |
| 색·코덱 | ColorManager·GUI ColorPickerModel·VisualCodec Image/ICC/해상도 메타데이터 | 회화 작업 색 공간·16비트 정규 문서·PPI 전달과 색 상태 정본 |
| 비동기·파일 | Core Runtime/TaskScope·FileProvider 원자적 파일/SQLite | 편집 결과 세대·취소·진행률·문서 트랜잭션·파일 실패 롤백 |
| 이력 | Engine RasterEditHistory | Native 원자적 다중 패치·미리보기·파일 연계·구조 delta·앱 전역 Undo |
| 미디어·Camera | Document PCM16/AudioTrack·TimelineProject mix·VisualCodec Spatial/USD camera | 회화 타임라인의 실제 재생/장치 시계와 2D Camera 정규 의미 |

`TimelineProject`의 locked/syncLocked/link group/subtitle/audio mix와 회화 `Document`는 별개의 모델이다. 타입이 다른 곳에 있다는 이유로 SDK 전체에서 없다고 단정하지 않았다. 반대로 이를 현재 Document.layers·FrameRenderer의 회화 잠금·그룹·편집 텍스트·Camera 지원으로 계산하지 않았다. `LayerAppearance::LayerMask`는 자산 ID를 참조하며 공통 이력의 float64 plane 시험이 정규 레이어 마스크 형식의 증거가 되는 것도 아니다.

신규로 정규화할 가능성이 큰 개념은 회화 문서의 **작업 픽셀 형식/색상/PPI**, **편집 가능한 캔버스 텍스트**, **중첩 그룹과 합성 경계**, **레이어 보호·링크 정책**, **하위 합성 결과에 작용하는 조정 레이어**, 그리고 의미 확정 뒤의 **2D Camera**이다. 자기 올가미는 별도의 대화형 경계 추종 API가 필요한 영역이다. 나머지 다수는 기존 모듈의 API를 조합하는 상위 편집 세션/어댑터 책임에 가깝다.

## 권장 보강 순서와 완료 조건

1. 완료한 D01/D03/D04/D05/D24 모듈 계약을 보존하고 D28의 Native·파일·구조·앱 이력을 먼저 연결한다. BrushTools의 여러 타일을 하나의 편집/Undo로 확정할 경계가 선행되어야 한다.
2. D02의 16비트·작업 프로파일·PPI 정규 문서를 완성한다. 8비트 compatibility나 내부 float 계산으로 대체하지 않는다. 이후 모든 도구가 정밀도를 보존할 수 있는 공통 읽기/쓰기 경계를 사용한다.
3. D14/D15/D16/D21/D22의 정규 모델과 D33의 불명확한 제품 의미를 고정한다. 임시 UI 문자열·레이어 이름으로 파일 의미를 우회하지 않는다.
4. 선택·리터치·국부 필터·워프·벡터·노출·마스크를 기존 커널 위의 가역 세션으로 연결한다. 앱에 알고리즘을 복사하지 않는다.
5. 실제 오디오, 실제 GPU 처리, ROI/취소/결과 세대와 플랫폼별 Qt 없는 패키지 공급을 완성한다. GPU 창 표시와 CPU 문서 처리의 실행 표기는 실제와 일치해야 한다.
6. 기능마다 모듈 수치/픽셀 검사 → 설치 소비자 → 문서 왕복/실패 원자성 → 앱 채택 → 허용된 플랫폼 검증을 기록한다. Windows/Linux는 앞서 요청한 소스 구현 범위와 실제 실행 증거를 구분한다.

## Figma 항목별 대조표

### 좌측 도구

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F001 | move | 기반 있음 | 부분 연결 | [D09](#d09) |
| F002 | rect-marquee | 기반 있음 | 미연결 | [D01](#d01) [D06](#d06) |
| F003 | ellipse-marquee | 부분 | 미연결 | [D06](#d06) |
| F004 | lasso | 부분 | 미연결 | [D06](#d06) |
| F005 | polygon-lasso | 부분 | 미연결 | [D06](#d06) |
| F006 | magnetic-lasso | 공개 API 미확인 | 미연결 | [D07](#d07) |
| F007 | magic-wand | 기반 있음 | 미연결 | [D01](#d01) [D06](#d06) |
| F008 | crop | 부분 | 미연결 | [D08](#d08) |
| F009 | perspective-crop | 부분 | 미연결 | [D08](#d08) |
| F010 | color-picker | 기반 있음 | 부분 연결 | [D02](#d02) [D26](#d26) |
| F011 | brush | 기반 있음 | 부분 연결 | [D03](#d03) [D24](#d24) [D25](#d25) |
| F012 | pencil | 기반 있음 | 부분 연결 | [D25](#d25) [D24](#d24) |
| F013 | airbrush | 기반 있음 | 부분 연결 | [D03](#d03) [D05](#d05) |
| F014 | mixer-brush | 기반 있음 | 부분 연결 | [D04](#d04) [D24](#d24) |
| F015 | dry-brush | 기반 있음 | 부분 연결 | [D25](#d25) [D24](#d24) |
| F016 | bristle-brush | 기반 있음 | 부분 연결 | [D03](#d03) [D24](#d24) |
| F017 | eraser | 기반 있음 | 연결 | [D06](#d06) [D16](#d16) |
| F018 | gradient | 기반 있음 / legacy 결함 수정 완료 | 미연결 | [D01](#d01) [D06](#d06) |
| F019 | paint-bucket | 기반 있음 | 부분 연결 | [D06](#d06) [D33](#d33) |
| F020 | smudge | 기반 있음 | 부분 연결 | [D04](#d04) [D24](#d24) |
| F021 | clone-stamp | 커널 또는 기반 있음 | 미연결 | [D10](#d10) [D31](#d31) |
| F022 | pattern-stamp | 커널 또는 기반 있음 | 미연결 | [D10](#d10) [D31](#d31) |
| F023 | healing-brush | 커널 또는 기반 있음 | 미연결 | [D10](#d10) [D31](#d31) |
| F024 | spot-heal | 커널 또는 기반 있음 | 미연결 | [D10](#d10) [D31](#d31) |
| F025 | blur | 커널 또는 기반 있음 | 미연결 | [D11](#d11) [D31](#d31) |
| F026 | sharpen | 커널 또는 기반 있음 | 미연결 | [D11](#d11) [D31](#d31) |
| F027 | dodge | 커널 또는 기반 있음 | 미연결 | [D11](#d11) [D31](#d31) |
| F028 | burn | 커널 또는 기반 있음 | 미연결 | [D11](#d11) [D31](#d31) |
| F029 | sponge | 커널 또는 기반 있음 | 미연결 | [D11](#d11) [D31](#d31) |
| F030 | liquify | 커널 있음 | 미연결 | [D12](#d12) [D31](#d31) |
| F031 | warp | 커널 있음 | 미연결 | [D12](#d12) [D31](#d31) |
| F032 | pen | 기반 있음 | 미연결 | [D13](#d13) |
| F033 | curvature-pen | 기반 있음 | 미연결 | [D13](#d13) |
| F034 | direct-select | 기반 있음 | 다른 의미로 연결 | [D13](#d13) [D09](#d09) |
| F035 | add-anchor | 기반 있음 | 미연결 | [D13](#d13) |
| F036 | delete-anchor | 기반 있음 | 미연결 | [D13](#d13) |
| F037 | rectangle | 기반 있음 | 연결 | [D09](#d09) |
| F038 | ellipse | 기반 있음 | 연결 | [D09](#d09) |
| F039 | polygon | 기반 있음 | 미연결 | [D13](#d13) [D33](#d33) |
| F040 | area-text | 정규 문서 타입 부족 | 래스터 삽입 | [D14](#d14) |
| F041 | path-text | 정규 문서 타입 부족 | 미연결 | [D14](#d14) |
| F042 | hand | 기반 있음 | 연결 | [D27](#d27) |
| F043 | rotate-view | 부분 | 미연결 | [D09](#d09) |
| F044 | Current color · LVRS default | 기반 있음 | 연결 | [D26](#d26) |
| F045 | quick-mask | 부분 | 미연결 | [D01](#d01) [D06](#d06) [D17](#d17) |
| F046 | imagezoomIn | 기반 있음 | 연결 | [D27](#d27) |
| F047 | rulers | 기반 있음 | 표시만 연결 | [D27](#d27) |
| F048 | settings | 기반 있음 | 연결 | [D33](#d33) |
| F049 | symmetry-paint | 기반 있음 | 부분 연결 | [D24](#d24) [D33](#d33) |

### 상단 명령

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F050 | generalopen | 기반 있음 | 연결 | [D33](#d33) |
| F051 | generalsave | 기반 있음 | 연결 | [D33](#d33) |
| F052 | generalundo | 기반 있음 | 연결 | [D28](#d28) |
| F053 | generalredo | 기반 있음 | 연결 | [D28](#d28) |
| F054 | Free transform | 부분 | 부분 연결 | [D09](#d09) [D33](#d33) |
| F055 | align-left | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F056 | align-center | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F057 | align-right | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F058 | align-top | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F059 | align-middle | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F060 | align-bottom | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F061 | distribute-horizontal | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F062 | distribute-vertical | 변환 기반 있음 | 미연결 | [D09](#d09) |
| F063 | flip-horizontal | 기반 있음 | 연결 | [D09](#d09) |
| F064 | flip-vertical | 기반 있음 | 연결 | [D09](#d09) |
| F065 | rotate-left | 기반 있음 | 연결 | [D09](#d09) |
| F066 | rotate-right | 기반 있음 | 연결 | [D09](#d09) |
| F067 | perspective-grid | 일부 기반 있음 | 미연결 | [D27](#d27) |
| F068 | symmetry-paint | 기반 있음 | 부분 연결 | [D24](#d24) [D33](#d33) |
| F069 | grid | 일부 기반 있음 | 토글만 연결 | [D27](#d27) |
| F070 | rulers | 기반 있음 | 표시만 연결 | [D27](#d27) |
| F071 | snap | 일부 기반 있음 | 미연결 | [D27](#d27) |
| F072 | sRGB / 16-bit | 부족 | 표시 불일치 | [D02](#d02) |
| F073 | color-picker | 기반 있음 | 부분 연결 | [D02](#d02) [D26](#d26) |
| F074 | generalexport | 부분 | 부분 연결 | [D02](#d02) [D18](#d18) [D20](#d20) |
| F075 | settings | 기반 있음 | 연결 | [D33](#d33) |

### Tool 파라미터

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F076 | Size | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D24](#d24) |
| F077 | Hardness | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D24](#d24) |
| F078 | Opacity | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D01](#d01) [D24](#d24) |
| F079 | Flow | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D24](#d24) |
| F080 | Roundness | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결하되 범위 불일치 | [D24](#d24) |
| F081 | Angle | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D03](#d03) [D24](#d24) |
| F082 | Spacing | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D24](#d24) |
| F083 | Scatter | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결 | [D24](#d24) |
| F084 | Bristle count | iiBrushTools 0.1.0 계약·설치 검증 완료 | 조건부 | [D03](#d03) [D24](#d24) |
| F085 | Stabilization | iiBrushTools 0.1.0 계약·설치 검증 완료 | 부분 연결 | [D03](#d03) [D05](#d05) [D24](#d24) |
| F086 | Post correction | iiBrushTools 0.1.0 계약·설치 검증 완료 | 다른 의미로 연결 | [D05](#d05) [D24](#d24) |
| F087 | Pressure sensitivity | iiBrushTools 0.1.0 계약·설치 검증 완료 | 부분 연결 | [D03](#d03) [D24](#d24) |
| F088 | Tilt sensitivity | iiBrushTools 0.1.0 계약·설치 검증 완료 | 입력 단절 | [D03](#d03) [D24](#d24) |
| F089 | Velocity response | iiBrushTools 0.1.0 계약·설치 검증 완료 | 입력 시간 부정확 | [D03](#d03) [D05](#d05) [D24](#d24) |
| F090 | Wetness | iiBrushTools 0.1.0 계약·설치 검증 완료 | 기존 매핑 연결 / 앱 키트 샘플러 구버전 | [D04](#d04) [D24](#d24) |
| F091 | Mix | iiBrushTools 0.1.0 계약·설치 검증 완료 | 기존 매핑 연결 / 앱 키트 샘플러 구버전 | [D04](#d04) [D24](#d24) |
| F092 | Load | iiBrushTools 0.1.0 계약·설치 검증 완료 | 기존 매핑 연결 / 앱 키트 샘플러 구버전 | [D04](#d04) [D24](#d24) |
| F093 | Dilution | iiBrushTools 0.1.0 계약·설치 검증 완료 | 기존 매핑 연결 / 앱 키트 샘플러 구버전 | [D04](#d04) [D24](#d24) |
| F094 | Start | iiBrushTools 0.1.0 계약·설치 검증 완료 | 연결하되 단위 불명확 | [D24](#d24) |
| F095 | End | iiBrushTools 0.1.0 계약·설치 검증 완료 | 미소비 | [D24](#d24) |
| F096 | Minimum size | iiBrushTools 0.1.0 계약·설치 검증 완료 | 조건부 | [D24](#d24) |
| F097 | Taper strength | iiBrushTools 0.1.0 계약·설치 검증 완료 | 부분 연결 | [D24](#d24) |

### 타임라인/레이어

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F098 | 레이어 추가·복제·삭제 | 기반 있음 | 정적 레이어 연결 | [D18](#d18) |
| F099 | 레이어 이름·가시성·순서 | 기반 있음 | 연결 | [D16](#d16) |
| F100 | 레이어 opacity | 기반 있음 | 연결 | [D23](#d23) |
| F101 | 전체 혼합 모드 | 기반 있음 | 일부만 연결 | [D23](#d23) |
| F102 | 그룹과 Pass | 정규 타입 부족 | 비활성 | [D15](#d15) |
| F103 | 래스터·벡터·필터 마스크 | 기반 있음 | 비활성 | [D17](#d17) |
| F104 | 클리핑 마스크 | 기반 있음 | 비활성 | [D17](#d17) |
| F105 | 잠금 | 정규 정책 부족 | 비활성 | [D16](#d16) |
| F106 | 레이어 링크 | 정규 정책 부족 | 비활성 | [D16](#d16) |
| F107 | 콘텐츠 키프레임 추가·삭제 | 기반 있음 | 비활성 | [D18](#d18) |
| F108 | 모션 키프레임 추가·삭제 | 기반 있음 | 비활성 | [D18](#d18) |
| F109 | 어니언 스킨 | 렌더링 기반 있음 | 비활성 | [D19](#d19) |
| F110 | 이전/다음 프레임·시간 스크럽 | 기반 있음 | 연결 | [D18](#d18) |
| F111 | 재생 | 기반 있음 | 기본 연결 | [D20](#d20) |
| F112 | 초 단위 눈금·프레임 틱 | 기반 있음 | 연결 | [D33](#d33) |
| F113 | 틱의 프레임 레이트 변경 | 유리수 기반 있음 | 정수 레이트만 연결 | [D18](#d18) [D33](#d33) |
| F114 | 타임라인 확대·위치 슬라이더 | 기반 있음 | 연결 | [D33](#d33) |
| F115 | 타임라인 스냅 | 일부 기반 있음 | 비활성 | [D18](#d18) [D27](#d27) |
| F116 | 패널 스택·더보기 | 기반 있음 | 제한 연결 | [D33](#d33) |
| F117 | Audio 행·음성 클립 | 모델 있음/네이티브 코덱 부족 | 미연결 | [D20](#d20) [D31](#d31) |
| F118 | Camera 행·Object 표시 | 의미와 타입 부족 | 미연결 | [D21](#d21) [D33](#d33) |
| F119 | Color balance 조정 레이어 | 커널 있음/정규 타입 부족 | 미연결 | [D22](#d22) |

### 뷰/패널

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F120 | 확대·축소·맞춤·팬 | 기반 있음 | 연결 | [D27](#d27) |
| F121 | 뷰 각도·회전 | 부분 | 읽기 전용/미연결 | [D09](#d09) |
| F122 | 캔버스 하단 반전 | 기반 있음 | 의미 확인 필요 | [D09](#d09) [D33](#d33) |
| F123 | Tool Doc View More 세그먼트 | UI 기반 있음 | 기본 연결 | [D33](#d33) |
| F124 | HSB 필드·색조·알파 | 기반 있음 | 연결 | [D02](#d02) [D26](#d26) |
| F125 | 색상 모드 선택 | 기반 있음 | 드롭다운 동작 미연결 | [D26](#d26) |
| F126 | Tool 그룹·압력 토글·복제 소스 | 일부 기반 있음 | 의미/연결 부분 | [D10](#d10) [D24](#d24) [D33](#d33) |
| F127 | 파라미터 리셋·저장·프리셋 | 기반 있음 | 수치만 연결 | [D25](#d25) |
| F128 | 스크롤·키보드·포커스·접근성 | 부분 지원 | 대상별 검증 필요 | [D32](#d32) [D34](#d34) |

### 기반 품질

| ID | 기능 | SDK 현행 | 앱 현행 | 관련 항목 |
|---|---|---|---|---|
| F129 | GPU 기반 표시와 문서 처리 | 표시 있음/문서 GPU 처리 부족 | 표시 연결 | [D29](#d29) |
| F130 | 큰 문서 Undo와 비동기 미리보기 | 공통 영역 이력 있음 / Native·파일 미전환 | 제한 연결 | [D28](#d28) [D30](#d30) |

[CSV 대조표](SDK_CAPABILITY_MATRIX_2026-10-27.csv)에는 130개 행의 Figma 노드 ID·담당 모듈·세부 판단·소스 근거를 담았다. 파라미터의 SDK 준비와 앱 채택은 별도 열이다.

## 검증과 증거의 범위

이번 재검토에서는 Figma의 두 화면을 새로 읽고 원본 크기 PNG를 보존했으며 101개 참조 노드가 모두 존재함을 검사했다. 현재 공개 헤더/실제 구현의 소스 앵커 **75개**, 모듈 소스 **13개**, 앱 및 플랫폼 키트와 보강 스테이지의 실제 패키지 버전을 읽었다. 기존 소스 해시가 달라진 입력·시계·샘플러·이력·GUI 경계를 다시 확인했고 이미지 코덱 소스 이동도 반영했다.

이번에 실행한 설치 소비자 검사는 **BrushTools 6/6**, **Native Canvas 4/4**이다. 강제 동적 라이브러리 경로 변수를 제거하고 기존 동결 스테이지로 빌드된 소비자를 실행했다. configure/전체 SDK build/ASan/기기 검사를 이번 감사에서 새로 수행했다는 의미는 아니다. 이전 단계의 전체 검사·ASan·설치 검증은 각 verification.json의 기록으로 구분했다. 현재 앱 소스가 해당 소비자 API를 사용한다는 증거로 확대하지 않았다.

D28a의 이전 8K 검사는 고엔트로피 ARGB32 두 plane과 float64 한 plane, 총 1 GiB 메모리 소스에서 48개 명령의 이력 약 1.67 MiB를 검증한 것이다. 실제 필터 실행·파일 I/O·브러시 취소나 프로세스 RSS 전체를 측정한 결과가 아니다. 병렬 전체 검사에서 적용 최댓값 **365,074µs**도 관측되었고 별도 단독 실행에서는 **61..118µs**였다. 큰 편차와 측정 범위가 그대로 남아 있으므로 최악 지연이나 제품 응답성 충족을 선언하지 않는다.

재검토 증거는 `build/sdk-capability-review-20261027/`에 있으며 다음을 포함한다.

- `figma-main.json`, `figma-tool.json`, `figma-references.json`, `figma-main.png`, `figma-tool.png`: 이번 실시간 Figma 원본이다.
- `source-evidence.json`, `module-snapshot.json`, `checkpoint-review.json`: 현재 소스 경로·앵커·해시·키트 버전·기존 체크포인트의 검토이다.
- `installed-brush-ctest.log`, `installed-canvas-ctest.log`: 이번에 재실행한 설치 소비자 검사이다.
- `gaps.json`, `features.json`, `report-validation.json`: 34개 추적 항목·130행 대조표와 연결 무결성 검증이다.
- `collect_review.py`, `write_report.py`, `validate_report.py`: 출처 수집·생성·검증 절차이다. 생성기는 보존한 원본으로 작성하며 Figma를 수정하지 않는다.

보고서 갱신 후에는 프로젝트의 Markdown/agent JSON 쌍과 색인을 문서 도구로 동기화하고 원문 해시·스키마·왕복 복원을 검사한다. 기존 2026-10-26 감사, 보강 스테이지, 앱 빌드와 사용자 변경은 보존한다.

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
- **E06** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:458](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:458) — `bool BitmapEditor::advanceStrokeTime`

<a id="e07"></a>
- **E07** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:729](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.cpp:729) — `samples = projectBrushDabs`

<a id="e08"></a>
- **E08** [SDK/iiPaintEngine/src/Stroke/Rasterizer.cpp:136](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Stroke/Rasterizer.cpp:136) — `std::uint32_t wetDabColorArgb`

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
- **E16** [SDK/iiPaintEngine/src/Selection/Selection.cpp:110](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Selection/Selection.cpp:110) — `Types::Scalar selectionAlphaAt`

<a id="e17"></a>
- **E17** [SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:92](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:92) — `void applyFill`

<a id="e18"></a>
- **E18** [SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:105](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Tool/RasterEditTool.cpp:105) — `void applyGradient`

<a id="e19"></a>
- **E19** [SDK/iiPaintEngine/src/Render/GpuRenderer.cpp:39](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/Render/GpuRenderer.cpp:39) — `result.layer = compositeLayerStack`

<a id="e20"></a>
- **E20** [SDK/iiVisualCodec/src/iiVisualCodec/ImageCodec.h:13](/Volumes/Storage/Workspace/SDK/iiVisualCodec/src/iiVisualCodec/ImageCodec.h:13) — `enum class PixelFormat`

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
- **E30** [SDK/iiGUI/src/iiGUI/NativeWidgetsIOS.mm:52](/Volumes/Storage/Workspace/SDK/iiGUI/src/iiGUI/NativeWidgetsIOS.mm:52) — `- (void)sample:`

<a id="e31"></a>
- **E31** [SDK/iiGUI/docs/IPADOS.md:17](/Volumes/Storage/Workspace/SDK/iiGUI/docs/IPADOS.md:17) — `UIKeyInput 경로는`

<a id="e32"></a>
- **E32** [SDK/LVRS/docs/native/Parity.md:3](/Volumes/Storage/Workspace/SDK/LVRS/docs/native/Parity.md:3) — `기준은 현재 QML`

<a id="e33"></a>
- **E33** [SDK/LVUI/README.md:70](/Volumes/Storage/Workspace/SDK/LVUI/README.md:70) — `Dynamic composition and the full lifecycle remain incomplete`

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
- **E41** [SDK/iiSharedCanvas/docs/NATIVE.md:41](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/docs/NATIVE.md:41) — `네이티브와 기존 Qt 패키지는`

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
- **E57** [SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.cpp:644](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.cpp:644) — `const StrokeSourceSnapshot source`

<a id="e58"></a>
- **E58** [SDK/iiGraphicAdjust/src/iiGraphicAdjust/SampleFilters.h:48](/Volumes/Storage/Workspace/SDK/iiGraphicAdjust/src/iiGraphicAdjust/SampleFilters.h:48) — `sampleAverage16(`

<a id="e59"></a>
- **E59** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:733](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:733) — `struct TimelineTrackProperties`

<a id="e60"></a>
- **E60** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:585](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:585) — `struct TimelineAudioMix`

<a id="e61"></a>
- **E61** [SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:630](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Timeline/TimelineProject.h:630) — `struct TimelineSubtitleClip`

<a id="e62"></a>
- **E62** [SDK/iiSharedCanvas/docs/AUDIO_TIMELINE.md:5](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/docs/AUDIO_TIMELINE.md:5) — `별도의 `TimelineProject` 모델과 독립적인`

<a id="e63"></a>
- **E63** [SDK/iiPaintEngine/src/History/RasterEditHistory.h:50](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/History/RasterEditHistory.h:50) — `class RasterEditHistory`

<a id="e64"></a>
- **E64** [SDK/iiBrushTools/src/iiBrushTools/Controls.h:26](/Volumes/Storage/Workspace/SDK/iiBrushTools/src/iiBrushTools/Controls.h:26) — `class Controls final`

<a id="e65"></a>
- **E65** [SDK/iiBrushTools/src/iiBrushTools/Gesture.h:42](/Volumes/Storage/Workspace/SDK/iiBrushTools/src/iiBrushTools/Gesture.h:42) — `renderGesture`

<a id="e66"></a>
- **E66** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.h:74](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.h:74) — `bool beginStrokeInput`

<a id="e67"></a>
- **E67** [SDK/iiSharedCanvas/src/Native/DocumentFile.cpp:480](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Native/DocumentFile.cpp:480) — `Document draft = m_impl->document`

<a id="e68"></a>
- **E68** [SDK/iiVisualCodec/CMakeLists.txt:94](/Volumes/Storage/Workspace/SDK/iiVisualCodec/CMakeLists.txt:94) — `add_library(iiImageCodec INTERFACE)`

<a id="e69"></a>
- **E69** [Product/Congregation/App/Native/Artwork.cpp:271](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Artwork.cpp:271) — `bool Artwork::commit`

<a id="e70"></a>
- **E70** [SDK/iiVisualCodec/src/iiImageCodec/ImageCodec.h:3](/Volumes/Storage/Workspace/SDK/iiVisualCodec/src/iiImageCodec/ImageCodec.h:3) — `#include <iiVisualCodec/ImageCodec.h>`

<a id="e71"></a>
- **E71** [SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.h:126](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/BitmapEditor.h:126) — `std::vector<std::shared_ptr<const RasterLayer>> m_undoHistory`

<a id="e72"></a>
- **E72** [SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.h:90](/Volumes/Storage/Workspace/SDK/iiSharedCanvas/src/Bitmap/ChunkedBitmapEditor.h:90) — `m_undoHistory`

<a id="e73"></a>
- **E73** [Product/Congregation/App/Native/Artwork.cpp:594](/Volumes/Storage/Workspace/Product/Congregation/App/Native/Artwork.cpp:594) — `raster_.beginStroke(point, pressure)`

<a id="e74"></a>
- **E74** [Product/Congregation/CMakeLists.txt:47](/Volumes/Storage/Workspace/Product/Congregation/CMakeLists.txt:47) — `find_package(iiSharedCanvasNative 0.32`

<a id="e75"></a>
- **E75** [SDK/iiPaintEngine/src/History/RasterEditHistory.h:10](/Volumes/Storage/Workspace/SDK/iiPaintEngine/src/History/RasterEditHistory.h:10) — `enum class RasterHistoryOverflow`

