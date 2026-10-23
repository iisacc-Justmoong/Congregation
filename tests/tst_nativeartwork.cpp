#include <Native/Artwork.h>
#include <Engine/RecentCanvas.h>
#include <iiFileProviderNative.h>
#include <iostream>
#include <algorithm>
#include <stdexcept>
using congregation::native::Artwork;
using namespace iiSharedCanvas;
namespace
{
void check(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 2, "Workspace test directory missing");
        iiFileProvider::native::TemporaryDirectory temporary{std::filesystem::path(argv[1])};
        Artwork artwork(temporary.path());
        check(artwork.create(96, 64), artwork.error());
        const auto initial = artwork.revision();
        const auto selected = artwork.selected();
        check(artwork.select(selected) && artwork.revision() == initial,
              "Layer selection modified persisted artwork");
        BitmapBrush brush;
        brush.size = 12;
        brush.argb = 0xff1122ff;
        brush.pressureToOpacityEnabled = true;
        check(artwork.begin({12, 12}, .4, brush), artwork.error());
        check(artwork.extend({24, 24}, .8), artwork.error());
        check(artwork.end({36, 24}, 1), artwork.error());
        check(artwork.revision() == initial + 1 && artwork.can_undo(),
              "Stroke was not one durable edit");
        auto painted = renderFrame(artwork.document(), 0);
        check(painted.ok(), painted.message);
        check(std::ranges::any_of(painted.pixels.pixels, [](auto pixel) { return pixel >> 24; }),
              "Brush did not commit raster pixels");
        const auto committed = artwork.snapshot();
        const auto revision = artwork.revision();
        check(artwork.begin({50, 30}, 1, brush), artwork.error());
        check(artwork.extend({75, 40}, 1), artwork.error());
        artwork.cancel();
        check(artwork.snapshot() == committed && artwork.revision() == revision,
              "Canceled input altered the document or its revision");
        check(artwork.undo(), artwork.error());
        check(artwork.redo(), artwork.error());
        check(renderFrame(artwork.document(), 0).pixels.pixels == painted.pixels.pixels,
              "Undo/redo changed stroke pixels");
        check(artwork.rename(selected, "한글 레이어"), artwork.error());
        const auto named = artwork.revision();
        check(artwork.rename(selected, "한글 레이어") && artwork.revision() == named,
              "Identical layer name wrote a new revision");
        check(artwork.shape("ellipse", {3, 3}, {80, 50}, 0xffdd4400, 2, false), artwork.error());
        check(std::holds_alternative<VectorAsset>(artwork.document().assets.back()),
              "Shape was flattened instead of preserving vector paths");
        check(artwork.duplicate_layer(), artwork.error());
        check(artwork.document().layers.size() == 3, "Layer duplication failed");
        check(artwork.remove_layer(), artwork.error());
        check(artwork.document().layers.size() == 2, "Layer deletion failed");
        const auto source = temporary.path() / "native.iisc";
        check(artwork.save(source), artwork.error());
        const auto saved = renderFrame(artwork.document(), 0).pixels.pixels;
        Artwork reopened(temporary.path());
        check(reopened.open(source), reopened.error());
        check(renderFrame(reopened.document(), 0).pixels.pixels == saved,
              "Canonical save/reopen changed artwork");
        check(reopened.export_image(temporary.path() / "export.png", "png"), reopened.error());
        check(reopened.export_image(temporary.path() / "export.psd", "psd"), reopened.error());
        Artwork imported(temporary.path());
        check(imported.open(temporary.path() / "export.png"), imported.error());
        check(imported.document().extent == reopened.document().extent,
              "Image import lost its extent");
        Artwork recovery(temporary.path());
        check(recovery.recover(reopened.recovery()) && recovery.source().empty(),
              "Recovery adopted an internal path as a user save destination");
        const auto before = reopened.snapshot();
        check(!reopened.receive("invalid"), "Invalid collaboration snapshot accepted");
        check(reopened.snapshot() == before, "Rejected snapshot changed the artwork");
        check(reopened.receive(artwork.snapshot()), reopened.error());
        check(reopened.history_bytes() <= 64 * 1024 * 1024, "Undo history exceeded its budget");
        check(!reopened.create(0, 64), "Invalid canvas extent accepted");
        const auto assets = artwork.document().assets.size();
        check(artwork.duplicate_layer(), artwork.error());
        check(artwork.remove_layer(), artwork.error());
        check(artwork.document().assets.size() == assets,
              "Deleting a duplicated layer leaked its unreferenced pixels");
        congregation::RecentCanvas legacy;
        legacy.document = artwork.snapshot();
        using congregation::Object;
        using congregation::Value;
        legacy.objects.emplace_back(Object{{"id", Value(7)},
                                           {"type", Value("shape")},
                                           {"shapeKind", Value("diamond")},
                                           {"x", Value(60.)},
                                           {"y", Value(5.)},
                                           {"width", Value(20.)},
                                           {"height", Value(20.)},
                                           {"color", Value("#ff0000")},
                                           {"name", Value("이전 도형")}});
        auto png = encodeBitmap(makeRasterLayer(2, 2, 0xff00ff00));
        check(png.ok(), png.result.message);
        legacy.assets.push_back(
            {8, 1, {reinterpret_cast<const char*>(png.bytes.data()), png.bytes.size()}});
        legacy.objects.emplace_back(Object{{"id", Value(8)},
                                           {"type", Value("image")},
                                           {"x", Value(80.)},
                                           {"y", Value(40.)},
                                           {"width", Value(8.)},
                                           {"height", Value(8.)}});
        auto encoded = congregation::encode_canvas(legacy);
        check(bool(encoded), "Cannot construct legacy fixture");
        auto old_file = temporary.path() / "legacy.vrc";
        iiFileProvider::native::File::create(old_file, *encoded);
        check(reopened.open(old_file), reopened.error());
        check(reopened.document().layers.size() == artwork.document().layers.size() + 2,
              "Legacy objects were discarded");
        const auto legacy_pixels = renderFrame(reopened.document(), 0);
        check(legacy_pixels.ok(), legacy_pixels.message);
        check(legacy_pixels.pixels.pixels[15 * 96 + 70] == 0xffff0000 &&
                  legacy_pixels.pixels.pixels[43 * 96 + 83] == 0xff00ff00,
              "Legacy shape or image transform changed pixels");
        check(iiFileProvider::native::File::read(old_file) == *encoded,
              "Migration overwrote its source");
        const auto migrated = reopened.snapshot();
        legacy.objects.emplace_back(Object{{"id", Value(99)}, {"type", Value("unknown")}});
        auto unsupported = congregation::encode_canvas(legacy);
        iiFileProvider::native::File::write(old_file, *unsupported);
        check(!reopened.open(old_file) && reopened.snapshot() == migrated,
              "Unsupported legacy data was silently discarded or changed the current document");
        legacy.objects.pop_back();
        auto oversized_object = congregation::object(legacy.objects.front());
        oversized_object["id"] = Value(1e200);
        legacy.objects.front() = Value(std::move(oversized_object));
        auto oversized = congregation::encode_canvas(legacy);
        iiFileProvider::native::File::write(old_file, *oversized);
        check(!reopened.open(old_file) && reopened.snapshot() == migrated,
              "Oversized legacy identifiers were not rejected safely");
        check(reopened.create(64, 64, true), reopened.error());
        auto infinite_revision = reopened.revision();
        const auto infinite_original = reopened.snapshot();
        auto invalid_brush = brush;
        invalid_brush.size = 0;
        check(!reopened.begin({-12, -12}, 1, invalid_brush) &&
                  reopened.snapshot() == infinite_original,
              "Rejected brush changed infinite canvas bounds");
        check(reopened.begin({-12, -12}, 1, brush) && reopened.end({-4, -4}, 1), reopened.error());
        check(reopened.revision() == infinite_revision + 1,
              "Infinite canvas growth produced multiple durable edits");
        auto negative = renderFrameRegion(reopened.document(), 0, {{-20, -20}, {24, 24}}, {24, 24});
        if (!negative.ok() ||
            !std::ranges::any_of(negative.pixels.pixels, [](auto pixel) { return pixel >> 24; }))
        {
            const auto& chunks = std::get<ChunkedRasterAsset>(reopened.document().assets[0]);
            std::cerr << "negative region: " << negative.message
                      << " chunks=" << chunks.chunks.size() << "\n";
            for (const auto& chunk : chunks.chunks)
                std::cerr << "chunk=" << chunk.column << "," << chunk.row << " painted="
                          << std::ranges::count_if(chunk.pixels.pixels,
                                                   [](auto pixel) { return pixel >> 24; })
                          << "\n";
            throw std::runtime_error("Infinite painting lost negative document coordinates");
        }
        std::cout << "NATIVE_ARTWORK_OK brush=true pressure=true cancel=true undo=true layers=true "
                     "vectors=true iisc=true png=true psd=true bounded=true\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
