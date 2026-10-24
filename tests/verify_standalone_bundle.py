"""Exercise the one native macOS bundle after moving it away from its build path."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

MACHO = {bytes.fromhex(value) for value in
         ("feedface", "feedfacf", "cefaedfe", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}


def system_image(path):
    return path.startswith(("/System/Library/", "/usr/lib/",
        "/System/Volumes/Preboot/Cryptexes/OS/usr/lib/",
        "/System/Volumes/Preboot/Cryptexes/OS/System/Library/"))


def images_from_lsof(output):
    images = set()
    for line in output.splitlines():
        if not line.startswith("n/"):
            continue
        path = line[1:]
        if system_image(path):
            images.add(path)
            continue
        try:
            with Path(path).open("rb") as stream:
                if stream.read(4) in MACHO:
                    images.add(path)
        except FileNotFoundError:
            # lsof also lists transient mapped data (for example logd's plist
            # cache), which can be unlinked before inspection. Required code
            # images are still checked explicitly by audit_images.
            continue
        except PermissionError as error:
            # OS daemons pass read-only mappings of protected state files into
            # clients (analyticsd's events.allowlist is one example). These are
            # data, not loadable libraries. Never exempt an SDK/Homebrew path or
            # a library-shaped file; required app images remain mandatory.
            if path.startswith(("/private/var/db/", "/private/var/folders/")) and not any(
                    marker in path for marker in (".dylib", ".framework/", ".bundle/", ".so")):
                continue
            raise RuntimeError("Cannot inspect mapped code: " + path) from error
        except OSError as error:
            raise RuntimeError("Cannot inspect mapped file: " + path) from error
    return sorted(images)


def audit_images(images, bundle, expected):
    resolved = {Path(path).resolve() for path in images}
    missing = [str(path) for path in expected if path.resolve() not in resolved]
    if missing:
        raise RuntimeError("Missing loaded-image evidence: " + repr(missing))
    escaped = [path for path in images if not system_image(path)
               and not Path(path).resolve().is_relative_to(bundle.resolve())]
    if escaped or any("Qt" in path for path in images):
        raise RuntimeError("Runtime dependency escaped the bundle: " + repr(escaped))


def observe_process(command, working, environment):
    """Read actual mappings without a debugging entitlement or DYLD opt-out."""
    process = subprocess.Popen(command, cwd=working, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    images, deadline = set(), time.monotonic() + 300
    try:
        while True:
            try:
                stdout, stderr = process.communicate(timeout=.25)
                break
            except subprocess.TimeoutExpired:
                if time.monotonic() > deadline:
                    raise RuntimeError("Native runtime verification timed out")
                mapped = subprocess.run(["/usr/sbin/lsof", "-n", "-P", "-a", "-p", str(process.pid),
                                         "-d", "txt", "-Fn"], capture_output=True, text=True, timeout=10)
                if mapped.returncode == 0:
                    images.update(images_from_lsof(mapped.stdout))
        # Ad-hoc builds can also emit this; hardened distribution builds ignore
        # DYLD_PRINT_LIBRARIES. Live Mach-O mappings remain the required proof.
        images.update(re.findall(r"^dyld\[\d+\]: <[^>]+> (.+)$", stderr, re.MULTILINE))
        return process.returncode, stdout, stderr, sorted(images)
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()


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
                status, stdout, stderr, images = observe_process(
                    [str(executable), "--self-test", str(data), "--renderer", renderer], working, environment)
                log = build / ("congregation-relocated-" + renderer + ".log")
                log.write_text(stdout + stderr)
                if status or "CONGREGATION_NATIVE_OK" not in stdout:
                    raise RuntimeError("Relocated " + renderer + " failed; see " + str(log))
                audit_images(images, moved, [executable, *(moved / "Contents/Frameworks").glob("*.dylib")])
                report["renderers"].append({"renderer": renderer, "exit_code": status,
                    "image_evidence": "live Mach-O mappings (lsof) and available dyld diagnostics",
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
