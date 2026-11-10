#include <Native/Artwork.h>
#include <Serialization/IiscCodec.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderDatabase.h>
#include <iiSharedCanvasNative.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>

namespace measured { thread_local bool active=false; thread_local std::size_t bytes=0; }
void* operator new(std::size_t n) {
    if(measured::active)measured::bytes+=n;
    if(auto* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

using namespace congregation::native;
using namespace iiSharedCanvas;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::uint32_t pixel(const Artwork& artwork,int x,int y){
    const auto* asset=findChunkedRasterAsset(artwork.document(),"paint");check(asset,"chunk asset missing");
    const auto* chunk=findRasterChunk(*asset,x/256,y/256);
    return chunk?chunk->pixels.pixels[std::size_t(y%256)*256+x%256]:0;
}
StrokePoint sample(double x,double y,double time){StrokePoint p;p.position={x,y};p.time=time;p.pressure=1;return p;}
}
int main()try{
    iiFileProvider::native::TemporaryDirectory temp(std::filesystem::temp_directory_path());
    check(temp.isValid(),"test directory unavailable");
    Document doc;doc.extent={1024,256};doc.infiniteCanvas.chunkSize=256;
    ChunkedRasterAsset asset{"paint",{}};
    std::uint32_t random=1;
    for(int col=0;col<3;++col){
        auto pixels=makeRasterLayer(256,256);
        for(auto& p:pixels.pixels){random^=random<<13;random^=random>>17;random^=random<<5;p=0xff000000u|(random&0xffffffu);}
        asset.chunks.push_back({col,0,std::move(pixels)});
    }
    doc.assets.emplace_back(std::move(asset));
    doc.layers.emplace_back(StaticBitmapLayer{{"layer","Paint"},StaticSource{"paint"}});
    constexpr std::size_t budget=1024*1024;
    check(encodeIisc(doc).bytes.size()>budget/2,"fixture does not force compact stroke history");
    Artwork artwork(temp.path(),budget);
    check(artwork.replace(std::move(doc)),"compact document setup failed");
    BitmapBrush brush;brush.size=6;brush.argb=0xff6533ff;brush.pressureToOpacityEnabled=false;
    const auto original=pixel(artwork,16,32);
    const auto original_document=artwork.snapshot();
    check(artwork.begin_input(sample(16,32,1),brush),"compact stroke begin failed");
    auto move=[&](double x,double y,double time){
        measured::bytes=0;measured::active=true;
        const auto start=std::chrono::steady_clock::now();
        const bool ok=artwork.extend_input(sample(x,y,time));
        const auto micros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        measured::active=false;check(ok,"compact stroke move failed");return std::pair{measured::bytes,micros};
    };
    auto early=move(18,32,1.01);double time=1.01;
    for(int i=1;i<=15;++i)check(artwork.extend_input(sample(16+i*64,32,time+=.01)),"upper chunk path");
    check(artwork.extend_input(sample(976,96,time+=.01)),"chunk turn");
    for(int i=14;i>=0;--i)check(artwork.extend_input(sample(16+i*64,96,time+=.01)),"lower chunk path");
    auto late=move(18,96,time+=.01);
    std::cout<<"COMPACT_LIVE_STROKE early_bytes="<<early.first<<" late_bytes="<<late.first
             <<" early_us="<<early.second<<" late_us="<<late.second<<std::endl;
    check(late.first<=early.first*3+65536,"compact per-input work grows with prior stroke length");
    check(artwork.end_input(sample(20,96,time+=.01)),"compact stroke finish failed");
    const auto painted=pixel(artwork,16,32);check(painted!=original,"compact path did not paint");
    const auto painted_document=artwork.snapshot();
    check(artwork.undo()&&pixel(artwork,16,32)==original&&artwork.snapshot()==original_document,
          "compact stroke undo did not restore the exact document");
    check(artwork.redo()&&pixel(artwork,16,32)==painted&&artwork.snapshot()==painted_document,
          "compact stroke redo did not restore the exact document");

    // A document that fits the history budget must use the same tile path.
    Document larger;larger.extent={1024,1024};larger.infiniteCanvas.chunkSize=256;
    ChunkedRasterAsset larger_asset{"paint",{}};
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){
        auto pixels=makeRasterLayer(256,256);
        for(auto& p:pixels.pixels){random^=random<<13;random^=random>>17;random^=random<<5;p=0xff000000u|(random&0xffffffu);}
        larger_asset.chunks.push_back({col,row,std::move(pixels)});
    }
    larger.assets.emplace_back(std::move(larger_asset));
    larger.layers.emplace_back(StaticBitmapLayer{{"layer","Paint"},StaticSource{"paint"}});
    Artwork roomy(temp.path(),128*1024*1024);
    check(roomy.replace(std::move(larger)),"roomy chunk document setup failed");
    const auto roomy_before=roomy.snapshot();
    measured::bytes=0;measured::active=true;
    const bool began=roomy.begin_input(sample(16,32,1),brush);
    measured::active=false;const auto begin_bytes=measured::bytes;
    check(began && roomy.extend_input(sample(24,32,1.01)),"roomy chunk stroke begin failed");
    measured::bytes=0;measured::active=true;
    const bool ended=roomy.end_input(sample(32,32,1.02));
    measured::active=false;const auto end_bytes=measured::bytes;
    check(ended,"roomy chunk stroke end failed");
    const auto roomy_after=roomy.snapshot();
    measured::bytes=0;measured::active=true;const bool undone=roomy.undo();
    measured::active=false;const auto undo_bytes=measured::bytes;
    check(undone && roomy.snapshot()==roomy_before,"roomy chunk undo was not exact");
    measured::bytes=0;measured::active=true;const bool redone=roomy.redo();
    measured::active=false;const auto redo_bytes=measured::bytes;
    check(redone && roomy.snapshot()==roomy_after,"roomy chunk redo was not exact");
    std::cout<<"ROOMY_CHUNK_HISTORY begin_bytes="<<begin_bytes<<" end_bytes="<<end_bytes
             <<" undo_bytes="<<undo_bytes<<" redo_bytes="<<redo_bytes<<std::endl;
    constexpr auto max_bytes=std::size_t(2)*1024*1024;
    check(begin_bytes<max_bytes && end_bytes<max_bytes && undo_bytes<max_bytes && redo_bytes<max_bytes,
          "roomy chunk stroke or history copied the complete document");
    namespace sql=iiFileProvider::native::sql;
    auto database=sql::Database::open(roomy.recovery());
    check(bool(database),"cannot open chunk recovery database");
    {
        auto lock=sql::Transaction::begin(*database,true);
        check(bool(lock),"cannot lock chunk undo");
        const auto revision=roomy.revision();
        check(!roomy.undo() && roomy.snapshot()==roomy_after && roomy.revision()==revision && roomy.can_undo(),
              "failed chunk undo changed the document or stack");
        check(bool(lock->rollback()),"cannot unlock chunk undo");
    }
    check(roomy.undo() && roomy.snapshot()==roomy_before,"chunk undo retry failed");
    auto invisible=brush;invisible.opacity=0;
    const auto no_op_revision=roomy.revision();
    check(roomy.begin_input(sample(16,32,2),invisible) && roomy.end_input(sample(24,32,2.01)) &&
          roomy.revision()==no_op_revision && roomy.snapshot()==roomy_before && roomy.can_redo(),
          "no-op chunk stroke changed the document or redo stack");
    check(roomy.begin_input(sample(16,32,3),brush) && roomy.extend_input(sample(24,32,3.01)),
          "chunk cancellation setup failed");
    roomy.cancel();
    check(roomy.revision()==no_op_revision && roomy.snapshot()==roomy_before && roomy.can_redo(),
          "cancelled chunk stroke changed the document or redo stack");
    {
        auto lock=sql::Transaction::begin(*database,true);
        check(bool(lock),"cannot lock chunk redo");
        const auto revision=roomy.revision();
        check(!roomy.redo() && roomy.snapshot()==roomy_before && roomy.revision()==revision && roomy.can_redo(),
              "failed chunk redo changed the document or stack");
        check(bool(lock->rollback()),"cannot unlock chunk redo");
    }
    check(roomy.redo() && roomy.snapshot()==roomy_after,"chunk redo retry failed");
    check(roomy.set_selection(SelectionGeometry::Rectangle,{20,48},{80,112}),
          "roomy chunk selection setup failed");
    for(int variant=0;variant<2;++variant){
        const int y=64+variant*32;
        const auto before=roomy.snapshot();const auto outside=pixel(roomy,12,y);
        const auto inside=pixel(roomy,32,y);
        const StrokeTaper taper=variant?StrokeTaper{.25,.25,.25,1}:StrokeTaper{};
        measured::bytes=0;measured::active=true;
        const bool began=roomy.begin_input(sample(12,y,4),brush,0,taper);
        measured::active=false;const auto variant_begin=measured::bytes;
        check(began && roomy.extend_input(sample(32,y,4.01)),"selected chunk begin failed");
        measured::bytes=0;measured::active=true;
        const bool ended=roomy.end_input(sample(48,y,4.02));
        measured::active=false;const auto variant_end=measured::bytes;
        check(ended && pixel(roomy,12,y)==outside && pixel(roomy,32,y)!=inside,
              "selected chunk stroke escaped selection or failed to paint");
        const auto after=roomy.snapshot();
        measured::bytes=0;measured::active=true;const bool undone=roomy.undo();
        measured::active=false;const auto variant_undo=measured::bytes;
        check(undone && roomy.snapshot()==before,"selected chunk undo was not exact");
        measured::bytes=0;measured::active=true;const bool redone=roomy.redo();
        measured::active=false;const auto variant_redo=measured::bytes;
        check(redone && roomy.snapshot()==after,"selected chunk redo was not exact");
        std::cout<<"SELECTED_CHUNK_HISTORY tapered="<<variant<<" begin_bytes="<<variant_begin
                 <<" end_bytes="<<variant_end<<" undo_bytes="<<variant_undo
                 <<" redo_bytes="<<variant_redo<<std::endl;
        check(variant_begin<max_bytes && variant_end<max_bytes && variant_undo<max_bytes && variant_redo<max_bytes,
              "selected or tapered chunk history copied the complete document");
    }
    check(roomy.undo() && roomy.can_redo(),"selected chunk no-op setup failed");
    const auto selected_no_op=roomy.snapshot();const auto selected_no_op_revision=roomy.revision();
    check(roomy.begin_input(sample(180,180,5),brush) && roomy.end_input(sample(184,180,5.01)) &&
          roomy.snapshot()==selected_no_op && roomy.revision()==selected_no_op_revision && roomy.can_redo(),
          "outside-selection chunk stroke changed the document or redo stack");
    check(roomy.begin_input(sample(32,64,6),invisible,0,StrokeTaper{.25,.25,.25,1}) &&
          roomy.end_input(sample(40,64,6.01)) && roomy.snapshot()==selected_no_op &&
          roomy.revision()==selected_no_op_revision && roomy.can_redo(),
          "transparent selected taper changed the document or redo stack");
    check(roomy.redo(),"redo after selected chunk no-ops failed");
    Document diagonal;diagonal.extent={2304,2304};diagonal.infiniteCanvas.chunkSize=256;
    diagonal.assets.emplace_back(ChunkedRasterAsset{"paint",{}});
    diagonal.layers.emplace_back(StaticBitmapLayer{{"layer","Paint"},StaticSource{"paint"}});
    Artwork sparse_diagonal(temp.path(),16*1024*1024);
    check(sparse_diagonal.replace(std::move(diagonal)) &&
          sparse_diagonal.set_selection(SelectionGeometry::Rectangle,{0,0},{2304,2304}),
          "diagonal chunk selection setup failed");
    const auto diagonal_before=sparse_diagonal.snapshot();
    auto fine=brush;fine.size=2;fine.spacingRatio=.5;
    check(sparse_diagonal.begin_input(sample(16,16,1),fine),"diagonal chunk begin failed");
    for(int i=1;i<36;++i)
        check(sparse_diagonal.extend_input(sample(16+i*64,16+i*64,1+i*.01)),"diagonal chunk input failed");
    check(sparse_diagonal.end_input(sample(2256,2256,1.37)),"sparse diagonal exceeded its tile budget");
    const auto diagonal_after=sparse_diagonal.snapshot();
    check(diagonal_after!=diagonal_before && sparse_diagonal.undo() &&
          sparse_diagonal.snapshot()==diagonal_before && sparse_diagonal.redo() &&
          sparse_diagonal.snapshot()==diagonal_after,"sparse diagonal history was not exact");
    const auto chunk_fixture=[] {
        Document value;value.extent={512,64};value.infiniteCanvas.chunkSize=256;
        value.assets.emplace_back(ChunkedRasterAsset{"paint",{}});
        value.layers.emplace_back(StaticBitmapLayer{{"layer","Paint"},StaticSource{"paint"}});
        return value;
    };
    Artwork feathered(temp.path()),unmasked(temp.path());
    check(feathered.replace(chunk_fixture()) && unmasked.replace(chunk_fixture()) &&
          feathered.set_selection(SelectionGeometry::Rectangle,{256,0},{300,64},
                                  MaskOperation::Replace,SelectionOptions{true,4}),
          "feathered chunk selection setup failed");
    const auto* feather_mask=feathered.selection_mask();
    check(feather_mask && feather_mask->alpha[std::size_t(32)*512+256]>0 &&
          feather_mask->alpha[std::size_t(32)*512+256]<255,
          "feathered chunk selection has no partial coverage at its boundary");
    const auto feather_before=feathered.snapshot();
    auto boundary_brush=brush;boundary_brush.size=16;
    const auto paint_boundary=[&](Artwork& value){
        return value.begin_input(sample(242,32,1),boundary_brush) &&
               value.extend_input(sample(256,32,1.01)) && value.end_input(sample(274,32,1.02));
    };
    check(paint_boundary(unmasked) && paint_boundary(feathered),"feathered chunk stroke failed");
    auto reference=makeRasterLayer(512,64,0);
    for(int y=0;y<64;++y)for(int x=0;x<512;++x)
        reference.pixels[std::size_t(y)*512+x]=pixel(unmasked,x,y);
    const auto expected=blendRaster(makeRasterLayer(512,64,0),reference,*feather_mask);
    check(expected.ok(),"feathered chunk reference failed");
    for(int y=0;y<64;++y)for(int x=0;x<512;++x)
        check(pixel(feathered,x,y)==expected.pixels.pixels[std::size_t(y)*512+x],
              "feathered chunk boundary differs from whole-image compositing");
    const auto feather_after=feathered.snapshot();
    check(feathered.undo() && feathered.snapshot()==feather_before && feathered.redo() &&
          feathered.snapshot()==feather_after,"feathered chunk history was not exact");
    Artwork failed_chunk(temp.path());
    check(failed_chunk.replace(chunk_fixture()),"chunk rollback fixture failed");
    const auto failed_before=failed_chunk.snapshot();
    check(failed_chunk.begin_input(sample(16,32,1),brush),"chunk rollback begin failed");
    (void)failed_chunk.take_stroke_damage();
    check(failed_chunk.extend_input(sample(128,32,1.1)),"chunk rollback first move failed");
    (void)failed_chunk.take_stroke_damage();
    check(failed_chunk.extend_input(sample(384,32,1.2)),"chunk rollback second move failed");
    (void)failed_chunk.take_stroke_damage();
    auto failed_database=sql::Database::open(failed_chunk.recovery());
    check(bool(failed_database),"cannot open chunk rollback database");
    auto failed_lock=sql::Transaction::begin(*failed_database,true);
    check(bool(failed_lock),"cannot lock chunk rollback database");
    check(!failed_chunk.end_input(sample(480,32,1.3)),"locked chunk stroke unexpectedly committed");
    check(bool(failed_lock->rollback()),"cannot unlock chunk rollback database");
    const auto failed_damage=failed_chunk.take_stroke_damage();
    check(failed_damage && failed_damage->left<=16 && failed_damage->right>=480 &&
          failed_chunk.snapshot()==failed_before,
          "failed chunk publication left earlier preview damage unreported");

    Artwork broad_chunk(temp.path());
    check(broad_chunk.replace(chunk_fixture()),"broad chunk fixture failed");
    const auto broad_before=broad_chunk.snapshot();
    auto broad_brush=brush;broad_brush.size=256;broad_brush.spacingRatio=.12;
    const StrokeTaper broad_taper{.25,.25,.25,1};
    check(broad_chunk.begin_input(sample(16,32,0),broad_brush,0,broad_taper),
          "broad chunk stroke begin failed");
    int broad_index=0;
    for(int pass=0;pass<36;++pass) for(int step=1;step<=8;++step) {
        const double x=pass%2 ? 496.-60.*step : 16.+60.*step;
        check(broad_chunk.extend_input(sample(x,32,++broad_index*.1)),"broad chunk stroke move failed");
    }
    if(!broad_chunk.end_input(sample(16,32,++broad_index*.1)))
        throw std::runtime_error("broad chunk stroke finish: "+broad_chunk.error());
    const auto broad_after=broad_chunk.snapshot();
    check(pixel(broad_chunk,256,32)!=0 && broad_chunk.undo() && broad_chunk.snapshot()==broad_before &&
          broad_chunk.redo() && broad_chunk.snapshot()==broad_after,
          "broad chunk stroke or its history was lost");
    std::cout<<"INTERACTIVE_CHUNK_STROKE_OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
