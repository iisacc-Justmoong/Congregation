#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void click(CanvasView& v,double x,double y){ii::gui::WidgetInput e;e.kind=ii::gui::WidgetInputKind::pointer_down;e.buttons=1;e.x=x;e.y=y;check(v.input(e).has_value(),"wand click failed");}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temp unavailable");
 Artwork a(dir.path());check(a.create(5,1),"fixture failed");auto pixels=makeRasterLayer(5,1,0);pixels.pixels={0,0,0xff000000U,0x80000000U,0};check(a.import_pixels(std::move(pixels),"fixture",0,0),"fixture import failed");
 CanvasView v(a);v.tool="magic-wand";v.selection_antialias=false;v.wand_contiguous=false;v.wand_tolerance=.51;const auto revision=a.revision();click(v,0,0);
 check(a.selection_mask() && a.selection_mask()->alpha==std::vector<std::uint8_t>({255,255,0,255,255}),"wand options were not applied");
 check(a.revision()==revision && a.undo() && !a.selection_mask(),"wand options changed document or history");
 Artwork b(dir.path());check(b.create(4,1),"layer fixture failed");auto bottom=makeRasterLayer(4,1,0);bottom.pixels={0xffff0000U,0xff0000ffU,0xffff0000U,0xff0000ffU};check(b.import_pixels(std::move(bottom),"bottom",0,0),"bottom import failed");auto top=makeRasterLayer(4,1,0);check(b.import_pixels(std::move(top),"top",0,0),"top import failed");
 CanvasView w(b);w.tool="magic-wand";w.selection_antialias=false;w.wand_contiguous=false;click(w,0,0);
 check(b.selection_mask() && b.selection_mask()->alpha==std::vector<std::uint8_t>({255,0,255,0}),"composite reference failed");
 w.wand_current_layer=true;click(w,0,0);check(b.selection_mask() && b.selection_mask()->alpha==std::vector<std::uint8_t>({255,255,255,255}),"current layer reference failed");
 check(b.undo() && b.selection_mask()->alpha==std::vector<std::uint8_t>({255,0,255,0}),"reference history failed");
 check(b.visible(b.selected(),false),"hide reference layer failed");const auto hiddenRevision=b.revision();
 std::string status;w.status=[&](std::string message){status=std::move(message);};click(w,0,0);
 check(!status.empty() && b.revision()==hiddenRevision && b.selection_mask()->alpha==std::vector<std::uint8_t>({255,0,255,0}),"hidden reference changed selection");
 std::cout<<"WAND_OPTIONS_INTEGRATION_OK\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
