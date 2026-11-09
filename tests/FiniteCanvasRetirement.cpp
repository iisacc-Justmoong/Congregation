#include <Native/Artwork.h>
#include <Engine/EditCommand.h>
#include <Engine/RecentCanvas.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 using congregation::Object;using congregation::Value;
 Object command{{"type",Value("new-canvas")},{"width",Value(16)},{"height",Value(16)},{"infinite",Value(false)}};check(congregation::normalize_command(command).has_value(),"finite command rejected");command["infinite"]=Value(true);check(!congregation::normalize_command(command),"infinite command accepted");Object grow{{"type",Value("ensure-region")},{"x",Value(0)},{"y",Value(0)},{"width",Value(10)},{"height",Value(10)}};check(!congregation::normalize_command(grow),"infinite growth command accepted");
 Artwork art(dir.path());check(art.create(16,16)&&art.set_selection(SelectionGeometry::Rectangle,{2,2},{6,6})&&art.add_layer("redo fixture")&&art.undo()&&art.can_redo(),"finite fixture failed");const auto before=encodeIisc(art.document());const auto revision=art.revision();const auto selection=art.selection_mask_handle();check(bool(selection),"selection fixture missing");
 check(!art.create(16,16,true),"infinite creation was accepted");check(art.error().find("Infinite canvas")!=std::string::npos&&art.revision()==revision&&art.can_redo()&&art.selection_mask_handle()==selection&&encodeIisc(art.document()).bytes==before.bytes,"rejected creation changed document or redo");
 Document retired;retired.canvasMode=CanvasMode::Infinite;retired.extent={32,32};retired.assets.emplace_back(ChunkedRasterAsset{"paint",{}});retired.layers.emplace_back(StaticBitmapLayer{{"paint-layer","Paint"},StaticSource{"paint"}});auto bytes=encodeIisc(retired);check(bytes.ok(),"retired file encoding failed");const auto path=dir.path()/"retired.iisc";iiFileProvider::native::File::write(path,{reinterpret_cast<const char*>(bytes.bytes.data()),bytes.bytes.size()});
 check(!art.receive({reinterpret_cast<const char*>(bytes.bytes.data()),bytes.bytes.size()}),"infinite collaboration snapshot accepted");check(art.error().find("Infinite canvas")!=std::string::npos&&art.revision()==revision&&art.can_redo()&&art.selection_mask_handle()==selection&&encodeIisc(art.document()).bytes==before.bytes,"rejected snapshot changed document or redo");
 check(!art.open(path),"infinite file was accepted");check(art.error().find("Infinite canvas")!=std::string::npos&&art.revision()==revision&&art.can_redo()&&art.selection_mask_handle()==selection&&encodeIisc(art.document()).bytes==before.bytes,"rejected open changed document or redo");const auto stored=iiFileProvider::native::File::read(path,bytes.bytes.size()+1);check(stored.size()==bytes.bytes.size()&&std::equal(stored.begin(),stored.end(),reinterpret_cast<const char*>(bytes.bytes.data())),"rejected open changed file");
 congregation::RecentCanvas legacy;legacy.document.assign(reinterpret_cast<const char*>(bytes.bytes.data()),bytes.bytes.size());auto legacyBytes=congregation::encode_canvas(legacy);check(bool(legacyBytes),"legacy fixture encoding failed");const auto legacyPath=dir.path()/"retired.vrc";iiFileProvider::native::File::write(legacyPath,*legacyBytes);check(!art.open(legacyPath)&&art.error().find("Infinite canvas")!=std::string::npos&&art.revision()==revision&&art.can_redo()&&art.selection_mask_handle()==selection&&iiFileProvider::native::File::read(legacyPath)==*legacyBytes,"legacy infinite open changed state or file");
 check(art.redo(),"redo lost after rejection");const auto finite=dir.path()/"finite.iisc";check(art.save(finite),"finite save failed");Artwork reopened(dir.path());check(reopened.open(finite)&&reopened.document().canvasMode==CanvasMode::Finite,"finite reopen failed");std::cout<<"FINITE_CANVAS_RETIREMENT_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
