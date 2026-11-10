#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderDatabase.h>
#include <iiSharedCanvasNative.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
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

template<class Action>
void whileTransientReaderHoldsSnapshot(const std::filesystem::path& path, Action action)
{
    namespace sql = iiFileProvider::native::sql;
    std::promise<std::string> ready, finished;
    auto started = ready.get_future();
    auto completed = finished.get_future();
    std::jthread reader([&] {
        try {
            // This connection only reads. A same-process read-only descriptor
            // cannot be lock-upgraded by macOS SQLite's shared VFS state.
            auto database = sql::Database::open(path);
            if (!database) throw std::runtime_error(database.error().message);
            auto transaction = sql::Transaction::begin(*database, false);
            if (!transaction) throw std::runtime_error(transaction.error().message);
            {
                auto query = database->prepare("SELECT revision FROM canvas_state WHERE singleton=1");
                if (!query) throw std::runtime_error(query.error().message);
                auto row = query->step();
                if (!row || *row != sql::Step::row) throw std::runtime_error("reader did not obtain a snapshot");
            }
            ready.set_value({});
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            auto result = transaction->commit();
            finished.set_value(result ? std::string{} : result.error().message);
        } catch (const std::exception& error) {
            try { ready.set_value(error.what()); } catch (const std::future_error&) {}
            finished.set_value(error.what());
        }
    });
    const auto startError = started.get();
    if (!startError.empty()) throw std::runtime_error("reader setup: " + startError);
    const bool changed = action();
    reader.join();
    const auto finishError = completed.get();
    if (!finishError.empty()) throw std::runtime_error("reader cleanup: " + finishError);
    check(changed, "history action failed while a transient reader was finishing");
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
    {
        namespace sql = iiFileProvider::native::sql;
        auto database = sql::Database::open(artwork.recovery());
        check(bool(database), "cannot open recovery database for undo lock");
        auto lock = sql::Transaction::begin(*database, true);
        check(bool(lock), "cannot lock recovery database for undo");
        const auto revision = artwork.revision();
        check(!artwork.undo() && artwork.snapshot() == second_document &&
              artwork.revision() == revision && artwork.can_undo(),
              "failed undo changed the document or history stack");
        check(bool(lock->rollback()), "cannot unlock recovery database after undo");
    }
    const auto before_transient = artwork.revision();
    whileTransientReaderHoldsSnapshot(artwork.recovery(), [&] { return artwork.undo(); });
    check(pixels(artwork) == first && artwork.snapshot() == first_document && artwork.can_redo() &&
          artwork.revision() == before_transient + 1,
          "undo under a transient reader did not preserve the expected pixels and history");
    whileTransientReaderHoldsSnapshot(artwork.recovery(), [&] { return artwork.redo(); });
    check(pixels(artwork) == second && artwork.snapshot() == second_document && artwork.can_undo() &&
          artwork.revision() == before_transient + 2,
          "redo under a transient reader did not restore the expected pixels and history");
    Document persisted;
    check(DocumentFile::readSnapshot(artwork.recovery().string(), persisted).ok() &&
          encodeIisc(persisted).bytes == encodeIisc(artwork.document()).bytes,
          "history under a transient reader did not persist the exact document");
    check(second != first && artwork.undo() && pixels(artwork) == first &&
          artwork.snapshot() == first_document && artwork.undo() && pixels(artwork) == original &&
          artwork.snapshot() == original_document, "consecutive strokes did not undo exactly");
    check(artwork.redo() && pixels(artwork) == first && artwork.snapshot() == first_document &&
          artwork.redo() && pixels(artwork) == second && artwork.snapshot() == second_document,
          "consecutive strokes did not redo exactly");

    check(artwork.undo() && pixels(artwork) == first && artwork.can_redo(), "redo setup failed");
    {
        namespace sql = iiFileProvider::native::sql;
        auto database = sql::Database::open(artwork.recovery());
        check(bool(database), "cannot open recovery database for redo lock");
        auto lock = sql::Transaction::begin(*database, true);
        check(bool(lock), "cannot lock recovery database for redo");
        const auto revision = artwork.revision();
        check(!artwork.redo() && artwork.snapshot() == first_document &&
              artwork.revision() == revision && artwork.can_redo(),
              "failed redo changed the document or history stack");
        check(bool(lock->rollback()), "cannot unlock recovery database after redo");
    }
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

    // Compare the tiled commit to a full-image reference with partial coverage
    // on both sides of a 64-pixel tile boundary.
    Artwork masked(directory.path()), reference(directory.path());
    check(masked.create(130, 80) && reference.create(130, 80), "feathered fixture failed");
    check(masked.set_selection(SelectionGeometry::Rectangle, {62, 20}, {80, 60},
                               MaskOperation::Replace, SelectionOptions{true, 4}),
          "feathered selection failed");
    const auto* mask=masked.selection_mask();
    check(mask && std::any_of(mask->alpha.begin(),mask->alpha.end(),
                              [](std::uint8_t value){return value>0 && value<255;}),
          "feathered fixture has no partial coverage");
    const auto selected_before=masked.snapshot();
    BitmapBrush feather_brush=brush;feather_brush.size=16;
    const auto paint_boundary=[&](Artwork& value){
        check(value.begin_input(point(54,40,1),feather_brush) &&
              value.extend_input(point(64,40,1.02)) && value.end_input(point(76,40,1.04)),
              "feathered boundary stroke failed");
    };
    paint_boundary(reference);paint_boundary(masked);
    const auto* full=findRasterAsset(reference.document(),"paint");
    check(full!=nullptr,"reference raster missing");
    const auto expected=blendRaster(makeRasterLayer(130,80,0),full->pixels,*mask);
    check(expected.ok() && pixels(masked)==expected.pixels.pixels,
          "tile boundary or partial selection differs from whole-image compositing");
    const auto selected_after=masked.snapshot();
    check(masked.undo() && masked.snapshot()==selected_before && masked.redo() &&
          masked.snapshot()==selected_after,"feathered history was not exact");
    std::cout << "INTERACTIVE_RASTER_HISTORY_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
