"""Reject Qt acquisition and deployment in Congregation's active build entrypoints."""
import argparse
from pathlib import Path
import re


def violations(root):
    paths = [root / name for name in ("CMakeLists.txt", "build.sh", "build-windows.ps1")]
    paths += list((root / "cmake").glob("*.cmake"))
    for suffix in ("*.yml", "*.yaml"):
        paths += list((root / ".github/workflows").glob(suffix))
    patterns = (r"install-qt-action|macdeployqt|windeployqt|QTKEYCHAIN_COMMIT",
                r"LVRS_BOOTSTRAP_QT|IIPAINTENGINE_WINDOWS_QT_PREFIX",
                r"\bfind_(?:package|dependency)\s*\(\s*Qt[56]?\b")
    found = []
    for path in paths:
        if path.is_file():
            for number, line in enumerate(path.read_text().splitlines(), 1):
                if any(re.search(pattern, line, re.I) for pattern in patterns):
                    found.append(str(path.relative_to(root)) + ":" + str(number))
    return found


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    found = violations(args.root)
    if found:
        raise SystemExit("Qt in active build entrypoints: " + ", ".join(found))
    print("NATIVE_BUILD_SURFACE_OK qt_acquisition=false qt_deployment=false")
