# Changelog

## 1.0.0 — 2026-09-07

### Congregation 첫 소스 릴리스

- 활성 프로젝트 디렉터리와 소스, QML 모듈, 테스트, 문서, IDE 설정, 플랫폼별 패키징 및 파일명을 `Congregation`으로 통일했다. 소문자 식별자는 `congregation`, 대문자 상수와 환경 변수 접두사는 `CONGREGATION`을 사용한다.
- 변경 전 프로젝트 전체는 `Product/.Deprecated`에 복제하여 보관했다. 기존 빌드 결과와 배포 패키지, 이전 번들에 서명된 App Store 프로비저닝 프로파일은 보관본에 남기고 활성 프로젝트는 `build/`에서 다시 구성한다.
- 앱 버전은 `1.0.0`, macOS 빌드 번호는 `1`, QML 모듈은 `1.0`, Windows 파일·MSIX 버전은 `1.0.0.0`으로 초기화했다. 앱 번들 식별자는 `com.iisacc.congregation.painter`이다.
- 개발 저장소는 `iisacc-Justmoong/Congregation`, 첫 릴리스 태그는 `v1.0.0`이다. 기존 개발 이력은 보존하며, 새 이름의 서명된 설치 패키지·스토어 등록·프로덕션 라이선스 및 업데이트 서비스는 별도 출시 단계이다.
- Windows MSI의 UpgradeCode와 설치 컨텍스트 컴포넌트 GUID를 새 제품에 맞게 분리했다. 이전 제품의 설치를 다운그레이드하거나 교체하지 않고 독립된 Congregation 업그레이드 계열을 시작한다.
- 라이선스 테스트의 서명된 인증서 샘플도 `congregation` 제품 ID로 다시 발급했다. 기존 공개 테스트 키를 유지하여 계정 활성화와 재시작 후 계정 정보 복원 검증이 새 제품 ID를 사용한다.
- 이름이 바뀐 IDE 프로젝트 파일과 macOS entitlements·아이콘 자산이 전역 IDE 무시 규칙과 패키징 무시 규칙에 가려지지 않도록 Git 예외를 명시했다.

## 이전 프로젝트에서 이어받은 개발 이력

아래 버전과 날짜는 이름 변경 전에 축적된 개발 기록이며, Congregation의 현재 릴리스 버전은 위의 `1.0.0`이다.

## 2026-08-25

### Congregation 6.0 전역 버전 갱신

- 마케팅 버전을 `6.0`, macOS App Store 빌드 번호를 `60000`으로 올렸다. 중앙 CMake 정의에서 앱 런타임, QML 모듈, macOS 번들, CPack 및 플랫폼별 패키지 버전이 같은 릴리스로 파생된다.
- Windows 일반 배포·Microsoft Store·대응 소스 워크플로와 파일명, macOS 패키징 검증값, 업데이트 비교 계약, 공개 문서의 현재 릴리스 표기를 6.0으로 맞췄다.
- PowerShell 5.1 최소 실행 조건, MSI 4.0.0 업그레이드 기준, Partner Center 예약 표시 이름, Store 버전의 네 번째 `0`처럼 앱 릴리스와 의미가 다른 호환성 계약은 유지했다.

## 2026-08-24

### 프레젠테이션 레이저 꼬리 정리

- 레이저 점의 유일한 소유자를 현재 누르고 있는 마우스 커서 좌표에 결합된 `activeLaserPoint` 하나로 제한했다. 꼬리 Canvas는 연속 마우스 샘플을 모두 통과하는 Catmull–Rom 기반 cubic Bézier 곡선으로 코어·글로우 획을 렌더링하여 직선 구간이 이어진 꺾은선 효과를 제거한다.
- 곡선은 원형 샘플·round cap·round join을 만들지 않고 flat cap과 bevel join만 사용한다. 따라서 커서 밖의 궤적에는 별도 점이 생기지 않으며, 현재 커서 점은 보간이나 지연 없이 실제 포인터 좌표를 그대로 사용한다.
- 버튼을 놓을 때 마지막 포인터 좌표가 이미 샘플링된 경우 0길이 구간을 추가하지 않는다. 꼬리의 2초 지점별 페이드와 256개 상한은 유지하고, 새 포인터 제스처가 직전 꼬리 끝과 직선으로 연결되지 않도록 제스처 시작 경계를 기록한다.

### Members 추가 아이콘

- 설정창 Members 리스트 푸터의 Add member 아이콘을 파일 외곽선이 포함된 `addFile`에서 LVRS stock `add`로 교체했다. `add`는 `generaladd.svg`에 매핑되는 순수 `+` glyph이며 기존 초대 대상 메뉴와 활성화 조건은 그대로 유지한다.

### 호스트 캔버스 직접 편집

- 참가한 Congregation의 캔버스를 별도 문서 소유자가 아닌 호스트 캔버스의 입력 클라이언트로 변경했다. 참여자 포인터·태블릿 스트로크는 압력과 브러시 스타일만 캡처하며, 채우기·텍스트·도형·변형·캔버스·레이어·실행 취소/재실행·이미지 작업도 참여자 로컬 문서나 편집 기록을 변경하지 않는다.
- `LocalCanvasSession`은 참여자의 전체 `.vrc` 업로드를 거부하고, 키·형식·수치·개수·용량이 제한된 의미 단위 편집 명령만 호스트에 전달한다. 호스트가 실제 `DrawingSurface`에서 명령을 검증·실행한 뒤에만 revision을 증가시키고 SHA-256 검증 가능한 권위 `.vrc`를 모든 참여자에게 배포한다.
- 새 래스터 레이어의 QML 표면이 생성된 뒤 직렬화되도록 참여자 명령 결과 발행을 다음 이벤트 루프에 모은다. 참여자는 첫 권위 상태 복원이 성공할 때까지 명령을 보내지 않고 연결 해제·복원 실패 시 즉시 준비 상태를 지운다. 호스트 자신의 로컬 편집만 기존 350ms 디바운스를 사용하며, 참여자는 호스트 상태를 항상 복원하고 원격 세션을 자기 Recent canvas로 저장하지 않는다.
- TCP 루프백 테스트는 명령 전달만으로 호스트 revision이나 스냅샷이 바뀌지 않고 호스트 발행 뒤에만 참여자 상태가 갱신됨을 증명한다. 실제 QML 캔버스 테스트는 참여자 채우기와 스트로크가 로컬 문서를 바꾸지 않고 호스트 적용 경로만 변경함을 검증한다.

### 캔버스 초대 뱃지와 수락 흐름

- Profile의 `Allow inviting other users`를 켠 Congregation만 Members의 `+` 대상 메뉴에 익명 주소로 나타난다. 평상시 presence beacon에는 프로필 대신 초대 가능 여부 Boolean만 추가하며, 초대자가 특정 대상을 선택해야 현재 캔버스 공유가 시작되고 대상 지정 초대가 전송된다.
- 초대는 임시 초대 UUID, 송수신 세션 UUID, 캔버스 포트와 80자 이하 초대자 프로필 이름만 담는다. 계정·기기 이름·문서·프로필 이미지는 포함하지 않으며, 수신 측은 대상·발신 세션·LAN 주소를 검증하고 여러 인터페이스에서 중복 수신한 같은 초대 UUID를 제거한다.
- 초대를 받으면 툴바 Profile `LV.IconButton`에 LVRS danger 색상 뱃지가 나타난다. 버튼의 `LV.ContextMenu`는 초대자 이름과 stock `user` 아이콘, `Accept`·`Decline` 버튼을 표시하며 응답을 Boolean 인자로 전달한다. Accept는 해당 호스트 캔버스에 참가하고 Decline은 연결 없이 초대를 제거한다.
- 최대 16개의 서로 다른 발신자 초대를 순서대로 보관하고, 같은 발신자의 새 초대는 이전 대기 항목을 교체한다. 프로필의 초대 허용을 끄면 대기 초대도 함께 제거한다.

### 툴바 우측 LVRS 기본 버튼

- 툴바 우측의 프레젠테이션과 프로필 컨트롤을 원형 콘텐츠를 다시 그리는 `ToolbarCircleButton`에서 순정 `LV.IconButton`으로 교체했다. 두 버튼은 LVRS 기본 크기·패딩·아이콘 렌더링을 그대로 사용하고, 기존 보더리스 톤·접근성·툴팁과 프레젠테이션 진입 동작을 유지한다.
- 현재 브러시 색상을 직접 그려야 하는 컬러 피커만 원형 커스텀 콘텐츠를 유지한다. 프로필 버튼은 기존 계약대로 클릭 동작을 추가하지 않는다.

### 로컬 네트워크 캔버스 공유

- Preferences의 Members에서 현재 캔버스를 명시적으로 공유·중지하고, 같은 LAN에서 발견된 다른 Congregation 호스트 캔버스에 참가·이탈할 수 있게 했다. 호스트는 참가자 목록을 보고 원격 참가자를 제거할 수 있다.
- 기존 익명 UDP 멀티캐스트 presence에는 공유 중일 때만 임시 TCP 포트를 추가한다. 프로필 이름과 캔버스 데이터는 beacon에 포함하지 않고, 참가자가 실제 연결한 뒤에만 해당 TCP 세션으로 전송한다.
- `LocalCanvasSession`이 최대 16명의 참가자, 크기 제한 프레임, 8초 연결 제한, 프로토콜·세션·UUID 검증, 호스트 권위 단조 revision을 관리한다. 호스트가 발행하는 전체 `.vrc`에는 기본 래스터뿐 아니라 추가 래스터 레이어, 이미지, 텍스트, 도형과 배경 상태도 함께 포함된다.
- 참여자 편집 명령은 호스트 도착 순서대로 실제 호스트 캔버스에서 직렬 실행한다. 동시 편집의 구조적 병합은 수행하지 않으며, 모든 참여자는 각 명령 실행 뒤 호스트가 발행한 최신 권위 상태로 재동기화한다.
- iiSharedCanvas의 비동기 타일 렌더링 여부와 무관하게 저장·세션 래스터 자산 추출이 현재 문서를 동기적으로 렌더링하도록 보강하고, 실제 TCP host/client 왕복·참가자 갱신·호스트 제거·UDP 공유 endpoint·메모리 `.vrc` 왕복 회귀 테스트를 추가했다.

## 2026-08-23

### iiSharedCanvas 무한 캔버스

- 새 캔버스 모달 하단에 LVRS 체크박스 기반 `Infinite canvas` 옵션을 추가했다. 선택하면 입력한 폭·높이를 초기 할당 영역으로 사용하고 256픽셀 희소 청크를 갖는 iiSharedCanvas 1.1 문서를 실제 생성한다.
- `Infinite canvas`를 선택한 동안 폭·높이 입력란을 비활성화하며, 입력값이 유효하지 않아도 정규화된 기본 크기로 무한 캔버스를 생성할 수 있다.
- Pan·Zoom 카메라가 현재 영역 밖의 세계 좌표를 드러낼 때 보이는 영역을 청크 경계까지 확장한다. 좌·상단 확장 시 세션 오브젝트 좌표와 추가 래스터 레이어 원점을 함께 이동하고 비대칭 성장량으로 카메라 중심을 보정해 기존 콘텐츠의 화면 위치가 튀지 않는다.
- 전면 래스터 레이어의 실제 포인터 입력 영역을 확장된 `canvasSurface`에 직접 고정해, 세션 크기 역할값이 갱신되는 도중에도 흰 캔버스 전체에서 드로잉할 수 있게 했다.
- 음수 세계 좌표 브러시 청크, 추가 래스터 레이어 동기화, 실행 취소, `.iisc` 저장·재열기와 기존 1.0 유한 문서 호환을 테스트한다.

### 프레젠테이션 모드 진입 버튼

- 툴바 최우측 프로필 버튼 바로 왼쪽에 LVRS `screens` 아이콘 기반 `Presentation mode` 버튼을 추가했다. 버튼을 누르면 진입 전 창·카메라 상태를 보존하고 전체화면으로 전환한다.
- 프레젠테이션 중에는 메뉴·툴바·레이어 패널·편집 오버레이와 보조 알림을 숨기고 드로잉 입력과 카메라 조작을 잠근다. 캔버스는 화면 전체 픽셀 영역에서 종횡비를 유지하는 최대 배율로 정중앙에 고정한다.
- 프레젠테이션 화면을 좌클릭하면 현재 커서에만 붉은 레이저 점이 나타난다. 버튼을 누른 채 이동한 경로는 최대 256개 마우스 샘플을 통과하는 연속 곡선의 점 없는 획 잔상을 남기며, 각 경로 구간은 생성 시점부터 2초에 걸쳐 부드럽게 사라진다.
- 프레젠테이션 중 마우스 휠과 트랙패드 스크롤로 현재 fit 배율의 12.5%부터 800%까지 확대·축소할 수 있다. 캔버스 중심과 0 팬 오프셋은 유지하며 패닝과 무한 캔버스 확장은 계속 차단한다.
- 일반 편집 화면에서도 브러시, 지우개, 팬, 이동, 줌, 도형, 채우기, 텍스트 등 활성 도구와 관계없이 마우스 휠과 트랙패드 스크롤로 캔버스를 확대·축소할 수 있다.
- `Escape`를 누르면 프레젠테이션을 종료하고 진입 전 창 상태, 캔버스 확대율과 팬 위치를 복원한다.

## 2026-08-22

### Members 환경설정 리스트

- Figma `171:172` 프레임처럼 `170:156` SmallList 인스턴스를 237×231로 확장하고 설정 윈도우의 수평 중심선에 맞춘 LVRS 순정 `LV.List`를 Preferences의 Members 탭에 배치했다. 목록은 현재 캔버스가 제공하는 `collaboratorProfiles`를 받고, 각 프로필의 `profileName`과 선택적 `profileImageSource`를 행에 표시한다.
- 멤버 리스트의 고정 높이를 제거하고 헤더 아래 기존 20-DIP 간격과 설정 창 하단 24-DIP 패딩 사이를 채우도록 상·하단 앵커를 연결했다. 설정 창 높이가 바뀌면 리스트와 내부 뷰포트가 함께 늘어나거나 줄어들며, 237-DIP 폭과 푸터 배치는 유지된다.
- 현재 프로필을 멤버 목록에 항상 합성한다. 캔버스 호스트를 첫 행으로 올리고 이름 뒤에 `(host)`, 현재 사용자에게 `(me)`, 두 역할이 같으면 `(host, me)`를 표시한다. 아직 프로필 이름을 입력하지 않은 현재 사용자도 `Unnamed member`로 보이며, 호스트와 현재 사용자 행은 삭제할 수 없다.
- 푸터는 목록과 항상 함께 표시하며 첫 슬롯은 새 멤버 추가 요청, 두 번째 슬롯은 선택된 제거 가능 멤버의 삭제 요청을 현재 캔버스에 전달한다. 세 번째 22픽셀 버튼 슬롯은 Figma 배치를 유지하되 공백 glyph와 비활성 상태로 두어 아이콘이나 동작이 없다.
- 주변 Congregation 기기 탐색은 계속 익명 presence 전용으로 유지한다. Members 목록은 인근 기기 수를 프로필로 변환하지 않으며, 실제 캔버스 협업 세션 소유자가 프로필 모델과 추가·삭제 요청을 공급하는 별도 경계를 사용한다.

## 2026-08-20

### General 환경설정 실제 동작

- General의 고정 레이블을 `Account`로 바꾸고, `iiLicenseManager 0.2`가 서버 승인 활성화에서 확인·보존한 iisacc.com 계정 이메일을 표시한다. 환경설정을 열 때 먼저 서명된 오프라인 라이선스와 연결된 이메일을 읽고, 아직 이관되지 않은 기존 보안 저장소 자격 증명이 있을 때만 C++ 내부에서 iiLicenseManager 활성화를 수행한다. 라이선스 키는 QML에 노출되지 않으며 연결 계정이 없으면 `Not connected`로 표시한다.
- New canvas/Recent canvas 시작 방식과 주변 Congregation 사용자 탐색 여부를 비민감 `QSettings`에 영속화했다. 최근 캔버스는 외부 파일 경로가 아니라 앱 데이터 디렉터리의 단일 `recent-canvas.vrc` 내부 컨테이너에 1.2초 디바운스로 자동 저장한다. 컨테이너는 현재 보이는 캔버스 크기로 정규화한 iiSharedCanvas 문서와 추가 래스터 레이어·삽입 이미지 PNG, 텍스트·도형 편집 메타데이터를 함께 보존하고 SHA-256 무결성 검사를 거쳐 원자적으로 이전 스냅샷을 교체한다. Recent canvas 시작 시 이 내부 스냅샷을 복원하며, 포함된 PNG는 세션 수명에 묶인 소유자 전용 임시 디렉터리에서만 사용한다. 파일이 없거나 손상되었으면 이를 제거하고 빈 캔버스를 유지한다.
- 탐색 설정 변경을 C++의 `NearbyCongregationDiscovery` 생명주기에 연결했다. 기본값은 활성화이지만 사용자가 끄면 현재 worker를 중지하고 이후 실행에서도 시작하지 않는다.
- **Restore Purchases**를 활성화해 인증된 `https://iisacc.com/Account/Dashboard` 구매·제품 접근 복구 경로를 기본 브라우저로 연다. **Check for Updates…**는 기존 `iiUpdateManager 0.2` 수동 확인 모달을 그대로 사용하며 자동·시작 시 업데이트 요청은 추가하지 않는다.

## 2026-08-19

### macOS 단축키 수정

- Qt의 Apple 플랫폼 `QKeySequence`가 휴대형 `Ctrl` 토큰을 물리적 Command 키로, `Meta` 토큰을 물리적 Control 키로 변환하는 규칙에 맞춰 전역 단축키 정의를 교정했다. New/Open/Save/Undo/Paste/레이어/도형/창 명령은 macOS에서 Command를, Windows/Linux에서는 Control을 사용한다.
- Help의 키보드 단축키 목록도 macOS에서 `Command`, `Control`, `Option` 명칭으로 표시한다. 전체 화면 전환은 macOS 표준 `Control+Command+F`를 의도적인 예외로 유지한다.

### 환경설정 윈도우 기반

- `StandardKey.Preferences`가 제공하는 macOS `Command+,` 및 Windows/Linux `Ctrl+,` 표준 단축키로 별도 LVRS 환경설정 윈도우를 열 수 있게 했다. 메뉴 항목은 macOS에서 Congregation 애플리케이션 메뉴로 승격되고 Windows/Linux에서는 Edit 메뉴에 남는다. Preferences의 최초 표시 중심은 현재 Congregation 메인 윈도우 중심과 정확히 일치하며, 이후 사용자가 옮긴 위치는 강제로 되돌리지 않는다.
- 환경설정 윈도우 상단 중앙 헤더를 Figma와 같은 LVRS `LabelSegmentedControl`로 구성하고 General, Profile, Members 항목을 배치했다. 환경설정 윈도우를 열 때마다 General에서 시작하고 기존 설정 내용은 Profile을 선택할 때만 표시하며, Members의 내용 영역은 의도적으로 비워 두었다.
- General에는 고정 `Account` 레이블, New canvas/Recent canvas 라디오, 주변 Congregation 사용자 탐색 체크박스를 배치했다. 탐색 체크박스는 현재 프로세스의 익명 LAN 탐색을 즉시 시작·중지하고, 하단 좌우에는 구매 복원과 기존 수동 업데이트 확인 흐름을 각각 배치했다. 최근 캔버스 시작 선택은 아직 현재 윈도우 상태만 유지하며, 구매 복원은 실제 백엔드가 생길 때까지 보이는 비활성 상태를 유지한다.
- 원형 보더리스 프로필 이미지 버튼은 먼저 LVRS 컨텍스트 메뉴를 열고 `Select profile image`와 `Delete profile image`를 표시한다. Select를 선택한 다음 단계에서 로컬 이미지 선택기를 열며, Delete는 등록 이미지가 있을 때 미리보기와 임시 파일을 제거한다. 선택 사진의 방향 메타데이터를 적용한 뒤 짧은 변의 원본 픽셀 수를 유지하는 최대 중앙 정사각형을 원형 무손실 PNG로 절삭한다. 프로필 이름 입력 필드와 다른 사용자 초대 허용 체크박스를 함께 제공하며, 값은 현재 실행 중인 윈도우 인스턴스에만 유지하고 계정 저장이나 서버 연동은 아직 수행하지 않는다.

### 툴바 컬러 피커 및 프로필 배치

- 우측 끝에 있던 컬러 피커를 브러시 컨트롤 다음의 좌측 정렬 흐름으로 이동했다.
- 컬러 피커가 있던 우측 끝에는 같은 크기와 반경의 원형 프로필 버튼을 추가하고, 프로필 원형에는 보더를 표시하지 않는다.
- 프로필 버튼은 LVRS `user` 아이콘과 접근성 이름·툴팁만 제공하며 클릭 동작은 아직 연결하지 않는다.

## 5.1 App Store build metadata

- App Store build number is now `50100`, which preserves the `5.1` marketing version while advancing beyond the previously uploaded build `20201` required by App Store Connect.

## 2026-08-17

### Congregation 5.1 릴리스 패키징

- CMake 프로젝트와 런타임 표시의 릴리스 버전을 `5.1`로 올리고, macOS plist·PKG 및 Windows PE·MSI·MSIX 메타데이터가 같은 버전 원천을 따르도록 갱신했다.
- 두 자리 마케팅 버전은 macOS와 사용자 문서에서 `5.1`로 유지하고, Windows 형식 제약에 맞춰 PE/MSIX는 `5.1.0.0`, MSI는 `5.1.0`으로 패딩한다.
- iiUpdateManager의 안정 버전 비교 경계에는 표시 버전 `5.1`을 `5.1.0`으로만 정규화하여 수동 업데이트 확인이 두 자리 마케팅 버전에서도 동작하게 했다.
- macOS 일반 배포·App Store·로컬 무서명 패키지와 Windows 대응 소스·웹사이트 MSI·Store MSIX의 문서 및 계약 테스트를 5.1 산출물 이름으로 맞췄다.
- macOS 번들링 후 로컬 의존성에서 유입된 절대 `LC_RPATH`를 제거하고 이동 가능 경로 검증을 다시 수행하도록 패키징 계약을 강화했다.
- Windows 실행 파일 버전 리소스 회귀 검증값을 `5.1.0.0`으로 갱신했다.
- Windows 빌드·법적 고지·대응 소스 워크플로가 필수 `iiSharedCanvas` 바이너리, 라이선스와 고정 소스까지 함께 검증하도록 보완했다.
- MinGW 공유 라이브러리에서 결과 상태 검사기가 미수출 심볼로 남던 `iiSharedCanvas` 결함을 헤더 인라인 구현으로 고친 커밋에 Windows 빌드와 대응 소스를 고정했다.
- Windows runner가 `iiSharedCanvas` 테스트를 실행할 때 빌드 DLL과 iiPaintEngine·LVRS·Qt 런타임 디렉터리를 `PATH`에 명시해 DLL 로드 실패 코드 `0xc0000135`를 실제 테스트 실패와 구분하도록 했다.
- 비공개 `iiUpdateManager`는 로그나 Git 설정에 토큰을 남기지 않는 별도 checkout으로 고정 커밋을 가져오고, checkout 자격 증명을 즉시 폐기하도록 Windows CI 경계를 보강했다.

### 컬러피커 아이콘 가시성 개선

- 툴바 우측 컬러피커를 별도 원형 프레임과 이중 스와치 링으로 그리던 구현에서 stock `LV.IconButton`의 `Borderless` tone으로 교체했다.
- 아이콘은 현재 브러시 색상을 반영하는 단일 원과 정확한 2픽셀 흰색 보더만 표시한다. 외곽 버튼 프레임과 장식 링은 제거하면서 블랙 선택 상태의 식별성은 확보했다.
- 툴바 콘텐츠 레이아웃 좌우에 `LV.Theme.gap16` 패딩을 적용해 첫 파일 버튼과 마지막 컬러피커가 창 끝자락에서 각각 16픽셀 떨어지게 했다.
- 접근성 이름·툴팁·HSL 컬러피커 열기 동작을 유지하고, QML 계약 테스트가 stock LVRS geometry, 현재 색상 바인딩과 2픽셀 흰색 보더를 함께 고정한다.

### Photoshop식 임시 카메라 조작

- 캔버스 포커스에만 의존하던 Space 및 Control/Command 키 처리를 해당 카메라 조합만 소비하는 경량 애플리케이션 입력 필터로 보강했다. 전체 LVRS runtime-event 데몬은 활성화하지 않는다.
- Space를 누른 동안 선택 도구를 바꾸지 않고 열린 손 커서로 임시 패닝하며, 드래그 중에는 닫힌 손으로 바뀌고 Space를 놓으면 원래 도구와 커서로 즉시 돌아간다.
- Control 또는 Command와 Space를 함께 누르면 선택 도구를 바꾸지 않고 임시 Zoom으로 전환한다. 누른 채 오른쪽으로 드래그하면 확대하고 왼쪽으로 드래그하면 축소하며, 보조키만 놓으면 Pan으로 돌아가고 Space까지 놓으면 원래 선택 도구를 복원한다.
- 캔버스 텍스트, 레이어 이름, 툴바 다이얼로그가 입력을 받는 동안에는 임시 카메라 모드를 시작하지 않아 공백 입력을 보존한다.

### LVRS 순정 컴포넌트 치수 복원

- Congregation가 `LV.IconButton`, `LV.IconMenuButton`, `LV.ToggleSwitch`, `LV.ContextMenu`, `LV.Hierarchy`, `LV.AppCard`, `LV.ApplicationWindow`의 프레임·아이콘·패딩·메뉴·최소 치수를 덮지 않고 설치된 LVRS의 테마 스케일과 `implicitSize`를 그대로 사용하도록 정리했다.
- 수제 툴바 버튼과 shape 분할 버튼을 제거하고 stock `IconButton` 및 `IconMenuButton`으로 교체했다. 툴바 본문의 13개 `IconButton`과 1개 `IconMenuButton`은 모두 stock `LV.AbstractButton.Borderless` tone을 사용하며, 제품 코드는 아이콘, 접근성, 모델과 이벤트처럼 동작에 필요한 인자만 제공한다.
- 초기 창은 표시 전에 LVRS가 산출한 숨은 창의 종횡비를 측정하고 가로 1,280 논리 픽셀에 맞춰 세로를 자동 계산한다. 화면에 들어가지 않을 때만 같은 비율로 축소하며 표시 후에는 크기를 강제하지 않는다.
- 원시 픽셀 치수의 재도입을 막는 QML 계약 테스트를 추가하고 창 초기 크기 및 Windows 시작 문서를 LVRS 기본 geometry 기준으로 갱신했다.

### 라이선스 시행 임시 중단

- Congregation 앱 엔트리가 `LicenseManager::EnforcementMode::Disabled`를 명시적으로 선택하도록 하여 활성화 화면 없이 캔버스를 즉시 열고, 시작 시 보안 저장소 조회와 라이선스 검증 네트워크 요청을 수행하지 않게 했다.
- 온라인 검증, 보안 자격 증명 저장, 수동 업데이트용 자격 증명 제공 구현은 삭제하지 않았다. 추후 앱 정책 한 줄을 `Enabled`로 바꾸면 기존 계약과 테스트를 그대로 재활성화할 수 있다.
- 비활성 모드에서 활성화, 저장 라이선스 재시도, 라이선스 삭제 호출이 저장소나 네트워크를 변경하지 않는 회귀 테스트를 추가했다.

## 2026-08-13

### 보안 라이선스 자동 복원

- 이미 공개된 `v4.0.4` 태그와 그 대응 소스를 변경하지 않고, 라이선스 매니저와 보안 자격 증명 저장이 포함되는 다음 공개 배포본의 버전을 `4.0.5`로 올렸다.
- 성공적으로 온라인 검증된 계정 이메일과 Congregation 키만 JSON으로 직렬화하여 macOS Keychain 또는 Windows Credential Manager에 저장하도록 QtKeychain 0.17.0을 고정 커밋·정적 구성으로 도입했다. 평문 fallback, 번역, 데모 및 QtKeychain 자체 테스트는 포함하지 않는다.
- 다음 실행에는 보안 저장소를 읽어 같은 온라인 검증을 자동 수행한다. 명시적 invalid 또는 손상된 저장 JSON은 삭제하고, 오프라인·타임아웃·429·5xx는 저장값을 보존하여 키 재입력 없이 재시도할 수 있게 했다.
- 잠긴 활성화 화면에 저장 라이선스 재시도 및 **Use another license** 삭제 경로를 추가했다. 캔버스가 열린 뒤에는 미저장 작업 손실을 막기 위해 삭제 경로를 노출하지 않는다. Linux는 보안 저장소를 지원하지 않으므로 평문 대체 없이 매 실행 수동 활성화를 유지한다.
- Windows 패키지 legal tree와 대응 소스 도구, macOS 앱 번들에 QtKeychain BSD-3-Clause 고지와 고정 소스를 포함하도록 배포 계약을 갱신했다.
- 2026년 12월 31일까지 Windows x64 MSI를 명시적인 미서명 상태로 직접 판매하는 임시 정책을 적용했다. 공개 미서명 빌드도 전체 테스트·대응 소스·법적 고지·MSI 데이터베이스·SHA-256 검증을 통과해야 하며 2027년부터는 빌드가 자동 거부된다.

## 2026-07-16

### Windows SignPath 서명 결과 검증 수정

- SignPath가 반환한 MSI를 검증할 때 생성된 CMake 캐시에서 프로젝트 버전을 읽고, MSI 데이터베이스 계약 검사에 빌드 디렉터리·버전·MSI 경로를 모두 명시하도록 수정했다.
- 공개된 `v4.0.3` 태그를 변경하지 않고, 이 서명 결과 검증 수정이 포함되는 다음 공개 Windows 설치 프로그램의 버전을 `4.0.4`로 올렸다.
- Windows, macOS, Microsoft Store 패키징 계약과 사용자·빌드 문서의 배포 파일 이름을 `4.0.4`로 맞췄다.
- 태그와 고정 의존성 커밋에서 Git 아카이브를 만들고, 필요한 Qt 6.8.3 소스 모듈·파일별 SHA-256·구성요소 메타데이터를 함께 묶는 대응 소스 생성 도구를 추가했다. Git에 추적되는 `psd_sdk/build` 프로젝트 소스는 보존하고 저장소 메타데이터와 Qt 생성 디렉터리만 제외하며, 생성된 빌드 안내의 Markdown 경로가 PowerShell 제어문자로 변환되지 않도록 리터럴 템플릿을 사용한다.

## 2026-07-13

### Windows Store 최종 배포 경로

- Windows Store용 `.msix`, `.msixupload` 및 각 SHA-256 파일을 임시 빌드 산출물과 구분하여 저장소의 `dist/`에 직접 게시하도록 변경했다.
- Store 공개 패키지는 기존과 같이 깨끗한 Release 빌드와 전체 테스트, manifest·라이선스·corresponding-source 검증을 모두 통과해야 생성된다.
- Windows Authenticode 정책 테스트가 Windows PowerShell 5.1에서도 안정적으로 실행되도록 기본 유틸리티 모듈 로딩과 임시 라이선스 파일 생성을 명시했다.
- 비용 없는 자체 웹사이트 배포를 위해 SignPath Foundation OSS 후원 서명 경로를 추가했다. GitHub 호스팅 러너가 공개 소스에서 MSI를 재현하고, 외부 서명 전용 입력은 `build/signpath-input/`에 격리되며, SignPath가 반환한 공인 서명 MSI 한 개만 웹사이트 배포본이 된다.
- README에 SignPath가 요구하는 Code signing policy, 담당 역할, 개인정보 및 시스템 변경 정책을 문서화하고 GitHub CODEOWNERS와 Windows 서명 workflow 계약 테스트를 추가했다.
- 이미 공개된 `v4.0.1` 태그를 변경하지 않고 무료 서명 최종본의 애플리케이션·MSI·MSIX 버전을 `4.0.2`로 올렸다.
- GitHub 러너의 Qt 6.8.3 모듈 목록에 실제로 존재하는 추가 모듈만 요청하고, SignPath 입출력 MSI를 버전 와일드카드로 단일 탐색하여 릴리스 버전 증가가 workflow 경로를 깨뜨리지 않게 했다.
- 메모리가 제한된 GitHub 러너에서 LVRS의 대형 QML 리소스 번역 단위가 IPO/LTO로 장시간 정체되지 않도록 의존성 bootstrap의 IPO만 비활성화하고, LVRS와 iiPaintEngine 설치 단계를 분리했다.
- PowerShell이 LVRS Rust CLI의 CMake 인자 구분자 `--`를 소비하지 않도록 전체 호출 인자를 배열로 구성해 전달한다.
- LVRS 설치 스크립트의 예제·테스트 제외 옵션을 `--` 앞에 명시하여 자동 추가 옵션이 CMake 인자로 잘못 이동하지 않게 했다.
- 무료 GitHub 러너에서 대형 LVRS QML 리소스의 Release `-O3` 컴파일이 장시간 소요되어, 해당 의존성만 공식 배포 최적화 구성인 `MinSizeRel`로 빌드하도록 변경했다. 또한 LVRS의 플랫폼 최적화가 별도로 LTO를 다시 켜 MinGW 13 링크 단계에서 내부 컴파일러 오류를 일으키지 않도록 두 최적화 스위치를 모두 끈다. framework bootstrap이 두 스위치를 내부 구성에 전달하지 않거나 명시적인 `OFF` 값을 누락하던 LVRS 결함을 고친 커밋으로 의존성을 갱신했다. Congregation 본체는 계속 Release로 빌드한다.
- 단일 구성 Ninja에서 iiPaintEngine 설치 대상이 `NOCONFIG`만 내보내 Congregation Release 구성이 import library를 찾지 못하던 문제를 해결한 엔진 커밋을 고정했다.
- 비대화형 GitHub 러너의 Qt GUI 테스트는 offscreen 플랫폼으로 고정하고, Windows PowerShell 정책 테스트는 `$PSHOME`의 내장 Utility 모듈을 명시적으로 불러 PowerShell 7 모듈 경로 상속의 영향을 제거했다.
- 선택형 Qt 소스 아카이브가 최상위 `Src/LICENSES`를 만들지 않는 경우에는 동일한 5개 공통 라이선스 문서를 검증한 `qtbase/LICENSES`를 사용하고, 실제 배포되는 번역 모듈의 소스 라이선스도 CI에서 함께 받도록 수정했다.
- 이미 공개된 소스 전용 `v4.0.2` 태그를 변경하지 않고, 공개 GitHub 러너에서 서명 직전 MSI까지 검증된 최종 배포 계보를 `4.0.3`으로 올렸다.

## 2026-07-02

### 현재 작업트리 수정본

- Flat 이미지 열기 경로를 transformable 이미지 객체 생성에서 원본 이미지 크기의 배경 래스터 캔버스 생성으로 변경했다. 이제 PNG/JPEG/BMP/WebP/TIFF 같은 일반 이미지 파일은 작업 영역 크기에 맞춰 축소 배치되지 않으며, 캔버스 크기와 이미지 크기가 일치한다.
- 열기 성공 후에는 이미지가 `Background` 래스터로 들어가고, 그 위에 투명 `Layer 1`이 선택 상태로 생성되어 기존 레이어 기반 편집 흐름을 유지한다.
- `tst_drawingsurfaceitem`은 C++ `openRaster`가 큰 이미지도 원본 크기로 여는지와 QML 열기 경로가 이미지 객체 대신 이미지 크기 캔버스를 생성하고 같은 크기로 저장하는지를 검증하도록 갱신했다.
- Move tool의 자유 변형에서 이동과 코너 핸들 리사이즈가 더 이상 캔버스 경계에 clamp되지 않도록 바꿨다. 객체 bounds는 Photoshop 레이어처럼 캔버스 밖으로 확장될 수 있으며, 최소 크기 제한만 유지된다.
- Flat 이미지로 만든 문서에서 다시 이미지를 열면 캔버스를 재생성하지 않고 기존 캔버스 중앙에 원본 크기의 transformable 이미지 객체 레이어를 추가하도록 바꿨다.
- 모양 툴과 텍스트 툴이 생성하는 객체를 레이어 패널에서 각각 별개의 `layer` 행으로 투영하도록 바꿨다. 레이어 행은 `contentKind`로 shape/text/image/raster layer 구분을 유지한다.
- 선택된 도구와 무관하게 Space를 누르고 있는 동안 캔버스 입력이 임시 pan으로 동작하도록 했다. 실제 선택 도구는 유지되며, 텍스트 편집 중에는 Space 입력을 가로채지 않는다.
- 개체 선택 툴의 자유 변형 핸들을 8방향으로 확장하고 hit target을 키웠다. 모서리와 변 근처에서 resize cursor와 핸들 강조가 표시되어 작은 꼭짓점도 더 쉽게 잡을 수 있다.
- 개체 좌표와 자유 변형 bounds는 캔버스 밖으로 나갈 수 있게 유지하되, 실제 보이는 객체와 변형 UI는 캔버스 내부로 clip되도록 했다.

## 2026-07-01

분석 기준: `2026-07-01 00:00:00 +0900` 이후의 Git 커밋 3건과 현재 작업트리 수정본을 함께 검토했다.

### 커밋 반영분

#### `732ed9a` - Add menu bar func

- 브러시 압력 입력이 불투명도에 반영되는지 제어하는 `brushPressureControlsOpacity` 상태를 문서 모델, QML 툴바, 캔버스 표면, 브리지 계층에 연결했다.
- 브러시 설정 패널에 `Pressure Opacity` 토글을 추가해 압력 기반 불투명도 동작을 사용자가 켜고 끌 수 있게 했다.
- 수동 stroke 입력 경로에서 pressure-sensitive 입력이 마우스 이벤트로 축소되지 않도록 합성 `QTabletEvent` 경로를 추가했다.
- 테스트는 `tst_canvasdocumentviewmodel`, `tst_canvastoolbarqmlcontract`, `tst_drawingsurfaceitem`에 pressure opacity 상태, QML 계약, pressure 보존 시나리오를 추가했다.
- README와 앱 구조 문서는 브러시 압력/불투명도 및 관련 UI 계약을 반영했다.

#### `9388d2e` - Add func shortcut

- `Main.qml`이 메뉴 단축키의 단일 계약 소유자가 되도록 `shortcutNewCanvas`, `shortcutUndo`, `shortcutBrushTool`, `shortcutFitCanvasToWindow` 같은 named shortcut property를 추가했다.
- File, Edit, Tools, Shape Kind, Window 메뉴 항목에 실제 `shortcut` 바인딩을 연결하고, 메뉴만으로 실행되던 일부 명령에는 `Shortcut` 항목을 추가해 표시 단축키와 실행 동작을 맞췄다.
- Help 메뉴의 `Keyboard Shortcuts` 항목을 File/Edit/Tools/Shape Kind/Window 범주별 전체 단축키 참조로 확장했다.
- 테스트는 `tst_mainqmlcontract`에서 모든 shortcut property, 메뉴 바인딩, Help 참조, application shortcut 존재 여부를 계약으로 고정했다.
- README와 앱 구조 문서는 전역 메뉴 단축키 범위와 플랫폼별 Command/Ctrl 차이를 반영했다.

#### `d6c65f8` - Add func shortcut

- 플랫폼 앱 아이콘 소스를 `resources/Appicon.icns`와 `resources/Appicon.ico`로 정리하고, macOS 번들과 Windows 리소스가 각각 해당 파일을 사용하도록 CMake 패키징 계약을 갱신했다.
- macOS `Info.plist`의 `CFBundleIconFile`을 CMake 변수 기반으로 치환해 실제 번들 아이콘 파일명과 중복 계약이 생기지 않도록 했다.
- `.gitignore`에서 `resources/Appicon.icns`와 `resources/Appicon.ico`를 추적 허용 목록에 추가하고, 기존 `resources/icon.icns`를 제거했다.
- 테스트는 `tst_macosbuildworkflowcontract`에서 아이콘 파일 존재, CMake 번들 등록, Windows `.rc` 생성, `Info.plist` 치환, `.gitignore` 허용 규칙을 확인하도록 확장했다.
- README, `docs/BUILD.md`, `docs/app_structure.md`는 macOS/Windows 앱 아이콘 패키징 계약을 설명하도록 갱신했다.

### 현재 작업트리 수정본

- `Main.qml`의 메뉴 단축키 연결을 `Controls.MenuItem.shortcut`에서 `Controls.Action.shortcut`으로 옮겼다. 현재 Qt Quick Controls `MenuItem`에는 `shortcut` 속성이 없어 앱 시작 시 QML 로드가 실패했기 때문이다.
- 메뉴 항목은 `action` 속성으로 named action에 연결되며, 테스트와 앱 구조 문서는 이 계약을 기준으로 갱신했다.
- `build.sh` 기본 local 흐름이 `dist/Congregation.app` 생성에서 멈추지 않고 unsigned `dist/Congregation.pkg`와 `dist/Congregation-appstore.pkg`를 함께 재생성하도록 바꿨다.
- `dist/Congregation.pkg`는 수정 전 산출물이어서 현재 정상 `dist/Congregation.app` 기준으로 unsigned 로컬 설치용 패키지를 재생성했다. 이 패키지는 Developer ID Installer 서명이 없으므로 Gatekeeper 배포 신뢰 대상은 아니다.
- Transporter Active 목록에 이전 아이콘이 보이는 경우를 패키지 payload 문제와 분리했다. 현재 `dist/Congregation-appstore.pkg`는 `Appicon.icns`를 포함하고 legacy `icon.icns`는 포함하지 않으므로, 목록 썸네일은 App Store Connect 레코드/캐시 아이콘으로 별도 확인해야 한다.
- CMake binary directory를 저장소 루트의 `build/`로 강제했다. 이제 다른 build tree로 configure하면 CMake가 즉시 실패하며, 저장소 지침과 패키징 문서도 `build/` 단일 경로만 안내한다.
- CMake 프로젝트 버전, macOS `CFBundleShortVersionString`, `CFBundleVersion`, 앱 메뉴 표시, README와 패키징 문서를 모두 Congregation 4.0 기준으로 고정했다.
- Windows PowerShell용 `build-windows.ps1`을 추가했다. 이 스크립트는 Windows용 Qt/LVRS/iiPaintEngine prefix를 검증하고, `build/`에서 configure/build/test를 수행한 뒤 `windeployqt`, dependency DLL 및 LVRS QML import 복사, `dist/Congregation-Windows` staging, `dist/Congregation-4.0-Windows.zip` 생성을 처리한다.
- Windows configure에서 macOS 전용 `productbuild` 설정이 섞이지 않도록 CPack 설정을 플랫폼별로 분리하고, Windows install target은 `Congregation.exe` runtime을 패키지 루트에 설치하도록 정리했다.

### 검증 관점

- 오늘 커밋들은 기능 변경에 대응하는 테스트와 문서 갱신을 함께 포함한다.
- 현재 작업트리 수정본은 아직 커밋되지 않았으며, 설치 앱 시작 실패를 막기 위한 QML 메뉴 단축키 후속 변경과 `build/` 단일 CMake build tree 계약으로 분류된다.
- `tests_mainqmlcontract`, macOS 빌드 workflow 계약 테스트, 전체 `ctest`, `./build.sh local`, `/Applications/Congregation.app` 실행 스모크, 새 `dist/Congregation.pkg` payload 및 추출 앱 실행 스모크를 통과했다.
- `dist/Congregation-appstore.pkg` payload의 `Appicon.icns`는 canonical `resources/Appicon.icns`와 일치하며, `CFBundleIconFile`도 `Appicon.icns`로 해석된다.
- `dist/Congregation.pkg`는 unsigned 상태이므로 `spctl -a -vv -t install dist/Congregation.pkg`는 `no usable signature`로 거부된다. 신뢰 배포용 패키지는 Developer ID Installer 서명과 notarization이 필요하다.
- Windows 스크립트는 macOS 호스트에서 직접 실행 검증할 수 없으므로, `tests_windowsbuildworkflowcontract`가 PowerShell 실행 계약, CMake Windows install/ZIP CPack 설정, 문서화된 Windows 실행 명령을 텍스트 계약으로 고정한다.
