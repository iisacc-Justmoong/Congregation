# iiSDKCore 기반 앱 엔진

Congregation의 실행 소스는 `App/Engine`과 `App/Native`이다. 최종 진입점, 이벤트 루프, 작업 수명은 iiSDKCore가 소유한다. iiGUI Host는 같은 Core 루프를 네이티브 창과 GPU 표시 경로에 연결한다. Qt 호스트·QObject 어댑터·QML 엔진은 현재 실행 경로에 없다.

## 소유 관계

| 소유자 | 책임 |
| --- | --- |
| `ApplicationEngine` | 소유 또는 차용한 Core EventLoop, TaskScope, 시작·종료와 제품 서비스 수명 |
| `CanvasDocument`, `Palette` | 타입이 있는 QueryPort, 브러시·압력·도구·팔레트 상태 |
| `CanvasSession`, `NearbyDiscovery` | TCP 협업, 정규 문서·기준 리비전, 멤버·초대와 LAN 탐색 |
| `Preferences`, `RecentCanvas` | 설정 이행·원자적 저장, 복구 경로 검증과 이전 컨테이너 해석 |
| `LicenseService` | 라이선스 검증 정책, 보안 저장소 복원·삭제·재시도 |
| `EditorBackend` | LVUI 스코프가 소유하는 Core Observable 값, UI 명령과 목록·색상 모델 |
| `Artwork` | 제품 도구와 실행 취소 정책, SharedCanvas 문서 명령 |
| `CanvasView` | 문서 좌표와 화면 좌표, 보이는 타일·입력·선택 표시 |
| `State` | 주 창·보조창, 메뉴·대화상자와 서비스 결과 연결 |

Core를 하위 기반으로 사용하며 Core에서 앱·GUI·문서를 참조하지 않는다. 문서의 파일·SQLite 소유자는 iiFileProvider이고 픽셀·레이어·변환·합성·파일 형식 소유자는 iiSharedCanvas Native이다. 이미지 코덱과 ICC 변환은 iiImageCodec에 둔다. 계정·라이선스·서명된 업데이트·보안 저장소는 각 Native SDK를 사용한다.

## 실행과 해제

GUI 실행은 Core Application의 이벤트 루프를 엔진에 차용한다. 차용한 엔진을 종료할 때에는 자기 TaskScope만 닫고 공유 루프는 유지한다. 엔진 단독 생성자는 자신의 루프를 소유하며 함께 닫는다. 주 창·환경설정·새 캔버스 보조창은 하나의 Core 루프를 공유한다. 보조창 표시 콜백은 비동기적으로 도착하므로 `show()` 직후 마운트가 완료되었다고 가정하지 않는다.

HTTP·TCP·UDP·예약 작업은 Core 수명과 네이티브 iiNetwork에 연결한다. 서비스 결과는 약한 소유 참조와 작업 취소를 거쳐 전달하며 이미 해제된 뷰를 갱신하지 않는다. 파일 선택은 운영체제의 비동기 대화상자를 사용한다. 웹 이미지 가져오기는 HTTP(S), 16 MiB·15초·리디렉션 3회로 제한하며 대상 문서가 바뀌면 결과를 적용하지 않는다.

LVUI는 HTML과 인라인 스타일을 빌드 시 C++23으로 생성한다. 백엔드를 구성한 뒤 위젯을 생성하여 같은 목록·색상 모델을 주입한다. `vm.method`는 지연 콜백 참조이고 `vm.method()`는 호출이다. Core Observable과 위젯 BindingScope가 변경 통지와 구독 해제를 담당한다.

## 문서와 편집

완성된 스트로크 하나는 문서 변경 하나이다. 미리보기 중 취소·포커스 상실·잘못된 브러시는 이전 픽셀과 캔버스 범위를 복원한다. 무한 캔버스의 음수 좌표 확장은 미리보기 문서에서 계산하고 완료 시에만 반영한다. 화면 이동·확대·레이어 선택·한 픽셀 조회·브러시 설정은 문서 리비전·dirty·저장을 바꾸지 않는다.

저장 파일은 이식 가능한 정규 `.iisc`이며 작업 복구용 SQLite는 앱의 관리 디렉터리에 별도로 둔다. 복구본을 열어도 내부 파일을 사용자의 저장 대상 경로로 설정하지 않는다. 이전 `.vrc`는 확인 가능한 개별 레이어·도형·텍스트를 이행하며 알 수 없는 개체·잘못된 식별자·누락 데이터는 원본 변경 없이 거부한다.

협업의 편집 페이로드는 정규 IISC와 기준 리비전이다. 호스트는 오래된 편집을 거부하고 클라이언트의 로컬 복구본을 유지한다. 로컬 협업 모의 서버 검사와 실운영 인터넷 협업의 검증은 구분한다.

빌드와 실행 증거는 [BUILD.md](BUILD.md), [NATIVE_VERIFICATION.md](NATIVE_VERIFICATION.md)에 있다. 세부 제품 계약은 [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md)에 있다. 이전 Qt 호스트 설명은 `build/native-migration-backup/APP_ENGINE_QT.md`에 보존한다.
