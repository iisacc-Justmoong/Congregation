#include <Native/Artwork.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const RasterLayer& pixels(const Artwork& art) {
 for(const auto& item:art.document().layers) if(layerProperties(item).id==art.selected()) {
  const auto* source=staticLayerSource(item);check(source!=nullptr,"bitmap source missing");
  const auto* asset=findRasterAsset(art.document(),source->assetId);check(asset!=nullptr,"bitmap asset missing");return asset->pixels;
 }
 throw std::runtime_error("selected layer missing");
}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temp unavailable");
 using P=ToolParameter;ToolSettings settings;settings.set(P::size,8);settings.set(P::wetness,100);settings.set(P::mix,100);settings.set(P::dilution,100);settings.set(P::scatter,0);settings.set(P::taper,0);settings.set(P::stabilization,0);settings.set(P::post,0);
 Artwork art(dir.path());check(art.create(128,64),"smudge create failed");auto source=makeRasterLayer(128,64,0);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)source.pixels[std::size_t(y)*128+x]=0xff0000ffU;
 check(art.import_pixels(std::move(source),"smudge fixture",0,0),"smudge import failed");
 check(art.set_selection(SelectionGeometry::Rectangle,{64,0},{80,64}),"smudge selection failed");auto before=pixels(art).pixels;
 auto smudge=settings.brush("smudge");smudge.argb=0xffff0000U;smudge.engineState->rasterizer.airbrushEnabled=true;smudge.engineState->rasterizer.airbrushRate=100;
 StrokePoint point;point.position={64,32};point.time=1;check(art.begin_input(point,smudge),"smudge begin failed");point.time=2;check(art.end_input(point),"smudge end failed");



 const auto result=pixels(art).pixels;const auto changed=result[32*128+64];check((changed>>24)>0 && (changed&255)>0 && ((changed>>16)&255)==0,"smudge must pull existing blue without injecting red");
 check(result[32*128+63]==before[32*128+63] && result[32*128+80]==before[32*128+80],"smudge escaped selection");
 check(art.undo() && pixels(art).pixels==before && art.redo() && pixels(art).pixels==result,"smudge undo/redo failed");
 auto footprint=[&](int count){ Artwork fixture(dir.path());check(fixture.create(32,32),"bristle fixture failed");settings.set(P::count,count);settings.set(P::size,16);settings.set(P::hardness,100);auto brush=settings.brush("bristle-brush");brush.argb=0xff000000U;check(fixture.begin({16,16},1,brush) && fixture.end({16,16},1),"bristle stroke failed");std::size_t n=0;for(auto px:pixels(fixture).pixels)if(px>>24)++n;return n;};
 const auto one=footprint(1),many=footprint(16);check(one>0 && many>one,"bristle count did not change painted footprint");

 auto stamp=[&](std::string_view tool){ Artwork fixture(dir.path());check(fixture.create(32,32),"profile fixture failed");settings.set(P::size,16);settings.set(P::flow,100);settings.set(P::opacity,100);settings.set(P::hardness,100);settings.set(P::scatter,0);settings.set(P::taper,0);auto brush=settings.brush(tool);brush.argb=0xff000000U;check(fixture.begin({16,16},1,brush) && fixture.end({16,16},1),"profile stroke failed");return pixels(fixture).pixels;};
 const auto pencil=stamp("pencil");std::size_t drawn=0;for(auto px:pencil)if(auto a=px>>24){++drawn;check(a==255,"pencil edge is not binary at full opacity and flow");}check(drawn>0,"pencil has no pixels");
 const auto normal=stamp("brush"),dry=stamp("dry-brush");check(normal!=dry,"dry brush must differ from a plain brush");

 for(auto tool:{"pencil","dry-brush"}) {
  Artwork fixture(dir.path());check(fixture.create(32,32),"selected profile fixture failed");
  check(fixture.set_selection(SelectionGeometry::Rectangle,{16,0},{32,32}),"profile selection failed");
  auto brush=settings.brush(tool);brush.argb=0xff000000U;check(fixture.begin({16,16},1,brush) && fixture.end({16,16},1),"selected profile stroke failed");
  const auto result=pixels(fixture).pixels;std::size_t inside=0;for(int y=0;y<32;++y)for(int x=0;x<32;++x){auto a=result[std::size_t(y)*32+x]>>24;if(x<16)check(a==0,"profile escaped selection");else if(a)++inside;}check(inside>0,"selected profile missing pixels");
  check(fixture.undo() && fixture.redo() && pixels(fixture).pixels==result,"profile undo/redo failed");
 }
 settings.reset(); settings.set(P::size,16); settings.set(P::flow,100); settings.set(P::opacity,100);
 settings.set(P::hardness,100); settings.set(P::scatter,0); settings.set(P::taper,0);
 settings.set(P::smoothing_enabled,0);
 const auto baseline=stamp("brush");
 settings.set(P::noise_enabled,1); settings.set(P::noise_amount,80); settings.set(P::noise_grain_size,4);
 check(stamp("brush")!=baseline,"Noise grain controls did not change painted pixels");
 settings.set(P::noise_enabled,0); settings.set(P::wet_edges_enabled,1); settings.set(P::wet_edges_strength,80);
 check(stamp("brush")!=baseline,"Wet edge controls did not change painted pixels");
 settings.set(P::wet_edges_enabled,0); settings.set(P::build_up_enabled,1); settings.set(P::build_up_rate,25);
 check(stamp("brush")!=baseline,"Build-up controls did not change painted pixels");
 settings.set(P::build_up_enabled,0); settings.set_background(0xffffffffU); settings.set(P::color_mix_jitter,100);
 check(stamp("brush")!=baseline,"Foreground/background dynamics did not change painted pixels");
 settings.set(P::color_mix_jitter,0); settings.set_texture("test",2,2,{0,255,0,255});
 settings.set(P::texture_mode,0); settings.set(P::texture_scale,100); settings.set(P::texture_depth,100);
 check(stamp("brush")!=baseline,"Texture asset did not change painted pixels");
 std::cout<<"BRUSH_TOOLS_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
