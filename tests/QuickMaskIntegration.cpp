#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void send(CanvasView& v,ii::gui::WidgetInputKind k,double x,double y){ii::gui::WidgetInput e;e.kind=k;e.x=x;e.y=y;e.buttons=1;check(v.input(e).has_value(),"quick mask input failed");}
int main() try {
 iiFileProvider::native::TemporaryDirectory d(std::filesystem::temp_directory_path());check(d.isValid(),"temp unavailable");Artwork a(d.path());check(a.create(16,16),"fixture failed");
 check(a.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"selection fixture failed");const auto revision=a.revision();
 CanvasView v(a);v.tool="brush";v.brush_size=4;v.opacity=1;check(v.begin_quick_mask()&&v.quick_mask_active(),"quick mask did not start");
 using K=ii::gui::WidgetInputKind;send(v,K::pointer_down,8,8);send(v,K::pointer_move,9,8);
 check(v.quick_mask_preview()->alpha[8*16+8]==0,"quick mask did not update during the stroke");
 send(v,K::pointer_up,9,8);
 auto preview=v.quick_mask_preview();check(preview && preview->alpha[8*16+8]==0 && preview->alpha[5*16+5]==255 && preview->alpha[1*16+1]==0,"quick mask brush failed");
 check(a.revision()==revision && a.selection_mask()->alpha[8*16+8]==255,"quick mask changed the document or committed selection early");
 check(v.quick_mask_undo() && v.quick_mask_preview()->alpha[8*16+8]==255,"quick mask undo failed");check(v.quick_mask_redo() && v.quick_mask_preview()->alpha[8*16+8]==0,"quick mask redo failed");
 check(v.commit_quick_mask()&&!v.quick_mask_active()&&a.selection_mask()->alpha[8*16+8]==0&&a.revision()==revision,"quick mask commit failed");
 check(a.undo() && a.selection_mask()->alpha[8*16+8]==255 && a.can_redo(),"quick mask selection undo failed");
 check(v.begin_quick_mask(),"second quick mask failed");v.tool="eraser";send(v,K::pointer_down,1,1);send(v,K::pointer_up,1,1);
 check(v.quick_mask_preview()->alpha[1*16+1]>0,"quick mask eraser did not restore selection");
 ii::gui::WidgetInput escape;escape.kind=K::key_down;escape.key=ii::gui::WidgetKey::escape;check(v.input(escape).has_value(),"quick mask escape failed");
 check(!v.quick_mask_active()&&a.selection_mask()->alpha[8*16+8]==255&&a.can_redo()&&a.revision()==revision,"quick mask cancel changed selection or redo");

 Artwork empty(d.path());check(empty.create(16,16),"empty fixture failed");
 check(empty.set_selection(SelectionGeometry::Rectangle,{2,2},{4,4}) && empty.undo() && !empty.selection_mask() && empty.can_redo(),"redo fixture failed");
 CanvasView emptyView(empty);emptyView.tool="brush";emptyView.brush_size=4;int transitions=0;
 emptyView.quick_mask_changed=[&](bool){++transitions;};const auto emptyRevision=empty.revision();
 check(emptyView.begin_quick_mask() && emptyView.commit_quick_mask() && !empty.selection_mask() && empty.can_redo(),"unchanged mask disturbed unrestricted selection or redo");
 check(emptyView.begin_quick_mask(),"empty quick mask failed");send(emptyView,K::pointer_down,8,8);send(emptyView,K::pointer_up,8,8);
 check(emptyView.commit_quick_mask() && empty.selection_mask()->alpha[8*16+8]==0 && empty.selection_mask()->alpha[1*16+1]==255,"unrestricted selection mask commit failed");
 const auto afterMask=empty.revision();emptyView.brush_size=1;send(emptyView,K::pointer_down,8,8);send(emptyView,K::pointer_up,8,8);
 check(empty.revision()==afterMask,"brush ignored committed quick mask");
 check(empty.undo() && !empty.selection_mask() && empty.revision()==emptyRevision && empty.can_redo(),"committed mask undo failed");
 check(emptyView.begin_quick_mask(),"focus cancellation fixture failed");send(emptyView,K::pointer_down,8,8);send(emptyView,K::focus_lost,8,8);
 check(emptyView.quick_mask_active() && emptyView.quick_mask_preview()->alpha[8*16+8]==255 && !empty.selection_mask() && empty.can_redo() && empty.revision()==emptyRevision && transitions==5,"focus loss did not cancel only the active mask stroke");
 check(emptyView.commit_quick_mask() && !empty.selection_mask() && empty.can_redo() && transitions==6,"focus change prevented applying unchanged mask");
 Document bigDoc;bigDoc.extent={9000,9000};bigDoc.assets.emplace_back(RasterAsset{"big-mask",makeRasterLayer(64,64,0)});bigDoc.layers.emplace_back(StaticBitmapLayer{{"big-mask-layer","Mask target"},StaticSource{"big-mask"}});Artwork big(d.path());check(big.replace(std::move(bigDoc))&&big.set_selection(SelectionGeometry::Rectangle,{16,16},{48,48}),"large quick mask fixture failed");CanvasView bigView(big);bigView.tool="brush";bigView.brush_size=4;check(bigView.begin_quick_mask(),"large quick mask did not start");send(bigView,K::pointer_down,32,32);send(bigView,K::pointer_up,32,32);check(bigView.quick_mask_undo()&&bigView.quick_mask_redo()&&bigView.commit_quick_mask(),"large quick mask local history failed");check(big.tiled_selection_handle()&&big.tiled_selection_handle()->coverage(32,32)==0&&big.tiled_selection_handle()->coverage(20,20)==255&&big.tiled_selection_handle()->coverage(4,4)==0,"large quick mask changed the wrong coverage");check(big.undo()&&big.tiled_selection_handle()->coverage(32,32)==255&&big.redo()&&big.tiled_selection_handle()->coverage(32,32)==0,"large quick mask artwork history failed");const auto bigRevision=big.revision();BitmapBrush b;b.size=2;b.argb=0xffff0000U;check(big.begin({32,32},1,b)&&big.end({32,32},1)&&big.revision()==bigRevision,"large quick mask did not block paint");check(big.begin({20,20},1,b)&&big.end({20,20},1)&&big.revision()!=bigRevision&&big.undo()&&big.redo(),"large quick mask did not permit paint or preserve history");
 check(bigView.begin_quick_mask(),"stale quick mask fixture failed");send(bigView,K::pointer_down,24,24);send(bigView,K::pointer_up,24,24);check(big.set_selection(SelectionGeometry::Rectangle,{8,8},{12,12}),"stale selection fixture failed");check(!bigView.commit_quick_mask()&&big.tiled_selection_handle()->coverage(10,10)==255&&big.tiled_selection_handle()->coverage(24,24)==0,"stale quick mask replaced a newer selection");check(bigView.input(escape).has_value(),"stale mask cancel failed");
 check(big.clear_selection(),"unrestricted mask fixture failed");check(big.set_selection(SelectionGeometry::Rectangle,{1,1},{3,3})&&big.undo()&&!big.has_selection()&&big.can_redo(),"large unchanged redo fixture failed");check(bigView.begin_quick_mask()&&bigView.commit_quick_mask()&&!big.has_selection()&&big.can_redo(),"unchanged large mask lost redo");check(bigView.begin_quick_mask(),"unrestricted large quick mask did not start");send(bigView,K::pointer_down,30,30);send(bigView,K::pointer_up,30,30);check(bigView.commit_quick_mask()&&big.tiled_selection_handle()->coverage(30,30)==0&&big.tiled_selection_handle()->coverage(500,500)==255&&big.undo()&&!big.has_selection()&&big.redo(),"unrestricted large mask history failed");
 Document infDoc;infDoc.canvasMode=CanvasMode::Infinite;infDoc.infiniteCanvas.origin={-64,-64};infDoc.extent={128,128};infDoc.assets.emplace_back(ChunkedRasterAsset{"quick-chunks",{}});infDoc.layers.emplace_back(StaticBitmapLayer{{"quick-chunk-layer","Mask target"},StaticSource{"quick-chunks"}});Artwork inf(d.path());check(inf.replace(std::move(infDoc))&&inf.set_selection(SelectionGeometry::Rectangle,{-40,-40},{40,40}),"infinite quick mask fixture failed");CanvasView infView(inf);infView.tool="brush";infView.brush_size=4;check(infView.begin_quick_mask(),"infinite quick mask did not start");send(infView,K::pointer_down,-20,-20);send(infView,K::pointer_up,-20,-20);check(infView.commit_quick_mask()&&inf.tiled_selection_handle()->coverage(-20,-20)==0&&inf.tiled_selection_handle()->coverage(-30,-30)==255&&inf.tiled_selection_handle()->coverage(50,50)==0&&inf.undo()&&inf.tiled_selection_handle()->coverage(-20,-20)==255&&inf.redo(),"infinite quick mask coordinates or history failed");
 infView.tool="eraser";check(infView.begin_quick_mask(),"infinite restore fixture failed");send(infView,K::pointer_down,-20,-20);send(infView,K::pointer_up,-20,-20);check(infView.quick_mask_undo()&&infView.quick_mask_redo()&&infView.commit_quick_mask()&&inf.tiled_selection_handle()->coverage(-20,-20)==255&&inf.undo()&&inf.tiled_selection_handle()->coverage(-20,-20)==0&&inf.redo(),"infinite quick mask eraser restore failed");
 check(inf.undo()&&inf.can_redo(),"infinite focus-loss redo fixture failed");infView.tool="brush";check(infView.begin_quick_mask(),"infinite focus-loss mask failed");send(infView,K::pointer_down,-30,-30);send(infView,K::focus_lost,-30,-30);check(infView.quick_mask_active()&&infView.commit_quick_mask()&&inf.can_redo()&&inf.tiled_selection_handle()->coverage(-30,-30)==255&&inf.redo(),"infinite focus loss did not cancel the active stroke");
 std::cout<<"QUICK_MASK_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
