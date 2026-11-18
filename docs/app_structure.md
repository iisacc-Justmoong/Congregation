# Congregation 1.0.0 네이티브 앱 구조

실행 소스는 `App/`이다. C++23과 iiSDKCore, LVUI, iiGUI를 사용한다. 화면은 브라우저나 WebView 없이 네이티브 창의 GPU 표면에 그린다.

| 경로 | 역할 |
| --- | --- |
| `App/Application/` | 앱·창·서비스 조립과 최상위 메뉴 |
| `App/Workspace/` | LVUI 작업 공간과 반응형 화면 상태 |
| `App/Canvas/`, `App/Document/` | 캔버스 입력·그리기와 문서 모델·명령·형식 |
| `App/Tools/`, `App/Timeline/`, `App/Color/` | 도구 설정, 타임라인, 색상과 관련 화면 |
| `App/Account/`, `App/Agent/`, `App/Collaboration/` | 계정, 에이전트, 협업 서비스 |
| `App/Assets/`, `App/Storage/`, `App/Preferences/`, `App/Update/` | 에셋, 저장소, 환경설정과 업데이트 |
| `App/Shared/` | 여러 도메인이 공유하는 화면·유틸리티 계약과 LVUI 레이아웃 프로필 |
| `cmake/` | Qt 없는 의존성 감사, 번들 메타데이터와 네이티브 패키징 |
| `tests/` | 엔진·서비스·편집·실제 GUI·번들 재배치 검증 |
| `resources/` | 앱 아이콘과 제품 자원 |
| `build/` | 생성 C++·SDK 소비 결과·앱 하나·검사 자료 |

각 도메인의 C++ 헤더는 구현과 나란히 두며, 도메인 간 경로와 소유권은 [앱 소스 도메인 구조](APP_DOMAINS.md)를 따른다. 하위 SDK는 제품 정책을 알지 않는다. 파일·잠금·SQLite는 iiFileProvider, 문서·합성은 iiSharedCanvas Native, 브러시 엔진은 iiPaintEngine, 코덱·ICC는 iiImageCodec에 둔다. Core는 이벤트 루프·작업·관측 값을 제공하고 iiGUI는 OS 창·입력·텍스트·GPU 표시·운영체제 서비스를 제공한다. LVUI는 위젯과 모델, 선언 구문 및 C++ 생성을 담당한다.

주 창과 보조창은 Core 루프를 공유한다. 화면 이동·확대·선택과 문서 편집의 리비전을 분리한다. 문서 저장은 정규 IISC이며 내부 복구본·이전 VRC·PSD·ORA·비트맵을 경계에서 검증한다. 실제 제품 기능과 제한은 [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md), 엔진 소유 관계는 [APP_ENGINE.md](APP_ENGINE.md), 빌드는 [BUILD.md](BUILD.md)에 있다.

`src/App/`과 이전 QML은 비교와 기존 변경 보존을 위한 자료이다. 현재 타깃과 리소스에 포함하지 않는다. 이전 구조 문서는 [legacy/app_structure.md](legacy/app_structure.md)에 보존한다.
