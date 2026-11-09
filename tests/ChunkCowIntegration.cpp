#include "DocumentFixture.h"
#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Document fixture(unsigned frames = 3) {
    Document document;
    document.canvasMode = CanvasMode::Infinite;
    document.infiniteCanvas.origin = {-64, -64};
    document.extent = {128, 128};
    document.timeline = {{24, 1}, frames};
    auto pixels = makeRasterLayer(256, 256, 0xffff0000U);
    document.assets.emplace_back(ChunkedRasterAsset{"shared", {{-1, -1, std::move(pixels)}}});
    document.layers.emplace_back(StaticBitmapLayer{{"base", "Base"}, StaticSource{"shared"}});
    document.layers.emplace_back(StaticBitmapLayer{{"paint", "Paint"}, StaticSource{"shared"}});
    return document;
}
std::uint32_t pixel(const Artwork& artwork, std::string_view id, FrameIndex frame, int x, int y) {
    for (const auto& layer : artwork.document().layers) {
        if (layerProperties(layer).id != id) continue;
        const auto* asset = resolveAssetAt(artwork.document(), layer, frame);
        const auto* chunks = asset ? std::get_if<ChunkedRasterAsset>(asset) : nullptr;
        check(chunks != nullptr, "chunk frame missing");
        const int size = artwork.document().infiniteCanvas.chunkSize;
        const auto divide = [size](int value) { const int q = value / size, r = value % size; return r < 0 ? q - 1 : q; };
        const int col = divide(x), row = divide(y);
        const auto* chunk = findRasterChunk(*chunks, col, row);
        return chunk ? chunk->pixels.pixels[std::size_t(y-row*size)*size+x-col*size] : 0;
    }
    throw std::runtime_error("layer missing");
}
int main() try {
    iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());
    check(dir.isValid(), "temporary directory unavailable");
    Artwork fill(dir.path());
    check(fill.replace(fixture()) && fill.select("paint") && fill.set_selection(SelectionGeometry::Rectangle, {-12,-12}, {-8,-8}), "chunk fill fixture failed");
    FloodOptions options; options.current_layer = true; options.target_current = true;
    check(fill.fill_at({-10,-10}, 0xff0000ffU, 1, options), "shared chunk frame fill failed");
    check(pixel(fill,"paint",1,-10,-10) == 0xff0000ffU && pixel(fill,"paint",1,-6,-10) == 0xffff0000U, "chunk fill selection failed");
    check(pixel(fill,"paint",0,-10,-10) == 0xffff0000U && pixel(fill,"paint",2,-10,-10) == 0xffff0000U && pixel(fill,"base",1,-10,-10) == 0xffff0000U, "chunk fill changed another frame or shared layer");
    check(fill.undo() && pixel(fill,"paint",1,-10,-10) == 0xffff0000U && fill.redo() && pixel(fill,"paint",1,-10,-10) == 0xff0000ffU, "chunk fill history failed");
    check(fill.fill_at({-10,-10}, 0xff00ff00U, 1, options), "dynamic chunk frame fill failed");
    check(pixel(fill,"paint",1,-10,-10) == 0xff00ff00U && pixel(fill,"paint",0,-10,-10) == 0xffff0000U && pixel(fill,"paint",2,-10,-10) == 0xffff0000U && pixel(fill,"base",1,-10,-10) == 0xffff0000U, "dynamic chunk fill changed another frame or layer");
    check(fill.undo() && pixel(fill,"paint",1,-10,-10) == 0xff0000ffU && fill.redo() && pixel(fill,"paint",1,-10,-10) == 0xff00ff00U && fill.undo(), "dynamic chunk fill history failed");
    const auto path = dir.path()/"chunk-frame.iisc";
    check(fill.save(path), "chunk frame save failed");
    Artwork reopened(dir.path());
    check(open_document_fixture(reopened, path) && pixel(reopened,"paint",0,-10,-10) == 0xffff0000U && pixel(reopened,"paint",1,-10,-10) == 0xff0000ffU && pixel(reopened,"base",1,-10,-10) == 0xffff0000U, "chunk frame reopen failed");
    Artwork brush(dir.path());
    check(brush.replace(fixture()) && brush.select("paint") && brush.set_selection(SelectionGeometry::Rectangle, {-12,-12}, {-8,-8}), "chunk brush fixture failed");
    BitmapBrush ink; ink.size = 4; ink.argb = 0xff0000ffU; ink.pressureToOpacityEnabled = false;
    StrokePoint a; a.position = {-10,-10}; a.time = 0;
    StrokePoint b; b.position = {-9,-10}; b.time = .1;
    check(brush.begin_input(a, ink, 1) && brush.end_input(b), "shared chunk frame brush failed");
    const auto painted = pixel(brush,"paint",1,-10,-10);
    check(painted != 0xffff0000U && pixel(brush,"paint",1,-6,-10) == 0xffff0000U && pixel(brush,"paint",0,-10,-10) == 0xffff0000U && pixel(brush,"base",1,-10,-10) == 0xffff0000U, "chunk brush changed unselected or shared pixels");
    check(brush.undo() && pixel(brush,"paint",1,-10,-10) == 0xffff0000U && brush.redo() && pixel(brush,"paint",1,-10,-10) == painted, "chunk brush history failed");
    auto eraser = ink; eraser.eraser = true;
    check(brush.begin_input(a, eraser, 1, StrokeTaper{.25,.25,.25,1}) && brush.end_input(b), "dynamic tapered chunk eraser failed");
    const auto erased = pixel(brush,"paint",1,-10,-10);
    check((erased >> 24) < (painted >> 24) && pixel(brush,"paint",0,-10,-10) == 0xffff0000U && pixel(brush,"base",1,-10,-10) == 0xffff0000U, "dynamic eraser changed shared content");
    check(brush.undo() && pixel(brush,"paint",1,-10,-10) == painted && brush.redo() && pixel(brush,"paint",1,-10,-10) == erased && brush.undo(), "dynamic chunk eraser history failed");
    check(brush.undo(), "chunk brush no-op setup failed");
    const auto revision = brush.revision(); ink.opacity = 0;
    check(brush.begin_input(a, ink, 1) && brush.end_input(b) && brush.revision() == revision && brush.can_redo(), "chunk brush no-op lost redo");
    Artwork single(dir.path());
    check(single.replace(fixture(1)) && single.select("paint") && single.set_selection(SelectionGeometry::Rectangle, {-12,-12}, {-8,-8}) && single.fill_at({-10,-10}, 0xff0000ffU, 0, options), "single-frame shared chunk fill failed");
    check(pixel(single,"paint",0,-10,-10) == 0xff0000ffU && pixel(single,"base",0,-10,-10) == 0xffff0000U && single.undo() && single.redo(), "single-frame shared fill changed the original");
    Artwork unselected(dir.path());
    check(unselected.replace(fixture()) && unselected.select("paint"), "unselected shared chunk fixture failed");
    BitmapBrush freeInk; freeInk.size = 4; freeInk.argb = 0xff0000ffU; freeInk.pressureToOpacityEnabled = false;
    StrokePoint freeA; freeA.position = {-10,-10}; freeA.time = 0;
    StrokePoint freeB; freeB.position = {-9,-10}; freeB.time = .1;
    check(unselected.begin_input(freeA, freeInk, 1) && unselected.end_input(freeB), "unselected shared chunk stroke failed");
    const auto unselectedPaint = pixel(unselected,"paint",1,-10,-10);
    check(unselectedPaint != 0xffff0000U && pixel(unselected,"paint",0,-10,-10) == 0xffff0000U && pixel(unselected,"paint",2,-10,-10) == 0xffff0000U && pixel(unselected,"base",1,-10,-10) == 0xffff0000U, "unselected stroke changed another frame or shared layer");
    check(unselected.undo() && pixel(unselected,"paint",1,-10,-10) == 0xffff0000U && unselected.redo() && pixel(unselected,"paint",1,-10,-10) == unselectedPaint, "unselected shared stroke history failed");
    check(unselected.undo(), "unselected stroke no-op setup failed");
    const auto freeRevision = unselected.revision(); auto zeroInk=freeInk; zeroInk.opacity=0;
    check(unselected.begin_input(freeA, zeroInk, 1) && unselected.end_input(freeB) && unselected.revision() == freeRevision && unselected.can_redo(), "unselected no-op discarded redo");
    check(unselected.redo(), "unselected stroke redo after no-op failed");
    auto taperedEraser=freeInk;taperedEraser.eraser=true;
    check(unselected.begin_input(freeA, taperedEraser, 1, StrokeTaper{.25,.25,.25,1}) && unselected.end_input(freeB), "unselected dynamic tapered eraser failed");
    check((pixel(unselected,"paint",1,-10,-10)>>24)<(unselectedPaint>>24) && pixel(unselected,"paint",0,-10,-10)==0xffff0000U && pixel(unselected,"base",1,-10,-10)==0xffff0000U, "unselected tapered eraser changed shared content");
    check(unselected.undo() && pixel(unselected,"paint",1,-10,-10)==unselectedPaint && unselected.redo(), "unselected tapered eraser history failed");
    auto singleTaper=fixture(1);singleTaper.layers.erase(singleTaper.layers.begin());
    Artwork oneChunk(dir.path());check(oneChunk.replace(std::move(singleTaper)), "unshared chunk taper fixture failed");
    check(oneChunk.begin_input(freeA,taperedEraser,0,StrokeTaper{.25,.25,.25,1}) && oneChunk.end_input(freeB), "unshared unselected chunk taper failed");
    check((pixel(oneChunk,"paint",0,-10,-10)>>24)<255 && oneChunk.undo() && pixel(oneChunk,"paint",0,-10,-10)==0xffff0000U && oneChunk.redo(), "unshared unselected taper history failed");
    check(oneChunk.undo(), "unshared taper no-op fixture failed");
    const auto oneRevision=oneChunk.revision(); auto zeroTaper=taperedEraser;zeroTaper.opacity=0;
    check(oneChunk.begin_input(freeA,zeroTaper,0,StrokeTaper{.25,.25,.25,1}) && oneChunk.end_input(freeB) && oneChunk.revision()==oneRevision && oneChunk.can_redo(), "unshared zero taper lost redo");
    std::cout << "CHUNK_COW_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
