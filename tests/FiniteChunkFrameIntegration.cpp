#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <span>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Document fixture(unsigned frames=3,bool shared=true){
 Document d;d.extent={9000,9000};d.timeline={{24,1},frames};
 auto pixels=makeRasterLayer(256,256);std::uint32_t random=1;
 for(auto& p:pixels.pixels){random^=random<<13;random^=random>>17;random^=random<<5;p=0xff000000U|(random&0xffffffU);}
 d.assets.emplace_back(ChunkedRasterAsset{"source",{{0,0,std::move(pixels)}}});
 if(shared)d.layers.emplace_back(StaticBitmapLayer{{"base","Base"},StaticSource{"source"}});
 d.layers.emplace_back(StaticBitmapLayer{{"paint","Paint"},StaticSource{"source"}});return d;
}
std::uint32_t pixel(const Artwork& art,std::string_view id,FrameIndex frame,int x,int y){
 for(const auto& layer:art.document().layers)if(layerProperties(layer).id==id){auto* asset=resolveAssetAt(art.document(),layer,frame);auto* chunks=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;check(chunks!=nullptr,"chunk frame unavailable");const int size=art.document().infiniteCanvas.chunkSize;const auto div=[size](int v){const int q=v/size,r=v%size;return r<0?q-1:q;};const int col=div(x),row=div(y);auto* chunk=findRasterChunk(*chunks,col,row);return chunk?chunk->pixels.pixels[std::size_t(y-row*size)*size+x-col*size]:0;}
 throw std::runtime_error("layer unavailable");
}
int main()try{
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temporary directory unavailable");
 constexpr std::size_t budget=256*1024;Artwork art(dir.path(),budget);
 check(art.replace(fixture())&&art.select("paint")&&art.snapshot().size()>budget/2,"large shared fixture failed");
 const auto old=pixel(art,"paint",1,100,100),outside=pixel(art,"paint",1,110,100);
 check(art.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104}),"frame selection failed");
 BitmapBrush brush;brush.size=4;brush.argb=0xff0000ffU;brush.pressureToOpacityEnabled=false;
 StrokePoint a;a.position={100,100};a.time=1;StrokePoint b=a;b.position={110,100};b.time=2;
 if(!art.begin_input(a,brush,1))throw std::runtime_error("compact frame brush begin failed: "+art.error());
 check(art.extend_input(b),"compact frame brush extend failed");
 auto preview=art.selection_preview();check(preview && !preview->tiles.empty(),"compact frame preview missing");
 FrameBitmapOverlay overlay{preview->asset_id,preview->tiles};auto outsidePreview=renderFrameRegion(art.document(),1,{{110,100},{1,1}},{1,1},RasterSampling::Nearest,std::span(&overlay,1));
 check(outsidePreview.ok()&&outsidePreview.pixels.pixels[0]==outside,"compact frame preview escaped selection");
 check(art.end_input(b),"compact frame brush failed");const auto painted=pixel(art,"paint",1,100,100);
 check(painted!=old&&pixel(art,"paint",1,110,100)==outside&&pixel(art,"paint",0,100,100)==old&&pixel(art,"paint",2,100,100)==old&&pixel(art,"base",1,100,100)==old&&art.history_bytes()<=budget,"compact frame brush changed shared data or exceeded budget");
 const auto stableHistory=art.history_bytes();for(int i=0;i<3;++i)check(art.undo()&&art.redo()&&art.history_bytes()==stableHistory,"frame history accounting drifted");
 check(art.set_selection(SelectionGeometry::Rectangle,{120,120},{124,124})&&art.undo()&&art.tiled_selection_handle()->coverage(100,100)==255,"frame and selection undo order failed");
 check(art.undo()&&pixel(art,"paint",1,100,100)==old&&art.has_selection()&&art.redo()&&pixel(art,"paint",1,100,100)==painted,"compact frame brush history failed");
 auto eraser=brush;eraser.eraser=true;check(art.begin_input(a,eraser,1,StrokeTaper{.25,.25,.25,1})&&art.end_input(b),"compact dynamic eraser failed");
 const auto erased=pixel(art,"paint",1,100,100);check((erased>>24)<(painted>>24)&&pixel(art,"base",1,100,100)==old,"dynamic frame eraser changed shared content");
 check(art.undo()&&pixel(art,"paint",1,100,100)==painted&&art.redo()&&pixel(art,"paint",1,100,100)==erased&&art.undo()&&art.undo(),"compact dynamic eraser history failed");
 const auto revision=art.revision();brush.opacity=0;check(art.begin_input(a,brush,1)&&art.end_input(b)&&art.revision()==revision&&art.can_redo(),"no-op frame brush lost redo");
 check(art.redo()&&pixel(art,"paint",1,100,100)==painted,"frame redo after no-op failed");
 check(art.undo()&&art.can_redo(),"frame cancel setup failed");brush.opacity=1;
 check(art.begin_input(a,brush,1),"frame cancel begin failed");art.cancel();
 check(!art.stroke_active()&&art.can_redo()&&pixel(art,"paint",1,100,100)==old&&art.redo(),"cancelled frame stroke changed pixels or redo");
 const auto path=dir.path()/"frame-history.iisc";check(art.save(path),"frame save failed");Artwork reopened(dir.path(),budget);
 check(reopened.open(path)&&pixel(reopened,"paint",1,100,100)==painted&&pixel(reopened,"paint",0,100,100)==old&&pixel(reopened,"base",1,100,100)==old,"frame save and reopen changed pixels");
 Artwork fill(dir.path(),budget);check(fill.replace(fixture())&&fill.select("paint")&&fill.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104}),"compact frame fill fixture failed");
 FloodOptions options;options.current_layer=true;options.target_current=true;
 check(fill.fill_at({100,100},0xff00ff00U,1,options),"compact frame fill failed");
 check(pixel(fill,"paint",1,100,100)==0xff00ff00U&&pixel(fill,"paint",0,100,100)==old&&pixel(fill,"base",1,100,100)==old,"compact frame fill changed shared content");
 check(fill.undo()&&pixel(fill,"paint",1,100,100)==old&&fill.redo()&&pixel(fill,"paint",1,100,100)==0xff00ff00U,"compact frame fill history failed");
 Artwork single(dir.path(),budget);check(single.replace(fixture(1,true))&&single.select("paint"),"single shared compact fixture failed");
 const auto singleOld=pixel(single,"paint",0,100,100);check(single.begin_input(a,brush,0)&&single.end_input(b),"single shared compact stroke failed");
 const auto singleNew=pixel(single,"paint",0,100,100);check(singleNew!=singleOld&&pixel(single,"base",0,100,100)==singleOld&&single.undo()&&pixel(single,"paint",0,100,100)==singleOld&&single.redo()&&pixel(single,"paint",0,100,100)==singleNew,"single shared compact history failed");
 for(FrameIndex frame:{FrameIndex{0},FrameIndex{2}}){Artwork edge(dir.path(),budget);check(edge.replace(fixture(3,false))&&edge.select("paint"),"edge frame fixture failed");
  const auto edgeOld=pixel(edge,"paint",frame,100,100);check(edge.begin_input(a,brush,frame)&&edge.end_input(b),"edge frame compact stroke failed");
  const auto edgeNew=pixel(edge,"paint",frame,100,100);check(edgeNew!=edgeOld&&pixel(edge,"paint",1,100,100)==edgeOld&&edge.undo()&&pixel(edge,"paint",frame,100,100)==edgeOld&&edge.redo()&&pixel(edge,"paint",frame,100,100)==edgeNew,"edge frame compact history failed");}
 Artwork bounded(dir.path(),budget);check(bounded.replace(fixture())&&bounded.select("paint"),"finite bounded fixture failed");
 const auto originalExtent=bounded.document().extent;StrokePoint ga=a;ga.position={9010,100};StrokePoint gb=ga;gb.position.x=9015;gb.time=2;
 check(bounded.begin_input(ga,brush,1)&&bounded.end_input(gb)&&bounded.document().extent==originalExtent&&pixel(bounded,"paint",1,100,100)==old,"finite out-of-bounds stroke changed canvas");
 Document shifted=fixture();layerProperties(shifted.layers.back()).transform.translationX=40;Artwork transformed(dir.path(),budget);
 check(transformed.replace(std::move(shifted))&&transformed.select("paint")&&transformed.set_selection(SelectionGeometry::Rectangle,{138,98},{144,104}),"transformed compact frame fixture failed");
 StrokePoint ta=a,tb=b;ta.position.x+=40;tb.position.x+=40;const auto transformedOld=pixel(transformed,"paint",1,100,100);
 check(transformed.begin_input(ta,brush,1)&&transformed.end_input(tb),"transformed compact frame stroke failed");const auto transformedNew=pixel(transformed,"paint",1,100,100);
 check(transformedNew!=transformedOld&&pixel(transformed,"base",1,100,100)==transformedOld&&transformed.undo()&&pixel(transformed,"paint",1,100,100)==transformedOld&&transformed.redo()&&pixel(transformed,"paint",1,100,100)==transformedNew,"transformed compact frame history failed");
 const auto working=dir.path()/"frame-working",moved=dir.path()/"frame-moved";std::filesystem::create_directory(working);Artwork failure(working,budget);
 check(failure.replace(fixture())&&failure.select("paint")&&failure.begin_input(a,brush,1)&&failure.end_input(b),"frame persistence fixture failed");
 const auto failurePixel=pixel(failure,"paint",1,100,100);const auto failureRevision=failure.revision();std::filesystem::rename(working,moved);
 check(!failure.undo()&&failure.can_undo()&&failure.revision()==failureRevision&&pixel(failure,"paint",1,100,100)==failurePixel,"failed frame undo changed document or history");
 std::filesystem::rename(moved,working);check(failure.undo()&&failure.redo()&&pixel(failure,"paint",1,100,100)==failurePixel,"frame history failed after storage recovery");
 check(failure.undo()&&failure.can_redo(),"frame failed-commit setup failed");const auto failedCommitRevision=failure.revision();const auto failedCommitPixel=pixel(failure,"paint",1,100,100);
 std::filesystem::rename(working,moved);const bool started=failure.begin_input(a,brush,1);const bool rejected=started && !failure.end_input(b);std::filesystem::rename(moved,working);
 check(rejected&&!failure.stroke_active()&&failure.can_redo()&&failure.revision()==failedCommitRevision&&pixel(failure,"paint",1,100,100)==failedCommitPixel&&failure.redo()&&pixel(failure,"paint",1,100,100)==failurePixel,"failed frame commit changed pixels or redo");
 Document denseDocument=fixture();denseDocument.extent={256,256};Artwork dense(dir.path(),budget);check(dense.replace(std::move(denseDocument))&&dense.select("paint")&&dense.set_selection(SelectionGeometry::Rectangle,{98,98},{104,104})&&dense.selection_mask(),"dense selected chunk fixture failed");const auto denseOld=pixel(dense,"paint",1,100,100);BitmapBrush denseBrush;denseBrush.size=4;denseBrush.argb=0xffbb4400U;denseBrush.pressureToOpacityEnabled=false;if(!dense.begin_input(a,denseBrush,1))throw std::runtime_error("dense compact stroke begin failed: "+dense.error());check(dense.end_input(b),"dense compact stroke failed");const auto denseNew=pixel(dense,"paint",1,100,100);check(denseNew!=denseOld&&pixel(dense,"base",1,100,100)==denseOld&&dense.undo()&&pixel(dense,"paint",1,100,100)==denseOld&&dense.redo()&&pixel(dense,"paint",1,100,100)==denseNew,"dense selection compact history failed");
 check(dense.set_selection(SelectionGeometry::Rectangle,{300,300},{310,310})&&dense.set_selection(SelectionGeometry::Rectangle,{1,1},{3,3})&&dense.undo()&&dense.can_redo(),"dense empty selection redo fixture failed");const auto emptyRevision=dense.revision();check(dense.begin_input(a,denseBrush,1)&&dense.end_input(b)&&dense.revision()==emptyRevision&&dense.can_redo(),"dense empty selection changed document or redo");
 std::cout<<"FINITE_CHUNK_FRAME_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
