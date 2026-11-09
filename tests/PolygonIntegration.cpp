#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
std::uint32_t pixel(const Artwork& a,FrameIndex frame,int x,int y){auto r=renderFrameRegion(a.document(),frame,{{x,y},{1,1}},{1,1});check(r.ok(),"polygon render failed");return r.pixels.pixels[0];}
const VectorAsset& vector(const Artwork& a,FrameIndex frame){auto* layer=findLayer(a.document(),a.selected());check(layer!=nullptr,"polygon layer missing");auto* asset=resolveAssetAt(a.document(),*layer,frame);auto* v=asset?std::get_if<VectorAsset>(asset):nullptr;check(v!=nullptr,"polygon vector asset missing");return *v;}
void send(CanvasView& v,ii::gui::WidgetInputKind kind,double x,double y,bool shift=false){ii::gui::WidgetInput e;e.kind=kind;e.x=x;e.y=y;e.buttons=1;e.shift=shift;check(v.input(e).has_value(),"polygon input failed");}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 Artwork art(dir.path());check(art.create(32,32)&&art.timing(24,3)&&art.set_selection(SelectionGeometry::Rectangle,{12,12},{20,20}),"polygon fixture failed");
 PolygonOptions geometry; geometry.sides=3;
 auto reversed=polygon_bounds({20,20},{4,8},geometry);check(reversed&&reversed->left==4&&reversed->top==8&&reversed->width==16&&reversed->height==12,"negative drag bounds failed");
 geometry.from_center=true;geometry.regular=true;auto centered=polygon_bounds({10,10},{5,7},geometry);check(centered&&centered->left==5&&centered->top==5&&centered->width==10&&centered->height==10,"center regular bounds failed");
 geometry.sides=64;auto many=polygon_points(*centered,geometry);check(many.size()==64,"maximum sides failed");
 PolygonOptions options;options.sides=6;options.rotation_degrees=30;options.from_center=true;options.fill=true;options.outline=true;options.stroke_width=2;
 if(!art.shape("polygon",{16,16},{24,22},0xff0000ffU,0,true,options,1))throw std::runtime_error("polygon creation failed: "+art.error());
 const auto& asset=vector(art,1);check(asset.paths.size()==1&&asset.paths[0].commands.size()==7&&asset.paths[0].fill&&asset.paths[0].stroke,"polygon geometry or style missing");
 check(std::holds_alternative<ClosePath>(asset.paths[0].commands.back()),"polygon is not closed");
 check((pixel(art,1,16,16)>>24)>0&&(pixel(art,1,9,16)>>24)==0&&(pixel(art,0,16,16)>>24)==0&&(pixel(art,2,16,16)>>24)==0,"polygon escaped selection or frame");
 const auto savedPixel=pixel(art,1,16,16);check(art.undo()&&pixel(art,1,16,16)==0&&art.has_selection()&&art.can_redo(),"polygon undo failed");
 const auto revision=art.revision();auto invalid=options;invalid.sides=2;check(!art.shape("polygon",{16,16},{24,22},0xff0000ffU,0,true,invalid,1)&&art.can_redo()&&art.revision()==revision,"invalid polygon changed document or redo");
 invalid=options;invalid.sides=65;check(!art.shape("polygon",{16,16},{24,22},0xff0000ffU,0,true,invalid,1)&&art.can_redo()&&art.revision()==revision,"excess polygon sides changed redo");
 invalid=options;invalid.fill=false;invalid.outline=false;check(!art.shape("polygon",{16,16},{24,22},0xff0000ffU,0,true,invalid,1)&&art.can_redo()&&art.revision()==revision,"invisible polygon changed redo");
 check(art.redo()&&pixel(art,1,16,16)==savedPixel,"polygon redo failed");const auto path=dir.path()/"polygon.iisc";check(art.save(path),"polygon save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,1,16,16)==savedPixel&&pixel(reopened,0,16,16)==0,"polygon reopen failed");
 Artwork drawn(dir.path());check(drawn.create(32,32),"polygon pointer fixture failed");CanvasView view(drawn);view.tool="polygon";view.polygon_options.sides=5;view.polygon_options.fill=true;
 using K=ii::gui::WidgetInputKind;send(view,K::pointer_down,4,4,true);send(view,K::pointer_move,24,14,true);send(view,K::pointer_up,24,14,true);
 const auto& pointed=vector(drawn,0);check(pointed.paths[0].commands.size()==6&&pointed.viewport.width==pointed.viewport.height,"pointer polygon sides or regular constraint failed");
 check(drawn.undo()&&drawn.can_redo(),"pointer polygon undo failed");const auto before=drawn.revision();send(view,K::pointer_down,4,4);send(view,K::pointer_up,4,4);check(drawn.revision()==before&&drawn.can_redo(),"zero-size polygon changed redo");
 Artwork zero(dir.path());check(zero.create(32,32),"zero-width fixture failed");
 PolygonOptions zero_options;zero_options.fill=true;zero_options.outline=true;zero_options.stroke_width=0;
 if(!zero.shape("polygon",{4,4},{24,24},0xff0000ffU,0,true,zero_options,0))throw std::runtime_error("zero-width filled polygon failed: "+zero.error());
 const auto& zero_path=vector(zero,0).paths[0];check(zero_path.fill&&!zero_path.stroke&&(pixel(zero,0,14,14)>>24)>0,"zero-width polygon should retain fill without outline");
 check(zero.undo()&&zero.can_redo(),"zero-width polygon undo failed");const auto zero_revision=zero.revision();
 zero_options.fill=false;check(!zero.shape("polygon",{4,4},{24,24},0xff0000ffU,0,true,zero_options,0)&&zero.revision()==zero_revision&&zero.can_redo(),"invisible zero-width polygon changed document or redo");
 check(zero.redo()&&(pixel(zero,0,14,14)>>24)>0,"zero-width polygon redo failed");
 std::cout<<"POLYGON_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
