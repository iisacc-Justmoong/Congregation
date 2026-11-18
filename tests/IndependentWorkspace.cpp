#include <Workspace/WorkspaceView.h>
#include <LVUI>
#include <iiGUI>
#include <iiSDKCore>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>
#include <iiSharedCanvas>

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
    // Figma 84:9425 starts the color picker immediately after the 30 px
    // segment strip; there is no Options or current-tool heading above it.
    const auto options=take(widget(tree,"/options").snapshot());
    const auto color_bounds=take(widget(tree,"/color-picker-panel").snapshot()).bounds;
    require(std::abs(options.bounds.height-30)<.01 && options.child_count==1 &&
            std::abs(color_bounds.y-options.bounds.y-30)<.01,
            "Inspector has extra headings before the color picker");
    // Figma 84:9464: four 22 px icon buttons with 2 px between them.
    const auto segment_bounds=take(widget(tree,"/option-segments").snapshot()).bounds;
    require(std::abs(segment_bounds.width-94)<.01 && std::abs(segment_bounds.height-22)<.01,
            "Inspector segments must hug the four original icon buttons");
    for(int i=0;i<4;++i) {
        const auto bounds=take(widget(tree,"/option-"+std::to_string(i)).snapshot()).bounds;
        require(std::abs(bounds.width-22)<.01 && std::abs(bounds.height-22)<.01 &&
                std::abs(bounds.x-segment_bounds.x-i*24)<.01,
                "Inspector icon buttons lost their Figma size or spacing");
    }
    auto color_panel=widget(tree,"/color-picker-panel");
    require(take(color_panel.snapshot()).visible,"Brush lost its Figma color picker");
    model->selected_tool.set("move");
    require(!take(color_panel.snapshot()).visible,"Move incorrectly shows a color picker");
    model->selected_tool.set("pencil");
    require(take(color_panel.snapshot()).visible,"Pencil lost its Figma color picker");
    model->selected_tool.set("eraser");
    require(!take(color_panel.snapshot()).visible,"Eraser incorrectly shows a color picker");
    model->selected_tool.set("brush");
    bool has_icons=!frame.images.empty();
    for(const auto& layer:frame.overlays)has_icons|=!layer.images.empty();
    require(has_icons,"Workspace did not draw its native icons");
    require(take(widget(tree,"/timeline").snapshot()).bounds.height==296,
            "Timeline did not preserve the Figma content height");
    const auto new_layer_button = widget(tree, "/add-layer");
    const auto new_layer_bounds = take(new_layer_button.snapshot()).bounds;
    if (std::abs(new_layer_bounds.width - 40) >= .01 || std::abs(new_layer_bounds.height - 22) >= .01)
        throw std::runtime_error("New layer dropdown size: " + std::to_string(new_layer_bounds.width) + " x " + std::to_string(new_layer_bounds.height));
    good(new_layer_button.activate());
    require(action == congregation::ui::Action::open_new_layer_menu,
            "New layer must open its menu instead of creating a layer immediately");
    iiSharedCanvas::Document document;
    document.extent={8,8};document.timeline.frameCount=48;
    document.assets.emplace_back(iiSharedCanvas::RasterAsset{"asset-test",makeRasterLayer(8,8,0xffff0000U)});
    document.assets.emplace_back(iiSharedCanvas::RasterAsset{"asset-next",makeRasterLayer(8,8,0xff00ff00U)});
    document.assets.emplace_back(iiSharedCanvas::RasterAsset{"asset-blue",makeRasterLayer(8,8,0xff0000ffU)});
    document.layers.emplace_back(iiSharedCanvas::StaticBitmapLayer{
        {"layer-blue", "Other layer"}, iiSharedCanvas::StaticBitmapContent{"asset-blue"}});
    iiSharedCanvas::DynamicBitmapLayer layer;
    layer.properties.id="layer-test"; layer.properties.name="Timeline test layer";
    layer.content.frameIndices={0,7};
    document.layers.emplace_back(std::move(layer));
    document.frames.push_back(iiSharedCanvas::Frame{0,{{"layer-test","asset-test"}}});
    document.frames.push_back(iiSharedCanvas::Frame{7,{{"layer-test","asset-next"}}});
    document.audioTracks.push_back({"audio-test", "Audio test track"});
    model->timeline->scrub(0);
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
    std::optional<ii::gui::TextRun> layer_text, audio_text;
    const auto find_layer_text=[&](const auto& texts){
        for(const auto& text:texts) {
            if(text.text=="Timeline test layer") layer_text=text;
            if(text.text=="Audio test track") audio_text=text;
        }
    };
    find_layer_text(timeline_frame.texts);
    for(const auto& overlay:timeline_frame.overlays)find_layer_text(overlay.texts);
    const auto layer_origin=take(widget(tree,"/layers").snapshot()).bounds.y;
    require(layer_text && layer_text->bounds.height==13 &&
            std::abs(layer_text->bounds.y-layer_origin-4.5)<0.01,
            "Timeline layer name does not use the Figma body line height and centered alignment");
    require(audio_text && audio_text->bounds.y > layer_text->bounds.y,
            "Audio tracks must be visible as their own timeline rows");
    const auto preview_at = [&](const auto& drawing, int row_index = 0) -> std::shared_ptr<const ii::gui::RasterImage> {
        const auto bounds = take(widget(tree, "/layers").snapshot()).bounds;
        std::shared_ptr<const ii::gui::RasterImage> preview;
        const auto inspect = [&](const auto& images) {
            for (const auto& item : images)
                if (std::abs(item.bounds.x - bounds.x - 32) < .01 &&
                    std::abs(item.bounds.y - bounds.y - 2 - row_index * 22) < .01 &&
                    item.bounds.width == 18 && item.bounds.height == 18) preview = item.image;
        };
        inspect(drawing.images);
        for (const auto& overlay : drawing.overlays) inspect(overlay.images);
        return preview;
    };
    const auto is_color = [&](const auto& drawing, std::uint8_t red, std::uint8_t green) {
        const auto preview = preview_at(drawing);
        if (!preview || preview->width() != 18 || preview->height() != 18) return false;
        const auto rgba = preview->rgba();
        const auto at = (std::size_t(9) * 18 + 9) * 4;
        return rgba[at] == red && rgba[at + 1] == green && rgba[at + 3] == 255;
    };
    require(is_color(timeline_frame, 255, 0), "Timeline row must show its actual frame-zero pixels");
    const auto other = preview_at(timeline_frame, 1);
    require(other && other->rgba()[(std::size_t(9) * 18 + 9) * 4 + 2] == 255,
            "A second layer must show its own content instead of the selected layer");
    model->timeline->scrub(7.0 / 24.0);
    view->refresh_frame(document, 7);
    require(is_color(take(tree->drawing_snapshot({1999,1080})), 0, 255),
            "Timeline preview must follow dynamic layer keyframes");
    iiSharedCanvas::layerProperties(document.layers.back()).visible = false;
    view->refresh_document(document, "layer-test");
    require(is_color(take(tree->drawing_snapshot({1999,1080})), 0, 255),
            "Hiding a layer must not hide its content preview");
    const auto cached_preview = preview_at(take(tree->drawing_snapshot({1999,1080})));
    view->refresh_frame(document, 8);
    require(preview_at(take(tree->drawing_snapshot({1999,1080}))) == cached_preview,
            "Unchanged frame content must reuse the existing preview");
    std::ranges::fill(std::get<iiSharedCanvas::RasterAsset>(document.assets[1]).pixels.pixels, 0xffff0000U);
    view->refresh_document(document, "layer-test");
    require(is_color(take(tree->drawing_snapshot({1999,1080})), 255, 0),
            "Committed edits must refresh previews even when their asset identity is unchanged");
    // Incremental document changes retain every unaffected preview, including
    // when a different row changes or a shared asset affects multiple rows.
    using congregation::native::DocumentChange;
    using congregation::native::DocumentChangeKind;
    const auto stable_other = preview_at(take(tree->drawing_snapshot({1999,1080})), 1);
    const auto stable_current = preview_at(take(tree->drawing_snapshot({1999,1080})));
    iiSharedCanvas::layerProperties(document.layers.back()).name = "Renamed layer";
    view->apply_document_change(document, "layer-test", {DocumentChangeKind::name, "layer-test"});
    const auto renamed = take(tree->drawing_snapshot({1999,1080}));
    require(preview_at(renamed) == stable_current && preview_at(renamed, 1) == stable_other,
            "A name edit must not recreate any preview");
    bool renamed_visible = false;
    const auto find_renamed = [&](const auto& texts) {
        for (const auto& item : texts) renamed_visible |= item.text == "Renamed layer";
    };
    find_renamed(renamed.texts);
    for (const auto& overlay : renamed.overlays) find_renamed(overlay.texts);
    require(renamed_visible, "A renamed layer did not update its timeline row");
    std::ranges::fill(std::get<iiSharedCanvas::RasterAsset>(document.assets[1]).pixels.pixels, 0xff00ff00U);
    view->apply_document_change(document, "layer-test", {DocumentChangeKind::pixels, "layer-test", "asset-next"});
    const auto painted = take(tree->drawing_snapshot({1999,1080}));
    require(is_color(painted, 0, 255) && preview_at(painted) != stable_current &&
            preview_at(painted, 1) == stable_other,
            "A pixel edit must refresh only the affected preview");
    const auto after_paint = preview_at(painted);
    view->select_layer("layer-blue");
    const auto selected = take(tree->drawing_snapshot({1999,1080}));
    require(preview_at(selected) == after_paint && preview_at(selected, 1) == stable_other,
            "Selection must preserve both previews");
    view->select_layer("layer-test");

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
    // Structural changes retain previews of existing rows. Shared assets
    // invalidate every visible reference, not only the edited layer.
    const auto before_structure = take(tree->drawing_snapshot({1999,1080}));
    const auto existing_preview = preview_at(before_structure, 0);
    const auto existing_other = preview_at(before_structure, 1);
    document.layers.emplace_back(iiSharedCanvas::StaticBitmapLayer{
        {"layer-alias", "Shared content"}, iiSharedCanvas::StaticBitmapContent{"asset-next"}});
    view->apply_document_change(document, "layer-test", {DocumentChangeKind::structure});
    const auto added = take(tree->drawing_snapshot({1999,1080}));
    require(preview_at(added, 1) == existing_preview && preview_at(added, 2) == existing_other,
            "Adding a layer must retain existing previews");
    const auto alias_before = preview_at(added, 0);
    std::ranges::fill(std::get<iiSharedCanvas::RasterAsset>(document.assets[1]).pixels.pixels, 0xff0000ffU);
    view->apply_document_change(document, "layer-test", {DocumentChangeKind::pixels, "layer-test", "asset-next"});
    const auto shared = take(tree->drawing_snapshot({1999,1080}));
    require(preview_at(shared, 0) != alias_before && preview_at(shared, 1) != existing_preview &&
            preview_at(shared, 2) == existing_other,
            "A shared asset edit must refresh both references and retain unrelated previews");
    document.layers.pop_back();
    view->apply_document_change(document, "layer-test", {DocumentChangeKind::structure});
    const auto removed = take(tree->drawing_snapshot({1999,1080}));
    require(preview_at(removed, 0) == preview_at(shared, 1) && preview_at(removed, 1) == existing_other,
            "Removing a layer must retain surviving previews");
    std::string clicked_layer;
    model->selected = [&](std::string key) { clicked_layer = std::move(key); };
    const auto layer_hit = take(widget(tree, "/layers").snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,
                      .x=layer_hit.x+100, .y=layer_hit.y+11, .buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,
                      .x=layer_hit.x+100, .y=layer_hit.y+11}));
    require(clicked_layer == "layer-test", "Retained timeline row intercepted layer selection");
    for (const auto& [index, panel] : std::array<std::pair<int, const char*>, 4>{{
             {0, "/tool-detail"}, {1, "/generative-detail"},
             {2, "/document-detail"}, {3, "/asset-detail"}}}) {
        good(widget(tree,"/option-" + std::to_string(index)).activate());
        require(model->option_tab.get()==index && take(widget(tree,panel).snapshot()).visible,
                "Inspector segment did not show its matching panel");
    }
    good(widget(tree, "/option-2").activate());
    good(widget(tree, "/new").activate());
    require(action == congregation::ui::Action::new_document, "Document panel new-file action is disconnected");
    good(widget(tree, "/add-canvas").activate());
    require(action == congregation::ui::Action::add_canvas, "Document panel add-canvas action is disconnected");
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
