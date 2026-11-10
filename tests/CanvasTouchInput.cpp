#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <Native/ToolSettings.h>
#include <iiFileProviderNative.h>
#include "../../../SDK/iiGUI/src/Widgets/WidgetState.h"
#include "TouchInputBridge.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <stdexcept>

using namespace congregation::native;
using namespace ii::runtime::touch;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
TouchSample sample(std::uint64_t id,double x,double y){TouchSample value;value.identity=id;value.x=x;value.y=y;value.timestamp=1;return value;}
void deliver(CanvasView& canvas,const std::optional<TouchBatch>& batch){
    check(bool(batch),"runtime rejected a touch frame");
    for(std::size_t i=0;i<batch->size;++i){
        auto input=ii::gui::detail::widget_touch_event((*batch)[i]);check(bool(input),"touch bridge rejected an event");
        check(bool(canvas.input(*input)),"canvas rejected an input result");
    }
}
}
int main(){
    iiFileProvider::native::TemporaryDirectory temporary(std::filesystem::temp_directory_path());
    check(temporary.isValid(),"temporary directory unavailable");
    Artwork art(temporary.path());check(art.create(160,90),"canvas creation failed");
    CanvasView canvas(art);ToolSettings options;canvas.settings=options.brush();canvas.brush_size=6;
    canvas.resize({0,0,640,480});canvas.fit();
    std::string status;canvas.status=[&](std::string value){status=std::move(value);};
    const auto original=art.snapshot();const auto start=canvas.viewport();
    TouchRouter router;const auto a=sample(1,300,220),b=sample(2,340,220);
    deliver(canvas,router.down(a,false));if(!art.stroke_active())throw std::runtime_error("initial touch did not start a stroke: "+status);
    deliver(canvas,router.down(b,false));
    check(!art.stroke_active()&&art.snapshot()==original,"two-finger navigation did not cancel the tentative stroke");
    deliver(canvas,router.move(sample(1,315,230),sample(2,355,230)));
    check(std::abs(canvas.viewport().x-start.x-15)<1e-6&&std::abs(canvas.viewport().y-start.y-10)<1e-6,
          "canvas did not follow the two fingers");
    const auto before_zoom=canvas.viewport().scale;
    deliver(canvas,router.move(sample(1,305,230),sample(2,365,230)));
    check(std::abs(canvas.viewport().scale-before_zoom*1.5)<1e-6,"pinch-out did not match the fingers' scale");
    deliver(canvas,router.up(sample(2,365,230)));deliver(canvas,router.up(sample(1,305,230)));
    check(art.snapshot()==original,"navigation changed document pixels");

    auto pen=sample(3,320,240);pen.pen=true;
    deliver(canvas,router.down(pen,false));check(art.stroke_active(),"pen did not start a stroke");
    const auto palm=sample(4,400,250);deliver(canvas,router.down(palm,false));
    pen.x=340;pen.y=245;deliver(canvas,router.move(pen,palm));
    check(art.stroke_active(),"finger interrupted an active pen stroke");
    deliver(canvas,router.up(pen));deliver(canvas,router.up(palm));
    check(!art.stroke_active()&&art.snapshot()!=original,"pen stroke was not committed after palm contact");

    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=tree->content_host({},"host",{640,480});check(bool(host),"canvas input host failed");
    check(bool(host->set_content([&](ii::gui::WidgetBuilder& b)->ii::core::Result<void>{
        auto root=b.absolute_stack({},"canvas",{640,480});if(!root)return std::unexpected(root.error());
        return root->set_input_handler([&](const ii::gui::WidgetInput& input){return canvas.input(input);});
    })),"canvas input mount failed");
    check(bool(tree->drawing_snapshot({640,480})),"canvas input layout failed");
    auto a2=sample(5,300,220),b2=sample(6,340,220);a2.timestamp=2.;b2.timestamp=2.001;
    const auto before_navigation=art.snapshot();TouchRouter gesture;
    const auto send=[&](const std::optional<TouchBatch>& batch){check(bool(batch),"momentum touch rejected");
        for(std::size_t i=0;i<batch->size;++i){auto input=ii::gui::detail::widget_touch_event((*batch)[i]);check(bool(input)&&bool(tree->input(*input)),"momentum event rejected");}};
    check(bool(tree->touch_begin(a2.timestamp)),"first contact failed");send(gesture.down(a2,false));
    check(bool(tree->touch_begin(b2.timestamp)),"second contact failed");send(gesture.down(b2,false));
    a2.x+=12;b2.x+=12;a2.timestamp=b2.timestamp=2.02;send(gesture.move(a2,b2));
    a2.x+=12;b2.x+=12;a2.timestamp=b2.timestamp=2.04;send(gesture.move(a2,b2));
    const auto release=canvas.viewport();send(gesture.up(b2));send(gesture.up(a2));
    check(bool(tree->touch_end(2.04)),"momentum release failed");
    (void)frames.step(ii::runtime::Clock::now()+std::chrono::milliseconds(50));
    check(canvas.viewport().x>release.x&&art.snapshot()==before_navigation,"canvas momentum lost movement or changed pixels");
    tree->close();
}
