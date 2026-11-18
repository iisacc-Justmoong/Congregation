#include "ToolPanelBackend.h"
#include "Shared/EditorLayout.h"
#include <Brush/BrushDynamics.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iiSDKCore>

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
bool is_paint_brush(const std::string& tool) {
    return tool=="brush" || tool=="pencil" || tool=="airbrush" || tool=="mixer-brush" ||
           tool=="dry-brush" || tool=="bristle-brush";
}
bool variant_field(std::string_view tool, std::string_view label) {
    const auto has = [&](std::initializer_list<std::string_view> values) {
        return std::ranges::find(values, label) != values.end();
    };
    if (tool=="pencil") return has({"Tip shape", "Pixel-aligned edges", "Auto erase", "Snap to pixel grid", "Blend mode", "Symmetry"});
    if (tool=="airbrush") return has({"Spray radius", "Spray rate", "Build up while stationary", "Blend mode", "Symmetry"});
    if (tool=="mixer-brush") return has({"Tip shape", "Sample all layers", "Clean after each stroke", "Load after each stroke", "Symmetry"});
    if (tool=="dry-brush") return has({"Grain amount", "Grain size", "Dryness", "Blend mode"});
    if (tool=="bristle-brush") return has({"Bristle length", "Bristle thickness", "Bristle stiffness", "Bristle spread", "Blend mode"});
    return false;
}
bool available(const EditorBackend& vm, P id) {
    const auto& settings=*vm.parameters;
    const bool wet=id==P::transfer_wetness_jitter || id==P::transfer_wetness_control ||
        id==P::transfer_wetness_minimum || id==P::transfer_mix_jitter ||
        id==P::transfer_mix_control || id==P::transfer_mix_minimum;
    if(wet && vm.selected_tool.get()!="mixer-brush" && vm.selected_tool.get()!="smudge" &&
       settings.value(P::mixing_active)==0) return false;
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
    const auto watch_parameter = [this](P id) {
        subscriptions_.push_back(editor_->parameters->parameter_revision(id).observe(
            [this](const auto&, auto) { refresh(); }));
    };
    watch_parameter(parameter_);
    const bool wet = parameter_==P::transfer_wetness_jitter || parameter_==P::transfer_wetness_control ||
        parameter_==P::transfer_wetness_minimum || parameter_==P::transfer_mix_jitter ||
        parameter_==P::transfer_mix_control || parameter_==P::transfer_mix_minimum;
    if (wet) {
        watch_parameter(P::mixing_active);
        subscriptions_.push_back(editor_->selected_tool.observe([this](const auto&, auto) { refresh(); }));
    }
    if (parameter_==P::shape_tilt_scale) watch_parameter(P::shape_size_control);
    if (parameter_==P::shape_minimum_roundness) watch_parameter(P::shape_roundness_control);
    if (parameter_==P::texture_minimum_depth) watch_parameter(P::texture_depth_control);
    if (parameter_==P::transfer_flow_minimum) watch_parameter(P::transfer_flow_control);
    if (parameter_==P::transfer_wetness_minimum) watch_parameter(P::transfer_wetness_control);
    if (parameter_==P::transfer_mix_minimum) watch_parameter(P::transfer_mix_control);
    if (parameter_==P::transfer_opacity_jitter)
        subscriptions_.push_back(editor_->pressure_enabled.observe([this](const auto&, auto) { refresh(); }));
    refresh();
}
void ToolParameterRow::refresh() {
    const auto keep_alive = weak_from_this().lock();
    ++refresh_count_;
    const auto& settings=*editor_->parameters;
    const bool can_edit=available(*editor_,parameter_);
    normalized.set(settings.normalized(parameter_));
    raw_value.set(settings.value(parameter_));
    value_label.set(can_edit ? settings.label(parameter_) : "—");
    enabled.set(can_edit);
    checked.set(settings.value(parameter_)!=0);
    choice_index.set(int(settings.value(parameter_)-native::ToolSettings::spec(parameter_).minimum));
}
void ToolParameterRow::move(double value) { if(enabled.get()) editor_->set_parameter_normalized(parameter_,value); }
void ToolParameterRow::set_value(double value) { if(enabled.get()) editor_->set_parameter(parameter_,value); }
void ToolParameterRow::toggle(bool value) { if(enabled.get()) editor_->set_parameter_normalized(parameter_,value?1.:0.); }
void ToolParameterRow::choose_boolean(int value) { if (value == 0 || value == 1) toggle(value != 0); }
void ToolParameterRow::flip() {
    if (enabled.get() && native::ToolSettings::spec(parameter_).checkbox)
        editor_->set_parameter(parameter_, editor_->parameters->value(parameter_) == 0 ? 1 : 0);
}
void ToolParameterRow::choose(int value) {
    const auto& spec=native::ToolSettings::spec(parameter_);
    if(enabled.get() && value>=0 && std::size_t(value)<options.size()) editor_->set_parameter(parameter_,spec.minimum+value);
}
void ToolParameterRow::decrease() { if(enabled.get()) editor_->step_parameter(parameter_,-1); }
void ToolParameterRow::increase() { if(enabled.get()) editor_->step_parameter(parameter_,1); }

GradientPanelModel::GradientPanelModel(std::shared_ptr<EditorBackend> editor)
    : editor_(std::move(editor)) {
    using Field = native::GradientSettings::Field;
    const auto watch = [this](Field field, auto callback) {
        subscriptions_.push_back(editor_->gradient_settings->field_revision(field).observe(
            [this, callback](const auto&, auto) { (this->*callback)(); }));
    };
    watch(Field::selection, &GradientPanelModel::refresh_selection);
    watch(Field::position, &GradientPanelModel::refresh_position);
    watch(Field::color, &GradientPanelModel::refresh_color);
    watch(Field::alpha, &GradientPanelModel::refresh_alpha);
    refresh_selection(); refresh_position(); refresh_color(); refresh_alpha();
}
void GradientPanelModel::refresh_selection() {
    const auto keep_alive = weak_from_this().lock();
    const auto& g=*editor_->gradient_settings;
    selection_label.set("Stop "+std::to_string(g.selected()+1)+" / "+std::to_string(g.stops().size()));
    position_enabled.set(g.selected()>0 && g.selected()+1<g.stops().size());
}
void GradientPanelModel::refresh_position() {
    const auto keep_alive = weak_from_this().lock();
    const auto& g=*editor_->gradient_settings;
    const auto value=g.stops()[g.selected()].position;
    position.set(value);
    position_label.set(ii::core::format_decimal(std::round(value*100))+"%");
}
void GradientPanelModel::refresh_color() {
    const auto keep_alive = weak_from_this().lock();
    const auto& g=*editor_->gradient_settings;
    std::ostringstream out; out<<'#'<<std::hex<<std::uppercase<<std::setw(6)<<std::setfill('0')
                               <<(g.stops()[g.selected()].argb & 0xffffffU);
    color_label.set(out.str());
}
void GradientPanelModel::refresh_alpha() {
    const auto keep_alive = weak_from_this().lock();
    const auto& g=*editor_->gradient_settings;
    const auto value=double(g.stops()[g.selected()].argb>>24)/255.;
    alpha.set(value);
    alpha_label.set(ii::core::format_decimal(std::round(value*100))+"%");
}
void GradientPanelModel::previous() { auto& g=*editor_->gradient_settings; if(g.selected()>0)g.select(g.selected()-1); }
void GradientPanelModel::next() { auto& g=*editor_->gradient_settings; g.select(g.selected()+1); }
void GradientPanelModel::add() { editor_->gradient_settings->add(); }
void GradientPanelModel::remove() { editor_->gradient_settings->remove(); }
void GradientPanelModel::set_position(double value) { editor_->gradient_settings->set_position(value); }
void GradientPanelModel::use_current_color() { editor_->gradient_settings->set_color(argb(editor_->current_color.get())); }
void GradientPanelModel::set_alpha(double value) { editor_->gradient_settings->set_alpha(value); }

PressureCurveModel::PressureCurveModel(std::shared_ptr<EditorBackend> editor) : editor_(std::move(editor)) {
    const auto watch = [this](const auto& value) {
        subscriptions_.push_back(value.observe([this](const auto&, auto) { draw(); }));
    };
    watch(editor_->pressure_min); watch(editor_->pressure_mid); watch(editor_->pressure_max);
}
void PressureCurveModel::attach(ii::gui::Widget curve) { curve_=curve; draw(); }
void PressureCurveModel::draw() {
    const auto keep_alive=weak_from_this().lock();
    if(curve_.snapshot()) native::editor::pressure_curve(curve_,editor_->pressure_min.get(),editor_->pressure_mid.get(),editor_->pressure_max.get());
}
void PressureCurveModel::resized(ii::gui::Rect) { draw(); }
ii::core::Result<bool> PressureCurveModel::input(const ii::gui::WidgetInput& event) {
    const auto keep_alive=weak_from_this().lock();
    using K=ii::gui::WidgetInputKind;
    auto bounds=curve_.snapshot();if(!bounds)return false;
    const auto x=event.local_position ? event.local_position->x : event.x-bounds->bounds.x;
    const auto y=event.local_position ? event.local_position->y : event.y-bounds->bounds.y;
    if(event.kind==K::pointer_down)pressure_handle_=native::editor::pressure_point(x,bounds->bounds.width);
    if(event.kind==K::focus_lost || event.kind==K::pointer_leave || event.kind==K::pointer_cancel){pressure_handle_=-1;return false;}
    if(pressure_handle_<0 || (event.kind!=K::pointer_down && event.kind!=K::pointer_move && event.kind!=K::pointer_up))return false;
    const auto value=native::editor::pressure_value(y,bounds->bounds.height);
    if(pressure_handle_==0)editor_->set_pressure_min(value);else if(pressure_handle_==1)editor_->set_pressure_mid(value);else editor_->set_pressure_max(value);
    if(event.kind==K::pointer_up)pressure_handle_=-1;
    return true;
}
void BrushPanelActions::choose_texture() { editor_->send(Action::choose_texture); }
void BrushPanelActions::set_background() {
    editor_->parameters->set_background(argb(editor_->current_color.get()));
    if(editor_->brush_changed)editor_->brush_changed();
}
void BrushPanelActions::reset() { editor_->reset_parameters(); }
void BrushPanelActions::save() { editor_->save_parameters(); }

ToolPanelBackend::ToolPanelBackend(std::shared_ptr<EditorBackend> value, bool expanded_value)
    : editor(std::move(value)), expanded(expanded_value), selected_tool(editor->inspected_tool), preset(editor->preset),
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
      gradient_revision(editor->gradient_settings->revision),
      gradient_panel(std::make_shared<GradientPanelModel>(editor)),
      pressure_curve(std::make_shared<PressureCurveModel>(editor)),
      brush_actions(std::make_shared<BrushPanelActions>(editor)) {
    for(const auto& spec:native::ToolSettings::specs())
        rows_.emplace(std::string(spec.key),std::make_shared<ToolParameterRow>(editor,spec));
    pressure_row.set(row("pressure-opacity-enabled"));
    absolute_spacing_row.set(row("absolute-spacing"));
    tool_lease_=selected_tool.observe([this, lifetime=std::weak_ptr(lifetime_)](const auto&,auto){
        if(lifetime.expired())return;
        (void)scroll_.set_scroll_offset(0);
        if(lifetime.expired())return;
        refresh_design();
    });
    refresh_design();
}
std::shared_ptr<ToolParameterRow> ToolPanelBackend::row(const std::string& key) const {
    auto found=rows_.find(key);
    if(found==rows_.end()) throw std::out_of_range("Unknown tool parameter: "+key);
    return found->second;
}
void ToolPanelBackend::refresh_design() {
    std::vector<std::shared_ptr<ToolPanelDesignRow>> next;
    if (const auto* panel=native::tool_panels::find(selected_tool.get())) {
        next.reserve(panel->fields.size());
        std::size_t pending_section = panel->fields.size();
        for (std::size_t i=0;i<panel->fields.size();++i) {
            const auto& field = panel->fields[i];
            if (is_paint_brush(std::string(panel->tool))) {
                if (field.kind == native::tool_panels::FieldKind::section) { pending_section = i; continue; }
                if (!variant_field(panel->tool, field.label)) continue;
                if (pending_section < panel->fields.size()) {
                    next.push_back(std::make_shared<ToolPanelDesignRow>(editor,std::string(panel->tool),panel->fields[pending_section],pending_section));
                    pending_section = panel->fields.size();
                }
            }
            next.push_back(std::make_shared<ToolPanelDesignRow>(editor,std::string(panel->tool),field,i));
        }
    }
    design_rows.set(std::move(next));
}
bool ToolPanelBackend::is_shape(const std::string& tool) const { return is_shape_tool(tool); }
bool ToolPanelBackend::is_selection(const std::string& tool) const { return is_selection_tool(tool); }
bool ToolPanelBackend::is_brush(const std::string& tool) const {
    return is_paint_brush(tool);
}
std::string ToolPanelBackend::heading(const std::string& tool) const {
    const auto* panel=native::tool_panels::find(tool);
    return panel ? std::string(panel->title) : std::string{};
}
std::string ToolPanelBackend::icon(const std::string& tool) const { return tool; }
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
void ToolPanelBackend::choose_texture(){brush_actions.get()->choose_texture();}
void ToolPanelBackend::set_background(){brush_actions.get()->set_background();}
void ToolPanelBackend::reset(){brush_actions.get()->reset();}
void ToolPanelBackend::save(){brush_actions.get()->save();}
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
    pressure_curve.get()->attach(curve);scroll_=scroll;tilt_=tilt;smoothing_=smoothing;
}
void ToolPanelBackend::curve_resized(ii::gui::Rect bounds){pressure_curve.get()->resized(bounds);}
ii::core::Result<bool> ToolPanelBackend::curve_input(const ii::gui::WidgetInput& event){
    return pressure_curve.get()->input(event);
}
void ToolPanelBackend::reveal(ii::gui::Widget target){const auto viewport=scroll_.snapshot(),item=target.snapshot();if(!viewport||!item)return;(void)scroll_.set_scroll_offset(std::max(0.,double(viewport->scroll_offset+item->bounds.y-viewport->bounds.y)));}
void ToolPanelBackend::reveal_tilt(){reveal(tilt_);}
void ToolPanelBackend::reveal_smoothing(){reveal(smoothing_);}
} // namespace congregation::ui
