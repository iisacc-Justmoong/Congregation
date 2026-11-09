#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

const RasterLayer& pixels(const Artwork& artwork)
{
    for (const auto& layer : artwork.document().layers) {
        if (layerProperties(layer).id != artwork.selected()) continue;
        const auto* source = staticLayerSource(layer);
        const auto* asset = source ? findRasterAsset(artwork.document(), source->assetId) : nullptr;
        check(asset != nullptr, "paint bitmap unavailable");
        return asset->pixels;
    }
    throw std::runtime_error("paint layer unavailable");
}

unsigned maximum_alpha(const RasterLayer& raster, int x, int y, int radius = 2)
{
    unsigned alpha = 0;
    for (int row = std::max(0, y - radius); row <= std::min(raster.height - 1, y + radius); ++row)
        for (int col = std::max(0, x - radius); col <= std::min(raster.width - 1, x + radius); ++col)
            alpha = std::max(alpha, raster.pixels[std::size_t(row) * raster.width + col] >> 24);
    return alpha;
}

void send(CanvasView& view, ii::gui::WidgetInputKind kind, double x, double y,
          double time, ii::gui::PointerDevice device = ii::gui::PointerDevice::mouse,
          double pressure = 1)
{
    ii::gui::WidgetInput event;
    event.kind = kind;
    event.x = x;
    event.y = y;
    event.buttons = kind == ii::gui::WidgetInputKind::pointer_up ? 0 : 1;
    event.timestamp = time;
    event.device = device;
    event.pressure = pressure;
    auto result = view.input(event);
    check(result && *result, "canvas rejected pointer input");
}

std::vector<std::uint32_t> line(const std::filesystem::path& directory, double pressure,
                                double scatter, bool noise = false, double hueJitter = 0)
{
    Artwork artwork(directory);
    check(artwork.create(160, 96), "line fixture failed");
    ToolSettings settings;
    using P = ToolParameter;
    settings.set(P::size, 20);
    settings.set(P::stabilization, 0);
    settings.set(P::post, 0);
    settings.set(P::scatter, scatter);
    settings.set(P::noise_enabled, noise ? 1 : 0);
    settings.set(P::noise_amount, 80);
    settings.set(P::color_hue_jitter, hueJitter);
    settings.set(P::color_apply_per_tip, 1);
    // Isolate the size response from the separate pressure-opacity response.
    settings.set(P::minimum_opacity, 100);
    CanvasView view(artwork);
    view.color = 0xff5c2eff;
    view.brush_size = 20;
    view.settings = settings.brush();
    using K = ii::gui::WidgetInputKind;
    using D = ii::gui::PointerDevice;
    send(view, K::pointer_down, 16, 48, 10, D::pen, pressure);
    send(view, K::pointer_move, 80, 48, 10.04, D::pen, pressure);
    send(view, K::pointer_up, 144, 48, 10.08, D::pen, pressure);
    return pixels(artwork).pixels;
}
}

int main()
try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "temporary directory unavailable");
    using K = ii::gui::WidgetInputKind;
    using P = ToolParameter;

    // Use the product defaults, including pressure response, correction and scatter.
    // Check the in-flight preview as well as the whole-stroke taper replay.
    for (bool tapered : {false, true}) {
        Artwork artwork(directory.path());
        check(artwork.create(160, 128), "curve fixture failed");
        ToolSettings settings;
        settings.set(P::size, 16);
        CanvasView view(artwork);
        view.brush_size = 16;
        view.settings = settings.brush();
        if (tapered)
            view.taper = {settings.value(P::start) / 100, settings.value(P::end) / 100,
                          settings.value(P::minimum_size) / 100, settings.value(P::taper) / 100};
        const auto before = pixels(artwork).pixels;
        send(view, K::pointer_down, 16, 32, 100);
        send(view, K::pointer_move, 80, 32, 100.04);
        check(maximum_alpha(pixels(artwork), 48, 32) > 80,
              "default brush lost the moving stroke in its live preview");
        send(view, K::pointer_move, 80, 96, 100.08);
        check(maximum_alpha(pixels(artwork), 80, 64) > 80,
              "brush did not follow the intermediate pointer path");
        send(view, K::pointer_up, 144, 96, 100.12);
        const auto after = pixels(artwork).pixels;
        check(maximum_alpha(pixels(artwork), 48, 32) > 80 &&
              maximum_alpha(pixels(artwork), 80, 64) > 80 &&
              maximum_alpha(pixels(artwork), 112, 96) > 80,
              "committed brush stroke lost part of the path");
        check(maximum_alpha(pixels(artwork), 40, 80) == 0,
              "brush painted away from the pointer trajectory");
        check(artwork.undo() && pixels(artwork).pixels == before &&
              artwork.redo() && pixels(artwork).pixels == after,
              "brush stroke was not one undoable operation");
    }

    const auto low = line(directory.path(), .2, 0);
    const auto high = line(directory.path(), 1, 0);
    auto width = [](const auto& values) {
        int count = 0;
        for (int y = 0; y < 96; ++y)
            if ((values[std::size_t(y) * 160 + 80] >> 24) > 20) ++count;
        return count;
    };
    check(width(low) > 0 && width(high) > width(low),
          "pen pressure did not change the width of the moving stroke");
    const auto scattered = line(directory.path(), 1, 75);
    check(scattered != high, "scatter did not change the moving brush marks");
    const auto noisy = line(directory.path(), 1, 0, true);
    check(noisy != high, "noise did not change the moving brush pixels");
    const auto colorJitter = line(directory.path(), 1, 0, false, 75);
    check(colorJitter != high, "color dynamics did not change moving brush pixels");

    std::cout << "BRUSH_STROKE_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
