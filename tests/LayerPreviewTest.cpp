#include <Timeline/LayerPreview.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

using namespace congregation::native;
using namespace iiSharedCanvas;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::array<unsigned, 4> pixel(const std::shared_ptr<const ii::gui::RasterImage>& image, int x, int y) {
    check(image && image->width() == 18 && image->height() == 18, "preview has wrong size");
    const auto rgba = image->rgba(); const auto index = (std::size_t(y) * 18 + x) * 4;
    return {rgba[index], rgba[index + 1], rgba[index + 2], rgba[index + 3]};
}
}
int main() try {
    Document doc; doc.extent = {2, 2}; doc.timeline.frameCount = 2;
    doc.assets.emplace_back(RasterAsset{"red", makeRasterLayer(2, 2, 0xffff0000U)});
    doc.assets.emplace_back(RasterAsset{"green", makeRasterLayer(2, 2, 0xff00ff00U)});
    DynamicBitmapLayer bitmap; bitmap.properties = {"paint", "Paint"};
    bitmap.content.frameIndices = {0, 1};
    doc.layers.emplace_back(bitmap);
    doc.frames = {{0, {{"paint", "red"}}}, {1, {{"paint", "green"}}}};
    check(pixel(render_layer_preview(doc, doc.layers[0], 0), 9, 9) == std::array<unsigned, 4>{255, 0, 0, 255}, "raster preview differs from frame zero");
    check(pixel(render_layer_preview(doc, doc.layers[0], 1), 9, 9) == std::array<unsigned, 4>{0, 255, 0, 255}, "dynamic preview differs from frame one");
    check(layer_preview_key(doc, doc.layers[0], 0) != layer_preview_key(doc, doc.layers[0], 1), "dynamic source identity did not change");
    auto& green = std::get<RasterAsset>(doc.assets[1]);
    std::ranges::fill(green.pixels.pixels, 0x8000ff00U);
    check(pixel(render_layer_preview(doc, doc.layers[0], 1), 9, 9) == std::array<unsigned, 4>{0, 255, 0, 128}, "preview lost source transparency");
    std::ranges::fill(green.pixels.pixels, 0xff00ff00U);
    layerProperties(doc.layers[0]).visible = false;
    check(pixel(render_layer_preview(doc, doc.layers[0], 1), 9, 9) == std::array<unsigned, 4>{0, 255, 0, 255}, "hidden layer lost its preview");
    layerProperties(doc.layers[0]).frameRange = LayerFrameRange{0, 0};
    check(!layer_preview_key(doc, doc.layers[0], 1) && !render_layer_preview(doc, doc.layers[0], 1), "out-of-range frame must have no image");
    std::ranges::fill(std::get<RasterAsset>(doc.assets[0]).pixels.pixels, 0U);
    check(pixel(render_layer_preview(doc, doc.layers[0], 0), 9, 9)[3] == 0, "empty content must stay transparent");

    Document vector_doc; vector_doc.extent = {8, 4};
    VectorPath path; path.commands = {MoveTo{{0, 0}}, LineTo{{8, 0}}, LineTo{{8, 4}}, LineTo{{0, 4}}, ClosePath{}};
    path.fill = SolidPaint{0xff0000ffU};
    vector_doc.assets.emplace_back(VectorAsset{"vector", {8, 4}, {path}});
    vector_doc.layers.emplace_back(StaticVectorLayer{{"vector-layer", "Vector", false}, StaticVectorContent{"vector"}});
    const auto vector_image = render_layer_preview(vector_doc, vector_doc.layers[0], 0);
    check(pixel(vector_image, 9, 9) == std::array<unsigned, 4>{0, 0, 255, 255}, "hidden vector preview is not rasterized from its geometry");
    check(pixel(vector_image, 9, 0)[3] == 0, "vector preview must preserve aspect ratio");

    Document video_doc; video_doc.extent = {2, 2}; video_doc.timeline.frameCount = 2;
    video_doc.assets.emplace_back(VideoAsset{"video", {24, 1}, {makeRasterLayer(2, 2, 0xffff0000U), makeRasterLayer(2, 2, 0xff0000ffU)}});
    video_doc.layers.emplace_back(VideoLayer{{"video-layer", "Video"}, StaticSource{"video"}});
    check(pixel(render_layer_preview(video_doc, video_doc.layers[0], 1), 9, 9) == std::array<unsigned, 4>{0, 0, 255, 255}, "video preview did not follow the source frame");
    check(layer_preview_key(video_doc, video_doc.layers[0], 0) != layer_preview_key(video_doc, video_doc.layers[0], 1), "video frame identity did not change");

    Document chunk_doc; chunk_doc.extent = {4, 2}; chunk_doc.infiniteCanvas.chunkSize = 2;
    ChunkedRasterAsset chunks{"chunks", {{0, 0, makeRasterLayer(2, 2, 0xffff0000U)}, {1, 0, makeRasterLayer(2, 2, 0xff00ff00U)}}};
    chunk_doc.assets.emplace_back(std::move(chunks));
    chunk_doc.layers.emplace_back(StaticBitmapLayer{{"chunk-layer", "Chunks"}, StaticBitmapContent{"chunks"}});
    const auto chunk_image = render_layer_preview(chunk_doc, chunk_doc.layers[0], 0);
    check(pixel(chunk_image, 4, 9) == std::array<unsigned, 4>{255, 0, 0, 255} &&
          pixel(chunk_image, 13, 9) == std::array<unsigned, 4>{0, 255, 0, 255}, "chunk preview lost source tiles");

    Document control_doc; control_doc.extent = {2, 2};
    control_doc.assets.emplace_back(CannyAsset{"edge", {2, 2}, {1, 1, 1, 1}});
    control_doc.layers.emplace_back(CannyLayer{{"edge-layer", "Edges"}, StaticSource{"edge"}});
    check(pixel(render_layer_preview(control_doc, control_doc.layers[0], 0), 9, 9) == std::array<unsigned, 4>{255, 255, 255, 255}, "spatial control layer preview is missing");
    IpAdapterAsset embedding; embedding.id = "embedding";
    control_doc.assets.emplace_back(std::move(embedding));
    control_doc.layers.emplace_back(IpAdapterLayer{{"embedding-layer", "Embedding"}, StaticSource{"embedding"}});
    check(!layer_preview_key(control_doc, control_doc.layers[1], 0) &&
          !render_layer_preview(control_doc, control_doc.layers[1], 0),
          "nonspatial embeddings must not receive fabricated artwork");
    std::cout << "LAYER_PREVIEW_OK\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
