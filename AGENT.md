# Congregation 작업 지침

현재 실행 소스는 `App/`이다. 과거의 `src/App` Qt 소스는 현재 작업 트리에서
삭제 상태이며 Git 기록에 남아 있다. C++23, iiSDKCore, LVUI와 iiGUI Native를
사용한다. 현재 타깃에 Qt·QML·LVRS 의존성을 추가하지 않는다.

LVUI 화면은 `HStack(...) { ... }`와 `VStack(...) { ... }`처럼 직접 선언한다.
HTML 태그, `<script>`, CSS형 인라인 스타일은 현재 `.lvui` 문법이 아니다.
애플리케이션 동작과 관측 가능한 상태는 C++ 뷰모델에서 import한다.

`EditorBackend`의 독립적인 목록·색상 표시 상태는 LVUI에서 사용할 수 있으며
Qt·QML·LVRS 타입을 요구하지 않는다. 네 선언 화면과 작업 영역은 독립 LVUI 경로를
사용하며 제품 모델과 연결하는 방법은 [뷰모델 계약](docs/LVUI_VIEWMODELS.md)을
따른다. 메인의 명령 막대·도구 레일·상태 표시줄은 LVUI에서 직접 구성한다.
도구 속성은 `ToolPanel.lvui`와 `ToolPanelBackend`로 구성한다. 타임라인,
나머지 상세 영역과 새 캔버스 선택기는 아직 C++ `NativeView` 위젯 구성에
남아 있으므로 전체 뷰의 선언형 전환 및 플랫폼 동등성은 별도로 확인한다.

출력은 저장소의 `build/`만 사용하고 Mac 앱은 `build/Congregation.app` 하나로 유지한다. 문서·테스트를 소스와 함께 갱신하고, 사용자 파일 및 다른 작업의 변경을 보존한다. 운영체제별 소스 구현, 빌드, 실행, 설치, 실운영 검증을 구분하여 기록한다.
2026-11-01 검사에서는 이 정책과 달리 이전 백업·작업 사본에 앱 번들 19개가 남아
있었다. 기존 작업을 보존하고 [단일 앱 빌드 기록](docs/SingleAppBuild.md)을 따른다.

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
