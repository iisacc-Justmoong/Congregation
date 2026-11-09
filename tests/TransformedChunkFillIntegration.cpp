#include "DocumentFixture.h"
#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t localPixel(const Artwork& art,std::string_view id,FrameIndex frame,int x,int y){
    for(const auto& layer:art.document().layers) if(layerProperties(layer).id==id) {
        const auto* asset=resolveAssetAt(art.document(),layer,frame);
        const auto* chunks=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;
        check(chunks!=nullptr,"transformed chunk asset missing");
        const int size=art.document().infiniteCanvas.chunkSize;
        const auto divide=[size](int v){const int q=v/size,r=v%size;return r<0?q-1:q;};
        const int col=divide(x),row=divide(y);const auto* chunk=findRasterChunk(*chunks,col,row);
        return chunk?chunk->pixels.pixels[std::size_t(y-row*size)*size+x-col*size]:0;
    }
    throw std::runtime_error("transformed layer missing");
}
Document fixture(AffineTransform transform){
    Document d;d.canvasMode=CanvasMode::Infinite;d.infiniteCanvas.origin={-64,-64};d.extent={128,128};d.timeline={{24,1},3};
    auto pixels=makeRasterLayer(256,256,0xffff0000U);
    d.assets.emplace_back(ChunkedRasterAsset{"shared",{{-1,-1,std::move(pixels)}}});
    d.layers.emplace_back(StaticBitmapLayer{{"base","Base"},StaticSource{"shared"}});
    LayerProperties paint{"paint","Paint"};paint.transform=transform;
    d.layers.emplace_back(StaticBitmapLayer{paint,StaticSource{"shared"}});return d;
}
int main() try {
    iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
    const std::array transforms{AffineTransform{1,0,0,1,5,7},AffineTransform{2,0,0,2,5,7},AffineTransform{0,1,-1,0,5,7}};
    for(const auto& t:transforms){
        Artwork art(dir.path());const double wx=t.m11*(-9.5)+t.m21*(-9.5)+t.translationX,wy=t.m12*(-9.5)+t.m22*(-9.5)+t.translationY;
        check(art.replace(fixture(t))&&art.select("paint")&&art.set_selection(SelectionGeometry::Rectangle,{wx-2,wy-2},{wx+2,wy+2}),"transformed fill fixture failed");
        FloodOptions fill;fill.current_layer=true;fill.target_current=true;
        if(!art.fill_at({wx,wy},0xff0000ffU,1,fill))throw std::runtime_error("transformed chunk fill failed: "+art.error());
        check(localPixel(art,"paint",1,-10,-10)==0xff0000ffU && localPixel(art,"paint",1,-3,-10)==0xffff0000U,"transformed fill missed selected local pixels or escaped selection");
        check(localPixel(art,"paint",0,-10,-10)==0xffff0000U && localPixel(art,"paint",2,-10,-10)==0xffff0000U && localPixel(art,"base",1,-10,-10)==0xffff0000U,"transformed fill changed shared asset or another frame");
        check(art.undo()&&localPixel(art,"paint",1,-10,-10)==0xffff0000U&&art.redo()&&localPixel(art,"paint",1,-10,-10)==0xff0000ffU,"transformed fill history failed");
        const auto path=dir.path()/"transformed-fill.iisc";check(art.save(path),"transformed fill save failed");Artwork reopened(dir.path());check(open_document_fixture(reopened, path)&&localPixel(reopened,"paint",1,-10,-10)==0xff0000ffU&&localPixel(reopened,"base",1,-10,-10)==0xffff0000U,"transformed fill reopen failed");
    }
    Document empty;empty.canvasMode=CanvasMode::Infinite;empty.infiniteCanvas.origin={-64,-64};empty.extent={128,128};empty.assets.emplace_back(ChunkedRasterAsset{"outside",{}});LayerProperties p{"outside-layer","Outside"};p.transform.translationX=300;empty.layers.emplace_back(StaticBitmapLayer{p,StaticSource{"outside"}});
    Artwork outside(dir.path());check(outside.replace(std::move(empty))&&outside.set_selection(SelectionGeometry::Rectangle,{-3,-3},{3,3}),"outside-local fixture failed");FloodOptions fill;fill.current_layer=true;fill.target_current=true;check(outside.fill_at({0,0},0xff00ff00U,0,fill),"outside-canvas local fill failed");check(localPixel(outside,"outside-layer",0,-300,0)==0xff00ff00U && outside.document().infiniteCanvas.origin==CanvasOrigin{-64,-64} && outside.document().extent==CanvasExtent{128,128},"outside-local fill altered canvas or missed pixel");check(outside.undo()&&localPixel(outside,"outside-layer",0,-300,0)==0&&outside.redo(),"outside-local history failed");
    check(outside.clear_selection()&&outside.fill_at({0,0},0xff0000ffU,0,fill),"unselected transformed fill failed");
    check(localPixel(outside,"outside-layer",0,-300,0)==0xff0000ffU && !outside.has_selection() && outside.undo() && localPixel(outside,"outside-layer",0,-300,0)==0xff00ff00U && outside.redo(),"unselected transformed fill pixels or history failed");
    auto semiDoc=fixture(AffineTransform{1,0,0,1,5,7});auto* semi=findChunkedRasterAsset(semiDoc,"shared");check(semi!=nullptr,"alpha fixture missing");semi->chunks.front().pixels.pixels[246*256+246]=0x80112233U;
    Artwork alpha(dir.path());check(alpha.replace(std::move(semiDoc))&&alpha.select("paint")&&alpha.set_selection(SelectionGeometry::Rectangle,{-7,-5},{-3,-1}),"preserve alpha fixture failed");
    FloodOptions preserve;preserve.current_layer=true;preserve.target_current=true;preserve.preserve_alpha=true;
    check(alpha.fill_at({-4.5,-2.5},0xff00ff00U,1,preserve)&&((localPixel(alpha,"paint",1,-10,-10)>>24)==128)&&localPixel(alpha,"base",1,-10,-10)==0x80112233U,"transformed fill did not preserve alpha or shared source");
    std::cout<<"TRANSFORMED_CHUNK_FILL_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
