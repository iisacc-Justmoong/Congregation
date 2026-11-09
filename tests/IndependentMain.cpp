#include "IndependentMainView.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end()) throw std::runtime_error("Main control did not mount: "+std::string(suffix));
    return take(tree->find(found->id));
}
void animated_brush_popover() {
    using namespace std::chrono_literals;
    using View=lvui_generated::IndependentMainView;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames{events};
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},
        ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"motion-host",{800,600}));
    auto model=std::make_shared<congregation::ui::EditorColor>();std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[&](auto& view){
        view.backend_0.set_color_model(model);
    },&scope);}));
    (void)take(tree->drawing_snapshot({800,600}));
    auto origin=widget(tree,"/activate"),popup=widget(tree,"/brush-popup");
    const auto open=[&]{good(popup.open_for(origin,0,take(origin.snapshot()).bounds.height));};
    auto start=ii::runtime::Clock::now();open();(void)frames.step(start+1h);
    good(popup.close_popup());(void)frames.step(start+2h);open();start+=2h;(void)frames.step(start+80ms);
    const auto entering=take(tree->drawing_snapshot({800,600}));const auto bounds=take(popup.snapshot()).bounds;
    require(std::ranges::any_of(entering.overlays,[&](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->bounds==bounds&&layer.backdrop->underlay&&layer.backdrop->border_width==1&&
        layer.content_transform.scale<1&&std::abs(layer.content_transform.opacity-.5)<.01;}),
        "The Congregation brush popover did not animate its complete LVUI material");
    (void)frames.step(start+500ms);good(popup.close_popup());(void)frames.step(start+575ms);
    const auto exiting=take(tree->drawing_snapshot({800,600}));
    require(take(popup.snapshot()).exiting&&std::ranges::any_of(exiting.overlays,[&](const auto& layer){
        return layer.backdrop&&layer.shadow&&layer.backdrop->bounds==bounds&&layer.backdrop->underlay&&
            layer.content_transform.opacity>0&&layer.content_transform.opacity<1;
    }),"The Congregation brush popover lost its material during exit");
    good(host.clear_content());scope.reset();tree->close();
}
}

int main() {
    animated_brush_popover();
    using View=lvui_generated::IndependentMainView;
    auto model=std::make_shared<congregation::ui::EditorColor>();
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{800,600}));
    std::shared_ptr<View::Scope> scope;
    congregation::ui::Action invoked=congregation::ui::Action::quit;
    good(host.set_content([&](auto& builder) {
        return View::build(builder,{},[&](auto& view) {
            view.backend_0.set_color_model(model);
            view.backend_0.perform=[&](auto action){invoked=action;};
        },&scope);
    }));
    (void)take(tree->drawing_snapshot({800,600}));
    auto activate=widget(tree,"/activate");
    good(activate.activate());
    require(invoked==congregation::ui::Action::restore_license,"Activation action was not connected");
    auto brush_popup=widget(tree,"/brush-popup");
    good(brush_popup.open_for(activate,0,take(activate.snapshot()).bounds.height));
    const auto brush_frame=take(tree->drawing_snapshot({800,600}));
    const auto popup_bounds=take(brush_popup.snapshot()).bounds;
    const ii::gui::Rect underlay_bounds{popup_bounds.x-16,popup_bounds.y-16,popup_bounds.width+32,popup_bounds.height+32};
    const auto blue_accent=[&](const auto& material){
        const auto image=material.underlay;const auto extent=material.underlay_bounds;
        const auto x=std::min(image->width()-1,unsigned((popup_bounds.x+popup_bounds.width/2-extent.x)*image->width()/extent.width));
        const auto y=std::min(image->height()-1,unsigned((popup_bounds.y+popup_bounds.height/2-extent.y)*image->height()/extent.height));
        const auto at=(std::size_t(y)*image->width()+x)*4;const auto rgba=image->rgba();
        return rgba[at+3]>0&&rgba[at+2]>rgba[at];
    };
    require(std::ranges::any_of(brush_frame.overlays,[&](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->bounds==popup_bounds&&layer.backdrop->blur_radius==16&&layer.backdrop->border_width==1&&
        layer.backdrop->underlay&&layer.backdrop->underlay_bounds==underlay_bounds&&!layer.backdrop->underlay_tint&&
        blue_accent(*layer.backdrop)&&
        std::abs(layer.backdrop->inner_highlight_opacity-.14)<.0001&&layer.backdrop->inner_highlight_offset_y==1&&
        layer.shadow->blur_radius==24&&layer.shadow->bounds.y==popup_bounds.y+8;}),
        "Brush popover did not use the common LVUI glass material");
    require(take(brush_popup.snapshot()).bounds.width==340&&
            take(widget(tree,"/pressure-curve").snapshot()).bounds.width==316,
            "Brush popover did not preserve its content width and padding");
    auto scroll=widget(tree,"/brush-scroll");
    auto scroll_snapshot=take(scroll.snapshot());
    if(scroll_snapshot.bounds.height!=550||scroll_snapshot.content_height!=578)
        throw std::runtime_error("Brush popover viewport="+std::to_string(scroll_snapshot.bounds.height)+
                                 " content="+std::to_string(scroll_snapshot.content_height));
    for(const auto* name:{"/brush-size-popup","/brush-flow-popup","/brush-opacity-popup",
                          "/brush-hardness-popup","/brush-spacing-popup","/brush-spacing-ratio-popup",
                          "/brush-stabilizer-popup"})
        require(take(widget(tree,name).snapshot()).bounds.height==22,
                "Brush popover did not use the common Mini slider");
    good(scroll.set_scroll_offset(10000));
    auto last=take(widget(tree,"/pressure-maximum-label").snapshot());
    scroll_snapshot=take(scroll.snapshot());
    require(last.visible&&last.bounds.y+last.bounds.height<=scroll_snapshot.bounds.y+scroll_snapshot.bounds.height+.01,
            "Brush pressure controls remained clipped after scrolling");
    auto curve=take(widget(tree,"/pressure-curve").snapshot()).bounds;
    int pressure_events=0;
    scope->backend_0.pressure_input=[&](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
        if(event.kind==ii::gui::WidgetInputKind::pointer_down||event.kind==ii::gui::WidgetInputKind::pointer_up){
            require(std::abs((event.x-curve.x)-curve.width/2)<.01&&
                    std::abs((event.y-curve.y)-curve.height/2)<.01,
                    "Scrolled pressure input used incorrect coordinates");
            ++pressure_events;
        }
        return true;
    };
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=curve.x+curve.width/2,.y=curve.y+curve.height/2}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=curve.x+curve.width/2,.y=curve.y+curve.height/2}));
    require(pressure_events==2,"Scrolled pressure curve lost its pointer input");
    good(scroll.set_scroll_offset(0));
    auto flow=take(widget(tree,"/brush-flow-popup").snapshot()).bounds;
    const auto old_flow=scope->backend_0.flow.get();
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=flow.x+flow.width*.7,.y=flow.y+flow.height/2}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=flow.x+flow.width*.7,.y=flow.y+flow.height/2}));
    require(scope->backend_0.flow.get()!=old_flow,"Brush slider did not update its viewmodel after scrolling was introduced");
    good(brush_popup.close_popup());
    auto text_popup=widget(tree,"/text-popup");
    good(text_popup.open_for(activate,0,take(activate.snapshot()).bounds.height));
    (void)take(tree->drawing_snapshot({800,600}));
    require(take(text_popup.snapshot()).bounds.width==320&&
            take(widget(tree,"/text-value").snapshot()).bounds.width==296,
            "Text popover did not preserve its content width and padding");
    good(text_popup.close_popup());
    good(model->set_color({.2,.4,.6,.5}));
    require(scope->backend_0.current_color.get()==ii::gui::WidgetColor{.2,.4,.6,.5},
            "Main backend did not observe its shared color");
    auto editor=widget(tree,"/color-editor");
    require(take(editor.snapshot()).accessibility_description=="#33669980",
            "Main color surface did not follow an external color change");
    auto color_source=model->source();
    auto edited=color_source->edit(lvui::ColorSurfaceKind::wheel,{.75,.5},color_source->revision());
    require(bool(edited),"Color surface did not accept an edit");
    scope->backend_0.color_edited({});
    require(scope->backend_0.color_revision.get()==static_cast<std::int64_t>(*edited) &&
                scope->backend_0.current_color.get()==model->current(),
            "Main did not publish a color-surface edit");
    auto graph=scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty(),"Main has an unresolved reactive binding failure");
    std::weak_ptr<View::Scope> weak=scope;
    good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Main retained its retired scope");
    tree->close();
}
