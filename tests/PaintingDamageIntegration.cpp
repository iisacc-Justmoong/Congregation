#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <Native/CanvasTilePolicy.h>
#include <iiFileProviderNative.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void send(CanvasView& canvas, ii::gui::WidgetInputKind kind, double x, double y, double time) {
    const auto viewport = canvas.viewport();
    ii::gui::WidgetInput event;
    event.kind = kind;
    event.x = viewport.x + x * viewport.scale;
    event.y = viewport.y + y * viewport.scale;
    event.timestamp = time;
    event.pressure = 1;
    event.buttons = kind == ii::gui::WidgetInputKind::pointer_up ? 0 : 1;
    event.device = ii::gui::PointerDevice::pen;
    check(bool(canvas.input(event)), "canvas input failed");
}
std::shared_ptr<const ii::gui::RasterImage> tile(
    const std::shared_ptr<ii::gui::detail::WidgetState>& tree, const CanvasView& canvas,
    int column, int row) {
    const auto frame = take(tree->drawing_snapshot({1280, 960}));
    const auto viewport = canvas.viewport();
    const auto left = viewport.x + column * 512 * viewport.scale;
    const auto top = viewport.y + row * 512 * viewport.scale;
    const auto find = [&](const auto& images) -> std::shared_ptr<const ii::gui::RasterImage> {
        for (const auto& item : images)
            if (item.image && std::abs(item.bounds.x - left) < .01 &&
                std::abs(item.bounds.y - top) < .01 && item.image->width() == 512)
                return item.image;
        return {};
    };
    if (auto result = find(frame.images)) return result;
    for (const auto& overlay : frame.overlays)
        if (auto result = find(overlay.images)) return result;
    throw std::runtime_error("canvas tile missing from drawing snapshot");
}
void check_tile(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
                const CanvasView& canvas, const Artwork& artwork, int column, int row) {
    const int x = column * 512, y = row * 512;
    const int width = std::min(512, artwork.document().extent.width - x);
    const int height = std::min(512, artwork.document().extent.height - y);
    auto preview = artwork.selection_preview();
    std::optional<FrameBitmapOverlay> overlay;
    if (preview) overlay.emplace(FrameBitmapOverlay{preview->asset_id, preview->tiles});
    const auto overlays = overlay ? std::span<const FrameBitmapOverlay>(&*overlay, 1)
                                  : std::span<const FrameBitmapOverlay>{};
    auto expected = renderFrameRegion(artwork.document(), 0, {{x, y}, {width, height}},
                                      {width, height}, RasterSampling::Smooth, overlays);
    check(expected.ok(), "reference tile composition failed");
    const auto actual = tile(tree, canvas, column, row);
    check(actual->width() == unsigned(width) && actual->height() == unsigned(height),
          "cached tile has incorrect dimensions");
    const auto rgba = actual->rgba();
    for (std::size_t i = 0; i < expected.pixels.pixels.size(); ++i) {
        const auto pixel = expected.pixels.pixels[i];
        if (rgba[i * 4] != std::uint8_t(pixel >> 16) ||
            rgba[i * 4 + 1] != std::uint8_t(pixel >> 8) ||
            rgba[i * 4 + 2] != std::uint8_t(pixel) ||
            rgba[i * 4 + 3] != std::uint8_t(pixel >> 24))
            throw std::runtime_error("cached tile differs from full reference composition");
    }
}
void check_tile_layers(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
                       const CanvasView& canvas) {
    const std::array tiles{tile(tree, canvas, 0, 0), tile(tree, canvas, 1, 0),
                           tile(tree, canvas, 0, 1), tile(tree, canvas, 1, 1)};
    const auto frame = take(tree->drawing_snapshot({1280, 960}));
    int isolated = 0;
    for (const auto& layer : frame.overlays) {
        int count = 0;
        for (const auto& image : layer.images)
            count += std::ranges::find(tiles, image.image) != tiles.end();
        check(count <= 1, "canvas tiles share a foreground rasterization layer");
        isolated += count;
    }
    check(isolated == int(tiles.size()), "canvas tile is missing its own retained layer");
}
}

int main() try {
    check(canvas_tile_group_size(0, 2) == 1 && canvas_tile_group_size(4, 2) == 2 &&
          canvas_tile_group_size(5, 2) == 3 && canvas_tile_group_size(1000, 0) == 1000,
          "canvas tile group did not respect its layer budget");
    check(canvas_composition_batch_size(0) == 4 && canvas_composition_batch_size(4) == 4 &&
          canvas_composition_batch_size(8) == 2 && canvas_composition_batch_size(32) == 1,
          "canvas composition did not bound its layer tile working set");
    std::map<CanvasTileKey, int> cache_fixture;
    for (int column = 0; column < 120; ++column) cache_fixture[{0, column, 0, 0}] = column;
    trim_canvas_tile_cache(cache_fixture, {0, 0, 0, 3, 0, 0, 4}, 96);
    check(cache_fixture.size() == 96 && cache_fixture.contains({0, 0, 0, 0}) &&
          cache_fixture.contains({0, 3, 0, 0}), "cache eviction removed a visible tile");
    for (int column = 0; column < 120; ++column) cache_fixture[{0, column, 0, 0}] = column;
    trim_canvas_tile_cache(cache_fixture, {0, 0, 0, 119, 0, 0, 120}, 96);
    check(cache_fixture.size() == 120, "cache capacity discarded visible tiles");
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "test directory unavailable");
    Artwork artwork(directory.path());
    check(artwork.create(1024, 768), "canvas creation failed");

    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames(events);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
    auto surface = take(tree->absolute_stack({}, "canvas", {1280, 960}));
    auto paint_host = take(tree->content_host(surface, "paint-host", {1280, 960}));
    check(bool(paint_host.set_frame({0, 0, 1280, 960})), "paint host placement failed");
    (void)take(tree->drawing_snapshot({1280, 960}));
    CanvasView canvas(artwork);
    canvas.brush_size = 12;
    canvas.color = 0xff123456;
    canvas.attach(surface, paint_host);
    check(canvas.viewport().visible_tiles >= 4, "fixture must show multiple canvas tiles");
    check(canvas.viewport().composition_batches == 1,
          "initial visible tiles did not share a composition batch");
    check_tile_layers(tree, canvas);
    int forwarded = 0;
    check(bool(surface.set_input_handler([&](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
        if (event.kind == ii::gui::WidgetInputKind::pointer_move) ++forwarded;
        return true;
    })), "canvas input handler registration failed");
    ii::gui::WidgetInput hit_test;
    hit_test.kind = ii::gui::WidgetInputKind::pointer_move;
    hit_test.x = 100; hit_test.y = 100;
    check(bool(tree->input(hit_test)) && forwarded > 0,
          "retained paint layers intercepted canvas input");
    const auto untouched = tile(tree, canvas, 1, 1);
    const auto baseline = renderFrameRegion(artwork.document(), 0, {{80, 64}, {1, 1}}, {1, 1});
    check(baseline.ok(), "baseline render failed");
    const auto before = canvas.viewport().uploads;
    using K = ii::gui::WidgetInputKind;
    send(canvas, K::pointer_down, 64, 64, 1);
    send(canvas, K::pointer_move, 80, 64, 1.01);
    send(canvas, K::pointer_move, 96, 64, 1.02);
    check(canvas.viewport().uploads == before, "input rendered before the display frame");
    const auto now = ii::runtime::Clock::now();
    frames.step(now);
    check(canvas.viewport().uploads == before + 1, "one changed tile must be composited once");
    check(tile(tree, canvas, 1, 1) == untouched, "untouched tile was replaced");
    check_tile_layers(tree, canvas);
    check_tile(tree, canvas, artwork, 0, 0);
    auto live = renderFrameRegion(artwork.document(), 0, {{80, 64}, {1, 1}}, {1, 1});
    check(live.ok() && live.pixels.pixels[0] != baseline.pixels.pixels[0], "coalescing lost an input sample");
    const auto batches_before_idle = canvas.viewport().composition_batches;
    frames.step(now + std::chrono::milliseconds(17));
    check(canvas.viewport().uploads == before + 1, "idle frame recomposited the canvas");
    check(canvas.viewport().composition_batches == batches_before_idle,
          "idle frame recomposed cached tiles");
    send(canvas, K::pointer_move, 600, 64, 1.03);
    const auto batches_before_crossing = canvas.viewport().composition_batches;
    frames.step(now + std::chrono::milliseconds(34));
    check(canvas.viewport().uploads == before + 3, "cross-tile stroke did not update exactly two tiles");
    check(canvas.viewport().composition_batches == batches_before_crossing + 1,
          "changed tiles from the same stroke frame were not composed together");
    check_tile(tree, canvas, artwork, 1, 0);
    send(canvas, K::pointer_move, 620, 64, 1.04);
    frames.step(now + std::chrono::milliseconds(51));
    check(canvas.viewport().uploads == before + 4, "previously touched tiles were unnecessarily recomposited");
    check(tile(tree, canvas, 1, 1) == untouched, "unrelated tile changed during the stroke");
    send(canvas, K::pointer_up, 620, 64, 1.05);
    frames.step(now + std::chrono::milliseconds(68));
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    check(artwork.undo() && artwork.redo(), "incremental painting broke stroke history");

    auto tick = now + std::chrono::milliseconds(85);
    const auto next_frame = [&] { frames.step(tick); tick += std::chrono::milliseconds(17); };

    // A selection preview must keep all earlier parts of the stroke even
    // though only the latest input's damage is invalidated for presentation.
    check(artwork.create(1024, 768), "selected canvas creation failed");
    check(artwork.set_selection(SelectionGeometry::Rectangle, {0, 0}, {512, 768}),
          "selection creation failed");
    canvas.refresh();
    send(canvas, K::pointer_down, 100, 200, 10);
    send(canvas, K::pointer_move, 200, 200, 10.01);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    send(canvas, K::pointer_move, 600, 200, 10.02);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    send(canvas, K::pointer_move, 620, 200, 10.03);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    const auto full_preview = artwork.selection_preview();
    const auto clipped_preview = artwork.selection_preview(CanvasRegion{{0, 0}, {512, 512}});
    check(full_preview && clipped_preview && clipped_preview->tiles.size() < full_preview->tiles.size(),
          "selection preview rebuilt tiles outside the requested canvas region");
    send(canvas, K::pointer_up, 620, 200, 10.04);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    const auto selected_result = artwork.snapshot();
    check(artwork.undo() && artwork.redo() && artwork.snapshot() == selected_result,
          "selected incremental stroke failed undo/redo");

    // Chunked assets use a separate selection-preview path. A clipped
    // preview must agree with a full composition for both touched tiles.
    Document chunked;
    chunked.extent = {1024, 768};
    chunked.infiniteCanvas.chunkSize = 256;
    chunked.assets.emplace_back(ChunkedRasterAsset{"chunk-pixels", {}});
    chunked.layers.emplace_back(StaticBitmapLayer{{"chunk-layer", "Paint"}, StaticSource{"chunk-pixels"}});
    check(artwork.replace(std::move(chunked)), "chunked canvas creation failed");
    check(artwork.set_selection(SelectionGeometry::Rectangle, {0, 0}, {1024, 768}),
          "chunked selection creation failed");
    canvas.refresh();
    send(canvas, K::pointer_down, 100, 200, 15);
    send(canvas, K::pointer_move, 650, 200, 15.01);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    const auto full_chunk_preview = artwork.selection_preview();
    const auto clipped_chunk_preview = artwork.selection_preview(CanvasRegion{{0, 0}, {512, 512}});
    check(full_chunk_preview && clipped_chunk_preview &&
          clipped_chunk_preview->tiles.size() < full_chunk_preview->tiles.size(),
          "chunked selection preview rebuilt tiles outside the requested canvas region");
    send(canvas, K::pointer_move, 670, 200, 15.02);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    send(canvas, K::pointer_up, 670, 200, 15.03);
    next_frame();
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);

    // The editor's document-changed callback normally refreshes its canvas
    // synchronously. A brush commit must still invalidate only stroke tiles.
    check(artwork.create(1024, 768), "taper canvas creation failed");
    canvas.refresh();
    canvas.taper = {.15, .3, .08, .35};
    artwork.changed = [&] { canvas.refresh(); };
    send(canvas, K::pointer_down, 100, 200, 20);
    send(canvas, K::pointer_move, 650, 200, 20.01);
    next_frame();
    const auto before_release = canvas.viewport().uploads;
    send(canvas, K::pointer_up, 650, 200, 20.02);
    check(canvas.viewport().uploads == before_release, "stroke commit rendered synchronously");
    next_frame();
    check(canvas.viewport().uploads > before_release && canvas.viewport().uploads <= before_release + 2,
          "taper replay invalidated unrelated tiles");
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    const auto taper_result = artwork.snapshot();
    check(artwork.undo() && artwork.redo() && artwork.snapshot() == taper_result,
          "tapered incremental stroke failed undo/redo");
    check_tile(tree, canvas, artwork, 0, 0);
    canvas.taper = {};
    artwork.changed = {};

    // Cancel restores the two modified tiles and leaves the others cached.
    check(artwork.create(1024, 768), "cancel canvas creation failed");
    canvas.refresh();
    send(canvas, K::pointer_down, 100, 200, 30);
    send(canvas, K::pointer_move, 650, 200, 30.01);
    next_frame();
    const auto before_cancel = canvas.viewport().uploads;
    ii::gui::WidgetInput escape;
    escape.kind = K::key_down;
    escape.key = ii::gui::WidgetKey::escape;
    check(bool(canvas.input(escape)), "cancel input failed");
    next_frame();
    check(canvas.viewport().uploads == before_cancel + 2, "cancel invalidated unrelated tiles");
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);

    // A replacement pointer-down cancels an unfinished stroke. Its former
    // tiles and the new stroke's tiles must both be presented in the frame.
    check(artwork.create(1024, 768), "replacement stroke canvas creation failed");
    canvas.refresh();
    send(canvas, K::pointer_down, 100, 200, 35);
    next_frame();
    const auto before_replacement = canvas.viewport().uploads;
    send(canvas, K::pointer_down, 650, 200, 35.01);
    next_frame();
    check(canvas.viewport().uploads == before_replacement + 2,
          "replacement stroke did not restore its predecessor's tile");
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    send(canvas, K::pointer_up, 650, 200, 35.02);
    next_frame();

    // The editor reports local asset pixels; invalidation must map them through
    // the sampled layer transform before selecting visible document tiles.
    check(artwork.create(1024, 768), "transformed canvas creation failed");
    auto selected = std::ranges::find_if(artwork.document().layers, [&](const auto& layer) {
        return layerProperties(layer).id == artwork.selected();
    });
    check(selected != artwork.document().layers.end(), "selected layer unavailable");
    auto transform = layerProperties(*selected).transform;
    transform.m11 = 0; transform.m12 = 1;
    transform.m21 = -1; transform.m22 = 0;
    transform.translationX = 750;
    check(artwork.transform(transform), "layer rotation and translation failed");
    check(artwork.set_selection(SelectionGeometry::Rectangle, {512, 0}, {1024, 512}),
          "transformed layer selection failed");
    canvas.refresh();
    const auto unchanged_left = tile(tree, canvas, 0, 0);
    const auto before_transform_stroke = canvas.viewport().uploads;
    send(canvas, K::pointer_down, 650, 200, 40);
    send(canvas, K::pointer_move, 650, 220, 40.01);
    next_frame();
    check(canvas.viewport().uploads == before_transform_stroke + 1,
          "transformed stroke invalidated the wrong tiles");
    check(tile(tree, canvas, 0, 0) == unchanged_left, "unaffected transformed tile changed");
    check_tile(tree, canvas, artwork, 1, 0);
    send(canvas, K::pointer_up, 650, 220, 40.02);
    next_frame();
    check_tile(tree, canvas, artwork, 1, 0);

    // A nonlocal filter can affect pixels outside the source damage rectangle.
    // Until its influence is bounded, all visible tiles are invalidated.
    check(artwork.create(1024, 768), "filtered canvas creation failed");
    auto filtered = artwork.document();
    auto filtered_layer = std::ranges::find_if(filtered.layers, [&](const auto& layer) {
        return layerProperties(layer).id == artwork.selected();
    });
    check(filtered_layer != filtered.layers.end(), "filtered layer unavailable");
    LayerFilter blur;
    blur.name = "Gaussian blur";
    blur.operation = RasterEffect{RasterEffectKind::GaussianBlur};
    layerProperties(*filtered_layer).appearance.filters.push_back(blur);
    check(artwork.replace(std::move(filtered)), "filtered document failed");
    canvas.refresh();
    const auto before_filter = canvas.viewport().uploads;
    const auto batches_before_filter = canvas.viewport().composition_batches;
    send(canvas, K::pointer_down, 100, 100, 45);
    next_frame();
    check(canvas.viewport().uploads == before_filter + canvas.viewport().visible_tiles,
          "nonlocal filter did not invalidate all visible tiles");
    check(canvas.viewport().composition_batches == batches_before_filter + 1,
          "filtered visible tiles did not share the layer preparation");
    check_tile(tree, canvas, artwork, 0, 0);
    check_tile(tree, canvas, artwork, 1, 0);
    check_tile(tree, canvas, artwork, 0, 1);
    check_tile(tree, canvas, artwork, 1, 1);
    send(canvas, K::pointer_up, 100, 100, 45.01);
    next_frame();

    // Disabled and zero-opacity filters have no visual influence and must not
    // force every visible tile to be recomposed on an unrelated brush dab.
    for (bool disabled : {true, false}) {
        auto inactive = artwork.document();
        auto layer = std::ranges::find_if(inactive.layers, [&](const auto& candidate) {
            return layerProperties(candidate).id == artwork.selected();
        });
        check(layer != inactive.layers.end(), "inactive filter layer unavailable");
        auto& filter = layerProperties(*layer).appearance.filters.front();
        filter.enabled = !disabled;
        filter.opacity = disabled ? 1 : 0;
        check(artwork.replace(std::move(inactive)), "inactive filter document failed");
        canvas.refresh();
        const auto stable = tile(tree, canvas, 1, 1);
        const auto previous_uploads = canvas.viewport().uploads;
        send(canvas, K::pointer_down, 100, 100, disabled ? 46 : 47);
        next_frame();
        check(canvas.viewport().uploads == previous_uploads + 1,
              "inactive filter invalidated unrelated tiles");
        check(tile(tree, canvas, 1, 1) == stable,
              "inactive filter replaced an unaffected tile");
        check_tile(tree, canvas, artwork, 0, 0);
        send(canvas, K::pointer_up, 100, 100, disabled ? 46.01 : 47.01);
        next_frame();
    }

    // Zoom can increase the number of visible tiles and resize their retained
    // layer pool without merging tiles back into a single foreground image.
    check(artwork.create(1024, 768), "zoom canvas creation failed");
    canvas.refresh();
    const auto zoom_batches_before = canvas.viewport().composition_batches;
    canvas.zoom(2);
    check(canvas.viewport().visible_tiles > 4, "zoom did not expand the visible tile pool");
    const auto zoom_batch_size = canvas_composition_batch_size(artwork.document().layers.size());
    check(canvas.viewport().composition_batches == zoom_batches_before +
          (canvas.viewport().visible_tiles + zoom_batch_size - 1) / zoom_batch_size,
          "zoomed tiles exceeded the composition batch limit or skipped the final batch");
    const auto zoomed_frame = take(tree->drawing_snapshot({1280, 960}));
    std::size_t zoomed_tiles = 0;
    for (const auto& layer : zoomed_frame.overlays) {
        if (layer.images.empty()) continue;
        check(layer.images.size() == 1, "zoom merged retained tile layers");
        ++zoomed_tiles;
    }
    check(zoomed_tiles == canvas.viewport().visible_tiles, "zoom lost a retained tile layer");
    canvas.resize({0, 0, 800, 600});
    check(std::abs(take(paint_host.snapshot()).bounds.width - 800) < .01,
          "canvas resize did not resize the paint host");
    canvas.resize({0, 0, 1280, 960});
    canvas.fit();
    check_tile_layers(tree, canvas);

    // A small layer budget simulates an unusually large viewport without
    // allocating hundreds of full-size image tiles in the test.
    {
        Artwork limited_artwork(directory.path());
        check(limited_artwork.create(1024, 768), "limited canvas creation failed");
        auto limited_tree = std::make_shared<ii::gui::detail::WidgetState>(
            [](ii::runtime::Task task) { task(); });
        auto limited_surface = take(limited_tree->absolute_stack({}, "limited", {1280, 960}));
        auto limited_host = take(limited_tree->content_host(limited_surface, "limited-host", {1280, 960}));
        check(bool(limited_host.set_frame({0, 0, 1280, 960})), "limited host placement failed");
        (void)take(limited_tree->drawing_snapshot({1280, 960}));
        CanvasView limited(limited_artwork, 2);
        limited.brush_size = 12;
        limited.attach(limited_surface, limited_host);
        const auto grouped = take(limited_tree->drawing_snapshot({1280, 960}));
        std::size_t groups = 0, images = 0;
        for (const auto& layer : grouped.overlays) if (!layer.images.empty()) {
            check(layer.images.size() <= 2, "tile group exceeded its calculated size");
            ++groups; images += layer.images.size();
        }
        check(groups == 2 && images == 4, "visible tiles did not fit the display layer budget");
        const auto stable = tile(limited_tree, limited, 1, 1);
        send(limited, K::pointer_down, 100, 100, 48);
        check(tile(limited_tree, limited, 1, 1) == stable, "grouping replaced an unaffected tile");
        check_tile(limited_tree, limited, limited_artwork, 0, 0);
        check_tile(limited_tree, limited, limited_artwork, 1, 1);
        send(limited, K::pointer_up, 100, 100, 48.01);
    }

    // A headless host without a frame source must still show each change.
    {
        Artwork fallback_artwork(directory.path());
        check(fallback_artwork.create(1024, 768), "fallback canvas creation failed");
        auto fallback_tree = std::make_shared<ii::gui::detail::WidgetState>(
            [](ii::runtime::Task task) { task(); });
        auto fallback_surface = take(fallback_tree->absolute_stack({}, "fallback", {1280, 960}));
        (void)take(fallback_tree->drawing_snapshot({1280, 960}));
        CanvasView fallback(fallback_artwork);
        fallback.brush_size = 12;
        fallback.attach(fallback_surface);
        const auto fallback_before = fallback.viewport().uploads;
        send(fallback, K::pointer_down, 100, 100, 50);
        check(fallback.viewport().uploads == fallback_before + 1,
              "missing frame source did not synchronously render the changed tile");
        check_tile(fallback_tree, fallback, fallback_artwork, 0, 0);
    }

    // Destroying a view with a queued frame must retire its callback safely.
    {
        Artwork pending_artwork(directory.path());
        check(pending_artwork.create(1024, 768), "pending canvas creation failed");
        ii::runtime::EventLoop pending_events;
        ii::runtime::FrameLoop pending_frames(pending_events);
        ii::gui::detail::WidgetFrameHooks pending_hooks;
        pending_hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &pending_frames; };
        auto pending_tree = std::make_shared<ii::gui::detail::WidgetState>(
            [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(pending_hooks));
        auto pending_surface = take(pending_tree->absolute_stack({}, "pending", {1280, 960}));
        (void)take(pending_tree->drawing_snapshot({1280, 960}));
        auto pending = std::make_unique<CanvasView>(pending_artwork);
        pending->brush_size = 12;
        pending->attach(pending_surface);
        send(*pending, K::pointer_down, 100, 100, 60);
        pending.reset();
        pending_frames.step(ii::runtime::Clock::now());
    }
    std::cout << "PAINTING_DAMAGE_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
