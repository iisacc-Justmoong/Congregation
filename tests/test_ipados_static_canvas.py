"""The iPad app must reject a dynamically linked canvas SDK."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class StaticCanvasContract(unittest.TestCase):
    def configure(self, library_type):
        with tempfile.TemporaryDirectory(prefix="ios-static-source-", dir=ROOT / "resources") as source, \
             tempfile.TemporaryDirectory(prefix="ios-static-test-", dir=ROOT / "build") as output:
            Path(source, "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(StaticSDKContract NONE)\n"
                f"add_library(iiSharedCanvas::Native {library_type} IMPORTED)\n"
                'include("${CONTRACT_MODULE}")\n'
                "congregation_require_static_ios_sdk(iiSharedCanvas::Native)\n",
                encoding="utf-8",
            )
            return subprocess.run(
                ["cmake", "-S", source, "-B", output,
                 "-DCONTRACT_MODULE=" + str(ROOT / "cmake/RequireStaticIOSSDK.cmake")],
                text=True, capture_output=True, check=False,
            )

    def test_static_canvas_is_accepted(self):
        result = self.configure("STATIC")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_dynamic_canvas_is_rejected(self):
        result = self.configure("SHARED")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires a static iiSharedCanvas::Native package", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
