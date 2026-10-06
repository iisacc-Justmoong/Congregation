<a id="repository-guidelines"></a>

# 저장소 지침

<a id="project-structure--module-organization"></a>

## 프로젝트 구조 및 모듈 구성
Congregation 는 KDE 의 ECM 레이아웃을 따릅니다. 루트 `CMakeLists.txt` 는 ECM 모듈을 연결하고 `src/App/` 에 위임합니다. 모든 C++ 소스는 `src/App/` 에 있으며, 애플리케이션 진입점은 `src/App/main.cpp` 입니다. QML 자산은 `src/App/qml/` 하에 유지됩니다. 모든 CMake 빌드 출력을 리포지토리 로컬 `build/` 디렉토리에 보관하세요; 대안 빌드 트리는 지원되지 않습니다. 생성된 바이너리를 절대 커밋하지 마십시오.

<a id="build-test-and-development-commands"></a>

## 빌드, 테스트 및 개발 명령
ECM 와 KDE 설치 디렉토리를 `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` 로 구성하세요. 스크립트는 유용한 의존성 요약을 출력합니다. `cmake --build build` 로 빌드하여 `Congregation` 번들을 `build/` 에 생성하세요. macOS 바이너리를 `build/Congregation.app/Contents/MacOS/Congregation` 에서 실행하세요 (플랫폼에 따라 조정). `cmake --build build --target clean` 를 사용하여 아티팩트를 정리하거나 `build/` 디렉토리를 가지치기하세요.

<a id="coding-style--naming-conventions"></a>

## 코딩 스타일 및 명명 규칙
Qt 와 KDE 코딩 가이드라인을 따르세요: 4스페이스 들여쓰기, 올먼 대괄호, camelCase 이름, 그리고 리포지토리 `.clang-format` 와 일치합니다. 명시적 Qt / KF 타입 (예: `QString`, `KLocalizedString`) 과 현대적 신호-슬롯 패턴 ( `QObject::connect` 람다) 을 선호하세요. QML 임포트를 상단에 그룹화하고, 파일당 하나의 루트 컴포넌트를 유지하며, 검토 전에 `qmlformat` 로 포맷하세요.

<a id="testing-guidelines"></a>

## 테스트 지침
현재 자동화된 테스트가 없습니다; 새 작업을 Qt 테스트 또는 Catch2 하에 `tests/` 디렉토리로 계획하세요. 테스트 타겟을 `tests_<feature>` 로 명명하고 CMake 에 연결하여 `ctest --output-on-failure` 에서 `build/` 트리로 실행되도록 하세요. UI 시나리오를 추가할 때는 자동적으로 시각적 회귀를 포괄하기 어렵기 때문에 재현 가능한 단계나 녹화를 포함하세요.

<a id="commit--pull-request-guidelines"></a>

## 커밋 및 풀 요청 지침
행동 변경 시 간결한 명령형 커밋 주제를 작성하고 (`Add QML brush toolbar`) 본문에 컨텍스트를 포함하세요. 검토를 간소화하기 위해 관련 편집을 논리적 커밋으로 그룹화하세요. 풀 리퀘스트는 동기, 사용자-facing 변경 사항 요약, 그리고 이슈 트래커 ID 를 연결해야 합니다. UI 업데이트에는 스크린샷이나 스크린 캡처를 첨부하고 수행된 수동 테스트 범위를 언급하세요.

<a id="qt-configuration-tips"></a>

## Qt 구성 팁
`CMAKE_PREFIX_PATH`를 내보내 CMake가 Qt 6.8+ 및 KDE 프레임워크 6(Kirigami, KI18n)를 찾습니다. 새 종속성을 추가한 후 `cmake -S . -B build`를 다시 실행하여 ECM 기능 요약을 새로 고치고 `qt_add_qml_module`가 추가된 QML 파일을 추적하는지 확인하세요.
