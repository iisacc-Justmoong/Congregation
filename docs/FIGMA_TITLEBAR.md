# Congregation 애플리케이션 타이틀바

메인 창은 [macOS Figma 타이틀바](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=346-6481)를 LVUI 뷰로 사용한다. 오른쪽에는 Agent 입력란과 알림, 학습, 내보내기, 설정 및 프로필이 있다. 구성요소와 간격은 LVUI 컴포넌트 및 공통 토큰을 사용한다.

macOS는 기본 창 버튼을 유지한다. [Windows·Linux 시안](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=359-5256)은 타이틀바 왼쪽에 LVUI 닫기, 최소화, 최대화·복원 버튼을 배치한다. 버튼은 iiGUI 호스트의 창 수명주기와 창 관리자 동작에 연결된다. [태블릿 시안](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=359-5317)은 같은 막대를 창 버튼 없이 표시한다. 데스크톱의 빈 타이틀바 영역은 기본 마우스 누름으로 창을 이동한다.

내보내기와 설정, 프로필은 기존 동작에 연결된다. Agent 제출, 알림과 학습은 아직 사용할 수 없음을 알린다. 사용자 정의 프레임을 지원하지 않는 호스트는 시스템 타이틀바와 기존 도구막대의 내보내기·설정을 사용한다.

헤드리스 뷰 검사는 세 배치의 가시성, 위치와 버튼 동작 연결을 확인한다. Windows·Linux의 실제 창 관리자와 iPadOS에서의 표시 및 포인터 동작은 별도 기기 실행 검증이 필요하다.
