"""Installer payload contracts without expanding a second application bundle."""
import hashlib
import importlib.util
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "packaging/macos"))
spec = importlib.util.spec_from_file_location("native_installer", ROOT / "packaging/macos/create_installer.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
from xcode_notarization import preserve_application
import verify_standalone_bundle as runtime_audit


def odc(name, data=b"", mode=stat.S_IFREG | 0o644):
    name = name.encode() + b"\0"
    fields = (0, 1, mode, 0, 0, 1, 0)
    return (b"070707" + b"".join(f"{n:06o}".encode() for n in fields)
            + f"{0:011o}{len(name):06o}{len(data):011o}".encode() + name + data)


class PayloadTests(unittest.TestCase):
    def test_protected_os_state_is_data_but_unreadable_external_code_fails(self):
        data = ["/private/var/db/analyticsd/events.allowlist",
                "/private/var/folders/test/0/com.apple.LaunchServices.dv/cache.csstore"]
        with patch.object(Path, "open", side_effect=PermissionError("protected")):
            for path in data:
                self.assertEqual(runtime_audit.images_from_lsof("n" + path), [])
            for path in ("/opt/homebrew/lib/foreign.dylib", "/private/var/db/foreign.dylib",
                         "/private/var/folders/test/foreign.dylib", "/Volumes/Storage/Workspace/SDK/foreign"):
                with self.assertRaises(RuntimeError):
                    runtime_audit.images_from_lsof("n" + path)

    def test_hardened_runtime_maps_require_all_bundled_images_and_reject_external_code(self):
        with tempfile.TemporaryDirectory(prefix="runtime-map-contract-", dir=Path(sys.argv[1])) as temp:
            root = Path(temp).resolve(); bundle = root / "fixture.bundle"; bundle.mkdir()
            executable = bundle / "Congregation"; executable.write_bytes(bytes.fromhex("cffaedfe") + b"native")
            library = bundle / "native.dylib"; library.write_bytes(bytes.fromhex("cffaedfe") + b"library")
            font = bundle / "font.ttf"; font.write_bytes(b"font data")
            foreign = root / "external.dylib"; foreign.write_bytes(bytes.fromhex("cffaedfe") + b"outside")
            output = f"p42\nftxt\nn{executable}\nftxt\nn{library}\nftxt\nn{font}\nftxt\nn{root / 'unlinked-plist-cache'}\n"
            images = runtime_audit.images_from_lsof(output)
            self.assertEqual(set(images), {str(executable), str(library)})
            runtime_audit.audit_images(images, bundle, [executable, library])
            with self.assertRaises(RuntimeError):
                runtime_audit.audit_images(images[:-1], bundle, [executable, library])
            with self.assertRaises(RuntimeError):
                runtime_audit.audit_images(images + [str(foreign)], bundle, [executable, library])

    def test_failed_xcode_operation_restores_signed_source(self):
        with tempfile.TemporaryDirectory(prefix="xcode-contract-", dir=Path(sys.argv[1])) as temp:
            root = Path(temp); app = root / "fixture.bundle"; work = root / "work"
            app.mkdir(); work.mkdir()
            (app / "executable").write_bytes(b"signed-original")
            (app / "executable").chmod(0o755)
            with self.assertRaisesRegex(RuntimeError, "network failed"):
                with preserve_application(app, work):
                    (app / "executable").write_bytes(b"temporary-signed-transport")
                    (app / "installer.pkg").write_bytes(b"temporary payload")
                    raise RuntimeError("network failed")
            self.assertEqual((app / "executable").read_bytes(), b"signed-original")
            self.assertFalse((app / "installer.pkg").exists())
            self.assertTrue((app / "executable").stat().st_mode & 0o111)

    def test_upgrade_policy_and_identity_guards(self):
        def info():
            return ET.fromstring('<pkg-info identifier="com.iisacc.congregation.painter.pkg" version="1.0.0" install-location="/Applications" relocatable="true">'
                                 '<bundle id="com.iisacc.congregation.painter" CFBundleVersion="1" CFBundleShortVersionString="1.0.0" path="./Congregation.app"/>'
                                 '<relocate><bundle id="com.iisacc.congregation.painter"/></relocate><update-bundle><bundle id="com.iisacc.congregation.painter"/></update-bundle></pkg-info>')
        xml = info()
        installer.configure_component_info(xml, "1.0.0", "1")
        self.assertEqual(xml.get("relocatable"), "false")
        self.assertEqual(len(xml.find("relocate")), 0)
        self.assertEqual(len(xml.find("update-bundle")), 0)
        self.assertEqual(xml.find("upgrade-bundle/bundle").get("id"), installer.IDENTIFIER)
        for attribute, value in (("identifier", "foreign"), ("version", "0.9"), ("install-location", "/tmp")):
            invalid = info(); invalid.set(attribute, value)
            with self.assertRaises(ValueError):
                installer.configure_component_info(invalid, "1.0.0", "1")

    def test_odc_regular_file_and_symlink(self):
        data = (odc("./Congregation.app/Contents/MacOS/Congregation", b"native")
                + odc("Congregation.app/current", b"Contents", stat.S_IFLNK | 0o755)
                + odc("TRAILER!!!"))
        entries = installer.read_cpio(data)
        self.assertEqual(entries[0][0], "Congregation.app/Contents/MacOS/Congregation")
        self.assertEqual(entries[0][2], b"native")
        self.assertTrue(stat.S_ISLNK(entries[1][1]))

    def test_rejects_truncated_archive_and_duplicate_names(self):
        for data in (odc("a", b"abc")[:-1], odc("a") + odc("a") + odc("TRAILER!!!")):
            with self.assertRaises(ValueError):
                installer.read_cpio(data)

    def test_rejects_escaping_paths(self):
        for name in ("../outside", "/Applications/other", "Congregation.app/../other"):
            with self.assertRaises(ValueError):
                installer.read_cpio(odc(name) + odc("TRAILER!!!"))

    def test_newc_alignment(self):
        def entry(name, content):
            name = name.encode() + b"\0"
            values = (1, stat.S_IFREG | 0o644, 0, 0, 1, 0, len(content), 0, 0, 0, 0, len(name), 0)
            block = b"070701" + b"".join(f"{v:08x}".encode() for v in values) + name
            block += b"\0" * (-len(block) % 4)
            return block + content + b"\0" * (-len(content) % 4)
        entries = installer.read_cpio(entry("a", b"123") + entry("TRAILER!!!", b""))
        self.assertEqual(entries, [("a", stat.S_IFREG | 0o644, b"123")])

    def test_appledouble_metadata_rejects_quarantine_and_invalid_targets(self):
        with tempfile.TemporaryDirectory(prefix="installer-metadata-", dir=Path(sys.argv[1])) as temp:
            app = Path(temp)
            metadata = b"\x00\x05\x16\x07\x00\x02\x00\x00" + bytes(18)
            entry = ("._Congregation.app", stat.S_IFREG | 0o644, metadata)
            self.assertEqual(installer.verify_payload([entry], app)["appledouble_metadata"], 1)
            for name, content in (("._Congregation.app", metadata + b"com.apple.quarantine"),
                                  ("._Other.app", metadata), ("._Congregation.app", b"not metadata")):
                with self.assertRaises(ValueError):
                    installer.verify_payload([(name, entry[1], content)], app)

    def test_payload_matches_signed_source_and_rejects_stale_or_extra_files(self):
        with tempfile.TemporaryDirectory(prefix="installer-contract-", dir=Path(sys.argv[1])) as temp:
            app = Path(temp) / "fixture"
            (app / "Contents/MacOS").mkdir(parents=True)
            (app / "Contents/MacOS/Congregation").write_bytes(b"signed-native")
            good = [("Congregation.app/Contents/MacOS/Congregation", stat.S_IFREG | 0o644, b"signed-native")]
            result = installer.verify_payload(good, app)
            self.assertEqual(result["files"], 1)
            self.assertEqual(result["sha256"]["Contents/MacOS/Congregation"], hashlib.sha256(b"signed-native").hexdigest())
            for bad in ([], [(good[0][0], good[0][1], b"stale")],
                        good + [("Congregation.app/Contents/Frameworks/QtCore.framework/QtCore", stat.S_IFREG | 0o644, b"Qt")],
                        good + [("Other.app/file", stat.S_IFREG | 0o644, b"other")]):
                with self.assertRaises(ValueError):
                    installer.verify_payload(bad, app)
            (app / "Contents/MacOS/Congregation").chmod(0o755)
            with self.assertRaises(ValueError):
                installer.verify_payload(good, app)
            executable = [(good[0][0], stat.S_IFREG | 0o755, good[0][2])]
            self.assertEqual(installer.verify_payload(executable, app)["files"], 1)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
