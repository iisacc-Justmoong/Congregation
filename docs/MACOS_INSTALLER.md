# Congregation macOS 설치 패키지

현재 네이티브 실행 소스와 `build/Congregation.app`을 사용한다. 이전 `dist/`의 Qt 앱은
패키징 입력으로 사용하지 않는다. 결과는 `build/Congregation.pkg`이며 설치 경로는
`/Applications/Congregation.app`이다. 제품 버전과 번들 빌드 번호는 변경하지 않는다.

## 2026-10-24 생성 결과

`build/Congregation.pkg`는 버전 1.0.0 / 빌드 1의 Apple Silicon(arm64) 앱을 포함한다.
설치 요구 조건은 macOS 12.0 이상이며 파일 크기는 19,434,089바이트이다.
Developer ID Application·Installer 서명과 신뢰 타임스탬프를 적용했다. Xcode의
로그인 계정으로 PKG 자체의 공증 티켓을 받아 부착했으며 `stapler validate`와
Gatekeeper install 평가에서 `accepted / Notarized Developer ID`를 확인했다.
압축 페이로드의 일반 파일 38개는 서명된 원본과 SHA-256이 일치한다.

최종 체크섬은 `build/Congregation.pkg.sha256`을 따른다. 생성·설치 정책·파일 해시·공증
결과는 `build/installer/verification.json`, 티켓과 Gatekeeper 로그는
`build/installer/xcode/`에 보존한다. 이 결과는 설치 파일 생성·검증의 증거이며 이 Mac의
`/Applications`에 앱을 설치한 기록은 아니다.

제품 검사 12개 항목을 확인했으며, 설치 계약 10개 세부 검사와 Metal·Vulkan의 최종
재배치 실행도 통과했다. 전체 실행의 11개 통과 기록에 보완한 계약·재배치 검사의 최종
통과 기록을 합친 결과이다. 실행 출처는 `docs/NATIVE_VERIFICATION.md`와
`build/installer/verification.json`에 구분하여 기록한다.

## 생성

먼저 `./build.sh local`로 현재 소스를 빌드·검사한다. 이후 다음을 실행한다.

```sh
cmake --build build --target CongregationInstaller
```

또는 빌드가 끝난 정식 앱에 다음 스크립트를 실행할 수 있다.

```sh
python3 -B packaging/macos/create_installer.py --xcode
```

앱은 Developer ID Application 인증서와 hardened runtime·신뢰 타임스탬프로 서명한다.
PKG는 Developer ID Installer 인증서와 신뢰 타임스탬프로 서명한다. 인증서 이름은
`--app-identity`와 `--installer-identity`로 선택할 수 있다. App Store용 샌드박스
entitlements나 Apple Distribution 인증서는 이 직접 배포 경로에 사용하지 않는다.

`--xcode`는 Xcode에 로그인된 개발자 계정과 `xcodebuild -exportArchive`의 Developer ID
업로드를 사용한다. 별도의 notarytool 비밀번호 프로필이 필요하지 않다. 최종 서명 PKG를
임시 제출 앱의 SharedSupport에 넣어 중첩 컨테이너로 제출하고, PKG 자체의 티켓을
받는다. 제출 앱은 배포하지 않는다. 원본 앱은 먼저 ZIP·CRC·파일 해시로 보존하며
성공·실패 모두 원래 서명 바이트로 복구한다. Xcode가 생성한 제출본도 압축 증거만
보관하므로 작업 후 `build/`에는 정식 `.app` 디렉터리 하나만 남는다.

공증 완료 전 업로드 성공을 배포 완료로 취급하지 않는다. 최종 PKG에 대한 티켓 부착,
티켓 검증 및 Gatekeeper의 install 평가가 모두 성공해야 한다. 이미 서명·검증된
`build/Congregation.pkg`만 공증할 때에는 다음을 실행한다.

```sh
python3 -B packaging/macos/xcode_notarization.py
```

키체인 공증 프로필이 있는 자동화 환경에서는 `--xcode` 대신 `--notary-profile <profile>`을 사용한다.
이 옵션은 완성된 PKG를 Apple에 제출하고 Accepted 결과·티켓 부착·Gatekeeper 설치
평가까지 확인한다. 비밀번호와 개인 키는 인수나 저장소에 기록하지 않는다.

서명과 공증은 별개이다. `build/installer/verification.json`의 `gatekeeper_ready`가
true일 때만 서명·공증이 모두 끝난 배포물로 취급한다. 공증 인증을 사용할 수 없는
경우에는 `--xcode`와 `--notary-profile`을 생략하여 서명된 PKG와 명시적인 미공증
상태만 생성할 수 있다. `CongregationInstaller` 타깃은 Xcode 공증 경로를 사용한다.

## 설치와 검증 계약

- 현재 앱을 복사하지 않고 `pkgbuild --component`로 직접 읽는다. 검사도 압축 Payload를
  메모리에서 읽어 별도 `.app` 디렉터리를 만들지 않는다.
- `/Applications`의 기존 번들을 교체하는 upgrade 동작을 사용한다. Qt 시대의 불필요한
  프레임워크가 기존 설치에 남지 않도록 병합형 update를 사용하지 않는다.
- 번들 재탐색을 끄고 같은 식별자·버전 조건을 검증한다. 현재 개발 앱이나 다른 위치의
  앱을 찾아 덮어쓰는 relocation을 허용하지 않는다.
- 실행 파일의 실제 아키텍처와 최소 macOS 버전을 Installer 요구 조건으로 넣는다.
- 번들 런타임의 Qt·외부 개발 경로 의존성을 거부한다. PKG 내부 모든 파일의 SHA-256,
  심볼릭 링크와 실행 권한을 서명된 원본과 대조한다.
- 빌드된 앱의 복사된 리소스에 남은 다운로드 quarantine만 서명 전에 제거한다. SDK
  원본과 사용자 문서는 변경하지 않는다. 압축 페이로드의 AppleDouble 메타데이터는
  대상 경로·형식·범위를 검사하고 quarantine 항목이 포함되면 거부한다.
- 검증된 임시 PKG만 `build/Congregation.pkg`로 교체한다. 이전 결과는 그때까지 보존한다.
- 체크섬은 `build/Congregation.pkg.sha256`, 상세 기록은
  `build/installer/verification.json`에 작성한다.

`Congregation.native_installer_contract`는 cpio 형식·정렬, 잘린 파일, 중복·탈출 경로,
오래된 실행 파일, 누락·추가 파일과 실행 권한, 설치 교체 정책, quarantine 차단과
Xcode 실패 시 원본 복구를 검사한다. 실제 생성 과정은 Apple
도구의 설치 메타데이터와 PKG 서명을 추가 확인한다. 패키지를 생성하는 작업은 이 Mac에
앱을 설치하거나 현재 실행 중인 앱을 재시작하지 않는다.

배포용 hardened runtime은 `DYLD_PRINT_LIBRARIES`를 무시할 수 있다. 재배치 실행 검사는
실행 중 프로세스의 `lsof` 파일 매핑에서 실제 Mach-O를 읽어 앱과 포함된 모든 dylib를
확인한다. 외부 SDK 경로가 없고 필수 번들 이미지가 모두 관측되어야 통과한다. 별도
디버깅 권한이나 런타임 보안 완화는 필요하지 않다.
OS가 매핑한 삭제된 임시 데이터와 보호된 `/private/var/db/`, `/private/var/folders/` 상태·캐시 파일은 코드로
분류하지 않는다. 필수 앱 이미지 누락과 읽을 수 없는 외부 라이브러리는 실패로 유지한다.

공식 참고 자료는 [Apple의 공증 워크플로](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow)와
[Xcode Developer ID 업로드](https://help.apple.com/xcode/mac/current/en.lproj/dev88332a81e.html)이다.
Apple은 중첩 컨테이너에도 별도의 티켓을 생성한다. 이 구현은 최종 PKG에 티켓을
실제로 부착하고 검증한 결과를 완료 기준으로 삼는다.
