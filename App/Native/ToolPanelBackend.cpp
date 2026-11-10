#include "ToolPanelBackend.h"
#include "EditorLayout.h"
#include <Brush/BrushDynamics.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iiSDKCore/Core/Text/Number.h>

namespace congregation::ui {
using P = native::ToolParameter;

namespace {
bool is_shape_tool(const std::string& tool) {
    return tool=="polygon" || tool=="rectangle" || tool=="ellipse";
}
bool is_selection_tool(const std::string& tool) {
    return tool=="rect-marquee" || tool=="ellipse-marquee" || tool=="lasso" ||
           tool=="polygon-lasso" || tool=="magnetic-lasso" || tool=="magic-wand";
}
bool available(const EditorBackend& vm, P id) {
    const auto& settings=*vm.parameters;
    const bool wet=id==P::transfer_wetness_jitter || id==P::transfer_wetness_control ||
        id==P::transfer_wetness_minimum || id==P::transfer_mix_jitter ||
        id==P::transfer_mix_control || id==P::transfer_mix_minimum;
    if(wet && vm.selected_tool.get()!="mixer-brush" && vm.selected_tool.get()!="smudge") return false;
    if(id==P::shape_tilt_scale) return int(settings.value(P::shape_size_control))==int(BrushDynamicsControl::PenTilt);
    if(id==P::shape_minimum_roundness) return settings.value(P::shape_roundness_control)!=0;
    if(id==P::texture_minimum_depth) return settings.value(P::texture_depth_control)!=0;
    if(id==P::transfer_flow_minimum) return settings.value(P::transfer_flow_control)!=0;
    if(id==P::transfer_wetness_minimum) return settings.value(P::transfer_wetness_control)!=0;
    if(id==P::transfer_mix_minimum) return settings.value(P::transfer_mix_control)!=0;
    if(id==P::transfer_opacity_jitter) return vm.pressure_enabled.get();
    return true;
}
std::uint32_t argb(ii::gui::WidgetColor c) {
    const auto channel=[](double value){return std::uint32_t(std::lround(std::clamp(value,0.,1.)*255));};
    return (channel(c.alpha)<<24) | (channel(c.red)<<16) | (channel(c.green)<<8) | channel(c.blue);
}
}

ToolParameterRow::ToolParameterRow(std::shared_ptr<EditorBackend> editor, const native::ParameterSpec& spec)
    : key(spec.key), name(spec.name), icon(spec.icon),
      minimum_label(ii::core::format_decimal(spec.minimum)+std::string(spec.unit)),
      maximum_label(ii::core::format_decimal(spec.maximum)+std::string(spec.unit)),
      choice_label(spec.id==P::texture_mode ? "Mode" : spec.id==P::smoothing_method ? "Method" : "Control"),
      editor_(std::move(editor)), parameter_(spec.id) {
    if(spec.dynamics_choice)
        for(int value=int(spec.minimum);value<=int(spec.maximum);++value)
            options.push_back(native::ToolSettings::option_label(spec.id,value));
    refresh();
}
void ToolParameterRow::refresh() {
    const auto& settings=*editor_->parameters;
    const bool can_edit=available(*editor_,parameter_);
    normalized.set(settings.normalized(parameter_));
    value_label.set(can_edit ? settings.label(parameter_) : "—");
    enabled.set(can_edit);
    checked.set(settings.value(parameter_)!=0);
    choice_index.set(int(settings.value(parameter_)-native::ToolSettings::spec(parameter_).minimum));
}
void ToolParameterRow::move(double value) { editor_->set_parameter_normalized(parameter_,value); }
void ToolParameterRow::toggle(bool value) { editor_->set_parameter_normalized(parameter_,value?1.:0.); }
void ToolParameterRow::choose(int value) {
    const auto& spec=native::ToolSettings::spec(parameter_);
    if(value>=0 && std::size_t(value)<options.size()) editor_->set_parameter(parameter_,spec.minimum+value);
}
void ToolParameterRow::decrease() { editor_->step_parameter(parameter_,-1); }
void ToolParameterRow::increase() { editor_->step_parameter(parameter_,1); }

ToolPanelBackend::ToolPanelBackend(std::shared_ptr<EditorBackend> value, bool expanded_value)
    : editor(std::move(value)), expanded(expanded_value), selected_tool(editor->selected_tool), preset(editor->preset),
      spacing_label(editor->spacing_label), pressure_min_label(editor->pressure_min_label),
      pressure_mid_label(editor->pressure_mid_label), pressure_max_label(editor->pressure_max_label),
      texture_name(editor->parameters->texture_name), pressure_enabled(editor->pressure_enabled),
      clone_aligned(editor->clone_aligned), clone_current_layer(editor->clone_current_layer),
      selection_antialias(editor->selection_antialias), wand_contiguous(editor->wand_contiguous),
      wand_current_layer(editor->wand_current_layer), fill_contiguous(editor->fill_contiguous),
      fill_current_layer(editor->fill_current_layer), fill_target_current(editor->fill_target_current),
      fill_preserve_alpha(editor->fill_preserve_alpha), fill_antialias(editor->fill_antialias),
      crop_destructive(editor->crop_destructive), polygon_center(editor->polygon_center),
      polygon_fill(editor->polygon_fill), polygon_outline(editor->polygon_outline),
      spacing_pixels(editor->spacing_pixels), selection_feather(editor->selection_feather),
      magnetic_radius(editor->magnetic_radius), wand_tolerance(editor->wand_tolerance),
      fill_tolerance(editor->fill_tolerance), polygon_rotation(editor->polygon_rotation),
      polygon_stroke_width(editor->polygon_stroke_width), polygon_opacity(editor->polygon_opacity),
      polygon_sides(editor->polygon_sides), crop_aspect(editor->crop_aspect),
      gradient_revision(editor->gradient_settings->revision) {
    for(const auto& spec:native::ToolSettings::specs())
        rows_.emplace(std::string(spec.key),std::make_shared<ToolParameterRow>(editor,spec));
    values_lease_=editor->parameters->values.observe([this](const auto&,auto){refresh_rows();});
    tool_lease_=editor->selected_tool.observe([this](const auto&,auto){refresh_rows();});
    pressure_lease_=editor->pressure_enabled.observe([this](const auto&,auto){refresh_rows();});
    min_lease_=editor->pressure_min.observe([this](const auto&,auto){draw_curve();});
    mid_lease_=editor->pressure_mid.observe([this](const auto&,auto){draw_curve();});
    max_lease_=editor->pressure_max.observe([this](const auto&,auto){draw_curve();});
}
std::shared_ptr<ToolParameterRow> ToolPanelBackend::row(const std::string& key) const {
    auto found=rows_.find(key);
    if(found==rows_.end()) throw std::out_of_range("Unknown tool parameter: "+key);
    return found->second;
}
void ToolPanelBackend::refresh_rows() { for(const auto& [key,value]:rows_) { (void)key; value->refresh(); } }
bool ToolPanelBackend::is_shape(const std::string& tool) const { return is_shape_tool(tool); }
bool ToolPanelBackend::is_selection(const std::string& tool) const { return is_selection_tool(tool); }
bool ToolPanelBackend::is_brush(const std::string& tool) const {
    return tool!="crop" && tool!="perspective-crop" && !is_selection_tool(tool) &&
           tool!="gradient" && tool!="paint-bucket" && !is_shape_tool(tool);
}
std::string ToolPanelBackend::heading(const std::string& tool) const {
    if(tool=="crop" || tool=="perspective-crop")return "Crop";
    if(tool=="magic-wand")return "Magic wand";
    if(tool=="clone-stamp")return "Clone stamp";
    if(tool=="polygon")return "Polygon";
    if(tool=="rectangle")return "Rectangle";
    if(tool=="ellipse")return "Ellipse";
    if(is_selection_tool(tool))return "Selection";
    if(tool=="gradient")return "Gradient";
    if(tool=="paint-bucket")return "Paint bucket";
    return "Brush settings";
}
std::string ToolPanelBackend::count(const std::string& tool) const {
    if(tool=="crop" || tool=="perspective-crop")return "Crop and aspect";
    if(tool=="magic-wand")return "5 controls";
    if(tool=="polygon")return "7 controls";
    if(is_shape_tool(tool))return "5 controls";
    if(is_selection_tool(tool))return tool=="magnetic-lasso"?"3 controls":"2 controls";
    if(tool=="gradient")return "Stops and modes";
    if(tool=="paint-bucket")return "6 controls";
    return std::to_string(native::ToolSettings::specs().size())+" controls";
}
std::string ToolPanelBackend::aspect_label(std::int64_t value) const { return value==1?"1:1":value==2?"4:3":value==3?"16:9":"Free"; }
std::string ToolPanelBackend::crop_mode_label(bool value) const { return value?"Delete cropped pixels":"Keep cropped pixels"; }
std::string ToolPanelBackend::percentage(double value) const { return ii::core::format_decimal(std::round(value*100))+"%"; }
std::string ToolPanelBackend::pixels(double value) const { return ii::core::format_decimal(std::round(value))+" px"; }
std::string ToolPanelBackend::integer(std::int64_t value) const { return std::to_string(value); }
std::string ToolPanelBackend::degrees(double value) const { return ii::core::format_decimal(std::round(value))+"°"; }
void ToolPanelBackend::choose_preset(){editor->choose_preset();}
void ToolPanelBackend::set_pressure(bool value){editor->set_pressure_enabled(value);}
void ToolPanelBackend::set_pressure_choice(int value){set_pressure(value!=0);}
void ToolPanelBackend::set_spacing(double value){editor->set_spacing_pixels(value);}
void ToolPanelBackend::choose_texture(){editor->send(Action::choose_texture);}
void ToolPanelBackend::set_background(){editor->parameters->set_background(argb(editor->current_color.get()));if(editor->brush_changed)editor->brush_changed();}
void ToolPanelBackend::reset(){editor->reset_parameters();}
void ToolPanelBackend::save(){editor->save_parameters();}
void ToolPanelBackend::toggle_pressure(){editor->set_pressure_enabled(!pressure_enabled.get());}
void ToolPanelBackend::select_clone(){editor->select_tool("clone-stamp");}
void ToolPanelBackend::cycle_crop_aspect(){crop_aspect.set((crop_aspect.get()+1)%4);}
void ToolPanelBackend::toggle_crop_mode(){crop_destructive.set(!crop_destructive.get());}
void ToolPanelBackend::apply_crop(){editor->send(Action::apply_crop);}
void ToolPanelBackend::cancel_crop(){editor->send(Action::cancel_crop);}
void ToolPanelBackend::set_polygon_sides(double value){polygon_sides.set(std::int64_t(std::lround(std::clamp(value,3.,64.))));}

std::string ToolPanelBackend::gradient_stop(std::uint64_t)const {const auto& g=*editor->gradient_settings;return "Stop "+std::to_string(g.selected()+1)+" / "+std::to_string(g.stops().size());}
double ToolPanelBackend::gradient_position(std::uint64_t)const {const auto& g=*editor->gradient_settings;return g.stops()[g.selected()].position;}
bool ToolPanelBackend::gradient_position_enabled(std::uint64_t)const {const auto& g=*editor->gradient_settings;return g.selected()>0 && g.selected()+1<g.stops().size();}
std::string ToolPanelBackend::gradient_color(std::uint64_t)const {const auto& g=*editor->gradient_settings;std::ostringstream out;out<<'#'<<std::hex<<std::uppercase<<std::setw(6)<<std::setfill('0')<<(g.stops()[g.selected()].argb&0xffffffU);return out.str();}
double ToolPanelBackend::gradient_alpha(std::uint64_t)const {const auto& g=*editor->gradient_settings;return double(g.stops()[g.selected()].argb>>24)/255.;}
bool ToolPanelBackend::gradient_radial(std::uint64_t)const {return editor->gradient_settings->kind()==iiSharedCanvas::GradientKind::Radial;}
bool ToolPanelBackend::gradient_repeat(std::uint64_t)const {return editor->gradient_settings->spread()==iiSharedCanvas::GradientSpread::Repeat;}
std::string ToolPanelBackend::gradient_blend(std::uint64_t)const {const auto mode=editor->gradient_settings->blend();return mode==RasterBlendMode::Multiply?"Multiply":mode==RasterBlendMode::Screen?"Screen":mode==RasterBlendMode::Overlay?"Overlay":"Normal";}
void ToolPanelBackend::gradient_previous(){auto& g=*editor->gradient_settings;if(g.selected()>0)g.select(g.selected()-1);}
void ToolPanelBackend::gradient_next(){auto& g=*editor->gradient_settings;g.select(g.selected()+1);}
void ToolPanelBackend::gradient_add(){editor->gradient_settings->add();}
void ToolPanelBackend::gradient_remove(){editor->gradient_settings->remove();}
void ToolPanelBackend::set_gradient_position(double value){editor->gradient_settings->set_position(value);}
void ToolPanelBackend::gradient_use_current_color(){editor->gradient_settings->set_color(argb(editor->current_color.get()));}
void ToolPanelBackend::set_gradient_alpha(double value){editor->gradient_settings->set_alpha(value);}
void ToolPanelBackend::set_gradient_radial(bool value){editor->gradient_settings->set_radial(value);}
void ToolPanelBackend::set_gradient_repeat(bool value){editor->gradient_settings->set_repeat(value);}
void ToolPanelBackend::cycle_gradient_blend(){editor->gradient_settings->cycle_blend();}

void ToolPanelBackend::attach(ii::gui::Widget curve,ii::gui::Widget scroll,ii::gui::Widget tilt,ii::gui::Widget smoothing){
    curve_=curve;scroll_=scroll;tilt_=tilt;smoothing_=smoothing;draw_curve();
}
void ToolPanelBackend::draw_curve(){if(curve_.snapshot())native::editor::pressure_curve(curve_,editor->pressure_min.get(),editor->pressure_mid.get(),editor->pressure_max.get());}
void ToolPanelBackend::curve_resized(ii::gui::Rect){draw_curve();}
ii::core::Result<bool> ToolPanelBackend::curve_input(const ii::gui::WidgetInput& event){
    using K=ii::gui::WidgetInputKind;
    auto bounds=curve_.snapshot();if(!bounds)return false;
    const auto x=event.local_position ? event.local_position->x : event.x-bounds->bounds.x;
    const auto y=event.local_position ? event.local_position->y : event.y-bounds->bounds.y;
    if(event.kind==K::pointer_down)pressure_handle_=native::editor::pressure_point(x,bounds->bounds.width);
    if(event.kind==K::focus_lost || event.kind==K::pointer_leave || event.kind==K::pointer_cancel){pressure_handle_=-1;return false;}
    if(pressure_handle_<0 || (event.kind!=K::pointer_down && event.kind!=K::pointer_move && event.kind!=K::pointer_up))return false;
    const auto value=native::editor::pressure_value(y,bounds->bounds.height);
    if(pressure_handle_==0)editor->set_pressure_min(value);else if(pressure_handle_==1)editor->set_pressure_mid(value);else editor->set_pressure_max(value);
    if(event.kind==K::pointer_up)pressure_handle_=-1;
    return true;
}
void ToolPanelBackend::reveal(ii::gui::Widget target){const auto viewport=scroll_.snapshot(),item=target.snapshot();if(!viewport||!item)return;(void)scroll_.set_scroll_offset(std::max(0.,double(viewport->scroll_offset+item->bounds.y-viewport->bounds.y)));}
void ToolPanelBackend::reveal_tilt(){reveal(tilt_);}
void ToolPanelBackend::reveal_smoothing(){reveal(smoothing_);}
} // namespace congregation::ui
