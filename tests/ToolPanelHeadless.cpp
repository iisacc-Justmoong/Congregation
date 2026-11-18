#include <Tools/ToolPanel.h>
#include <Tools/ToolPanelBackend.h>
#include <Tools/ToolPanelCatalog.h>
#include <Tools/BrushPressure.h>
#include "ToolPanelView.generated.h"
#include <LVUI>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>
#include <iiSharedCanvas>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace congregation;
using namespace ii::gui;
template<class T> T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}

int main() try {
    // Exercise the actual generated LVUI callbacks: ordinary edits must not
    // publish the root-wide revision that reevaluates every panel binding.
    {
        auto probe_tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
        auto probe_host=take(probe_tree->content_host({},"selective-root",{400,800}));
        auto probe_vm=std::make_shared<ui::EditorBackend>();
        auto factories=lvui::native::BackendFactories{}.with<ui::ToolPanelBackend>(
            [probe_vm] {return ui::ToolPanelBackend(probe_vm);});
        std::shared_ptr<lvui_generated::ToolPanelView::Scope> scope;
        good(probe_host.set_content([&](WidgetBuilder& builder) {
            return lvui_generated::ToolPanelView::build(builder,{}, {}, &scope,factories);
        }));
        check(bool(scope),"Selective LVUI scope was not created");
        scope->backend_0.attach(take(probe_host.content_item("tool-pressure-curve")),
                                take(probe_host.content_item("tool-scroll")),
                                take(probe_host.content_item("parameter-tilt")),
                                take(probe_host.content_item("parameter-stabilization")));
        (void)take(probe_tree->drawing_snapshot({400,800}));
        auto find=[&](std::string_view id) {
            std::string found;
            for(const auto& node:take(probe_tree->snapshots()))
                if(node.id==id || node.id.ends_with("/"+std::string(id))) {
                    if(!found.empty())throw std::runtime_error("Ambiguous selective widget: "+std::string(id));
                    found=node.id;
                }
            if(found.empty())throw std::runtime_error("Missing selective widget: "+std::string(id));
            return take(probe_tree->find(found));
        };
        auto probe_scroll=find("tool-scroll");
        auto click_slider=[&](Widget control, double fraction) {
            const auto viewport=take(probe_scroll.snapshot()), item=take(control.snapshot());
            good(probe_scroll.set_scroll_offset(std::max(0.,double(viewport.scroll_offset+item.bounds.y-viewport.bounds.y))));
            const auto bounds=take(control.snapshot()).bounds;
            const double x=bounds.x+bounds.width*fraction, y=bounds.y+bounds.height/2;
            good(probe_tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
            good(probe_tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
        };
        const auto unrelated=scope->backend_0.row("noise-amount");
        auto unchanged=unrelated->refresh_count();
        const auto root_before=scope->revision_0.revision();
        click_slider(find("tool-absolute-spacing"),.5);
        check(probe_vm->parameters->value(native::ToolParameter::absolute_spacing)>1 &&
              scope->revision_0.revision()==root_before && unrelated->refresh_count()==unchanged,
              "Spacing input reevaluated the entire panel");
        good(find("pressure-enabled").activate());
        check(!probe_vm->pressure_enabled.get() && scope->revision_0.revision()==root_before &&
              unrelated->refresh_count()==unchanged,"Pressure input reevaluated the entire panel");
        const auto graph=find("tool-pressure-curve");
        const auto viewport=take(probe_scroll.snapshot()), graph_before=take(graph.snapshot());
        good(probe_scroll.set_scroll_offset(std::max(0.,double(viewport.scroll_offset+graph_before.bounds.y-viewport.bounds.y))));
        const auto bounds=take(graph.snapshot()).bounds;
        const auto old_min=probe_vm->pressure_min.get();
        good(probe_tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+8,.y=bounds.y+bounds.height*.25}));
        good(probe_tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+8,.y=bounds.y+bounds.height*.25}));
        if (!(probe_vm->pressure_min.get()!=old_min && scope->revision_0.revision()==root_before &&
              unrelated->refresh_count()==unchanged)) {
            const auto input=take(find("pressure-curve-input").snapshot());
            std::cerr << "Pressure curve diagnostics: minimum " << old_min << " -> " << probe_vm->pressure_min.get()
                      << ", root revision " << root_before << " -> " << scope->revision_0.revision()
                      << ", unrelated refresh " << unchanged << " -> " << unrelated->refresh_count()
                      << ", graph " << bounds.x << ',' << bounds.y << ',' << bounds.width << ',' << bounds.height
                      << ", input " << input.bounds.x << ',' << input.bounds.y << ',' << input.bounds.width << ',' << input.bounds.height
                      << ", input visible " << input.visible << '\n';
        }
        check(probe_vm->pressure_min.get()!=old_min && scope->revision_0.revision()==root_before &&
              unrelated->refresh_count()==unchanged,"Pressure curve reevaluated the entire panel");
        good(find("reset-tool").activate());
        check(probe_vm->parameters->value(native::ToolParameter::absolute_spacing)==0 &&
              scope->revision_0.revision()==root_before && unrelated->refresh_count()==unchanged,
              "Reset reevaluated unchanged rows");
        probe_vm->selected_tool.set("gradient");
        const auto gradient_root=scope->revision_0.revision();
        good(find("gradient-add-stop").activate());
        check(probe_vm->gradient_settings->stops().size()==3 && scope->revision_0.revision()==gradient_root,
              "Gradient stop editing reevaluated the entire panel");
    }
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
        if(result.empty()) {
            for(const auto& node:take(tree->snapshots()))
                if(node.accessibility_name==name || node.text==name || node.id.find("figma-parameter-choice")!=std::string::npos)
                    std::cerr << "Candidate id=" << node.id << " name=" << node.accessibility_name << " text=" << node.text
                              << " visible=" << node.visible << " bounds=" << node.bounds.x << ',' << node.bounds.y
                              << ',' << node.bounds.width << ',' << node.bounds.height << '\n';
            throw std::runtime_error("Missing accessible control: "+std::string(name));
        }
        return take(tree->find(result));
    };
    // Figma 84:9425 begins the default brush panel with its Brush settings;
    // it has neither a second tool header nor a preset and instruction strip.
    check(!take(widget("tool-heading").snapshot()).visible,
          "Default brush has an extra heading above its settings");
    for(const auto& node:take(tree->snapshots()))
        check(!node.id.ends_with("/brush-preset") &&
              node.text!="Drag to adjust · − / + for fine steps",
              "Tool panel still shows the extra brush preset or instruction strip");
    // Figma 364:5591 places the tool icon and 13 px title at the left
    // edge of the parameter column: 22 px icon, then an 8 px gap.
    vm->selected_tool.set("ellipse-marquee");
    (void)take(tree->drawing_snapshot({400,800}));
    std::optional<WidgetSnapshot> tool_title, first_section;
    for(const auto& node:take(tree->snapshots())) {
        if(node.visible && node.kind==WidgetKind::label && node.text=="Elliptical Marquee") tool_title=node;
        if(node.visible && node.kind==WidgetKind::label && node.text=="Selection") first_section=node;
    }
    check(tool_title.has_value() && first_section.has_value(),"The ellipse panel header or section is missing");
    check(std::abs(tool_title->bounds.x-first_section->bounds.x-30)<.01 &&
          std::abs(tool_title->font_size-13)<.01,
          "Tool title is not aligned or sized like the Figma panel header");
    vm->selected_tool.set("brush");
    (void)take(tree->drawing_snapshot({400,800}));
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
    for (const auto* brush : {"brush", "pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush"})
        check(model.is_brush(brush), "A paint brush does not expose the common LVUI parameters");
    for(const auto& spec:native::ToolSettings::specs())check(bool(model.row(std::string(spec.key))),"A brush row is missing");
    auto tool=vm->parameters;
    {
        using P = native::ToolParameter;
        const auto size_row=model.row("size"), flow_row=model.row("flow"), noise_row=model.row("noise-amount");
        const auto size_before=size_row->refresh_count(), flow_before=flow_row->refresh_count(), noise_before=noise_row->refresh_count();
        vm->set_parameter(P::flow, 43);
        check(flow_row->refresh_count()==flow_before+1 && size_row->refresh_count()==size_before &&
              noise_row->refresh_count()==noise_before, "A brush edit refreshed unrelated parameter rows");
        const auto tilt_row=model.row("shape-tilt-scale"), control_row=model.row("shape-size-control");
        const auto tilt_before=tilt_row->refresh_count(), control_before=control_row->refresh_count();
        vm->set_parameter(P::shape_size_control, 3);
        check(control_row->refresh_count()==control_before+1 && tilt_row->refresh_count()==tilt_before+1 &&
              tilt_row->enabled.get() && noise_row->refresh_count()==noise_before,
              "A control edit did not selectively update its dependent row");
        vm->set_parameter(P::shape_size_control, 2);
        vm->selected_tool.set("pencil");
        const auto rows=model.design_rows.get();
        const auto by_label=[&](std::string_view label) {
            const auto found=std::ranges::find_if(rows,[&](const auto& row){return row->label==label;});
            if(found==rows.end())throw std::runtime_error("Missing design row: "+std::string(label));
            return *found;
        };
        const auto snap=by_label("Snap to pixel grid"), shape=by_label("Tip shape");
        const auto snap_before=snap->refresh_count(), shape_before=shape->refresh_count();
        vm->set_parameter(P::pencil_snap,0);
        check(snap->refresh_count()==snap_before+1 && shape->refresh_count()==shape_before,
              "A variant parameter refreshed unrelated design rows");
        vm->selected_tool.set("brush");
    }
    {
        auto gradient_vm=std::make_shared<ui::EditorBackend>();
        ui::ToolPanelBackend gradient_panel(gradient_vm);
        gradient_vm->selected_tool.set("gradient");
        const auto rows=gradient_panel.design_rows.get();
        const auto by_label=[&](std::string_view label) {
            const auto found=std::ranges::find_if(rows,[&](const auto& row){return row->label==label && row->editable;});
            if(found==rows.end())throw std::runtime_error("Missing gradient row: "+std::string(label));
            return *found;
        };
        const auto type=by_label("Type"), repeat=by_label("Repeat"), blend=by_label("Blend mode");
        const auto type_before=type->refresh_count(), repeat_before=repeat->refresh_count(), blend_before=blend->refresh_count();
        gradient_vm->gradient_settings->set_radial(true);
        check(type->refresh_count()==type_before+1 && repeat->refresh_count()==repeat_before &&
              blend->refresh_count()==blend_before,"Gradient type refreshed unrelated rows");
        const auto controls=gradient_panel.gradient_panel.get();
        check(gradient_vm->gradient_settings->add(),"Cannot add a gradient stop");
        const auto selection_before=controls->selection_label.revision();
        const auto position_before=controls->position.revision();
        const auto color_before=controls->color_label.revision();
        const auto alpha_before=controls->alpha.revision();
        controls->set_alpha(.25);
        check(controls->alpha.revision()==alpha_before+1 && controls->selection_label.revision()==selection_before &&
              controls->position.revision()==position_before && controls->color_label.revision()==color_before &&
              type->refresh_count()==type_before+1 && repeat->refresh_count()==repeat_before &&
              blend->refresh_count()==blend_before,"Gradient opacity refreshed unrelated controls");
        controls->set_position(.3);
        check(controls->position.revision()==position_before+1 && controls->alpha.revision()==alpha_before+1 &&
              controls->selection_label.revision()==selection_before && controls->color_label.revision()==color_before,
              "Gradient position refreshed unrelated controls");
    }
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
    vm->set_parameter(native::ToolParameter::wetness, 35);
    check(take(wet.snapshot()).enabled,"Active brush mixing did not enable wet dynamics");
    vm->set_parameter(native::ToolParameter::wetness, 0);
    check(!take(wet.snapshot()).enabled,"Disabling brush mixing left wet dynamics enabled");
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
    auto has_visible_text=[&](std::string_view text) {
        const auto nodes=take(tree->snapshots());
        return std::ranges::any_of(nodes,[&](const auto& node){return node.visible && node.text==text;});
    };
    auto has_text=[&](std::string_view text) {
        const auto nodes=take(tree->snapshots());
        return std::ranges::any_of(nodes,[&](const auto& node){return node.text==text;});
    };
    vm->selected_tool.set("move");
    check(take(widget("tool-heading").snapshot()).visible && has_visible_text("Move"),
          "Move lost the title shown inside its Figma tool panel");
    check(has_visible_text("Target") && has_visible_text("Transform") && has_visible_text("Auto select"),
          "Move did not show its Figma panel");
    bool save_requested=false;
    vm->perform=[&](ui::Action action) { if (action==ui::Action::save_tool_settings) save_requested=true; };
    check(take(widget("save-tool").snapshot()).visible,"Non-brush panels must expose the existing save action");
    good(widget("save-tool").activate());
    check(save_requested,"Non-brush panel save did not dispatch the save action");
    vm->perform={};
    vm->selected_tool.set("hand");
    check(has_visible_text("Overscroll") && has_visible_text("Natural gesture direction") && !has_visible_text("Auto select"),
          "Hand did not replace the Move panel with its own controls");
    vm->selected_tool.set("pencil");
    (void)take(tree->drawing_snapshot({400,800}));
    check(has_visible_text("Pixel-aligned edges") && has_text("Shape Dynamics") && !has_visible_text("Overscroll"),
          "Pencil did not show its distinct Figma panel");
    auto pencil_size=widget("brush-size");
    reveal(pencil_size);
    const auto pencil_size_bounds=take(pencil_size.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=pencil_size_bounds.x+pencil_size_bounds.width*.4,
                      .y=pencil_size_bounds.y+pencil_size_bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=pencil_size_bounds.x+pencil_size_bounds.width*.4,
                      .y=pencil_size_bounds.y+pencil_size_bounds.height/2}));
    check(std::abs(tool->normalized(native::ToolParameter::size)-.4)<.03,
          "The Pencil panel slider did not update the existing brush model");
    auto pixel_edges=named("Pixel-aligned edges","/figma-check");
    reveal(pixel_edges); good(pixel_edges.activate());
    check(tool->value(native::ToolParameter::pencil_pixel_aligned)==0 &&
          !tool->brush("pencil").engineState->material.paperGrain.enabled,
          "Pencil edge control did not reach the brush engine");
    std::string shape_label_id;
    for(const auto& node:take(tree->snapshots())) if(node.text=="Tip shape") shape_label_id=node.id;
    check(!shape_label_id.empty(),"Pencil tip label was not built");
    reveal(take(tree->find(shape_label_id)));
    (void)take(tree->drawing_snapshot({400,800}));
    auto pencil_shape=named("Tip shape","/control");good(pencil_shape.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    check(tool->brush("pencil").engineState->rasterizer.shape.kind==BrushTipShape::Square,
          "Pencil tip selector did not update the brush shape");
    vm->selected_tool.set("airbrush");
    (void)take(tree->drawing_snapshot({400,800}));
    auto spray_rate=named("Spray rate","/figma-slider");reveal(spray_rate);
    const auto rate_bounds=take(spray_rate.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=rate_bounds.x+rate_bounds.width*.7,.y=rate_bounds.y+rate_bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=rate_bounds.x+rate_bounds.width*.7,.y=rate_bounds.y+rate_bounds.height/2}));
    check(std::abs(tool->brush("airbrush").engineState->rasterizer.airbrushRate-70)<4,
          "Spray rate slider did not reach the paint engine");
    vm->selected_tool.set("mixer-brush");
    (void)take(tree->drawing_snapshot({400,800}));
    auto sample_all=named("Sample all layers","/figma-check");reveal(sample_all);good(sample_all.activate());
    check(tool->value(native::ToolParameter::mixer_sample_all)==1,
          "Mixer sample source toggle did not update the brush profile");
    vm->selected_tool.set("dry-brush");
    (void)take(tree->drawing_snapshot({400,800}));
    auto dryness=named("Dryness","/figma-slider");reveal(dryness);
    const auto dry_bounds=take(dryness.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=dry_bounds.x+dry_bounds.width*.8,.y=dry_bounds.y+dry_bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=dry_bounds.x+dry_bounds.width*.8,.y=dry_bounds.y+dry_bounds.height/2}));
    check(tool->value(native::ToolParameter::dry_dryness)>75,
          "Dryness slider did not update the dry brush profile");
    vm->selected_tool.set("bristle-brush");
    (void)take(tree->drawing_snapshot({400,800}));
    auto spread=named("Bristle spread","/figma-slider");reveal(spread);
    const auto spread_bounds=take(spread.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=spread_bounds.x+spread_bounds.width*.7,.y=spread_bounds.y+spread_bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=spread_bounds.x+spread_bounds.width*.7,.y=spread_bounds.y+spread_bounds.height/2}));
    check(std::abs(tool->brush("bristle-brush").engineState->material.bristle.spread-.7)<.04,
          "Bristle spread slider did not reach the paint engine");
    for (const auto* brush : {"pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush"}) {
        vm->selected_tool.set(brush);
        check(take(widget("tool-parameters").snapshot()).visible && has_text("Shape Dynamics") &&
              has_text("Scattering") && has_text("Transfer") && has_text("Brush Pose"),
              "A brush variant is missing its common detailed panel");
    }
    for (const auto& panel : native::tool_panels::panels()) {
        if (model.is_brush(std::string(panel.tool))) continue;
        vm->selected_tool.set(std::string(panel.tool));
        check(take(scroll.snapshot()).scroll_offset==0,"A new tool panel did not scroll back to its top");
        for (const auto& field : panel.fields) {
            const auto nodes=take(tree->snapshots());
            const auto node=std::ranges::find_if(nodes,[&](const auto& item) {
                return item.id.find("@iiscope:list:")!=std::string::npos &&
                       (item.text == field.label || item.accessibility_name == field.label);
            });
            if(node==nodes.end())throw std::runtime_error("Missing Figma control in " + std::string(panel.tool) + ": " + std::string(field.label));
            auto target=take(tree->find(node->id));
            if(!node->visible)reveal(target);
            if(!take(target.snapshot()).visible)
                throw std::runtime_error("Cannot scroll to Figma control in " + std::string(panel.tool) + ": " + std::string(field.label));
        }
    }
    vm->selected_tool.set("brush");
    vm->tool_selected=[](std::string){};
    vm->select_tool("pen");
    check(vm->selected_tool.get()=="brush" && vm->inspected_tool.get()=="pen" && has_visible_text("Path operation"),
          "A view-only tool did not show its panel without changing the active canvas tool");
    check(!take(named("Path operation","/figma-choice").snapshot()).enabled,
          "A view-only control was enabled without an implementation");
    vm->tool_selected={};
    vm->selected_tool.set("brush");
    vm->selected_tool.set("rect-marquee");
    check(has_visible_text("Geometry")&&!take(widget("tool-parameters").snapshot()).visible,
          "Selection properties did not replace brush properties");
    good(named("Anti-alias","/figma-check").activate());check(!vm->selection_antialias.get(),"Selection toggle disconnected");
    vm->selected_tool.set("magnetic-lasso");check(take(named("Detection width","/figma-slider").snapshot()).visible,"Magnetic settings missing");
    vm->selected_tool.set("crop");good(named("Aspect ratio","/figma-choice").activate());check(vm->crop_aspect.get()==1,"Crop choice disconnected");
    good(named("Delete cropped pixels","/figma-check").activate());check(vm->crop_destructive.get(),"Crop mode disconnected");
    vm->selected_tool.set("polygon");check(take(named("Sides","/figma-value").snapshot()).visible,"Polygon settings missing");
    good(named("Stroke","/figma-choice").activate());check(vm->polygon_outline.get(),"Polygon option disconnected");
    vm->selected_tool.set("gradient");good(widget("gradient-add-stop").activate());
    check(vm->gradient_settings->stops().size()==3,"Gradient stop action disconnected");
    vm->selected_tool.set("paint-bucket");good(widget("fill-target-current").activate());
    good(named("Preserve transparency","/figma-check").activate());check(vm->fill_preserve_alpha.get(),"Fill option disconnected");
    vm->selected_tool.set("clone-stamp");check(has_visible_text("Source transform"),"Clone settings missing");
    good(named("Aligned","/figma-check").activate());check(!vm->clone_aligned.get(),"Clone option disconnected");
    auto old=size;good(host.clear_content());check(!old.snapshot(),"A retired panel retained an active widget");
    tree->close();std::cout<<"DECLARATIVE_TOOL_PANEL_OK\n";return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
