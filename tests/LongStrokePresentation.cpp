#include <Native/Artwork.h>
#include <Native/CanvasDefinition.h>
#include <Native/CanvasView.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderDatabase.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace congregation::native;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if(!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if(!result) throw std::runtime_error(result.error().message);
}
std::optional<std::array<std::uint8_t,4>> displayed_pixel(const ii::gui::CanvasFrame& frame,double x,double y) {
    std::optional<std::array<std::uint8_t,4>> pixel;
    const auto inspect=[&](const auto& images) {
        for(const auto& run:images) {
            if(!run.image || x<run.bounds.x || y<run.bounds.y ||
               x>=run.bounds.x+run.bounds.width || y>=run.bounds.y+run.bounds.height) continue;
            const auto px=int((x-run.bounds.x)*run.image->width()/run.bounds.width);
            const auto py=int((y-run.bounds.y)*run.image->height()/run.bounds.height);
            const auto data=run.image->rgba();
            const auto index=(std::size_t(py)*run.image->width()+px)*4;
            if(index+3<data.size()) pixel=std::array{data[index],data[index+1],data[index+2],data[index+3]};
        }
    };
    inspect(frame.images);
    for(const auto& layer:frame.overlays) inspect(layer.images);
    return pixel;
}
}

int main() try {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(),"temporary directory unavailable");
    Artwork artwork(directory.path());
    CanvasDefinition definition;
    definition.width=definition.pixel_width=768;
    definition.height=definition.pixel_height=128;
    definition.background="White";
    check(artwork.create(definition),"canvas creation failed");
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames(events);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto tree=std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto root=take(tree->absolute_stack({},"root",{1600,600}));
    auto surface=take(tree->absolute_stack(root,"canvas",{1600,600}));
    good(surface.set_frame({0,0,1600,600}));
    auto paint=take(tree->content_host(surface,"paint",{1600,600}));
    good(paint.set_frame({0,0,1600,600}));
    CanvasView canvas(artwork);
    ToolSettings settings;
    settings.set(ToolParameter::size,12);
    canvas.brush_size=12;canvas.color=0xff6533ff;canvas.settings=settings.brush();
    canvas.taper={settings.value(ToolParameter::start)/100.,settings.value(ToolParameter::end)/100.,
                  settings.value(ToolParameter::minimum_size)/100.,settings.value(ToolParameter::taper)/100.};
    std::string status;
    canvas.status=[&](std::string message){status=std::move(message);};
    canvas.attach(surface,paint);
    // Fit may enlarge a small canvas. Exercise this multi-tile test at its
    // intended 1:1 scale regardless of the initial fit policy.
    canvas.zoom(1.0/canvas.viewport().scale);
    const auto viewport=canvas.viewport();
    check(std::abs(viewport.scale-1.0)<1e-9 && viewport.visible_tiles>=2,
          "fixture requires multiple visible tiles at 1:1 scale");
    const auto now=ii::runtime::Clock::now();
    auto position=[](int i){return std::pair{16.0+.6*i,64.0+12*std::sin(i/120.0)};};
    const auto baseline=take(tree->drawing_snapshot({1600,600}));
    for(int i:{120,360,600,840,1080}) {
        const auto [x,y]=position(i);
        check(bool(displayed_pixel(baseline,viewport.x+x,viewport.y+y)),
              "baseline native scene is missing a canvas tile");
    }
    auto input=[&](ii::gui::WidgetInputKind kind,int i) {
        const auto [x,y]=position(i);
        ii::gui::WidgetInput event;
        event.kind=kind;event.x=viewport.x+x;event.y=viewport.y+y;
        event.timestamp=100+i*.01;event.device=ii::gui::PointerDevice::pen;
        event.pressure=kind==ii::gui::WidgetInputKind::pointer_up?0:.4+.5*(i%31)/30.;
        event.buttons=kind==ii::gui::WidgetInputKind::pointer_up?0:1;
        check(take(canvas.input(event)),"canvas rejected pen input");
    };
    using K=ii::gui::WidgetInputKind;
    input(K::pointer_down,0);
    for(int i=1;i<=1200;++i) {
        input(K::pointer_move,i);
        if(i%60==0) frames.step(now+std::chrono::milliseconds(i*10));
    }
    frames.step(now+std::chrono::milliseconds(12001));
    auto live=take(tree->drawing_snapshot({1600,600}));
    for(int i:{120,360,600,840,1080}) {
        const auto [x,y]=position(i);
        check(displayed_pixel(live,viewport.x+x,viewport.y+y)!=displayed_pixel(baseline,viewport.x+x,viewport.y+y),
              "live native scene lost part of long stroke");
    }
    input(K::pointer_up,1200);
    if(!status.empty()) throw std::runtime_error("long stroke finish: "+status);
    frames.step(now+std::chrono::milliseconds(12020));
    auto committed=take(tree->drawing_snapshot({1600,600}));
    for (int y=0; y<128; ++y) for (int x=0; x<768; ++x)
        if (displayed_pixel(live,viewport.x+x,viewport.y+y) !=
            displayed_pixel(committed,viewport.x+x,viewport.y+y))
            throw std::runtime_error("native scene changed already displayed pixels on release");
    for(int i:{120,360,600,840,1080}) {
        const auto [x,y]=position(i);
        check(displayed_pixel(committed,viewport.x+x,viewport.y+y)!=displayed_pixel(baseline,viewport.x+x,viewport.y+y),
              "native scene lost part of the stroke on release");
    }

    // When durable publication fails, no retained preview tile may remain
    // visible after the in-memory document has been rolled back.
    check(artwork.create(definition),"rollback canvas creation failed");
    canvas.taper={};
    canvas.refresh();status.clear();
    input(K::pointer_down,0);
    for(int i=6;i<=1200;i+=6) {
        input(K::pointer_move,i);
        if(i%60==0) frames.step(now+std::chrono::milliseconds(13000+i*10));
    }
    frames.step(now+std::chrono::milliseconds(25001));
    const auto pending=take(tree->drawing_snapshot({1600,600}));
    const auto [first_x,first_y]=position(120);
    check(displayed_pixel(pending,viewport.x+first_x,viewport.y+first_y)!=
          displayed_pixel(baseline,viewport.x+first_x,viewport.y+first_y),
          "rollback fixture did not present the first tile");
    namespace sql=iiFileProvider::native::sql;
    auto database=sql::Database::open(artwork.recovery());
    check(bool(database),"cannot open rollback database");
    auto lock=sql::Transaction::begin(*database,true);
    check(bool(lock),"cannot lock rollback database");
    input(K::pointer_up,1200);
    check(!status.empty(),"failed publication was not reported");
    check(bool(lock->rollback()),"cannot unlock rollback database");
    frames.step(now+std::chrono::milliseconds(25020));
    const auto rolled_back=take(tree->drawing_snapshot({1600,600}));
    for(int i:{120,360,600,840,1080}) {
        const auto [x,y]=position(i);
        check(displayed_pixel(rolled_back,viewport.x+x,viewport.y+y)==
              displayed_pixel(baseline,viewport.x+x,viewport.y+y),
              "failed publication left stale stroke pixels on screen");
    }
    std::cout<<"LONG_STROKE_PRESENTATION_OK\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
