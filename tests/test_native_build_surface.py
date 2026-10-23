"""Contract cases for active build boundaries, using only build-local fixtures."""
from pathlib import Path
import sys
import tempfile
from verify_native_build_surface import violations


with tempfile.TemporaryDirectory(prefix="build-surface-", dir=sys.argv[1]) as temporary:
    root = Path(temporary)
    (root / "CMakeLists.txt").write_text("set(CMAKE_DISABLE_FIND_PACKAGE_Qt6 ON)\n")
    legacy = root / "docs/legacy"
    legacy.mkdir(parents=True)
    (legacy / "old.yml").write_text("uses: jurplel/install-qt-action@old\n")
    assert not violations(root), "Disabled Qt and historical records were rejected"
    workflows = root / ".github/workflows"
    workflows.mkdir(parents=True)
    (workflows / "bad.yml").write_text("uses: jurplel/install-qt-action@old\n")
    assert violations(root) == [".github/workflows/bad.yml:1"]
    (workflows / "bad.yml").unlink()
    for value in ("find_package(Qt6 REQUIRED)", "find_dependency(Qt5)", "macdeployqt App.app",
                  "$env:LVRS_BOOTSTRAP_QT_PREFIX_WINDOWS = 'legacy'"):
        (root / "build.sh").write_text(value + "\n")
        assert violations(root) == ["build.sh:1"], value
print("NATIVE_BUILD_SURFACE_CONTRACT_OK")
