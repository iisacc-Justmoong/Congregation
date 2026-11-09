#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const RasterLayer& pixels(const Artwork& art) {
 for(const auto& item:art.document().layers) if(layerProperties(item).id==art.selected()) {
  const auto* source=staticLayerSource(item);check(source!=nullptr,"bitmap source missing");
  const auto* asset=findRasterAsset(art.document(),source->assetId);check(asset!=nullptr,"bitmap asset missing");return asset->pixels;
 }
 throw std::runtime_error("selected bitmap missing");
}
int width(const RasterLayer& p,int x) {int n=0;for(int y=0;y<p.height;++y) if((p.pixels[std::size_t(y)*p.width+x]>>24)>=128)++n;return n;}
StrokePoint at(double x,double y,double time) {StrokePoint p;p.position={x,y};p.time=time;return p;}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 Artwork art(dir.path());check(art.create(192,96),"fixture failed");BitmapBrush brush;brush.size=16;brush.hardness=1;brush.flow=1;brush.opacity=1;brush.argb=0xff112233U;brush.spacingRatio=.05;brush.pressureToOpacityEnabled=false;
 StrokeTaper options{.25,.25,.25,1};auto a=at(16,48,0),b=at(80,48,.5),c=at(144,48,1);
 const auto before=pixels(art).pixels;const auto revision=art.revision();
 check(art.begin_input(a,brush,0,options)&&art.extend_input(b)&&art.end_input(c),"taper stroke failed");
 const auto result=pixels(art);check(width(result,80)>=14,"taper changed full-size middle");check(width(result,20)>=3&&width(result,20)<=8,"start does not use whole-stroke length");check(width(result,140)>=3&&width(result,140)<=8,"end does not use whole-stroke length");check((result.pixels[48*192+140]>>24)>=240,"size taper changed center opacity");
 check(art.revision()==revision+1,"taper stroke should make one document revision");check(art.undo()&&pixels(art).pixels==before&&art.redo()&&pixels(art).pixels==result.pixels,"taper undo redo failed");
 check(art.undo(),"undo before cancellation failed");
 auto invalid=options;invalid.end=std::numeric_limits<double>::quiet_NaN();check(!art.begin_input(a,brush,0,invalid) && pixels(art).pixels==before && art.can_redo(),"invalid taper changed pixels or lost redo");
 check(art.begin_input(a,brush,0,options)&&art.extend_input(b),"cancel fixture failed");art.cancel();check(pixels(art).pixels==before&&art.can_redo(),"cancel changed pixels or lost redo");check(art.redo(),"redo after cancel failed");
 Artwork selected(dir.path());check(selected.create(192,96),"selection fixture failed");check(selected.set_selection(SelectionGeometry::Rectangle,{80,0},{192,96}),"selection failed");const auto selected_before=pixels(selected).pixels;
 check(selected.begin_input(a,brush,0,options)&&selected.extend_input(b)&&selected.end_input(c),"selected taper stroke failed");const auto selected_after=pixels(selected).pixels;
 for(int y=0;y<96;++y)for(int x=0;x<80;++x)check(selected_after[std::size_t(y)*192+x]==selected_before[std::size_t(y)*192+x],"taper escaped selection");
 check(width(pixels(selected),140)>=3 && selected.undo() && pixels(selected).pixels==selected_before && selected.redo() && pixels(selected).pixels==selected_after,"selected taper undo redo failed");
 Artwork erased(dir.path());check(erased.create(192,96),"eraser fixture failed");check(erased.import_pixels(makeRasterLayer(192,96,0xffffffffU),"opaque",0,0),"eraser source failed");check(erased.set_selection(SelectionGeometry::Rectangle,{80,0},{192,96}),"eraser selection failed");auto eraser=brush;eraser.eraser=true;auto original=pixels(erased).pixels;
 check(erased.begin_input(a,eraser,0,options)&&erased.extend_input(b)&&erased.end_input(c),"tapered eraser stroke failed");const auto removed=pixels(erased).pixels;check(removed[48*192+60]==original[48*192+60] && (removed[48*192+140]>>24)<255,"eraser did not honor selected taper");check(erased.undo()&&pixels(erased).pixels==original&&erased.redo()&&pixels(erased).pixels==removed,"eraser undo redo failed");
 // Exercise the same pointer route used by the native canvas, without opening a window.
 Artwork routed(dir.path());check(routed.create(192,96),"canvas route fixture failed");CanvasView view(routed);view.brush_size=16;view.color=0xff112233U;view.settings=brush;view.taper=options;using K=ii::gui::WidgetInputKind;ii::gui::WidgetInput event;event.buttons=1;event.pressure=1;event.kind=K::pointer_down;event.x=16;event.y=48;check(view.input(event).has_value()&&routed.stroke_active(),"canvas begin failed");event.kind=K::pointer_move;event.x=80;check(view.input(event).has_value(),"canvas move failed");event.kind=K::pointer_up;event.x=144;check(view.input(event).has_value(),"canvas end failed");check(width(pixels(routed),140)>=3&&width(pixels(routed),140)<=8,"canvas did not pass taper settings");
 std::cout<<"TAPER_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
