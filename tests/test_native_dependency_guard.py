"""Exercise the configured native link graph without executing the application."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GUARD = ROOT / "cmake/VerifyNativeEngine.cmake"
RESOURCE_ROOT = ROOT / "resources"


def configure(extra, source="", after="", legacy_alias=False):
    with tempfile.TemporaryDirectory(prefix="native-link-guard-", dir=RESOURCE_ROOT) as temporary:
        root = Path(temporary)
        (root / "App/Engine").mkdir(parents=True)
        (root / "App/Native").mkdir(parents=True)
        if legacy_alias:
            (root / "libLVRSCore.a").touch()
            try:
                (root / "libpainting.a").symlink_to("libLVRSCore.a")
            except (OSError, NotImplementedError):
                raise unittest.SkipTest("This platform cannot create a test symlink")
        if source:
            (root / "App/Engine/probe.cpp").write_text(source, encoding="utf-8")
        project = f'''cmake_minimum_required(VERSION 3.31)
project(NativeDependencyGuardProbe NONE)
add_library(CongregationArtwork INTERFACE)
add_library(Congregation INTERFACE)
add_library(Bridge INTERFACE)
target_link_libraries(CongregationArtwork INTERFACE Bridge)
{extra}
include("{GUARD.as_posix()}")
{after}
'''
        (root / "CMakeLists.txt").write_text(project, encoding="utf-8")
        command = ["cmake", "-S", str(root), "-B", str(root / "build")]
        if shutil.which("ninja"):
            command += ["-G", "Ninja"]
        env = os.environ.copy()
        for key in ("CMAKE_GENERATOR", "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH",
                    "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "LD_LIBRARY_PATH",
                    "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
                    "DYLD_FALLBACK_FRAMEWORK_PATH", "DYLD_INSERT_LIBRARIES"):
            env.pop(key, None)
        return subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


class NativeDependencyGuardTests(unittest.TestCase):
    def test_transitive_legacy_targets_and_paths_are_rejected(self):
        cases = {
            "qt_target": '''add_library(Qt6::Core INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE Qt6::Core)''',
            "qt_unversioned": '''add_library(Qt::Gui INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE Qt::Gui)''',
            "lvrs_target": '''add_library(LVRS::Native INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE LVRS::Native)''',
            "lvrs_core_target": '''add_library(LVRSCore INTERFACE)
target_link_libraries(Bridge INTERFACE LVRSCore)''',
            "lvrs_core_plugin": 'target_link_libraries(Bridge INTERFACE "/legacy/libLVRSCoreplugin.a")',
            "raw_qt": 'target_link_libraries(Bridge INTERFACE "-lQt6Core")',
            "windows_qt": 'target_link_libraries(Bridge INTERFACE "C:/legacy/Qt6Core.lib")',
            "raw_lvrs": 'target_link_libraries(Bridge INTERFACE "/legacy/libLVRS.a")',
            "link_option": 'target_link_options(Bridge INTERFACE "-Wl,-framework,QtCore")',
            "wrapped_legacy": '''add_library(LVRS::Native INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE "$<$<CONFIG:Debug>:LVRS::Native>")''',
            "wrapped_bridge": '''add_library(Outer INTERFACE)
add_library(Qt6::Core INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE "$<$<CONFIG:Debug>:Outer>")
target_link_libraries(Outer INTERFACE Qt6::Core)''',
            "alias": '''add_library(Qt6::Core INTERFACE IMPORTED)
add_library(Vendor::Ui ALIAS Qt6::Core)
target_link_libraries(Bridge INTERFACE Vendor::Ui)''',
            "imported_location": '''add_library(Vendor::Hidden UNKNOWN IMPORTED)
set_target_properties(Vendor::Hidden PROPERTIES IMPORTED_CONFIGURATIONS DEBUG IMPORTED_LOCATION_DEBUG "/legacy/libLVRS.a")
target_link_libraries(Bridge INTERFACE Vendor::Hidden)''',
            "windows_import_library": '''add_library(Vendor::Windows SHARED IMPORTED)
set_target_properties(Vendor::Windows PROPERTIES IMPORTED_CONFIGURATIONS DEBUG IMPORTED_LOCATION_DEBUG "C:/sdk/innocent.dll" IMPORTED_IMPLIB_DEBUG "C:/sdk/Qt6Gui.lib")
target_link_libraries(Bridge INTERFACE Vendor::Windows)''',
        }
        for name, fragment in cases.items():
            with self.subTest(name=name):
                result = configure(fragment)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("Native Congregation reaches", result.stdout)

    def test_legacy_native_source_is_rejected(self):
        for source in ('#include <LVRS/Widgets.h>\n', 'void test() { lvrs::Widget* item; }\n',
                       '#include <QtCore/QObject>\n'):
            with self.subTest(source=source):
                result = configure("", source)
                self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_dependencies_added_later_in_configuration_are_rejected(self):
        result = configure("", after='''add_library(LVRS::Native INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE LVRS::Native)''')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Native Congregation reaches", result.stdout)

    def test_library_symlinks_cannot_hide_legacy_dependencies(self):
        cases = (
            'target_link_libraries(Bridge INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/libpainting.a")',
            '''add_library(Vendor::Paint UNKNOWN IMPORTED)
set_target_properties(Vendor::Paint PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libpainting.a")
target_link_libraries(Bridge INTERFACE Vendor::Paint)''',
        )
        for fragment in cases:
            with self.subTest(fragment=fragment):
                result = configure(fragment, legacy_alias=True)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("Native Congregation reaches", result.stdout)

    def test_independent_targets_and_similar_names_remain_valid(self):
        result = configure('''add_library(Vendor::Native INTERFACE IMPORTED)
target_link_libraries(Bridge INTERFACE Vendor::Native "/sdk/libLVRSCompatible.a" "-lSDL2")''')
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
