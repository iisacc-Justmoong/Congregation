#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderDatabase.h>
#include <iiSharedCanvasNative.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

StrokePoint point(double x, double y, double time, double pressure = 1)
{
    StrokePoint result; result.position = {x, y}; result.time = time; result.pressure = pressure;
    return result;
}

std::vector<std::uint32_t> pixels(const Artwork& artwork)
{
    const auto* asset = findRasterAsset(artwork.document(), "paint");
    check(asset != nullptr, "paint raster unavailable");
    return asset->pixels.pixels;
}

void stroke(Artwork& artwork, BitmapBrush brush, double y)
{
    if (!artwork.begin_input(point(16, y, 1, .4), brush))
        throw std::runtime_error("stroke begin: " + artwork.error());
    check(artwork.extend_input(point(38, y, 1.02, 1)), "stroke move failed");
    if (!artwork.end_input(point(48, y, 1.04, .8)))
        throw std::runtime_error("stroke finish: " + artwork.error());
}
}

int main()
try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "temporary directory unavailable");
    Document document; document.extent = {256, 256};
    auto raster = makeRasterLayer(256, 256);
    std::uint32_t noise = 0x12345678;
    for (auto& value : raster.pixels) {
        noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
        value = 0xff000000u | (noise & 0xffffffu);
    }
    document.assets.emplace_back(RasterAsset{"paint", std::move(raster)});
    document.layers.emplace_back(StaticBitmapLayer{{"layer", "Paint"}, StaticSource{"paint"}});
    constexpr std::size_t budget = 128 * 1024;
    check(encodeIisc(document).bytes.size() > budget / 2, "fixture fits full snapshot budget");
    Artwork artwork(directory.path(), budget);
    check(artwork.replace(std::move(document)), "document setup failed");
    const auto original = pixels(artwork);
    const auto original_document = artwork.snapshot();
    BitmapBrush brush; brush.argb = 0xff6533ff; brush.size = 8;
    brush.pressureToOpacityEnabled = false;

    stroke(artwork, brush, 24);
    const auto first = pixels(artwork);
    const auto first_document = artwork.snapshot();
    check(first != original && artwork.history_bytes() <= budget, "first tile history is invalid");
    stroke(artwork, brush, 104);
    const auto second = pixels(artwork);
    const auto second_document = artwork.snapshot();
    check(second != first && artwork.undo() && pixels(artwork) == first &&
          artwork.snapshot() == first_document && artwork.undo() && pixels(artwork) == original &&
          artwork.snapshot() == original_document, "consecutive strokes did not undo exactly");
    check(artwork.redo() && pixels(artwork) == first && artwork.snapshot() == first_document &&
          artwork.redo() && pixels(artwork) == second && artwork.snapshot() == second_document,
          "consecutive strokes did not redo exactly");

    check(artwork.undo() && pixels(artwork) == first && artwork.can_redo(), "redo setup failed");
    const auto revision = artwork.revision();
    const auto before_failure = artwork.snapshot();
    auto wide_brush = brush; wide_brush.size = 64;
    check(artwork.begin_input(point(8, 32, 1), wide_brush), "budget rejection setup failed");
    double time = 1;
    for (int row = 0; row < 4; ++row) {
        const double y = 32 + row * 64;
        check(artwork.extend_input(point(row % 2 ? 8 : 240, y, time += .02)),
              "budget rejection path failed");
        if (row != 3)
            check(artwork.extend_input(point(row % 2 ? 8 : 240, y + 64, time += .02)),
                  "budget rejection turn failed");
    }
    check(!artwork.end_input(point(8, 224, time + .02)), "over-budget stroke was accepted");
    check(pixels(artwork) == first && artwork.snapshot() == before_failure &&
          artwork.revision() == revision && artwork.can_redo(),
          "over-budget stroke changed the document or redo");

    namespace sql = iiFileProvider::native::sql;
    auto database = sql::Database::open(artwork.recovery());
    check(bool(database), "cannot open recovery database for locking");
    auto lock = sql::Transaction::begin(*database, true);
    check(bool(lock), "cannot lock recovery database");
    check(artwork.begin_input(point(16, 24, 1), brush) &&
          artwork.extend_input(point(40, 24, 1.02)), "locked stroke setup failed");
    check(!artwork.end_input(point(48, 24, 1.04)), "locked stroke unexpectedly committed");
    check(pixels(artwork) == first && artwork.snapshot() == before_failure &&
          artwork.revision() == revision && artwork.can_redo(),
          "failed stroke write changed the document or redo");
    check(lock->rollback().has_value(), "cannot unlock recovery database");

    auto transparent = brush; transparent.opacity = 0;
    stroke(artwork, transparent, 24);
    check(pixels(artwork) == first && artwork.can_redo(), "no-op stroke discarded redo");
    check(artwork.begin_input(point(16, 24, 1), brush) &&
          artwork.extend_input(point(48, 24, 1.02)), "cancel setup failed");
    artwork.cancel();
    check(pixels(artwork) == first && artwork.can_redo(), "cancel changed the committed raster or redo");
    check(artwork.redo() && pixels(artwork) == second, "redo after cancel failed");

    const auto saved = directory.path() / "tile-history.iisc";
    check(artwork.save(saved), "save failed after tile history");
    Artwork reopened(directory.path());
    check(reopened.open(saved) && pixels(reopened) == second, "saved raster did not round trip");

    Artwork mixed(directory.path());
    check(mixed.create(128, 128), "mixed-history document setup failed");
    const auto blank = pixels(mixed);
    stroke(mixed, brush, 24);
    const auto mixed_paint = pixels(mixed);
    check(mixed.add_layer("next") && mixed.document().layers.size() == 2,
          "snapshot history setup failed");
    check(mixed.undo() && mixed.document().layers.size() == 1 && pixels(mixed) == mixed_paint &&
          mixed.undo() && pixels(mixed) == blank, "mixed history did not undo in order");
    check(mixed.redo() && pixels(mixed) == mixed_paint && mixed.redo() &&
          mixed.document().layers.size() == 2, "mixed history did not redo in order");
    std::cout << "INTERACTIVE_RASTER_HISTORY_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
