#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void send(CanvasView& view,ii::gui::WidgetInputKind kind,double x,double y){ii::gui::WidgetInput e;e.kind=kind;e.x=x;e.y=y;e.buttons=1;check(view.input(e).has_value(),"selection input failed");}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 Artwork art(dir.path());check(art.create(16,16),"create failed");const auto revision=art.revision();
 check(art.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12},MaskOperation::Replace,{.antialias=false,.feather=2}),"feather selection failed");
 auto mask=art.selection_mask();check(mask && mask->alpha[8*16+3]>0 && mask->alpha[8*16+4]<255 && mask->alpha[8*16+8]==255,"feather edge incorrect");
 check(art.revision()==revision && art.undo() && !art.selection_mask() && art.redo(),"feather history changed document");
 const auto boundary=art.selection_mask()->alpha[8*16+3];
 check(art.set_selection(SelectionGeometry::Rectangle,{12,4},{15,12},MaskOperation::Add,{.antialias=false,.feather=2}),"feather add failed");
 check(art.selection_mask()->alpha[8*16+3]==boundary && art.selection_mask()->alpha[8*16+13]>0,"feather add combination failed");
 check(art.undo() && art.selection_mask()->alpha[8*16+3]==boundary && art.redo(),"feather add history failed");

 check(art.undo() && art.undo() && art.can_redo(),"redo fixture failed");
 check(!art.set_selection(SelectionGeometry::Rectangle,{1,1},{4,4},MaskOperation::Replace,{.feather=std::numeric_limits<double>::quiet_NaN()}) && !art.selection_mask() && art.can_redo(),"invalid feather changed selection or redo");
 check(art.set_selection(SelectionGeometry::Ellipse,{3.25,3.25},{12.25,12.25},MaskOperation::Replace,{.antialias=false}),"aliased ellipse failed");
 for(auto a:art.selection_mask()->alpha)check(a==0||a==255,"disabled antialias generated partial coverage");
 check(art.undo(),"undo aliased selection failed");CanvasView view(art);view.tool="rect-marquee";view.selection_feather=2;view.selection_antialias=false;
 using K=ii::gui::WidgetInputKind;send(view,K::pointer_down,4,4);send(view,K::pointer_up,12,12);
 check(art.selection_mask() && art.selection_mask()->alpha[8*16+3]>0,"canvas selection options not applied");

 std::vector<Point> polygon{{4,4},{12,4},{12,12},{4,12}};
 check(art.set_polygon_selection(polygon,MaskOperation::Replace,{.antialias=false,.feather=2}),"polygon feather failed");
 check(art.selection_mask()->alpha[8*16+3]>0 && art.selection_mask()->alpha[8*16+8]==255,"polygon feather did not affect edge");
 std::cout<<"SELECTION_OPTIONS_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
