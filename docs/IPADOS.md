# iPadOS 빌드와 연결 기기 실행

Congregation의 iPad 소스는 `App/Engine`, `App/Native`와 LVUI AOT 화면을 공유한다. Qt·QML·LVRS·브라우저·WebView를 사용하지 않는 구성을 목표로 하며 iiGUI의 UIKit 장면과 자체 Metal 표면이 창·입력·표시를 담당한다. iPadOS 18.0 이상, arm64 기기를 대상으로 한다. 독립 LVUI 이전 이후의 iPad 빌드와 기기 실행은 아직 검증하지 않았으며 아래 기기 결과는 당시 빌드의 기록이다.

보존된 에셋 라이브러리 소스의 로컬 장치 표시는 iPad에서 `Society · This iPad`를 사용하지만 현재 Main 화면에는 사이드바를 구성하지 않는다. 앱 컨테이너의 저장·임시 파일은 iiFileProvider가 운영체제 컨테이너를 기준으로 고정하며 보호된 시스템 상위를 읽기용으로 순회하지 않는다. 이 경계와 링크 거부는 SDK의 임시 파일 검사 및 실제 기기의 저장·재열기에서 확인한다.

## 재현 절차

Xcode가 선택된 Mac에서 저장소 루트를 기준으로 실행한다. 기기는 개발자 모드와 Mac 신뢰가 설정되어 있어야 하며 개발 디스크 연결·설치·실행 시 잠금을 해제한다. 빌드 장치 식별자는 `xcrun devicectl list devices`로 조회한다.

```bash
python3 -B tools/build_ipados_sdk.py all
python3 -B tools/build_ipados.py --device <iPad-UDID> --install --launch
```

SDK 키트는 `SDK/build/congregation-ipados/prefix`, 모듈별 중간 출력은 각 SDK의 `build/ipados/build`에 둔다. macOS 키트와 라이브러리를 혼합하지 않는다. iiColorManager를 독립 LVUI보다 먼저 빌드하고 LVUI Color 어댑터, iiGraphicAdjust와 iiBrushTools를 키트에 포함한다. LVRS는 빌드하지 않으며 Qt·LVRS 패키지 탐색을 끈다. OpenSSL·CURL·이미지 코덱·Sodium·MoltenVK도 iPhoneOS 대상으로 준비한다. 외부 원본 URL·SHA-256은 키트의 `vendor/sources.json`에 기록한다. libtiff 4.7.1의 정적 CMake 패키지에서 빠진 `CMath::CMath`는 설치 키트의 소비자 어댑터로 보완하며 원본 소스는 수정하지 않는다.

iiNetwork는 기존 최상위 호스트 빌드를 보존하기 위해 별도 키트 드라이버의 하위 라이브러리로 소비한다. LVUI 생성기는 실행 호스트가 macOS이므로 기본적으로 `SDK/LVUI/build/lvui-compile`을 사용하고 `LVUI_HOST_COMPILER` 환경변수로 호환되는 다른 호스트 생성기를 지정할 수 있다. 생성된 C++와 실행 라이브러리는 iPad용으로 빌드한다. 필수 Color·그래픽·브러시 패키지나 호스트 생성기가 없으면 기존 macOS 번들을 이동하기 전에 중단한다.

제품의 활성 번들은 모든 플랫폼·구성에서 `build/Congregation.app` 하나이다. 플랫폼을 바꾸기 전에 기존 macOS 번들을 `build/ipados/Congregation-macOS-<hash>.zip`에 보존하고 실행 파일 SHA-256을 확인한다. 그 다음 같은 `build/`를 Xcode iPhoneOS 대상으로 다시 구성한다. 설치된 `/Applications` 앱이나 사용자의 문서를 교체하지 않는다. iPad의 설치는 기존 앱에 덮어 설치하며 앱을 삭제하지 않는다.

SVG 의존성은 iiGUI의 `resources/dependencies/lunasvg` 소스에서 가져온다.
Society가 없는 iPad의 로그인과 서버 라이선스 확인은 [웹 계정 계약](WEB_ACCOUNT.md)의
iPad 경로를 따른다.
네트워크와 제품이 사용하는 Core 값 기반 JSON 계약을 유지하기 위해 iPad 키트는
현재 Mac 키트와 같은 iiJson 0.1.1 리비전 `1a428a4135bf1f3d55173dd0cb075a4b9850dc3a`를
소스 저장소의 고정 리비전 스냅샷으로 준비한다. iiJson 최신 독립 값 API로의 전환은
별도 모듈 이행이며 이 빌드에서 소스 저장소의 변경 사항을 덮어쓰지 않는다.
파일 제공자의 Core I/O는 별도 소비자 프로젝트로 빌드하고 이미지 코덱은
`iiVisualCodec`의 iiImageCodec 호환 패키지를 사용한다.

Xcode 27의 SDK stat cache 준비가 FSEvents 대기 상태에 머무른 환경에서는 `SDK_STAT_CACHE_ENABLE=NO`를 CMake의 컴파일러 식별과 앱 빌드에 동일하게 적용한다. 컴파일러·SDK·아키텍처 검사를 생략하지 않는다. 이 설정은 빌드 도구 호출에만 적용하며 시스템 Xcode 설정을 변경하지 않는다.

개발 서명은 Xcode의 자동 프로비저닝과 `CONGREGATION_APPLE_TEAM`을 사용한다. 기본 팀은 `5U49ST9XZH`, 번들 ID는 `com.iisacc.congregation.painter`이다. 현재 소스의 버전은 1.0.2, 빌드는 3이다. 개발용 기기 설치 결과와 App Store 배포를 구분한다.

## 기기 진단

```bash
xcrun devicectl device process launch --device <iPad-UDID> --terminate-existing --console \
  com.iisacc.congregation.painter -- --device-self-test --renderer native
xcrun devicectl device process launch --device <iPad-UDID> --terminate-existing --console \
  com.iisacc.congregation.painter -- --device-self-test --renderer vulkan
```

`--device-self-test`는 앱 컨테이너의 `Library/Application Support/iisacc/Congregation/device-tests/congregation-<uuid>`를 새로 만들어 검사한다. 일반 라이선스·문서 저장소를 사용하지 않는다. 새 캔버스 취소·생성, 편집, 문서 저장·재열기, 내보내기, 보조 환경설정 창을 검사한 후 `success.json`, GPU 캡처와 `new-canvas.png`를 남긴다. UIKit 앱 수명 때문에 Core 종료 후에도 프로세스가 남을 수 있으므로 성공 파일과 콘솔의 `CONGREGATION_NATIVE_OK`를 확인한다. 그 다음 개발 도구로 해당 진단 프로세스를 종료하고 일반 실행으로 돌아간다.

자동 검사는 라이선스 활성화 전 화면도 표시하고 `unlicensed.png`를 캡처한다. 숨겨진 작업 패널의 테두리와 타임라인이 크기 0인 그리기 항목을 제출하지 않아야 하며, 이 표시 전환이 문서 리비전을 변경하지 않아야 한다. 실제 일반 실행의 초기 화면도 별도로 확인한다.

일반 실행의 `--device-evidence`는 라이선스 정책을 유지하면서 3초 후 첫 화면과 GPU·창 정보를 `device-diagnostics/startup.png`, `startup.json`에 기록한다. 일반 실행의 편집 권한은 기존 라이선스 정책을 따른다. 진단에서 합성한 펜 입력 검사는 물리적인 Apple Pencil 필기 검증과 구분한다.

UIKit 경로는 확정 문자 입력·삭제와 하드웨어 키보드를 지원한다. 전체 marked-text IME, 외부 드래그 앤드 드롭, 실제 Pencil 및 화면 회전·멀티태스킹의 수동 검증은 별도이다. 데스크톱 설치기를 iPad에서 실행하지 않으며 업데이트 플랫폼은 `ios-arm64`이다. Metal과 강제 Vulkan은 각각 검사한다. iPadOS의 Vulkan은 MoltenVK가 Metal 장치 위에서 실행한다.

최종 장치·서명·설치·실행·검사 결과와 캡처는 `build/ipados/`에 기록한다. 빌드 성공, 설치 성공, 프로세스 실행, 실제 화면 및 문서 검사 통과를 각각 확인한다.

## 2026-11-05 연결 기기 업데이트

- iPad Pro 12.9형 4세대, iPadOS 27.0.1에 버전 1.0.2 / 빌드 3을 개발 서명해
  기존 1.0.0 / 빌드 1 위에 설치했다. 앱 삭제는 수행하지 않았다.
- 일반 실행에서 저장된 웹 세션으로 서버의 Congregation 라이선스를 확인한 뒤
  편집기가 열리고 `Congregation license verified`가 표시된 것을 확인했다.
  기기 화면은 `build/ipados/startup-20261105.png`, 설치·실행 기록은 같은 디렉터리의
  `install-20261105.json`과 `launch-20261105.json`이다. 새 로그인·계정 전환과
  이후 서버 오류나 오프라인 재실행은 이번 기기 검사에 포함하지 않는다.
- 웹 라이선스의 보유·거부·다른 계정·다른 제품·만료·잘못된 응답과 저장 세션의
  재검증은 로컬 서버 회귀 검사에서 통과했다. iPad 빌더 11건, LVUI 스타일 11건,
  LVUI 경계 검사도 통과했다. 웹 테스트의 계정과 토큰은 실계정이 아닌 고정값이다.
- 기존 Mac 1.0.2 / 빌드 3 번들은 SHA-256을 확인한 후
  `build/ipados/Congregation-macOS-bb1130e1234b.zip`에 보존했다. 현재
  `build/Congregation.app`은 iPad용이며 Mac 앱을 설치하거나 교체하지 않았다.

## 2026-10-25 연결 기기 검증

- iPad Pro 12.9형 4세대, iPadOS 27.0.1, Apple A12Z GPU에서 iPhoneOS arm64 Release를 빌드·개발 서명·덮어 설치했다. 버전은 1.0.0 / 1이다.
- Metal과 강제 Vulkan 각각 실제 새 캔버스·취소·펜 경로·레이어·실행 취소·저장·재열기·PNG·PSD·환경설정 검사를 통과했다. 라이선스 활성화 전 화면도 표시하고 문서 불변식을 검사했다. 최종 콘솔에 UIKit appearance 불균형이나 iiGUI 콜백 실패가 없다.
- 각 렌더러의 최종 캡처는 2732×1998 픽셀이며 실제 장치의 하드웨어 가속과 11회의 표시를 기록했다. `build/ipados/device/{metal,vulkan}/success.json`과 PNG가 결과이다.
- 일반 실행은 라이선스 활성화 화면을 표시했다. `device/normal/startup.json`은 `mode=normal`, `visible=true`, Metal 하드웨어, 1366×999 논리 크기와 2732×1998 픽셀 크기를 기록했다. 최종 확인 시 PID는 9114이며 진단 편집 실행은 종료했다. 저장된 라이선스가 없어 `licensed=false`이다.
- Python 단위 검사는 27개 통과, macOS 번들 아이콘 검사 1개 제외이다. Qt 재유입·빌드 표면 계약, 터치 변환, 읽을 수 없는 상위 아래의 컨테이너 임대, 공유·독점·프로세스 간 임대, CURL 일반/별칭 타깃 구성을 함께 확인했다. Apple 공통 Metal·텍스트 소스와 시스템 신뢰 소스의 macOS 구문 검사도 통과했다. macOS 전체 GUI 검사를 이번 iPad 결과로 대신하지 않는다.
- 최종 앱 서명과 iPhoneOS 플랫폼·대상 기기 프로필·5개 폰트·Figma 리소스를 확인했다. 동적 의존성은 Apple 시스템 라이브러리만 포함하며 Qt·QML·WebKit·AppKit은 없다. 제품 `build/`의 `.app`는 정식 앱 하나이다. Congregation 이름의 기기 충돌 보고서는 조회 시 0개였다.

기계 판독 보고서는 `build/ipados/verification.json`, 빌드·설치·일반 실행 기록은 같은 디렉터리의 로그와 JSON이다. 초기 임시 경로·숨겨진 패널 문제는 수정했으며, 이전 실패 기록은 최종 통과 기록과 별도 파일로 보존했다. 전체 네트워크 프로토콜·외부 문서 제공자 저장·물리 Pencil·회전과 멀티태스킹을 모두 통과한 결과로 확대하지 않는다.


웹 계정 전환 후 Congregation의 네트워크 빌드 단계는 iiAccountManager Native와 iiUpdateManager Native를 구성한다. iiLicenseManager Native는 앱의 링크 및 생성 순서에서 제외한다. 업데이트 서명에 필요한 libsodium의 CMake 패키지는 crypto 단계가 직접 생성하므로 새 키트도 iiLicenseManager 설치에 의존하지 않는다. `tests/test_ipados_builder.py`가 별도 빈 키트에서 이 패키지 탐색을 검증한다. Society의 라이선스 관리 모듈은 Society에서 별도로 빌드한다.
