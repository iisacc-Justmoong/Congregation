#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t pixel(const Artwork& a,int x,int y,FrameIndex f=1){auto r=renderFrameRegion(a.document(),f,{{x,y},{1,1}},{1,1});check(r.ok(),"shape render failed");return r.pixels.pixels[0];}
const VectorAsset& vector(const Artwork& a,FrameIndex f=1){auto* l=findLayer(a.document(),a.selected());auto* p=l?resolveAssetAt(a.document(),*l,f):nullptr;auto* v=p?std::get_if<VectorAsset>(p):nullptr;check(v!=nullptr,"shape vector missing");return *v;}
int main()try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());check(art.create(64,64)&&art.timing(24,3)&&art.set_selection(SelectionGeometry::Rectangle,{10,10},{30,30}),"shape fixture failed");
 ShapeStyle style;style.fill=true;style.outline=true;style.stroke_width=2;style.opacity=.5;style.from_center=true;style.regular=true;
 if(!art.styled_shape("rectangle",{20,20},{32,26},0xffff0000U,style,1))throw std::runtime_error("styled rectangle failed: "+art.error());
 const auto& shape=vector(art);check(shape.paths.size()==1&&shape.paths[0].fill&&shape.paths[0].stroke&&shape.viewport.width==shape.viewport.height,"rectangle style or geometry missing");check((pixel(art,20,20)>>24)>0&&(pixel(art,6,20)>>24)==0&&pixel(art,20,20,0)==0&&pixel(art,20,20,2)==0,"shape escaped selection or frame");
 const auto expected=pixel(art,20,20);check(art.undo()&&pixel(art,20,20)==0&&art.redo()&&pixel(art,20,20)==expected,"styled shape history failed");const auto path=dir.path()/"styled.iisc";check(art.save(path),"styled shape save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,20,20)==expected,"styled shape reopen failed");
 check(art.undo()&&art.can_redo(),"shape invalid fixture failed");const auto revision=art.revision();auto invisible=style;invisible.fill=false;invisible.stroke_width=0;check(!art.styled_shape("ellipse",{12,12},{28,28},0xffffffffU,invisible,1)&&art.can_redo()&&art.revision()==revision,"invisible styled shape changed history");
 auto transparent=style;transparent.opacity=0;check(art.styled_shape("rectangle",{12,12},{28,28},0xffffffffU,transparent,1)&&art.revision()==revision&&art.can_redo(),"transparent styled shape changed redo");
 Artwork fillOnly(dir.path());check(fillOnly.create(64,64),"zero-width styled fixture failed");auto noOutline=style;noOutline.stroke_width=0;check(fillOnly.styled_shape("ellipse",{20,20},{30,30},0xff00ff00U,noOutline,0)&&vector(fillOnly,0).paths[0].fill&&!vector(fillOnly,0).paths[0].stroke,"zero-width styled outline should be absent");
 Artwork pointer(dir.path());check(pointer.create(64,64),"shape pointer fixture failed");CanvasView view(pointer);view.tool="ellipse";view.polygon_options.fill=true;view.polygon_options.outline=true;view.polygon_options.stroke_width=3;view.polygon_options.opacity=.5;
 using K=ii::gui::WidgetInputKind;auto send=[&](K kind,double x,double y,bool shift){ii::gui::WidgetInput e;e.kind=kind;e.x=x;e.y=y;e.buttons=1;e.shift=shift;check(view.input(e).has_value(),"shape input failed");};send(K::pointer_down,5,5,true);send(K::pointer_move,25,15,true);send(K::pointer_up,25,15,true);const auto& pv=vector(pointer,0);check(pv.paths[0].fill&&pv.paths[0].stroke&&pv.viewport.width==pv.viewport.height,"pointer ellipse options or constraint lost");
 Artwork opacity(dir.path());check(opacity.create(64,64),"opacity fixture failed");ShapeStyle mixed;mixed.fill=true;mixed.outline=true;mixed.stroke_width=8;mixed.opacity=.5;check(opacity.styled_shape("rectangle",{8,8},{40,40},0xffff0000U,mixed,0),"opacity shape failed");check((pixel(opacity,9,24,0)>>24)<=129&&(pixel(opacity,20,24,0)>>24)<=129,"fill and outline compounded shape opacity");
 std::cout<<"STYLED_SHAPE_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
