# Congregation 기준 iiSDK의 Qt 독립 실행 조사

조사일: 2026-10-23. 대상은 `/Volumes/Storage/Workspace/Product/Congregation`의 현재 소스, 관련 iiSDK 소스, `~/.local/SDK` 설치 패키지 및 기존 빌드 산출물이다. 조사 중 다른 작업에 의한 SDK 변경도 관측되어, 판정에 사용한 파일의 해시는 `build/qt-independence-audit/source-evidence.json`에 별도로 고정한다.

## 이행 후 상태 · 2026-10-23

이 문서의 아래 조사 본문은 **구현 전 스냅샷**이다. 당시의 “현재”·경로·설치본·실패 결과는 이행 전 상태를 뜻한다. 현재 실행 소스는 `App/`이며 아래 G01–G09에 대해 다음을 구현했다. 확정된 검사 결과와 플랫폼 경계는 [NATIVE_VERIFICATION.md](NATIVE_VERIFICATION.md)를 따른다.

| 부족 영역 | 현재 구현 |
| --- | --- |
| G01 | iiSharedCanvas Native의 Qt 없는 문서·SQL 저장·편집·타일 렌더링·정규 IISC와 설치 소비자 |
| G02 | 위젯 트리 안의 타일 캔버스, 유한·무한 좌표, 스트로크·레이어·도형·텍스트·변환·실행 취소 |
| G03 | 압력·장치·기울기·회전·시간·버튼·보조키·포커스 취소, 뷰포트와 문서 변경 분리 |
| G04 | HTML/인라인 LVUI, Core Observable 바인딩, 스코프 소유 백엔드·목록·계층·색상 선택 모델 |
| G05 | 비트맵·PSD·ORA, RGB ICC·방향·해상도, 프로필 썸네일·네이티브 텍스트와 PDF 스마트 오브젝트 |
| G06 | 네이티브 메뉴·단축키·대화상자·클립보드·드롭·보조창·다중행/보안 입력과 IME 경계 |
| G07 | Cocoa/Metal 실제 실행, 강제 Vulkan 실제 실행과 효과 픽셀 검사, Win32/D3D11·Linux X11/Vulkan 소스 |
| G08 | Workspace 내부 설치 키트, 최소 macOS 12.0, ABI 검사·키트 해시·상대 의존성 번들·로컬 서명 |
| G09 | Qt 탐색을 끈 실제 제품 검사, 설치·재배치 소비자, Metal/Vulkan 편집 흐름·번들 재배치 실행·활성 빌드/워크플로의 Qt 획득 차단 |

Windows·Linux의 실제 실행 검증은 사용자의 “소스 코드 구현만 하고 넘어가라” 지시로 이번 범위에서 제외했다. 물리 펜·구형 macOS 실기기·실운영 외부 서비스의 검증은 자동 입력과 현재 Mac의 결과로 대체하지 않는다. LVRS 전체 선언의 완전한 동등성이나 Linux OS 접근성의 완성도까지 완료했다고 주장하지 않는다.

## 이하: 구현 전 조사 기록

## 판정

**현재 Congregation은 자체 SDK만으로 실행되는 제품이 아니다.** 네이티브 엔진의 분리는 존재하지만, 실행 진입점, QML 화면, SharedCanvas, 일부 이미지 처리와 배포 과정이 Qt를 요구한다.

자체 SDK로 기본 창을 띄우는 기반은 이미 존재한다. iiSDKCore의 Application·EventLoop·작업 수명과 iiGUI의 네이티브 Host, Metal/D3D11/Vulkan 렌더러, LVUI AOT 및 LVRS 네이티브 위젯을 재사용할 수 있다. Congregation에 부족한 것은 이 기반 위의 실제 편집 캔버스, 제품 입력 계약, 동적 화면 연결과 재배포 가능한 런타임 묶음이다.

완료 기준은 세 단계로 나눈다.

1. **기동:** Qt가 없는 구성에서 Congregation 실행 파일을 만들고, 네이티브 주 창을 표시한 뒤 정상 종료한다.
2. **실용 실행:** 새 캔버스, 펜·마우스 편집, 레이어·개체 선택, 실행 취소·재실행, 열기·저장·재열기를 제공한다.
3. **현재 제품의 대체:** 현재 노출한 이미지/PSD 형식, 프로필·환경설정, 라이선스·업데이트, 협업·초대, 메뉴·단축키·보조창을 유지하고 플랫폼별 독립 배포를 검증한다.

빈 창의 기동과 현재 제품 전체의 대체를 같은 완료 상태로 취급하지 않는다. 모든 Qt 클래스와 LVRS 전체를 재구현하는 것도 이 조사의 완료 조건은 아니다.

## 현재 상태를 확인한 증거

| 관측 대상 | 결과 | 의미 |
| --- | --- | --- |
| `src/Engine` 직접 Qt include | 0개 파일 | 분리된 앱 엔진에는 Qt 헤더가 없다. SDK의 전이 의존성과 전체 제품의 Qt 부재까지 증명하지는 않는다. |
| `src`의 직접 Qt include | C++/헤더 37개 파일 | 호스트·QObject 어댑터·영상/화상 처리·입력이 남아 있다. |
| 앱 선언 | `Q_PROPERTY` 74개, `Q_INVOKABLE` 60개 | QML과 C++ 사이의 계약을 표준 값·명시적 메서드·관측으로 이행해야 한다. 상속한 SharedCanvas/LVRS 멤버는 이 수에 포함하지 않았다. |
| QML | 9개 파일, 8,676행, 함수 선언 349개 | 화면 구조뿐 아니라 편집·선택·변환·입력 로직도 QML 안에 있다. 함수 수는 정규식으로 센 선언 수이며 복잡도나 작업 기간의 추정값은 아니다. |
| Congregation 실행 파일 | Qt 프레임워크 11종을 직접 링크 | `QGuiApplication`/`QQmlApplicationEngine` 진입점 및 Qt Quick 화면이 실제 바이너리에 남아 있다. |
| 기존 앱 번들 | `Qt*.framework` 디렉터리 25개 | 직접 링크 이외의 QML·플러그인 런타임도 포함한다. 이 숫자는 디렉터리 수이며 모두 실행 중 로드되었다는 뜻은 아니다. |
| Congregation이 인스턴스화하는 LVRS 타입 | 22종 중 `partial` 17종, `pending` 5종 | 기본 구현이 있는 타입도 앱이 쓰는 속성·이벤트·동적 모델의 동등성을 보장하지 않는다. |
| LVUI AOT의 실제 도구 실행 | Label 생성 성공, Hierarchy·`var`·일반 C++ 속성 바인딩 거부 | 선언 인식, C++ 생성 지원, 실제 제품의 동작 지원은 서로 다른 범위이다. |

인벤토리는 [inventory.json](../build/qt-independence-audit/inventory.json), AOT 입력과 오류는 [aot-probes.json](../build/qt-independence-audit/aot-probes.json)에 있다. QML 타입 집계는 소스 문법에 대한 정적 조사이며 전체 화면을 실제로 조작한 결과가 아니다.

## 이미 존재하여 재사용할 영역

| 영역 | 현재 제공자 | Congregation에서 남은 일 |
| --- | --- | --- |
| 앱 수명·작업·타이머·프로세스·객체 간 호출 | iiSDKCore Runtime, Application, EventLoop, HostLoop, TaskScope, QueryPort | `QtEngineHost`를 iiGUI의 Core 루프 연결로 바꾸고 기존 엔진의 소유 관계를 유지한다. 별도 실행기나 전역 이벤트 버스를 추가할 이유가 없다. |
| TCP/UDP/HTTP/TLS·협업 메시지 | iiNetwork, 엔진의 CanvasSession/NearbyDiscovery | 네이티브 서비스의 결과를 Qt-free 화면에 연결한다. TLS 라이브러리 배치는 별도의 실제 차단 사항이다. |
| 계정·보안 저장소·인증서·업데이트 | iiAccountManager/Native, iiLicenseManager/Native, iiUpdateManager/Native | QObject 어댑터·오류 표현·창 수명을 대체하고 새 설치 패키지를 제품에 선택한다. 외부 서비스의 실운영 성공은 이번 조사 범위가 아니다. |
| 파일·임시/캐시·원자적 저장·기존 잠금·SQLite | iiFileProvider Native/CoreIO/Compatibility | SharedCanvas가 참조하는 기존 Qt 제공자와 파일 구현을 네이티브 계약으로 이행한다. |
| JSON·XML·표준 값 | iiJson, iiXml Native, Core Value 및 기존 엔진 코덱 | SharedCanvas와 잔여 어댑터의 Qt 사용을 옮긴다. 새 JSON/XML 엔진이 필요한 상황이 아니다. |
| 브러시·압력·보정·래스터 편집·레이어 합성·이력 | iiPaintEngine | 네이티브 입력과 문서/화면 어댑터를 연결한다. `StrokePoint`에는 이미 압력·시간·기울기·회전 등이 있다. 실제 OS 이벤트가 이를 공급하는 경계가 부족하다. |
| 색상 모델·선택기 | iiColorManager, iiGUI/LVRS ColorPicker | 앱의 QML HSL 삼각형·팔레트·브러시 상태와 연결하고 현재 동작을 회귀 검사한다. 범용 ICC 변환 구현이 있다는 뜻은 아니다. |
| PNG/JPEG 메모리 코덱 | iiImageCodec | 파일 제공자와 결합해 사용한다. Qt의 모든 이미지 플러그인과 메타데이터 처리까지 대체하지는 않는다. |
| 네이티브 창·GPU·텍스트·이미지·팝업·기본 위젯 | iiGUI, LVRS native, LVUI AOT | 현재 제품의 화면과 입력을 연결한다. 기본 창·버튼·텍스트·이미지·팝업 전체를 새로 만들 필요는 없다. |

## 부족 영역과 책임

P0는 해당 완료 단계의 직접 차단 사항이다. P1은 현재 제품의 사용 흐름을 유지하기 위해 필요한 기능 또는 이행이다. Windows/Linux 전용 P0는 macOS의 단일 창 기동을 차단한다는 뜻이 아니다.

### G01 · SharedCanvas의 Qt-free 문서·저장 경계 — P0

`iiSharedCanvas::Core`는 이름과 달리 Qt-free 타깃이 아니다. 현재 `CMakeLists.txt`는 `IISHAREDCANVAS_BUILD_QUICK=OFF`이어도 Qt Core·Gui를 찾으며, Core 타깃은 `iiFileProvider::iiFileProvider`, `Qt6::Core`, `Qt6::Gui`를 PUBLIC으로 연결한다. 설치 Config도 Core·Gui를 요구한다. 따라서 Quick 어댑터만 끄는 방법으로는 독립 실행이 되지 않는다.

문서·레이어·변환·타일 렌더링의 표준 C++ 구현과 공개 값은 상당 부분 존재한다. 이 코드를 보존하면서 `Document.h`의 제공자 헤더, 문서 파일의 Qt 해시, 이미지 코덱, 벡터/계층 문서의 Qt 파일·XML·문자열 경계를 분리해야 한다. Qt 의존성이 있는 일부 기능을 큰 Core 타깃에 함께 넣은 빌드 경계도 조정해야 한다.

소유자는 **iiSharedCanvas**이다. 해시·파일·JSON/XML·SQLite는 기존 동위 SDK에 의존한다. 하위 iiSDKCore가 SharedCanvas나 GUI를 참조하도록 바꾸지 않는다.

완료 조건은 Qt5/Qt6 탐색을 끈 구성·설치 소비자에서 실제 `.iisc` 생성 → 편집 → 저장 → 재열기, 파일 호환성·잠금·손상 입력 처리, 전이 타깃/바이너리의 Qt 부재를 확인하는 것이다.

### G02 · 네이티브 편집 캔버스와 위젯 트리의 연결 — P0

Congregation의 `DrawingSurfaceItem`은 `iiSharedCanvas::CanvasItem`을 상속하고, 그 기반은 `QQuickItem`이다. 따라서 기존 캔버스를 새 창에 단순 부착할 수 없다. 확대/이동, 무한 캔버스, 레이어와 개체 변환, 라이브 스트로크, 선택 핸들, 텍스트·도형, 썸네일과 오프스크린 출력의 계약을 네이티브 경계로 옮겨야 한다.

iiGUI에는 Panel/TextRun/ImageRun, 오버레이·클리핑, 이미지 위젯이 있다. 조사 후반에 확인한 소스에는 `WidgetDrawing`, `set_drawing`, `set_input_handler`, 가상 스크롤 관련 계약도 존재한다. 이 기반은 재사용 대상이다. 그러나 SharedCanvas 문서/타일과 Congregation의 편집 명령을 연결한 제품용 어댑터와 실행 증거는 없다.

`Host::set_canvas()`는 성공하면 기존 위젯 트리를 닫고, `mount_widgets()`는 별도 Host 캔버스를 지운다. 툴바를 마운트한 뒤 별도 Host 캔버스를 설정하는 방식은 화면 구성 계약과 맞지 않는다. 위젯 트리 안의 그리기 경로로 연결하거나 실제로 필요한 캔버스 부착 계약을 제공해야 한다.

현재 `RasterImage`는 한 이미지당 2,048픽셀 한계를 갖는다. 앱의 8,192픽셀 캔버스와 무한 캔버스를 단일 이미지로 전달하는 방식은 맞지 않는다. 기존 SharedCanvas 타일을 재사용하고, 부분 갱신·리비전·텍스처 수명·화면 배율을 연결하여 검증하는 것이 우선이다. 무조건 이미지 한도를 키우는 것은 완료 조건이 아니다.

소유자는 **iiSharedCanvas의 편집/문서 API + iiGUI의 화면/입력 경계 + Congregation의 도구 정책**이다. 현재 Foreground 구현은 이미지/텍스트를 CPU 픽셀에 합성한 뒤 GPU로 전달한다. 대형 편집에서의 비용은 측정해야 하며, 이번 조사는 성능 저하를 실측한 결과가 아니다.

### G03 · 펜·포인터·단축키 입력 계약 — P0(편집)

앱은 `QTabletEvent::pressure()`, 마우스 버튼, Control/Meta+Space 임시 확대/이동, 휠의 각도/픽셀 델타, 도구별 단축키와 커서를 사용한다. iiGUI `WidgetInput`은 기본 포인터/키·Shift·세로 스크롤·문자 조합을 제공하지만, 현재 공개 계약에는 펜 압력·장치 식별·기울기·전체 보조키·버튼 집합·정밀 휠 및 앱 단축키 식별이 없다.

브러시 엔진에 압력 기능을 다시 추가하는 문제가 아니다. **iiGUI 플랫폼 입력 어댑터**가 네이티브 이벤트를 표준 값으로 전달하고, Congregation이 문서 좌표·도구·보조키 정책을 적용해야 한다. 현재 앱이 실제 사용하는 필드를 우선으로 하며, 사용하지 않는 제스처 전체를 선행 구현할 필요는 없다.

완료 조건은 동일 입력 시퀀스의 좌표/압력/취소 보존, 스트로크 도중 포커스 상실·창 닫기, 텍스트 편집 중 단축키 억제, 화면 이동에 따른 문서 dirty/revision 불변, 물리 펜 실기기 확인이다.

### G04 · LVUI/LVRS의 동적 화면과 모델 연결 — P1(제품 대체)

현재 AOT는 여러 위젯과 스코프 소유 C++ 백엔드, 지연 메서드 참조를 실제 C++ 호출로 생성한다. 다만 일반 `var`, `Label(text: vm.value)` 형태의 반응형 C++ 속성, 동적 자식/모델 전개는 아직 전체 구현되지 않았다. 실제 컴파일 도구에서도 이 입력은 오류를 반환했다.

Congregation이 사용하는 22종의 LVRS 시각 타입 중 `AppCard`, `EventListener`, `Hierarchy`, `List`, `ListItem`의 5종이 현재 대응표에서 `pending`이다. `Theme`는 부분 이식이며 `ViewModels`는 `pending`이다. 계층 목록은 `PainterCanvasPage.qml`의 레이어 모델에, List는 환경설정의 멤버 목록에 실제로 사용된다. 나머지 17종의 `partial`도 원본 속성·이벤트 전체가 같은 상태는 아니다.

QML의 Repeater/Loader/Connections/Binding/Timer 및 349개 함수의 역할을 나눠야 한다. 제품의 편집 정책은 C++ 컨트롤러로 옮기고, 화면 구조는 LVUI에 둔다. Core의 QueryPort·TaskScope·속성 변경 기록과 LVRS의 기존 명시적 모델 관측을 재사용한다.

**일반 반응형 컴파일러 전체가 최소 창 기동의 필수 조건은 아니다.** C++에서 위젯을 명시적으로 갱신하고 모델 변화를 구독하는 경로로 먼저 독립 실행할 수 있다. 다만 현 화면을 선언적으로 유지하고 변경 비용을 낮추려면 타입이 있는 속성/목록 바인딩과 해제 계약을 제공하는 편이 적합하다. QML/JavaScript 호환 엔진을 새로 넣는 것이 요구사항은 아니다.

완료 조건은 레이어 추가·삭제·이름/선택/순서 변경과 협업 멤버 갱신이 화면에 반영되고, 창/뷰 해제 이후 콜백이 실행되지 않는 것이다. LVRS 전체 149개 선언의 완전 이식은 Congregation 실행의 선행 조건으로 잡지 않는다.

### G05 · 이미지·PSD·텍스트/도형 출력 — P1(형식 유지)

열기 UI는 `.iisc`, PNG/JPEG/BMP/GIF/WebP/TIFF/PSD를, 저장 UI는 PSD/PNG/JPEG/BMP/WebP/TIFF를 노출한다. iiImageCodec은 PNG/JPEG이며 iiPaintEngine은 추가 비트맵 읽기/일부 쓰기를 갖추었지만 WebP/TIFF와 계층 PSD 전체를 대체하지 않는다.

현재 PSD의 MolecularMatters SDK, 네이티브 I/O와 메타데이터 정책은 재사용 가능하다. 잔여 `PsdImageReader/Writer`의 QImage·QString 버퍼를 표준 RGBA/문자열로 바꾸는 이행이 필요하다. 이를 신규 PSD 엔진 부재로 분류하면 과대평가이다.

앱의 도형·다중행 텍스트·개체 회전/변환·썸네일 출력에는 QPainter/QPainterPath/QFont가 남아 있다. SharedCanvas의 표준 벡터/합성 로직, iiGUI 네이티브 조판, iiPaintEngine 래스터 기능을 연결하고 부족한 오프스크린 계약을 좁혀야 한다. `Panel`의 둥근 사각형만으로 모든 개체의 출력 동등성을 보장하지 않는다.

프로필 이미지는 QImageReader의 자동 방향 변환과 원형 자르기, 색공간/해상도 메타데이터 보존을 수행한다. 대체 코덱에서 이 계약도 검사해야 한다. 범용 HDR·인쇄용 ICC·PDF 전체가 현재 Congregation 실행의 필수라는 근거는 없으므로 별도의 선행 요구로 추가하지 않는다.

소유자는 **iiImageCodec의 코덱 계약, iiSharedCanvas의 문서/개체 출력, 앱의 PSD 호환 정책**이다. 완료 조건은 현재 노출한 형식의 실제 왕복, 투명도·방향·텍스트·레이어/메타데이터 호환성과 기존 기준 이미지 회귀이다.

### G06 · OS 서비스·보조창·텍스트 입력 — P0/P1

현재 앱은 열기/저장/프로필 파일 대화상자, 이미지와 로컬 URL 클립보드, 파일/웹 이미지 드롭, 앱 메뉴와 단축키, 환경설정 보조창, 최소화/전체 화면, 외부 URL 열기를 사용한다.

조사한 iiGUI 공개 Host/Widget 계약과 iiFileProvider Native에서는 이 요구를 묶어 제공하는 파일 선택·이미지 클립보드·드롭 API를 확인하지 못했다. FileProvider의 `Presentation`은 미리보기/세부 정보 부착 기능이며 파일 선택 대화상자가 아니다. iiGUI의 ContextMenu와 앱 수준 메뉴바도 같은 계약으로 취급하지 않는다.

macOS 위젯에는 `NSTextInputClient`, 문자 붙여넣기, 조합 입력과 가상 접근성 피어가 이미 있다. 그러나 IME replacement range 처리의 일부가 비어 있고, 다중행 편집·선택·복사/잘라내기·일반 validator의 동등성은 아직 없다. 기존 `TextEdit` 텍스트 도구와 인증 입력을 대상으로 한글/다국어·포커스·선택 검증이 필요하다.

소유자는 **iiGUI의 플랫폼 서비스와 창/입력 계약**이며, 파일의 읽기·원자적 저장·권한/스코프는 iiFileProvider에 유지한다. 기본 파일 열기/저장은 실용 실행의 P0, 보조창·메뉴·전체 제품의 세부 동등성은 P1이다. 보조창을 같은 Core 루프에서 열고 닫는 실제 수명 검증이 필요하다.

### G07 · 플랫폼 위젯과 Vulkan 폴백의 기능 범위 — P0(해당 플랫폼)

iiGUI는 macOS의 Metal, Windows의 D3D11, Vulkan의 네이티브 캔버스 경로를 갖는다. 그러나 CMake는 macOS 이외의 위젯 구현에 `NativeWidgetsUnsupported.cpp`를 선택하고, `mount_widgets()`는 `unsupported`를 반환한다. 기존 Windows/Linux Host와 GPU 캔버스의 존재가 현재 LVUI/LVRS 위젯 앱의 실행을 보장하지 않는다.

따라서 Windows/Linux Congregation에는 **플랫폼 위젯 표면의 입력·IME·접근성·수명 연결과 실제 실행**이 먼저 필요하다. Linux는 현재 X11/XWayland 경로이며 직접 Wayland, 모바일 호스트가 있다는 근거는 없다. 이들은 현재 데스크톱 목표와 분리한다.

추가로 Metal 전용 배경 블러·그림자 등의 기능은 Vulkan/D3D11에서 동일하게 구현되지 않았다. 해당 효과를 요구하는 LVRS 장면은 폴백에서도 실패할 수 있으므로, 실제 Congregation 장면을 강제 Vulkan으로 실행해 필요한 계약부터 맞춰야 한다. macOS의 MoltenVK는 Metal 위의 Vulkan 구현이므로 독립된 GPU 하드웨어 폴백으로 표현하지 않는다.

완료 조건은 각 목표 OS에서 동일 LVUI/C++ 화면을 빌드하고 실제 입력·렌더·종료를 확인하는 것이다. 강제 Vulkan과 네이티브 초기화/표시 실패 후 폴백도 별도로 검사한다. macOS 로그나 타 OS 소스의 존재를 Windows/Linux 실행 성공으로 집계하지 않는다.

### G08 · 설치 패키지·런타임 묶음·최소 OS — P0(독립 배포)

`~/.local/SDK/iiGUI`는 0.4.1 설치본이며 새 `NativeWidgets.h`가 없다. `~/.local/SDK/LVUI`는 없고, LVRS 설치본은 Qt 의존성을 갖는 기존 패키지이다. 현재 네이티브 소스/스테이지를 제품의 기본 설치 패키지로 바로 사용할 수 있는 상태가 아니다.

이름·버전이 같은 소스와 설치본의 기능 차이를 식별하고, Core/LVUI/iiGUI/LVRS 및 제품이 쓰는 동위 SDK를 일관된 키트로 제공해야 한다. 공통 최종 앱/리소스 부착 기능은 존재하지만, 전체 동적 라이브러리 수집·재배치·서명·실행 검증은 별도이다. 기존 CMake는 Qt 전용 실행 파일/QML 생성과 `macdeployqt` 배치를 호출한다.

설치된 Core 0.18.1, PaintEngine, FileProvider Native 0.8.1, SharedCanvas 0.31.2의 Mach-O는 최소 macOS 27.0을 선언한다. Congregation CMake의 기본값 12.0과 맞지 않는다. 제품의 지원 하한을 유지하려면 전체 네이티브 의존성을 같은 최소 OS로 빌드해 실제로 검증해야 한다.

기존 네이티브 테스트 실행 파일에는 `/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib`의 절대 의존성이 있고, 현재 파일이 없다. `DYLD_*`와 `LD_LIBRARY_PATH`를 모두 제거한 재실행에서도 두 테스트 모두 본문 실행 전에 로더 오류로 종료했다. TLS 기능의 부재가 아니라 패키징/설치 경계의 실제 차단 사항이다.

iiGUI의 현재 Vulkan 경로도 외부 `iiDecision/build/dependencies/MoltenVK` 키트를 가리킨다. 네이티브 기본 경로를 쓸 때도 링크 시 선택한 동적 라이브러리가 프로세스 시작에 필요할 수 있다. OpenSSL·MoltenVK·코덱·SDK 동적 라이브러리 전체를 배포 묶음과 RPATH 감사 대상으로 삼아야 한다.

완료 조건은 개발 소스·`~/.local`·Homebrew·임시 키트를 런타임에 참조하지 않는 재배치 소비자와 최종 Congregation 앱의 실행이다. OS 표준 라이브러리와 시스템 프레임워크 의존성은 정상적인 네이티브 배포 경계이다.

### G09 · Qt 부재와 제품 동작을 함께 보증하는 검사 — P0(완료 판정)

현재의 네이티브 엔진 검사와 별도로, 최종 Congregation의 헤더/타깃/실행 파일/동적 플러그인 전체에 적용할 독립성 검사가 필요하다. 빌드 성공만으로 설치·실행·호환성 성공을 대체하지 않는다.

필요한 검사는 다음과 같다.

- Qt5/Qt6 탐색 비활성화, Qt 타깃·공개 헤더·심볼·런타임 플러그인의 부재.
- SDK 설치 후 위치를 바꾼 소비자, 생성된 C++만으로의 실행, LVUI 소스/컴파일러가 없는 런타임.
- 새 문서 → 펜/마우스 편집 → 레이어/개체 조작 → undo/redo → 저장 → 재열기.
- 확대/이동·순수 조회가 문서 revision/dirty/save를 바꾸지 않는 회귀.
- 라이선스/업데이트/협업의 취소·창 해제·종료 후 지연 콜백 차단.
- 필수 이미지 형식·PSD/최근 파일 호환성, 실제 한글 IME·클립보드·드롭·보조창.
- 네이티브/강제 Vulkan/실패 후 폴백의 장면·입력 검증, 목표 OS와 최소 OS의 실제 실행.
- canonical `.app` 하나, 전체 런타임 의존성, 서명·재배치 실행.

## 권장 실행 순서

1. **G08의 키트/로더 문제를 재현 가능하게 정리하고, Core + LVUI + iiGUI + LVRS native로 Congregation 주 창을 연결한다.** 기존 계정/라이선스 정책을 유지한다. 이 단계의 성공은 네이티브 기동이다.
2. **G01과 G02로 한 문서의 래스터 편집·레이어·undo/redo·저장/재열기를 닫는다.** 범용 UI 언어 기능 전체보다 제품의 핵심 작업을 먼저 검증한다. G03의 마우스/압력/보조키 전달을 함께 연결한다.
3. **G04와 G06의 실제 사용 목록을 이행한다.** 초기에는 명시적인 C++ 관측/갱신을 허용하고, 반복되는 화면 갱신 요구에 맞춰 LVUI 바인딩을 구현한다. AppCard의 단순 표면과 Hierarchy/List의 동적 모델은 별개 작업으로 취급한다.
4. **G05로 기존 파일 형식과 오프스크린 출력을 복구하고, 환경설정·협업·라이선스/업데이트 화면의 동등성을 확인한다.** 네이티브 서비스 자체를 중복 구현하지 않는다.
5. **G07을 플랫폼별로 완료하고 G08/G09의 배포/실행 검사를 통과시킨다.** macOS의 결과와 Windows/Linux의 결과를 구분해 보고한다.

이 순서는 제품 실행에 필요한 닫힌 경로를 우선한다. 새로운 범용 UI/그래픽/OS 서비스 프레임워크 전체를 선행 설계하는 작업은 제안하지 않는다.

## 이번 조사에서 실행한 검증과 한계

이번 작업에서 작성·갱신한 것은 이 조사 문서, `build/qt-independence-audit/`의 조사 자료와 기존 CTest 실행 로그이다. 제품/SDK 구현·기본 설치본·실행 중인 앱은 바꾸지 않았다. 제품이나 SDK의 전체 빌드를 이번 조사에서 새로 수행한 것은 아니다.

| 검사 | 이번 결과 | 범위 |
| --- | --- | --- |
| 현재 소스·CMake·설치 헤더/Config·기존 Mach-O 조사 | 완료 | `inventory.json`, `source-evidence.json` |
| AOT Label | C++ 생성 성공 | 기존 `lvui-compile` 도구 실행. 최종 C++ 앱 컴파일 결과는 아님. |
| AOT Hierarchy | 예상 미지원 진단 | 현재 제품이 사용하는 타입. |
| AOT `var`/일반 C++ 속성 | 예상 미지원 진단 | 일반 반응형 경계. 명시적 C++ 갱신과 특화 모델 관측은 존재한다. |
| 기존 native 테스트 2개 재실행 | 0/2, 모두 로더에서 중단 | OpenSSL 누락. 테스트 본문의 기능 회귀 여부는 판정할 수 없음. |
| 같은 native 테스트의 환경 변수 정리 후 실행 | 0/2, 같은 오류 | `native-artifact-clean-result.json`, XML, 로그. |
| 과거 Congregation GUI 22/22·native 2/2 로그 | 존재 확인 | 2026-10-20의 기록. 현재 성공으로 재사용하지 않음. |
| iiGUI/LVUI의 최근 성공 JUnit | 존재 확인 | 조사 시 iiGUI 159개, LVUI 32개 성공 기록. 이번 조사에서 전체 재실행한 결과가 아님. |

과거 native source proof의 46개 경로 중 27개만 해시가 그대로이며 6개는 변경, 13개는 원래 경로에 없다. 일부는 디렉터리 재구성 결과이다. 이를 소스 손실로 단정하지 않지만, 과거의 통과 로그가 현재 SDK 조합을 그대로 보증한다는 해석은 성립하지 않는다.

## 주요 근거 파일

- [앱 실행 진입점](../src/App/main.cpp), [앱 CMake](../CMakeLists.txt), [네이티브 엔진 타깃](../src/Engine/CMakeLists.txt), [Qt 엔진 호스트](../src/App/host/qtenginehost.cpp).
- [제품 캔버스 상속/계약](../src/App/models/painting/drawingsurfaceitem.h), [Qt 이미지/개체/입력 구현](../src/App/models/painting/drawingsurfaceitem.cpp), [QML 편집 로직](../src/App/qml/painting/DrawingSurface.qml).
- [계층/뷰모델 사용](../src/App/qml/canvas/PainterCanvasPage.qml), [파일 형식/대화상자](../src/App/qml/brush/CanvasToolBar.qml), [보조창/프로필/멤버 목록](../src/App/qml/preferences/PreferencesWindow.qml).
- [SharedCanvas 빌드 경계](../../../SDK/iiSharedCanvas/CMakeLists.txt), [설치 의존성](../../../SDK/iiSharedCanvas/cmake/iiSharedCanvasConfig.cmake.in), [문서 파일](../../../SDK/iiSharedCanvas/src/File/DocumentFile.cpp), [문서/타일 렌더 계약](../../../SDK/iiSharedCanvas/src/Render/FrameRenderer.h).
- [iiGUI Host](../../../SDK/iiGUI/src/iiGUI/Host.h), [위젯과 입력](../../../SDK/iiGUI/src/iiGUI/NativeWidgets.h), [비 macOS 위젯 구현](../../../SDK/iiGUI/src/iiGUI/NativeWidgetsUnsupported.cpp), [이미지 제한](../../../SDK/iiGUI/src/iiGUI/Image.cpp), [그리기/위젯 교체](../../../SDK/iiGUI/src/iiGUI/Host.mm).
- [LVUI 컴파일러](../../../SDK/LVUI/src/LVUI/Compiler.cpp), [네이티브 AOT 지원 계약](../../../SDK/LVUI/docs/LVRSNative.md), [LVRS 대응표](../../../SDK/LVRS/catalog/native/parity.json).
- [PaintEngine의 독립 기능/형식](../../../SDK/iiPaintEngine/README.md), [입력 값](../../../SDK/iiPaintEngine/src/Stroke/StrokePoint.h), [이미지 코덱 계약](../../../SDK/iiImageCodec/src/iiImageCodec/ImageCodec.h), [색상 계약](../../../SDK/iiColorManager/src/iiColorManager/ColorManager.h).

조사 수집기는 [collect.py](../build/qt-independence-audit/collect.py)이다. 조사 산출물의 무결성/링크/진단 검사는 `validation.json`에 기록한다. SDK와 앱 소스가 계속 변경될 수 있으므로 후속 구현은 이 조사 스냅샷과 현재 소스를 다시 대조해야 한다.
