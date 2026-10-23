# Congregation 1.0.0 네이티브 앱 구조

실행 소스는 `App/`이다. C++23과 iiSDKCore, LVUI, LVRS Native, iiGUI를 사용한다. 화면은 브라우저나 WebView 없이 네이티브 창의 GPU 표면에 그린다.

| 경로 | 역할 |
| --- | --- |
| `App/Engine/` | Core 수명, QueryPort, 협업·탐색, 설정·복구·라이선스와 문서 메타데이터 |
| `App/Native/Application.cpp` | 앱·창·서비스 조립, 대화상자·메뉴·클립보드·드롭·명령 |
| `App/Native/EditorBackend.*` | LVUI의 스코프 소유 백엔드와 반응형 값·목록 모델 |
| `App/Native/Artwork.*` | 문서 편집, 실행 취소, 파일 이행과 가져오기·내보내기 |
| `App/Native/CanvasView.*` | 화면 영역의 타일, 포인터·펜·도구·뷰포트와 선택 표시 |
| `App/Native/Shapes.*`, `LegacyCanvas.cpp` | 벡터 도형 정책과 이전 문서의 검증·이행 |
| `App/Native/views/` | HTML 요소·인라인 스타일의 Main, Preferences, CanvasSetup LVUI |
| `cmake/` | Qt 없는 의존성 감사, 번들 메타데이터와 네이티브 패키징 |
| `tests/` | 엔진·서비스·편집·실제 GUI·번들 재배치 검증 |
| `resources/` | 앱 아이콘과 제품 자원 |
| `build/` | 생성 C++·SDK 소비 결과·앱 하나·검사 자료 |

하위 SDK는 제품 정책을 알지 않는다. 파일·잠금·SQLite는 iiFileProvider, 문서·합성은 iiSharedCanvas Native, 브러시 엔진은 iiPaintEngine, 코덱·ICC는 iiImageCodec에 둔다. Core는 이벤트 루프·작업·관측 값을 제공하고 iiGUI는 OS 창·입력·텍스트·GPU 표시·운영체제 서비스를 제공한다. LVRS는 위젯과 모델, LVUI는 선언 구문과 C++ 생성을 담당한다.

주 창과 보조창은 Core 루프를 공유한다. 화면 이동·확대·선택과 문서 편집의 리비전을 분리한다. 문서 저장은 정규 IISC이며 내부 복구본·이전 VRC·PSD·ORA·비트맵을 경계에서 검증한다. 실제 제품 기능과 제한은 [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md), 엔진 소유 관계는 [APP_ENGINE.md](APP_ENGINE.md), 빌드는 [BUILD.md](BUILD.md)에 있다.

`src/App/`과 이전 QML은 비교와 기존 변경 보존을 위한 자료이다. 현재 타깃과 리소스에 포함하지 않는다. 이전 구조 문서는 [legacy/app_structure.md](legacy/app_structure.md)에 보존한다.
