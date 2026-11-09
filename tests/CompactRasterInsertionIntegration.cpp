#include <Native/Artwork.h>
#include "DocumentFixture.h"
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
Document fixture(){Document d;d.canvasMode=CanvasMode::Infinite;d.extent={256,256};d.timeline={{24,1},3};auto pixels=makeRasterLayer(256,256);std::uint32_t random=9;for(auto& p:pixels.pixels){random^=random<<13;random^=random>>17;random^=random<<5;p=0xff000000U|(random&0xffffffU);}d.assets.emplace_back(ChunkedRasterAsset{"base",{{0,0,std::move(pixels)}}});d.layers.emplace_back(StaticBitmapLayer{{"paint","Paint"},StaticSource{"base"}});return d;}
std::uint32_t pixel(const Artwork& a,FrameIndex frame,int x,int y){auto r=renderFrameRegion(a.document(),frame,{{x,y},{1,1}},{1,1});check(r.ok(),"frame render failed");return r.pixels.pixels[0];}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");constexpr std::size_t budget=256*1024;
 Artwork art(dir.path(),budget);check(art.replace(fixture())&&art.snapshot().size()>budget/2&&art.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104}),"gradient insertion fixture failed");
 const auto old=pixel(art,1,100,100),oldOutside=pixel(art,1,110,100);GradientDefinition g;g.start={98,100};g.end={104,100};g.stops={{0,0xffff0000U},{1,0xff0000ffU}};
 if(!art.gradient(g,1))throw std::runtime_error("compact gradient failed: "+art.error());
 const auto changed=pixel(art,1,100,100);check(changed!=old&&pixel(art,1,110,100)==oldOutside&&pixel(art,0,100,100)==old&&pixel(art,2,100,100)==old&&art.document().layers.size()==2&&art.history_bytes()<=budget,"gradient selection or frame changed");
 const auto retained=art.history_bytes();for(int i=0;i<3;++i)check(art.undo()&&art.redo()&&art.history_bytes()==retained,"insertion history accounting drifted");
 check(art.set_selection(SelectionGeometry::Rectangle,{120,120},{124,124})&&art.undo()&&art.tiled_selection_handle()->coverage(100,100)==255,"gradient selection history order failed");
 check(art.undo()&&art.document().layers.size()==1&&pixel(art,1,100,100)==old&&art.can_redo(),"gradient undo failed");
 auto transparent=g;transparent.stops={{0,0},{1,0}};const auto revision=art.revision();check(art.gradient(transparent,1)&&art.revision()==revision&&art.can_redo(),"empty gradient lost redo");
 check(art.redo()&&art.document().layers.size()==2&&pixel(art,1,100,100)==changed&&pixel(art,0,100,100)==old,"gradient redo failed");
 const auto path=dir.path()/"gradient.iisc";check(art.save(path),"gradient save failed");Artwork reopened(dir.path(),budget);check(open_document_fixture(reopened,path)&&pixel(reopened,1,100,100)==changed&&pixel(reopened,0,100,100)==old,"gradient reopen failed");
 Artwork fill(dir.path(),budget);check(fill.replace(fixture())&&fill.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104}),"fill insertion fixture failed");
 const auto fillOld=pixel(fill,1,100,100);FloodOptions options;options.current_layer=true;options.target_current=false;
 check(fill.fill_at({100,100},0xff00ff00U,1,options),"compact new-layer fill failed");const auto fillNew=pixel(fill,1,100,100);
 check(fillNew==0xff00ff00U&&pixel(fill,0,100,100)==fillOld&&fill.document().layers.size()==2&&fill.undo()&&pixel(fill,1,100,100)==fillOld&&fill.redo()&&pixel(fill,1,100,100)==fillNew,"compact fill layer history failed");
 const auto working=dir.path()/"insert-working",moved=dir.path()/"insert-moved";std::filesystem::create_directory(working);Artwork failure(working,budget);check(failure.replace(fixture())&&failure.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104})&&failure.gradient(g,1),"insertion persistence fixture failed");
 const auto failedPixel=pixel(failure,1,100,100);const auto failedRevision=failure.revision();std::filesystem::rename(working,moved);const bool rejected=!failure.undo();std::filesystem::rename(moved,working);
 check(rejected&&failure.can_undo()&&failure.revision()==failedRevision&&pixel(failure,1,100,100)==failedPixel&&failure.undo()&&failure.redo(),"failed insertion undo changed document or history");
 check(failure.undo()&&failure.can_redo(),"failed insertion commit setup failed");const auto failedCommitRevision=failure.revision();const auto failedCommitPixel=pixel(failure,1,100,100);
 std::filesystem::rename(working,moved);const bool commitRejected=!failure.gradient(g,1);std::filesystem::rename(moved,working);
 check(commitRejected&&failure.can_redo()&&failure.revision()==failedCommitRevision&&pixel(failure,1,100,100)==failedCommitPixel&&failure.redo()&&pixel(failure,1,100,100)==failedPixel,"failed insertion commit changed document or redo");
 Artwork tight(dir.path(),64*1024);check(tight.replace(fixture())&&tight.set_selection(SelectionGeometry::Rectangle,{0,0},{100,100})&&tight.set_selection(SelectionGeometry::Rectangle,{120,120},{124,124})&&tight.undo()&&tight.can_redo(),"insertion budget fixture failed");
 const auto tightRevision=tight.revision();const auto tightLayers=tight.document().layers.size();check(!tight.gradient(g,1)&&tight.can_redo()&&tight.revision()==tightRevision&&tight.document().layers.size()==tightLayers,"rejected gradient changed document or redo");
 std::cout<<"COMPACT_RASTER_INSERTION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
