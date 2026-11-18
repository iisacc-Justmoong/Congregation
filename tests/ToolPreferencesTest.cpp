#include <Workspace/EditorBackend.h>
#include <Workspace/ToolPreferences.h>
#include <iiFileProvider>
#include <iiSharedCanvas>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace congregation;
using namespace congregation::native;
using namespace congregation::ui;
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main(int argc, char** argv) try {
    if (argc == 3 && std::string_view(argv[1])=="--write-restart-fixture") {
        EditorBackend backend;
        backend.set_parameter(ToolParameter::size,81);
        check(bool(backend.set_current_color({.1,.2,.3,.8})),"Cannot set restart fixture color");
        backend.fill_tolerance.set(.33);
        backend.selected_tool.set("pencil");
        backend.set_parameter(ToolParameter::flow,37);
        backend.parameters->set_texture("Paper",2,1,{33,201});
        check(bool(ToolPreferences(argv[2]).save(backend)),"Cannot save restart fixture");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1])=="--read-restart-fixture") {
        EditorBackend backend;
        auto loaded=ToolPreferences(argv[2]).load(backend);
        check(loaded && *loaded && backend.brush_size.get()==81 && backend.fill_tolerance.get()==.33,
              "Fresh process did not restore saved panel values");
        const auto color=backend.current_color.get();
        check(std::abs(color.red-.1)<.0001 && std::abs(color.alpha-.8)<.0001,
              "Fresh process did not restore foreground color");
        backend.selected_tool.set("pencil");
        check(backend.flow.get()==.37 && backend.parameters->texture_name.get()=="Paper" &&
              backend.parameters->brush("pencil").engineState->material.texture.alpha==std::vector<std::uint8_t>({33,201}),
              "Fresh process did not restore pencil and texture");
        return 0;
    }
    check(argc == 2, "An isolated test resource directory is required");
    iiFileProvider::native::TemporaryDirectory tmp{fs::path(argv[1])};
    check(tmp.isValid(), "Cannot create isolated test directory");
    ToolPreferences store(tmp.path());
    EditorBackend first;
    first.set_parameter(ToolParameter::size, 91);
    check(bool(first.set_current_color({.1,.2,.3,.8})), "Foreground fixture failed");
    first.parameters->set_texture("연필 질감", 2, 2, {0, 32, 128, 255});
    first.parameters->set_background(0xff123456);
    first.selected_tool.set("pencil");
    first.set_parameter(ToolParameter::size, 23);
    first.set_parameter(ToolParameter::flow, 41);
    first.parameters->set_texture("Pencil", 2, 1, {90, 210});
    first.selected_tool.set("eraser");
    first.set_parameter(ToolParameter::size, 37);
    first.selected_tool.set("brush");
    check(first.brush_size.get() == 91, "Eraser or pencil overwrote brush settings");
    first.selection_feather.set(12);
    first.selection_antialias.set(false);
    first.wand_tolerance.set(.27);
    first.fill_tolerance.set(.42);
    first.fill_contiguous.set(false);
    first.clone_aligned.set(false);
    first.crop_aspect.set(2);
    first.polygon_sides.set(7);
    first.polygon_rotation.set(33);
    auto& gradient=*first.gradient_settings;
    check(gradient.add() && gradient.set_position(.25) && gradient.set_color(0xff123abc), "Gradient fixture failed");
    gradient.set_radial(true); gradient.set_repeat(true); gradient.cycle_blend();
    bool saved = false;
    first.perform = [&](Action action) {
        if (action == Action::save_tool_settings) { auto result=store.save(first); saved=bool(result); }
    };
    first.save_parameters();
    check(saved, "Panel save action did not persist settings");
    first.set_parameter(ToolParameter::size, 150); // not saved
    first.fill_tolerance.set(.9);

    EditorBackend reopened;
    auto loaded=store.load(reopened);
    check(loaded && *loaded, "Saved preferences were not loaded by a fresh backend");
    check(reopened.brush_size.get()==91 && reopened.selection_feather.get()==12 &&
          !reopened.selection_antialias.get() && reopened.wand_tolerance.get()==.27 &&
          reopened.fill_tolerance.get()==.42 && !reopened.fill_contiguous.get() &&
          !reopened.clone_aligned.get() && reopened.crop_aspect.get()==2 &&
          reopened.polygon_sides.get()==7 && reopened.polygon_rotation.get()==33,
          "Saved panel values changed or unsaved edits leaked across restart");
    const auto restoredColor=reopened.current_color.get();
    check(std::abs(restoredColor.red-.1)<.0001 && std::abs(restoredColor.green-.2)<.0001 &&
          std::abs(restoredColor.blue-.3)<.0001 && std::abs(restoredColor.alpha-.8)<.0001,
          "Foreground color did not restore");
    auto brush=reopened.parameters->brush();
    check(brush.engineState->material.texture.alpha==std::vector<std::uint8_t>({0,32,128,255}) &&
          brush.engineState->rasterizer.color.secondaryArgb==0xff123456 &&
          reopened.parameters->texture_name.get()=="연필 질감", "Brush texture or background was not restored");
    reopened.selected_tool.set("pencil");
    check(reopened.brush_size.get()==23 && reopened.flow.get()==.41 &&
          reopened.parameters->texture_name.get()=="Pencil" &&
          reopened.parameters->brush("pencil").engineState->material.texture.alpha==std::vector<std::uint8_t>({90,210}),
          "Pencil settings or texture did not restore independently");
    reopened.selected_tool.set("eraser");
    check(reopened.brush_size.get()==37, "Eraser settings were not restored independently");
    const auto& g=*reopened.gradient_settings;
    check(g.stops().size()==3 && g.selected()==1 && g.stops()[1].position==.25 &&
          g.stops()[1].argb==0xff123abc && g.kind()==iiSharedCanvas::GradientKind::Radial &&
          g.spread()==iiSharedCanvas::GradientSpread::Repeat && g.blend()==RasterBlendMode::Multiply,
          "Gradient settings were not restored");
    reopened.selected_tool.set("pencil");
    reopened.set_parameter(ToolParameter::size, 45);
    check(bool(store.save(reopened)), "A subsequent save failed");
    EditorBackend resaved;
    auto resavedLoaded=store.load(resaved);
    check(resavedLoaded && *resavedLoaded && resaved.brush_size.get()==91,
          "Saving another tool discarded the earlier brush profile");
    resaved.selected_tool.set("pencil");
    check(resaved.brush_size.get()==45, "The subsequent pencil save did not persist");

    iiFileProvider::native::TemporaryDirectory legacyDir{fs::path(argv[1])};
    check(legacyDir.isValid(), "Cannot create legacy fixture");
    ToolSettings legacy;
    legacy.set(ToolParameter::size, 73); legacy.select_brush("pencil"); legacy.set(ToolParameter::size, 31);
    auto legacyBytes=legacy.serialize();
    check(legacyBytes.starts_with("congregation.tools=3\n"), "Expected multiple profile fixture");
    legacyBytes.replace(0, std::string("congregation.tools=3\n").size(), "congregation.tools=2\n");
    for (const auto* prefix : {"eraser.", "smudge.", "clone-stamp."}) {
        for (auto pos=legacyBytes.find(prefix); pos!=std::string::npos; pos=legacyBytes.find(prefix)) {
            const auto end=legacyBytes.find('\n',pos);
            legacyBytes.erase(pos, end==std::string::npos ? std::string::npos : end-pos+1);
        }
    }
    check(legacyBytes.size()>8192, "Legacy profile fixture must exceed the old read limit");
    iiFileProvider::native::File::write(legacyDir.path()/"tools.ini", legacyBytes);
    EditorBackend migrated;
    auto legacyLoaded=ToolPreferences(legacyDir.path()).load(migrated);
    check(legacyLoaded && *legacyLoaded && migrated.brush_size.get()==73,
          "Legacy settings did not load");
    migrated.selected_tool.set("pencil");
    check(migrated.brush_size.get()==31, "Legacy profile was lost");
    check(fs::exists(legacyDir.path()/"tools.ini"), "Legacy settings were deleted");

    const auto target=tmp.path()/"tool-preferences.bin";
    auto intact=iiFileProvider::native::File::read(target, 2*1024*1024);
    iiFileProvider::native::File::write(target, intact.substr(0, intact.size()-1));
    EditorBackend unchanged; unchanged.set_parameter(ToolParameter::size, 19); unchanged.fill_tolerance.set(.73);
    auto corrupt=store.load(unchanged);
    check(!corrupt && unchanged.brush_size.get()==19 && unchanged.fill_tolerance.get()==.73,
          "Truncated settings partially changed the backend");
    iiFileProvider::native::File::write(target, intact);
    first.fill_tolerance.set(std::numeric_limits<double>::quiet_NaN());
    check(!store.save(first) && iiFileProvider::native::File::read(target,2*1024*1024)==intact,
          "Invalid values damaged the previous save");
    first.fill_tolerance.set(.9);
    auto badSave=ToolPreferences(target/"not-a-directory").save(first);
    check(!badSave && iiFileProvider::native::File::read(target,2*1024*1024)==intact,
          "Failed save damaged the existing preferences");
    iiFileProvider::native::TemporaryDirectory blockedDir{fs::path(argv[1])};
    check(blockedDir.isValid(), "Cannot create publication failure fixture");
    const auto blockedTarget=blockedDir.path()/"tool-preferences.bin";
    iiFileProvider::native::File::createDirectories(blockedTarget);
    iiFileProvider::native::File::write(blockedTarget/"keep", "original");
    check(!ToolPreferences(blockedDir.path()).save(first) &&
          iiFileProvider::native::File::read(blockedTarget/"keep")=="original",
          "Failed atomic publication damaged the pre-existing target");
    std::cout << "TOOL_PREFERENCES_OK\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
