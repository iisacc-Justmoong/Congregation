"""Guard the tool panel's declarative view boundary without opening a window."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ToolPanelDeclarativeBoundary(unittest.TestCase):
    def test_layout_is_lvui_and_mounts_the_generated_view(self):
        view = ROOT / "App/Tools/ToolPanel.lvui"
        self.assertTrue(view.is_file(), "The tool properties view must be declared in LVUI")
        source = view.read_text(encoding="utf-8")
        self.assertIn('ScrollView(id: "tool-scroll"', source)
        sections = ("Shape Dynamics", "Scattering", "Texture", "Color Dynamics", "Transfer",
                    "Brush Pose", "Noise", "Wet Edges", "Build-up", "Smoothing")
        window = source[source.index("\nWindow {"):]
        positions = [window.index(f'Label(text: "{name}"') for name in sections]
        self.assertEqual(positions, sorted(positions), "Brush sections must retain their design order")
        mount = (ROOT / "App/Tools/ToolPanel.cpp").read_text(encoding="utf-8")
        self.assertIn("ToolPanelView::build", mount)
        self.assertNotIn("b.controls.slider(", mount)
        self.assertNotIn("b.controls.check_box(", mount)
        self.assertNotIn("raw.scroll_view(", mount)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("App/Tools/ToolPanel.lvui", cmake)

    def test_every_brush_parameter_has_a_declared_control(self):
        view = (ROOT / "App/Tools/ToolPanel.lvui").read_text(encoding="utf-8")
        settings = (ROOT / "App/Tools/ToolSettings.cpp").read_text(encoding="utf-8")
        expected = set(re.findall(r'\{P::\w+,\s*"([^"]+)"', settings))
        declared = re.findall(r'panel\.row\("([^"]+)"\)', view)
        variant = {"tip-shape", "brush-blend-mode", "brush-symmetry", "pencil-pixel-edges",
                   "pencil-auto-erase", "pencil-snap", "airbrush-radius", "airbrush-rate",
                   "airbrush-stationary", "dry-grain-amount", "dry-grain-size", "dry-dryness",
                   "bristle-length", "bristle-thickness", "bristle-stiffness", "bristle-spread",
                   "mixer-sample-all", "mixer-clean-after", "mixer-load-after", "mixing-active",
                   "absolute-spacing", "pressure-opacity-enabled", "pressure-curve-min",
                   "pressure-curve-mid", "pressure-curve-max"}
        # Base opacity is shown above the scroll view in the shared inspector.
        outside_panel = {"opacity"}
        self.assertEqual(expected - (variant | outside_panel), set(declared))
        self.assertTrue((variant | outside_panel).issubset(expected))
        self.assertEqual(len(declared), len(set(declared)), "Every parameter must have one control")

    def test_applying_brush_does_not_write_back_unrelated_settings(self):
        app = (ROOT / "App/Application/Application.cpp").read_text(encoding="utf-8")
        start = app.index("    void apply_brush(ui::EditorBackend& backend)")
        end = app.index("\n    void ", start + 1)
        self.assertNotIn("parameters->set(", app[start:end],
                         "Applying a brush must not rewrite other tool parameters")

    def test_toolbar_selectors_have_tool_specific_panels(self):
        toolbar = (ROOT / "App/Workspace/Main.lvui").read_text(encoding="utf-8")
        editor = (ROOT / "App/Workspace/EditorBackend.h").read_text(encoding="utf-8")
        catalog = (ROOT / "App/Tools/ToolPanelCatalog.cpp").read_text(encoding="utf-8")
        selectors = set(re.findall(r'onClick:\s*vm\.(choose_tool_\w+)', toolbar))
        panels = set(re.findall(r'\{"([^"\n]+)",\s*"[^"\n]+",\s*"\d+:\d+",\s*(?:true|false),', catalog))
        self.assertTrue(selectors)
        for selector in sorted(selectors):
            target = re.search(r'void\s+' + re.escape(selector) + r'\(\)\s*\{\s*select_tool\("([^"]+)"\)', editor)
            self.assertIsNotNone(target, f"Cannot identify the tool selected by {selector}")
            self.assertIn(target.group(1), panels, f"No tool panel for {target.group(1)}")
        self.assertIn("symmetry-paint", panels)


if __name__ == "__main__":
    unittest.main()
