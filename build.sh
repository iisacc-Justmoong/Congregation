#!/usr/bin/env bash
set -euo pipefail
root_dir="$(cd "$(dirname "$0")" && pwd)"
sdk_prefix="${CONGREGATION_SDK_PREFIX:-$root_dir/../../SDK/build/congregation-native/prefix}"
build_type="${BUILD_TYPE:-Release}"
run_tests="${RUN_TESTS:-1}"
configure_only=0
native_flags=(-DCMAKE_FIND_FRAMEWORK=LAST -DOPENSSL_ROOT_DIR="$sdk_prefix" -DOPENSSL_USE_STATIC_LIBS=ON)
if [[ "$(uname -s)" == Darwin ]]; then
  system_sdk="$(xcrun --show-sdk-path)"
  native_flags+=(-DCURL_LIBRARY="$system_sdk/usr/lib/libcurl.tbd" -DCURL_INCLUDE_DIR="$system_sdk/usr/include" -DLIBXML2_LIBRARY="$system_sdk/usr/lib/libxml2.tbd" -DLIBXML2_INCLUDE_DIR="$system_sdk/usr/include/libxml2")
fi
if [[ ! -f "$root_dir/build/CMakeCache.txt" && -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then
  native_flags+=(-G Ninja)
fi
for option in "$@"; do
  case "$option" in
    local) ;;
    --no-tests) run_tests=0 ;;
    --configure-only) configure_only=1 ;;
    -h|--help) printf '%s\n' 'Usage: ./build.sh [local] [--no-tests] [--configure-only]' 'CONGREGATION_SDK_PREFIX selects an installed native kit. Outputs stay in build/.'; exit 0 ;;
    *) printf 'Unknown option: %s\n' "$option" >&2; exit 2 ;;
  esac
done
cmake -S "$root_dir" -B "$root_dir/build" -DCMAKE_BUILD_TYPE="$build_type" -DCONGREGATION_SDK_PREFIX="$sdk_prefix" -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON -DBUILD_TESTING=ON "${native_flags[@]}"
if [[ "$configure_only" == 1 ]]; then exit 0; fi
cmake --build "$root_dir/build" --parallel "${BUILD_JOBS:-6}"
if [[ "$run_tests" == 1 ]]; then ctest --test-dir "$root_dir/build" --output-on-failure; fi
