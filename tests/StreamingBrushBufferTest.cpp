#include "../App/Native/StreamingBrush.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
thread_local bool measuring = false;
thread_local std::size_t requested = 0;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void checkSamples(const std::vector<RasterSample>& actual, const std::vector<RasterSample>& expected) {
    check(actual.size() == expected.size(), "streamed brush changed the sample count");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& a = actual[i]; const auto& b = expected[i];
        check(a.position.x == b.position.x && a.position.y == b.position.y && a.argb == b.argb &&
              a.opacityCap == b.opacityCap && a.blendMode == b.blendMode,
              "streamed brush changed projected pixel order");
    }
}
void checkClippedSequence(BrushState state) {
    state.rasterizer.spacing = 16;
    const RasterSourceSampler sampler{};
    const DevicePixelRect clip{{0, 0}, 96, 96};
    RasterDabStream reference;
    congregation::native::StreamingBrush brush(state, 0, 0, 1, 0);
    for (const auto& point : {StrokePoint{{5, 5}, .3, 1}, StrokePoint{{85, 5}, .9, 1.1}}) {
        auto dabs = appendRasterDabs(reference, point, state, false);
        auto expected = projectBrushDabs(BrushDabSpan{dabs}, state.rasterizer,
                                         RasterProjection{}, sampler, state.material);
        auto predicted = reference;
        auto tail = appendRasterDabs(predicted, point, state, true);
        auto expectedTail = projectBrushDabs(BrushDabSpan{tail}, state.rasterizer,
                                             RasterProjection{}, sampler, state.material);
        const auto outside = [&](const RasterSample& sample) {
            return sample.position.x < clip.origin.x || sample.position.y < clip.origin.y ||
                std::int64_t(sample.position.x) >= std::int64_t(clip.origin.x) + clip.width ||
                std::int64_t(sample.position.y) >= std::int64_t(clip.origin.y) + clip.height;
        };
        std::erase_if(expected, outside); std::erase_if(expectedTail, outside);
        auto result = brush.append(point, false, sampler, clip);
        check(bool(result), "clipped brush sequence failed");
        checkSamples(result->stable, expected);
        checkSamples(result->transient, expectedTail);
    }
}
}
void* operator new(std::size_t size) {
    if (measuring) requested += size;
    if (auto* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() try {
    BrushState state;
    state.rasterizer.radius = 64;
    state.rasterizer.brushSize = 128;
    state.rasterizer.spacing = 64;
    state.rasterizer.argb = 0x804f22ccU;
    StrokePoint point;
    point.position = {200, 200}; point.pressure = .65; point.time = 1;
    const RasterSourceSampler sampler{};
    RasterDabStream referenceStream;
    const auto dabs = appendRasterDabs(referenceStream, point, state, false);
    const auto expected = projectBrushDabs(BrushDabSpan{dabs}, state.rasterizer,
                                           RasterProjection{}, sampler, state.material);
    check(!expected.empty(), "reference brush did not project any pixels");
    congregation::native::StreamingBrush brush(state, 0, 0, 1, 0);
    requested = 0; measuring = true;
    auto result = brush.append(point, false, sampler, {{0, 0}, 512, 512});
    measuring = false;
    const auto bytes = requested;
    check(result && result->stable.size() == expected.size(), "streamed brush lost its first dab");
    check(result->projected_dabs == 0, "disabled input profiling counted brush dabs");
    checkSamples(result->stable, expected);
    const std::size_t estimatedBounds = 129 * 129;
    const auto budget = estimatedBounds * sizeof(RasterSample) + 128 * 1024;
    std::cout << "STREAMING_BRUSH_BUFFER samples=" << expected.size() << " requested=" << bytes
              << " budget=" << budget << '\n';
    check(bytes <= budget, "streamed brush copied its first projected sample buffer");
    congregation::native::StreamingBrush profiled(state, 0, 0, 1, 0);
    auto measured = profiled.append(point, false, sampler, {{0, 0}, 512, 512}, true);
    check(measured && measured->projected_dabs > 0, "input profiling did not count brush dabs");
    checkSamples(measured->stable, expected);
    checkClippedSequence(state);
    state.material.texture.enabled = true;
    state.material.texture.width = 4;
    state.material.texture.height = 4;
    state.material.texture.alpha = {0, 64, 128, 255, 255, 128, 64, 0,
                                    32, 192, 96, 224, 224, 96, 192, 32};
    state.material.texture.grainStrength = .75;
    const auto textured = projectBrushDabs(BrushDabSpan{dabs}, state.rasterizer,
                                           RasterProjection{}, sampler, state.material);
    const bool textureChangedPixels = textured.size() != expected.size() ||
        !std::equal(textured.begin(), textured.end(), expected.begin(), expected.end(),
                    [](const RasterSample& a, const RasterSample& b) {
                        return a.position.x == b.position.x && a.position.y == b.position.y &&
                            a.argb == b.argb && a.opacityCap == b.opacityCap && a.blendMode == b.blendMode;
                    });
    check(textureChangedPixels, "textured test brush did not affect projected pixels");
    checkClippedSequence(state);
    return 0;
} catch (const std::exception& error) {
    measuring = false;
    std::cerr << error.what() << '\n';
    return 1;
}
