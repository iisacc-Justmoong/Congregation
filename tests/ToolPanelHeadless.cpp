#include <Native/ToolPanel.h>
#include <Native/ToolPanelBackend.h>
#include <Native/BrushPressure.h>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>
#include <iiSharedCanvasNative.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace congregation;
using namespace ii::gui;
template<class T> T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}

int main() try {
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"root",{400,800}));
    auto vm=std::make_shared<ui::EditorBackend>();
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        (void)native::build_tool_panel(builder,{},vm);return {};
    }));
    (void)take(tree->drawing_snapshot({400,800}));
    auto widget=[&](std::string_view id){
        std::string result;
        for(const auto& node:take(tree->snapshots()))
            if(node.id==id || node.id.ends_with("/"+std::string(id))) {
                if(!result.empty())throw std::runtime_error("Ambiguous widget: "+std::string(id));
                result=node.id;
            }
        if(result.empty())throw std::runtime_error("Missing widget: "+std::string(id));
        return take(tree->find(result));
    };
    auto named=[&](std::string_view name,std::string_view suffix){
        std::string result;
        for(const auto& node:take(tree->snapshots()))
            if(node.accessibility_name==name && node.id.ends_with(suffix)) {
                if(!result.empty())throw std::runtime_error("Ambiguous accessible control: "+std::string(name));
                result=node.id;
            }
        if(result.empty())throw std::runtime_error("Missing accessible control: "+std::string(name));
        return take(tree->find(result));
    };
    auto click=[&](Widget target){
        const auto bounds=take(target.snapshot()).bounds;
        const auto x=bounds.x+bounds.width/2,y=bounds.y+bounds.height/2;
        good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
    };
    auto scroll=widget("tool-scroll");
    auto reveal=[&](Widget target){
        auto viewport=take(scroll.snapshot()), item=take(target.snapshot());
        good(scroll.set_scroll_offset(std::max(0.,double(viewport.scroll_offset+item.bounds.y-viewport.bounds.y))));
    };
    auto size=widget("brush-size"), flow=named("Flow","/slider"), tilt=named("Tilt Scale","/slider");
    check(take(size.snapshot()).bounds.height==22 && take(flow.snapshot()).bounds.height==22,
          "Declared brush controls did not retain the mini slider geometry");
    check(!take(tilt.snapshot()).enabled,"Tilt Scale was enabled without Pen Tilt");
    ui::ToolPanelBackend model(vm);
    check(model.row("size")->name=="Size","The declarative row did not borrow the brush model");
    for(const auto& spec:native::ToolSettings::specs())check(bool(model.row(std::string(spec.key))),"A brush row is missing");
    auto tool=vm->parameters;
    int brush_changes=0;vm->brush_changed=[&]{++brush_changes;};
    reveal(flow);const auto flow_bounds=take(flow.snapshot()).bounds;
    const auto flow_x=flow_bounds.x+flow_bounds.width*.7;
    const auto flow_y=flow_bounds.y+flow_bounds.height/2;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=flow_x,.y=flow_y}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=flow_x,.y=flow_y}));
    check(tool->value(native::ToolParameter::flow)<90 && brush_changes>0,
          "Moving a declared slider did not update the brush model");
    vm->set_flow(.43);
    check(std::abs(take(flow.snapshot()).value-.43)<.001,"Popup and declared panel values diverged");
    auto size_control=named("Size Control","/control");
    reveal(size_control);good(size_control.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    check(tool->value(native::ToolParameter::shape_size_control)==3,"Choosing Pen Tilt did not update the brush model");
    good(size_control.activate());
    std::string open_menu;
    for(const auto& node:take(tree->snapshots()))
        if(node.id.ends_with("/menu") && node.opened)open_menu=node.id;
    check(!open_menu.empty(),"The Size Control menu did not open");
    good(take(tree->find(open_menu)).close_popup());
    check(take(tilt.snapshot()).enabled,"Pen Tilt did not enable Tilt Scale");
    vm->set_parameter(native::ToolParameter::shape_tilt_scale,80);
    check(vm->parameters->brush().engineState->dynamics.sizeResponse.tilt.enabled,
          "Tilt Scale did not reach brush dynamics");
    vm->set_parameter(native::ToolParameter::shape_size_control,2);
    auto wet=named("Wetness Jitter","/slider");
    check(!take(wet.snapshot()).enabled,"Dry brush exposed wet mixing controls");
    vm->selected_tool.set("mixer-brush");check(take(wet.snapshot()).enabled,"Mixer controls remained disabled");
    vm->selected_tool.set("brush");check(!take(wet.snapshot()).enabled,"Dry brush enabled wet mixing controls");
    vm->set_parameter(native::ToolParameter::noise_enabled,1);
    vm->set_parameter(native::ToolParameter::noise_monochrome,0);
    check(!tool->brush().engineState->rasterizer.noise.monochrome,"Noise option did not reach the engine");
    auto curve=widget("tool-pressure-curve");reveal(curve);
    (void)take(tree->drawing_snapshot({400,800}));
    check(take(curve.snapshot()).drawing_panels>0,"Pressure curve was not drawn by its backend");
    const auto old_min=vm->pressure_min.get();auto graph=take(curve.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=graph.x+8,.y=graph.y+graph.height*.25}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=graph.x+8,.y=graph.y+graph.height*.25}));
    check(vm->pressure_min.get()!=old_min,"Pressure curve input did not reach the editor");
    good(scroll.set_scroll_offset(0));auto bounds=take(scroll.snapshot()).bounds;
    check(tree->touch_scrollable_at(bounds.x+10,bounds.y+40),"Declared panel did not expose a scroll viewport");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=bounds.x+10,.y=bounds.y+40,.scroll_y=300}));
    check(take(scroll.snapshot()).scroll_offset>0,"Wheel input did not scroll the declared panel");
    good(scroll.set_scroll_offset(0));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=bounds.x+10,.y=bounds.y+40,.scroll_y=-120,
                      .precise_scroll=true,.device=PointerDevice::touch}));
    check(take(scroll.snapshot()).scroll_offset>0,"Touch input did not scroll the declared panel");
    good(scroll.set_scroll_offset(0));
    vm->selected_tool.set("rect-marquee");
    check(take(widget("selection-parameters").snapshot()).visible&&!take(widget("tool-parameters").snapshot()).visible,
          "Selection properties did not replace brush properties");
    good(widget("selection-antialias").activate());check(!vm->selection_antialias.get(),"Selection toggle disconnected");
    vm->selected_tool.set("magnetic-lasso");check(take(widget("magnetic-radius").snapshot()).visible,"Magnetic settings missing");
    vm->selected_tool.set("crop");good(widget("crop-aspect").activate());check(vm->crop_aspect.get()==1,"Crop choice disconnected");
    good(widget("crop-mode").activate());check(vm->crop_destructive.get(),"Crop mode disconnected");
    vm->selected_tool.set("polygon");check(take(widget("polygon-sides").snapshot()).visible,"Polygon settings missing");
    good(widget("polygon-outline").activate());check(vm->polygon_outline.get(),"Polygon option disconnected");
    vm->selected_tool.set("gradient");good(widget("gradient-add-stop").activate());
    check(vm->gradient_settings->stops().size()==3,"Gradient stop action disconnected");
    vm->selected_tool.set("paint-bucket");good(widget("fill-target-current").activate());
    good(widget("fill-preserve-alpha").activate());check(vm->fill_preserve_alpha.get(),"Fill option disconnected");
    vm->selected_tool.set("clone-stamp");check(take(widget("clone-parameters").snapshot()).visible,"Clone settings missing");
    good(widget("clone-aligned").activate());check(!vm->clone_aligned.get(),"Clone option disconnected");
    auto old=size;good(host.clear_content());check(!old.snapshot(),"A retired panel retained an active widget");
    tree->close();std::cout<<"DECLARATIVE_TOOL_PANEL_OK\n";return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
