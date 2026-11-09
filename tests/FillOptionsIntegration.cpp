#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void click(CanvasView& v,double x,double y){ii::gui::WidgetInput e;e.kind=ii::gui::WidgetInputKind::pointer_down;e.buttons=1;e.x=x;e.y=y;check(v.input(e).has_value(),"fill input failed");}
const RasterLayer& lastPixels(const Artwork& a){const auto* source=staticLayerSource(a.document().layers.back());check(source!=nullptr,"fill source missing");auto* asset=findRasterAsset(a.document(),source->assetId);check(asset!=nullptr,"fill raster missing");return asset->pixels;}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temp unavailable");
 Artwork a(dir.path());check(a.create(5,1),"fill fixture failed");auto p=makeRasterLayer(5,1,0);p.pixels={0,0,0xff000000U,0x80000000U,0};check(a.import_pixels(std::move(p),"fixture",0,0),"fill fixture import");check(a.set_selection(SelectionGeometry::Rectangle,{0,0},{4,1}),"fill mask fixture");
 CanvasView v(a);v.tool="fill";v.color=0xff00ff00U;v.fill_tolerance=.51;v.fill_contiguous=false;click(v,0,0);
 auto raster=lastPixels(a);check(raster.pixels==std::vector<std::uint32_t>({0xff00ff00U,0xff00ff00U,0,0xff00ff00U,0}),"fill options and selection not applied");
 check(a.undo() && a.selection_mask() && a.redo() && lastPixels(a).pixels==raster.pixels,"fill options history failed");
 Artwork b(dir.path());check(b.create(4,1),"reference fixture failed");auto bottom=makeRasterLayer(4,1,0);bottom.pixels={0xffff0000U,0xff0000ffU,0xffff0000U,0xff0000ffU};check(b.import_pixels(std::move(bottom),"bottom",0,0),"bottom import");auto top=makeRasterLayer(4,1,0);check(b.import_pixels(std::move(top),"top",0,0),"top import");
 CanvasView w(b);w.tool="fill";w.color=0xff00ff00U;w.fill_contiguous=false;w.fill_current_layer=true;click(w,0,0);
 check(lastPixels(b).pixels==std::vector<std::uint32_t>({0xff00ff00U,0xff00ff00U,0xff00ff00U,0xff00ff00U}),"fill current layer reference failed");
 check(b.undo(),"fill reference undo");check(b.visible(b.selected(),false),"hide reference");auto revision=b.revision();std::string message;w.status=[&](std::string value){message=std::move(value);};click(w,0,0);check(!message.empty() && b.revision()==revision,"hidden fill reference changed document");
 Artwork frames(dir.path());check(frames.create(4,1)&&frames.timing(24,3),"fill frame fixture");CanvasView f(frames);f.tool="fill";f.color=0xffff0000U;f.set_frame(1);click(f,0,0);
 auto first=renderFrame(frames.document(),0),current=renderFrame(frames.document(),1),last=renderFrame(frames.document(),2);check(first.ok()&&current.ok()&&last.ok()&&(first.pixels.pixels[0]>>24)==0&&(current.pixels.pixels[0]>>24)==255&&(last.pixels.pixels[0]>>24)==0,"fill escaped current frame");
 Document shared;shared.extent={4,1};auto sharedPixels=makeRasterLayer(4,1,0);sharedPixels.pixels={0x80ff0000U,0,0x80ff0000U,0};shared.assets.emplace_back(RasterAsset{"shared",std::move(sharedPixels)});shared.layers.emplace_back(StaticBitmapLayer{{"base","Base"},StaticSource{"shared"}});shared.layers.emplace_back(StaticBitmapLayer{{"top","Top"},StaticSource{"shared"}});
 Artwork edit(dir.path());check(edit.replace(shared) && edit.select("top"),"current layer fixture");check(edit.set_selection(SelectionGeometry::Rectangle,{0,0},{2,1}),"current layer selection");
 CanvasView direct(edit);direct.tool="fill";direct.color=0x8000ff00U;direct.fill_contiguous=false;direct.fill_current_layer=true;direct.fill_target_current=true;click(direct,0,0);
 check(edit.document().layers.size()==2 && edit.selected()=="top","current fill added a layer");
 auto* base=staticLayerSource(edit.document().layers.front());auto* target=staticLayerSource(edit.document().layers.back());check(base&&target&&base->assetId=="shared"&&target->assetId!="shared","current fill changed shared asset identity");
 auto* original=findRasterAsset(edit.document(),base->assetId);auto* changed=findRasterAsset(edit.document(),target->assetId);check(original&&changed&&original->pixels.pixels[0]==0x80ff0000U && changed->pixels.pixels[0]==0xc055aa00U && changed->pixels.pixels[2]==0x80ff0000U,"current fill compositing or selection failed");
 check(edit.undo() && staticLayerSource(edit.document().layers.back())->assetId=="shared" && edit.redo(),"current fill undo redo");
 check(edit.undo(),"current fill undo for no-op");auto noopRevision=edit.revision();direct.fill_preserve_alpha=true;click(direct,1,0);check(edit.revision()==noopRevision && edit.can_redo(),"preserve alpha transparent no-op changed history");
 click(direct,0,0);target=staticLayerSource(edit.document().layers.back());changed=findRasterAsset(edit.document(),target->assetId);check(changed && changed->pixels.pixels[0]==0x807f8000U,"fill did not preserve alpha");
 Document transformed=shared;transformed.extent={6,1};layerProperties(transformed.layers.back()).transform.translationX=2;
 Artwork moved(dir.path());check(moved.replace(transformed)&&moved.select("top")&&moved.set_selection(SelectionGeometry::Rectangle,{2,0},{3,1}),"transformed fill fixture");
 CanvasView movedView(moved);movedView.tool="fill";movedView.color=0xff00ff00U;movedView.fill_current_layer=true;movedView.fill_contiguous=false;movedView.fill_target_current=true;click(movedView,2,0);
 auto* movedSource=staticLayerSource(moved.document().layers.back());auto* movedPixels=movedSource?findRasterAsset(moved.document(),movedSource->assetId):nullptr;
 check(movedPixels&&movedPixels->pixels.pixels[0]==0xff00ff00U&&movedPixels->pixels.pixels[2]==0x80ff0000U,"transformed fill escaped selection");
 check(moved.undo()&&moved.redo(),"transformed fill history failed");
 Artwork multi(dir.path());check(multi.replace(shared)&&multi.select("top")&&multi.timing(24,3),"multi-frame fixture");CanvasView singleFrame(multi);singleFrame.tool="fill";singleFrame.color=0xff00ff00U;singleFrame.fill_target_current=true;singleFrame.fill_current_layer=true;singleFrame.set_frame(1);auto multiRevision=multi.revision();std::string reason;singleFrame.status=[&](std::string value){reason=std::move(value);};click(singleFrame,0,0);check(reason.empty()&&multi.revision()==multiRevision+1,"multi-frame direct fill failed");
 const auto* topLayer=findLayer(multi.document(),"top");check(topLayer!=nullptr,"multi-frame target missing");const auto* firstAsset=resolveAssetAt(multi.document(),*topLayer,0);const auto* currentAsset=resolveAssetAt(multi.document(),*topLayer,1);const auto* finalAsset=resolveAssetAt(multi.document(),*topLayer,2);check(firstAsset&&currentAsset&&finalAsset&&assetId(*firstAsset)=="shared"&&assetId(*currentAsset)!="shared"&&assetId(*finalAsset)=="shared","multi-frame fill changed other frames");
 Artwork smooth(dir.path());check(smooth.create(8,8),"antialias fill fixture");auto diagonal=makeRasterLayer(8,8,0xffffffffU);
 for(int y=0;y<8;++y)for(int x=0;x<8;++x)if(x<=y)diagonal.pixels[std::size_t(y)*8+x]=0xff000000U;
 check(smooth.import_pixels(std::move(diagonal),"diagonal",0,0)&&smooth.set_selection(SelectionGeometry::Rectangle,{0,0},{4,8}),"antialias fill selection");
 CanvasView aa(smooth);aa.tool="fill";aa.color=0xff00ff00U;aa.fill_current_layer=true;aa.fill_antialias=true;click(aa,0,7);const auto filled=lastPixels(smooth).pixels;
 bool fractional=false;for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto alpha=filled[std::size_t(y)*8+x]>>24;if(x>=4)check(alpha==0,"antialias fill escaped selection");if(x<4 && alpha>0 && alpha<255)fractional=true;}
 check(fractional,"antialias fill did not create fractional edge coverage");check(smooth.undo()&&smooth.redo()&&lastPixels(smooth).pixels==filled,"antialias fill undo redo failed");
 Artwork aaDirect(dir.path());check(aaDirect.create(8,8),"direct antialias fixture");auto originalAA=makeRasterLayer(8,8,0xffffffffU);
 for(int y=0;y<8;++y)for(int x=0;x<8;++x)if(x<=y)originalAA.pixels[std::size_t(y)*8+x]=0xff000000U;
 const auto aaBefore=originalAA.pixels;check(aaDirect.import_pixels(std::move(originalAA),"diagonal",0,0)&&aaDirect.set_selection(SelectionGeometry::Rectangle,{0,0},{4,8}),"direct antialias selection");
 CanvasView editAA(aaDirect);editAA.tool="fill";editAA.color=0xff00ff00U;editAA.fill_current_layer=true;editAA.fill_target_current=true;editAA.fill_antialias=true;click(editAA,0,7);const auto aaAfter=lastPixels(aaDirect).pixels;
 bool mixed=false;for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto i=std::size_t(y)*8+x;if(x>=4)check(aaAfter[i]==aaBefore[i],"direct antialias escaped selection");if(x<4){const auto green=(aaAfter[i]>>8)&255;if(green>0 && green<255)mixed=true;}}
 check(mixed && aaDirect.undo() && lastPixels(aaDirect).pixels==aaBefore && aaDirect.redo() && lastPixels(aaDirect).pixels==aaAfter,"direct antialias coverage or history failed");
 std::cout<<"FILL_OPTIONS_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
