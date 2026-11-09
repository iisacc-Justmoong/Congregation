#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::uint32_t pixel(const Artwork& art, int x, int y) {
    auto image = renderFrameRegion(art.document(), 0, {{x,y},{1,1}}, {1,1});
    check(image.ok(), "cannot render shape pixel");
    return image.pixels.pixels[0];
}
Document makeDocument() {
    Document d; d.extent={9000,9000};
    d.assets.emplace_back(RasterAsset{"base",makeRasterLayer(8,8,0)});
    d.layers.emplace_back(StaticBitmapLayer{{"base-layer","Base"},StaticSource{"base"}});
    return d;
}
int main() try {
    iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());
    check(dir.isValid(), "temporary directory unavailable");
    Artwork art(dir.path());
    check(art.replace(makeDocument()) && art.set_selection(SelectionGeometry::Rectangle,{10,10},{30,30}), "large shape fixture failed");
    check(art.shape("rectangle",{0,0},{40,40},0xff0000ffU,0,true), "selected tiled shape failed");
    check((pixel(art,20,20)>>24)>0 && (pixel(art,5,20)>>24)==0, "tiled shape escaped selection");
    check(std::holds_alternative<StaticVectorLayer>(art.document().layers.back()), "shape is not vector content");
    check(art.undo() && (pixel(art,20,20)>>24)==0 && art.tiled_selection_handle(), "shape undo lost selection");
    check(art.redo() && (pixel(art,20,20)>>24)>0 && (pixel(art,5,20)>>24)==0, "shape redo changed coverage");
    auto encoded=art.snapshot(); auto restored=decodeIisc({reinterpret_cast<const std::uint8_t*>(encoded.data()),encoded.size()});
    check(restored.ok(), "selected tiled shape save failed");
    auto savedInside=renderFrameRegion(restored.document,0,{{20,20},{1,1}},{1,1});
    auto savedOutside=renderFrameRegion(restored.document,0,{{5,20},{1,1}},{1,1});
    check(savedInside.ok() && savedOutside.ok() && (savedInside.pixels.pixels[0]>>24)>0 && (savedOutside.pixels.pixels[0]>>24)==0, "selected shape did not survive save");
    ::AffineTransform moved{}; moved.translationX=10;
    check(art.transform(moved) && (pixel(art,15,20)>>24)==0 && (pixel(art,35,20)>>24)>0, "shape mask did not move with layer");
    check(art.undo() && art.undo() && art.can_redo(), "shape redo fixture failed");
    const auto revision=art.revision();
    check(art.shape("rectangle",{200,200},{220,220},0xff0000ffU,0,true) && art.can_redo() && art.revision()==revision, "unselected shape lost redo");
    Artwork inverted(dir.path(),256*1024);
    check(inverted.replace(makeDocument()) && inverted.set_selection(SelectionGeometry::Rectangle,{10,10},{30,30}) && inverted.invert_selection(), "inverted shape fixture failed");
    check(inverted.shape("rectangle",{0,0},{40,40},0xff0000ffU,0,true), "bounded inverted shape failed");
    check((pixel(inverted,5,20)>>24)>0 && (pixel(inverted,20,20)>>24)==0, "inverted mask coverage failed");
    check(inverted.undo() && inverted.can_redo(), "inverted shape undo failed");
    const auto before=inverted.revision();
    check(!inverted.shape("rectangle",{0,0},{500,500},0xff0000ffU,0,true) && inverted.can_redo() && inverted.revision()==before, "over-budget shape changed history");
    Document infinite; infinite.canvasMode=CanvasMode::Infinite; infinite.infiniteCanvas.origin={-64,-64}; infinite.extent={128,128};
    infinite.assets.emplace_back(RasterAsset{"infinite-base",makeRasterLayer(8,8,0)});
    infinite.layers.emplace_back(StaticBitmapLayer{{"infinite-layer","Base"},StaticSource{"infinite-base"}});
    Artwork infiniteArt(dir.path());
    check(infiniteArt.replace(std::move(infinite)) && infiniteArt.set_selection(SelectionGeometry::Rectangle,{-30,-30},{-10,-10}), "infinite shape fixture failed");
    check(infiniteArt.shape("ellipse",{-40,-40},{0,0},0xff0000ffU,0,true), "infinite selected shape failed");
    check((pixel(infiniteArt,-20,-20)>>24)>0 && (pixel(infiniteArt,-35,-20)>>24)==0, "infinite shape mask coordinates failed");
    check(infiniteArt.undo() && (pixel(infiniteArt,-20,-20)>>24)==0 && infiniteArt.redo() && (pixel(infiniteArt,-20,-20)>>24)>0, "infinite shape history failed");
    std::cout << "TILED_SHAPE_INTEGRATION_OK\n"; return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
