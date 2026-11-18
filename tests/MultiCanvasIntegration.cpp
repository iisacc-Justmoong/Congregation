#include <Canvas/Artwork.h>
#include <Canvas/CanvasView.h>
#include <iiFileProvider>
#include <iiSharedCanvas>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
CanvasDefinition definition(int width, int height, std::string background, std::string name) {
    CanvasDefinition result;
    result.width = result.pixel_width = width;
    result.height = result.pixel_height = height;
    result.background = std::move(background);
    result.name = std::move(name);
    return result;
}
std::uint32_t pixel(const Artwork& art, int x, int y) {
    const auto result = renderFrameRegion(art.document(), 0, {{x, y}, {1, 1}}, {1, 1});
    check(result.ok(), "cannot render document pixel");
    return result.pixels.pixels.front();
}
void paint(CanvasView& view, double x1, double y1, double x2, double y2) {
    using Kind = ii::gui::WidgetInputKind;
    auto send = [&](Kind kind, double x, double y, double time) {
        ii::gui::WidgetInput event;
        event.kind = kind; event.x = x; event.y = y; event.timestamp = time;
        event.device = ii::gui::PointerDevice::pen; event.pressure = 1;
        event.buttons = kind == Kind::pointer_up ? 0 : 1;
        const auto accepted = view.input(event);
        check(accepted && *accepted, "canvas rejected pen event");
    };
    send(Kind::pointer_down, x1, y1, 1);
    send(Kind::pointer_move, x2, y2, 1.1);
    send(Kind::pointer_up, x2, y2, 1.2);
}
}

int main(int argc, char** argv) try {
    check(argc == 2, "test resources directory is required");
    iiFileProvider::native::TemporaryDirectory temporary(argv[1]);
    check(temporary.isValid(), "cannot create test resources directory");
    constexpr std::size_t budget = 768 * 1024;
    Artwork art(temporary.path(), budget);
    check(art.create(definition(512, 512, "White", "Original")), "original creation failed");
    auto noisy = art.document();
    auto* original_asset = findRasterAsset(noisy, "paint");
    check(original_asset, "original bitmap is unavailable");
    std::uint32_t random = 9;
    for (auto& value : original_asset->pixels.pixels) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        value = 0xff000000U | (random & 0x00ffffffU);
    }
    check(art.replace(std::move(noisy)), "cannot prepare large canvas fixture");
    CanvasView view(art);
    view.brush_size = 10; view.color = 0xffff0000U;
    const auto unpainted = pixel(art, 30, 20);
    paint(view, 20, 20, 45, 20);
    const auto original = pixel(art, 30, 20);
    check(original != unpainted, "original canvas was not painted");
    if (!art.set_selection(SelectionGeometry::Rectangle, {10, 10}, {50, 50}))
        throw std::runtime_error("selection failed: " + art.error());
    check(art.snapshot().size() > budget / 2, "fixture does not exceed the snapshot history budget");
    const auto old_selected = art.selected();
    const auto old_file = art.recovery();
    const auto before_addition = art.snapshot();
    if (!art.add_canvas(definition(128, 96, "Black", "Second")))
        throw std::runtime_error("add canvas failed: " + art.error());
    check(art.document().canvasMode == CanvasMode::Finite && art.document().artboards.size() == 2,
          "addition did not create two finite artboards");
    const auto second = art.document().artboards.back();
    check(second.name == "Second" && second.region.extent == CanvasExtent{128, 96}, "second canvas metadata is wrong");
    check(art.recovery() == old_file && pixel(art, 30, 20) == original, "addition replaced original content or file");
    check(pixel(art, second.region.origin.x + 10, 10) == 0xff000000U, "second background is wrong");
    check(pixel(art, 520, 10) == 0, "gap between canvases is not transparent");
    check(!art.has_selection() && art.history_bytes() <= budget, "addition retained an invalid selection or exceeded history budget");
    const auto after_addition = art.snapshot();
    check(art.undo() && art.document().artboards.empty() && art.selected() == old_selected && art.has_selection() &&
          pixel(art, 30, 20) == original && art.snapshot() == before_addition,
          "undo did not restore original canvas, metadata and selection");
    check(art.redo() && art.document().artboards.size() == 2 && !art.has_selection() && art.snapshot() == after_addition,
          "redo did not restore added canvas and metadata");

    view.color = 0xff00ff00U;
    paint(view, second.region.origin.x + 15, 25, second.region.origin.x + 40, 25);
    const auto second_painted = pixel(art, second.region.origin.x + 30, 25);
    check(second_painted != 0xff000000U, "new canvas did not receive the brush stroke");
    const auto original_before_second_stroke = pixel(art, 80, 70);
    paint(view, 70, 70, 90, 70);
    check(pixel(art, 80, 70) != original_before_second_stroke, "painting did not select the original canvas");
    check(pixel(art, second.region.origin.x + 30, 25) == second_painted, "painting original changed second canvas");
    const auto saved = temporary.path() / "two-canvases.iisc";
    check(art.save(saved), "save failed: " + art.error());
    Artwork reopened(temporary.path());
    check(reopened.open(saved) && reopened.document().artboards.size() == 2 &&
          pixel(reopened, 30, 20) == original && pixel(reopened, second.region.origin.x + 30, 25) == second_painted,
          "saved canvases did not reopen intact");
    check(art.undo() && art.undo() && art.undo() && art.document().artboards.empty(), "paint and canvas undo order failed");
    check(art.redo() && art.redo() && art.redo() && pixel(art, second.region.origin.x + 30, 25) == second_painted,
          "paint and canvas redo order failed");

    const auto prior_revision = art.revision();
    const auto prior_selected = art.selected();
    check(!art.add_canvas(definition(CanvasMaximumDimension, 2, "White", "Too large")) &&
          art.revision() == prior_revision && art.selected() == prior_selected && art.document().artboards.size() == 2,
          "invalid canvas modified the document");
    check(art.create(definition(16, 16, "Transparent", "Independent")) && art.document().artboards.empty(),
          "new document did not create an independent canvas");
    Artwork saved_again(temporary.path());
    check(saved_again.open(saved) && saved_again.document().artboards.size() == 2 &&
          pixel(saved_again, second.region.origin.x + 30, 25) == second_painted,
          "creating a new document damaged the previously saved file");

    Artwork layers(temporary.path());
    check(layers.create(definition(32, 32, "White", "First")) &&
          layers.add_canvas(definition(40, 40, "White", "Second")), "layer ownership fixture failed");
    const auto owner = layers.document().artboards.back().id;
    check(layers.add_layer("Additional paint"), "adding a canvas layer failed");
    const auto* added_layer = findLayer(layers.document(), layers.selected());
    check(added_layer && layerProperties(*added_layer).artboardId == std::optional<std::string>{owner},
          "new layer did not inherit the current canvas");
    check(layers.undo() && layers.redo(), "new canvas layer history failed");
    check(layers.add_canvas(definition(20, 20, "Transparent", "Third")) && layers.document().artboards.size() == 3 &&
          layers.undo() && layers.document().artboards.size() == 2 &&
          layers.redo() && layers.document().artboards.size() == 3, "multiple additions did not undo and redo independently");

    Artwork tools(temporary.path());
    check(tools.create(definition(32, 32, "White", "First")) &&
          tools.add_canvas(definition(40, 40, "Black", "Second")), "canvas tools fixture failed");
    const auto tool_board = tools.document().artboards.back();
    CanvasView tool_view(tools);
    tool_view.tool = "fill"; tool_view.color = 0xff00ff00U;
    ii::gui::WidgetInput click;
    click.kind = ii::gui::WidgetInputKind::pointer_down; click.x = tool_board.region.origin.x + 8;
    click.y = 8; click.buttons = 1; click.device = ii::gui::PointerDevice::mouse;
    check(bool(tool_view.input(click)) && pixel(tools, tool_board.region.origin.x + 8, 8) == 0xff00ff00U &&
          pixel(tools, 8, 8) == 0xffffffffU && pixel(tools, 40, 8) == 0, "fill escaped its canvas");
    check(layerProperties(*findLayer(tools.document(), tools.selected())).artboardId == std::optional<std::string>{tool_board.id},
          "fill layer lost canvas ownership");
    check(tools.undo(), "canvas fill undo failed");
    tool_view.tool = "gradient"; tool_view.color = 0xffff0000U;
    paint(tool_view, tool_board.region.origin.x + 5, 10, tool_board.region.origin.x + 30, 10);
    check(layerProperties(*findLayer(tools.document(), tools.selected())).artboardId == std::optional<std::string>{tool_board.id} &&
          pixel(tools, 8, 8) == 0xffffffffU && pixel(tools, 40, 8) == 0, "gradient escaped its canvas");
    check(tools.undo() && tools.redo(), "canvas gradient history failed");
    check(tools.select_canvas_at({double(tool_board.region.origin.x + 5), 10}) &&
          tools.shape("rectangle", {double(tool_board.region.origin.x - 10), 5},
                      {double(tool_board.region.origin.x + 10), 20}, 0xffff0000U, 0, true),
          "canvas shape creation failed");
    check(layerProperties(*findLayer(tools.document(), tools.selected())).artboardId == std::optional<std::string>{tool_board.id} &&
          pixel(tools, tool_board.region.origin.x - 5, 10) == 0, "shape escaped its canvas");
    const auto image_path = temporary.path() / "import.png";
    check(exportBitmap(makeRasterLayer(4, 4, 0xff1234ffU), image_path.string()).ok(), "image fixture failed");
    check(tools.import_image(image_path) && pixel(tools, tool_board.region.origin.x + 1, 1) == 0xff1234ffU &&
          layerProperties(*findLayer(tools.document(), tools.selected())).artboardId == std::optional<std::string>{tool_board.id},
          "default image import did not use the active canvas");
    check(tools.undo() && tools.redo() && pixel(tools, tool_board.region.origin.x + 1, 1) == 0xff1234ffU,
          "canvas image import history failed");
    check(tools.import_image(image_path, tool_board.region.origin.x + 10) &&
          pixel(tools, tool_board.region.origin.x + 11, 1) == 0xff1234ffU,
          "explicit horizontal image placement changed");

    Artwork sparse(temporary.path(), 4 * 1024 * 1024);
    check(sparse.create(definition(64, 64, "White", "Small")) &&
          sparse.add_canvas(definition(8193, 8193, "Transparent", "Large")), "large tiled canvas creation failed");
    const auto large = sparse.document().artboards.back();
    CanvasView sparse_view(sparse);
    sparse_view.brush_size = 10; sparse_view.color = 0xff123456U;
    paint(sparse_view, large.region.origin.x + 8186, 40, large.region.origin.x + 8200, 40);
    check(pixel(sparse, large.region.origin.x + 8191, 40) != 0, "tiled canvas did not receive a stroke at its edge");
    const auto* sparse_layer = findLayer(sparse.document(), sparse.selected());
    const auto* sparse_source = sparse_layer ? staticLayerSource(*sparse_layer) : nullptr;
    const auto* sparse_asset = sparse_source ? findChunkedRasterAsset(sparse.document(), sparse_source->assetId) : nullptr;
    check(sparse_asset, "large canvas did not use tiled storage");
    for (const auto& tile : sparse_asset->chunks) {
        if (tile.column == 32 && tile.row == 0)
            check(tile.pixels.pixels[std::size_t(40) * 256 + 2] == 0,
                  "tiled stroke leaked beyond the canvas edge");
    }
    check(sparse.undo() && pixel(sparse, large.region.origin.x + 8191, 40) == 0 &&
          sparse.redo() && pixel(sparse, large.region.origin.x + 8191, 40) != 0,
          "tiled canvas stroke history failed");
    ::AffineTransform shift;
    shift.translationX = 10;
    check(sparse.transform(shift) &&
          sparse.set_selection(SelectionGeometry::Rectangle,
              {double(large.region.origin.x), 10}, {double(large.region.origin.x + 16), 30}),
          "shifted tiled canvas selection fixture failed");
    paint(sparse_view, large.region.origin.x + 2, 20, large.region.origin.x + 7, 20);
    check(pixel(sparse, large.region.origin.x + 5, 20) != 0 && sparse.undo() &&
          pixel(sparse, large.region.origin.x + 5, 20) == 0 && sparse.redo() &&
          pixel(sparse, large.region.origin.x + 5, 20) != 0,
          "shifted tiled canvas lost negative-local selected stroke history");

    Document imported;
    imported.extent = {32, 32};
    imported.artboards.push_back({"external", "Existing", {{50, 0}, {16, 16}}, 0xff000000U, true});
    imported.assets.emplace_back(RasterAsset{"external-paint", makeRasterLayer(16, 16, 0)});
    LayerProperties imported_properties{"external-layer", "Existing paint"};
    imported_properties.artboardId = "external";
    imported.layers.emplace_back(StaticBitmapLayer{imported_properties, StaticSource{"external-paint"}});
    Artwork external(temporary.path());
    check(external.replace(imported) && external.add_canvas(definition(8, 8, "White", "New")) &&
          external.document().artboards.back().region.origin.x >= 130 &&
          pixel(external, 51, 1) == 0xff000000U, "new canvas overlapped an existing external artboard");
    check(external.undo() && external.document().extent == CanvasExtent{32, 32} &&
          external.document().artboards.size() == 1 && external.redo(), "external artboard history failed");
    imported.artboards.front().region.origin.x = -5;
    Artwork negative(temporary.path());
    check(negative.replace(std::move(imported)), "negative artboard fixture failed");
    const auto negative_revision = negative.revision();
    check(!negative.add_canvas(definition(8, 8, "White", "Rejected")) &&
          negative.revision() == negative_revision && negative.document().artboards.size() == 1,
          "unsupported negative-position artboard changed on rejection");

    const auto working = temporary.path() / "working";
    const auto moved = temporary.path() / "working-moved";
    std::filesystem::create_directory(working);
    Artwork failure(working, budget);
    check(failure.create(definition(64, 64, "White", "Failure fixture")), "failure fixture failed");
    const auto before = failure.revision();
    std::filesystem::rename(working, moved);
    const bool rejected = !failure.add_canvas(definition(32, 32, "Black", "Rejected"));
    std::filesystem::rename(moved, working);
    check(rejected && failure.revision() == before && failure.document().artboards.empty(),
          "failed durable addition changed the document");
    check(failure.add_canvas(definition(32, 32, "Black", "Accepted")), "addition failed after storage recovered");
    const auto added_revision = failure.revision();
    std::filesystem::rename(working, moved);
    const bool undo_rejected = !failure.undo();
    std::filesystem::rename(moved, working);
    check(undo_rejected && failure.revision() == added_revision && failure.document().artboards.size() == 2 &&
          failure.can_undo() && failure.undo() && failure.redo(), "failed undo damaged canvas history");
    std::cout << "MULTI_CANVAS_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
