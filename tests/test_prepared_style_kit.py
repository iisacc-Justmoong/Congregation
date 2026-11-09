"""Verify the prepared legacy LVRS kit still contains the current design snapshot."""
import hashlib
import importlib.util
import subprocess
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
workspace = root.parents[1]
lvrs = workspace / "SDK/LVRS"
lvui = workspace / "SDK/LVUI"
stage = Path(sys.argv[1]).resolve()
compiler = Path(sys.argv[2]).resolve()

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for part in iter(lambda: source.read(1024 * 1024), b""):
            h.update(part)
    return h.hexdigest()

manifest = json.loads((stage / "manifest.json").read_text(encoding="utf-8"))
assert manifest["kind"] == "congregation-lvrs-style-bridge"

# A different native kit must never be silently paired with the legacy ABI.
spec = importlib.util.spec_from_file_location("prepare_legacy_style", root / "App/Native/prepare_legacy_style.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
legacy_theme = subprocess.check_output(["git", "-C", str(lvrs), "show", bridge.LEGACY_LVRS_COMMIT + ":src/LVRS/Theme.h"])
theme_header = bridge.KIT / "include/LVRS/Theme.h"
bridge.verify_legacy_public_header(legacy_theme, theme_header)
try:
    bridge.verify_legacy_public_header(legacy_theme + b"\n// changed ABI\n", theme_header)
except ValueError:
    pass
else:
    raise AssertionError("Mismatched legacy LVRS headers must be rejected")
assert manifest["figma_catalog_sha256"] == digest(lvrs / "resources/figma-color-tokens.json")
assert manifest["lvui_catalog_sha256"] == digest(lvui / "catalog/lvui-style-tokens.json")
assert manifest["compiler_sha256"] == digest(compiler)
assert manifest["lvrs_bridged_archive_sha256"] == digest(stage / "lib/libLVRS.a")
assert digest(stage / "source/FigmaColors.h") == digest(lvrs / "src/Styling/FigmaColors.h")
assert manifest["legacy_color_replacements"] >= 40 and manifest["recompiled_legacy_sources"] >= 40

assets = {}
for filename in ("NativeIcons.json", "NativeAlertAssets.json", "NativeCardAssets.json"):
    data = json.loads((stage / filename).read_text(encoding="utf-8"))
    for entry in data if isinstance(data, list) else data["assets"]:
        assets[entry.get("path", entry.get("source"))] = entry["sha256"]
bindings = json.loads((lvrs / "resources/figma-icon-token-bindings.json").read_text(encoding="utf-8"))
for icon in bindings["icons"]:
    key = "resources/" + icon["path"]
    assert key in assets, f"Bound icon absent from the native package: {key}"
    assert assets[key] == digest(lvrs / key), f"Native icon is stale: {key}"
print(f"Prepared LVRS style matches both token catalogs and {len(bindings['icons'])} Figma-bound icons.")
