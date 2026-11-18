#include <Canvas/Artwork.h>
#include <iiFileProvider>
#include <iiSharedCanvas>
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
 {
  // A tiny metadata edit must fit even when one complete document snapshot
  // is already larger than the entire history budget.
  Document d;d.extent={256,256};d.formatVersion.minor=CurrentFormatMinor-1;
  d.assets.emplace_back(RasterAsset{"paint",noise(256)});
  d.layers.emplace_back(StaticBitmapLayer{{"layer","Before"},StaticSource{"paint"}});
  constexpr std::size_t limit=128*1024;
  check(encodeIisc(d).bytes.size()>limit,"delta-history fixture is too small");
  Artwork delta(dir.path(),limit);check(delta.replace(std::move(d)) &&
   delta.set_selection(SelectionGeometry::Rectangle,{16,16},{64,64}),"delta-history fixture failed");
  const auto before=delta.snapshot();const auto oldVersion=delta.document().formatVersion;
  const auto selection=delta.selection_mask_handle();const auto initialRevision=delta.revision();
  check(delta.rename("layer","After"),"small property edit unexpectedly required a whole-document snapshot");
  const auto after=delta.snapshot();check(delta.revision()==initialRevision+1 &&
   delta.selection_mask_handle()==selection && delta.history_bytes()<=limit,"property edit damaged mask or budget");
  check(delta.undo() && delta.snapshot()==before && delta.selection_mask_handle()==selection &&
   delta.document().formatVersion.major==oldVersion.major && delta.document().formatVersion.minor==oldVersion.minor,
   "property undo did not restore exact document, mask and version");
  check(delta.undo() && !delta.has_selection() && delta.redo() && delta.selection_mask_handle()==selection &&
   delta.redo() && delta.snapshot()==after,"property and selection history order changed");
  check(delta.undo() && delta.can_redo(),"delta redo fixture failed");
  namespace sql=iiFileProvider::native::sql;
  auto database=sql::Database::open(delta.recovery());check(bool(database),"cannot open delta recovery");
  auto lock=sql::Transaction::begin(*database,true);check(bool(lock),"cannot lock delta recovery");
  const auto lockedRevision=delta.revision();
  check(!delta.redo() && delta.snapshot()==before && delta.selection_mask_handle()==selection &&
   delta.revision()==lockedRevision && delta.can_redo(),"failed property redo changed document, mask or stack");
  check(bool(lock->rollback()) && delta.redo() && delta.snapshot()==after,"property redo failed after recovery");
  const auto renameState=delta.snapshot();
  check(delta.visible("layer",false),"visibility delta failed");const auto hiddenState=delta.snapshot();
  check(delta.opacity("layer",.4),"opacity delta failed");const auto opacityState=delta.snapshot();
  check(delta.cycle_blend("layer"),"blend delta failed");const auto blendState=delta.snapshot();
  ::AffineTransform offset;offset.translationX=12;offset.translationY=-4;
  check(delta.transform(offset),"transform delta failed");const auto transformedState=delta.snapshot();
  check(delta.history_bytes()<=limit && delta.selection_mask_handle()==selection,"property sequence changed mask or exceeded budget");
  check(delta.undo()&&delta.snapshot()==blendState&&delta.undo()&&delta.snapshot()==opacityState&&
        delta.undo()&&delta.snapshot()==hiddenState&&delta.undo()&&delta.snapshot()==renameState,
        "property undo sequence is not exact");
  const auto noOpRevision=delta.revision();
  check(delta.visible("layer",true)&&delta.revision()==noOpRevision&&delta.can_redo(),"no-op discarded redo or changed revision");
  check(delta.redo()&&delta.snapshot()==hiddenState&&delta.redo()&&delta.snapshot()==opacityState&&
        delta.redo()&&delta.snapshot()==blendState&&delta.redo()&&delta.snapshot()==transformedState,
        "property redo sequence is not exact");
  auto lockEdit=sql::Transaction::begin(*database,true);check(bool(lockEdit),"cannot lock property edit");
  const auto beforeRejectedEdit=delta.snapshot();const auto editRevision=delta.revision();
  check(!delta.rename("layer","Locked")&&delta.snapshot()==beforeRejectedEdit&&
        delta.revision()==editRevision&&delta.selection_mask_handle()==selection,
        "failed property edit changed document, mask or revision");
  check(bool(lockEdit->rollback())&&delta.rename("layer","Unlocked"),"property edit did not recover after unlock");
 }
 {
  Artwork mixed(dir.path());check(mixed.create(64,64),"mixed property-stroke fixture failed");
  check(mixed.set_selection(SelectionGeometry::Rectangle,{4,4},{60,60}),"mixed selection failed");
  const auto selected=mixed.selection_mask_handle();const auto beforeStroke=mixed.snapshot();
  BitmapBrush paint;paint.size=6;paint.argb=0xff274bc1U;
  check(mixed.begin({12,12},1,paint)&&mixed.extend({32,30},.7)&&mixed.end({45,45},.5),"mixed stroke failed");
  const auto afterStroke=mixed.snapshot();check(afterStroke!=beforeStroke,"mixed stroke changed no pixels");
  check(mixed.rename("paint-layer","Ink"),"mixed property edit failed");const auto afterRename=mixed.snapshot();
  check(mixed.undo()&&mixed.snapshot()==afterStroke&&mixed.selection_mask_handle()==selected&&
        mixed.undo()&&mixed.snapshot()==beforeStroke&&mixed.selection_mask_handle()==selected&&
        mixed.undo()&&!mixed.has_selection(),"mixed undo ordering or mask failed");
  check(mixed.redo()&&mixed.selection_mask_handle()==selected&&mixed.redo()&&mixed.snapshot()==afterStroke&&
        mixed.redo()&&mixed.snapshot()==afterRename,"mixed redo ordering or mask failed");
 }
 {
  Artwork conflicted(dir.path());check(conflicted.create(32,32)&&conflicted.rename("paint-layer","Local"),"conflict fixture failed");
  const auto local=conflicted.snapshot();const auto revision=conflicted.revision();const auto undo=conflicted.can_undo();
  DocumentFile external;check(external.open(conflicted.recovery().string()).ok(),"cannot open concurrent editor");
  DocumentEditor externalEdit(external);check(externalEdit.setLayerName("paint-layer","External").changed,"concurrent edit failed");
  check(!conflicted.rename("paint-layer","Rejected")&&conflicted.snapshot()==local&&
        conflicted.revision()==revision&&conflicted.can_undo()==undo,"conflict changed the local document or history");
  check(!conflicted.rename("paint-layer","Retry")&&!conflicted.undo()&&conflicted.snapshot()==local&&
        conflicted.can_undo()==undo,"an unavailable working file did not reject safely");
 }
 Document d;d.extent={128,128};d.assets.emplace_back(RasterAsset{"large",noise(128)});d.layers.emplace_back(StaticBitmapLayer{{"large-layer","Large"},StaticSource{"large"}});Artwork big(dir.path(),16*1024);check(big.replace(std::move(d)),"large fixture failed");const auto largeRevision=big.revision();BitmapBrush brush;brush.size=8;brush.argb=0xff112233U;check(!big.begin({32,32},1,brush),"oversized stroke should fail");check(!big.stroke_active()&&big.revision()==largeRevision&&!big.can_undo(),"rejected stroke changed document");
 auto saved=dir.path()/"budget.iisc";check(big.save(saved),"large save failed");Artwork loaded(dir.path());check(loaded.open(saved)&&loaded.document().extent==CanvasExtent{128,128},"rejected stroke damaged save");
 std::cout<<"HISTORY_BUDGET_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
