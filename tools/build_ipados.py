#!/usr/bin/env python3
"""Build and optionally install the canonical iPad app without duplicating .app bundles."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parents[1]
BUILD = ROOT / "build"
EVIDENCE = BUILD / "ipados"
PREFIX = WORKSPACE / "SDK/build/congregation-ipados/prefix"
HOST_COMPILER = WORKSPACE / "SDK/build/congregation-native/prefix/bin/lvui-compile"
BUNDLE = BUILD / "Congregation.app"
BUNDLE_ID = "com.iisacc.congregation.painter"


def run(arguments, name):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    log = EVIDENCE / (name + ".log")
    print(name, flush=True)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("DYLD_") and key not in
                   ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "LD_LIBRARY_PATH",
                    "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH")}
    if name == "configure":
        # CMake's compiler identification invokes xcodebuild before target
        # attributes exist. Apply the same setting to that probe as the app.
        shim = EVIDENCE / "xcode-tools/xcodebuild"
        shim.parent.mkdir(parents=True, exist_ok=True)
        shim.write_text('#!/bin/sh\nexec /usr/bin/xcodebuild "$@" SDK_STAT_CACHE_ENABLE=NO\n')
        shim.chmod(0o755)
        environment["PATH"] = str(shim.parent) + os.pathsep + environment["PATH"]
    with log.open("w") as output:
        result = subprocess.run([str(a) for a in arguments], cwd=ROOT,
                                env=environment, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(errors="replace")[-10000:])
        raise SystemExit(result.returncode)


def preserve_previous_platform():
    if not BUNDLE.exists():
        return
    info_path = BUNDLE / "Contents/Info.plist"
    if not info_path.exists():
        return  # The current iPad bundle is incrementally rebuilt in place.
    info = plistlib.loads(info_path.read_bytes())
    binary = BUNDLE / "Contents/MacOS" / info["CFBundleExecutable"]
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    archive = EVIDENCE / ("Congregation-macOS-" + digest[:12] + ".zip")
    if not archive.exists():
        partial = archive.with_suffix(".partial.zip")
        run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", BUNDLE, partial],
            "preserve-macos")
        partial.replace(archive)
    with zipfile.ZipFile(archive) as source:
        archived = source.read("Congregation.app/Contents/MacOS/" + info["CFBundleExecutable"])
    if hashlib.sha256(archived).hexdigest() != digest:
        raise SystemExit("macOS preservation archive failed verification")
    (EVIDENCE / "preserved-macos.json").write_text(json.dumps({
        "archive": str(archive), "executable_sha256": digest,
        "version": info.get("CFBundleShortVersionString"),
        "build": info.get("CFBundleVersion")}, indent=2) + "\n")
    cache = BUILD / "CMakeCache.txt"
    if cache.exists():
        shutil.copy2(cache, EVIDENCE / "macOS-CMakeCache.txt")
    shutil.rmtree(BUNDLE)


def build(team, device):
    required = [PREFIX / "lib/libiiGUI.a", PREFIX / "lib/libiiSharedCanvasNative.a",
                PREFIX / "lib/libiiNetwork.a", HOST_COMPILER]
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise SystemExit("Build the SDK first with tools/build_ipados_sdk.py all: " + ", ".join(missing))
    preserve_previous_platform()
    sysroot = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
    run(["cmake", "--fresh", "-S", ROOT, "-B", BUILD, "-G", "Xcode",
         "-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_SYSTEM_PROCESSOR=arm64",
         "-DCMAKE_OSX_SYSROOT=iphoneos", "-DCMAKE_OSX_ARCHITECTURES=arm64",
         "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0", "-DBUILD_TESTING=OFF",
         "-DCMAKE_XCODE_ATTRIBUTE_SDK_STAT_CACHE_ENABLE=NO",
         "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
         "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH", "-DCMAKE_FIND_FRAMEWORK=LAST",
         "-DCMAKE_FIND_ROOT_PATH=" + str(PREFIX),
         "-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON", "-DCONGREGATION_SDK_PREFIX=" + str(PREFIX),
         "-DLVUI_HOST_COMPILER=" + str(HOST_COMPILER), "-DCONGREGATION_APPLE_TEAM=" + team,
         "-DOPENSSL_ROOT_DIR=" + str(PREFIX), "-DOPENSSL_USE_STATIC_LIBS=ON",
         "-DOPENSSL_INCLUDE_DIR=" + str(PREFIX / "include"),
         "-DOPENSSL_CRYPTO_LIBRARY=" + str(PREFIX / "lib/libcrypto.a"),
         "-DOPENSSL_SSL_LIBRARY=" + str(PREFIX / "lib/libssl.a"),
         "-DCURL_LIBRARY=" + str(PREFIX / "lib/libcurl.a"), "-DCURL_INCLUDE_DIR=" + str(PREFIX / "include"),
         "-DVulkan_LIBRARY=" + str(PREFIX / "lib/libMoltenVK.a"), "-DVulkan_INCLUDE_DIR=" + str(PREFIX / "include"),
         "-DLIBXML2_LIBRARY=" + sysroot + "/usr/lib/libxml2.tbd",
         "-DLIBXML2_INCLUDE_DIR=" + sysroot + "/usr/include/libxml2",
         "-DZLIB_LIBRARY=" + sysroot + "/usr/lib/libz.tbd", "-DZLIB_INCLUDE_DIR=" + sysroot + "/usr/include"],
        "configure")
    run(["xcodebuild", "-project", BUILD / "Congregation.xcodeproj", "-target", "Congregation",
         "-configuration", "Release", "-sdk", "iphoneos", "-destination", "id=" + device,
         "-jobs", "4", "-allowProvisioningUpdates", "-allowProvisioningDeviceRegistration", "build",
         "SDK_STAT_CACHE_ENABLE=NO"], "build")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True, help="The exact connected iPad UDID")
    parser.add_argument("--team", default="5U49ST9XZH")
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--launch", action="store_true")
    args = parser.parse_args()
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with (EVIDENCE / "build.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit("Another iPad build owns the canonical application output")
        build(args.team, args.device)
        if args.install or args.launch:
            run(["xcrun", "devicectl", "device", "install", "app", "--device", args.device,
                 "--json-output", EVIDENCE / "install.json", BUNDLE], "install")
        if args.launch:
            run(["xcrun", "devicectl", "device", "process", "launch", "--device", args.device,
                 "--terminate-existing", "--json-output", EVIDENCE / "launch.json", BUNDLE_ID], "launch")


if __name__ == "__main__":
    main()
