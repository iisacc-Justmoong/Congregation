#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t pixel(const Artwork& a,int x,int y,FrameIndex frame=1){auto r=renderFrameRegion(a.document(),frame,{{x,y},{1,1}},{1,1});check(r.ok(),"sparse fill render failed");return r.pixels.pixels[0];}
int main()try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());
 check(art.create(9000,9000)&&art.timing(24,3)&&art.set_selection(SelectionGeometry::Rectangle,{10.25,10.25},{20.75,20.75})&&art.set_selection(SelectionGeometry::Rectangle,{8500.25,8500.25},{8510.75,8510.75},MaskOperation::Add),"sparse fill fixture failed");
 FloodOptions o;o.current_layer=true;o.target_current=false;const auto color=0x8000ff00U;
 if(!art.fill_at({15,15},color,1,o))throw std::runtime_error("sparse fill failed: "+art.error());
 const auto expected=[](std::uint8_t coverage){const auto alpha=(128U*unsigned(coverage)+127)/255;return alpha?(alpha<<24)|0x00ff00U:0;};
 auto selection=art.tiled_selection_handle();check(selection&&pixel(art,15,15)==expected(selection->coverage(15,15))&&pixel(art,10,10)==expected(selection->coverage(10,10))&&pixel(art,8505,8505)==expected(selection->coverage(8505,8505))&&pixel(art,4500,4500)==0&&pixel(art,15,15,0)==0&&pixel(art,15,15,2)==0,"sparse fill pixels or frame differ");
 auto* layer=findLayer(art.document(),art.selected());auto* asset=layer?resolveAssetAt(art.document(),*layer,1):nullptr;auto* chunks=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;check(chunks&&chunks->chunks.size()==2,"sparse fill was not stored sparsely");
 check(art.undo()&&pixel(art,15,15)==0&&art.redo()&&pixel(art,8505,8505)==expected(selection->coverage(8505,8505)),"sparse fill history failed");
 const auto path=dir.path()/"sparse-fill.iisc";check(art.save(path),"sparse fill save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,15,15)==expected(selection->coverage(15,15))&&pixel(reopened,8505,8505)==expected(selection->coverage(8505,8505))&&pixel(reopened,4500,4500)==0,"sparse fill reopen failed");
 check(art.undo()&&art.can_redo(),"transparent fill fixture failed");const auto before=art.snapshot();const auto revision=art.revision();check(art.fill_at({15,15},0,1,o)&&art.snapshot()==before&&art.revision()==revision&&art.can_redo(),"transparent sparse fill changed history");
 Artwork separated(dir.path());check(separated.create(9000,9000),"separated color fixture failed");BitmapBrush brush;brush.size=8;brush.argb=0xffff0000U;brush.pressureToOpacityEnabled=false;
 check(separated.begin({15,15},1,brush)&&separated.end({15,15},1)&&separated.begin({8505,8505},1,brush)&&separated.end({8505,8505},1)&&separated.set_selection(SelectionGeometry::Rectangle,{0,0},{9000,9000}),"separated reference setup failed");
 FloodOptions all;all.current_layer=true;all.contiguous=false;if(!separated.fill_at({15,15},0xff00ff00U,0,all))throw std::runtime_error("sparse reference fill failed: "+separated.error());
 check(pixel(separated,15,15,0)==0xff00ff00U&&pixel(separated,8505,8505,0)==0xff00ff00U&&pixel(separated,4500,4500,0)==0,"sparse reference fill missed or escaped regions");
 auto* selectedLayer=findLayer(separated.document(),separated.selected());auto* selectedAsset=selectedLayer?resolveAssetAt(separated.document(),*selectedLayer,0):nullptr;auto* selectedChunks=selectedAsset?std::get_if<ChunkedRasterAsset>(selectedAsset):nullptr;check(selectedChunks&&selectedChunks->chunks.size()==2&&separated.undo()&&pixel(separated,15,15,0)!=0xff00ff00U&&separated.redo()&&pixel(separated,8505,8505,0)==0xff00ff00U,"sparse reference fill storage or history failed");
 check(separated.undo(),"sparse wand setup undo failed");const auto wandRevision=separated.revision();
 check(separated.set_wand_selection({15,15},0,0,false,MaskOperation::Replace,true,{false,0})&&separated.tiled_selection_handle()&&separated.tiled_selection_handle()->coverage(15,15)==255&&separated.tiled_selection_handle()->coverage(8505,8505)==255&&separated.tiled_selection_handle()->coverage(4500,4500)==0&&separated.revision()==wandRevision,"sparse noncontiguous wand mismatch");
 check(separated.set_wand_selection({4500,4500},0,0,false,MaskOperation::Replace,true,{false,0})&&separated.tiled_selection_handle()->coverage(4500,4500)==255&&separated.tiled_selection_handle()->coverage(15,15)==0&&separated.tiled_selection_handle()->coverage(8505,8505)==0&&separated.revision()==wandRevision,"sparse transparent exterior mismatch");

 const auto working=dir.path()/"sparse-working",moved=dir.path()/"sparse-moved";std::filesystem::create_directory(working);Artwork failure(working);check(failure.create(9000,9000)&&failure.set_selection(SelectionGeometry::Rectangle,{10,10},{20,20})&&failure.set_selection(SelectionGeometry::Rectangle,{8500,8500},{8510,8510},MaskOperation::Add)&&failure.set_selection(SelectionGeometry::Rectangle,{1,1},{2,2})&&failure.undo()&&failure.can_redo(),"sparse fill rollback fixture failed");
 const auto failedBefore=failure.snapshot();const auto failedRevision=failure.revision();std::filesystem::rename(working,moved);const bool failed=!failure.fill_at({15,15},0xff00ff00U,0,o);std::filesystem::rename(moved,working);
 check(failed&&failure.snapshot()==failedBefore&&failure.revision()==failedRevision&&failure.can_redo(),"sparse fill storage failure changed document or redo");
 check(failure.fill_at({15,15},0xff00ff00U,0,o)&&failure.undo()&&failure.redo(),"sparse fill recovery failed");
 Artwork budget(dir.path());check(budget.create(9000,9000)&&budget.set_selection(SelectionGeometry::Rectangle,{0,0},{9000,9000})&&budget.set_selection(SelectionGeometry::Rectangle,{1,1},{2,2})&&budget.undo()&&budget.can_redo(),"sparse fill budget fixture failed");
 const auto budgetBefore=budget.snapshot();const auto budgetRevision=budget.revision();check(!budget.fill_at({5,5},0xff00ff00U,0,o)&&budget.snapshot()==budgetBefore&&budget.revision()==budgetRevision&&budget.can_redo(),"oversized sparse fill changed document or redo");
 std::cout<<"SPARSE_FILL_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
