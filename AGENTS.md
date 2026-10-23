# Congregation 작업 지침

현재 실행 소스는 `App/`이며 `src/App`은 빌드하지 않는 이행 참고 자료이다. C++23, iiSDKCore, LVUI, LVRS와 iiGUI Native를 사용한다. Qt 의존성을 현재 타깃에 추가하지 않는다.

출력은 저장소의 `build/`만 사용하고 Mac 앱은 `build/Congregation.app` 하나로 유지한다. 문서·테스트를 소스와 함께 갱신하고, 사용자 파일 및 다른 작업의 변경을 보존한다. 운영체제별 소스 구현, 빌드, 실행, 설치, 실운영 검증을 구분하여 기록한다.

실행과 검사는 `docs/NATIVE_RUNTIME.md`, `docs/BUILD.md`, `tests/`에 있다. README.md는 영어로, docs/ 문서는 한국어로 작성한다.
