#include <Canvas/Artwork.h>
#include <Tools/ToolSettings.h>
#include <iiFileProvider>
#include <iiSharedCanvas>
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

 ToolSettings wetBristle;check(wetBristle.select_brush("bristle-brush"),"bristle profile was not selected");
 wetBristle.set(P::size,16);wetBristle.set(P::count,8);wetBristle.set(P::taper,0);
 wetBristle.set(P::smoothing_enabled,0);
 auto bristle_wetness=[&] {
  Artwork fixture(dir.path());check(fixture.create(32,32),"wet bristle fixture failed");
  auto brush=wetBristle.brush("bristle-brush");brush.argb=0xff000000U;
  check(fixture.begin({16,16},1,brush) && fixture.end({16,16},1),"wet bristle stroke failed");
  const auto result=pixels(fixture).pixels;
  check(fixture.undo() && fixture.redo() && pixels(fixture).pixels==result,"wet bristle history failed");
  return result;
 };
 const auto wetBristlePixels=bristle_wetness();wetBristle.set(P::wetness,0);
 check(wetBristlePixels!=bristle_wetness(),"Bristle default wetness did not affect painted pixels");

 settings.reset();
 auto stamp=[&](std::string_view tool){ Artwork fixture(dir.path());check(fixture.create(32,32),"profile fixture failed");settings.set(P::size,16);settings.set(P::flow,100);settings.set(P::opacity,100);settings.set(P::hardness,100);settings.set(P::scatter,0);settings.set(P::taper,0);auto brush=settings.brush(tool);brush.argb=0xff000000U;check(fixture.begin({16,16},1,brush) && fixture.end({16,16},1),"profile stroke failed");return pixels(fixture).pixels;};
 const auto pencil=stamp("pencil");std::size_t drawn=0;for(auto px:pencil)if(auto a=px>>24){++drawn;check(a==255,"pencil edge is not binary at full opacity and flow");}check(drawn>0,"pencil has no pixels");
 const auto normal=stamp("brush"),dry=stamp("dry-brush");check(normal!=dry,"dry brush must differ from a plain brush");
 settings.set(P::dry_grain_amount,0);settings.set(P::dry_dryness,0);const auto noGrain=stamp("dry-brush");
 settings.set(P::dry_grain_amount,80);const auto strongGrain=stamp("dry-brush");
 check(strongGrain!=noGrain,"Dry brush grain amount did not change painted pixels");
 settings.set(P::dry_grain_size,5);const auto largeGrain=stamp("dry-brush");
 check(largeGrain!=strongGrain,"Dry brush grain size did not change painted pixels");
 settings.set(P::dry_dryness,80);check(stamp("dry-brush")!=largeGrain,"Dryness did not change painted pixels");
 settings.set(P::airbrush_radius,50);const auto smallSpray=stamp("airbrush");
 settings.set(P::airbrush_radius,150);check(stamp("airbrush")!=smallSpray,"Spray radius did not change painted pixels");

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
 settings.set(P::noise_monochrome,1); const auto monochrome=stamp("brush");
 settings.set(P::noise_monochrome,0); const auto colored=stamp("brush");
 check(monochrome.size()==colored.size(),"Noise changed the canvas size");
 bool coloredNoise=false;
 for(std::size_t i=0;i<colored.size();++i) {
  check((monochrome[i]>>24)==(colored[i]>>24),"Noise color mode changed coverage");
  coloredNoise|=(monochrome[i]&0xffffffU)!=(colored[i]&0xffffffU);
 }
 check(coloredNoise,"Noise Monochrome did not change the painted colors");
 settings.set(P::noise_enabled,0); settings.set(P::wet_edges_enabled,1); settings.set(P::wet_edges_strength,80);
 check(stamp("brush")!=baseline,"Wet edge controls did not change painted pixels");
 settings.set(P::wet_edges_enabled,0); settings.set(P::build_up_enabled,1); settings.set(P::build_up_rate,25);
 check(stamp("brush")!=baseline,"Build-up controls did not change painted pixels");
 settings.set(P::build_up_enabled,0); settings.set_background(0xffffffffU); settings.set(P::color_mix_jitter,100);
 check(stamp("brush")!=baseline,"Foreground/background dynamics did not change painted pixels");
 settings.set(P::color_mix_jitter,0); settings.set_texture("test",2,2,{0,255,0,255});
 settings.set(P::texture_mode,0); settings.set(P::texture_scale,100); settings.set(P::texture_depth,100);
 check(stamp("brush")!=baseline,"Texture asset did not change painted pixels");
 auto mixer_sample=[&](bool all) {
  Artwork fixture(dir.path());check(fixture.create(32,32),"mixer fixture failed");
  check(fixture.import_pixels(makeRasterLayer(32,32,0xff0000ffU),"Blue",0,0) && fixture.add_layer("Paint"),
        "mixer source layers failed");
  ToolSettings mix;mix.set(P::size,8);mix.set(P::flow,100);mix.set(P::opacity,100);
  mix.set(P::scatter,0);mix.set(P::taper,0);mix.set(P::smoothing_enabled,0);
  mix.set(P::wetness,100);mix.set(P::mix,100);mix.set(P::dilution,100);mix.set(P::load,100);
  auto brush=mix.brush("mixer-brush");brush.argb=0xffff0000U;
  StrokePoint sample;sample.position={16,16};sample.pressure=1;
  check(fixture.begin_input(sample,brush,0,{},nullptr,all),"mixer stroke begin failed");
  check(fixture.end_input(sample),"mixer stroke end failed");
  const auto painted=pixels(fixture).pixels;
  check(fixture.undo() && fixture.redo() && pixels(fixture).pixels==painted,"mixer sample history failed");
  return painted[16*32+16];
 };
 const auto current=mixer_sample(false), all=mixer_sample(true);
 check(((current>>16)&255)>(current&255) && (all&255)>((all>>16)&255),
       "Mixer sampling must use the selected layer or the visible layer composite");
 Artwork blendArt(dir.path());check(blendArt.create(32,32),"blend fixture failed");
 check(blendArt.import_pixels(makeRasterLayer(32,32,0xff0000ffU),"Blue",0,0),"blend base failed");
 ToolSettings blendSettings;blendSettings.set(P::size,8);blendSettings.set(P::flow,100);blendSettings.set(P::opacity,100);
 blendSettings.set(P::hardness,100);blendSettings.set(P::scatter,0);blendSettings.set(P::taper,0);
 blendSettings.set(P::smoothing_enabled,0);blendSettings.set(P::blend_mode,1);
 auto multiply=blendSettings.brush("pencil");multiply.argb=0xffff0000U;
 check(blendArt.begin({16,16},1,multiply) && blendArt.end({16,16},1),"multiply stroke failed");
 const auto blended=pixels(blendArt).pixels;check((blended[16*32+16]&0xffffffU)==0,"Multiply did not blend against the destination");
 check(blendArt.undo() && blendArt.redo() && pixels(blendArt).pixels==blended,"Multiply history failed");
 Artwork symmetryArt(dir.path());check(symmetryArt.create(32,32),"symmetry fixture failed");
 blendSettings.set(P::blend_mode,0);blendSettings.set(P::symmetry_mode,1);
 auto mirror=blendSettings.brush("pencil");mirror.argb=0xffff0000U;
 mirror.engineState->rasterizer.symmetry.center={16,16};
 check(symmetryArt.begin({8,16},1,mirror) && symmetryArt.end({8,16},1),"symmetry stroke failed");
 const auto mirrored=pixels(symmetryArt).pixels;
 check((mirrored[16*32+8]>>24)>0 && (mirrored[16*32+24]>>24)>0,"Symmetry did not paint the reflected location");
 check(symmetryArt.undo() && symmetryArt.redo() && pixels(symmetryArt).pixels==mirrored,"Symmetry history failed");
 std::cout<<"BRUSH_TOOLS_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
