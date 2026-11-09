#include <Native/Artwork.h>
#include <Serialization/IiscCodec.h>
#include <iiFileProviderNative.h>
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
    check(artwork.undo()&&pixel(artwork,16,32)==original&&artwork.redo()&&pixel(artwork,16,32)==painted,
          "compact stroke undo/redo failed");
    std::cout<<"INTERACTIVE_CHUNK_STROKE_OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
