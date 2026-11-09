#include "DocumentFixture.h"
#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <span>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t localPixel(const Artwork& art,std::string_view id,FrameIndex frame,int x,int y){
    for(const auto& layer:art.document().layers)if(layerProperties(layer).id==id){
        const auto* value=resolveAssetAt(art.document(),layer,frame);const auto* chunks=value?std::get_if<ChunkedRasterAsset>(value):nullptr;check(chunks!=nullptr,"stroke chunk asset missing");
        const int size=art.document().infiniteCanvas.chunkSize;const auto div=[size](int v){const int q=v/size,r=v%size;return r<0?q-1:q;};const int col=div(x),row=div(y);const auto* chunk=findRasterChunk(*chunks,col,row);
        return chunk?chunk->pixels.pixels[std::size_t(y-row*size)*size+x-col*size]:0;
    }throw std::runtime_error("stroke layer missing");
}
Document fixture(AffineTransform transform){
    Document doc;doc.canvasMode=CanvasMode::Infinite;doc.infiniteCanvas.origin={-64,-64};doc.extent={128,128};doc.timeline={{24,1},3};
    doc.assets.emplace_back(ChunkedRasterAsset{"shared",{{-1,-1,makeRasterLayer(256,256,0xffff0000U)}}});
    doc.layers.emplace_back(StaticBitmapLayer{{"base","Base"},StaticSource{"shared"}});LayerProperties p{"paint","Paint"};p.transform=transform;doc.layers.emplace_back(StaticBitmapLayer{p,StaticSource{"shared"}});return doc;
}
int main() try {
    iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
    const std::array transforms{AffineTransform{1,0,0,1,5,7},AffineTransform{2,0,0,2,5,7},AffineTransform{0,1,-1,0,5,7}};
    BitmapBrush brush;brush.size=4;brush.argb=0xff0000ffU;brush.pressureToOpacityEnabled=false;
    for(const auto& t:transforms){
        const auto map=[&](double x,double y){return Point{t.m11*x+t.m21*y+t.translationX,t.m12*x+t.m22*y+t.translationY};};
        const auto a=map(-9.5,-9.5),b=map(-8.5,-9.5);
        Artwork art(dir.path());check(art.replace(fixture(t))&&art.select("paint")&&art.set_selection(SelectionGeometry::Rectangle,{a.x-3,a.y-3},{a.x+3,a.y+3}),"transformed selected stroke fixture failed");
        StrokePoint start;start.position={a.x,a.y};start.time=0;StrokePoint end=start;end.position={b.x,b.y};end.time=.1;
        if(!art.begin_input(start,brush,1))throw std::runtime_error("transformed stroke begin failed: "+art.error());
        auto preview=art.selection_preview();check(preview&&!preview->tiles.empty(),"transformed stroke preview missing");
        FrameBitmapOverlay overlay{preview->asset_id,preview->tiles};auto rendered=renderFrameRegion(art.document(),1,{{int(std::floor(a.x)),int(std::floor(a.y))},{1,1}},{1,1},RasterSampling::Nearest,std::span(&overlay,1));check(rendered.ok()&&rendered.pixels.pixels[0]!=0xffff0000U,"transformed stroke preview missing visible paint");
        check(art.end_input(end),"transformed selected stroke end failed");const auto painted=localPixel(art,"paint",1,-10,-10);
        check(painted!=0xffff0000U&&localPixel(art,"paint",1,-3,-10)==0xffff0000U&&localPixel(art,"paint",0,-10,-10)==0xffff0000U&&localPixel(art,"base",1,-10,-10)==0xffff0000U,"transformed selection or frame isolation failed");
        check(art.undo()&&localPixel(art,"paint",1,-10,-10)==0xffff0000U&&art.can_redo(),"transformed stroke undo failed");
        const auto revision=art.revision();check(art.begin_input(start,brush,1),"transformed cancellation begin failed");art.cancel();check(art.revision()==revision&&art.can_redo()&&art.redo()&&localPixel(art,"paint",1,-10,-10)==painted,"transformed cancellation lost redo");
        auto eraser=brush;eraser.eraser=true;check(art.begin_input(start,eraser,1,StrokeTaper{.25,.25,.25,1})&&art.end_input(end),"transformed tapered eraser failed");
        check((localPixel(art,"paint",1,-10,-10)>>24)<(painted>>24)&&localPixel(art,"base",1,-10,-10)==0xffff0000U,"transformed eraser failed or changed shared source");
        check(art.undo()&&localPixel(art,"paint",1,-10,-10)==painted&&art.redo(),"transformed eraser history failed");
        const auto path=dir.path()/"transformed-stroke.iisc";check(art.save(path),"transformed stroke save failed");Artwork reopened(dir.path());check(open_document_fixture(reopened, path)&&localPixel(reopened,"paint",0,-10,-10)==0xffff0000U&&localPixel(reopened,"base",1,-10,-10)==0xffff0000U&&localPixel(reopened,"paint",1,-10,-10)==localPixel(art,"paint",1,-10,-10),"transformed stroke reopen failed");
    }
    Document empty;empty.canvasMode=CanvasMode::Infinite;empty.infiniteCanvas.origin={-64,-64};empty.extent={128,128};empty.assets.emplace_back(ChunkedRasterAsset{"outside",{}});LayerProperties p{"outside-layer","Outside"};p.transform.translationX=300;empty.layers.emplace_back(StaticBitmapLayer{p,StaticSource{"outside"}});
    Artwork outside(dir.path());check(outside.replace(std::move(empty)),"outside-local stroke fixture failed");StrokePoint point;point.position={0,0};point.time=0;
    check(outside.begin_input(point,brush,0)&&outside.end_input(point),"unselected outside-local stroke failed");
    check(localPixel(outside,"outside-layer",0,-300,0)!=0&&outside.document().infiniteCanvas.origin==CanvasOrigin{-64,-64}&&outside.document().extent==CanvasExtent{128,128},"outside-local stroke changed canvas or missed pixel");
    check(outside.undo()&&localPixel(outside,"outside-layer",0,-300,0)==0&&outside.redo(),"outside-local stroke history failed");
    auto wide=brush;wide.size=12;StrokePoint edge;edge.position={63,0};edge.time=0;
    check(outside.begin_input(edge,wide,0)&&outside.end_input(edge),"transformed canvas edge stroke failed");
    check(localPixel(outside,"outside-layer",0,-237,0)!=0&&localPixel(outside,"outside-layer",0,-236,0)==0,"transformed stroke escaped allocated world canvas");
    std::cout<<"TRANSFORMED_CHUNK_STROKE_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
