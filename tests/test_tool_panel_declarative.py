"""Guard the tool panel's declarative view boundary without opening a window."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ToolPanelDeclarativeBoundary(unittest.TestCase):
    def test_layout_is_lvui_and_mounts_the_generated_view(self):
        view = ROOT / "App/Native/views/ToolPanel.lvui"
        self.assertTrue(view.is_file(), "The tool properties view must be declared in LVUI")
        source = view.read_text(encoding="utf-8")
        self.assertIn('ScrollView(id: "tool-scroll"', source)
        sections = ("Shape Dynamics", "Scattering", "Texture", "Color Dynamics", "Transfer",
                    "Brush Pose", "Noise", "Wet Edges", "Build-up", "Smoothing")
        positions = [source.index(f'Label(text: "{name}"') for name in sections]
        self.assertEqual(positions, sorted(positions), "Brush sections must retain their design order")
        mount = (ROOT / "App/Native/ToolPanel.cpp").read_text(encoding="utf-8")
        self.assertIn("ToolPanelView::build", mount)
        self.assertNotIn("b.controls.slider(", mount)
        self.assertNotIn("b.controls.check_box(", mount)
        self.assertNotIn("raw.scroll_view(", mount)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("App/Native/views/ToolPanel.lvui", cmake)

    def test_every_brush_parameter_has_a_declared_control(self):
        view = (ROOT / "App/Native/views/ToolPanel.lvui").read_text(encoding="utf-8")
        settings = (ROOT / "App/Native/ToolSettings.cpp").read_text(encoding="utf-8")
        expected = set(re.findall(r'\{P::\w+,\s*"([^"]+)"', settings))
        declared = re.findall(r'panel\.row\("([^"]+)"\)', view)
        self.assertEqual(expected, set(declared))
        self.assertEqual(len(declared), len(set(declared)), "Every parameter must have one control")


if __name__ == "__main__":
    unittest.main()
