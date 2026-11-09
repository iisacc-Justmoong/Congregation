#include <iiSharedCanvasNative.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace iiSharedCanvas;

namespace {
using Clock = std::chrono::steady_clock;

void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

Document fixture(bool filtered)
{
    Document document;
    document.extent = {1024, 1024};
    document.assets.emplace_back(RasterAsset{"paper", makeRasterLayer(1024, 1024, 0xffffffff)});
    auto paint = makeRasterLayer(1024, 1024);
    for (std::size_t i = 0; i < paint.pixels.size(); ++i)
        paint.pixels[i] = 0x80000000U | (std::uint32_t(i * 2654435761U) & 0x00ffffffU);
    document.assets.emplace_back(RasterAsset{"paint", std::move(paint)});
    document.layers.emplace_back(StaticBitmapLayer{{"paper-layer", "Paper"}, StaticSource{"paper"}});
    document.layers.emplace_back(StaticBitmapLayer{{"paint-layer", "Paint"}, StaticSource{"paint"}});
    if (filtered) {
        LayerFilter filter;
        filter.operation = RasterEffect{RasterEffectKind::Invert};
        layerProperties(document.layers.back()).appearance.filters.push_back(filter);
    }
    return document;
}

void run(bool filtered)
{
    const auto document = fixture(filtered);
    const std::vector<FrameRenderTileRequest> requests{
        {{{0, 0}, {512, 512}}, {512, 512}, RasterSampling::Smooth},
        {{{512, 0}, {512, 512}}, {512, 512}, RasterSampling::Smooth},
        {{{0, 512}, {512, 512}}, {512, 512}, RasterSampling::Smooth},
        {{{512, 512}, {512, 512}}, {512, 512}, RasterSampling::Smooth}};
    std::array<long long, 5> separate_times{}, batched_times{};
    for (std::size_t iteration = 0; iteration < separate_times.size(); ++iteration) {
        std::vector<FrameRenderResult> separate;
        separate.reserve(requests.size());
        FrameTileRenderResult batched;
        const auto run_separate = [&] {
            const auto start = Clock::now();
            for (const auto& request : requests)
                separate.push_back(renderFrameRegion(document, 0, request.region,
                                                     request.outputExtent, request.sampling));
            separate_times[iteration] =
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
        };
        const auto run_batched = [&] {
            const auto start = Clock::now();
            batched = renderFrameTiles(document, 0, requests);
            batched_times[iteration] =
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
        };
        if (iteration % 2 == 0) { run_separate(); run_batched(); }
        else { run_batched(); run_separate(); }
        check(batched.ok() && batched.tiles.size() == separate.size(), "batch composition failed");
        for (std::size_t tile = 0; tile < separate.size(); ++tile)
            check(separate[tile].ok() &&
                  separate[tile].pixels.width == batched.tiles[tile].pixels.width &&
                  separate[tile].pixels.height == batched.tiles[tile].pixels.height &&
                  separate[tile].pixels.pixels == batched.tiles[tile].pixels.pixels,
                  "batched tile pixels differ from separate composition");
    }
    std::ranges::sort(separate_times);
    std::ranges::sort(batched_times);
    std::cout << "TILE_COMPOSITION filtered=" << filtered << " tiles=" << requests.size()
              << " iterations=" << separate_times.size()
              << " separate_median_us=" << separate_times[separate_times.size() / 2]
              << " batch_median_us=" << batched_times[batched_times.size() / 2] << '\n';
}
}

int main()
try {
    run(false);
    run(true);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
