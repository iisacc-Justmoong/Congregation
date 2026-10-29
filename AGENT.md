# Congregation 작업 지침

현재 실행 소스는 `App/`이며 `src/App`은 빌드하지 않는 이행 참고 자료이다. C++23, iiSDKCore, LVUI, LVRS와 iiGUI Native를 사용한다. Qt 의존성을 현재 타깃에 추가하지 않는다.

출력은 저장소의 `build/`만 사용하고 Mac 앱은 `build/Congregation.app` 하나로 유지한다. 문서·테스트를 소스와 함께 갱신하고, 사용자 파일 및 다른 작업의 변경을 보존한다. 운영체제별 소스 구현, 빌드, 실행, 설치, 실운영 검증을 구분하여 기록한다.

실행과 검사는 `docs/NATIVE_RUNTIME.md`, `docs/BUILD.md`, `tests/`에 있다. README.md는 영어로, docs/ 문서는 한국어로 작성한다.

<!-- workspace-docs:agent:start -->
## 문서 구조와 에이전트 탐색

- 프로젝트 루트의 `README.md`는 사람용 영문 진입 문서이며 `AGENT.md`는 에이전트 진입점이다.
- 하위 인간용 Markdown과 구조화된 agent JSON을 함께 유지한다. Markdown이 원본이며 JSON은 원문 해시와 줄 범위를 가진 파생 문서이다.
- 먼저 [docs/agent-index.json](docs/agent-index.json)에서 관련 문서와 JSON 투영을 찾는다. 원문 SHA-256이 다르면 현재 Markdown을 읽고 투영을 갱신한다.
- JSON의 규칙 후보는 검색용 어휘 일치 결과이다. 과거 기록·예제·실패 상태를 현재 지시나 검증 완료로 자동 해석하지 않는다.
- 기존 프로젝트별 규칙, 동결된 증거, 라이선스, 외부 원본, 사용자 데이터와 다른 작업의 변경을 보존한다.
- 이 문서 재편 지침은 과거 작업의 기능 구현·배포를 자동 재개하라는 요청이 아니다. 현재 사용자의 작업 범위를 우선한다.
- 문서를 수정하면 JSON 쌍·색인을 함께 갱신하고 검증한다. 기존 소스 변경에 대한 빌드·테스트 규칙은 계속 따른다.

Workspace에서 이 프로젝트의 문서를 갱신하는 명령은 다음과 같다.

```sh
python3 ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. sync --project Product/Congregation
```
<!-- workspace-docs:agent:end -->
