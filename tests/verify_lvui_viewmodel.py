"""Compile and exercise the real editor backend with an independent LVUI kit.

The existing app still uses the older LVRS renderer and its SDK kit. This host
check takes explicit, mutually compatible LVUI/iiGUI/Core packages instead of
mixing those headers or libraries with the app's legacy kit.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    for name in ("lvui", "iigui-source", "iigui-prefix", "core-prefix", "color-prefix", "iijson-source", "vulkan"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This host-link check currently supports macOS only")
    for name in ("lvui", "iigui_source", "iigui_prefix", "core_prefix", "color_prefix", "iijson_source", "vulkan"):
        setattr(args, name, getattr(args, name).resolve())
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    compiler = args.lvui / "build" / "lvui-compile"
    native_library = args.lvui / "build" / "libLVUINative.a"
    for path in (compiler, native_library, args.iigui_source / "iiGUI/WidgetState.h",
                 args.iigui_prefix / "lib/libiiGUI.a", args.core_prefix / "lib",
                 args.color_prefix / "include", args.iijson_source / "iiJson/Json.cpp", args.vulkan):
        if not path.exists():
            parser.error("Missing independent SDK input: " + str(path))
    environment = os.environ.copy()
    for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "CMAKE_PREFIX_PATH",
                "LIBRARY_PATH", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
        environment.pop(key, None)
    temporary = Path(tempfile.mkdtemp(prefix="lvui-viewmodel-check-", dir=build))
    environment["TMPDIR"] = str(temporary)
    try:
        for name, header in (("EditorViewModel", "Native/EditorBackend.h"),
                             ("CanvasSetupViewModel", "Native/CanvasSetupModel.h"),
                             ("TimelineViewModel", "Native/TimelineModel.h")):
            result = subprocess.run([str(compiler), "--name", name, "--include", header,
                                     str(root / "tests/fixtures" / (name + ".lvui"))],
                                    check=True, stdout=subprocess.PIPE, text=True, env=environment)
            (temporary / (name + ".generated.h")).write_text(result.stdout, encoding="utf-8")
            if re.search(r'\blvrs::|#\s*include\s*[<"](?:LVRS/|Qt)', result.stdout):
                raise RuntimeError("Independent LVUI output references a legacy UI dependency")
        catalogue = (root / "resources/canvas-presets.json").read_text(encoding="utf-8")
        if ')CATALOG"' in catalogue:
            raise RuntimeError("Canvas catalogue contains the generated-header delimiter")
        (temporary / "CanvasCatalogue.generated.h").write_text(
            '#pragma once\n#include <string_view>\nnamespace congregation::native::data { '
            'inline constexpr std::string_view canvas_catalogue = R"CATALOG('
            + catalogue + ')CATALOG"; }\n', encoding="utf-8")
        includes = [temporary, root / "App", args.lvui / "src", args.lvui / "build/lvui-generated-icons",
                    args.iigui_source, args.iigui_prefix / "include", args.core_prefix / "include",
                    args.color_prefix / "include", args.iijson_source]
        sources = [root / "tests/tst_lvui_editor_generated.cpp", root / "App/Native/EditorBackend.cpp",
                   root / "App/Native/PresentationList.cpp", root / "App/Native/ToolSettings.cpp",
                   root / "App/Native/TimelineModel.cpp", root / "App/Native/CanvasSetupModel.cpp",
                   args.iijson_source / "iiJson/Json.cpp", args.iijson_source / "iiJson/Value.cpp"]
        command = ["xcrun", "clang++", "-std=c++23", "-DIIGUI_STATIC_DEFINE"]
        for include in includes:
            command.extend(["-I", str(include)])
        command.extend(str(source) for source in sources)
        command.extend([str(native_library), str(args.iigui_prefix / "lib/libiiGUI.a"),
                        str(args.iigui_prefix / "lib/liblunasvg.a"),
                        str(args.iigui_prefix / "lib/libplutovg.a"),
                        str(args.core_prefix / "lib/libiiSDKCore.a"),
                        str(args.core_prefix / "lib/libiiSDKCoreObservation.dylib"), str(args.vulkan)])
        for framework in ("AppKit", "Foundation", "CoreText", "CoreFoundation", "CoreGraphics",
                          "Metal", "MetalPerformanceShaders", "QuartzCore"):
            command.extend(["-framework", framework])
        command.extend(["-Wl,-rpath," + str(args.core_prefix / "lib"),
                        "-Wl,-rpath," + str(args.vulkan.parent), "-o", str(temporary / "test")])
        subprocess.run(command, check=True, env=environment)
        libraries = subprocess.run(["otool", "-L", str(temporary / "test")], check=True,
                                   capture_output=True, text=True, env=environment).stdout
        symbols = subprocess.run(["nm", "-gU", str(temporary / "test")], check=True,
                                 capture_output=True, text=True, env=environment).stdout
        symbols = subprocess.run(["c++filt"], input=symbols, check=True,
                                 capture_output=True, text=True, env=environment).stdout
        if re.search(r'libLVRS|lvrs::|QtCore|QtGui|QtQml|QtQuick|QQml|QQuick|Qt::',
                     libraries + "\n" + symbols, re.IGNORECASE):
            raise RuntimeError("Independent view-model consumer links a legacy UI dependency")
        subprocess.run([str(temporary / "test")], check=True, env=environment)
    finally:
        shutil.rmtree(temporary)


if __name__ == "__main__":
    main()
