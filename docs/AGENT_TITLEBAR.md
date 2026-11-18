# 타이틀바 에이전트

타이틀바 Agent 입력란에서 Enter로 메시지를 제출한다. 응답과 진행 상태는 입력란 아래 LVUI 팝오버에 표시한다. 사용 중인 호스트는 Society의 작업 공간 에이전트이며, iiMultimodalAgent 세션과 모델 선택은 Society가 소유한다. 대화는 Society의 기존 작업 공간 대화와 공유된다.

화면 예제는 Congregation Figma Main의 [Agent · title bar and response example](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=456-20544)이다. 응답은 읽기 전용으로 스크롤할 수 있으며 표시 한도를 넘는 경우 전체 응답은 Society에서 확인한다.

요청은 UI 스레드 밖에서 Society의 공식 `society-cli`로 전달한다. 입력은 프로세스 인자가 아닌 표준 입력으로 보낸다. 응답은 제출한 메시지 ID 이후에 도착한 것만 표시한다. 동시 제출은 막고, 수락되지 않은 초안은 보존한다. 수락 여부가 불확실할 때 자동으로 재전송하지 않는다.

Society와 해당 CLI, 선택된 저장소 및 사용 가능한 에이전트 모델이 필요하다. 현재 Society의 이 로컬 호스트는 데스크톱 기능이다. iPad에서 독립 호스트가 없는 경우 사용할 수 없음을 알린다. Congregation은 Qt를 직접 링크하지 않고 모델이나 Society를 자동 설치하지 않는다. 외부 호스트의 도구 권한과 승인 정책을 변경하지 않는다.

테스트는 가짜 호스트의 수락, 점진 응답, 실패, 불확실한 전달과 대화 변경을 검증한다. 실제 모델 추론, Society 설치본의 교체, iPad에서의 동작 검증은 이 테스트에 포함되지 않는다.
