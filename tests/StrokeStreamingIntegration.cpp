#include <Native/Artwork.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
StrokePoint point(double x, double y, double time) {
    StrokePoint sample;
    sample.position = {x, y};
    sample.time = time;
    sample.pressure = 1;
    return sample;
}
std::vector<std::uint32_t> pixels(const Artwork& artwork) {
    const auto* asset = findRasterAsset(artwork.document(), "paint");
    check(asset, "paint asset unavailable");
    return asset->pixels.pixels;
}
std::vector<std::uint32_t> displayed(const Artwork& artwork) {
    auto preview = artwork.selection_preview();
    std::optional<FrameBitmapOverlay> overlay;
    if (preview) overlay.emplace(FrameBitmapOverlay{preview->asset_id, preview->tiles});
    const auto overlays = overlay ? std::span<const FrameBitmapOverlay>(&*overlay, 1) :
                                    std::span<const FrameBitmapOverlay>{};
    auto frame = renderFrameRegion(artwork.document(), 0, {{0, 0}, {192, 96}}, {192, 96},
                                   RasterSampling::Nearest, overlays);
    check(frame.ok(), "cannot render stroke presentation");
    return frame.pixels.pixels;
}

void pressureAndSelection(const std::filesystem::path& directory, bool selected, bool erase) {
    Artwork artwork(directory);
    check(artwork.create(192, 96), "pressure fixture failed");
    if (erase) check(artwork.import_pixels(makeRasterLayer(192, 96, 0xffccbb99), "source", 0, 0),
                     "eraser fixture failed");
    if (selected) check(artwork.set_selection(SelectionGeometry::Rectangle, {58, 10}, {168, 86},
                                             MaskOperation::Replace, SelectionOptions{true, 4}),
                        "selection fixture failed");
    ToolSettings settings;
    auto brush = settings.brush();
    brush.size = 16;
    brush.argb = 0xff6533ff;
    brush.eraser = erase;
    const StrokeTaper taper{settings.value(ToolParameter::start)/100.,
        settings.value(ToolParameter::end)/100., settings.value(ToolParameter::minimum_size)/100.,
        settings.value(ToolParameter::taper)/100.};
    const auto before = displayed(artwork);
    auto sample = point(16, 48, 1);
    sample.pressure = .3;
    check(artwork.begin_input(sample, brush, 0, taper), "pressure stroke begin failed");
    for (int x = 20; x <= 144; x += 4) {
        sample = point(x, 48 + 8 * std::sin(x / 18.), 1 + x / 120.);
        sample.pressure = .25 + .7 * (x % 28) / 28.;
        sample.tiltX = .4;
        check(artwork.extend_input(sample), "pressure stroke update failed");
    }
    const auto live = displayed(artwork);
    check(live != before, "pressure stroke is not visible while drawing");
    auto released = sample; released.time += .01; released.pressure = 0;
    check(artwork.end_input(released), "pressure stroke finish failed");
    const auto finished = displayed(artwork);
    check(live == finished, "pressure or selection preview changed at release");
    check(artwork.undo() && displayed(artwork) == before && artwork.redo() &&
          displayed(artwork) == finished, "pressure or selection history differs");
}
}

int main() try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "temporary directory unavailable");
    Artwork artwork(directory.path());
    check(artwork.create(192, 96), "fixture creation failed");
    BitmapBrush brush;
    brush.size = 16;
    brush.hardness = 1;
    brush.flow = 1;
    brush.opacity = 1;
    brush.spacingRatio = .05;
    brush.pressureToOpacityEnabled = false;
    brush.argb = 0xff6533ff;
    const StrokeTaper taper{.75, .75, .25, 1};
    const auto before = pixels(artwork);
    check(artwork.begin_input(point(16, 48, 0), brush, 0, taper), "stroke begin failed");
    std::vector<std::uint32_t> early;
    for (int x = 24; x <= 144; x += 8) {
        check(artwork.extend_input(point(x, 48, x / 120.)), "stroke update failed");
        if (x == 48) early = pixels(artwork);
    }
    const auto live = pixels(artwork);
    check(live != before, "stroke was not painted during input");
    for (int y=0; y<96; ++y) for (int x=0; x<24; ++x)
        check(early[std::size_t(y)*192+x] == live[std::size_t(y)*192+x],
              "later input changed a completed start taper");
    check(artwork.end_input(point(144, 48, 144 / 120.)), "stroke finish failed");
    const auto finished = pixels(artwork);
    check(live == finished, "release changed the shape of the already displayed stroke");
    check(artwork.undo() && pixels(artwork) == before && artwork.redo() &&
          pixels(artwork) == finished, "streamed stroke history is not exact");
    const auto save = directory.path() / "streamed.iisc";
    check(artwork.save(save), "streamed stroke save failed");
    Artwork reopened(directory.path());
    check(reopened.open(save) && pixels(reopened) == finished, "saved stroke differs after reopening");
    Artwork shortStroke(directory.path());
    check(shortStroke.create(192, 96) && shortStroke.begin_input(point(16, 48, 0), brush, 0, taper),
          "short taper fixture failed");
    for (int x=24; x<=80; x+=8)
        check(shortStroke.extend_input(point(x, 48, x/120.)), "short taper input failed");
    check(shortStroke.end_input(point(80, 48, 80/120.)), "short taper finish failed");
    const auto shortPixels = pixels(shortStroke);
    for (int y=32; y<64; ++y) for (int offset=-12; offset<=8; ++offset)
        check(shortPixels[std::size_t(y)*192+80+offset] == finished[std::size_t(y)*192+144+offset],
              "end taper changed with the full stroke length");
    Artwork untapered(directory.path());
    check(untapered.create(192, 96) && untapered.begin_input(point(16, 48, 0), brush),
          "untapered fixture failed");
    for (int x=24; x<=144; x+=8)
        check(untapered.extend_input(point(x, 48, x/120.)), "untapered input failed");
    check(untapered.end_input(point(144, 48, 144/120.)), "untapered finish failed");
    const auto plain = pixels(untapered);
    bool changedStart = false, changedEnd = false;
    for (int y=32; y<64; ++y) for (int x=0; x<192; ++x) {
        const bool changed = plain[std::size_t(y)*192+x] != finished[std::size_t(y)*192+x];
        if (x < 28) changedStart |= changed;
        if (x >= 132 && x < 156) changedEnd |= changed;
        if (x >= 48 && x < 112) check(!changed, "taper changed the stable middle");
    }
    check(changedStart && changedEnd, "start or end taper has no visible effect");
    Artwork manySamples(directory.path());
    auto tiny = brush; tiny.size = 1;
    check(manySamples.create(192, 96) && manySamples.begin_input(point(40, 40, 0), tiny, 0, taper),
          "many-sample fixture failed");
    for (int index=1; index<=65550; ++index)
        check(manySamples.extend_input(point(40, 40, index*.001)), "long input sequence was rejected");
    check(manySamples.end_input(point(40, 40, 65.55)) && manySamples.undo(),
          "long input sequence did not finish as one undoable stroke");
    pressureAndSelection(directory.path(), false, false);
    pressureAndSelection(directory.path(), true, false);
    pressureAndSelection(directory.path(), true, true);
    std::cout << "STROKE_STREAMING_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
