"""Preservation and incremental-build contracts without a device or signing secrets."""
import hashlib
import io
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import tempfile
import tarfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ipados_builder", ROOT / "tools/build_ipados.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class Preservation(unittest.TestCase):
    def setUp(self):
        temporary = ROOT / "resources"
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
        temporary = ROOT / "resources"
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
        temporary = ROOT / "resources"
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
            build_root = ROOT / "build/ipados-contracts"
            build_root.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(dir=build_root) as build_directory:
                subprocess.run(["cmake", "-S", str(source), "-B", str(Path(build_directory) / "build"),
                                "-DCMAKE_PREFIX_PATH=" + str(prefix)], check=True, capture_output=True)


class IndependentUISDK(unittest.TestCase):
    def test_ipad_bundle_uses_the_project_build_number(self):
        with (ROOT / "packaging/ios/Info.plist.in").open("rb") as stream:
            info = plistlib.load(stream)
        self.assertEqual(info["CFBundleVersion"], "@CONGREGATION_BUNDLE_VERSION@")

    def test_compatible_json_snapshot_rejects_modified_sources(self):
        spec = importlib.util.spec_from_file_location("ipados_json_snapshot", ROOT / "tools/build_ipados_sdk.py")
        sdk = importlib.util.module_from_spec(spec)
        with patch("subprocess.check_output", return_value="/fixture/sdk\n"):
            spec.loader.exec_module(sdk)
        files = {name: ("fixture " + name).encode() for name in (
            "CMakeLists.txt", "src/iiJson/Json.h", "src/iiJson/Json.cpp",
            "cmake/iiJsonConfig.cmake.in", "tests/boundary.py")}
        archive = io.BytesIO()
        with tarfile.open(fileobj=archive, mode="w") as output:
            for name, data in files.items():
                info = tarfile.TarInfo(name)
                info.size = len(data)
                output.addfile(info, io.BytesIO(data))
        def git(args, **kwargs):
            if args[3] == "archive":
                return archive.getvalue()
            return files[args[4].split(":", 1)[1]]
        with tempfile.TemporaryDirectory(dir=ROOT / "resources") as directory:
            root = Path(directory)
            with patch.multiple(sdk, SDK=root / "sdk", WORK=root / "build"), \
                    patch.object(sdk.subprocess, "check_output", side_effect=git):
                source = sdk.prepare_compatible_json()
                self.assertEqual(sdk.prepare_compatible_json(), source)
                (source / "src/iiJson/Json.cpp").write_text("changed")
                with self.assertRaises(SystemExit):
                    sdk.prepare_compatible_json()

    def test_ipados_graph_builds_independent_ui_dependencies_in_order(self):
        import sys
        spec = importlib.util.spec_from_file_location("ipados_sdk_graph", ROOT / "tools/build_ipados_sdk.py")
        sdk = importlib.util.module_from_spec(spec)
        with patch("subprocess.check_output", return_value="/fixture/sdk\n"):
            spec.loader.exec_module(sdk)
        with tempfile.TemporaryDirectory(dir=ROOT / "resources") as directory:
            root = Path(directory)
            with patch.multiple(sdk, SDK=root / "sdk", WORK=root / "work", PREFIX=root / "prefix"), \
                    patch.object(sdk, "cmake") as cmake, patch.object(sdk, "invalidate_generation") as invalidated, \
                    patch.object(sdk, "record_generation") as recorded, patch.object(sdk, "crypto"), \
                    patch.object(sdk, "codecs"), patch.object(sdk, "prepare_compatible_json", return_value=root / "compatible/iiJson") as json_source, \
                    patch.object(sdk, "prepare_moltenvk", return_value=[]), \
                    patch.object(sys, "argv", ["build_ipados_sdk.py", "all"]):
                sdk.main()
            json_source.assert_called_once()
            self.assertEqual([call.args[1] for call in invalidated.call_args_list],
                             ['base', 'gui', 'network', 'canvas', 'files', 'network', 'canvas', 'gui'])
            self.assertEqual([call.args[2] for call in recorded.call_args_list],
                             ['files', 'network', 'canvas', 'gui'])
            self.assertTrue(any(call.args[0] == root / "compatible/iiJson" for call in cmake.call_args_list))
            calls = [(call.args[0].name, call.args[2] if len(call.args) > 2 else []) for call in cmake.call_args_list]
            names = [name for name, _ in calls]
            self.assertNotIn("LVRS", names)
            self.assertLess(names.index("iiColorManager"), names.index("LVUI"))
            self.assertIn("iiGraphicAdjust", names)
            self.assertIn("iiVisualCodec", names)
            self.assertNotIn("iiImageCodec", names)
            self.assertIn("-DIIVISUALCODEC_VIDEO=OFF", dict(calls)["iiVisualCodec"])
            self.assertNotIn("-DIIFILEPROVIDER_BUILD_CORE_IO=ON", dict(calls)["iiFileProvider"])
            self.assertTrue(any(call.args[0] == root / "sdk/iiFileProvider/example/CoreIO" for call in cmake.call_args_list))
            self.assertIn("-DLVUI_BUILD_COLOR_ADAPTER=ON", dict(calls)["LVUI"])
            self.assertIn("-DFETCHCONTENT_SOURCE_DIR_LUNASVG=" +
                          str(root / "sdk/iiGUI/resources/dependencies/lunasvg"), dict(calls)["iiGUI"])
            self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON", sdk.COMMON)

    def test_missing_independent_ui_packages_do_not_replace_existing_bundle(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "resources") as directory:
            root = Path(directory)
            (root / "lib").mkdir()
            for name in ("libiiGUI.a", "libiiSharedCanvasNative.a", "libiiNetwork.a"):
                (root / "lib" / name).touch()
            compiler = root / "lvui-compile"
            compiler.touch()
            with patch.multiple(builder, PREFIX=root, HOST_COMPILER=compiler), \
                    patch.object(builder, "preserve_previous_platform") as preserve, \
                    patch.object(builder.subprocess, "check_output", return_value="/fixture/sdk\n"), \
                    patch.object(builder, "run"):
                with self.assertRaises(SystemExit) as error:
                    builder.build("team", "device")
                self.assertIn("ColorSelectionSurface.cpp", str(error.exception))
                preserve.assert_not_called()

    def test_complete_kit_uses_selected_host_compiler_without_legacy_packages(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "resources") as directory:
            root = Path(directory)
            (root / "lib").mkdir()
            for name in ("libiiGUI.a", "libiiSharedCanvasNative.a", "libiiNetwork.a", "libiiColorManager.a",
                         "libiiGraphicAdjust.a", "libiiPaintEngineBrushTools.a"):
                (root / "lib" / name).touch()
            for name in ("LVUI/LVUIConfig.cmake", "LVUI/LVUIColorTargets.cmake",
                         "iiColorManager/iiColorManagerConfig.cmake", "iiGraphicAdjust/iiGraphicAdjustConfig.cmake",
                         "iiBrushTools/iiBrushToolsConfig.cmake", "iiGUI/iiGUIConfig.cmake"):
                config = root / "lib/cmake" / name
                config.parent.mkdir(parents=True, exist_ok=True)
                config.touch()
            adapter = root / "share/LVUI/examples/Integrations/App/ColorSelectionSurface.cpp"
            adapter.parent.mkdir(parents=True)
            adapter.touch()
            compiler = root / "lvui-compile"
            compiler.touch()
            with patch.multiple(builder, PREFIX=root, HOST_COMPILER=compiler), \
                    patch.object(builder, "verify_sdk_coherence"), \
                    patch.object(builder, "preserve_previous_platform"), \
                    patch.object(builder.subprocess, "check_output", return_value="/fixture/sdk\n"), \
                    patch.object(builder, "run") as run:
                builder.build("team", "device")
            configure = run.call_args_list[0].args[0]
            self.assertIn("-DLVUI_HOST_COMPILER=" + str(compiler), configure)
            self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON", configure)
            self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON", configure)
            self.assertIn("-DiiColorManager_DIR=" + str(root / "lib/cmake/iiColorManager"), configure)
            self.assertIn("-DiiGraphicAdjust_DIR=" + str(root / "lib/cmake/iiGraphicAdjust"), configure)
            with patch.multiple(builder, PREFIX=root, HOST_COMPILER=compiler), \
                    patch.object(builder, "verify_sdk_coherence", side_effect=RuntimeError("base generation changed")), \
                    patch.object(builder, "preserve_previous_platform") as preserve:
                with self.assertRaisesRegex(SystemExit, "base generation changed"):
                    builder.build("team", "device")
                preserve.assert_not_called()
            (root / "lib/cmake/LVUI/LVUIColorTargets.cmake").unlink()
            with patch.multiple(builder, PREFIX=root, HOST_COMPILER=compiler), \
                    patch.object(builder, "preserve_previous_platform") as preserve, \
                    patch.object(builder.subprocess, "check_output", return_value="/fixture/sdk\n"), \
                    patch.object(builder, "run"):
                with self.assertRaises(SystemExit) as error:
                    builder.build("team", "device")
                self.assertIn("LVUIColorTargets.cmake", str(error.exception))
                preserve.assert_not_called()

    def test_host_compiler_can_be_selected_explicitly(self):
        spec = importlib.util.spec_from_file_location("ipados_host_compiler", ROOT / "tools/build_ipados.py")
        module = importlib.util.module_from_spec(spec)
        with patch.dict(os.environ, {"LVUI_HOST_COMPILER": "/fixture/custom-lvui-compile"}):
            spec.loader.exec_module(module)
        self.assertEqual(module.HOST_COMPILER, Path("/fixture/custom-lvui-compile"))


if __name__ == "__main__":
    unittest.main()
