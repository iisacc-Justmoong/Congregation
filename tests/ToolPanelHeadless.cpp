#include <Native/ToolPanel.h>
#include <Native/OwnedWidgetStyle.h>
#include <Native/EditorLayout.h>
#include <Native/Style.h>
#include <LVUI/NativeAuthoredIcons.h>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
using namespace congregation;
template<class T> T take(ii::core::Result<T> value) { if(!value) throw std::runtime_error(value.error().message); return std::move(*value); }
void good(ii::core::Result<void> value) { if(!value) throw std::runtime_error(value.error().message); }
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void slider_size_preset(){
 using namespace ii::gui;
 using congregation::native::owned_widgets::Controls;
 using congregation::native::owned_widgets::SliderHandle;
 auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
 std::optional<SliderHandle> mini,regular;double moved=-1;bool custom_style_rejected=false;
 good(state->construct([&](WidgetBuilder& builder)->ii::core::Result<void>{
  auto root=builder.absolute_stack({},"slider-root",{240,120});if(!root)return std::unexpected(root.error());
  Controls controls(builder);
  ButtonOptions custom;custom.style.emplace();
  auto rejected=controls.button(*root,"custom-style",std::move(custom));
  custom_style_rejected=!rejected&&rejected.error().code==ii::core::ErrorCode::unsupported;
  auto first=controls.slider(*root,"mini",{.width=100,.accessibility_name="Mini",.size=lvui::SliderSize::mini},[&](double v){moved=v;});
  if(!first)return std::unexpected(first.error());mini=*first;if(auto result=mini->root().set_frame({20,20,100,22});!result)return result;
  auto second=controls.slider(*root,"regular",{.width=100,.accessibility_name="Regular"});
  if(!second)return std::unexpected(second.error());regular=*second;return regular->root().set_frame({20,60,100,32});
 }));
 check(custom_style_rejected,"Product button adapter accepted a custom LVUI style");
 auto small=take(mini->root().snapshot()),standard=take(regular->root().snapshot());
 check(small.size.height==22&&standard.size.height==32&&small.accessibility_name=="Mini","Slider preset or accessibility name did not reach the native control");
 good(state->input({.kind=WidgetInputKind::pointer_down,.x=28,.y=31}));
 good(state->input({.kind=WidgetInputKind::pointer_move,.x=112,.y=31}));
 good(state->input({.kind=WidgetInputKind::pointer_up,.x=112,.y=31}));
 check(std::abs(moved-1)<1e-9&&take(mini->root().snapshot()).value==1,"Mini Slider preset changed pointer range mapping");
 state->close();
}
void expanded_panel_geometry(){
 using namespace ii::gui;
 for(const double host_width : {352.,400.}) {
 auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
 auto host=take(state->content_host({},"expanded-host",{host_width,2200}));
 auto vm=std::make_shared<ui::EditorBackend>();
 Widget panel;
 good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
  auto root=builder.vstack({},"expanded-root",{.alignment=StackAlignment::center});
  if(!root)return std::unexpected(root.error());
  panel=native::build_tool_panel(builder,*root,vm,true);return {};
 }));
 (void)take(state->drawing_snapshot({host_width,2200}));
 const auto nodes=take(state->snapshots());
 const auto found=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("/tool-scroll");});
 const auto size_slider=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("/brush-size");});
 const auto actions=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("/tool-actions");});
 check(found!=nodes.end(),"Expanded tool scroll view did not mount");
 if(size_slider==nodes.end())throw std::runtime_error("Tool size parameter did not mount");
 check(take(panel.snapshot()).bounds.width==host_width&&size_slider->bounds.width==host_width-16,
       "Tool parameter width does not follow the Figma panel padding");
 check(actions!=nodes.end()&&actions->bounds.height==38,"Tool footer did not follow the LVUI segment height");
 std::set<std::string_view> groups;
 for (const auto& spec : native::ToolSettings::specs()) groups.insert(spec.group);
 const auto expected_scroll=8.0+30.0*groups.size()+73.0*native::ToolSettings::specs().size()+16.0*(groups.size()-1);
 check(take(panel.snapshot()).bounds.height==expected_scroll+130 && found->bounds.height==expected_scroll,
       "Expanded tool panel does not follow the Figma content and spacing");
 good(host.clear_content());state->close();
 }
}
int main() try {
 slider_size_preset();
 expanded_panel_geometry();
 auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
 auto host=take(state->content_host({},"root",{400,800}));
 auto vm=std::make_shared<ui::EditorBackend>();
 good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{auto panel=native::build_tool_panel(builder,{},vm);(void)panel;return {};}));
 auto widget=[&](std::string_view id) {std::string found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+std::string(id))){check(found.empty(),"ambiguous widget");found=node.id;}check(!found.empty(),"missing widget");return take(state->find(found));};
 auto wand=widget("wand-parameters"),brush=widget("tool-parameters");
 (void)widget("parameter-minimum-opacity");
 (void)widget("parameter-maximum-opacity");
 check(!take(wand.snapshot()).visible && take(brush.snapshot()).visible,"initial tool panel visibility");
 check(take(widget("parameter-texture-scale").snapshot()).bounds.height==42 &&
       take(widget("adjust-texture-scale").snapshot()).bounds.height==22,
       "Texture parameters must use the Figma compact slider geometry");
 (void)widget("choose-texture"); (void)widget("tool-pressure-curve");
 (void)widget("set-background-color"); (void)widget("build-up-time");
 auto priorColor=vm->current_color.get(); vm->current_color.set({.1,.2,.3,1});
 good(widget("set-background-color").activate());
 check(vm->parameters->brush().engineState->rasterizer.color.secondaryArgb==0xff1a334dU,
       "Background color control did not update the engine brush");
 vm->current_color.set(priorColor);
 auto brush_frame=take(state->drawing_snapshot({400,800}));
 check(take(widget("parameter-size").snapshot()).bounds.height==73,
       "Tool parameter row differs from the Figma component composition");
 check(take(widget("section-gap-shape-size-jitter").snapshot()).bounds.height==16,
       "Tool sections must be visibly separated");
 const auto segment_bounds=take(widget("brush-dynamics").snapshot()).bounds;
 const auto segment_color=native::editor::linear(native::Color::surfaceSolid);
 const auto has_segment_surface=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){
     return panel.bounds==segment_bounds && panel.stroke_width==0 &&
            std::abs(panel.top.red-segment_color.red)<1e-5 && std::abs(panel.top.green-segment_color.green)<1e-5 &&
            std::abs(panel.top.blue-segment_color.blue)<1e-5;});};
 bool segment_surface=has_segment_surface(brush_frame.panels);
 for(const auto& layer:brush_frame.overlays)segment_surface|=has_segment_surface(layer.panels);
 check(segment_bounds.height==30 && segment_surface,"Tool segment does not use the LVUI Figma geometry and surface");
 const auto scroll=widget("tool-scroll"),tilt=widget("parameter-tilt"),smoothing=widget("parameter-stabilization");
 const auto parameter_revision=vm->parameters->values.revision();
 good(widget("tilt-detail").activate());
 auto viewport=take(scroll.snapshot());auto target=take(tilt.snapshot());
 check(viewport.scroll_offset>0 && std::abs(target.bounds.y-viewport.bounds.y)<1,
       "Tilt shortcut did not reveal the actual tilt parameter");
 const auto tilt_offset=viewport.scroll_offset;
 good(widget("smoothing-detail").activate());
 viewport=take(scroll.snapshot());target=take(smoothing.snapshot());
 check(viewport.scroll_offset<tilt_offset && std::abs(target.bounds.y-viewport.bounds.y)<1 &&
       vm->parameters->values.revision()==parameter_revision,
       "Smoothing shortcut did not reveal its parameter without changing settings");
 auto design_brush=take(lvui::native::design_icon_image_for_logical_size("brush-size"));
 auto contains_brush=[&](const auto& images){return std::ranges::any_of(images,[&](const auto& image){
     return image.image && image.bounds.width==lvui::design_icon_default_extent && image.bounds.height==lvui::design_icon_default_extent &&
            image.image->width()==design_brush->width() && image.image->height()==design_brush->height() &&
            std::ranges::equal(image.image->rgba(),design_brush->rgba());});};
 bool figma_brush=contains_brush(brush_frame.images);
 for(const auto& layer:brush_frame.overlays)figma_brush|=contains_brush(layer.images);
 check(figma_brush,"tool panel did not use the LVUI design brush icon");
 vm->selected_tool.set("rect-marquee");auto selection=widget("selection-parameters");check(take(selection.snapshot()).visible && !take(brush.snapshot()).visible,"selection panel visibility");
 auto frame=take(state->drawing_snapshot({400,800}));bool has_icons=!frame.images.empty();
 for(const auto& layer:frame.overlays)has_icons|=!layer.images.empty();
 check(has_icons,"tool panel did not draw its icons");
 auto feather_bounds=take(widget("selection-feather").snapshot()).bounds;
 good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=feather_bounds.x+feather_bounds.width/2,.y=feather_bounds.y+feather_bounds.height/2}));
 good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=feather_bounds.x+feather_bounds.width/2,.y=feather_bounds.y+feather_bounds.height/2}));
 check(vm->selection_feather.get()>0&&vm->selection_feather.get()<32,"selection feather pointer input did not update its model");
 vm->selection_feather.set(7);check(take(widget("selection-feather").snapshot()).value==7,"selection feather slider binding");
 good(widget("selection-antialias").activate());check(!vm->selection_antialias.get(),"selection antialias action");
 vm->selected_tool.set("magnetic-lasso");check(take(selection.snapshot()).visible && take(widget("magnetic-radius").snapshot()).visible,"magnetic panel visibility");
 vm->magnetic_radius.set(19);check(take(widget("magnetic-radius").snapshot()).value==19,"magnetic radius binding");
 vm->selected_tool.set("crop");auto crop=widget("crop-parameters");check(take(crop.snapshot()).visible&&!take(selection.snapshot()).visible&&!take(brush.snapshot()).visible,"crop panel visibility");
 good(widget("crop-mode").activate());check(vm->crop_destructive.get(),"crop mode action");good(widget("crop-aspect").activate());check(vm->crop_aspect.get()==1,"crop aspect action");ui::Action action=ui::Action::quit;vm->perform=[&](ui::Action value){action=value;};good(widget("crop-apply").activate());check(action==ui::Action::apply_crop,"crop apply action");good(widget("crop-cancel").activate());check(action==ui::Action::cancel_crop,"crop cancel action");
 vm->selected_tool.set("magic-wand");
 check(take(wand.snapshot()).visible && take(selection.snapshot()).visible && !take(brush.snapshot()).visible,"wand tool panel visibility");
 good(widget("selection-antialias").activate());check(vm->selection_antialias.get(),"wand antialias action");
 vm->selection_feather.set(4);check(take(widget("selection-feather").snapshot()).value==4,"wand feather binding");
 vm->wand_tolerance.set(.42);check(take(widget("wand-tolerance").snapshot()).value==.42,"wand slider binding");
 auto contiguous=widget("wand-contiguous");good(contiguous.activate());check(!vm->wand_contiguous.get(),"wand contiguous action");
 auto reference=widget("wand-current-layer");good(reference.activate());check(vm->wand_current_layer.get(),"wand reference action");
 vm->selected_tool.set("polygon");auto polygon=widget("polygon-parameters");check(take(polygon.snapshot()).visible&&!take(wand.snapshot()).visible&&!take(selection.snapshot()).visible&&!take(brush.snapshot()).visible,"polygon tool panel visibility");
 vm->polygon_sides.set(7);check(take(widget("polygon-sides").snapshot()).value==7,"polygon sides binding");
 vm->polygon_rotation.set(30);check(take(widget("polygon-rotation").snapshot()).value==30,"polygon rotation binding");
 good(widget("polygon-center").activate());check(vm->polygon_center.get(),"polygon center action");
 good(widget("polygon-outline").activate());check(vm->polygon_outline.get(),"polygon outline action");
 vm->polygon_stroke_width.set(4);check(take(widget("polygon-stroke-width").snapshot()).value==4,"polygon outline width binding");
 vm->polygon_opacity.set(.5);check(take(widget("polygon-opacity").snapshot()).value==.5,"polygon opacity binding");
 vm->selected_tool.set("clone-stamp");check(take(widget("clone-parameters").snapshot()).visible,"clone options hidden");check(take(brush.snapshot()).visible,"clone brush settings hidden");good(widget("clone-aligned").activate());check(!vm->clone_aligned.get(),"clone aligned action");good(widget("clone-current-layer").activate());check(!vm->clone_current_layer.get(),"clone reference action");
 vm->selected_tool.set("rectangle");check(take(polygon.snapshot()).visible&&!take(brush.snapshot()).visible&&!take(widget("polygon-sides").snapshot()).visible&&!take(widget("polygon-rotation").snapshot()).visible,"rectangle shape controls visibility");
 vm->selected_tool.set("ellipse");check(take(polygon.snapshot()).visible&&!take(brush.snapshot()).visible&&!take(widget("polygon-sides").snapshot()).visible,"ellipse shape controls visibility");
 vm->selected_tool.set("gradient");auto gradient=widget("gradient-parameters");check(take(gradient.snapshot()).visible && !take(wand.snapshot()).visible && !take(brush.snapshot()).visible,"gradient panel visibility");
 good(widget("gradient-add-stop").activate());check(vm->gradient_settings->stops().size()==3,"gradient add stop action");
 vm->gradient_settings->set_position(.25);check(take(widget("gradient-stop-position").snapshot()).value==.25,"gradient position binding");
 vm->current_color.set({.2,.4,.6,.5});good(widget("gradient-use-current-color").activate());check(vm->gradient_settings->stops()[1].argb==0x80336699U,"gradient picker color action");
 vm->gradient_settings->set_alpha(.25);check(std::abs(take(widget("gradient-stop-alpha").snapshot()).value - 64./255) < 1e-9,"gradient alpha binding");
 good(widget("gradient-radial").activate());check(vm->gradient_settings->kind()==iiSharedCanvas::GradientKind::Radial,"gradient radial action");
 good(widget("gradient-repeat").activate());check(vm->gradient_settings->spread()==iiSharedCanvas::GradientSpread::Repeat,"gradient repeat action");
 good(widget("gradient-blend").activate());check(vm->gradient_settings->blend()==RasterBlendMode::Multiply,"gradient blend action");
 good(widget("gradient-remove-stop").activate());check(vm->gradient_settings->stops().size()==2,"gradient remove stop action");
 vm->selected_tool.set("paint-bucket");auto fill=widget("fill-parameters");check(take(fill.snapshot()).visible && !take(gradient.snapshot()).visible && !take(brush.snapshot()).visible,"fill panel visibility");
 vm->fill_tolerance.set(.31);check(take(widget("fill-tolerance").snapshot()).value==.31,"fill tolerance binding");
 good(widget("fill-contiguous").activate());check(!vm->fill_contiguous.get(),"fill contiguous action");
 good(widget("fill-antialias").activate());check(vm->fill_antialias.get(),"fill antialias action");
 good(widget("fill-current-layer").activate());check(vm->fill_current_layer.get(),"fill reference action");
 vm->fill_preserve_alpha.set(true);check(!take(widget("fill-preserve-alpha").snapshot()).enabled,"a value update enabled a disabled fill option");
 vm->fill_preserve_alpha.set(false);
 good(widget("fill-target-current").activate());check(vm->fill_target_current.get(),"fill target action");
 good(widget("fill-preserve-alpha").activate());check(vm->fill_preserve_alpha.get(),"fill alpha action");
 vm->selected_tool.set("perspective-crop");check(take(widget("crop-parameters").snapshot()).visible && !take(fill.snapshot()).visible,"perspective crop panel visibility");
 vm->selected_tool.set("brush");check(!take(fill.snapshot()).visible && !take(gradient.snapshot()).visible && !take(wand.snapshot()).visible && take(brush.snapshot()).visible,"brush panel restore");
 state->close();std::cout<<"TOOL_PANEL_HEADLESS_OK\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
