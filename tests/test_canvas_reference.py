"""Keep the native chooser's catalogue and public design dimensions traceable."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CanvasReferenceTests(unittest.TestCase):
    def test_catalogue_preserves_all_authored_records(self):
        source = ROOT / "resources/canvas-presets.json"
        record = json.loads((ROOT / "resources/canvas-presets-source.json").read_text(encoding="utf-8"))
        data = json.loads(source.read_text(encoding="utf-8"))
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), record["sha256"])
        self.assertEqual(len(data["categories"]), 14)
        entries = [p for c in data["categories"] for s in c["sections"] for p in s["presets"]]
        self.assertEqual(len(entries), 289)
        self.assertEqual(len({p["id"] for p in entries}), 289)
        self.assertTrue(all(p.get("classification") in {"starter", "documented-recommendation"} and (p.get("classification") == "starter" or p.get("source")) for p in entries))
        result = subprocess.run([sys.executable, "-B", str(ROOT / "tools/sync_canvas_presets.py"), "--check"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_dreamscapes_layout_is_expressed_as_metric_values(self):
        theme = (ROOT / "App/Native/views/Theme.lvui").read_text(encoding="utf-8")
        values = dict(re.findall(r"let (\w+): Metric = ([\d.]+)", theme))
        for name, expected in {"canvasChooserWidth": 1280, "canvasChooserHeight": 880,
                               "canvasChooserCompactThreshold": 1000, "canvasChooserCategories": 200,
                               "canvasChooserDetails": 268, "canvasChooserHeader": 96,
                               "canvasChooserCompactHeader": 132, "canvasChooserFooter": 68,
                               "canvasChooserCompactFooter": 94, "canvasChooserCard": 144,
                               "canvasChooserCardPreview": 62, "canvasChooserDetailPreview": 124}.items():
            with self.subTest(name=name):
                self.assertEqual(float(values[name]), expected)
        content = (ROOT / "App/Native/CanvasSetupWindow.cpp").read_text(encoding="utf-8")
        self.assertNotRegex(content, r"#include [<\"]Q|QQml|QVariant|QQuick")
        lvui = (ROOT / "App/Native/views/CanvasSetup.lvui").read_text(encoding="utf-8")
        self.assertIn('<Window title="New canvas"', lvui)
        self.assertNotIn('<Modal', lvui)
        app = (ROOT / "App/Native/Application.cpp").read_text(encoding="utf-8")
        self.assertIn('setup_host->show(', app)


if __name__ == "__main__":
    unittest.main()
