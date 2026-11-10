import importlib.util
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_native_macos', ROOT / 'cmake/package_native_macos.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class NativePackageDirectoryTests(unittest.TestCase):
    def test_build_script_forwards_explicit_sdk_prefixes(self):
        with tempfile.TemporaryDirectory(prefix='sdk-script-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw)
            capture = root / 'arguments.txt'
            cmake = root / 'cmake'
            cmake.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$CAPTURE_FILE"\n', encoding='utf-8')
            cmake.chmod(0o755)
            xcrun = root / 'xcrun'
            xcrun.write_text('#!/bin/sh\nprintf "%s\\n" /fixture/sdk\n', encoding='utf-8')
            xcrun.chmod(0o755)
            selected = root / 'selected sdk'
            environment = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'], CAPTURE_FILE=str(capture),
                               CONGREGATION_LVUI_PREFIX=str(selected), CONGREGATION_IIGUI_PREFIX=str(root / 'gui'),
                               CONGREGATION_COLOR_MANAGER_PREFIX=str(root / 'color'), CONGREGATION_CORE_PREFIX=str(root / 'core'),
                               LVUI_HOST_COMPILER=str(root / 'compiler'))
            result = subprocess.run(['/bin/bash', str(ROOT / 'build.sh'), '--configure-only'],
                                    cwd=root, env=environment, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            arguments = capture.read_text(encoding='utf-8').splitlines()
            self.assertIn('-DCONGREGATION_LVUI_PREFIX=' + str(selected), arguments)
            self.assertIn('-DCONGREGATION_IIGUI_PREFIX=' + str(root / 'gui'), arguments)
            self.assertIn('-DCONGREGATION_COLOR_MANAGER_PREFIX=' + str(root / 'color'), arguments)
            self.assertIn('-DCONGREGATION_CORE_PREFIX=' + str(root / 'core'), arguments)
            self.assertIn('-DLVUI_HOST_COMPILER=' + str(root / 'compiler'), arguments)
            self.assertIn('-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON', arguments)

    def test_explicit_sdk_packages_override_the_aggregate_kit(self):
        with tempfile.TemporaryDirectory(prefix='sdk-overrides-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw)
            for package in ('LVUI', 'iiGUI', 'iiColorManager', 'iiSDKCore'):
                directory = root / package / 'lib/cmake' / package
                directory.mkdir(parents=True)
                (directory / (package + 'Config.cmake')).touch()
            script = root / 'probe.cmake'
            script.write_text('''include("${MODULE}")
foreach(package IN ITEMS LVUI iiGUI iiColorManager iiSDKCore)
  set(${package}_DIR "old-aggregate" CACHE PATH "" FORCE)
  set(prefix "${ROOT}/${package}")
  congregation_override_native_package("${prefix}" "${package}")
  if(NOT "${${package}_DIR}" STREQUAL "${prefix}/lib/cmake/${package}")
    message(FATAL_ERROR "Selected package was not used: ${package}")
  endif()
endforeach()
''', encoding='utf-8')
            command = ['cmake', '-DMODULE=' + str(ROOT / 'cmake/NativeSDKOverrides.cmake'),
                       '-DROOT=' + str(root), '-P', str(script)]
            result = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            (root / 'iiGUI/lib/cmake/iiGUI/iiGUIConfig.cmake').unlink()
            rejected = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn('iiGUI', rejected.stdout + rejected.stderr)

    def test_legacy_ui_libraries_are_rejected_by_install_name(self):
        for name in ('@rpath/LVRS.framework/Versions/A/LVRS', '@rpath/libLVRS.1.dylib',
                     '@rpath/libLVRSCore.1.dylib', '@rpath/libLVRSCoreplugin.a',
                     '@rpath/LVRSNative.framework/Versions/A/LVRSNative',
                     '@rpath/QtCore.framework/QtCore', '/kit/lib/libQt6Gui.dylib',
                     r'C:\sdk\qt6core.dll'):
            with self.subTest(name=name):
                self.assertTrue(module.forbidden_ui_dependency(name))
        for name in ('@rpath/libiiSDKCore.dylib', '@rpath/libLVRSCompatible.dylib',
                     '/System/Library/Frameworks/AppKit.framework/AppKit'):
            with self.subTest(name=name):
                self.assertFalse(module.forbidden_ui_dependency(name))

    def test_only_selected_library_directories_are_trusted(self):
        with tempfile.TemporaryDirectory(prefix='package-dirs-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw).resolve()
            prefix, frameworks, selected, other = (root / name for name in ('prefix', 'frameworks', 'selected', 'other'))
            for path in (prefix, frameworks, selected, other):
                path.mkdir()
            library = selected / 'libfixture.dylib'
            library.touch()
            self.assertTrue(module.trusted_library(library, prefix, frameworks, [selected]))
            self.assertFalse(module.trusted_library(other / 'libfixture.dylib', prefix, frameworks, [selected]))
            (selected / 'escape.dylib').symlink_to(other / 'libfixture.dylib')
            self.assertFalse(module.trusted_library(selected / 'escape.dylib', prefix, frameworks, [selected]))

    def test_selected_library_cannot_hide_legacy_ui_behind_a_symlink(self):
        with tempfile.TemporaryDirectory(prefix='package-legacy-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw).resolve()
            legacy = root / 'libLVRSCore.dylib'
            legacy.touch()
            alias = root / 'libpainting.dylib'
            alias.symlink_to(legacy.name)
            with self.assertRaisesRegex(RuntimeError, 'Qt or LVRS'):
                module.validate_selected_library(alias, root, root / 'frameworks', [])
            compatible = root / 'libLVRSCompatible.dylib'
            compatible.touch()
            alias.unlink()
            alias.symlink_to(compatible.name)
            self.assertEqual(module.validate_selected_library(alias, root, root / 'frameworks', []), compatible)

    def test_explicit_linked_library_precedes_stale_prefix_copy(self):
        prefix = pathlib.Path('/kit')
        frameworks = pathlib.Path('/bundle/Frameworks')
        selected = pathlib.Path('/selected')
        candidates = module.library_candidates('libfixture.dylib', prefix, frameworks, [selected])
        self.assertEqual(candidates[0], selected / 'libfixture.dylib')
        self.assertEqual(candidates[1], prefix / 'lib/libfixture.dylib')

    def test_exact_linked_target_precedes_stale_copy_in_another_selected_directory(self):
        stale, actual = pathlib.Path('/other-sdk'), pathlib.Path('/linked-sdk')
        candidates = module.library_candidates('libfixture.dylib', pathlib.Path('/kit'),
            pathlib.Path('/bundle/Frameworks'), [stale, actual], [('libfixture.dylib', actual / 'libfixture.dylib')])
        self.assertEqual(candidates[0], actual / 'libfixture.dylib')
        self.assertEqual(candidates[1], stale / 'libfixture.dylib')

    def test_install_name_alias_selects_the_exact_link_target(self):
        actual = pathlib.Path('/linked-sdk/libfixture.0.11.dylib')
        candidates = module.library_candidates('libfixture.0.dylib', pathlib.Path('/kit'),
            pathlib.Path('/bundle/Frameworks'), [pathlib.Path('/stale')],
            [('libfixture.0.dylib', actual)])
        self.assertEqual(candidates[0], actual)

    def test_staged_canvas_library_wins_over_an_older_kit_copy(self):
        with tempfile.TemporaryDirectory(prefix='package-canvas-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw).resolve()
            prefix, frameworks, selected = root / 'kit', root / 'frameworks', root / 'selected'
            (prefix / 'lib').mkdir(parents=True)
            frameworks.mkdir()
            selected.mkdir()
            name = 'libiiSharedCanvasNative.dylib'
            (prefix / 'lib' / name).touch()
            exact = selected / name
            exact.touch()
            candidates = module.library_candidates(name, prefix, frameworks, [selected], [(name, exact)])
            located = next(path for path in candidates if path.is_file())
            self.assertEqual(module.validate_selected_library(located, prefix, frameworks, [selected]), exact)
            self.assertFalse(module.trusted_library(root / 'unselected' / name, prefix, frameworks, [selected]))

    def test_bundle_arguments_include_shared_sdk_but_omit_static_archive(self):
        with tempfile.TemporaryDirectory(prefix='bundle-sdk-source-', dir=ROOT / 'resources') as source, \
             tempfile.TemporaryDirectory(prefix='bundle-sdk-build-', dir=ROOT / 'build') as output:
            for library_type in ('STATIC', 'SHARED'):
                fixture = pathlib.Path(source) / library_type
                fixture.mkdir()
                (fixture / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.31)\nproject(BundleSDK NONE)\n'
                    f'add_library(canvas {library_type} IMPORTED)\n'
                    'set_target_properties(canvas PROPERTIES IMPORTED_LOCATION "/fixture/libcanvas.dylib")\n'
                    'include("${BUNDLE_MODULE}")\n'
                    'congregation_bundle_sdk_arguments(arguments canvas)\n'
                    'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/arguments.txt" CONTENT "${arguments}")\n',
                    encoding='utf-8')
                build = pathlib.Path(output) / library_type
                result = subprocess.run(['cmake', '-S', str(fixture), '-B', str(build),
                    '-DBUNDLE_MODULE=' + str(ROOT / 'cmake/NativeBundleSDK.cmake')],
                    text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                arguments = (build / 'arguments.txt').read_text(encoding='utf-8')
                if library_type == 'STATIC':
                    self.assertEqual(arguments, '')
                else:
                    self.assertEqual(arguments.split(';'),
                                     ['--library', '/fixture/libcanvas.dylib', '--library-dir', '/fixture'])

    def test_additional_static_sdk_notices_are_packaged(self):
        with tempfile.TemporaryDirectory(prefix='package-notices-', dir=ROOT / 'resources') as raw:
            root = pathlib.Path(raw)
            prefix, bundle, extra = (root / name for name in ('kit', 'bundle', 'extra'))
            (prefix / 'share/licenses/base').mkdir(parents=True)
            (prefix / 'share/licenses/base/LICENSE').write_text('base')
            (extra / 'iiGraphicAdjust/photocraft').mkdir(parents=True)
            (extra / 'iiGraphicAdjust/photocraft/LICENSE-MIT').write_text('photocraft')
            module.copy_license_notices(prefix, bundle, [extra])
            dest = bundle / 'Contents/Resources/licenses'
            self.assertEqual((dest / 'base/LICENSE').read_text(), 'base')
            self.assertEqual((dest / 'iiGraphicAdjust/photocraft/LICENSE-MIT').read_text(), 'photocraft')
            with self.assertRaises(RuntimeError):
                module.copy_license_notices(prefix, bundle, [root / 'missing'])

if __name__ == '__main__':
    unittest.main()
