#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>

namespace measured { thread_local bool active=false; thread_local std::size_t bytes=0; }
void* operator new(std::size_t n) {
    if (measured::active) measured::bytes+=n;
    if (auto* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace congregation::native;
using namespace iiSharedCanvas;
namespace {
void check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
const RasterLayer& pixels(const Artwork& artwork) {
    for(const auto& layer:artwork.document().layers) {
        if(layerProperties(layer).id!=artwork.selected())continue;
        const auto* source=staticLayerSource(layer);
        const auto* asset=source?findRasterAsset(artwork.document(),source->assetId):nullptr;
        check(asset,"paint bitmap unavailable");return asset->pixels;
    }
    throw std::runtime_error("paint layer unavailable");
}
unsigned alpha(const RasterLayer& image,int x,int y) {
    unsigned value=0;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx) {
        const int px=std::clamp(x+dx,0,image.width-1),py=std::clamp(y+dy,0,image.height-1);
        value=std::max(value,image.pixels[std::size_t(py)*image.width+px]>>24);
    }
    return value;
}
void send(CanvasView& canvas,ii::gui::WidgetInputKind kind,double x,double y,double time,double pressure) {
    ii::gui::WidgetInput event;
    event.kind=kind;event.x=x;event.y=y;event.timestamp=time;
    event.device=ii::gui::PointerDevice::pen;event.pressure=pressure;
    event.buttons=kind==ii::gui::WidgetInputKind::pointer_up?0:1;
    auto accepted=canvas.input(event);
    check(accepted&&*accepted,"canvas rejected input");
}
}
int main() try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(),"temporary directory unavailable");
    Artwork artwork(directory.path());
    check(artwork.create(1024,192),"canvas creation failed");
    ToolSettings options;
    options.set(ToolParameter::size,6);
    options.set(ToolParameter::stabilization,0);
    options.set(ToolParameter::post,0);
    CanvasView canvas(artwork);
    canvas.brush_size=6;canvas.color=0xff6533ff;canvas.settings=options.brush();
    using K=ii::gui::WidgetInputKind;
    const auto before=pixels(artwork).pixels;
    send(canvas,K::pointer_down,16,32,1,.25);
    auto measure=[&](double x,double y,double time,double pressure) {
        measured::bytes=0;measured::active=true;
        const auto start=std::chrono::steady_clock::now();
        send(canvas,K::pointer_move,x,y,time,pressure);
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        measured::active=false;
        return std::pair{measured::bytes,elapsed};
    };
    const auto early=measure(18,32,1.01,.25);
    double time=1.01;
    for(int i=1;i<=15;++i)send(canvas,K::pointer_move,16+i*64,32,time+=.01,1);
    send(canvas,K::pointer_move,976,96,time+=.01,1);
    for(int i=14;i>=0;--i)send(canvas,K::pointer_move,16+i*64,96,time+=.01,1);
    const auto late=measure(18,96,time+=.01,1);
    std::cout << "CANVAS_LIVE_STROKE early_bytes=" << early.first << " late_bytes=" << late.first
              << " early_us=" << early.second << " late_us=" << late.second << std::endl;
    check(late.first<=early.first*3+65536,"per-input work grows with prior stroke length");
    check(alpha(pixels(artwork),512,32)>0 && alpha(pixels(artwork),976,64)>0 &&
          alpha(pixels(artwork),512,96)>0 && alpha(pixels(artwork),512,64)==0,
          "live stroke lost its curved path or painted off the path");
    check(alpha(pixels(artwork),18,96)>alpha(pixels(artwork),16,32),"pressure response was lost");
    send(canvas,K::pointer_up,20,96,time+=.01,1);
    const auto painted=pixels(artwork).pixels;
    check(artwork.undo() && pixels(artwork).pixels==before &&
          artwork.redo() && pixels(artwork).pixels==painted,"stroke undo/redo is not exact");
    std::cout << "INTERACTIVE_CANVAS_STROKE_OK\n";
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
