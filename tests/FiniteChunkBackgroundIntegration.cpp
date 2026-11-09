#include <Native/Artwork.h>
#include <Native/CanvasDefinition.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t pixel(const Artwork& a,int x,int y){auto r=renderFrameRegion(a.document(),0,{{x,y},{1,1}},{1,1});check(r.ok(),"background render failed");return r.pixels.pixels[0];}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");CanvasDefinition d;d.width=d.height=9000;d.pixel_width=d.pixel_height=9000;d.background="White";Artwork art(dir.path());check(art.create(d)&&pixel(art,6,6)==0xffffffffU&&art.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"white canvas fixture failed");
 FloodOptions options;options.current_layer=false;options.target_current=false;if(!art.fill_at({6,6},0xff00ff00U,0,options))throw std::runtime_error("white canvas composite fill failed: "+art.error());check(pixel(art,6,6)==0xff00ff00U&&pixel(art,20,20)==0xffffffffU,"white canvas fill escaped selection");check(art.undo()&&pixel(art,6,6)==0xffffffffU&&art.redo()&&pixel(art,6,6)==0xff00ff00U,"white canvas fill history failed");const auto path=dir.path()/"background.iisc";check(art.save(path),"white canvas save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,6,6)==0xff00ff00U&&pixel(reopened,20,20)==0xffffffffU,"white canvas reopen failed");
 CanvasDefinition smallDef=d;smallDef.width=smallDef.height=64;smallDef.pixel_width=smallDef.pixel_height=64;Artwork large(dir.path()),small(dir.path());check(large.create(d)&&small.create(smallDef),"background comparison fixture failed");BitmapBrush brush;brush.size=3;brush.argb=0xffff0000U;brush.pressureToOpacityEnabled=false;
 for(auto* target:{&large,&small}) {check(target->set_selection(SelectionGeometry::Rectangle,{0,0},{64,64})&&target->begin({20,20},1,brush)&&target->extend({40,20},1)&&target->extend({40,40},1)&&target->extend({20,40},1)&&target->end({20,20},1),"background boundary fixture failed");}
 const auto compare=[&]{auto l=renderFrameRegion(large.document(),0,{{0,0},{64,64}},{64,64});auto r=renderFrameRegion(small.document(),0,{{0,0},{64,64}},{64,64});check(l.ok()&&r.ok()&&l.pixels.pixels==r.pixels.pixels,"large composite differs from dense reference");};
 check(large.fill_at({30,30},0xff00ff00U,0,options)&&small.fill_at({30,30},0xff00ff00U,0,options),"bounded white interior fill failed");compare();check(pixel(large,30,30)==0xff00ff00U&&pixel(large,10,10)==0xffffffffU,"white background flood crossed boundary");check(large.undo()&&small.undo(),"white boundary undo failed");options.contiguous=false;check(large.fill_at({10,10},0xff0000ffU,0,options)&&small.fill_at({10,10},0xff0000ffU,0,options),"noncontiguous white fill failed");compare();check(pixel(large,30,30)==0xff0000ffU&&pixel(large,10,10)==0xff0000ffU,"noncontiguous white fill missed disconnected region");
 CanvasDefinition blackDef=d;blackDef.background="Black";Artwork black(dir.path());check(black.create(blackDef)&&black.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12})&&black.fill_at({6,6},0xffff0000U,0,options)&&pixel(black,6,6)==0xffff0000U&&pixel(black,20,20)==0xff000000U,"black background fill failed");
 Document changed=large.document();layerProperties(changed.layers.front()).opacity=.5;Artwork unsupported(dir.path());check(unsupported.replace(std::move(changed))&&unsupported.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12})&&unsupported.set_selection(SelectionGeometry::Rectangle,{8,8},{16,16})&&unsupported.undo()&&unsupported.can_redo(),"unsupported background fixture failed");const auto before=unsupported.snapshot();const auto revision=unsupported.revision();check(!unsupported.fill_at({6,6},0xff00ff00U,0,options)&&unsupported.revision()==revision&&unsupported.snapshot()==before&&unsupported.can_redo(),"unsupported background changed document or redo");
 Artwork wide(dir.path());check(wide.create(9000,9000),"large selection fixture failed");BitmapBrush dot;dot.size=8;dot.argb=0xffff0000U;dot.pressureToOpacityEnabled=false;
 check(wide.begin({20,20},1,dot)&&wide.end({20,20},1)&&wide.set_selection(SelectionGeometry::Rectangle,{0,0},{9000,9000}),"large selection setup failed");
 FloodOptions bounded;bounded.current_layer=true;bounded.target_current=false;const auto previous=pixel(wide,20,20);
 if(!wide.fill_at({20,20},0xff00ff00U,0,bounded))throw std::runtime_error("bounded fill in large selection failed: "+wide.error());
 check(pixel(wide,20,20)==0xff00ff00U&&pixel(wide,1000,1000)==0,"bounded fill escaped reference region");
 check(wide.undo()&&pixel(wide,20,20)==previous&&wide.redo()&&pixel(wide,20,20)==0xff00ff00U,"bounded fill history failed");
 check(wide.set_selection(SelectionGeometry::Rectangle,{1000,1000},{1010,1010})&&wide.set_selection(SelectionGeometry::Rectangle,{2000,2000},{2010,2010})&&wide.undo()&&wide.can_redo(),"disjoint fill fixture failed");
 const auto beforeDisjoint=wide.snapshot();const auto revisionDisjoint=wide.revision();
 check(wide.fill_at({20,20},0xff0000ffU,0,bounded)&&wide.snapshot()==beforeDisjoint&&wide.revision()==revisionDisjoint&&wide.can_redo(),"disjoint bounded fill changed document or redo");
 std::cout<<"FINITE_CHUNK_BACKGROUND_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
