#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t pixel(const Artwork& a,int x,int y,FrameIndex frame=1){auto r=renderFrameRegion(a.document(),frame,{{x,y},{1,1}},{1,1});check(r.ok(),"sparse render failed");return r.pixels.pixels[0];}
int main()try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());
 check(art.create(9000,9000)&&art.timing(24,3)&&art.set_selection(SelectionGeometry::Rectangle,{10.25,10.25},{20.75,20.75})&&art.set_selection(SelectionGeometry::Rectangle,{8500.25,8500.25},{8510.75,8510.75},MaskOperation::Add),"sparse selection fixture failed");
 GradientDefinition g;g.start={0,0};g.end={9000,9000};g.stops={{0,0x80ff0000U},{1,0xff0000ffU}};
 if(!art.gradient(g,1))throw std::runtime_error("sparse gradient failed: "+art.error());
 auto expected=[&](int x,int y){auto d=g;d.start.x-=x;d.end.x-=x;d.start.y-=y;d.end.y-=y;auto r=gradientRaster({1,1},d);check(r.ok(),"reference gradient failed");auto mask=art.tiled_selection_handle();check(bool(mask),"sparse selection missing");RasterMask m{{1,1},{mask->coverage(x,y)}};auto v=maskRaster(r.pixels,m);check(v.ok(),"reference mask failed");return v.pixels.pixels[0];};
 check(pixel(art,15,15)==expected(15,15)&&pixel(art,8505,8505)==expected(8505,8505)&&pixel(art,4500,4500)==0&&pixel(art,10,10)==expected(10,10)&&pixel(art,15,15,0)==0&&pixel(art,15,15,2)==0,"sparse gradient pixels differ");
 auto* layer=findLayer(art.document(),art.selected());auto* asset=layer?resolveAssetAt(art.document(),*layer,1):nullptr;auto* chunks=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;check(chunks&&chunks->chunks.size()==2,"sparse gradient did not remain sparse");
 BitmapBrush brush;brush.size=4;brush.argb=0xff00ff00U;brush.pressureToOpacityEnabled=false;const auto original=pixel(art,15,15);
 check(art.begin({15,15},1,brush,1)&&art.end({15,15},1)&&pixel(art,15,15)!=original&&pixel(art,15,15,0)==0,"painting on sparse gradient failed");
 check(art.undo()&&pixel(art,15,15)==original&&art.redo()&&pixel(art,15,15)!=original&&art.undo()&&pixel(art,15,15)==original,"sparse gradient paint history failed");
 check(art.undo()&&pixel(art,15,15)==0&&art.redo()&&pixel(art,8505,8505)==expected(8505,8505),"sparse gradient undo redo failed");
 const auto path=dir.path()/"sparse-gradient.iisc";check(art.save(path),"sparse gradient save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,15,15)==expected(15,15)&&pixel(reopened,8505,8505)==expected(8505,8505)&&pixel(reopened,4500,4500)==0,"sparse gradient reopen failed");
 check(art.undo()&&art.can_redo(),"transparent fixture undo failed");auto transparent=g;transparent.stops={{0,0},{1,0}};const auto beforeTransparent=art.snapshot();const auto revTransparent=art.revision();check(art.gradient(transparent,1)&&art.snapshot()==beforeTransparent&&art.revision()==revTransparent&&art.can_redo(),"transparent sparse gradient changed history");check(art.redo()&&art.undo()&&art.can_redo(),"budget fixture undo failed");check(art.set_selection(SelectionGeometry::Rectangle,{0,0},{9000,9000})&&art.set_selection(SelectionGeometry::Rectangle,{100,100},{101,101})&&art.undo()&&art.can_redo(),"budget selection fixture failed");const auto before=art.snapshot();const auto revision=art.revision();check(!art.gradient(g,1)&&art.snapshot()==before&&art.revision()==revision&&art.can_redo(),"oversized gradient changed document or redo");
 std::cout<<"SPARSE_GRADIENT_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
