#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string_view>

namespace allocations {
thread_local bool active = false;
thread_local std::size_t bytes = 0;
}

void* operator new(std::size_t size)
{
    if (allocations::active) allocations::bytes += size;
    if (auto* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Measurement final { long long microseconds; std::size_t bytes; };

template<typename Function>
Measurement measure(Function&& function)
{
    allocations::bytes = 0;
    allocations::active = true;
    const auto start = std::chrono::steady_clock::now();
    try { function(); }
    catch (...) { allocations::active = false; throw; }
    const auto end = std::chrono::steady_clock::now();
    allocations::active = false;
    return {std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(), allocations::bytes};
}

StrokePoint sample(double x, double y, double time, double pressure)
{
    StrokePoint result;
    result.position = {x, y}; result.time = time; result.pressure = pressure;
    return result;
}

std::uint32_t pixel(const Artwork& artwork, int x, int y)
{
    for (const auto& layer : artwork.document().layers) {
        if (layerProperties(layer).id != artwork.selected()) continue;
        const auto* source = staticLayerSource(layer);
        const auto* asset = source ? findRasterAsset(artwork.document(), source->assetId) : nullptr;
        check(asset != nullptr, "raster paint asset unavailable");
        return asset->pixels.pixels[std::size_t(y) * asset->pixels.width + x];
    }
    throw std::runtime_error("paint layer unavailable");
}

void print(const char* stage, Measurement value)
{
    std::cout << ' ' << stage << "_us=" << value.microseconds << ' ' << stage << "_bytes=" << value.bytes;
}

void run(const std::filesystem::path& directory, int side, int brush_size)
{
    Artwork artwork(directory);
    check(artwork.create(side, side), "canvas setup failed");
    BitmapBrush brush;
    brush.size = brush_size; brush.argb = 0xff6533ff; brush.pressureToOpacityEnabled = false;
    const int x = side / 2, y = side / 2;
    const auto original = pixel(artwork, x, y);
    bool ok = false;
    IiscEncodeResult encoded;
    const auto snapshot = measure([&] { encoded = encodeIisc(artwork.document()); });
    check(encoded.ok(), "snapshot measurement failed");
    const auto begin = measure([&] { ok = artwork.begin_input(sample(x, y, 1, .3), brush); });
    check(ok, "stroke begin failed");
    const auto move = measure([&] { ok = artwork.extend_input(sample(x + 64, y, 1.02, .9)); });
    check(ok, "stroke move failed");
    if (pixel(artwork, x + 32, y) == original) {
        std::cerr << "missing live pixel: canvas=" << side << " brush=" << brush_size
                  << " center=" << pixel(artwork, x, y) << " middle=" << pixel(artwork, x + 32, y)
                  << " end=" << pixel(artwork, x + 64, y) << '\n';
        throw std::runtime_error("stroke lost its intermediate path");
    }
    FrameRenderResult rendered;
    const int tile_x = std::min(side - 512, (x / 512) * 512);
    const int tile_y = std::min(side - 512, (y / 512) * 512);
    const auto composite = measure([&] {
        rendered = renderFrameRegion(artwork.document(), 0, {{tile_x, tile_y}, {512, 512}}, {512, 512});
    });
    check(rendered.ok(), "tile composite failed");
    FrameRenderResult small_rendered;
    const int small_x = std::min(side - 256, (x / 256) * 256);
    const int small_y = std::min(side - 256, (y / 256) * 256);
    const auto small_composite = measure([&] {
        small_rendered = renderFrameRegion(artwork.document(), 0, {{small_x, small_y}, {256, 256}}, {256, 256});
    });
    check(small_rendered.ok(), "small tile composite failed");
    for (int row = 0; row < 256; ++row)
        for (int column = 0; column < 256; ++column)
            check(small_rendered.pixels.pixels[std::size_t(row) * 256 + column] ==
                  rendered.pixels.pixels[std::size_t(small_y - tile_y + row) * 512 + small_x - tile_x + column],
                  "small tile differs from the corresponding full tile pixels");
    const auto end = measure([&] { ok = artwork.end_input(sample(x + 80, y, 1.04, 1)); });
    check(ok, "stroke end failed");
    const auto painted = pixel(artwork, x + 32, y);
    check(artwork.undo() && pixel(artwork, x + 32, y) == original, "stroke undo failed");
    check(artwork.redo() && pixel(artwork, x + 32, y) == painted, "stroke redo failed");
    std::cout << "STROKE_PERFORMANCE canvas=" << side << " brush=" << brush_size;
    print("encode", snapshot); print("begin", begin); print("move", move);
    print("composite_512", composite); print("composite_256", small_composite); print("end", end); std::cout << '\n';
}
}

int main(int argc, char** argv)
try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "temporary directory unavailable");
    if (argc > 1 && std::string_view(argv[1]) == "--large-only") {
        run(directory.path(), 512, 128);
        return 0;
    }
    const bool extended = argc > 1 && std::string_view(argv[1]) == "--extended";
    for (int side : {512, 2048})
        for (int brush_size : {8, 128}) run(directory.path(), side, brush_size);
    if (extended)
        for (int side : {4096, 8192}) run(directory.path(), side, 32);
    std::cout << "STROKE_PERFORMANCE_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
