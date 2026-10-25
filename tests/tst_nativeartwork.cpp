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
        {
            Artwork controls(temporary.path());
            check(controls.create(96, 64), controls.error());
            const auto id = controls.selected();
            const auto rev = controls.revision();
            check(controls.opacity(id, .72) && controls.revision() == rev + 1,
                  "Layer opacity did not commit one edit");
            check(controls.opacity(id, .72) && controls.revision() == rev + 1,
                  "Unchanged opacity created a document edit");
            check(controls.cycle_blend(id) &&
                      layerProperties(controls.document().layers.back()).blendMode ==
                          RasterBlendMode::Multiply,
                  "Layer blend control did not reach the document");
            check(controls.timing(30, 60) && controls.document().timeline.frameCount == 60,
                  "Timeline tick rate did not update document timing");
            const auto saved = controls.snapshot();
            check(!controls.timing(0, 60) && controls.snapshot() == saved,
                  "Invalid timing changed the document");
            AffineTransform original;
            original.m11 = 1;
            original.m12 = 2;
            original.m21 = 3;
            original.m22 = 4;
            original.translationX = 5;
            original.translationY = 6;
            check(controls.transform(original), controls.error());
            check(controls.transform(congregation::native::LayerTransform::flip_horizontal),
                  controls.error());
            auto result = layerProperties(controls.document().layers.back()).transform;
            check(result.m11 == -1 && result.m12 == 2 && result.m21 == -3 && result.m22 == 4 &&
                      result.translationX == 91 && result.translationY == 6,
                  "Horizontal flip used the wrong affine matrix convention");
            check(controls.transform(original) &&
                      controls.transform(congregation::native::LayerTransform::rotate_right),
                  controls.error());
            result = layerProperties(controls.document().layers.back()).transform;
            check(result.m11 == -2 && result.m12 == 1 && result.m21 == -4 && result.m22 == 3 &&
                      result.translationX == 74 && result.translationY == -11,
                  "Rotation failed to compose around the canvas center");
            check(controls.transform(congregation::native::LayerTransform::rotate_left),
                  controls.error());
            result = layerProperties(controls.document().layers.back()).transform;
            check(result.m11 == 1 && result.m12 == 2 && result.m21 == 3 && result.m22 == 4 &&
                      result.translationX == 5 && result.translationY == 6,
                  "Inverse rotation did not restore the layer transform");
            check(controls.add_layer("Middle"), controls.error());
            const auto middle = controls.selected();
            check(controls.add_layer("Top"), controls.error());
            const auto top = controls.selected();
            check(controls.move(id, top) &&
                      layerProperties(controls.document().layers.back()).id == id,
                  "Dragging a layer above the top row did not change compositing order");
            check(controls.move(id, "") &&
                      layerProperties(controls.document().layers.front()).id == id,
                  "Dropping a layer at the bottom did not change compositing order");
            check(controls.move(top, middle) &&
                      layerProperties(controls.document().layers.back()).id == top,
                  "Moving before an adjacent row reversed the displayed order");
        }
        {
            Artwork canvas(temporary.path());
            for (const auto& [background, expected] : {std::pair{"White", 0xffffffffU},
                                                       {"Transparent", 0x00000000U},
                                                       {"Black", 0xff000000U}})
            {
                congregation::native::CanvasDefinition definition{
                    25.4,          12.7,    64,    64, 32, "mm", background, "Test print canvas",
                    "test-preset", "Print", "Test"};
                check(canvas.create(definition), canvas.error());
                check(canvas.document().extent == CanvasExtent{64, 32} && !canvas.can_undo() &&
                          canvas.selected() == "paint-layer",
                      "New canvas did not initialize its paint target");
                check(canvas.creation() && canvas.creation()->unit == "mm" &&
                          canvas.creation()->ppi == 64 &&
                          canvas.creation()->preset_id == "test-preset",
                      "Creation specification was discarded");
                const auto rendered = renderFrame(canvas.document(), 0);
                check(rendered.ok() &&
                          std::ranges::all_of(rendered.pixels.pixels,
                                              [expected](auto pixel) { return pixel == expected; }),
                      "Selected canvas background was not rendered");
                const auto before = canvas.snapshot();
                const auto revision = canvas.revision();
                auto invalid = definition;
                invalid.pixel_width = 0;
                check(!canvas.create(invalid) && canvas.snapshot() == before &&
                          canvas.revision() == revision,
                      "Rejected canvas creation replaced the existing document");
                invalid = definition;
                invalid.background = "unsupported";
                check(!canvas.create(invalid) && canvas.snapshot() == before,
                      "Rejected background changed the document");
                const auto saved = temporary.path() / (std::string(background) + ".iisc");
                check(canvas.save(saved), canvas.error());
                Artwork reopened(temporary.path());
                check(reopened.open(saved) && renderFrame(reopened.document(), 0).pixels.pixels ==
                                                  rendered.pixels.pixels,
                      "Canvas background was lost after saving and reopening");
                check(!reopened.creation(),
                      "Session-only preset metadata was retained for a reopened file");
            }
            check(canvas.create(48, 32) && !canvas.creation(),
                  "Legacy create retained stale preset metadata");
        }
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
