# Congregation 네이티브 전환

실행 진입점과 이벤트 루프는 iiSDKCore Application이 소유한다. LVUI의 HTML 태그와
인라인 스타일을 빌드 때 C++로 컴파일하며 LVRS 위젯을 iiGUI의 네이티브 창과 GPU에
표시한다. 브라우저, 웹뷰, Qt, QML 실행기와 JavaScript 런타임은 사용하지 않는다.
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
Windows와 Linux는 소스 구현 범위이며 실제 실행 검증은 현재 Mac에서 수행한다.

## Vincent 화면 배치

얇은 상단 도구 막대와 왼쪽 레이어 목록, 중앙 캔버스를 복원했다. 구성·팝업·환경설정 및 재현 검사 방법은 VINCENT_UI.md에 있다. 실행 결과와 원본 대비 시각 비교는 저장소 루트 design-qa.md와 build/vincent-ui-verification.json을 따른다.
