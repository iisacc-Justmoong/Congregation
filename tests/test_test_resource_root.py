from pathlib import Path
import os
import subprocess
import sys
import tempfile
import uuid

root, owned, source = (Path(value).resolve(strict=True) for value in sys.argv[1:4])
cmake = sys.argv[4]
assert owned != root and owned.is_relative_to(root) and owned.is_dir()
for key in ("TMPDIR", "TMP", "TEMP"):
    assert Path(os.environ[key]).resolve(strict=True) == owned
assert Path(tempfile.gettempdir()).resolve(strict=True) == owned
with tempfile.TemporaryDirectory(prefix=".resource-contract-", dir=owned) as work:
    work = Path(work)
    path = work / "probe"
    path.write_text("test-resource-contract", encoding="utf-8")
    assert path.read_text(encoding="utf-8") == "test-resource-contract"
    module = source / "cmake/NativeTestResources.cmake"
    driver = source / "tests/test_resource_resolution.cmake"
    def resolve(requested):
        return subprocess.run([cmake, f"-DMODULE={module}", f"-DRESOURCE_ROOT={root}",
            f"-DREQUESTED={requested}", f"-DBASE_DIRECTORY={source}", "-P", str(driver)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    safe = work / "safe"
    result = resolve(safe)
    assert result.returncode == 0 and safe.is_dir(), result.stdout
    def optional_link(link):
        return subprocess.run([cmake, f"-DMODULE={module}", f"-DRESOURCE_ROOT={safe}",
            f"-DLINK_PATH={link}", "-P", str(driver)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    unavailable = work / "no-such-parent" / "link"
    result = optional_link(unavailable)
    assert result.returncode == 0 and not unavailable.parent.exists(), result.stdout
    preserved = work / "existing-file"
    preserved.write_text("keep", encoding="utf-8")
    result = optional_link(preserved)
    assert result.returncode == 0 and preserved.read_text(encoding="utf-8") == "keep", result.stdout
    missing = work / "missing-parent" / "child"
    result = resolve(missing)
    assert result.returncode != 0 and not missing.parent.exists(), result.stdout
    outside = source / "build" / ("resource-outside-" + uuid.uuid4().hex)
    result = resolve(outside)
    assert result.returncode != 0 and not outside.exists(), result.stdout
    link = work / "outside-link"
    try:
        link.symlink_to(source / "build", target_is_directory=True)
    except OSError:
        # Creating symlinks is not available to every Windows user account.
        pass
    else:
        escaped = link / outside.name
        result = resolve(escaped)
        assert result.returncode != 0 and not outside.exists(), result.stdout
        result = resolve(link)
        assert result.returncode != 0, result.stdout
print("TEST_RESOURCE_ROOT_OK")
