#include <Native/WorkspaceView.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/NativeEventListenerView.h>
#include <LVUI/NativeValues.h>
#include <iiSDKCore/Runtime/EventLoop.h>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>
#include <iiSharedCanvasNative.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
    return std::move(*value);
}
void good(ii::core::Result<void> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Workspace control did not mount: "+std::string(suffix));
    return take(tree->find(found->id));
}
void input_loop_trace(){
    using namespace ii::gui;
    struct Model final { ii::core::Observable<int> x{0}, y{0}; int events=0; };
    auto model=std::make_shared<Model>();
    auto graph=std::make_shared<lvui::native::Bindings>();
    std::vector<int> shown;
    take(graph->bind([&](lvui::native::ReadContext& context){return context.read(model->x)+context.read(model->y);},
                     [&](int value)->ii::core::Result<void>{shown.push_back(value);return {};}));
    auto input=lvui::native::scoped_function<ii::core::Result<bool>>(model,graph,
        [](const auto& scope,const WidgetInput& event)->ii::core::Result<bool>{
            if(event.kind==WidgetInputKind::pointer_down||event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_up){
                const auto n=++scope->events;scope->x.set(n);scope->y.set(n);
            }
            return true;
        });
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"trace-host",{640,480}));
    std::optional<ii::gui::NativeEventListener> listener;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeEventListener::create(builder,{},"trace-listener",{320,180},
            {.input=[input](const WidgetInput& event) mutable {return input(event);}});
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));
    const auto bounds=take(listener->root().snapshot()).bounds;
    ii::runtime::EventLoop loop(128);
    constexpr int moves=64,total=moves+2;
    for(int index=0;index<total;++index){
        WidgetInput event{.kind=index==0?WidgetInputKind::pointer_down:index==total-1?WidgetInputKind::pointer_up:WidgetInputKind::pointer_move,
                          .x=bounds.x+5+index,.y=bounds.y+5,.device=PointerDevice::pen,
                          .pressure=.5,.timestamp=double(index),.buttons=index==total-1?0u:1u};
        require(bool(loop.post([tree,event]{good(tree->input(event));})),"Trace input admission failed");
    }
    while(loop.stats().pending)loop.poll({.max_callbacks=128,.max_time=std::chrono::seconds(5)});
    const auto stats=loop.stats();
    std::cout<<"INPUT_LOOP_TRACE submitted="<<stats.submitted<<" executed="<<stats.executed
             <<" input="<<model->events<<" effects="<<shown.size()-1<<" final="<<shown.back()<<'\n';
    require(stats.submitted==total&&stats.executed==total&&model->events==total&&
            shown.size()==std::size_t(total+1)&&shown.back()==2*total,
            "Repeated input did not produce one coherent binding update per loop task");
    good(host.clear_content());tree->close();
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::runtime_error("Expected the Figma resource directory");
    input_loop_trace();
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{1999,1080}));
    auto model=std::make_shared<congregation::ui::EditorBackend>();model->licensed.set(true);
    auto view=std::make_shared<congregation::native::WorkspaceView>(model,argv[1]);
    congregation::ui::Action action=congregation::ui::Action::quit;
    model->perform=[&](auto value){action=value;};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{view->build(builder,{});return builder.retain(view);}));
    auto frame=take(tree->drawing_snapshot({1999,1080}));
    bool has_icons=!frame.images.empty();
    for(const auto& layer:frame.overlays)has_icons|=!layer.images.empty();
    require(has_icons,"Workspace did not draw its native icons");
    require(take(widget(tree,"/timeline").snapshot()).bounds.height==296,
            "Timeline did not preserve the Figma content height");
    iiSharedCanvas::Document document;
    document.extent={8,8};document.timeline.frameCount=48;
    document.assets.emplace_back(iiSharedCanvas::RasterAsset{"asset-test",makeRasterLayer(8,8)});
    iiSharedCanvas::DynamicBitmapLayer layer;
    layer.properties.id="layer-test"; layer.properties.name="Timeline test layer";
    layer.content.frameIndices={0,7};
    document.layers.emplace_back(std::move(layer));
    document.frames.push_back(iiSharedCanvas::Frame{0,{{"layer-test","asset-test"}}});
    document.frames.push_back(iiSharedCanvas::Frame{7,{{"layer-test","asset-test"}}});
    view->refresh_document(document,"layer-test");
    // Unimplemented timeline controls still accept input and provide button
    // feedback without changing the document or dispatching an action.
    const auto idle_frame = model->timeline->state.get().frame;
    const auto idle_timeline_revision = model->timeline->state.revision();
    const auto idle_layer_revision = model->layer_revision.get();
    const auto idle_action = action;
    for (const char* id : {"/layer-lock", "/layer-groups", "/layer-mask", "/clipping-mask",
                           "/link-layers", "/timeline-snap", "/onion-skin",
                           "/timeline-add-keyframe", "/timeline-remove-keyframe",
                           "/add-motion-keyframe", "/remove-motion-keyframe"}) {
        auto button = widget(tree, id);
        const auto state = take(button.snapshot());
        require(state.enabled, "Placeholder button must remain interactive");
        const auto x = state.bounds.x + state.bounds.width / 2;
        const auto y = state.bounds.y + state.bounds.height / 2;
        for (int repeat = 0; repeat < 3; ++repeat) {
            good(tree->input({.kind=WidgetInputKind::pointer_down, .x=x, .y=y, .buttons=1}));
            require(take(button.snapshot()).pressed, "Placeholder button did not receive the press");
            good(tree->input({.kind=WidgetInputKind::pointer_up, .x=x, .y=y}));
            require(!take(button.snapshot()).pressed, "Placeholder button remained pressed after release");
        }
        good(button.activate());
    }
    require(model->timeline->state.get().frame == idle_frame &&
            model->timeline->state.revision() == idle_timeline_revision &&
            model->layer_revision.get() == idle_layer_revision && action == idle_action,
            "Placeholder button changed application state");
    const auto timeline_frame=take(tree->drawing_snapshot({1999,1080}));
    std::optional<ii::gui::TextRun> layer_text;
    const auto find_layer_text=[&](const auto& texts){
        for(const auto& text:texts)if(text.text=="Timeline test layer")layer_text=text;
    };
    find_layer_text(timeline_frame.texts);
    for(const auto& overlay:timeline_frame.overlays)find_layer_text(overlay.texts);
    const auto layer_origin=take(widget(tree,"/layers").snapshot()).bounds.y;
    require(layer_text && layer_text->bounds.height==13 &&
            std::abs(layer_text->bounds.y-layer_origin-4.5)<0.01,
            "Timeline layer name does not use the Figma body line height and centered alignment");
    const auto keyframe_icon=take(lvui::native::design_icon_image_for_logical_size<ii::gui::CachedSvgDecoder>("keyframe"));
    std::vector<ii::gui::Rect> keyframes;
    const auto find_keyframes=[&](const auto& images){
        for(const auto& image:images)if(image.image==keyframe_icon)keyframes.push_back(image.bounds);
    };
    find_keyframes(timeline_frame.images);
    for(const auto& overlay:timeline_frame.overlays)find_keyframes(overlay.images);
    std::ranges::sort(keyframes,{},&ii::gui::Rect::x);
    const auto layer_bounds=take(widget(tree,"/layers").snapshot()).bounds;
    const auto ruler_bounds=take(widget(tree,"/timeline-ruler").snapshot()).bounds;
    const auto timeline_state=model->timeline->state.get();
    const auto ticks=std::clamp(int(std::round(timeline_state.duration*timeline_state.rate)),1,1000);
    const double cell=(ruler_bounds.width-416.)/ticks;
    require(keyframes.size()==2,"Timeline did not draw both keyframes using the LVUI icon");
    for(std::size_t i=0;i<keyframes.size();++i){
        const auto expected_x=layer_bounds.x+416.+cell*(i==0?.5:7.5)-9.;
        require(keyframes[i].width==18&&keyframes[i].height==18&&
                std::abs(keyframes[i].x-expected_x)<.05&&std::abs(keyframes[i].y-layer_bounds.y-2)<.05,
                "Timeline keyframe did not use the Figma LVUI icon size and centered cell placement");
    }
    for (const auto& [index, panel] : std::array<std::pair<int, const char*>, 4>{{
             {0, "/tool-detail"}, {1, "/generative-detail"},
             {2, "/document-detail"}, {3, "/asset-detail"}}}) {
        good(widget(tree,"/option-" + std::to_string(index)).activate());
        require(model->option_tab.get()==index && take(widget(tree,panel).snapshot()).visible,
                "Inspector segment did not show its matching panel");
    }
    good(widget(tree,"/option-0").activate());
    (void)take(tree->drawing_snapshot({1999,1080}));
    require(take(widget(tree,"/color-alpha").snapshot()).bounds.width==336,
            "Color opacity width does not follow the Figma panel padding");
    good(model->set_current_color({.2,.4,.6,.5}));
    require(take(widget(tree,"/color-hue").snapshot()).accessibility_description=="#33669980",
            "Detail color surface did not follow the shared color");
    const auto prior=model->timeline->state.get().frame;
    good(widget(tree,"/next-frame").activate());
    require(model->timeline->state.get().frame>prior,"Timeline transport did not advance a frame");
    const auto timeline_revision=model->timeline->state.revision();
    const auto layers_revision=model->layer_revision.get();
    view->select_layer("selection-only");
    require(model->timeline->state.revision()==timeline_revision && model->layer_revision.get()==layers_revision,
            "Changing the selected layer mutated timeline or layer data");
    good(host.clear_content());view.reset();tree->close();
}
