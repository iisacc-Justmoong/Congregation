# Windows 검증

저장소가 지정한 iiSharedCanvas 0.11.0 의존성을 별도 설치 경로에 빌드하여 사용한다. 최신 SDK와 ABI를 혼합하지 않는다. Qt MinGW 키트와 Ninja로 `build/`에 빌드한다. GUI 검증은 Windows 플랫폼 플러그인에서 실제 창 생성과 정상 종료를 확인한다.

문서 계약 테스트는 번역된 제목 대신 보존된 Markdown section ID와 실행 명령·API 이름을 검사한다. 영어 원문에 종속된 검사 때문에 한국어 설명서가 오탐되지 않도록 한다.

Windows 바이너리 버전 검사는 실제 CMake 프로젝트 버전을 따른다. GUI 프로세스는 native Windows 플러그인을 명시하여 실제 창의 크기와 유지 상태를 확인한다.
