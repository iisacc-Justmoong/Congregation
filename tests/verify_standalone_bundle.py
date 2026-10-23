"""Exercise the one native macOS bundle after moving it away from its build path."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile


def system_image(path):
    return path.startswith(("/System/Library/", "/usr/lib/",
        "/System/Volumes/Preboot/Cryptexes/OS/usr/lib/",
        "/System/Volumes/Preboot/Cryptexes/OS/System/Library/"))


def interrupted(number, frame):
    raise RuntimeError("Standalone verification interrupted: " + str(number))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    bundle, build = args.bundle.resolve(), args.build.resolve()
    if bundle.parent != build or bundle.name != "Congregation.app":
        raise RuntimeError("Verify the canonical build/Congregation.app bundle")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    report = {"canonical_bundle": str(bundle), "renderers": []}
    with tempfile.TemporaryDirectory(prefix="standalone-relocation-", dir=build) as temporary:
        root = Path(temporary)
        moved = root / bundle.name
        working = root / "empty-working-directory"
        working.mkdir()
        data = root / "isolated-data"
        data.mkdir()
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(("DYLD_", "LD_"))
                       and key not in {"CPATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"}}
        environment.update(TMPDIR=str(data), TMP=str(data), TEMP=str(data), DYLD_PRINT_LIBRARIES="1")
        bundle.rename(moved)
        try:
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(moved)], check=True)
            executable = moved / "Contents/MacOS/Congregation"
            for renderer in ("native", "vulkan"):
                result = subprocess.run([str(executable), "--self-test", str(data), "--renderer", renderer],
                                        cwd=working, env=environment, capture_output=True, text=True, timeout=300)
                log = build / ("congregation-relocated-" + renderer + ".log")
                log.write_text(result.stdout + result.stderr)
                if result.returncode or "CONGREGATION_NATIVE_OK" not in result.stdout:
                    raise RuntimeError("Relocated " + renderer + " failed; see " + str(log))
                images = re.findall(r"^dyld\[\d+\]: <[^>]+> (.+)$", result.stderr, re.MULTILINE)
                if not images or not any(str(executable) == path for path in images):
                    raise RuntimeError("Missing loaded-image evidence for " + renderer)
                escaped = [path for path in images if not system_image(path)
                           and not Path(path).resolve().is_relative_to(moved)]
                if escaped or any("Qt" in path for path in images):
                    raise RuntimeError("Runtime dependency escaped the bundle: " + repr(escaped))
                report["renderers"].append({"renderer": renderer, "exit_code": result.returncode,
                    "log": str(log), "bundle_images": [str(Path(path).relative_to(moved))
                        for path in images if Path(path).is_relative_to(moved)], "external_images": []})
        finally:
            if not bundle.exists() and moved.exists():
                moved.rename(bundle)
        report["canonical_restored"] = bundle.is_dir()
    (build / "native-relocation-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print("NATIVE_RELOCATION_OK native=true vulkan=true external_sdk=false qt=false canonical_restored=true")


if __name__ == "__main__":
    main()
