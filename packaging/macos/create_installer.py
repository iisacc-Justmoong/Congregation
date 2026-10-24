"""Sign the canonical native app and build a verified macOS installer in build/."""
import argparse
import fcntl
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import stat
import struct
import tempfile
import xml.etree.ElementTree as ET

from installer_support import APP, BUILD, IDENTIFIER, run
MACHO = {bytes.fromhex(value) for value in
         ("feedface", "feedfacf", "cefaedfe", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}


def read_cpio(data):
    """Read Apple's odc/newc payload in memory; never expand another .app."""
    offset, entries, seen = 0, [], set()
    while offset < len(data):
        magic = data[offset:offset + 6]
        if magic == b"070707":
            header = data[offset:offset + 76]
            if len(header) != 76:
                raise ValueError("Truncated odc header")
            mode = int(header[18:24], 8)
            namesize, size = int(header[59:65], 8), int(header[65:76], 8)
            offset += 76
            align = False
        elif magic in (b"070701", b"070702"):
            header = data[offset:offset + 110]
            if len(header) != 110:
                raise ValueError("Truncated newc header")
            mode, size, namesize = int(header[14:22], 16), int(header[54:62], 16), int(header[94:102], 16)
            offset += 110
            align = True
        else:
            raise ValueError("Unsupported or truncated cpio payload")
        raw = data[offset:offset + namesize]
        if not raw or len(raw) != namesize or raw[-1] != 0:
            raise ValueError("Truncated cpio name")
        offset += namesize
        if align:
            offset += -offset % 4
        content = data[offset:offset + size]
        if len(content) != size:
            raise ValueError("Truncated cpio file")
        offset += size
        if align:
            offset += -offset % 4
        name = raw[:-1].decode("utf-8")
        if name == "TRAILER!!!":
            return entries
        if name.startswith("/") or ".." in PurePosixPath(name).parts:
            raise ValueError("Payload path escapes the application")
        name = str(PurePosixPath(name))
        if name in seen:
            raise ValueError("Duplicate payload path: " + name)
        seen.add(name)
        entries.append((name, mode, content))
    raise ValueError("Missing cpio trailer")


def verify_payload(entries, app):
    expected = {}
    for path in app.rglob("*"):
        if path.is_symlink():
            expected[str(path.relative_to(app))] = ("link", os.readlink(path))
        elif path.is_file():
            expected[str(path.relative_to(app))] = ("file", hashlib.sha256(path.read_bytes()).hexdigest())
    actual, metadata = {}, 0
    for name, mode, content in entries:
        if stat.S_ISDIR(mode):
            continue
        path = PurePosixPath(name)
        if path.name.startswith("._"):
            target = path.with_name(path.name[2:])
            if str(target) == "Congregation.app":
                source = app
            elif str(target).startswith("Congregation.app/"):
                source = app / str(target).removeprefix("Congregation.app/")
            else:
                raise ValueError("Unexpected metadata target")
            if (not source.exists() or len(content) < 26
                    or content[:8] != b"\x00\x05\x16\x07\x00\x02\x00\x00"
                    or b"com.apple.quarantine" in content):
                raise ValueError("Invalid or quarantined AppleDouble metadata")
            count = struct.unpack_from(">H", content, 24)[0]
            if 26 + 12 * count > len(content):
                raise ValueError("Truncated AppleDouble metadata")
            for index in range(count):
                _, start, length = struct.unpack_from(">III", content, 26 + 12 * index)
                if start < 26 + 12 * count or start + length > len(content):
                    raise ValueError("Invalid AppleDouble entry")
            metadata += 1
            continue
        if not name.startswith("Congregation.app/"):
            raise ValueError("Unexpected installed payload: " + name)
        relative = name.removeprefix("Congregation.app/")
        if stat.S_ISLNK(mode):
            actual[relative] = ("link", content.decode("utf-8"))
        elif stat.S_ISREG(mode):
            actual[relative] = ("file", hashlib.sha256(content).hexdigest())
            source = app / relative
            if source.is_file() and bool(mode & 0o111) != bool(source.stat().st_mode & 0o111):
                raise ValueError("Executable mode differs: " + relative)
        else:
            raise ValueError("Unexpected payload file type: " + name)
    if actual != expected:
        differing = [name for name in actual.keys() | expected.keys() if actual.get(name) != expected.get(name)]
        raise ValueError("Installer differs from signed source: " + repr(sorted(differing)[:12]))
    return {"files": len(actual), "appledouble_metadata": metadata, "quarantine": False,
            "sha256": {name: value[1] for name, value in actual.items() if value[0] == "file"}}


def check_native_bundle():
    binaries = []
    for path in APP.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            if stream.read(4) not in MACHO:
                continue
        for line in run("otool", "-L", path).splitlines():
            if not line.startswith("\t"):
                continue
            dependency = line.strip().split(" (", 1)[0]
            if "Qt" in dependency:
                raise ValueError("Qt dependency in installer source: " + dependency)
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            if not dependency.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
                raise ValueError("Non-system absolute runtime dependency: " + dependency)
            if not (APP / "Contents/Frameworks" / Path(dependency).name).is_file():
                raise ValueError("Missing bundled runtime: " + dependency)
        binaries.append(path)
    if not binaries:
        raise ValueError("No native executables found")
    return binaries


def configure_component_info(xml, version, bundle_version):
    """Set installation policy in compressed component metadata, without staging an app."""
    primary = xml.find("bundle")
    if (xml.get("identifier") != IDENTIFIER + ".pkg" or xml.get("version") != version
            or xml.get("install-location") != "/Applications" or primary is None
            or primary.get("id") != IDENTIFIER or primary.get("CFBundleVersion") != bundle_version
            or primary.get("CFBundleShortVersionString") != version
            or primary.get("path") not in ("./Congregation.app", "Congregation.app")):
        raise ValueError("Unexpected installer identifier, version or destination")
    xml.set("relocatable", "false")
    for name in ("bundle-version", "upgrade-bundle", "strict-identifier"):
        node = xml.find(name)
        if node is None:
            node = ET.SubElement(xml, name)
        node.clear()
        ET.SubElement(node, "bundle", {"id": IDENTIFIER})
    for name in ("relocate", "update-bundle", "atomic-update-bundle"):
        node = xml.find(name)
        if node is not None:
            node.clear()


def build(app_identity, installer_identity, notary_profile=None, xcode=False):
    if APP.is_symlink() or not APP.is_dir():
        raise ValueError("Use the canonical build/Congregation.app")
    info = plistlib.loads((APP / "Contents/Info.plist").read_bytes())
    if info.get("CFBundleIdentifier") != IDENTIFIER:
        raise ValueError("Unexpected application identifier")
    version, bundle_version = info["CFBundleShortVersionString"], info["CFBundleVersion"]
    binaries = check_native_bundle()
    executable = APP / "Contents/MacOS/Congregation"
    arch = run("lipo", "-archs", executable).strip().split()
    # Imported build resources must not transfer their download quarantine to users.
    # Keep protected provenance and all source/SDK files untouched.
    run("xattr", "-dr", "com.apple.quarantine", APP)
    for binary in binaries:
        if binary != executable:
            run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", app_identity, binary)
    run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", app_identity, APP)
    run("codesign", "--verify", "--deep", "--strict", APP)
    details = run("codesign", "--display", "--verbose=4", APP)
    if "Authority=Developer ID Application:" not in details or "runtime" not in details or "Timestamp=" not in details:
        raise ValueError("Application needs Developer ID, hardened runtime and trusted timestamp")
    work = BUILD / "installer"
    work.mkdir(exist_ok=True)
    (work / "application-signature.txt").write_text(details)
    with tempfile.TemporaryDirectory(prefix="package-", dir=work) as temporary:
        temp = Path(temporary)
        component = temp / "Congregation-component.pkg"
        print("Creating signed-application component", flush=True)
        run("pkgbuild", "--component", APP, "--install-location", "/Applications", "--identifier", IDENTIFIER + ".pkg",
            "--version", version, "--compression", "legacy", component)
        expanded = temp / "component"
        run("pkgutil", "--expand", component, expanded)
        package_info = ET.parse(expanded / "PackageInfo").getroot()
        configure_component_info(package_info, version, bundle_version)
        ET.ElementTree(package_info).write(expanded / "PackageInfo", encoding="utf-8", xml_declaration=True)
        component = temp / "Congregation-install.pkg"
        run("pkgutil", "--flatten", expanded, component)
        (work / "PackageInfo.xml").write_bytes(ET.tostring(package_info, encoding="utf-8", xml_declaration=True))
        requirements = temp / "requirements.plist"
        requirements.write_bytes(plistlib.dumps({"os": [info.get("LSMinimumSystemVersion", "12.0")], "arch": arch}))
        distribution = temp / "Distribution.xml"
        run("productbuild", "--synthesize", "--product", requirements, "--package", component, distribution)
        tree = ET.parse(distribution); xml = tree.getroot()
        title = xml.find("title")
        if title is None:
            title = ET.SubElement(xml, "title")
        title.text = "Congregation"
        ET.SubElement(xml, "product", {"id": IDENTIFIER, "version": version})
        ET.SubElement(xml, "domains", {"enable_anywhere": "false", "enable_currentUserHome": "false", "enable_localSystem": "true"})
        tree.write(distribution, encoding="utf-8", xml_declaration=True)
        (work / "Distribution.xml").write_bytes(distribution.read_bytes())
        candidate = temp / "Congregation.pkg"
        print("Signing installer package", flush=True)
        run("productbuild", "--distribution", distribution, "--package-path", temp,
            "--sign", installer_identity, "--timestamp", candidate)
        signature = run("pkgutil", "--check-signature", candidate)
        if "Developer ID Installer:" not in signature:
            raise ValueError("Installer is not signed for direct distribution")
        (work / "package-signature.txt").write_text(signature)
        final_expanded = temp / "final-package"
        run("pkgutil", "--expand", candidate, final_expanded)
        payloads = list(final_expanded.rglob("Payload"))
        if len(payloads) != 1:
            raise ValueError("Installer must contain exactly one application payload")
        payload = read_cpio(gzip.decompress(payloads[0].read_bytes()))
        verified = verify_payload(payload, APP)
        notarization = {"status": "not_submitted"}
        if xcode:
            from xcode_notarization import notarize
            notarization = notarize(candidate, APP, work / "xcode", app_identity, installer_identity)
        elif notary_profile:
            print("Submitting installer to Apple notarization", flush=True)
            response = run("xcrun", "notarytool", "submit", candidate, "--keychain-profile", notary_profile,
                           "--wait", "--timeout", "10m", "--output-format", "json")
            (work / "notarization.json").write_text(response)
            notarization = json.loads(response)
            if notarization.get("status") != "Accepted":
                raise ValueError("Apple did not accept installer notarization")
            run("xcrun", "stapler", "staple", candidate)
            run("xcrun", "stapler", "validate", candidate)
            run("spctl", "--assess", "--type", "install", "--verbose=4", candidate)
        after = plistlib.loads((APP / "Contents/Info.plist").read_bytes())
        if (after["CFBundleShortVersionString"], after["CFBundleVersion"]) != (version, bundle_version):
            raise ValueError("Application version changed during packaging")
        # Last gate also detects an app rebuild while pkgbuild was reading it.
        verify_payload(payload, APP)
        output = BUILD / "Congregation.pkg"
        os.replace(candidate, output)
        digest = hashlib.sha256(output.read_bytes()).hexdigest()
        (BUILD / "Congregation.pkg.sha256").write_text(digest + "  Congregation.pkg\n")
        report = {"package": str(output), "bytes": output.stat().st_size, "sha256": digest,
                  "application": str(APP), "version": version, "bundle_version": bundle_version,
                  "install_location": "/Applications/Congregation.app", "architectures": arch,
                  "minimum_macos": info.get("LSMinimumSystemVersion"), "qt": False,
                  "application_signature": "Developer ID Application, hardened runtime, timestamp",
                  "installer_signature": "Developer ID Installer, timestamp", "payload": verified,
                  "notarization": notarization, "gatekeeper_ready": notarization.get("status") == "Accepted"}
        (work / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
        print("CONGREGATION_INSTALLER_OK " + str(output), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-identity", default="Developer ID Application: MUYEONG YUN (5U49ST9XZH)")
    parser.add_argument("--installer-identity", default="Developer ID Installer: MUYEONG YUN (5U49ST9XZH)")
    parser.add_argument("--notary-profile")
    parser.add_argument("--xcode", action="store_true", help="Notarize using Xcode's signed-in account")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    with (BUILD / "native-installer.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.xcode and args.notary_profile:
            parser.error("Choose Xcode or a notarytool profile")
        build(args.app_identity, args.installer_identity, args.notary_profile, args.xcode)


if __name__ == "__main__":
    main()
