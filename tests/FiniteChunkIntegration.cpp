#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <span>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t pixel(const Artwork& art,int x,int y){auto r=renderFrameRegion(art.document(),0,{{x,y},{1,1}},{1,1});check(r.ok(),"finite chunk render failed");return r.pixels.pixels[0];}
Document fixture(){Document d;d.extent={9000,9000};d.assets.emplace_back(ChunkedRasterAsset{"paint",{}});d.layers.emplace_back(StaticBitmapLayer{{"layer","Paint"},StaticSource{"paint"}});return d;}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());check(art.replace(fixture())&&art.set_selection(SelectionGeometry::Rectangle,{10,10},{20,20}),"finite chunk fixture failed");BitmapBrush brush;brush.size=4;brush.argb=0xffff0000U;brush.pressureToOpacityEnabled=false;
 if(!art.begin({12,12},1,brush))throw std::runtime_error("finite selected stroke begin failed: "+art.error());check(art.extend({24,12},1),"finite selected stroke extend failed");auto preview=art.selection_preview();check(preview&&!preview->tiles.empty(),"finite selected preview missing");FrameBitmapOverlay overlay{preview->asset_id,preview->tiles};auto outside=renderFrameRegion(art.document(),0,{{24,12},{1,1}},{1,1},RasterSampling::Nearest,std::span(&overlay,1));check(outside.ok()&&outside.pixels.pixels[0]==0,"finite preview escaped selection");check(art.end({24,12},1),"finite chunk stroke failed");const auto painted=pixel(art,12,12);check(painted!=0&&pixel(art,24,12)==0,"finite chunk stroke escaped selection");check(art.undo()&&pixel(art,12,12)==0&&art.redo()&&pixel(art,12,12)==painted,"finite chunk stroke history failed");
 FloodOptions fill;fill.target_current=true;fill.current_layer=true;if(!art.fill_at({14,17},0xff0000ffU,0,fill))throw std::runtime_error("finite selected fill failed: "+art.error());const auto filled=pixel(art,14,17);check(filled!=0&&pixel(art,24,17)==0&&art.undo()&&pixel(art,14,17)==0&&art.redo()&&pixel(art,14,17)==filled,"finite selected fill or history failed");
 const auto path=dir.path()/"finite-chunk.iisc";if(!art.save(path))throw std::runtime_error("finite chunk save failed: "+art.error());Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,12,12)==painted&&pixel(reopened,14,17)==filled,"finite chunk reopen failed");
 Document smallDoc=fixture();smallDoc.extent={64,64};Artwork small(dir.path());check(small.replace(std::move(smallDoc))&&small.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"small chunk selection fixture failed");
 if(!small.fill_at({6,6},0xff00ff00U,0,fill))throw std::runtime_error("small finite chunk fill failed: "+small.error());check(pixel(small,6,6)==0xff00ff00U&&pixel(small,20,20)==0&&small.undo()&&pixel(small,6,6)==0&&small.redo()&&pixel(small,6,6)==0xff00ff00U,"small finite chunk fill history or selection failed");
 Artwork created(dir.path());check(created.create(9000,9000)&&created.document().canvasMode==CanvasMode::Finite&&std::holds_alternative<ChunkedRasterAsset>(created.document().assets.back()),"large finite canvas was not created sparsely");check(created.add_layer("Second")&&std::holds_alternative<ChunkedRasterAsset>(created.document().assets.back())&&created.undo()&&created.redo(),"large finite new layer was not sparse or undoable");
 std::cout<<"FINITE_CHUNK_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
