#!/usr/bin/env python3
"""Prepare an ABI-compatible, workspace-local LVRS style bridge for Congregation."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
from concurrent.futures import ThreadPoolExecutor

PROJECT = Path(__file__).resolve().parents[2]
WORKSPACE = PROJECT.parents[1]
LVRS = WORKSPACE / "SDK/LVRS"
LVUI = WORKSPACE / "SDK/LVUI"
KIT = WORKSPACE / "SDK/build/congregation-web-account/prefix"
WORK = PROJECT / "build/lvrs-style-compat"
LEGACY_COMMIT = "818e24a"
LEGACY_LVRS_COMMIT = "c5131090"

sys.path.insert(0, str(Path.home() / ".local/SDK/iiFileProvider/share/iiFileProvider/python"))
import ii_file_provider as provider

THEME = r'''#include <LVRS/Theme.h>
#include "FigmaColors.h"
#include <array>
#include <cmath>
#include <stdexcept>
namespace lvrs {
namespace {
constexpr std::array<int,12> rows{0,4,5,1,6,3,2,-1,7,8,9,-1};
ii::gui::WidgetColor role(Accent accent, unsigned state) {
    const auto i = static_cast<unsigned>(accent);
    if (i >= rows.size() || state > 2) throw std::invalid_argument("Invalid accent");
    if (rows[i] >= 0) return figma_colors::accents[std::size_t(rows[i])][state];
    auto c = accent == Accent::gray ? ii::gui::WidgetColor{140./255,140./255,140./255,1}
                                    : ii::gui::WidgetColor{1,1,1,1};
    const auto factor = state == 1 ? 1.12 : state == 2 ? 1.2 : 1.;
    c.red /= factor; c.green /= factor; c.blue /= factor;
    return c;
}
}
ii::gui::WidgetColor Theme::accent_color(Accent accent) { return role(accent,0); }
ii::gui::WidgetAppearance Theme::appearance() const {
    auto result = ii::gui::lvrs_appearance();
    result.primary = role(accent_,0);
    result.hover = role(accent_,1);
    result.pressed = role(accent_,2);
    const auto base = figma_colors::backgrounds[19];
    const auto mix = [](double background,double accent) {
        return std::round((background*.98 + accent*.02)*255)/255;
    };
    result.window = {mix(base.red,result.primary.red),mix(base.green,result.primary.green),
                     mix(base.blue,result.primary.blue),1};
    return result;
}
}
'''

def run(*args):
    subprocess.run([str(arg) for arg in args], check=True, cwd=PROJECT)

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def verify_legacy_public_header(content: bytes, kit_header: Path) -> None:
    """Reject legacy source that differs from the native kit's public ABI."""
    if kit_header.is_file() and content != kit_header.read_bytes():
        raise ValueError("Legacy LVRS header does not match the selected native kit: " + str(kit_header))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, default=PROJECT / "resources/dependencies/lvrs-style-compat-v2")
    args = parser.parse_args()
    stage = args.stage.resolve()
    if not stage.is_relative_to((PROJECT / "resources").resolve()):
        raise SystemExit("Style package must be inside Congregation resources/")
    if stage.exists():
        raise SystemExit("Style package already exists; choose an unused --stage path")
    if not (PROJECT / "build/CMakeCache.txt").is_file():
        raise SystemExit("Configure Congregation's build/ before preparing the bridge")

    old_header = subprocess.check_output(["git", "-C", str(LVUI), "show",
                                          LEGACY_COMMIT + ":src/LVUI/StyleTokens.h"])
    if old_header != (KIT / "include/LVUI/StyleTokens.h").read_bytes():
        raise SystemExit("Legacy LVUI source does not match the selected native kit")
    archive = KIT / "lib/libLVRS.a"
    members = subprocess.check_output(["ar", "-t", str(archive)], text=True).splitlines()
    required = ["Theme.cpp.o", "NativeIcons.cpp.o", "NativeAlertAssets.cpp.o", "NativeCardAssets.cpp.o"]
    if any(members.count(member) != 1 for member in required):
        raise SystemExit("Native LVRS archive has an unexpected object inventory")

    provider.mkdir(stage / "include", parents=True, exist_ok=True)
    provider.mkdir(stage / "lib/cmake", parents=True, exist_ok=True)
    provider.copytree(KIT / "include/LVRS", stage / "include/LVRS", symlinks=True)
    provider.copytree(KIT / "lib/cmake/LVRS", stage / "lib/cmake/LVRS", symlinks=True)
    provider.copyfile(archive, stage / "lib/libLVRS.a", overwrite=False)
    provider.mkdir(stage / "source", parents=True, exist_ok=True)
    provider.mkdir(WORK, parents=True, exist_ok=True)

    def from_git(relative, target):
        data = subprocess.check_output(["git", "-C", str(LVUI), "show", LEGACY_COMMIT + ":" + relative])
        provider.write_bytes(target, data)

    from_git("src/LVUI/StyleTokens.cpp", stage / "source/StyleTokens.cpp")
    from_git("src/LVUI/StyleTokens.h", stage / "source/StyleTokens.h")
    from_git("src/lvui-compile.cpp", stage / "source/lvui-compile.cpp")
    provider.copyfile(LVUI / "src/Style/StyleTokens.generated.inc",
                      stage / "source/StyleTokens.generated.inc", overwrite=False)
    provider.copyfile(LVRS / "src/Styling/FigmaColors.h",
                      stage / "source/FigmaColors.h", overwrite=False)
    provider.write_text(stage / "source/Theme.cpp", THEME, encoding="utf-8")

    # Rebuild the matching legacy implementation, updating only colors that
    # correspond to the former shared Figma tokens. This keeps the older ABI
    # while avoiding stale defaults inside the native controls themselves.
    old_snapshot = json.loads(subprocess.check_output(
        ["git", "-C", str(LVUI), "show", "60fb2ec:catalog/lvui-style-tokens.json"]))
    current = json.loads((LVUI / "catalog/lvui-style-tokens.json").read_text(encoding="utf-8"))
    current_by_id = {item["id"]: item for item in current["variables"]}
    collections = {item["id"]: item["name"] for item in old_snapshot["collections"]}
    colors = []
    for item in old_snapshot["variables"]:
        if collections[item["collectionId"]] != "Theme Colors" or not item["name"].startswith(("Panel/", "Accent/")):
            continue
        old = next(iter(item["valuesByMode"].values()))
        new_item = current_by_id.get(item["id"])
        if not new_item or not isinstance(old, dict) or "r" not in old:
            continue
        new = next(iter(new_item["valuesByMode"].values()))
        colors.append((item["name"], [old[k] for k in "rgb"], [new[k] for k in "rgb"]))
    number = r"(?:\d+(?:\.\d*)?|\.\d+)(?:\s*/\s*\d+(?:\.\d*)?)?"
    quadruple = re.compile(r"\{\s*(" + number + r")\s*,\s*(" + number + r")\s*,\s*(" + number + r")\s*,\s*(" + number + r")\s*\}")
    def value(text):
        parts = text.split("/")
        return float(parts[0]) / float(parts[1]) if len(parts) == 2 else float(parts[0])
    replacements = [0]
    def update(text, name):
        if name == "MaterialDetail.cpp":
            old = "variant==MaterialVariant::window?gui::WidgetColor{11./255,11./255,11./255,1}:gui::WidgetColor{20./255,20./255,20./255,1}"
            window = next(row[2] for row in colors if row[0] == "Panel/window")
            if text.count(old) != 1:
                raise ValueError("Legacy material tint does not match the expected ABI source")
            text = text.replace(old, "gui::WidgetColor{" + ",".join(map(repr, window)) + ",1}")
            replacements[0] += 1
        def change(match):
            rgb = [value(x) for x in match.groups()[:3]]
            candidates = [row for row in colors if all(abs(rgb[i] - row[1][i]) < .000015 for i in range(3))]
            if not candidates:
                return match.group()
            defaults = [row for row in candidates if row[0].endswith("/Default")]
            chosen = defaults[0] if defaults else candidates[0]
            if len(candidates) > 1 and not defaults:
                raise ValueError("Ambiguous legacy color in " + name)
            replacements[0] += 1
            return "{" + ",".join(map(repr, chosen[2])) + "," + match.group(4) + "}"
        text = quadruple.sub(change, text)
        if name in {"Alert.cpp", "AlertButton.cpp", "ListFooter.cpp"}:
            surface = next(row[2][0] for row in colors if row[0] == "Panel/surfaceInteractive")
            text, count = re.subn(r"\.1411764\d*", repr(surface), text)
            replacements[0] += count
        return text

    source_dir = stage / "source/legacy-lvrs"
    provider.mkdir(source_dir, parents=True, exist_ok=True)
    archive_bytes = subprocess.check_output(["git", "-C", str(LVRS), "archive", LEGACY_LVRS_COMMIT, "src/LVRS"])
    sources = []
    with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:") as tree:
        for item in tree:
            if not item.isfile() or not item.name.endswith((".cpp", ".h", ".hpp")):
                continue
            name = Path(item.name).name
            raw = tree.extractfile(item).read()
            if name.endswith((".h", ".hpp")):
                verify_legacy_public_header(raw, KIT / "include/LVRS" / name)
            content = update(raw.decode("utf-8"), name)
            target = source_dir / name
            provider.write_text(target, content, encoding="utf-8")
            public_header = stage / "include/LVRS" / name
            if name.endswith((".h", ".hpp")) and public_header.is_file():
                provider.write_text(public_header, content, encoding="utf-8")
            if name.endswith(".cpp") and name != "Theme.cpp" and name + ".o" in members:
                sources.append(target)
    if replacements[0] < 40 or len(sources) < 40:
        raise SystemExit("Legacy LVRS source inventory is incomplete")

    cxx = "/usr/bin/c++"
    flags = ["-std=c++23", "-O2", "-arch", "arm64", "-mmacosx-version-min=12.0",
             "-I" + str(stage / "include"), "-I" + str(KIT / "include"),
             "-I" + str(stage / "source"), "-I" + str(source_dir)]
    objects = []
    for name in ("Theme",):
        obj = WORK / (name + ".cpp.o")
        run(cxx, *flags, "-c", stage / "source" / (name + ".cpp"), "-o", obj)
        objects.append(obj)
    def compile_legacy(source):
        obj = WORK / (source.name + ".o")
        run(cxx, *flags, "-c", source, "-o", obj)
        return obj
    with ThreadPoolExecutor(max_workers=4) as workers:
        objects.extend(workers.map(compile_legacy, sources))
    for name, generator in (("NativeIcons", "embed_native_icons.py"),
                            ("NativeAlertAssets", "embed_native_alert_assets.py"),
                            ("NativeCardAssets", "embed_native_card_assets.py")):
        generated = WORK / (name + ".cpp")
        run(sys.executable, "-B", LVRS / "tools" / generator, "--root", LVRS,
            "--output", generated, "--manifest", stage / (name + ".json"))
        obj = WORK / (name + ".cpp.o")
        run(cxx, *flags, "-c", generated, "-o", obj)
        objects.append(obj)
    run("ar", "-r", stage / "lib/libLVRS.a", *objects)
    run("ranlib", stage / "lib/libLVRS.a")

    token_obj, main_obj = WORK / "StyleTokens.cpp.o", WORK / "lvui-compile.cpp.o"
    run(cxx, *flags, "-c", stage / "source/StyleTokens.cpp", "-o", token_obj)
    run(cxx, *flags, "-c", stage / "source/lvui-compile.cpp", "-o", main_obj)
    compiler = WORK / "lvui-compile"
    run(cxx, "-arch", "arm64", "-mmacosx-version-min=12.0", main_obj, token_obj,
        KIT / "lib/libLVUI.a", "-o", compiler)

    manifest = {"kind": "congregation-lvrs-style-bridge", "legacy_lvui_commit": LEGACY_COMMIT,
                "native_kit": str(KIT), "figma_catalog_sha256": sha(LVRS / "resources/figma-color-tokens.json"),
                "lvui_catalog_sha256": sha(LVUI / "catalog/lvui-style-tokens.json"),
                "lvrs_original_archive_sha256": sha(archive), "lvrs_bridged_archive_sha256": sha(stage / "lib/libLVRS.a"),
                "compiler": str(compiler), "compiler_sha256": sha(compiler),
                "legacy_lvrs_commit": LEGACY_LVRS_COMMIT,
                "legacy_color_replacements": replacements[0],
                "recompiled_legacy_sources": len(sources),
                "replaced_objects": required + [source.name + ".o" for source in sources]}
    provider.write_text(stage / "manifest.json", json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))

if __name__ == "__main__":
    main()
