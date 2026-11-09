#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
RasterLayer noise(int n){auto out=makeRasterLayer(n,n,0);std::uint32_t v=0x12345678U;for(auto& pixel:out.pixels){v^=v<<13;v^=v>>17;v^=v<<5;pixel=0xff000000U|(v&0xffffffU);}return out;}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temp unavailable");
 Artwork art(dir.path(),16*1024);check(art.create(8,8)&&art.add_layer("temporary"),"redo fixture failed");const auto recorded=encodeIisc(art.document());check(recorded.ok()&&art.undo()&&art.can_redo(),"redo fixture failed");const auto revision=art.revision();const auto layers=art.document().layers.size();
 check(!art.import_pixels(noise(128),"oversized",0,0),"oversized edit should fail");check(art.revision()==revision&&art.document().layers.size()==layers&&art.can_redo(),"rejected edit changed document or redo");check(art.redo()&&art.document().layers.size()==layers+1,"redo after rejected edit failed");const auto redone=encodeIisc(art.document());check(redone.ok()&&redone.bytes==recorded.bytes,"redo did not restore the exact recorded document");
 Document d;d.extent={128,128};d.assets.emplace_back(RasterAsset{"large",noise(128)});d.layers.emplace_back(StaticBitmapLayer{{"large-layer","Large"},StaticSource{"large"}});Artwork big(dir.path(),16*1024);check(big.replace(std::move(d)),"large fixture failed");const auto largeRevision=big.revision();BitmapBrush brush;brush.size=8;brush.argb=0xff112233U;check(!big.begin({32,32},1,brush),"oversized stroke should fail");check(!big.stroke_active()&&big.revision()==largeRevision&&!big.can_undo(),"rejected stroke changed document");
 auto saved=dir.path()/"budget.iisc";check(big.save(saved),"large save failed");Artwork loaded(dir.path());check(loaded.open(saved)&&loaded.document().extent==CanvasExtent{128,128},"rejected stroke damaged save");
 std::cout<<"HISTORY_BUDGET_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
