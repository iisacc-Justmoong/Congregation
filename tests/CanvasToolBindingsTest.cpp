#include <Application/CanvasToolBindings.h>
#include <Canvas/Artwork.h>
#include <Canvas/CanvasView.h>
#include <Workspace/EditorBackend.h>
#include <iiFileProvider>
#include <iiSharedCanvas>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace congregation::native;
using congregation::ui::EditorBackend;

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::vector<std::uint32_t> pixels(const Artwork& artwork) {
    auto result = iiSharedCanvas::renderFrame(artwork.document(), 0);
    check(result.ok(), "Cannot render test document");
    return std::move(result.pixels.pixels);
}
}

int main(int argc, char** argv) try {
    check(argc == 2, "Expected an isolated resource directory");
    iiFileProvider::native::TemporaryDirectory directory{std::filesystem::path(argv[1])};
    check(directory.isValid(), "Cannot create isolated document directory");
    Artwork artwork(directory.path());
    check(artwork.create(96, 64), "Cannot create test canvas");
    CanvasView canvas(artwork);
    EditorBackend editor;

    // Values already present when a view mounts must reach the canvas without
    // waiting for a pointer event, as with restored user preferences.
    editor.brush_size.set(19);
    editor.opacity.set(.4);
    editor.text_value.set("Saved text");
    editor.selection_feather.set(7);
    editor.magnetic_radius.set(21);
    editor.clone_aligned.set(false);
    editor.clone_current_layer.set(false);
    editor.polygon_sides.set(8);
    editor.polygon_rotation.set(35);
    editor.polygon_center.set(true);
    editor.polygon_fill.set(false);
    editor.polygon_outline.set(true);
    editor.polygon_stroke_width.set(4);
    editor.polygon_opacity.set(.5);
    editor.crop_aspect.set(2);
    editor.crop_destructive.set(true);
    editor.selection_antialias.set(false);
    editor.wand_tolerance.set(.3);
    editor.wand_contiguous.set(false);
    editor.wand_current_layer.set(true);
    editor.fill_tolerance.set(.4);
    editor.fill_contiguous.set(false);
    editor.fill_current_layer.set(true);
    editor.fill_target_current.set(true);
    editor.fill_preserve_alpha.set(true);
    editor.fill_antialias.set(true);
    check(bool(editor.set_current_color({.2, .4, .6, 1})), "Cannot set foreground");
    editor.gradient_settings->set_radial(true);
    editor.gradient_settings->set_repeat(true);
    editor.gradient_settings->cycle_blend();

    auto bindings = std::make_unique<CanvasToolBindings>(editor, canvas);
    check(canvas.brush_size == 19 && canvas.opacity == .4 && canvas.text == "Saved text",
          "Initial brush or text settings were not applied");
    check(canvas.selection_feather == 7 && canvas.magnetic_radius == 21 &&
          !canvas.clone_aligned && !canvas.clone_current_layer && !canvas.selection_antialias,
          "Initial selection or clone settings were not applied");
    check(canvas.polygon_options.sides == 8 && canvas.polygon_options.rotation_degrees == 35 &&
          canvas.polygon_options.from_center && !canvas.polygon_options.fill && canvas.polygon_options.outline &&
          canvas.polygon_options.stroke_width == 4 && canvas.polygon_options.opacity == .5,
          "Initial shape settings were not applied");
    check(std::abs(canvas.crop_aspect - 4. / 3.) < 1e-12 && canvas.crop_destructive &&
          canvas.wand_tolerance == .3 && !canvas.wand_contiguous && canvas.wand_current_layer,
          "Initial crop or wand settings were not applied");
    check(canvas.fill_tolerance == .4 && !canvas.fill_contiguous && canvas.fill_current_layer &&
          canvas.fill_target_current && canvas.fill_preserve_alpha && canvas.fill_antialias,
          "Initial fill settings were not applied");
    check(canvas.color == 0xff336699U && canvas.gradient_stops.size() == 2 &&
          canvas.gradient_stops[0].argb == canvas.color &&
          canvas.gradient_kind == iiSharedCanvas::GradientKind::Radial &&
          canvas.gradient_spread == iiSharedCanvas::GradientSpread::Repeat &&
          canvas.gradient_blend == RasterBlendMode::Multiply,
          "Initial color or gradient settings were not applied");

    // Each edit takes effect before the next canvas input.
    editor.set_brush_size(11);
    editor.set_opacity(.7);
    editor.text_value.set("New text");
    editor.selection_feather.set(3);
    editor.magnetic_radius.set(9);
    editor.clone_aligned.set(true);
    editor.clone_current_layer.set(true);
    editor.polygon_sides.set(100); // Preserve the canvas's existing bounds.
    editor.polygon_rotation.set(-20);
    editor.polygon_center.set(false);
    editor.polygon_fill.set(true);
    editor.polygon_outline.set(false);
    editor.polygon_stroke_width.set(2);
    editor.polygon_opacity.set(.8);
    editor.crop_aspect.set(3);
    editor.crop_destructive.set(false);
    editor.selection_antialias.set(true);
    editor.wand_tolerance.set(.2);
    editor.wand_contiguous.set(true);
    editor.wand_current_layer.set(false);
    editor.fill_tolerance.set(.1);
    editor.fill_contiguous.set(true);
    editor.fill_current_layer.set(false);
    editor.fill_target_current.set(false);
    editor.fill_preserve_alpha.set(false);
    editor.fill_antialias.set(false);
    check(canvas.brush_size == 11 && canvas.opacity == .7 && canvas.text == "New text" &&
          canvas.selection_feather == 3 && canvas.magnetic_radius == 9 &&
          canvas.clone_aligned && canvas.clone_current_layer && canvas.selection_antialias,
          "Changed brush, text, selection, or clone settings were not applied");
    check(canvas.polygon_options.sides == 64 && canvas.polygon_options.rotation_degrees == -20 &&
          !canvas.polygon_options.from_center && canvas.polygon_options.fill && !canvas.polygon_options.outline &&
          canvas.polygon_options.stroke_width == 2 && canvas.polygon_options.opacity == .8,
          "Changed shape settings were not applied");
    check(std::abs(canvas.crop_aspect - 16. / 9.) < 1e-12 && !canvas.crop_destructive &&
          canvas.wand_tolerance == .2 && canvas.wand_contiguous && !canvas.wand_current_layer &&
          canvas.fill_tolerance == .1 && canvas.fill_contiguous && !canvas.fill_current_layer &&
          !canvas.fill_target_current && !canvas.fill_preserve_alpha && !canvas.fill_antialias,
          "Changed crop, wand, or fill settings were not applied");
    editor.crop_aspect.set(1);
    check(canvas.crop_aspect == 1, "Square crop ratio was not applied");
    editor.crop_aspect.set(0);
    check(canvas.crop_aspect == 0, "Unconstrained crop ratio was not applied");
    editor.polygon_sides.set(1);
    check(canvas.polygon_options.sides == 3, "Shape side minimum was not preserved");

    editor.selected_tool.set("pencil");
    editor.set_brush_size(23);
    editor.set_opacity(.35);
    check(canvas.brush_size == 23 && canvas.opacity == .35, "Pencil settings were not applied");
    editor.selected_tool.set("brush");
    check(canvas.brush_size == 11 && canvas.opacity == .7,
          "Returning to the brush did not restore its own settings");

    check(bool(editor.set_current_color({.8, .4, .2, 1})), "Cannot change foreground");
    check(canvas.color == 0xffcc6633U && canvas.gradient_stops[0].argb == canvas.color,
          "Foreground did not update the default gradient");
    check(editor.gradient_settings->set_color(0xff123456U), "Cannot set custom gradient start");
    check(bool(editor.set_current_color({.1, .2, .3, 1})), "Cannot change foreground again");
    check(canvas.color == 0xff1a334dU && canvas.gradient_stops[0].argb == 0xff123456U,
          "Changing foreground overwrote the custom gradient start");
    check(editor.gradient_settings->add() && editor.gradient_settings->set_position(.25),
          "Cannot change gradient stops");
    editor.gradient_settings->set_radial(false);
    editor.gradient_settings->set_repeat(false);
    editor.gradient_settings->cycle_blend();
    check(canvas.gradient_stops.size() == 3 && canvas.gradient_stops[1].position == .25 &&
          canvas.gradient_kind == iiSharedCanvas::GradientKind::Linear &&
          canvas.gradient_spread == iiSharedCanvas::GradientSpread::Pad &&
          canvas.gradient_blend == RasterBlendMode::Screen,
          "Gradient edits did not reach the canvas");

    // Settings may change during input without dropping samples or corrupting
    // the completed stroke and its undo/redo history.
    editor.set_brush_size(8);
    editor.set_opacity(1);
    canvas.settings = editor.parameters->brush();
    canvas.tool = "brush";
    canvas.resize({0, 0, 96, 64});
    canvas.fit();
    const auto before = pixels(artwork);
    ii::gui::WidgetInput event;
    event.kind = ii::gui::WidgetInputKind::pointer_down;
    event.device = ii::gui::PointerDevice::pen;
    event.buttons = 1;
    event.x = 10; event.y = 32; event.pressure = .4; event.tilt_x = 20; event.timestamp = 10;
    check(canvas.input(event).has_value() && artwork.stroke_active(), "Cannot begin pen stroke");
    event.kind = ii::gui::WidgetInputKind::pointer_move;
    event.x = 35; event.pressure = .8; event.tilt_y = 15; event.timestamp = 10.1;
    check(canvas.input(event).has_value(), "Cannot continue pen stroke");
    editor.set_brush_size(16);
    editor.set_opacity(.5);
    check(canvas.brush_size == 16 && canvas.opacity == .5, "Settings did not change during a stroke");
    event.kind = ii::gui::WidgetInputKind::pointer_up;
    event.x = 65; event.pressure = 1; event.timestamp = 10.2;
    check(canvas.input(event).has_value() && !artwork.stroke_active(), "Cannot finish pen stroke");
    const auto drawn = pixels(artwork);
    check(drawn != before && drawn[32 * 96 + 50] != before[32 * 96 + 50],
          "Pen stroke lost its moving or final pixels");
    check(artwork.undo() && pixels(artwork) == before, "Stroke undo changed the wrong pixels");
    check(artwork.redo() && pixels(artwork) == drawn, "Stroke redo did not restore the pixels");

    Artwork crop_artwork(directory.path());
    check(crop_artwork.create(96, 64), "Cannot create crop fixture");
    CanvasView crop_canvas(crop_artwork);
    EditorBackend crop_editor;
    CanvasToolBindings crop_bindings(crop_editor, crop_canvas);
    crop_editor.crop_aspect.set(1);
    crop_canvas.tool = "crop";
    ii::gui::WidgetInput crop_event;
    crop_event.buttons = 1;
    crop_event.kind = ii::gui::WidgetInputKind::pointer_down;
    crop_event.x = 10; crop_event.y = 10;
    check(crop_canvas.input(crop_event).has_value(), "Cannot begin crop gesture");
    crop_event.kind = ii::gui::WidgetInputKind::pointer_move;
    crop_event.x = 40; crop_event.y = 25;
    check(crop_canvas.input(crop_event).has_value(), "Cannot continue crop gesture");
    crop_event.kind = ii::gui::WidgetInputKind::pointer_up;
    check(crop_canvas.input(crop_event).has_value() && crop_canvas.apply_crop(),
          "Cannot apply bound crop settings");
    check(crop_artwork.document().extent == iiSharedCanvas::CanvasExtent{30, 30},
          "Crop did not use the bound aspect ratio");
    check(crop_artwork.undo() && crop_artwork.document().extent == iiSharedCanvas::CanvasExtent{96, 64} &&
          crop_artwork.redo() && crop_artwork.document().extent == iiSharedCanvas::CanvasExtent{30, 30},
          "Bound crop did not preserve undo/redo");

    // Replacing or destroying the binding must retire the previous observers.
    EditorBackend replacement;
    replacement.selection_feather.set(11);
    bindings = std::make_unique<CanvasToolBindings>(replacement, canvas);
    check(canvas.selection_feather == 11, "Replacement backend was not applied");
    editor.selection_feather.set(30);
    check(canvas.selection_feather == 11, "Retired backend still updates the canvas");
    replacement.selection_feather.set(14);
    check(canvas.selection_feather == 14, "Replacement backend stopped updating the canvas");
    bindings.reset();
    replacement.selection_feather.set(18);
    check(canvas.selection_feather == 14, "Destroyed binding still updates the canvas");

    EditorBackend retiring;
    auto retire_during_notification = retiring.current_color.observe(
        [&](const auto&, std::uint64_t) { bindings.reset(); });
    bindings = std::make_unique<CanvasToolBindings>(retiring, canvas);
    const auto last_color = canvas.color;
    check(bool(retiring.set_current_color({.7, .3, .2, 1})), "Cannot send retiring color change");
    check(!bindings && canvas.color == last_color,
          "Retired binding received a reentrant color notification");

    std::cout << "CANVAS_TOOL_BINDINGS_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
