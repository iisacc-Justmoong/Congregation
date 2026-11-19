#include <Canvas/Artwork.h>
#include <Canvas/CanvasDefinition.h>
#include <Canvas/CanvasView.h>
#include <Application/CanvasToolBindings.h>
#include <Application/MainWindow.h>
#include <Tools/ToolSettings.h>
#include <iiFileProvider>
#include <iiSDKCore>
#include <iiSharedCanvas>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace congregation::native;

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& node) { return node.id.ends_with(suffix); });
    if (found == nodes.end()) throw std::runtime_error("Missing widget: " + std::string(suffix));
    return take(tree->find(found->id));
}
unsigned alpha_at(const Artwork& artwork, int x, int y) {
    const auto& doc = artwork.document();
    for (const auto& layer : doc.layers) {
        if (iiSharedCanvas::layerProperties(layer).id != artwork.selected()) continue;
        const auto* source = iiSharedCanvas::staticLayerSource(layer);
        const auto* asset = source ? iiSharedCanvas::findRasterAsset(doc, source->assetId) : nullptr;
        check(asset != nullptr, "paint bitmap unavailable");
        return asset->pixels.pixels[std::size_t(y) * asset->pixels.width + x] >> 24;
    }
    throw std::runtime_error("paint layer unavailable");
}
std::vector<std::uint8_t> displayed_canvas(const std::shared_ptr<ii::gui::detail::WidgetState>& tree) {
    const auto frame = take(tree->drawing_snapshot({2100, 1180}));
    check(ii::gui::valid_canvas(frame), "native renderer rejected the canvas scene");
    auto image = [](const auto& images) -> std::shared_ptr<const ii::gui::RasterImage> {
        const auto it = std::ranges::find_if(images, [](const auto& item) {
            return item.image && item.image->width() == 512 && item.image->height() == 256;
        });
        return it == images.end() ? nullptr : it->image;
    };
    auto raster = image(frame.images);
    for (const auto& layer : frame.overlays) if (!raster) raster = image(layer.images);
    check(bool(raster), "canvas image is missing from the native scene");
    const auto pixels = raster->rgba();
    return {pixels.begin(), pixels.end()};
}
}

int main(int argc, char** argv) try {
    check(argc == 2, "expected Figma resource directory");
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "temporary directory unavailable");
    Artwork artwork(directory.path());
    CanvasDefinition definition;
    definition.width = definition.pixel_width = 512;
    definition.height = definition.pixel_height = 256;
    definition.background = "White";
    check(artwork.create(definition), "cannot create test canvas");
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames(events);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
    auto root = take(tree->absolute_stack({}, "outer", {2100, 1180}));
    auto host = take(tree->content_host(root, "host", {1999, 1080}));
    good(host.set_frame({41, 53, 1999, 1080}));
    MainWindow window;
    window.set_assets(argv[1]);
    congregation::ui::EditorBackend* model = nullptr;
    good(host.set_content([&](auto& builder) {
        return window.build_content(builder, {}, [&](auto& scope) { model = &scope.backend_0; });
    }));
    check(model != nullptr, "editor backend missing");
    model->licensed.set(true);
    model->needs_license.set(false);
    (void)take(tree->drawing_snapshot({2100, 1180}));
    // The workspace and outer LVUI menu are separate content generations.
    // Context input must reach the menu without replacing the layer handler.
    auto editor_root = widget(tree, "/root");
    auto workspace = widget(tree, "/workspace");
    auto shape_menu = widget(tree, "/shape-menu");
    auto color_actions = widget(tree, "/color-actions");
    auto layer_menu = widget(tree, "/layer-menu");
    good(bind_scoped_context_menus(workspace, editor_root,
         {{"layers", layer_menu}}));
    good(bind_scoped_context_menus(widget(tree, "/tool-rail"), editor_root,
         {{"shape", shape_menu}}));
    good(bind_scoped_context_menus(widget(tree, "/toolbar"), editor_root,
         {{"brush-color", color_actions}}));
    for (const auto& [target, popup] : std::initializer_list<std::pair<std::string_view, ii::gui::Widget>>{
             {"/shape", shape_menu}, {"/brush-color", color_actions}, {"/layers", layer_menu}}) {
        const auto rect = take(widget(tree, target).snapshot()).bounds;
        good(tree->input({.kind = ii::gui::WidgetInputKind::context_request,
                          .x = rect.x + rect.width / 2, .y = rect.y + rect.height / 2}));
        check(take(popup.snapshot()).opened, "workspace context menu did not open");
        good(popup.close_popup());
    }
    auto color_anchor = widget(tree, "/brush-color");
    auto color_popup = widget(tree, "/color-popup");
    good(open_scoped_popup_for(editor_root, color_popup, color_anchor,
                               0, take(color_anchor.snapshot()).bounds.height));
    check(take(color_popup.snapshot()).opened, "workspace color popup did not open");
    good(color_popup.close_popup());
    auto new_layer_button = widget(tree, "/add-layer");
    auto new_layer_menu = widget(tree, "/new-layer-menu");
    int menu_requests = 0;
    model->perform = [&](congregation::ui::Action action) {
        if (action == congregation::ui::Action::open_new_layer_menu) {
            ++menu_requests;
            good(open_new_layer_popup(editor_root, new_layer_menu, new_layer_button));
        }
    };
    good(new_layer_button.activate());
    const auto layer_anchor_bounds = take(new_layer_button.snapshot()).bounds;
    const auto layer_popup_bounds = take(new_layer_menu.snapshot()).bounds;
    check(menu_requests == 1 && take(new_layer_menu.snapshot()).opened &&
          layer_popup_bounds.height > 0 && layer_popup_bounds.y + layer_popup_bounds.height <= layer_anchor_bounds.y + .01,
          "New layer menu did not open above its cross-generation timeline anchor");
    good(new_layer_menu.close_popup());
    // The size +/- controls were replaced by the declarative Tool panel.
    // Exercise the current tool buttons without assuming that separately
    // mounted panel is part of this MainWindow-only fixture.
    const auto pencil = widget(tree, "/tool-pencil");
    const auto brush_button = widget(tree, "/brush");
    const auto button_press = [&](const ii::gui::Widget& button) {
        const auto rect = take(button.snapshot()).bounds;
        const auto x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
        good(tree->input({.kind = ii::gui::WidgetInputKind::pointer_down, .x = x, .y = y, .buttons = 1}));
        good(tree->input({.kind = ii::gui::WidgetInputKind::pointer_up, .x = x, .y = y}));
    };
    for (int i = 0; i < 32; ++i) {
        button_press(pencil);
        check(model->selected_tool.get() == "pencil", "rapid tool selection did not choose pencil");
        button_press(brush_button);
        check(model->selected_tool.get() == "brush", "rapid tool selection did not restore brush");
    }
    auto surface = widget(tree, "/canvas");
    auto paint_host = widget(tree, "/canvas-paint-layers");
    CanvasView canvas(artwork);
    good(model->set_current_color({.36, .18, 1, 1}));
    CanvasToolBindings tool_bindings(*model, canvas);
    model->set_brush_size(24);
    check(canvas.brush_size == 24, "Mounted tool setting did not update the canvas");
    model->set_brush_size(64);
    ToolSettings settings;
    settings.set(ToolParameter::size, model->brush_size.get());
    settings.set(ToolParameter::opacity, model->opacity.get() * 100);
    settings.set(ToolParameter::flow, model->flow.get() * 100);
    settings.set(ToolParameter::hardness, model->hardness.get() * 100);
    settings.set(ToolParameter::spacing, model->spacing.get() * 100);
    settings.set(ToolParameter::stabilization, model->stabilizer.get() * 100);
    canvas.brush_size = model->brush_size.get();
    canvas.settings = settings.brush();
    canvas.taper = {settings.value(ToolParameter::start) / 100., settings.value(ToolParameter::end) / 100.,
                    settings.value(ToolParameter::minimum_size) / 100., settings.value(ToolParameter::taper) / 100.};
    canvas.settings.spacing = model->spacing_pixels.get();
    canvas.settings.pressureToOpacityEnabled = model->pressure_enabled.get();
    auto& engine = *canvas.settings.engineState;
    engine.dynamics.pressureToOpacity = 0;
    engine.dynamics.opacityResponse.enabled = model->pressure_enabled.get();
    engine.dynamics.opacityResponse.pressure = {true, model->pressure_min.get(),
                                               model->pressure_mid.get(), model->pressure_max.get()};
    canvas.attach(surface, paint_host);
    // This fixture inspects the single 512x256 raster tile directly. Fit can
    // zoom above 200%, where the current renderer correctly uses smaller tiles.
    canvas.zoom(1.0 / canvas.viewport().scale);
    check(std::abs(canvas.viewport().scale - 1) < 1e-9, "fixture zoom was not normalized");
    const auto baseline = displayed_canvas(tree);
    const auto bounds = take(surface.snapshot()).bounds;
    const auto viewport = canvas.viewport();
    check(bounds.x > 41 && bounds.y > 53, "nested canvas fixture has no offset");
    ii::gui::WidgetInput injected;
    injected.x = bounds.x + 17; injected.y = bounds.y + 29;
    auto converted = canvas.localize_input(injected);
    check(converted.x == 17 && converted.y == 29, "window-coordinate input fallback failed");
    std::vector<ii::gui::WidgetInputKind> received;
    bool inject_nested_burst = false;
    std::function<void()> nested_burst;
    model->input = [&](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
        received.push_back(event.kind);
        auto result = canvas.input(canvas.localize_input(event));
        model->zoom.set(ii::core::format_decimal(std::round(canvas.viewport().scale * 100)) + "%");
        if (inject_nested_burst && event.kind == ii::gui::WidgetInputKind::pointer_move) {
            inject_nested_burst = false;
            nested_burst();
        }
        return result;
    };
    auto send = [&](ii::gui::WidgetInputKind kind, double x, double y, double time,
                    ii::gui::PointerDevice device = ii::gui::PointerDevice::mouse, double pressure = 1,
                    bool alt = false) {
        ii::gui::WidgetInput event;
        event.kind = kind;
        event.x = bounds.x + viewport.x + x * viewport.scale;
        event.y = bounds.y + viewport.y + y * viewport.scale;
        event.buttons = kind == ii::gui::WidgetInputKind::pointer_up ? 0 : 1;
        event.device = device;
        event.pressure = pressure;
        event.alt = alt;
        event.timestamp = time;
        good(tree->input(event));
    };
    using K = ii::gui::WidgetInputKind;
    // Hover, sample and release through the same retained view and input
    // router as a real stroke. No test interaction may modify the artwork.
    const auto pointer_at = [&](double x, double y) -> std::optional<ii::gui::ImageRun> {
        const auto frame = take(tree->drawing_snapshot({2100, 1180}));
        const auto find = [&](const auto& images) -> std::optional<ii::gui::ImageRun> {
            for (const auto& run : images) {
                if (run.image && run.bounds.width <= 80 && run.bounds.height <= 80 &&
                    std::abs(run.bounds.x + run.bounds.width / 2 - bounds.x - viewport.x - x) < .01 &&
                    std::abs(run.bounds.y + run.bounds.height / 2 - bounds.y - viewport.y - y) < .01)
                    return run;
            }
            return std::nullopt;
        };
        if (auto run = find(frame.images)) return run;
        for (const auto& overlay : frame.overlays) if (auto run = find(overlay.images)) return run;
        return std::nullopt;
    };
    const auto pointer_revision = artwork.revision();
    const auto pointer_compositions = canvas.viewport().composition_batches;
    const auto foreground = canvas.color;
    send(K::pointer_move, 160, 70, 98);
    const auto tip = pointer_at(160, 70);
    check(tip && std::abs(tip->bounds.width - 68) < .01, "brush cursor does not show its 64 px footprint");
    send(K::pointer_move, 176, 74, 98.1);
    const auto moved_tip = pointer_at(176, 74);
    check(moved_tip && moved_tip->image == tip->image, "moving the pointer rasterized its brush tip again");
    send(K::pointer_down, 176, 74, 98.2, ii::gui::PointerDevice::mouse, 1, true);
    const auto ring = pointer_at(176, 74);
    check(ring && ring->bounds.width == 72 && canvas.color == 0xffffffff, "eyedropper did not show its sampling ring");
    const auto rgba = [&](int y) {
        const auto& image = *ring->image;
        const auto offset = (std::size_t(y) * image.width() + image.width() / 2) * 4;
        const auto pixels = image.rgba();
        return (std::uint32_t(pixels[offset + 3]) << 24) | (std::uint32_t(pixels[offset]) << 16) |
               (std::uint32_t(pixels[offset + 1]) << 8) | pixels[offset + 2];
    };
    const auto middle_row = int(ring->image->height() / 2);
    check(rgba(middle_row / 6) == 0xffffffff && rgba(middle_row * 11 / 6) == foreground &&
          (rgba(middle_row) >> 24) == 0, "sampling ring did not compare the new and previous colors around a clear center");
    send(K::pointer_up, 176, 74, 98.3);
    const auto restored_tip = pointer_at(176, 74);
    check(restored_tip && restored_tip->image == tip->image, "brush cursor did not resume after sampling");
    canvas.color = foreground;
    canvas.tool = "eyedropper";
    send(K::pointer_down, 176, 74, 98.4);
    const auto standalone_ring = pointer_at(176, 74);
    check(standalone_ring && standalone_ring->image == ring->image && canvas.tool == "brush",
          "the eyedropper tool did not show the same color comparison");
    send(K::pointer_up, 176, 74, 98.5);
    canvas.color = foreground;

    auto& shape = canvas.settings.engineState->rasterizer.shape;
    const auto round_tip = shape;
    shape.kind = BrushTipShape::Square;
    shape.angleRadians = std::numbers::pi / 2;
    shape.roundness = .5;
    canvas.refresh_pointer();
    const auto rotated_tip = pointer_at(176, 74);
    check(rotated_tip && std::abs(rotated_tip->bounds.width - 36) < .01 &&
          std::abs(rotated_tip->bounds.height - 68) < .01, "the cursor lost the tip's roundness or rotation");
    shape = round_tip;
    model->set_brush_size(32);
    const auto smaller_tip = pointer_at(176, 74);
    check(smaller_tip && std::abs(smaller_tip->bounds.width - 36) < .01,
          "the cursor did not respond to Size with a stationary pointer");
    model->set_brush_size(64);
    check(!artwork.stroke_active() && artwork.revision() == pointer_revision &&
          canvas.viewport().composition_batches == pointer_compositions,
          "pointer feedback modified or recomposited the canvas");

    canvas.zoom(.5, viewport.x + 176, viewport.y + 74);
    const auto zoomed_tip = pointer_at(176, 74);
    check(zoomed_tip && std::abs(zoomed_tip->bounds.width - 36) < .01,
          "the brush cursor did not scale with viewport zoom");
    canvas.zoom(2, viewport.x + 176, viewport.y + 74);
    good(tree->input({.kind = K::pointer_leave}));
    check(!pointer_at(176, 74), "brush cursor remained after pointer exit");
    received.clear();

    std::vector<congregation::native::DocumentChange> changes;
    artwork.document_changed = [&](const auto& change) {
        changes.push_back(change);
        window.apply_document_change(artwork.document(), artwork.selected(), change);
    };
    window.refresh_document(artwork.document(), artwork.selected());
    const auto preview_has_pixels = [&] {
        const auto frame = take(tree->drawing_snapshot({2100, 1180}));
        const auto row = take(widget(tree, "/layers").snapshot()).bounds;
        bool has_pixels = false;
        const auto inspect = [&](const auto& images) {
            for (const auto& item : images) {
                if (!item.image || item.image->width() != 18 || item.image->height() != 18 ||
                    std::abs(item.bounds.x - row.x - 32) > .01 || std::abs(item.bounds.y - row.y - 2) > .01) continue;
                const auto rgba = item.image->rgba();
                for (std::size_t index = 3; index < rgba.size(); index += 4) has_pixels |= rgba[index] != 0;
            }
        };
        inspect(frame.images);
        for (const auto& overlay : frame.overlays) inspect(overlay.images);
        return has_pixels;
    };
    check(!preview_has_pixels(), "empty paint layer must have an empty thumbnail");
    send(K::pointer_down, 80, 100, 100);
    check(artwork.stroke_active(), "pointer down did not start a stroke");
    const auto now = ii::runtime::Clock::now();
    frames.step(now);
    send(K::pointer_move, 180, 100, 100.04);
    check(artwork.stroke_active(), "pointer move canceled the stroke");
    check(alpha_at(artwork, 140, 100) > 40, "live stroke lost the middle of the drag");
    frames.step(now + std::chrono::milliseconds(17));
    const auto live = displayed_canvas(tree);
    const auto middle = (std::size_t(100) * 512 + 140) * 4;
    check(!std::equal(live.begin() + middle, live.begin() + middle + 4, baseline.begin() + middle),
          "live stroke pixels were not presented by the nested frame loop");
    send(K::pointer_move, 260, 100, 100.08);
    send(K::pointer_up, 340, 100, 100.12);
    frames.step(now + std::chrono::milliseconds(34));
    check(!artwork.stroke_active(), "pointer release did not finish the stroke");
    check(std::ranges::count(received, K::pointer_down) == 1 &&
          std::ranges::count(received, K::pointer_move) == 2 &&
          std::ranges::count(received, K::pointer_up) == 1,
          "nested workspace lost a pointer event");
    check(alpha_at(artwork, 80, 100) > 40 && alpha_at(artwork, 140, 100) > 40 &&
          alpha_at(artwork, 220, 100) > 40 && alpha_at(artwork, 300, 100) > 40 &&
          alpha_at(artwork, 340, 100) > 40,
          "committed stroke lost pixels along the drag");
    check(preview_has_pixels(), "committed brush stroke did not update the layer thumbnail");
    check(!changes.empty() && changes.back().kind == congregation::native::DocumentChangeKind::pixels &&
          changes.back().layer == artwork.selected() && !changes.back().asset.empty(),
          "Stroke must publish the affected layer and asset");
    check(artwork.undo() && alpha_at(artwork, 140, 100) == 0 && !preview_has_pixels(),
          "stroke undo did not clear the layer thumbnail");
    check(changes.back().kind == congregation::native::DocumentChangeKind::pixels,
          "Stroke undo lost its pixel change scope");
    check(artwork.redo() && alpha_at(artwork, 140, 100) > 40 && preview_has_pixels(),
          "stroke redo did not restore the layer thumbnail");
    check(changes.back().kind == congregation::native::DocumentChangeKind::pixels,
          "Stroke redo lost its pixel change scope");
    using D = ii::gui::PointerDevice;
    send(K::pointer_down, 80, 200, 101, D::pen, .5);
    send(K::pointer_move, 180, 200, 101.04, D::pen, .7);
    frames.step(now + std::chrono::milliseconds(51));
    check(alpha_at(artwork, 140, 200) > 20, "pen pressure lost the live moving stroke");
    send(K::pointer_move, 260, 200, 101.08, D::pen, .9);
    send(K::pointer_up, 340, 200, 101.12, D::pen, .8);
    frames.step(now + std::chrono::milliseconds(68));
    check(alpha_at(artwork, 220, 200) > 20 && alpha_at(artwork, 300, 200) > 20 &&
          alpha_at(artwork, 340, 200) > 20,
          "pen pressure lost the committed stroke");
    check(artwork.undo() && alpha_at(artwork, 220, 200) == 0 &&
          artwork.redo() && alpha_at(artwork, 220, 200) > 20,
          "pen stroke undo/redo failed");
    const auto outside = (bounds.width - viewport.x + 40) / viewport.scale;
    const auto before_outside = received.size();
    send(K::pointer_down, 50, 25, 102);
    send(K::pointer_move, 120, 25, 102.04);
    send(K::pointer_move, outside, 25, 102.08);
    send(K::pointer_up, outside, 25, 102.12);
    frames.step(now + std::chrono::milliseconds(85));
    check(!artwork.stroke_active() && received.size() == before_outside + 4 &&
          alpha_at(artwork, 100, 25) > 40,
          "pointer capture lost a drag or release outside the canvas");
    check(artwork.create(512, 256), "cannot create touch canvas");
    canvas.initialize_document();
    const auto touch_baseline = displayed_canvas(tree);
    send(K::pointer_down, 80, 128, 103, D::touch, 1);
    send(K::pointer_move, 180, 128, 103.04, D::touch, 1);
    frames.step(now + std::chrono::milliseconds(102));
    check(alpha_at(artwork, 140, 128) > 40, "touch input lost the live moving stroke");
    const auto touch_live = displayed_canvas(tree);
    const auto touch_middle = (std::size_t(128) * 512 + 140) * 4;
    check(!std::equal(touch_live.begin() + touch_middle, touch_live.begin() + touch_middle + 4,
                      touch_baseline.begin() + touch_middle), "touch stroke was not presented");
    send(K::pointer_up, 340, 128, 103.08, D::touch, 1);
    frames.step(now + std::chrono::milliseconds(119));
    check(alpha_at(artwork, 260, 128) > 40 && artwork.undo() &&
          alpha_at(artwork, 260, 128) == 0 && artwork.redo() &&
          alpha_at(artwork, 260, 128) > 40, "touch stroke or history failed");

    // Exercise the mounted LVUI listener, artwork and frame loop with a burst
    // of native-style samples, rather than calling CanvasView directly.
    const auto product_brush = canvas.settings;
    const auto product_taper = canvas.taper;
    const auto product_brush_size = canvas.brush_size;
    check(artwork.create(512, 256), "cannot create burst canvas");
    canvas.settings = {};
    canvas.brush_size = 6;
    canvas.taper = {};
    canvas.initialize_document();
    send(K::pointer_down, 40, 80, 200, D::mouse, 1);
    for (int i = 1; i <= 256; ++i) {
        send(K::pointer_move, 40 + 400. * i / 256., 80 + 5 * std::sin(i * .04),
             200 + i * .008, D::mouse, 1);
        if (i % 16 == 0) {
            frames.step(now + std::chrono::milliseconds(119 + i));
            (void)displayed_canvas(tree);
        }
    }
    send(K::pointer_up, 440, 80, 202.1, D::mouse, 1);
    frames.step(now + std::chrono::milliseconds(400));
    check(!artwork.stroke_active(), "rapid input left a stroke active");
    check(alpha_at(artwork, 240, 75) > 0, "rapid input lost its moving pixels");
    check(artwork.undo() && artwork.redo(), "rapid input lost its stroke history");

    // The default streaming brush remains correct through a burst of samples.
    check(artwork.create(512, 256), "cannot create default-brush burst canvas");
    canvas.settings = product_brush;
    canvas.brush_size = product_brush_size;
    canvas.taper = product_taper;
    canvas.initialize_document();
    send(K::pointer_down, 40, 128, 250);
    for (int i = 1; i <= 96; ++i) {
        send(K::pointer_move, 40 + 400. * i / 96., 128 + 6 * std::sin(i * .08),
             250 + i * .008);
        if (i % 24 == 0) {
            frames.step(now + std::chrono::milliseconds(500 + i));
            (void)displayed_canvas(tree);
        }
    }
    send(K::pointer_up, 440, 128, 250.8);
    frames.step(now + std::chrono::milliseconds(620));
    check(!artwork.stroke_active() && alpha_at(artwork, 240, 128) > 0 &&
          artwork.undo() && artwork.redo(), "default tapered brush failed after rapid input");

    check(artwork.create(512, 256), "cannot create reentrant burst canvas");
    canvas.settings = {};
    canvas.brush_size = 6;
    canvas.taper = {};
    canvas.initialize_document();
    const auto burst_start = received.size();
    nested_burst = [&] {
        for (int i = 0; i < 300; ++i)
            send(K::pointer_move, 100 + i % 100, 120, 301 + i * .001);
        send(K::pointer_up, 200, 120, 301.5);
    };
    send(K::pointer_down, 80, 120, 300);
    inject_nested_burst = true;
    send(K::pointer_move, 90, 120, 300.01);
    check(!artwork.stroke_active() &&
          std::ranges::count(received.begin() + burst_start, received.end(), K::focus_lost) == 1,
          "reentrant input overflow did not cancel the stroke safely");
    send(K::pointer_down, 80, 180, 302);
    send(K::pointer_move, 180, 180, 302.1);
    send(K::pointer_up, 240, 180, 302.2);
    frames.step(now + std::chrono::milliseconds(637));
    check(!artwork.stroke_active() && alpha_at(artwork, 140, 180) > 0,
          "canvas did not recover after an input burst");
    std::cout << "NATIVE_CANVAS_STROKE_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
