#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<congregation::native::SelectionPreview>);
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void send(CanvasView &view,ii::gui::WidgetInputKind kind,double x,double y,bool shift=false,bool alt=false) {
 ii::gui::WidgetInput e; e.kind=kind;e.x=x;e.y=y;e.buttons=1;e.shift=shift;e.alt=alt;
 auto result=view.input(e);check(result.has_value(),"input failed");
}
unsigned alpha(const Artwork &art,int x,int y) {
 auto p=renderFrameRegion(art.document(),0,{{x,y},{1,1}},{1,1});
 check(p.ok(),"pixel read failed");return p.pixels.pixels[0]>>24;
}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());
 check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());check(art.create(16,16),"create failed");
 CanvasView view(art);using K=ii::gui::WidgetInputKind;view.tool="rect-marquee";
 const auto original=art.revision();send(view,K::pointer_down,4,4);send(view,K::pointer_move,12,12);send(view,K::pointer_up,12,12);
 check(art.selection_mask() && art.selection_mask()->alpha[8*16+8]==255 && art.selection_mask()->alpha[1*16+1]==0,"rectangle selection missing");
 check(art.revision()==original && art.can_undo(),"selection changed document revision or omitted undo");
 check(art.undo() && !art.selection_mask() && art.revision()==original,"selection undo changed document");
 check(art.redo() && art.selection_mask() && art.revision()==original,"selection redo changed document");
 view.tool="brush";view.brush_size=16;view.color=0xffff0000U;
 send(view,K::pointer_down,8,8);
 auto preview=art.selection_preview();check(preview.has_value(),"selection preview unavailable");
 FrameBitmapOverlay overlay{preview->asset_id,preview->tiles};
 auto display=renderFrameRegion(art.document(),0,{{2,8},{1,1}},{1,1},RasterSampling::Nearest,std::span(&overlay,1));
 check(display.ok() && (display.pixels.pixels[0]>>24)==0,"unselected pixels visible during brush preview");
 auto inside=renderFrameRegion(art.document(),0,{{8,8},{1,1}},{1,1},RasterSampling::Nearest,std::span(&overlay,1));
 check(inside.ok() && (inside.pixels.pixels[0]>>24)>0,"selected pixels missing during brush preview");
 send(view,K::pointer_up,8,8);
 check(alpha(art,8,8)>0 && alpha(art,2,8)==0,"brush escaped selection");
 check(art.undo() && alpha(art,8,8)==0 && art.selection_mask(),"brush undo lost selection");
 check(art.redo() && alpha(art,8,8)>0 && alpha(art,2,8)==0,"brush redo escaped selection");
 check(art.undo(),"brush undo failed");view.tool="fill";view.color=0xff00ff00U;send(view,K::pointer_down,8,8);
 check(alpha(art,8,8)>0 && alpha(art,2,8)==0,"fill escaped selection");
 check(art.undo() && alpha(art,8,8)==0,"fill undo failed");
 check(art.redo() && alpha(art,8,8)>0 && alpha(art,2,8)==0,"fill redo failed");
 check(art.undo(),"fill undo before no-op stroke failed");
 const auto beforeNoop=art.revision();
 view.tool="brush";view.brush_size=2;send(view,K::pointer_down,1,1);send(view,K::pointer_up,1,1);
 check(art.revision()==beforeNoop && alpha(art,1,1)==0 && art.can_redo(),"outside selection stroke changed revision or redo");
 view.tool="ellipse-marquee";const auto beforeEllipse=art.revision();
 send(view,K::pointer_down,0,0);send(view,K::pointer_move,8,8);send(view,K::pointer_up,8,8);
 check(art.selection_mask() && art.selection_mask()->alpha[4*16+4]==255 && art.selection_mask()->alpha[12*16+12]==0,"ellipse selection failed");
 check(art.revision()==beforeEllipse && art.undo() && art.selection_mask()->alpha[8*16+8]==255,"selection undo did not restore previous mask");
 // Intersecting disjoint shapes must select nothing, not remove the selection.
 check(art.set_selection(SelectionGeometry::Rectangle,{0,0},{2,2},MaskOperation::Intersect),"empty intersection failed");
 check(art.selection_mask() && art.selection_mask()->alpha[8*16+8]==0,"empty selection became unrestricted");
 const auto emptyRevision=art.revision();
 view.tool="brush";view.brush_size=2;send(view,K::pointer_down,8,8);send(view,K::pointer_up,8,8);
 check(art.revision()==emptyRevision && alpha(art,8,8)==0,"empty selection allowed paint");
 view.tool="fill";send(view,K::pointer_down,8,8);
 check(art.revision()==emptyRevision && alpha(art,8,8)==0,"empty selection allowed fill");
 check(art.undo() && art.selection_mask()->alpha[8*16+8]==255,"empty selection undo failed");
 const auto selectionRevision=art.revision();
 check(art.invert_selection() && art.selection_mask()->alpha[8*16+8]==0 && art.selection_mask()->alpha[1*16+1]==255,"invert selection failed");
 check(art.undo() && art.selection_mask()->alpha[8*16+8]==255,"invert undo failed");
 check(art.clear_selection() && !art.selection_mask(),"clear selection failed");
 check(art.revision()==selectionRevision && art.undo() && art.selection_mask()->alpha[8*16+8]==255,"clear selection undo changed document");
 check(art.clear_selection(),"clear before eraser test failed");
 view.tool="brush";view.brush_size=16;view.color=0xff123456U;
 send(view,K::pointer_down,8,8);send(view,K::pointer_up,8,8);
 const auto outsideBefore=alpha(art,2,8);check(outsideBefore>0,"eraser fixture missing pixels");
 check(art.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"eraser selection failed");
 view.tool="eraser";send(view,K::pointer_down,8,8);send(view,K::pointer_up,8,8);
 check(alpha(art,8,8)==0 && alpha(art,2,8)==outsideBefore,"eraser escaped selection");
 check(art.undo() && alpha(art,8,8)>0 && alpha(art,2,8)==outsideBefore,"eraser undo failed");
 const auto beforeCancel=art.revision();view.tool="brush";send(view,K::pointer_down,8,8);send(view,K::focus_lost,8,8);
 check(art.revision()==beforeCancel && art.can_redo(),"cancelled stroke changed document or redo");
 Artwork vectorArt(dir.path());check(vectorArt.create(16,16),"vector fixture failed");
 check(vectorArt.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"vector selection failed");
 check(vectorArt.shape("rectangle",{0,0},{16,16},0xff0000ffU,0,true),"selected vector shape failed");
 check(alpha(vectorArt,8,8)>0 && alpha(vectorArt,2,8)==0,"vector shape escaped selection");
 check(vectorArt.undo() && alpha(vectorArt,8,8)==0 && vectorArt.selection_mask(),"vector shape undo failed");
 check(vectorArt.redo() && alpha(vectorArt,8,8)>0 && alpha(vectorArt,2,8)==0,"vector shape redo failed");
 check(std::holds_alternative<StaticVectorLayer>(vectorArt.document().layers.back()),"masked shape is not editable vector content");
 auto encoded=vectorArt.snapshot();
 auto restored=decodeIisc({reinterpret_cast<const std::uint8_t*>(encoded.data()),encoded.size()});
 check(restored.ok(),"masked vector snapshot failed");
 auto restoredInside=renderFrameRegion(restored.document,0,{{8,8},{1,1}},{1,1});
 auto restoredOutside=renderFrameRegion(restored.document,0,{{2,8},{1,1}},{1,1});
 check(restoredInside.ok() && restoredOutside.ok() && (restoredInside.pixels.pixels[0]>>24)>0 && (restoredOutside.pixels.pixels[0]>>24)==0,"masked vector snapshot changed pixels");
 ::AffineTransform moved{};moved.translationX=3;
 check(vectorArt.transform(moved) && alpha(vectorArt,5,8)==0 && alpha(vectorArt,13,8)>0,"vector mask did not move with its layer");
 check(vectorArt.undo() && alpha(vectorArt,5,8)>0 && alpha(vectorArt,13,8)==0,"vector transform undo failed");
 Artwork lassoArt(dir.path());check(lassoArt.create(16,16),"lasso fixture failed");
 CanvasView lassoView(lassoArt);lassoView.tool="lasso";const auto lassoRevision=lassoArt.revision();
 send(lassoView,K::pointer_down,3,3);send(lassoView,K::pointer_move,13,3);send(lassoView,K::pointer_move,13,13);send(lassoView,K::pointer_move,3,13);send(lassoView,K::pointer_up,3,3);
 check(lassoArt.selection_mask() && lassoArt.selection_mask()->alpha[8*16+8]==255 && lassoArt.selection_mask()->alpha[1*16+1]==0,"freehand lasso failed");
 check(lassoArt.revision()==lassoRevision && lassoArt.undo() && !lassoArt.selection_mask() && lassoArt.redo(),"lasso history changed document");
 send(lassoView,K::pointer_down,0,0,true);send(lassoView,K::pointer_move,2,0,true);send(lassoView,K::pointer_move,2,2,true);send(lassoView,K::pointer_move,0,2,true);send(lassoView,K::pointer_up,0,0,true);
 check(lassoArt.selection_mask()->alpha[1*16+1]==255 && lassoArt.selection_mask()->alpha[8*16+8]==255,"lasso add modifier failed");
 check(lassoArt.undo() && lassoArt.selection_mask()->alpha[1*16+1]==0,"lasso add undo failed");
 send(lassoView,K::pointer_down,0,0);send(lassoView,K::pointer_move,5,0);
 ii::gui::WidgetInput escape;escape.kind=K::key_down;escape.key=ii::gui::WidgetKey::escape;check(lassoView.input(escape).has_value(),"lasso cancellation input failed");
 check(lassoArt.selection_mask() && lassoArt.selection_mask()->alpha[8*16+8]==255 && lassoArt.revision()==lassoRevision && lassoArt.can_redo(),"cancelled lasso changed selection or redo");
 Artwork transformedArt(dir.path());check(transformedArt.create(16,16),"transformed brush fixture failed");
 ::AffineTransform translated{};translated.translationX=4;
 check(transformedArt.transform(translated),"paint layer transform failed");
 check(transformedArt.set_selection(SelectionGeometry::Rectangle,{6,4},{10,12}),"transformed selection failed");
 CanvasView transformedView(transformedArt);transformedView.tool="brush";transformedView.brush_size=12;transformedView.color=0xffff0000U;
 send(transformedView,K::pointer_down,8,8);send(transformedView,K::pointer_up,8,8);
 check(alpha(transformedArt,8,8)>0 && alpha(transformedArt,5,8)==0,"transformed brush escaped selection");
 check(transformedArt.undo() && alpha(transformedArt,8,8)==0 && transformedArt.redo() && alpha(transformedArt,8,8)>0,"transformed brush undo or redo failed");
 Artwork failureArt(dir.path());check(failureArt.create(16,16),"failure fixture failed");
 check(failureArt.set_selection(SelectionGeometry::Rectangle,{4,4},{12,12}),"failure selection failed");
 check(failureArt.add_layer("fixture") && failureArt.undo() && failureArt.can_redo(),"failure redo fixture failed");
 const auto failureRevision=failureArt.revision(); BitmapBrush failureBrush;failureBrush.size=4;failureBrush.argb=0xffff0000U;
 check(failureArt.begin({8,8},1,failureBrush),"failed-end begin failed");
 check(!failureArt.end({8,8},std::numeric_limits<double>::quiet_NaN()),"invalid stroke end was accepted");
 check(!failureArt.stroke_active() && failureArt.revision()==failureRevision && failureArt.can_redo() && alpha(failureArt,8,8)==0,"failed end did not roll back");
 check(failureArt.begin({8,8},1,failureBrush),"failed-move begin failed");
 check(!failureArt.extend({8,8},std::numeric_limits<double>::quiet_NaN()),"invalid stroke move was accepted");
 check(!failureArt.stroke_active() && failureArt.revision()==failureRevision && failureArt.can_redo() && alpha(failureArt,8,8)==0,"failed move did not roll back");
 Artwork polygonArt(dir.path());check(polygonArt.create(16,16),"polygon lasso fixture failed");
 CanvasView polygonView(polygonArt);polygonView.tool="polygon-lasso";const auto polygonRevision=polygonArt.revision();
 auto click=[&](double x,double y,double time) { ii::gui::WidgetInput e;e.x=x;e.y=y;e.buttons=1;e.timestamp=time;e.kind=K::pointer_down;check(polygonView.input(e).has_value(),"polygon press failed");e.kind=K::pointer_up;check(polygonView.input(e).has_value(),"polygon release failed"); };
 click(2,2,1);click(14,2,2);click(14,14,3);click(2,14,4);click(2,14,4.2);
 check(polygonArt.selection_mask() && polygonArt.selection_mask()->alpha[8*16+8]==255 && polygonArt.selection_mask()->alpha[0]==0,"polygon double click selection failed");
 check(polygonArt.revision()==polygonRevision && polygonArt.undo() && !polygonArt.selection_mask() && polygonArt.redo(),"polygon selection history failed");
 click(1,1,10);click(12,1,11);click(12,12,12);click(1,12,13);
 ii::gui::WidgetInput key;key.kind=K::key_down;key.key=ii::gui::WidgetKey::backspace;check(polygonView.input(key).has_value(),"polygon backspace failed");key.key=ii::gui::WidgetKey::enter;check(polygonView.input(key).has_value(),"polygon enter failed");
 check(polygonArt.selection_mask() && polygonArt.selection_mask()->alpha[3*16+10]==255 && polygonArt.selection_mask()->alpha[11*16+2]==0,"polygon point removal or enter failed");
 check(polygonArt.undo() && polygonArt.selection_mask()->alpha[8*16+8]==255 && polygonArt.can_redo(),"polygon edit undo failed");
 click(0,0,20);key.key=ii::gui::WidgetKey::escape;check(polygonView.input(key).has_value(),"polygon cancel failed");
 check(polygonArt.selection_mask()->alpha[8*16+8]==255 && polygonArt.can_redo() && polygonArt.revision()==polygonRevision,"polygon cancel changed state");
 Artwork wandArt(dir.path());check(wandArt.create(5,1),"wand fixture failed");
 auto wandPixels=makeRasterLayer(5,1,0);wandPixels.pixels={0x00000000U,0x00000000U,0xff000000U,0x80000000U,0x00000000U};
 check(wandArt.import_pixels(std::move(wandPixels),"wand fixture",0,0),"wand image import failed");
 CanvasView wandView(wandArt);wandView.tool="magic-wand";wandView.selection_antialias=false;const auto wandRevision=wandArt.revision();
 send(wandView,K::pointer_down,0,0);send(wandView,K::pointer_up,0,0);
 check(wandArt.selection_mask() && wandArt.selection_mask()->alpha==std::vector<std::uint8_t>({255,255,0,0,0}),"wand contiguous transparency selection failed");
 check(wandArt.revision()==wandRevision && wandArt.undo() && !wandArt.selection_mask() && wandArt.redo(),"wand selection history failed");
 send(wandView,K::pointer_down,4,0,true);send(wandView,K::pointer_up,4,0,true);
 check(wandArt.selection_mask()->alpha==std::vector<std::uint8_t>({255,255,0,0,255}),"wand add modifier failed");
 check(wandArt.undo() && wandArt.selection_mask()->alpha[4]==0,"wand selection undo failed");
 std::cout << "SELECTION_INTEGRATION_OK\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
