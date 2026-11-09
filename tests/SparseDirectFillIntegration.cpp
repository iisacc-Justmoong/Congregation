#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t pixel(const Artwork& a,int x,int y,FrameIndex frame=1){auto r=renderFrameRegion(a.document(),frame,{{x,y},{1,1}},{1,1});check(r.ok(),"direct fill render failed");return r.pixels.pixels[0];}
int main()try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");Artwork art(dir.path());
 check(art.create(9000,9000)&&art.timing(24,3)&&art.set_selection(SelectionGeometry::Rectangle,{10,10},{20,20})&&art.set_selection(SelectionGeometry::Rectangle,{8500,8500},{8510,8510},MaskOperation::Add),"direct sparse fixture failed");
 FloodOptions o;o.current_layer=true;o.target_current=true;if(!art.fill_at({15,15},0xff00ff00U,1,o))throw std::runtime_error("sparse direct fill failed: "+art.error());
 check(pixel(art,15,15)==0xff00ff00U&&pixel(art,8505,8505)==0xff00ff00U&&pixel(art,4500,4500)==0&&pixel(art,15,15,0)==0&&pixel(art,15,15,2)==0,"direct fill escaped selection or frame");
 check(art.undo()&&pixel(art,15,15)==0&&art.redo()&&pixel(art,8505,8505)==0xff00ff00U,"direct sparse history failed");const auto path=dir.path()/"direct-sparse.iisc";check(art.save(path),"direct sparse save failed");Artwork reopened(dir.path());check(reopened.open(path)&&pixel(reopened,15,15)==0xff00ff00U&&pixel(reopened,8505,8505)==0xff00ff00U&&pixel(reopened,4500,4500)==0,"direct sparse reopen failed");
 check(art.undo()&&art.can_redo(),"direct sparse no-op setup failed");const auto before=art.snapshot();const auto rev=art.revision();check(art.fill_at({15,15},0,1,o)&&art.snapshot()==before&&art.revision()==rev&&art.can_redo(),"direct sparse transparent fill changed redo");
 auto alphaLock=o;alphaLock.preserve_alpha=true;check(art.fill_at({15,15},0xff00ff00U,1,alphaLock)&&art.snapshot()==before&&art.revision()==rev&&art.can_redo(),"alpha-locked sparse no-op changed history");
 Document d;d.extent={9000,9000};d.timeline={{24,1},3};ChunkedRasterAsset initial;initial.id="noise";std::uint32_t random=13;
 for(int i=0;i<6;++i){const int index=i==5?33:i*5;auto pixels=makeRasterLayer(256,256);for(auto& value:pixels.pixels){random^=random<<13;random^=random>>17;random^=random<<5;value=0xff000000U|(random&0xffffffU);}if(i==0 || i==5){const int x0=i==0?10:52,y0=x0;for(int y=y0;y<y0+10;++y)for(int x=x0;x<x0+10;++x)pixels.pixels[std::size_t(y)*256+x]=0xffff0000U;}initial.chunks.push_back({index,index,std::move(pixels)});}
 d.assets.emplace_back(std::move(initial));d.layers.emplace_back(StaticBitmapLayer{{"paint","Paint"},StaticSource{"noise"}});Artwork compact(dir.path(),2*1024*1024);
 check(compact.replace(std::move(d))&&compact.snapshot().size()>1024*1024&&compact.set_selection(SelectionGeometry::Rectangle,{10,10},{20,20})&&compact.set_selection(SelectionGeometry::Rectangle,{8500,8500},{8510,8510},MaskOperation::Add),"compact direct fixture failed");
 FloodOptions noncontiguous=o;noncontiguous.contiguous=false;if(!compact.fill_at({15,15},0xff00ff00U,1,noncontiguous))throw std::runtime_error("compact sparse direct fill failed: "+compact.error());
 check(pixel(compact,15,15)==0xff00ff00U&&pixel(compact,8505,8505)==0xff00ff00U&&pixel(compact,15,15,0)==0xffff0000U&&pixel(compact,15,15,2)==0xffff0000U&&compact.history_bytes()<=2*1024*1024,"compact direct frame or budget failed");
 check(compact.undo()&&pixel(compact,15,15)==0xffff0000U&&compact.redo()&&pixel(compact,8505,8505)==0xff00ff00U,"compact direct undo redo failed");
 const auto working=dir.path()/"direct-working",moved=dir.path()/"direct-moved";std::filesystem::create_directory(working);Artwork failure(working);
 check(failure.create(9000,9000)&&failure.set_selection(SelectionGeometry::Rectangle,{10,10},{20,20})&&failure.set_selection(SelectionGeometry::Rectangle,{8500,8500},{8510,8510},MaskOperation::Add)&&failure.set_selection(SelectionGeometry::Rectangle,{1,1},{2,2})&&failure.undo()&&failure.can_redo(),"direct rollback fixture failed");
 const auto failedBefore=failure.snapshot();const auto failedRevision=failure.revision();std::filesystem::rename(working,moved);const bool failed=!failure.fill_at({15,15},0xff00ff00U,0,o);std::filesystem::rename(moved,working);
 check(failed&&failure.snapshot()==failedBefore&&failure.revision()==failedRevision&&failure.can_redo(),"direct fill storage failure changed document or redo");
 check(failure.fill_at({15,15},0xff00ff00U,0,o)&&failure.undo()&&failure.redo(),"direct fill storage recovery failed");
 std::cout<<"SPARSE_DIRECT_FILL_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
