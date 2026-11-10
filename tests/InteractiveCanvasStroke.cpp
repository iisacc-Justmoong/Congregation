#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <chrono>
#include <cmath>
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

void long_pen_stroke(const std::filesystem::path& directory,bool tapered,bool selected) {
    Artwork artwork(directory);
    check(artwork.create(768,128),"long stroke canvas creation failed");
    if(selected)
        check(artwork.set_selection(SelectionGeometry::Rectangle,{0,0},{768,128}),
              "long stroke selection failed");
    ToolSettings options;
    options.set(ToolParameter::size,12);
    CanvasView canvas(artwork);
    canvas.brush_size=12;canvas.color=0xff6533ff;canvas.settings=options.brush();
    if(tapered) canvas.taper={options.value(ToolParameter::start)/100.,
                             options.value(ToolParameter::end)/100.,
                             options.value(ToolParameter::minimum_size)/100.,
                             options.value(ToolParameter::taper)/100.};
    std::string status;
    canvas.status=[&](std::string message){status=std::move(message);};
    using K=ii::gui::WidgetInputKind;
    const auto original=pixels(artwork).pixels;
    auto position=[](int i) {
        return std::pair{16.0+2.4*i,64.0+12*std::sin(i/30.0)};
    };
    auto [start_x,start_y]=position(0);
    send(canvas,K::pointer_down,start_x,start_y,100,.5);
    for(int i=1;i<=300;++i) {
        const auto [x,y]=position(i);
        send(canvas,K::pointer_move,x,y,100+i*.04,.4+.5*(i%31)/30.);
    }
    for(int i:{15,60,120,180,240,285}) {
        const auto [x,y]=position(i);
        check(alpha(pixels(artwork),int(x),int(y))>0,"long stroke preview lost input");
    }
    const auto [end_x,end_y]=position(300);
    send(canvas,K::pointer_up,end_x,end_y,112.01,tapered?0:.7);
    if(!status.empty()) throw std::runtime_error("long stroke finish: "+status);
    for(int i:{15,60,120,180,240,285}) {
        const auto [x,y]=position(i);
        check(alpha(pixels(artwork),int(x),int(y))>0,"long stroke commit lost input");
    }
    const auto painted=pixels(artwork).pixels;
    check(artwork.undo() && pixels(artwork).pixels==original && artwork.redo() &&
          pixels(artwork).pixels==painted,"long stroke history is not exact");
    const auto saved=directory/(std::string("long-stroke-")+(tapered?"taper":"plain")+
                                 (selected?"-selected":"")+".iisc");
    check(artwork.save(saved),"long stroke save failed");
    Artwork reopened(directory);
    check(reopened.open(saved) && pixels(reopened).pixels==painted,
          "long stroke did not survive save and reopen");
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
    long_pen_stroke(directory.path(),false,false);
    long_pen_stroke(directory.path(),true,false);
    long_pen_stroke(directory.path(),true,true);
    std::cout << "INTERACTIVE_CANVAS_STROKE_OK\n";
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
