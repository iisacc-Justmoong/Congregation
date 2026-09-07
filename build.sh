#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

on_err() {
  local rc=$?
  printf 'ERROR: line %s: command failed: %s\n' "$LINENO" "$BASH_COMMAND" >&2
  exit "$rc"
}
trap on_err ERR

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
say() { printf '%s\n' "$*" >&2; }
run() { printf '+ %q ' "$@" >&2; printf '\n' >&2; "$@"; }

usage() {
  cat >&2 <<'USAGE'
Usage: ./build.sh [--clean] [local|devid|mas|all]

Modes:
  local   Build, test, deploy Qt runtime, sign dist/Congregation.app for local use,
          and create unsigned local installer packages.
          Uses LOCAL_APP_CERT, the first valid Apple Development identity, or ad-hoc signing.
  devid   Build, test, create the Developer ID pkg, notarize it, and staple it. (default)
  mas     Build, test, create the Mac App Store pkg.
  all     Build, test, create both Developer ID and Mac App Store pkgs.

Environment:
  CONGREGATION_BUILD_MODE may be used instead of the positional mode.
  CLEAN_BUILD_DIR=1 or --clean discards build/ before configuring.
  RUN_TESTS=0 skips ctest.
  IIUPDATEMANAGER_PREFIX selects the installed iiUpdateManager 0.2 prefix.
  IILICENSEMANAGER_PREFIX selects the installed iiLicenseManager 0.2 prefix.
USAGE
}

normalize_build_mode() {
  case "$1" in
    local|devid|mas|all) printf '%s\n' "$1" ;;
    package) printf '%s\n' "all" ;;
    *) die "unknown build mode: $1 (expected local, devid, mas, or all)" ;;
  esac
}

mode_wants_local() { [[ "$BUILD_MODE" == "local" ]]; }
mode_wants_devid() { [[ "$BUILD_MODE" == "devid" || "$BUILD_MODE" == "all" ]]; }
mode_wants_mas() { [[ "$BUILD_MODE" == "mas" || "$BUILD_MODE" == "all" ]]; }
mode_wants_package() { mode_wants_local || mode_wants_devid || mode_wants_mas; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "required command not found in PATH: $1"; }
need_xcrun_tool() { xcrun --find "$1" >/dev/null 2>&1 || die "required Xcode tool not found (xcrun --find failed): $1"; }

require_file() { [[ -f "$1" ]] || die "file not found: $1"; }
require_dir() { [[ -d "$1" ]] || die "directory not found: $1"; }
require_nonempty_file() { [[ -s "$1" ]] || die "file is empty or missing: $1"; }

# codesigning / installer identity가 "유효(valid)" 목록에 존재하는지 확인한다.
security_supports_policy() {
   local p="$1"
   security find-identity -h 2>&1 | grep -qE "Supported policies:.*(^|[[:space:],])${p}([[:space:],]|$)"
 }

 identity_valid() {
   local policy="$1"     # codesigning | installer | ...
   local needle="$2"     # Common Name or 40-hex SHA1

   # 이 OS에서 installer 정책이 없으면 basic으로 대체
   if [[ "$policy" == "installer" ]] && ! security_supports_policy installer; then
     policy="basic"
   fi

   local out
   out="$(security find-identity -v -p "$policy" 2>&1 || true)"

   # 만약 여전히 Usage가 나오면(정책 미지원 등), basic으로 한 번 더 시도
   if grep -q "^Usage: find-identity" <<<"$out"; then
     out="$(security find-identity -v -p basic 2>&1 || true)"
   fi

   if [[ "$needle" =~ ^[0-9A-Fa-f]{40}$ ]]; then
     grep -qiE "^[[:space:]]*[0-9]+\)[[:space:]]*${needle}[[:space:]]" <<<"$out" && return 0
   else
     grep -F "\"$needle\"" <<<"$out" >/dev/null 2>&1 && return 0
     grep -F "$needle" <<<"$out" >/dev/null 2>&1 && return 0
   fi

   printf 'ERROR: valid %s identity not found: %s\n' "$policy" "$needle" >&2
   printf -- '--- security find-identity -v -p %s (first 60 lines) ---\n' "$policy" >&2
   printf '%s\n' "$out" | sed -n '1,60p' >&2
   return 1
   }

first_valid_codesigning_identity() {
  local prefix="$1"
  local out
  out="$(security find-identity -v -p codesigning 2>&1 || true)"
  awk -v prefix="$prefix" '
    index($0, "\"" prefix) {
      sub(/^[^"]*"/, "", $0)
      sub(/"[[:space:]]*$/, "", $0)
      print
      exit
    }
  ' <<<"$out"
}

resolve_local_codesign_identity() {
  if [[ -n "$LOCAL_APP_CERT" ]]; then
    identity_valid codesigning "$LOCAL_APP_CERT" || die "valid local codesigning identity not found: $LOCAL_APP_CERT"
    printf '%s\n' "$LOCAL_APP_CERT"
    return 0
  fi

  local found
  found="$(first_valid_codesigning_identity "Apple Development:")"
  if [[ -n "$found" ]]; then
    printf '%s\n' "$found"
    return 0
  fi

  printf '%s\n' "-"
}

# plist 문법 검증
lint_plist() {
  local p="$1"
  require_file "$p"
  /usr/bin/plutil -lint "$p" >/dev/null
}

# Info.plist 키 추출(실패 시 빈 문자열)
plist_get() {
  local plist="$1"
  local key="$2"
  /usr/libexec/PlistBuddy -c "Print :$key" "$plist" 2>/dev/null || true
}

resolve_app_icon_path() {
  local app="$1"
  local plist="${app}/Contents/Info.plist"
  require_file "$plist"

  local icon_file
  icon_file="$(plist_get "$plist" CFBundleIconFile)"
  [[ -n "$icon_file" ]] || die "CFBundleIconFile is missing in: $plist"
  [[ "$icon_file" != */* ]] || die "CFBundleIconFile must be a resource filename, got: $icon_file"

  local icon_path="${app}/Contents/Resources/${icon_file}"
  if [[ ! -f "$icon_path" && "$icon_file" != *.icns ]]; then
    icon_path="${app}/Contents/Resources/${icon_file}.icns"
  fi

  require_nonempty_file "$icon_path"
  printf '%s\n' "$icon_path"
}

assert_app_icon_bundle() {
  local app="$1"
  require_dir "$app"

  local icon_path
  icon_path="$(resolve_app_icon_path "$app")"
  local icon_file
  icon_file="$(basename "$icon_path")"
  [[ "$icon_file" == "$APP_ICON_FILE" ]] || die "unexpected app icon file in $(basename "$app"): $icon_file (expected $APP_ICON_FILE)"

  local legacy_icon="${app}/Contents/Resources/${LEGACY_APP_ICON_FILE}"
  if [[ "$LEGACY_APP_ICON_FILE" != "$icon_file" && -e "$legacy_icon" ]]; then
    die "legacy app icon remains in bundle: $legacy_icon"
  fi

  local source_icon="${CMAKE_SOURCE_DIR%/}/resources/${APP_ICON_FILE}"
  if [[ -f "$source_icon" ]] && ! cmp -s "$source_icon" "$icon_path"; then
    die "bundled app icon differs from source icon: $icon_path"
  fi
}

verify_pkg_icon_payload() {
  local pkg="$1"
  local app="$2"
  require_file "$pkg"
  require_dir "$app"

  local icon_path
  icon_path="$(resolve_app_icon_path "$app")"
  local icon_file
  icon_file="$(basename "$icon_path")"

  local payload_list="${WORKDIR}/payload_files_$(basename "$pkg").txt"
  pkgutil --payload-files "$pkg" > "$payload_list" || die "failed to list package payload: $pkg"

  local app_bundle
  app_bundle="$(basename "$app")"
  local expected_icon="./${app_bundle}/Contents/Resources/${icon_file}"
  grep -Fx "$expected_icon" "$payload_list" >/dev/null || die "package payload is missing app icon: $expected_icon"

  local legacy_icon="./${app_bundle}/Contents/Resources/${LEGACY_APP_ICON_FILE}"
  if [[ "$legacy_icon" != "$expected_icon" ]] && grep -Fx "$legacy_icon" "$payload_list" >/dev/null; then
    die "package payload still contains legacy app icon: $legacy_icon"
  fi

  grep -E "^\\./${app_bundle}/Contents/Frameworks/libiiUpdateManager([.][^/]*)?[.]dylib$" \
    "$payload_list" >/dev/null \
    || die "package payload is missing the iiUpdateManager runtime"
  grep -E "^\\./${app_bundle}/Contents/Frameworks/libiiLicenseManager([.][^/]*)?[.]dylib$" \
    "$payload_list" >/dev/null \
    || die "package payload is missing the iiLicenseManager runtime"
}

to_lower() {
  # Bash 3 호환을 위해 ${var,,} 대신 tr을 사용한다.
  printf '%s' "$1" | tr '[:upper:]' '[:lower:]'
}

# =============================================================================
# 템플릿 설정: 프로젝트별로 이 구역만 수정하는 것을 전제로 한다.
# =============================================================================

BUILD_MODE="${CONGREGATION_BUILD_MODE:-devid}"
CLEAN_BUILD_DIR="${CLEAN_BUILD_DIR:-0}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --clean)
      CLEAN_BUILD_DIR="1"
      ;;
    local|devid|mas|all|package)
      BUILD_MODE="$1"
      ;;
    *) die "unexpected argument: $1" ;;
  esac
  shift
done
BUILD_MODE="$(normalize_build_mode "$BUILD_MODE")"
RUN_TESTS="${RUN_TESTS:-1}"

# 앱 이름(.app 제외)
APP_NAME="Congregation"
APP_ICON_FILE="Appicon.icns"
LEGACY_APP_ICON_FILE="icon.icns"

# CMake/Ninja 빌드 설정
CMAKE_SOURCE_DIR="."
BUILD_DIR="./build"
DIST_DIR="./dist"
BUILD_TYPE="Release"          # 보통 Release 고정
CONGREGATION_MIN_MACOS_VERSION="${CONGREGATION_MIN_MACOS_VERSION:-12.0}"
CONGREGATION_MACOS_ARCHITECTURE="arm64"
CMAKE_PRESET=""               # 사용 시 preset의 binaryDir가 BUILD_DIR와 일치하도록 맞추는 것이 정석이다.
CMAKE_GENERATOR="Ninja"       # Qt Quick + CMake + Ninja 전제
declare -a CMAKE_EXTRA_ARGS
CMAKE_EXTRA_ARGS=()
IIUPDATEMANAGER_PREFIX="${IIUPDATEMANAGER_PREFIX:-${HOME}/.local/SDK/iiUpdateManager}"
CMAKE_EXTRA_ARGS+=("-DiiUpdateManager_DIR=${IIUPDATEMANAGER_PREFIX}/lib/cmake/iiUpdateManager")
IILICENSEMANAGER_PREFIX="${IILICENSEMANAGER_PREFIX:-${HOME}/.local/SDK/iiLicenseManager}"
CMAKE_EXTRA_ARGS+=("-DiiLicenseManager_DIR=${IILICENSEMANAGER_PREFIX}/lib/cmake/iiLicenseManager")
# 빌드 병렬도(빈 값이면 cmake 기본 동작)
CMAKE_BUILD_PARALLEL=""       # 예: "12"

# Qt 배포 도구(macdeployqt)
MACDEPLOYQT="macdeployqt"
QML_DIR="./App/qml"               # Qt Quick 프로젝트의 QML 루트(필수에 가깝다)
MACDEPLOYQT_VERBOSE="2"       # 0-3
MACDEPLOYQT_NO_STRIP="${MACDEPLOYQT_NO_STRIP:-}"
MACDEPLOYQT_ALWAYS_OVERWRITE="1" # 1이면 -always-overwrite
MACDEPLOYQT_LIBPATH="${MACDEPLOYQT_LIBPATH:-${IIUPDATEMANAGER_PREFIX}/lib}"
MACDEPLOYQT_LICENSE_LIBPATH="${MACDEPLOYQT_LICENSE_LIBPATH:-${IILICENSEMANAGER_PREFIX}/lib}"
# macdeployqt 추가 옵션이 필요하면 아래 배열에 추가한다.
MACDEPLOYQT_EXTRA_ARGS=(
  # "-codesign="
)

if [[ -z "$MACDEPLOYQT_NO_STRIP" ]]; then
  if [[ "$BUILD_MODE" == "local" ]]; then
    MACDEPLOYQT_NO_STRIP="1"
  else
    MACDEPLOYQT_NO_STRIP="0"
  fi
fi

# pkg 설치 경로(요구사항: /Applications)
INSTALL_DIR="/Applications"

# pkg 메타데이터
APP_VERSION=""                # 비워 두면 dist/<App>.app의 Info.plist에서 CFBundleShortVersionString을 자동으로 읽는다.
PKG_ID_DEVID="com.iisacc.app.congregation.pkg"
PKG_ID_MAS="com.iisacc.congregation.painter"

# 엔타이틀먼트(MAS는 사실상 필수)
ENTITLEMENTS_MAS_APP="./packaging/macos/Congregation.entitlements"
ENTITLEMENTS_MAS_INHERIT=""   # helper(.xpc/.appex/.app) 상속용(필요 시)
# Developer ID는 일반적으로 비우는 편이나, 필요한 앱은 지정한다.
ENTITLEMENTS_DEVID_APP=""
ENTITLEMENTS_DEVID_INHERIT=""

# MAS 수동 서명 워크플로에서 provisioning profile을 내장해야 하면 지정한다.
MAS_PROVISIONPROFILE=""

# 인증서 Common Name(키체인에 표시되는 이름 그대로)
LOCAL_APP_CERT="${LOCAL_APP_CERT:-}"
DEVID_APP_CERT="${DEVID_APP_CERT:-Developer ID Application: MUYEONG YUN (5U49ST9XZH)}"
DEVID_INSTALLER_CERT="${DEVID_INSTALLER_CERT:-Developer ID Installer: MUYEONG YUN (5U49ST9XZH)}"
MAS_APP_CERT="${MAS_APP_CERT:-Apple Distribution: MUYEONG YUN (5U49ST9XZH)}"
MAS_INSTALLER_CERT="${MAS_INSTALLER_CERT:-3rd Party Mac Developer Installer: MUYEONG YUN (5U49ST9XZH)}"

# notarization 인증(권장: keychain profile)
NOTARY_KEYCHAIN_PROFILE="${NOTARY_KEYCHAIN_PROFILE:-notary-main}"    # 예: "notary-main"
NOTARY_KEYCHAIN_PATH="${NOTARY_KEYCHAIN_PATH:-}"       # 필요 시 login.keychain-db 같은 경로
# 대안: App Store Connect API Key
NOTARY_API_KEY_P8="${NOTARY_API_KEY_P8:-}"          # /path/to/AuthKey_XXXXXXXXXX.p8
NOTARY_API_KEY_ID="${NOTARY_API_KEY_ID:-}"          # Key ID
NOTARY_API_ISSUER_ID="${NOTARY_API_ISSUER_ID:-}"       # Issuer ID(환경에 따라 필요)
# 최후 대안: Apple ID + app-specific password(권장되지 않음)
NOTARY_APPLE_ID="${NOTARY_APPLE_ID:-ymyeong0504@gmail.com}"            # example@domain.com
NOTARY_TEAM_ID="${NOTARY_TEAM_ID:-5U49ST9XZH}"             # 10자리 Team ID
NOTARY_APP_PASSWORD="${NOTARY_APP_PASSWORD:-}"        # xxxx-xxxx-xxxx-xxxx

# 산출물
OUT_LOCAL_DEVID_PKG="${DIST_DIR}/${APP_NAME}-local-unsigned.pkg"
OUT_LOCAL_MAS_PKG="${DIST_DIR}/${APP_NAME}-appstore-local-unsigned.pkg"
OUT_DEVID_PKG="${DIST_DIR}/${APP_NAME}.pkg"
OUT_MAS_PKG="${DIST_DIR}/${APP_NAME}-appstore.pkg"

# 디버깅/보존 옵션
KEEP_STAGED_APPS="0"          # 1이면 dist에 배포 준비된 .app 사본도 남긴다(추가 산출물)
# =============================================================================

set_notary_auth_args() {
  NOTARY_AUTH_ARGS=()

  if [[ "$NOTARY_MODE" == "profile" ]]; then
    NOTARY_AUTH_ARGS+=(--keychain-profile "$NOTARY_KEYCHAIN_PROFILE")
    if [[ -n "$NOTARY_KEYCHAIN_PATH" ]]; then
      NOTARY_AUTH_ARGS+=(--keychain "$NOTARY_KEYCHAIN_PATH")
    fi
    if [[ -n "$NOTARY_TEAM_ID" ]]; then
      NOTARY_AUTH_ARGS+=(--team-id "$NOTARY_TEAM_ID")
    fi
    return 0
  fi

  if [[ "$NOTARY_MODE" == "api" ]]; then
    NOTARY_AUTH_ARGS+=(--key "$NOTARY_API_KEY_P8" --key-id "$NOTARY_API_KEY_ID")
    if [[ -n "$NOTARY_API_ISSUER_ID" ]]; then
      NOTARY_AUTH_ARGS+=(--issuer "$NOTARY_API_ISSUER_ID")
    fi
    return 0
  fi

  NOTARY_AUTH_ARGS+=(--apple-id "$NOTARY_APPLE_ID" --team-id "$NOTARY_TEAM_ID" --password "$NOTARY_APP_PASSWORD")
}

validate_notary_credentials() {
  set_notary_auth_args

  local output=""
  local rc=0
  say "notarization credential preflight begins"
  set +e
  output="$(xcrun notarytool history "${NOTARY_AUTH_ARGS[@]}" 2>&1)"
  rc=$?
  set -e

  if [[ "$rc" -ne 0 ]]; then
    say "notarization credential preflight failed:" >&2
    printf '%s\n' "$output" | sed -n '1,80p' >&2
    die "notarization credential preflight failed; configure a valid NOTARY_KEYCHAIN_PROFILE or another supported credential mode"
  fi

  say "notarization credentials verified"
}

# =============================================================================
# 도구 점검
# =============================================================================

need_cmd cmake
need_cmd "$MACDEPLOYQT"
need_cmd codesign
need_cmd security
need_cmd ditto
need_cmd xattr
need_cmd find
need_cmd awk
need_cmd sort
need_cmd cut
need_cmd grep
need_cmd sed
need_cmd cmp
need_cmd /usr/bin/plutil
need_cmd /usr/libexec/PlistBuddy
need_cmd /usr/bin/file
need_cmd /usr/bin/lipo
need_cmd /usr/bin/otool
need_cmd /usr/bin/install_name_tool
if [[ "$RUN_TESTS" == "1" ]]; then
  need_cmd ctest
fi

# Ninja 제너레이터를 전제로 하는 경우 ninja 자체도 점검한다.
if [[ "$CMAKE_GENERATOR" == "Ninja" ]]; then
  need_cmd ninja
fi

if mode_wants_package; then
  need_cmd pkgutil
  need_xcrun_tool productbuild
fi
if mode_wants_devid; then
  need_cmd spctl
  need_xcrun_tool pkgbuild
  need_xcrun_tool productsign
  need_xcrun_tool notarytool
  need_xcrun_tool stapler
fi

# =============================================================================
# 입력/설정 검증
# =============================================================================

require_dir "$CMAKE_SOURCE_DIR"
require_file "${IIUPDATEMANAGER_PREFIX}/lib/cmake/iiUpdateManager/iiUpdateManagerConfig.cmake"
require_dir "${IIUPDATEMANAGER_PREFIX}/lib"
require_file "${IILICENSEMANAGER_PREFIX}/lib/cmake/iiLicenseManager/iiLicenseManagerConfig.cmake"
require_dir "${IILICENSEMANAGER_PREFIX}/lib"
run mkdir -p "$BUILD_DIR" "$DIST_DIR"

[[ "$INSTALL_DIR" == /* ]] || die "INSTALL_DIR must be an absolute path, got: $INSTALL_DIR"
[[ "$INSTALL_DIR" != */ ]] || INSTALL_DIR="${INSTALL_DIR%/}"

# QML_DIR은 Qt Quick 배포에서 누락되면 macdeployqt가 QML import를 충분히 수집하지 못하는 경우가 흔하므로 기본적으로 강제한다.
require_dir "$QML_DIR"

# 인증서 유효성 점검(단순 존재가 아니라 'valid identities only'에 포함되는지 확인)
LOCAL_SIGN_IDENTITY=""
if mode_wants_local; then
  LOCAL_SIGN_IDENTITY="$(resolve_local_codesign_identity)"
  say "local codesigning identity: $LOCAL_SIGN_IDENTITY"
fi
if mode_wants_devid; then
  identity_valid codesigning "$DEVID_APP_CERT" || die "valid codesigning identity not found: $DEVID_APP_CERT"
  identity_valid installer  "$DEVID_INSTALLER_CERT" || die "valid installer identity not found: $DEVID_INSTALLER_CERT"
fi
if mode_wants_mas; then
  identity_valid codesigning "$MAS_APP_CERT" || die "valid codesigning identity not found: $MAS_APP_CERT"
  identity_valid installer  "$MAS_INSTALLER_CERT" || die "valid installer identity not found: $MAS_INSTALLER_CERT"
fi

# 엔타이틀먼트 문법 점검
if mode_wants_mas; then
  lint_plist "$ENTITLEMENTS_MAS_APP"
  if [[ -n "$ENTITLEMENTS_MAS_INHERIT" ]]; then lint_plist "$ENTITLEMENTS_MAS_INHERIT"; fi
  if [[ -n "$MAS_PROVISIONPROFILE" ]]; then require_file "$MAS_PROVISIONPROFILE"; fi
fi
if mode_wants_devid; then
  if [[ -n "$ENTITLEMENTS_DEVID_APP" ]]; then lint_plist "$ENTITLEMENTS_DEVID_APP"; fi
  if [[ -n "$ENTITLEMENTS_DEVID_INHERIT" ]]; then lint_plist "$ENTITLEMENTS_DEVID_INHERIT"; fi
fi

# notarization 인증 수단 선택(하나 이상 필수)
NOTARY_MODE=""
if mode_wants_devid; then
  if [[ -n "$NOTARY_KEYCHAIN_PROFILE" ]]; then
    NOTARY_MODE="profile"
  elif [[ -n "$NOTARY_API_KEY_P8" && -n "$NOTARY_API_KEY_ID" ]]; then
    NOTARY_MODE="api"
  elif [[ -n "$NOTARY_APPLE_ID" && -n "$NOTARY_TEAM_ID" ]]; then
    NOTARY_MODE="appleid"
  else
    die "notarization auth is not configured. Set NOTARY_KEYCHAIN_PROFILE (recommended) or (NOTARY_API_KEY_P8/NOTARY_API_KEY_ID[/NOTARY_API_ISSUER_ID]) or (NOTARY_APPLE_ID/NOTARY_TEAM_ID[/NOTARY_APP_PASSWORD])."
  fi
  if [[ "$NOTARY_MODE" == "api" ]]; then require_file "$NOTARY_API_KEY_P8"; fi
  if [[ "$NOTARY_MODE" == "appleid" && -z "$NOTARY_APP_PASSWORD" ]]; then
    die "appleid notarization mode selected but NOTARY_APP_PASSWORD is empty. Prefer keychain profile; otherwise set app-specific password."
  fi
  validate_notary_credentials
fi

# =============================================================================
# 1) CMake Configure/Build
# =============================================================================

say "=== configure ==="
if [[ "$CLEAN_BUILD_DIR" == "1" ]]; then
  say "cleaning build directory: $BUILD_DIR"
  run rm -rf "$BUILD_DIR"
  run mkdir -p "$BUILD_DIR"
fi

if [[ -n "$CMAKE_PRESET" ]]; then
  run cmake --preset "$CMAKE_PRESET"
else
  GEN_ARGS=()
  if [[ -n "$CMAKE_GENERATOR" ]]; then
    GEN_ARGS+=(-G "$CMAKE_GENERATOR")
  fi
  TESTING_ARGS=()
  if [[ "$RUN_TESTS" == "1" ]]; then
    TESTING_ARGS+=(-DBUILD_TESTING=ON)
  fi

  CMAKE_CONFIGURE_ARGS=(cmake -S "$CMAKE_SOURCE_DIR" -B "$BUILD_DIR")
  # macOS 기본 Bash 3.2는 set -u 상태에서 빈 배열의 "${array[@]}" 확장을 실패시킨다.
  # 선택 인자 배열은 길이를 확인한 뒤에만 configure 명령에 붙인다.
  if [[ "${#GEN_ARGS[@]}" -gt 0 ]]; then
    CMAKE_CONFIGURE_ARGS+=("${GEN_ARGS[@]}")
  fi
  CMAKE_CONFIGURE_ARGS+=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE")
  CMAKE_CONFIGURE_ARGS+=(-DCMAKE_OSX_DEPLOYMENT_TARGET="$CONGREGATION_MIN_MACOS_VERSION")
  CMAKE_CONFIGURE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="$CONGREGATION_MACOS_ARCHITECTURE")
  if [[ "${#TESTING_ARGS[@]}" -gt 0 ]]; then
    CMAKE_CONFIGURE_ARGS+=("${TESTING_ARGS[@]}")
  fi
  if [[ "${#CMAKE_EXTRA_ARGS[@]}" -gt 0 ]]; then
    CMAKE_CONFIGURE_ARGS+=("${CMAKE_EXTRA_ARGS[@]}")
  fi
  run "${CMAKE_CONFIGURE_ARGS[@]}"
fi

say "=== build ==="
BUILD_ARGS=(cmake --build "$BUILD_DIR" --config "$BUILD_TYPE")
if [[ -n "$CMAKE_BUILD_PARALLEL" ]]; then
  BUILD_ARGS+=(--parallel "$CMAKE_BUILD_PARALLEL")
fi
run "${BUILD_ARGS[@]}"

if [[ "$RUN_TESTS" == "1" ]]; then
  say "=== test ==="
  run ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# =============================================================================
# 2) Locate .app and stage to dist as canonical name
# =============================================================================

say "=== locate app bundle ==="
FOUND_APP=""
CANDIDATES=(
  "$BUILD_DIR/$BUILD_TYPE/$APP_NAME.app"
  "$BUILD_DIR/$APP_NAME.app"
  "$BUILD_DIR/bin/$APP_NAME.app"
  "$BUILD_DIR/out/$APP_NAME.app"
)

for p in "${CANDIDATES[@]}"; do
  if [[ -d "$p" ]]; then
    FOUND_APP="$p"
    break
  fi
done

if [[ -z "$FOUND_APP" ]]; then
  FOUND_APP="$(find "$BUILD_DIR" -maxdepth 4 -type d -name "${APP_NAME}.app" -print -quit || true)"
fi

[[ -n "$FOUND_APP" ]] || die "built app bundle not found. expected ${APP_NAME}.app under: $BUILD_DIR"
say "built app: $FOUND_APP"
require_nonempty_file "$FOUND_APP/Contents/MacOS/$APP_NAME"

say "=== stage built app to dist ==="
DIST_APP="${DIST_DIR}/${APP_NAME}.app"
run rm -rf "$DIST_APP" || true
run ditto --rsrc "$FOUND_APP" "$DIST_APP"
run xattr -rc "$DIST_APP" || true
require_nonempty_file "$DIST_APP/Contents/MacOS/$APP_NAME"
assert_app_icon_bundle "$DIST_APP"

# APP_VERSION 자동 결정(비어 있으면 dist 앱 Info.plist에서 추출) 후 마케팅 버전과
# 단조 증가 App Store 빌드 번호를 각각 검증한다.
INFO_PLIST="${DIST_APP}/Contents/Info.plist"
require_file "$INFO_PLIST"
PLIST_MARKETING_VERSION="$(plist_get "$INFO_PLIST" CFBundleShortVersionString)"
PLIST_BUNDLE_VERSION="$(plist_get "$INFO_PLIST" CFBundleVersion)"
if [[ -z "$APP_VERSION" ]]; then
  APP_VERSION="$PLIST_MARKETING_VERSION"
fi
[[ -n "$APP_VERSION" ]] || die "APP_VERSION is empty and CFBundleShortVersionString not found in Info.plist. Set APP_VERSION explicitly."
[[ "$PLIST_MARKETING_VERSION" == "$APP_VERSION" ]] \
  || die "CFBundleShortVersionString does not match APP_VERSION: ${PLIST_MARKETING_VERSION:-<missing>} != $APP_VERSION"
[[ "$PLIST_BUNDLE_VERSION" =~ ^[0-9]+([.][0-9]+){0,2}$ ]] \
  || die "CFBundleVersion is not a valid numeric App Store build number: ${PLIST_BUNDLE_VERSION:-<missing>}"
APP_BUNDLE_VERSION="$PLIST_BUNDLE_VERSION"
say "APP_VERSION: $APP_VERSION (build $APP_BUNDLE_VERSION)"

# =============================================================================
# 임시 작업 디렉터리(모드별 스테이징/서명/패키징은 여기서 수행)
# =============================================================================

WORKDIR="$(mktemp -d -t "pkg_all_${APP_NAME}_XXXXXXXX")"
cleanup() { rm -rf "$WORKDIR" >/dev/null 2>&1 || true; }
trap cleanup EXIT

STAGE_DEVID="${WORKDIR}/stage_devid"
STAGE_MAS="${WORKDIR}/stage_mas"
STAGE_LOCAL="${WORKDIR}/stage_local"
mkdir -p "$STAGE_DEVID" "$STAGE_MAS" "$STAGE_LOCAL"

# =============================================================================
# 3) macdeployqt (모드별 분기)
# =============================================================================

macdeployqt_run() {
  local app="$1"
  local mode="$2" # devid|mas

  local -a cmd=("$MACDEPLOYQT" "$app" "-verbose=${MACDEPLOYQT_VERBOSE}")

  if [[ "$MACDEPLOYQT_NO_STRIP" == "1" ]]; then cmd+=("-no-strip"); fi
  if [[ "$MACDEPLOYQT_ALWAYS_OVERWRITE" == "1" ]]; then cmd+=("-always-overwrite"); fi
  cmd+=("-qmldir=${QML_DIR}")
  if [[ -n "$MACDEPLOYQT_LIBPATH" ]]; then cmd+=("-libpath=${MACDEPLOYQT_LIBPATH}"); fi
  if [[ -n "$MACDEPLOYQT_LICENSE_LIBPATH" ]]; then cmd+=("-libpath=${MACDEPLOYQT_LICENSE_LIBPATH}"); fi
  if [[ "$mode" == "mas" ]]; then cmd+=("-appstore-compliant"); fi

  # 추가 옵션
  if [[ "${#MACDEPLOYQT_EXTRA_ARGS[@]}" -gt 0 ]]; then
    cmd+=("${MACDEPLOYQT_EXTRA_ARGS[@]}")
  fi

  run "${cmd[@]}"
}

remove_unused_qt_sql_plugins() {
  local app="$1"
  local sql_plugins="$app/Contents/PlugIns/sqldrivers"

  if [[ -d "$sql_plugins" ]]; then
    say "removing unused Qt SQL driver plugins"
    run rm -rf "$app/Contents/PlugIns/sqldrivers"
  fi
}

remove_absolute_macho_rpaths() {
  local app="$1"

  while IFS= read -r -d '' binary; do
    if ! /usr/bin/file -b "$binary" 2>/dev/null | grep -q 'Mach-O'; then
      continue
    fi

    local runtime_paths
    runtime_paths="$(/usr/bin/otool -l "$binary" 2>/dev/null \
      | awk '$1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
             want_path && $1 == "path" { print $2; want_path = 0 }' \
      | awk '/^\// && !seen[$0]++' || true)"

    if [[ -z "$runtime_paths" ]]; then
      continue
    fi

    while IFS= read -r runtime_path; do
      say "removing non-portable LC_RPATH from ${binary#${app}/}: $runtime_path"
      run /usr/bin/install_name_tool -delete_rpath "$runtime_path" "$binary"
    done <<< "$runtime_paths"
  done < <(find "${app}/Contents" -type f -print0)
}

assert_portable_macho_links() {
  local app="$1"
  local violations="${WORKDIR}/macho_path_violations_$(basename "$app")_$$.txt"
  : > "$violations"

  while IFS= read -r -d '' binary; do
    if ! /usr/bin/file -b "$binary" 2>/dev/null | grep -q 'Mach-O'; then
      continue
    fi

    local dylib_ids
    dylib_ids="$(/usr/bin/otool -D "$binary" 2>/dev/null \
      | sed -nE '/:$/d; /^[[:space:]]*$/d; p' || true)"

    /usr/bin/otool -L "$binary" \
      | sed -nE 's/^[[:space:]]+([^[:space:]]+).*/\1/p' \
      | while IFS= read -r dependency; do
          if grep -Fx "$dependency" <<<"$dylib_ids" >/dev/null; then
            continue
          fi
          case "$dependency" in
            ""|@*|/System/Library/*|/usr/lib/*) ;;
            /*) printf '%s: dependency %s\n' "$binary" "$dependency" >> "$violations" ;;
          esac
        done

    /usr/bin/otool -l "$binary" \
      | awk '$1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
             want_path && $1 == "path" { print $2; want_path = 0 }' \
      | while IFS= read -r runtime_path; do
          case "$runtime_path" in
            ""|@*) ;;
            /*) printf '%s: LC_RPATH %s\n' "$binary" "$runtime_path" >> "$violations" ;;
          esac
        done
  done < <(find "${app}/Contents" -type f -print0)

  if [[ -s "$violations" ]]; then
    sed -n '1,80p' "$violations" >&2
    die "deployed app contains non-portable absolute Mach-O paths: $app"
  fi
}

assert_update_manager_runtime() {
  local app="$1"
  local executable="${app}/Contents/MacOS/${APP_NAME}"
  require_nonempty_file "$executable"

  # Contract tests use a shell-script stand-in; deployed Congregation binaries are Mach-O.
  if ! /usr/bin/file -b "$executable" 2>/dev/null | grep -q 'Mach-O'; then
    return 0
  fi

  /usr/bin/otool -L "$executable" \
    | sed -nE 's#^[[:space:]]+(@rpath/libiiUpdateManager[^[:space:]]*[.]dylib).*#\1#p' \
    | grep -q . \
    || die "Congregation does not link the iiUpdateManager runtime"

  local runtime
  runtime="$(find "${app}/Contents/Frameworks" -maxdepth 1 \
    \( -type f -o -type l \) -name 'libiiUpdateManager*.dylib' -print -quit 2>/dev/null || true)"
  require_nonempty_file "$runtime"
  /usr/bin/file -b "$runtime" | grep -q 'Mach-O' \
    || die "bundled iiUpdateManager runtime is not a Mach-O library: $runtime"
}

assert_license_manager_runtime() {
  local app="$1"
  local executable="${app}/Contents/MacOS/${APP_NAME}"
  require_nonempty_file "$executable"

  # Contract tests use a shell-script stand-in; deployed Congregation binaries are Mach-O.
  if ! /usr/bin/file -b "$executable" 2>/dev/null | grep -q 'Mach-O'; then
    return 0
  fi

  /usr/bin/otool -L "$executable" \
    | sed -nE 's#^[[:space:]]+(@rpath/libiiLicenseManager[^[:space:]]*[.]dylib).*#\1#p' \
    | grep -q . \
    || die "Congregation does not link the iiLicenseManager runtime"

  local runtime
  runtime="$(find "${app}/Contents/Frameworks" -maxdepth 1 \
    \( -type f -o -type l \) -name 'libiiLicenseManager*.dylib' -print -quit 2>/dev/null || true)"
  require_nonempty_file "$runtime"
  /usr/bin/file -b "$runtime" | grep -q 'Mach-O' \
    || die "bundled iiLicenseManager runtime is not a Mach-O library: $runtime"
}

assert_macos_deployment_targets() {
  local app="$1"
  local violations="${WORKDIR}/macho_deployment_target_violations_$(basename "$app")_$$.txt"
  : > "$violations"

  while IFS= read -r -d '' binary; do
    if ! /usr/bin/file -b "$binary" 2>/dev/null | grep -q 'Mach-O'; then
      continue
    fi

    local deployment_targets
    deployment_targets="$(/usr/bin/otool -l "$binary" 2>/dev/null \
      | awk '$1 == "cmd" && $2 == "LC_BUILD_VERSION" { mode = "build"; next }
             $1 == "cmd" && $2 == "LC_VERSION_MIN_MACOSX" { mode = "legacy"; next }
             mode == "build" && $1 == "minos" { print $2; mode = ""; next }
             mode == "legacy" && $1 == "version" { print $2; mode = ""; next }' || true)"

    if [[ -z "$deployment_targets" ]]; then
      printf '%s: missing macOS deployment target load command\n' "$binary" >> "$violations"
      continue
    fi

    while IFS= read -r deployment_target; do
      if [[ "$deployment_target" != "$CONGREGATION_MIN_MACOS_VERSION" ]]; then
        printf '%s: minos %s does not match required macOS deployment target %s\n' \
          "$binary" "$deployment_target" "$CONGREGATION_MIN_MACOS_VERSION" >> "$violations"
      fi
    done <<< "$deployment_targets"
  done < <(find "${app}/Contents" -type f -print0)

  if [[ -s "$violations" ]]; then
    sed -n '1,80p' "$violations" >&2
    die "deployed app contains Mach-O binaries with an incompatible macOS deployment target: $app"
  fi
}

assert_macos_architectures() {
  local app="$1"
  local violations="${WORKDIR}/macho_architecture_violations_$(basename "$app")_$$.txt"
  : > "$violations"

  while IFS= read -r -d '' binary; do
    if ! /usr/bin/file -b "$binary" 2>/dev/null | grep -q 'Mach-O'; then
      continue
    fi

    local architectures
    architectures="$(/usr/bin/lipo -archs "$binary" 2>/dev/null || true)"
    if [[ -z "$architectures" ]]; then
      printf '%s: unable to read Mach-O architectures\n' "$binary" >> "$violations"
      continue
    fi

    case " $architectures " in
      *" $CONGREGATION_MACOS_ARCHITECTURE "*) ;;
      *)
        printf '%s: architectures %s do not include required %s slice\n' \
          "$binary" "$architectures" "$CONGREGATION_MACOS_ARCHITECTURE" >> "$violations"
        ;;
    esac

    local relative_path="${binary#${app}/Contents/}"
    case "$relative_path" in
      "MacOS/${APP_NAME}"|Frameworks/libLVRS.dylib|Frameworks/libiiPaintEngine.dylib|Frameworks/libiiUpdateManager*.dylib|Frameworks/libiiLicenseManager*.dylib)
        if [[ "$architectures" != "$CONGREGATION_MACOS_ARCHITECTURE" ]]; then
          printf '%s: Congregation-owned Mach-O architectures %s must be exactly %s\n' \
            "$binary" "$architectures" "$CONGREGATION_MACOS_ARCHITECTURE" >> "$violations"
        fi
        ;;
    esac
  done < <(find "${app}/Contents" -type f -print0)

  if [[ -s "$violations" ]]; then
    sed -n '1,80p' "$violations" >&2
    die "deployed app contains Mach-O binaries with incompatible architectures: $app"
  fi
}

set_distribution_channel() {
  local app="$1"
  local distribution_channel="$2"
  local info_plist="${app}/Contents/Info.plist"

  require_file "$info_plist"
  if /usr/bin/plutil -extract IISACCDistributionChannel raw "$info_plist" >/dev/null 2>&1; then
    run /usr/bin/plutil -replace IISACCDistributionChannel -string "$distribution_channel" "$info_plist"
  else
    run /usr/bin/plutil -insert IISACCDistributionChannel -string "$distribution_channel" "$info_plist"
  fi
  lint_plist "$info_plist"
}

prepare_app() {
  local mode="$1"
  local stage_dir="$2"
  local out_app="${stage_dir}/${APP_NAME}.app"

  run rm -rf "$out_app" || true
  run ditto --rsrc "$DIST_APP" "$out_app"
  run xattr -rc "$out_app" || true

  local distribution_channel="direct"
  if [[ "$mode" == "mas" ]]; then
    distribution_channel="appstore"
  fi
  set_distribution_channel "$out_app" "$distribution_channel"

  say "macdeployqt begins: mode=$mode"
  macdeployqt_run "$out_app" "$mode"
  remove_unused_qt_sql_plugins "$out_app"
  remove_absolute_macho_rpaths "$out_app"
  assert_update_manager_runtime "$out_app"
  assert_license_manager_runtime "$out_app"
  assert_portable_macho_links "$out_app"
  assert_macos_deployment_targets "$out_app"
  assert_macos_architectures "$out_app"

  if [[ "$mode" == "mas" && -n "$MAS_PROVISIONPROFILE" ]]; then
    say "embedding provisioning profile for MAS"
    run ditto "$MAS_PROVISIONPROFILE" "$out_app/Contents/embedded.provisionprofile"
  fi

  assert_app_icon_bundle "$out_app"
  printf '%s\n' "$out_app"
}

# =============================================================================
# 4) codesign (트리 정렬 서명)
# =============================================================================

list_signables_desc() {
  local app="$1"
  local signables_raw="${WORKDIR}/signables_raw_$(basename "$app")_$$.txt"
  : > "$signables_raw"

  # 번들/프레임워크 단위 대상
  find "${app}/Contents" -type d \( \
    -name "*.framework" -o -name "*.app" -o -name "*.xpc" -o -name "*.appex" -o -name "*.bundle" -o -name "*.plugin" \
  \) -print >> "$signables_raw"

  # 내부 프레임워크/헬퍼의 확장자 없는 Mach-O 실행 파일까지 포함
  while IFS= read -r -d '' f; do
    if /usr/bin/file -b "$f" 2>/dev/null | grep -q 'Mach-O'; then
      printf '%s\n' "$f" >> "$signables_raw"
    fi
  done < <(
    find "${app}/Contents" -type f \( -perm -111 -o -name "*.dylib" -o -name "*.so" \) -print0
  )

  awk 'NF && !seen[$0]++ { print }' "$signables_raw" \
    | awk '{print length, $0}' \
    | sort -rn \
    | cut -d' ' -f2-
}

is_inherit_bundle() {
  case "$1" in
    *.app|*.xpc|*.appex) return 0 ;;
    *) return 1 ;;
  esac
}

codesign_one() {
  local target="$1"
  local identity="$2"
  local mode="$3"          # local|devid|mas
  local entitlements="${4:-}"

  local -a cmd=(codesign --force --sign "$identity")

  if [[ "$mode" != "local" ]]; then
    cmd+=(--timestamp)
  fi

  # Developer ID 배포는 Hardened Runtime을 전제로 공증을 통과하는 흐름이 일반적이다.
  if [[ "$mode" == "devid" ]]; then
    cmd+=(--options runtime)
  fi

  if [[ -n "$entitlements" ]]; then
    cmd+=(--entitlements "$entitlements")
  fi

  cmd+=("$target")
  run "${cmd[@]}"
}

sign_app_tree() {
  local app="$1"
  local identity="$2"
  local mode="$3"           # local|devid|mas
  local ent_app="${4:-}"
  local ent_inherit="${5:-}"

  require_dir "$app"
  run xattr -rc "$app" || true

  local sign_list_file="${WORKDIR}/signables_$(basename "$app")_${mode}.txt"
  list_signables_desc "$app" > "$sign_list_file"

  while IFS= read -r target; do
    [[ -z "$target" ]] && continue
    if is_inherit_bundle "$target" && [[ -n "$ent_inherit" ]]; then
      codesign_one "$target" "$identity" "$mode" "$ent_inherit"
    else
      codesign_one "$target" "$identity" "$mode"
    fi
  done < "$sign_list_file"

  if [[ -n "$ent_app" ]]; then
    codesign_one "$app" "$identity" "$mode" "$ent_app"
  else
    codesign_one "$app" "$identity" "$mode"
  fi

  while IFS= read -r target; do
    [[ -z "$target" ]] && continue
    run codesign --verify --strict --verbose=2 "$target"
  done < "$sign_list_file"

  run codesign --verify --deep --strict --verbose=2 "$app"
}

# =============================================================================
# 5) pkg 생성: INSTALL_DIR 고정(/Applications)
# =============================================================================

write_product_requirements() {
  local requirements_plist="$1"

  run rm -f "$requirements_plist" || true
  run /usr/bin/plutil -create xml1 "$requirements_plist"
  run /usr/bin/plutil -insert os -json "[\"$CONGREGATION_MIN_MACOS_VERSION\"]" "$requirements_plist"
  run /usr/bin/plutil -insert arch -json "[\"$CONGREGATION_MACOS_ARCHITECTURE\"]" "$requirements_plist"
  lint_plist "$requirements_plist"
}

verify_pkg_distribution() {
  local pkg="$1"
  local pkg_id="$2"
  local identity_kind="$3"
  local expanded_dir="${WORKDIR}/distribution_$(basename "$pkg" .pkg)"
  local distribution="${expanded_dir}/Distribution"

  run rm -rf "$expanded_dir" || true
  run pkgutil --expand "$pkg" "$expanded_dir"
  require_file "$distribution"

  grep -F "hostArchitectures=\"$CONGREGATION_MACOS_ARCHITECTURE\"" "$distribution" >/dev/null \
    || die "package Distribution is not ${CONGREGATION_MACOS_ARCHITECTURE}-only: $pkg"
  grep -F "<os-version min=\"$CONGREGATION_MIN_MACOS_VERSION\"" "$distribution" >/dev/null \
    || die "package Distribution does not require macOS $CONGREGATION_MIN_MACOS_VERSION or later: $pkg"
  grep -F "path=\"${APP_NAME}.app\"" "$distribution" \
    | grep -F "CFBundleShortVersionString=\"$APP_VERSION\"" \
    | grep -F "CFBundleVersion=\"$APP_BUNDLE_VERSION\"" >/dev/null \
    || die "package Distribution app bundle versions do not match staged app: $pkg"
  case "$identity_kind" in
    pkg-ref)
      grep -F "<pkg-ref id=\"$pkg_id\" version=\"$APP_VERSION\"" "$distribution" >/dev/null \
        || die "package Distribution component identity/version does not match APP_VERSION: $pkg"
      ;;
    product)
      grep -F "<product id=\"$pkg_id\" version=\"$APP_VERSION\"" "$distribution" >/dev/null \
        || die "package Distribution product identity/version does not match APP_VERSION: $pkg"
      ;;
    *)
      die "unsupported package Distribution identity kind: $identity_kind"
      ;;
  esac
}

build_signed_pkg() {
  local app="$1"
  local pkg_id="$2"
  local installer_identity="$3"
  local out_pkg="$4"

  require_dir "$app"

  local base="$(basename "$out_pkg" .pkg)"
  local payload_root="${WORKDIR}/payload_${base}"
  local comp_plist="${WORKDIR}/components_${base}.plist"
  local comp_pkg="${WORKDIR}/component_${base}.pkg"
  local unsigned_pkg="${WORKDIR}/unsigned_${base}.pkg"
  local requirements_plist="${WORKDIR}/requirements_${base}.plist"

  run rm -rf "$payload_root" "$comp_plist" "$comp_pkg" "$unsigned_pkg" "$requirements_plist" "$out_pkg" || true
  run mkdir -p "$payload_root"

  # payload_root에 .app을 직접 배치하고 install-location을 /Applications로 고정한다.
  run ditto --rsrc "$app" "$payload_root/$(basename "$app")"

  # component property list 생성 후, 번들 재배치(relocation)를 비활성화한다.
  run xcrun pkgbuild --analyze --root "$payload_root" "$comp_plist"
  run /usr/bin/plutil -replace BundleIsRelocatable -bool NO "$comp_plist"

  run xcrun pkgbuild \
    --root "$payload_root" \
    --install-location "$INSTALL_DIR" \
    --component-plist "$comp_plist" \
    --identifier "$pkg_id" \
    --version "$APP_VERSION" \
    "$comp_pkg"

  write_product_requirements "$requirements_plist"
  run xcrun productbuild \
    --product "$requirements_plist" \
    --package "$comp_pkg" \
    "$unsigned_pkg"

  run xcrun productsign \
    --sign "$installer_identity" \
    "$unsigned_pkg" \
    "$out_pkg"

  run pkgutil --check-signature "$out_pkg"
  verify_pkg_icon_payload "$out_pkg" "$app"
  verify_pkg_distribution "$out_pkg" "$pkg_id" "pkg-ref"
}

# =============================================================================
# 5b) MAS용 pkg 생성: productbuild --component (App Store 요구사항)
# =============================================================================

build_unsigned_component_pkg() {
  local app="$1"
  local pkg_id="$2"
  local out_pkg="$3"
  local requirements_plist="${WORKDIR}/requirements_$(basename "$out_pkg" .pkg).plist"

  require_dir "$app"

  run rm -f "$requirements_plist" "$out_pkg" || true
  write_product_requirements "$requirements_plist"
  run xcrun productbuild \
    --product "$requirements_plist" \
    --component "$app" "$INSTALL_DIR" \
    --identifier "$pkg_id" \
    --version "$APP_VERSION" \
    "$out_pkg"

  run pkgutil --check-signature "$out_pkg" || true
  verify_pkg_icon_payload "$out_pkg" "$app"
  verify_pkg_distribution "$out_pkg" "$pkg_id" "product"
}

build_mas_pkg() {
  local app="$1"
  local pkg_id="$2"
  local installer_identity="$3"
  local out_pkg="$4"
  local requirements_plist="${WORKDIR}/requirements_$(basename "$out_pkg" .pkg).plist"

  require_dir "$app"

  run rm -f "$requirements_plist" "$out_pkg" || true
  write_product_requirements "$requirements_plist"
  run xcrun productbuild \
    --product "$requirements_plist" \
    --component "$app" "$INSTALL_DIR" \
    --identifier "$pkg_id" \
    --version "$APP_VERSION" \
    --sign "$installer_identity" \
    "$out_pkg"

  run pkgutil --check-signature "$out_pkg"
  verify_pkg_icon_payload "$out_pkg" "$app"
  verify_pkg_distribution "$out_pkg" "$pkg_id" "product"
}

# =============================================================================
# 6) notarization + stapling (Developer ID pkg)
# =============================================================================

notarize_and_staple_pkg() {
  local pkg="$1"
  require_file "$pkg"

  set_notary_auth_args

  local submit_out="${WORKDIR}/notary_submit.out"
  local submit_err="${WORKDIR}/notary_submit.err"

  say "notarytool submit begins (Developer ID pkg)"
  set +e
  xcrun notarytool submit "$pkg" --wait "${NOTARY_AUTH_ARGS[@]}" >"$submit_out" 2>"$submit_err"
  local rc=$?
  set -e

  if [[ "$rc" -ne 0 ]]; then
    say "notarytool submit failed. stdout (first 200 lines):" >&2
    sed -n '1,200p' "$submit_out" >&2 || true
    say "notarytool submit failed. stderr (first 200 lines):" >&2
    sed -n '1,200p' "$submit_err" >&2 || true
    die "notarization failed"
  fi

  # 출력 형식 차이를 흡수하기 위한 보수적 파싱(id/status)
  local req_id=""
  local status=""

  req_id="$(grep -Eo 'id[[:space:]]*:[[:space:]]*[0-9a-fA-F-]+' "$submit_out" | head -n 1 | sed -E 's/.*id[[:space:]]*:[[:space:]]*//')"
  status="$(grep -Eo 'status[[:space:]]*:[[:space:]]*[A-Za-z]+' "$submit_out" | tail -n 1 | sed -E 's/.*status[[:space:]]*:[[:space:]]*//')"

  if [[ -z "$status" ]]; then
    status="$(grep -Eo '"status"[[:space:]]*:[[:space:]]*"[^"]+"' "$submit_out" | head -n 1 | sed -E 's/.*"status"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')"
  fi
  if [[ -z "$req_id" ]]; then
    req_id="$(grep -Eo '"id"[[:space:]]*:[[:space:]]*"[^"]+"' "$submit_out" | head -n 1 | sed -E 's/.*"id"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')"
  fi

  if [[ -n "$status" && "$status" != "Accepted" ]]; then
    say "notarization status is not Accepted: $status" >&2
    if [[ -n "$req_id" ]]; then
      say "fetching notary log for request id: $req_id" >&2
      set +e
      xcrun notarytool log "$req_id" "${NOTARY_AUTH_ARGS[@]}" > "${WORKDIR}/notary_log.txt" 2>&1
      set -e
      sed -n '1,250p' "${WORKDIR}/notary_log.txt" >&2 || true
    fi
    die "notarization returned non-Accepted status"
  fi

  run xcrun stapler staple "$pkg"
  run xcrun stapler validate "$pkg"
  run spctl -a -vv -t install "$pkg"
}

# =============================================================================
# 7) 실행
# =============================================================================

say "INPUT (dist app) : $DIST_APP"
say "BUILD_MODE       : $BUILD_MODE"
say "DIST_DIR         : $DIST_DIR"
say "INSTALL_DIR      : $INSTALL_DIR"
if mode_wants_local; then
  say "OUT_LOCAL_PKG    : $OUT_LOCAL_DEVID_PKG"
  say "OUT_LOCAL_APPSTORE_PKG: $OUT_LOCAL_MAS_PKG"
fi
if mode_wants_devid; then say "OUT_DEVID_PKG    : $OUT_DEVID_PKG"; fi
if mode_wants_mas; then say "OUT_MAS_PKG      : $OUT_MAS_PKG"; fi

if mode_wants_local; then
  LOCAL_APP="$(prepare_app local "$STAGE_LOCAL")"
  say "Local signing begins"
  sign_app_tree "$LOCAL_APP" "$LOCAL_SIGN_IDENTITY" local

  say "Local deployed app staging begins"
  run rm -rf "$DIST_APP" || true
  run ditto --rsrc "$LOCAL_APP" "$DIST_APP"
  run xattr -rc "$DIST_APP" || true
  require_nonempty_file "$DIST_APP/Contents/MacOS/$APP_NAME"
  assert_app_icon_bundle "$DIST_APP"
  run codesign --verify --deep --strict --verbose=2 "$DIST_APP"

  say "Local unsigned pkg build begins"
  build_unsigned_component_pkg "$DIST_APP" "$PKG_ID_DEVID" "$OUT_LOCAL_DEVID_PKG"
  require_nonempty_file "$OUT_LOCAL_DEVID_PKG"

  say "Local unsigned appstore-named pkg build begins"
  build_unsigned_component_pkg "$DIST_APP" "$PKG_ID_MAS" "$OUT_LOCAL_MAS_PKG"
  require_nonempty_file "$OUT_LOCAL_MAS_PKG"

  say "done"
  say "$DIST_APP"
  say "$OUT_LOCAL_DEVID_PKG"
  say "$OUT_LOCAL_MAS_PKG"
  exit 0
fi

if mode_wants_devid; then
  # Developer ID용 준비(macdeployqt) -> 서명 -> pkg -> 공증/스테이플
  DEVID_APP="$(prepare_app devid "$STAGE_DEVID")"
  say "Developer ID signing begins"
  sign_app_tree "$DEVID_APP" "$DEVID_APP_CERT" devid "$ENTITLEMENTS_DEVID_APP" "$ENTITLEMENTS_DEVID_INHERIT"

  say "Developer ID pkg build begins"
  TMP_DEVID_PKG="${WORKDIR}/${APP_NAME}.devid.pkg"
  build_signed_pkg "$DEVID_APP" "$PKG_ID_DEVID" "$DEVID_INSTALLER_CERT" "$TMP_DEVID_PKG"
  require_nonempty_file "$TMP_DEVID_PKG"

  say "Developer ID notarization begins"
  notarize_and_staple_pkg "$TMP_DEVID_PKG"

  run rm -f "$OUT_DEVID_PKG" || true
  run mv "$TMP_DEVID_PKG" "$OUT_DEVID_PKG"
  require_nonempty_file "$OUT_DEVID_PKG"
  run pkgutil --check-signature "$OUT_DEVID_PKG"
  run xcrun stapler validate "$OUT_DEVID_PKG"
  run spctl -a -vv -t install "$OUT_DEVID_PKG"
fi

if mode_wants_mas; then
  # MAS용 준비(macdeployqt -appstore-compliant) -> 서명(엔타이틀먼트 필수) -> pkg
  MAS_APP="$(prepare_app mas "$STAGE_MAS")"
  say "MAS signing begins"
  sign_app_tree "$MAS_APP" "$MAS_APP_CERT" mas "$ENTITLEMENTS_MAS_APP" "$ENTITLEMENTS_MAS_INHERIT"

  say "MAS pkg build begins"
  TMP_MAS_PKG="${WORKDIR}/${APP_NAME}.appstore.pkg"
  build_mas_pkg "$MAS_APP" "$PKG_ID_MAS" "$MAS_INSTALLER_CERT" "$TMP_MAS_PKG"
  require_nonempty_file "$TMP_MAS_PKG"

  run rm -f "$OUT_MAS_PKG" || true
  run mv "$TMP_MAS_PKG" "$OUT_MAS_PKG"
  require_nonempty_file "$OUT_MAS_PKG"
fi

# 필요 시 스테이징된 .app을 dist에 보존한다(추가 산출물)
if [[ "$KEEP_STAGED_APPS" == "1" ]]; then
  say "keeping staged apps in dist (extra outputs)"
  run rm -rf "${DIST_DIR}/${APP_NAME}-devid.app" "${DIST_DIR}/${APP_NAME}-mas.app" || true
  if mode_wants_devid; then
    run ditto --rsrc "$DEVID_APP" "${DIST_DIR}/${APP_NAME}-devid.app"
    run xattr -rc "${DIST_DIR}/${APP_NAME}-devid.app" || true
  fi
  if mode_wants_mas; then
    run ditto --rsrc "$MAS_APP" "${DIST_DIR}/${APP_NAME}-mas.app"
    run xattr -rc "${DIST_DIR}/${APP_NAME}-mas.app" || true
  fi
fi

say "done"
if mode_wants_devid; then say "$OUT_DEVID_PKG"; fi
if mode_wants_mas; then say "$OUT_MAS_PKG"; fi
