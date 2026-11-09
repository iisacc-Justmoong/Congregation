#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Document fixture(int extent){Document d;d.extent={extent,extent};auto pixels=makeRasterLayer(32,32,0);for(int y=8;y<24;++y)for(int x=8;x<24;++x)pixels.pixels[std::size_t(y)*32+x]=0xffff0000U;d.assets.emplace_back(RasterAsset{"wand",std::move(pixels)});d.layers.emplace_back(StaticBitmapLayer{{"paint","Paint"},StaticSource{"wand"}});return d;}
std::uint8_t coverage(const Artwork& art,int x,int y){if(auto* m=art.selection_mask())return m->alpha[std::size_t(y)*m->extent.width+x];auto m=art.tiled_selection_handle();check(bool(m),"selection missing");return m->coverage(x,y);}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 Artwork dense(dir.path()),tiled(dir.path());check(dense.replace(fixture(32))&&tiled.replace(fixture(9000)),"wand options fixture failed");
 check(dense.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{false,0}),"binary wand failed");
 check(coverage(dense,8,16)==255&&coverage(dense,7,16)==0,"binary wand changed coverage");
 auto source=selectionWand(findRasterAsset(dense.document(),"wand")->pixels,{16,16},0,true);check(source.ok(),"wand oracle failed");
 auto smooth=antialiasBinaryMask(source.mask);check(smooth.ok(),"antialias oracle failed");
 check(dense.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{true,0}),"antialiased wand failed");
 check(dense.selection_mask()&&dense.selection_mask()->alpha==smooth.mask.alpha,"wand antialias differs from SDK reference");
 check(tiled.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{true,0}),"tiled antialiased wand failed");
 for(int y=0;y<32;++y)for(int x=0;x<32;++x)check(coverage(dense,x,y)==coverage(tiled,x,y),"dense and tiled antialias differ");
 check(dense.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{false,3})&&tiled.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{false,3}),"feathered wand failed");
 for(int y=0;y<32;++y)for(int x=0;x<32;++x)check(coverage(dense,x,y)==coverage(tiled,x,y),"dense and tiled feather differ");
 check(coverage(dense,7,16)>0&&coverage(dense,7,16)<255&&coverage(dense,16,16)==255,"wand feather did not soften boundary");
 const auto tiledRevision=tiled.revision();const auto tiledBefore=coverage(tiled,16,16);
 check(tiled.set_selection(SelectionGeometry::Rectangle,{1,1},{3,3})&&tiled.undo()&&tiled.can_redo(),"tiled feather failure setup failed");
 check(!tiled.set_wand_selection({0,0},0,0,true,MaskOperation::Replace,true,{false,3})&&tiled.can_redo()&&tiled.revision()==tiledRevision&&coverage(tiled,16,16)==tiledBefore,"oversized feather changed selection or redo");
 const auto revision=dense.revision();check(dense.set_selection(SelectionGeometry::Rectangle,{1,1},{3,3})&&dense.undo()&&dense.can_redo(),"invalid wand setup failed");
 check(!dense.set_wand_selection({16,16},0,0,true,MaskOperation::Replace,true,{false,std::numeric_limits<double>::quiet_NaN()})&&dense.can_redo()&&dense.revision()==revision,"invalid feather changed state");
 CanvasView view(dense);view.tool="magic-wand";view.selection_antialias=false;view.selection_feather=3;view.wand_current_layer=true;
 ii::gui::WidgetInput input;input.kind=ii::gui::WidgetInputKind::pointer_down;input.x=16;input.y=16;input.buttons=1;check(view.input(input).has_value(),"wand pointer down failed");input.kind=ii::gui::WidgetInputKind::pointer_up;check(view.input(input).has_value(),"wand pointer up failed");
 check(coverage(dense,7,16)>0&&coverage(dense,7,16)<255,"wand UI did not apply feather");
 std::cout<<"WAND_FEATHER_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
