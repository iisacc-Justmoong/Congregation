<a id="contributing-to-congregation"></a>

# Congregation에 기여

Congregation는 재현 가능한 버그 보고서, Windows 하드웨어 테스트, 문서 개선 및 집중적인 코드 기여를 환영합니다.

<a id="start-with-a-public-discussion-or-issue"></a>

## 공개 토론이나 문제로 시작하세요.

- 일반적인 피드백, 디자인 아이디어 및 질문에 대해서는 [GitHub 토론](https://github.com/iisacc-Justmoong/Congregation/discussions)를 사용하십시오.
- 재현 가능한 결함이나 한계가 설정된 구현 작업에는 [GitHub 문제](https://github.com/iisacc-Justmoong/Congregation/issues)를 사용하세요.
- [Windows 10/11 테스트 문제](https://github.com/iisacc-Justmoong/Congregation/issues/18)에는 유용한 첫 번째 검증 작업이 나열되어 있습니다.

운영 체제 버전, 하드웨어 또는 입력 장치, 정확한 재현 단계, 예상 결과 및 실제 결과를 포함합니다. 스크린샷과 로그에서 개인정보를 제거하세요.

<a id="build-and-test"></a>

## 빌드 및 테스트

생성된 모든 빌드 파일을 저장소 로컬 `build/` 디렉터리에 보관합니다.

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

끌어오기 요청을 제출하기 전에 전체 테스트 모음을 실행하세요. 새로운 동작은 일반적으로 자동화된 회귀 테스트로 시작되거나 포함되어야 합니다. UI 프로토타입과 탐색 작업을 통해 먼저 구현을 확립할 수 있지만 검토 전에 적절한 테스트와 문서를 추가해야 합니다.

<a id="project-rules"></a>

## 프로젝트 규칙

- Qt 및 KDE 코딩 지침, 저장소 `.clang-format`, 4칸 들여쓰기 및 Allman 중괄호를 따르세요.
- QML 변경 사항은 `.local/SDK/LVRS/` 프레임워크를 사용해야 하며 파일당 하나의 루트 구성 요소를 유지해야 합니다.
- 모듈을 교체 가능하게 유지하고, 순환 종속성을 피하고, 하위 수준 모듈이 상위 수준 모듈에 종속되지 않도록 하세요.
- 비도메인 인프라를 로컬로 구현하기 전에 유지 관리되고 적절하게 라이센스가 부여된 외부 라이브러리를 평가하십시오.
- 소스가 변경될 때마다 관련 문서와 테스트를 업데이트하세요.
- 생성된 바이너리, 자격 증명, 개인 키, 인증서 파일 또는 서명 토큰을 커밋하지 마세요.

<a id="windows-release-safety"></a>

## Windows 릴리스 안전

서명되지 않은 MSI, ZIP, MSIX 및 자체 서명된 SignPath 시험 출력물은 개발 전용 아티팩트입니다. 서비스 승인 전에 생성된 모든 필수 워크플로우 입력은 개발 전용입니다. 그것들은 공개 릴리스에 첨부되거나 신뢰할 수 있는 다운로드로 제시되어서는 안 됩니다. 절대 `NECESSARY_SIGN_TOKEN` 를 커밋하거나 출력하지 마세요; 제공자가 Congregation 를 승인한 후에만 GitHub 액션 리포지토리 시크릿 스토어에 속해야 합니다. 웹사이트 MSI 는 외부 MSI 와 중첩된 `Congregation.exe` 가 모두 리포지토리의 신뢰할 수 있는 Authenticode, 예상 발행자, 동일한 인증서, 및 RFC 3161 타임스탬프 검증 게이트를 통과한 후에만 게시할 수 있습니다.
