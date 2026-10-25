# Congregation 앱 아이콘

1024×1024 RGB 아이콘의 편집 원본은 `resources/AppIcon/Congregation.svg`와 Adobe Illustrator의 `Congregation.ai`이다. 사용자가 제공한 샘플의 기하 구조·금속 면·적색/주황 연결선을 벡터로 재구성했다. 샘플에 있던 외곽의 둥근 사각형 보더는 제거했다. 바탕은 불투명 정사각형이며 사전 적용된 모서리 마스크·라디우스가 없다.

AI 문서에는 샘플 이미지를 내장한 숨김·비인쇄 참고 레이어가 있다. 표시되는 아이콘 도형은 벡터이다. SVG에는 비트맵·외부 참조·폰트·래스터 효과가 없다. 원래의 PSD·PXD·Affinity 원본은 보존하며 새 리소스의 생성 입력으로 사용하지 않는다.

## 수정과 생성

1. SVG의 벡터 도형·그라디언트를 수정한 뒤 저장한다. Illustrator에서 수정한 경우에도 수정 내용을 SVG 원본에 반영한다.
2. Illustrator의 `File → Scripts → Other Script…`에서 `resources/AppIcon/Export.jsx`를 실행한다. 기존 `Congregation.ai`가 열려 있으면 먼저 저장하고 닫는다. 스크립트는 1024px 아트보드와 벡터 여부를 검사하고, AI와 불투명 PNG24를 저장한다. 최초 샘플이 `build/app-icon/Reference.png`에 있으면 숨김 참고 레이어로 내장한다.
3. macOS에서 아래 명령으로 플랫폼별 아이콘을 갱신한다. 생성에는 macOS의 `sips`와 `iconutil`, Python 표준 라이브러리만 사용한다. 중간 파일은 모두 `build/app-icon/`에 둔다.

```sh
python3 -B cmake/generate_app_icons.py
python3 -B cmake/generate_app_icons.py --check
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R 'Congregation.app_icon'
```

구성된 macOS 빌드에서는 `CongregationAppIcons` 타깃도 같은 생성 작업을 수행한다. 일반 앱 빌드는 `CongregationAppIconCheck`를 실행하여 편집 원본과 파생 리소스의 SHA-256 일치를 검사한다. SVG·AI·PNG 또는 카탈로그를 수정하고 재생성을 빠뜨리면 빌드 전에 오래된 리소스를 거부한다.

macOS에서는 ICNS도 앱의 링크 의존성으로 등록한다. 아이콘만 바뀌어도 번들 패키징과 서명을 다시 수행하여 리소스 교체 뒤에 이전 서명이 남지 않게 한다.

## 플랫폼 연결

| 대상 | 리소스와 연결 |
| --- | --- |
| 공통 PNG | `resources/AppIcon/Congregation.png`, 1024×1024 불투명 RGB8 |
| macOS 앱 | `resources/Appicon.icns`를 번들의 `Contents/Resources/Appicon.icns`에 복사하고 `CFBundleIconFile`로 지정 |
| macOS 자산 카탈로그 | `packaging/macos/Congregation.xcassets/AppIcon.appiconset`의 1×/2× 10개 슬롯과 1024px 마스터 |
| Windows 실행 파일 | `cmake/AppIcon.cmake`가 ICO를 참조하는 RC를 구성하고 현재 앱 타깃에 포함 |
| Windows ICO | `resources/Appicon.ico`, 16/24/32/48/64/128/256px의 불투명 RGBA PNG 7개 |
| Linux 빌드 리소스 | `resources/AppIcon/Congregation.png`를 실행 파일 옆 `resources/`로 복사. 시스템 아이콘 테마에 설치하는 작업은 별도 패키징 단계 |

`resources/AppIcon/manifest.json`은 편집 입력과 파생 파일의 해시를 기록한다. `tests/test_app_icon.py`는 PNG 형식·크기·불투명성, SVG의 벡터 및 정사각 배경, 실제 Illustrator 파일, 자산 카탈로그의 모든 슬롯, ICNS의 1024px 마스터, ICO의 모든 이미지, 해시 일치와 macOS 빌드 번들의 실제 아이콘을 검사한다.

## 검증 범위

2026-10-25에 Adobe Illustrator 2026에서 AI 저장과 PNG24 내보내기를 수행했다. 네이티브 내보내기 기록은 `build/app-icon/illustrator-export.json`이다. macOS의 빌드·서명·리소스 검증은 해당 작업의 `build/app-icon/` 로그를 기준으로 한다. Windows 실행 파일 빌드와 실제 Windows/Linux 표시 결과는 macOS의 검사로 대체하지 않는다. 설치된 앱과 배포 패키지를 갱신했다는 의미도 아니다.

이번 확인에서는 Illustrator의 벡터 패스 111개와 그라디언트 채움 86개, 표시되는 래스터 0개를 확인했다. 아이콘 계약의 세부 검사 8개와 제품 CTest 17개가 모두 통과했으며, 전체 제품 검사 시간은 278.34초이다. Metal·Vulkan 화면과 정식 앱의 재배치·복귀를 포함한다. Windows ICO의 7개 이미지도 해당 크기의 PNG와 픽셀이 같고 알파가 모두 255임을 독립적으로 비교했다.

Finder의 정식 앱 정보 창에서 새 아이콘과 버전 1.0.0 표시를 확인했다. 최종 번들의 ICNS가 소스와 일치하며 로컬 번들 서명이 유효하다. 제품 버전 1.0.0과 번들 빌드 번호 1을 유지한다. 최종 요약은 `build/app-icon/verification.json`, 전체 제품 검사 로그는 `build/app-icon/tests.log`, 이전 아이콘 리소스의 보존본은 `build/app-icon/previous-icons.zip`이다.
