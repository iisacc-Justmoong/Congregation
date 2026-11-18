#include "LayerPreview.h"
#include <LVUI>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace congregation::native
{
namespace
{
using namespace iiSharedCanvas;
constexpr int side = static_cast<int>(lvui::design_icon_default_extent);
constexpr int max_samples_per_axis = 8;

template<class Sample>
std::shared_ptr<const ii::gui::RasterImage> sampled_preview(int width, int height, Sample sample)
{
    if (width <= 0 || height <= 0) return {};
    const auto scale = std::min(double(side) / width, double(side) / height);
    const auto out_width = std::clamp(int(std::lround(width * scale)), 1, side);
    const auto out_height = std::clamp(int(std::lround(height * scale)), 1, side);
    const int offset_x = (side - out_width) / 2, offset_y = (side - out_height) / 2;
    const int sx = int(std::clamp((std::int64_t(width) + out_width - 1) / out_width, std::int64_t(1), std::int64_t(max_samples_per_axis)));
    const int sy = int(std::clamp((std::int64_t(height) + out_height - 1) / out_height, std::int64_t(1), std::int64_t(max_samples_per_axis)));
    std::vector<std::uint8_t> rgba(std::size_t(side * side * 4), 0);
    for (int y = 0; y < out_height; ++y) for (int x = 0; x < out_width; ++x) {
        std::uint64_t alpha = 0, red = 0, green = 0, blue = 0;
        for (int j = 0; j < sy; ++j) for (int i = 0; i < sx; ++i) {
            const int px = std::min(width - 1, int((double(x) + (i + .5) / sx) * width / out_width));
            const int py = std::min(height - 1, int((double(y) + (j + .5) / sy) * height / out_height));
            const std::uint32_t value = sample(px, py);
            const auto a = value >> 24;
            alpha += a; red += ((value >> 16) & 255) * a;
            green += ((value >> 8) & 255) * a; blue += (value & 255) * a;
        }
        auto* p = rgba.data() + (std::size_t(y + offset_y) * side + x + offset_x) * 4;
        if (alpha) { p[0] = std::uint8_t(red / alpha); p[1] = std::uint8_t(green / alpha); p[2] = std::uint8_t(blue / alpha); }
        p[3] = std::uint8_t(alpha / (sx * sy));
    }
    auto image = ii::gui::rgba_image(side, side, std::move(rgba));
    return image ? std::move(*image) : nullptr;
}

std::shared_ptr<const ii::gui::RasterImage> raster_preview(const ::RasterLayer& raster)
{
    if (raster.width <= 0 || raster.height <= 0 ||
        std::uint64_t(raster.width) * raster.height != raster.pixels.size()) return {};
    return sampled_preview(raster.width, raster.height, [&](int x, int y) {
        return raster.pixels[std::size_t(y) * raster.width + x];
    });
}

std::shared_ptr<const ii::gui::RasterImage> vector_preview(VectorAsset vector)
{
    if (vector.viewport.width <= 0 || vector.viewport.height <= 0) return {};
    Document preview;
    preview.extent = vector.viewport;
    const auto id = vector.id;
    preview.assets.emplace_back(std::move(vector));
    preview.layers.emplace_back(StaticVectorLayer{{"preview", "Preview"}, id});
    const auto scale = std::min(double(side) / preview.extent.width, double(side) / preview.extent.height);
    const CanvasExtent extent{std::clamp(int(std::lround(preview.extent.width * scale)), 1, side),
                              std::clamp(int(std::lround(preview.extent.height * scale)), 1, side)};
    const auto rendered = renderFrameLayerTiles(preview, 0, 0, {{canvasRegion(preview), extent, RasterSampling::Smooth}});
    if (!rendered.ok() || rendered.tiles.empty()) return {};
    return raster_preview(rendered.tiles.front().pixels);
}

std::uint32_t gray(double value)
{
    if (!std::isfinite(value)) return 0xff000000U;
    const auto v = std::uint32_t(std::lround(std::clamp(value, 0., 1.) * 255));
    return 0xff000000U | (v << 16) | (v << 8) | v;
}
} // namespace

std::optional<LayerPreviewKey> layer_preview_key(const Document& doc, const Layer& layer, FrameIndex frame)
{
    const auto* asset = resolveAssetAt(doc, layer, frame);
    if (!asset || std::holds_alternative<IpAdapterAsset>(*asset)) return {};
    LayerPreviewKey key{assetId(*asset), {}};
    if (const auto* video = std::get_if<VideoLayer>(&layer)) {
        key.video_frame = videoFrameIndexAt(doc, *video, frame);
        if (!key.video_frame) return {};
    }
    return key;
}

std::shared_ptr<const ii::gui::RasterImage> render_layer_preview(
    const Document& doc, const Layer& layer, FrameIndex frame)
{
    const auto* asset = resolveAssetAt(doc, layer, frame);
    if (!asset) return {};
    try {
        return std::visit([&](const auto& value) -> std::shared_ptr<const ii::gui::RasterImage> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, RasterAsset>) return raster_preview(value.pixels);
            else if constexpr (std::is_same_v<T, VideoAsset>) {
                const auto* video = std::get_if<VideoLayer>(&layer);
                const auto* frame_pixels = video ? resolveVideoFrameAt(doc, *video, frame) : nullptr;
                return frame_pixels ? raster_preview(*frame_pixels) : nullptr;
            } else if constexpr (std::is_same_v<T, VectorAsset>) return vector_preview(value);
            else if constexpr (std::is_same_v<T, PoseAsset>) return vector_preview(poseVectorPreview(value));
            else if constexpr (std::is_same_v<T, MlsdAsset>) return vector_preview(mlsdVectorPreview(value));
            else if constexpr (std::is_same_v<T, ChunkedRasterAsset>) {
                const auto* board = layerProperties(layer).artboardId ? findArtboard(doc, *layerProperties(layer).artboardId) : nullptr;
                const auto extent = board ? board->region.extent : doc.extent;
                const int chunk = doc.infiniteCanvas.chunkSize;
                if (chunk <= 0) return {};
                const auto key = [](std::int32_t column, std::int32_t row) {
                    return (std::uint64_t(std::uint32_t(row)) << 32) | std::uint32_t(column);
                };
                std::unordered_map<std::uint64_t, const RasterChunk*> tiles;
                tiles.reserve(value.chunks.size());
                for (const auto& tile : value.chunks) tiles.emplace(key(tile.column, tile.row), &tile);
                return sampled_preview(extent.width, extent.height, [&](int x, int y) -> std::uint32_t {
                    const auto found = tiles.find(key(x / chunk, y / chunk));
                    if (found == tiles.end()) return 0;
                    const auto* tile = found->second;
                    const int tx = x % chunk, ty = y % chunk;
                    if (tx >= tile->pixels.width || ty >= tile->pixels.height ||
                        tile->pixels.pixels.size() != std::size_t(tile->pixels.width) * tile->pixels.height) return 0;
                    return tile->pixels.pixels[std::size_t(ty) * tile->pixels.width + tx];
                });
            } else if constexpr (std::is_same_v<T, IpAdapterAsset>) return {};
            else {
                const auto width = value.viewport.width, height = value.viewport.height;
                if (width <= 0 || height <= 0) return {};
                const auto count = std::uint64_t(width) * height;
                if constexpr (std::is_same_v<T, DepthAsset>) {
                    if (value.values.size() != count) return {};
                    return sampled_preview(width, height, [&](int x, int y) { return gray(value.values[std::size_t(y) * width + x]); });
                } else if constexpr (std::is_same_v<T, LineArtAsset>) {
                    if (value.coverage.size() != count) return {};
                    return sampled_preview(width, height, [&](int x, int y) { return gray(1 - value.coverage[std::size_t(y) * width + x]); });
                } else if constexpr (std::is_same_v<T, CannyAsset> || std::is_same_v<T, ScribbleAsset>) {
                    if (value.mask.size() != count) return {};
                    return sampled_preview(width, height, [&](int x, int y) { return gray(value.mask[std::size_t(y) * width + x] ? 1 : 0); });
                } else if constexpr (std::is_same_v<T, NormalMapAsset>) {
                    if (value.samples.size() != count) return {};
                    return sampled_preview(width, height, [&](int x, int y) -> std::uint32_t {
                        const auto& n = value.samples[std::size_t(y) * width + x];
                        if (!n.valid) return 0xff000000U;
                        const auto c = [](double v) { return std::uint32_t(std::lround((std::clamp(std::isfinite(v) ? v : 0., -1., 1.) + 1) * 127.5)); };
                        return 0xff000000U | (c(n.x) << 16) | (c(n.y) << 8) | c(n.z);
                    });
                } else {
                    if (value.colors.size() != count) return {};
                    return sampled_preview(width, height, [&](int x, int y) -> std::uint32_t {
                        const auto& c = value.colors[std::size_t(y) * width + x];
                        return 0xff000000U | (std::uint32_t(c.red) << 16) | (std::uint32_t(c.green) << 8) | c.blue;
                    });
                }
            }
        }, *asset);
    } catch (const std::exception&) {
        return {};
    }
}
} // namespace congregation::native
