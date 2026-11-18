import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / "App"


class AppDomainLayoutTest(unittest.TestCase):
    def test_runtime_does_not_own_duplicate_canvas_state(self):
        engine = (APP / "Application/ApplicationEngine.h").read_text(encoding="utf-8")
        palette = (APP / "Color/Palette.h").read_text(encoding="utf-8")
        document = (APP / "Document/CanvasDocument.h").read_text(encoding="utf-8")
        self.assertFalse("CanvasDocument" in engine, "application lifecycle must not own unused canvas state")
        self.assertFalse("Document/CanvasDocument.h" in palette, "palette must not depend on canvas state")
        self.assertFalse("struct Color final" in document, "color values belong to the color domain")
        self.assertTrue((APP / "Color/Color.h").is_file(), "the color domain must own the color value")

    def test_layer_change_semantics_have_one_owner(self):
        header = (APP / "Canvas/Artwork.h").read_text(encoding="utf-8")
        implementation = (APP / "Canvas/Artwork.cpp").read_text(encoding="utf-8")
        for duplicate in ("enum class LayerPropertyField", "struct LayerPropertyEdit",
                          "struct LayerPropertyRollback", "same_layer_property(",
                          "read_layer_property(", "swap_layer_property(", "apply_layer_property("):
            self.assertFalse(duplicate in header or duplicate in implementation,
                             f"layer change behavior must be owned by iiSharedCanvas: {duplicate}")
        self.assertIn("iiSharedCanvas::LayerChange", header)
        self.assertIn("LayerPropertyGuard", implementation)

    def test_objects_and_views_belong_to_domains(self):
        self.assertFalse((APP / "Native").exists())
        self.assertFalse((APP / "Engine").exists())
        for relative in (
            "Application/Application.cpp", "Application/ApplicationEngine.cpp",
            "Canvas/Artwork.cpp", "Canvas/CanvasView.cpp", "Canvas/CanvasSetup.lvui",
            "Document/CanvasDocument.cpp", "Document/EditCommand.cpp",
            "Tools/ToolSettings.cpp", "Tools/ToolPanelBackend.cpp", "Tools/ToolPanel.lvui",
            "Timeline/TimelineModel.cpp", "Timeline/TimelineView.cpp",
            "Color/EditorColor.cpp", "Account/LicenseService.cpp",
            "Agent/WorkspaceAgentClient.cpp", "Collaboration/CanvasSession.cpp",
            "Preferences/Preferences.cpp", "Preferences/Preferences.lvui",
            "Assets/AssetLibraryView.cpp", "Storage/SocietyFiles.cpp",
            "Update/SocietyUpdateFeed.cpp", "Workspace/EditorBackend.cpp",
            "Workspace/WorkspaceView.cpp", "Workspace/Main.lvui", "Shared/Layout.lvui",
        ):
            self.assertTrue((APP / relative).is_file(), relative)

    def test_current_sources_do_not_depend_on_retired_paths(self):
        files = [ROOT / "CMakeLists.txt", APP / "CMakeLists.txt"]
        for directory in (APP, ROOT / "cmake", ROOT / "tests", ROOT / "tools"):
            files.extend(p for p in directory.rglob("*")
                         if p.is_file() and p.suffix in {".h", ".cpp", ".lvui", ".cmake", ".py", ".txt"})
        for path in files:
            if path.resolve() == pathlib.Path(__file__).resolve():
                continue
            contents = path.read_text(encoding="utf-8", errors="replace")
            for needle in ("App/" + "Native/", "App/" + "Engine/", "<" + "Native/", "<" + "Engine/", '"' + "Native/", '"' + "Engine/"):
                self.assertFalse(needle in contents, f"{path}: retired path {needle}")


if __name__ == "__main__":
    unittest.main()
