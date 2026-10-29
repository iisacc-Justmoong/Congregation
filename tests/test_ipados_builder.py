"""Preservation and incremental-build contracts without a device or signing secrets."""
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ipados_builder", ROOT / "tools/build_ipados.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class Preservation(unittest.TestCase):
    def setUp(self):
        temporary = ROOT / "build/ipados-contracts"
        temporary.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=temporary)
        self.root = Path(self.temporary.name)
        # Fixtures are ordinary directories, never additional application bundles.
        self.bundle = self.root / "previous-bundle"
        self.evidence = self.root / "evidence"
        self.evidence.mkdir()
        self.payload = b"verified macOS executable fixture"
        self.digest = hashlib.sha256(self.payload).hexdigest()
        self.archive = self.evidence / ("Congregation-macOS-" + self.digest[:12] + ".zip")

    def tearDown(self):
        self.temporary.cleanup()

    def source(self):
        (self.bundle / "Contents/MacOS").mkdir(parents=True)
        (self.bundle / "Contents/MacOS/Congregation").write_bytes(self.payload)
        (self.bundle / "Contents/Info.plist").write_bytes(plistlib.dumps({
            "CFBundleExecutable": "Congregation", "CFBundleShortVersionString": "1.0.0",
            "CFBundleVersion": "1"}))

    def archive_payload(self, payload):
        with zipfile.ZipFile(self.archive, "w") as output:
            output.writestr("Congregation.app/Contents/MacOS/Congregation", payload)

    def preserve(self):
        with patch.multiple(builder, BUNDLE=self.bundle, EVIDENCE=self.evidence, BUILD=self.root):
            builder.preserve_previous_platform()

    def test_verified_archive_allows_platform_switch_and_records_version(self):
        self.source()
        self.archive_payload(self.payload)
        self.preserve()
        self.assertFalse(self.bundle.exists())
        record = json.loads((self.evidence / "preserved-macos.json").read_text())
        self.assertEqual(record["executable_sha256"], self.digest)
        self.assertEqual((record["version"], record["build"]), ("1.0.0", "1"))

    def test_tampered_archive_preserves_original(self):
        self.source()
        self.archive_payload(b"different binary")
        with self.assertRaises(SystemExit):
            self.preserve()
        self.assertEqual((self.bundle / "Contents/MacOS/Congregation").read_bytes(), self.payload)

    def test_ipad_incremental_build_keeps_bundle(self):
        self.bundle.mkdir()
        (self.bundle / "Info.plist").write_bytes(b"iPad fixture")
        self.preserve()
        self.assertTrue(self.bundle.exists())
        self.assertFalse(self.archive.exists())


class CryptoPackage(unittest.TestCase):
    def test_clean_sdk_without_license_manager_cache_uses_vendor_fetch(self):
        spec = importlib.util.spec_from_file_location("ipados_sdk_cold", ROOT / "tools/build_ipados_sdk.py")
        sdk = importlib.util.module_from_spec(spec)
        with patch("subprocess.check_output", return_value="/fixture/sdk\n"):
            spec.loader.exec_module(sdk)
        temporary = ROOT / "build/ipados-contracts"
        temporary.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary) as directory:
            root = Path(directory)
            prefix = root / "prefix"
            (prefix / "lib").mkdir(parents=True)
            (prefix / "lib/libssl.a").touch()
            (prefix / "lib/libsodium.a").touch()
            with patch.multiple(sdk, PREFIX=prefix, SDK=root / "missing-sdk", VENDOR=root / "vendor"), \
                    patch.object(sdk, "archive", return_value=root / "source.tar.gz") as archive, \
                    patch.object(sdk, "unpack", return_value=root / "sources"), \
                    patch.object(sdk, "cmake"), patch.object(sdk.shutil, "copy2") as copy:
                sdk.crypto()
                copy.assert_not_called()
                self.assertTrue(any(call.args[0] == "libsodium-1.0.22.tar.gz" for call in archive.call_args_list))

    def test_sodium_package_does_not_need_license_manager(self):
        import subprocess
        spec = importlib.util.spec_from_file_location("ipados_sdk_builder", ROOT / "tools/build_ipados_sdk.py")
        sdk = importlib.util.module_from_spec(spec)
        with patch("subprocess.check_output", return_value="/fixture/sdk\n"):
            spec.loader.exec_module(sdk)
        temporary = ROOT / "build/ipados-contracts"
        temporary.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary) as directory:
            root = Path(directory)
            prefix = root / "prefix"
            with patch.object(sdk, "PREFIX", prefix):
                sdk.write_sodium_package()
            (prefix / "include").mkdir()
            (prefix / "lib/libsodium.a").touch()
            source = root / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\nproject(SodiumContract NONE)\n'
                'find_package(Sodium CONFIG REQUIRED)\n'
                'if(NOT TARGET iiNativeSodium)\nmessage(FATAL_ERROR "Missing updater crypto target")\nendif()\n')
            subprocess.run(["cmake", "-S", str(source), "-B", str(root / "build"),
                            "-DCMAKE_PREFIX_PATH=" + str(prefix)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
