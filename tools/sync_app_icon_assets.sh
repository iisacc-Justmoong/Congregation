#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

usage() {
  cat >&2 <<'USAGE'
Usage: tools/sync_app_icon_assets.sh [source.icns] [AppIcon.appiconset]

Regenerates the macOS AppIcon asset catalog from the canonical Congregation .icns.
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

source_icon="${1:-resources/Appicon.icns}"
asset_dir="${2:-packaging/macos/Congregation.xcassets/AppIcon.appiconset}"

[[ -f "$source_icon" ]] || { printf 'ERROR: source icon not found: %s\n' "$source_icon" >&2; exit 1; }
[[ -d "$asset_dir" ]] || { printf 'ERROR: asset catalog not found: %s\n' "$asset_dir" >&2; exit 1; }

command -v iconutil >/dev/null 2>&1 || { printf 'ERROR: iconutil not found\n' >&2; exit 1; }
command -v sips >/dev/null 2>&1 || { printf 'ERROR: sips not found\n' >&2; exit 1; }

tmpdir="$(mktemp -d -t congregation_icon_assets_XXXXXX)"
cleanup() { rm -rf "$tmpdir" >/dev/null 2>&1 || true; }
trap cleanup EXIT

iconset="$tmpdir/Appicon.iconset"
iconutil -c iconset -o "$iconset" "$source_icon"

copy_png() {
  local source_name="$1"
  local target_name="$2"
  local source_path="$iconset/$source_name"
  local target_path="$asset_dir/$target_name"

  [[ -f "$source_path" ]] || { printf 'ERROR: generated icon missing: %s\n' "$source_path" >&2; exit 1; }
  sips -s format png "$source_path" --out "$target_path" >/dev/null
}

copy_png "icon_16x16.png" "icon_16x16.png"
copy_png "icon_16x16@2x.png" "icon_16x16@2x.png"
copy_png "icon_32x32.png" "icon_32x32.png"
copy_png "icon_32x32@2x.png" "icon_32x32@2x.png"
copy_png "icon_128x128.png" "icon_128x128.png"
copy_png "icon_128x128@2x.png" "icon_128x128@2x.png"
copy_png "icon_256x256.png" "icon_256x256.png"
copy_png "icon_256x256@2x.png" "icon_256x256@2x.png"
copy_png "icon_512x512.png" "icon_512x512.png"
copy_png "icon_512x512@2x.png" "icon_512x512@2x.png"
copy_png "icon_512x512@2x.png" "AppIcon-1024.png"

printf 'Synced %s from %s\n' "$asset_dir" "$source_icon"
