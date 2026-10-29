# 문서 경로 계약

사람용 영문 진입점은 루트 `README.md`이고 에이전트 진입점은 `AGENT.md`이다. 기여 지침의 원본은 [CONTRIBUTING.md](CONTRIBUTING.md)이며 루트의 기존 경로는 이동 안내로 유지한다.

보존된 `tests/tst_windowsbuildworkflowcontract.cpp`는 기여 지침을 `../docs/CONTRIBUTING.md`에서 읽는다. WorkspaceDocumentation의 C++23 문서 소비 검사는 이 소스가 선택한 실제 파일과 원래 필수 문구를 대조한다.

현재 애플리케이션은 `App/`의 네이티브 타깃이며 과거 Qt 테스트는 현재 빌드 대상으로 승격하지 않는다. 문서 데이터 계약 검사와 현재 네이티브 빌드·테스트 및 Windows CI 실제 실행을 각각 구분한다.
