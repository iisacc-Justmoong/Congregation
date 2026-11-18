#include "ToolPanelDesignRow.h"
#include "Workspace/EditorBackend.h"

#include <iiSDKCore>
#include <algorithm>
#include <array>
#include <cmath>

namespace congregation::ui {
namespace {
using P = native::ToolParameter;
using Kind = native::tool_panels::FieldKind;

std::string kind_name(Kind kind) {
    switch (kind) {
    case Kind::section: return "section";
    case Kind::value: return "value";
    case Kind::slider: return "slider";
    case Kind::choice: return "choice";
    case Kind::checkbox: return "checkbox";
    }
    return {};
}
bool member(std::string_view value, std::initializer_list<std::string_view> names) {
    return std::ranges::find(names, value) != names.end();
}
std::string number(double value, std::string_view unit) {
    return ii::core::format_decimal(std::round(value)) + std::string(unit);
}
}

ToolPanelDesignRow::Binding ToolPanelDesignRow::binding_for(
        std::string_view tool, const native::tool_panels::Field& field, std::optional<P>& parameter) {
    const auto label = field.label;
    const bool painting = member(tool, {"pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush", "eraser", "smudge", "clone-stamp"});
    if (painting) {
        constexpr std::array pairs{
            std::pair{"Size", P::size}, std::pair{"Hardness", P::hardness},
            std::pair{"Spacing", P::spacing}, std::pair{"Opacity", P::opacity},
            std::pair{"Flow", P::flow}, std::pair{"Smoothing", P::stabilization},
            std::pair{"Minimum diameter", P::shape_minimum_diameter},
            std::pair{"Opacity minimum", P::minimum_opacity},
            std::pair{"Opacity maximum", P::maximum_opacity},
            std::pair{"Wetness", P::wetness}, std::pair{"Mix", P::mix},
            std::pair{"Load", P::load}, std::pair{"Dilution", P::dilution},
            std::pair{"Tip shape", P::tip_shape}, std::pair{"Blend mode", P::blend_mode},
            std::pair{"Symmetry", P::symmetry_mode}, std::pair{"Pixel-aligned edges", P::pencil_pixel_aligned},
            std::pair{"Auto erase", P::pencil_auto_erase}, std::pair{"Snap to pixel grid", P::pencil_snap},
            std::pair{"Spray radius", P::airbrush_radius}, std::pair{"Spray rate", P::airbrush_rate},
            std::pair{"Build up while stationary", P::airbrush_stationary},
            std::pair{"Grain amount", P::dry_grain_amount}, std::pair{"Grain size", P::dry_grain_size},
            std::pair{"Dryness", P::dry_dryness}, std::pair{"Bristle length", P::bristle_length},
            std::pair{"Bristle thickness", P::bristle_thickness}, std::pair{"Bristle stiffness", P::bristle_stiffness},
            std::pair{"Bristle spread", P::bristle_spread}, std::pair{"Sample all layers", P::mixer_sample_all},
            std::pair{"Clean after each stroke", P::mixer_clean_after}, std::pair{"Load after each stroke", P::mixer_load_after}};
        for (const auto& [name, value] : pairs) if (label == name && field.kind != Kind::section) {
            const bool brush_variant = member(tool, {"pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush"});
            if (static_cast<std::size_t>(value) >= static_cast<std::size_t>(P::tip_shape) &&
                static_cast<std::size_t>(value) <= static_cast<std::size_t>(P::mixer_load_after) && !brush_variant) continue;
            if ((value == P::mixer_sample_all || value == P::mixer_clean_after || value == P::mixer_load_after) && tool != "mixer-brush") continue;
            parameter = value; return Binding::parameter;
        }
        if (label == "Pressure controls opacity") return Binding::pressure_opacity;
    }
    const bool selection = member(tool, {"rect-marquee", "ellipse-marquee", "lasso", "polygon-lasso", "magnetic-lasso", "magic-wand"});
    if (selection && label == "Feather") return Binding::selection_feather;
    if (selection && label == "Anti-alias") return Binding::selection_antialias;
    if (tool == "magnetic-lasso" && label == "Detection width") return Binding::magnetic_radius;
    if (tool == "magic-wand") {
        if (label == "Tolerance") return Binding::wand_tolerance;
        if (label == "Contiguous") return Binding::wand_contiguous;
        if (label == "Sample from") return Binding::wand_sample;
    }
    if (tool == "paint-bucket") {
        if (label == "Tolerance") return Binding::fill_tolerance;
        if (label == "Contiguous") return Binding::fill_contiguous;
        if (label == "Anti-alias") return Binding::fill_antialias;
        if (label == "Sample from") return Binding::fill_sample;
        if (label == "Preserve transparency") return Binding::fill_preserve;
    }
    if (tool == "clone-stamp") {
        if (label == "Aligned") return Binding::clone_aligned;
        if (label == "Sample from") return Binding::clone_sample;
    }
    if (tool == "crop" || tool == "perspective-crop") {
        if (label == "Aspect ratio") return Binding::crop_aspect;
        if (tool == "crop" && label == "Delete cropped pixels") return Binding::crop_destructive;
    }
    if (member(tool, {"polygon", "rectangle", "ellipse"})) {
        if (tool == "polygon" && label == "Sides") return Binding::shape_sides;
        if (label == "Rotation") return Binding::shape_rotation;
        if (label == "Draw from center") return Binding::shape_center;
        if (label == "Fill") return Binding::shape_fill;
        if (label == "Stroke") return Binding::shape_outline;
        if (label == "Stroke width") return Binding::shape_stroke;
        if (label == "Opacity") return Binding::shape_opacity;
    }
    if (tool == "gradient") {
        if (label == "Type") return Binding::gradient_type;
        if (label == "Repeat") return Binding::gradient_repeat;
        if (label == "Blend mode") return Binding::gradient_blend;
    }
    if (tool == "rulers" && label == "Show rulers") return Binding::rulers_visible;
    return Binding::none;
}

ToolPanelDesignRow::ToolPanelDesignRow(std::shared_ptr<EditorBackend> editor, std::string tool,
                                     const native::tool_panels::Field& field, std::size_t index)
    : id(tool + ":" + std::to_string(index)), label(field.label), kind(kind_name(field.kind)),
      value_label(std::string(field.value)), position(field.position), checked(field.checked),
      editor_(std::move(editor)), tool_(std::move(tool)), binding_(binding_for(tool_, field, parameter_)) {
    editable = binding_ != Binding::none;
    if (binding_ == Binding::parameter && native::ToolSettings::spec(*parameter_).dynamics_choice) {
        const auto& spec = native::ToolSettings::spec(*parameter_);
        parameter_choice = true;
        for (int i=int(spec.minimum); i<=int(spec.maximum); ++i)
            options.push_back(native::ToolSettings::option_label(*parameter_, i));
    }
    switch (binding_) {
    case Binding::parameter: watch(editor_->parameters->parameter_revision(*parameter_)); break;
    case Binding::pressure_opacity: watch(editor_->pressure_enabled); break;
    case Binding::selection_feather: watch(editor_->selection_feather); break;
    case Binding::selection_antialias: watch(editor_->selection_antialias); break;
    case Binding::magnetic_radius: watch(editor_->magnetic_radius); break;
    case Binding::wand_tolerance: watch(editor_->wand_tolerance); break;
    case Binding::wand_contiguous: watch(editor_->wand_contiguous); break;
    case Binding::wand_sample: watch(editor_->wand_current_layer); break;
    case Binding::fill_tolerance: watch(editor_->fill_tolerance); break;
    case Binding::fill_contiguous: watch(editor_->fill_contiguous); break;
    case Binding::fill_antialias: watch(editor_->fill_antialias); break;
    case Binding::fill_sample: watch(editor_->fill_current_layer); break;
    case Binding::fill_preserve: watch(editor_->fill_preserve_alpha); break;
    case Binding::clone_aligned: watch(editor_->clone_aligned); break;
    case Binding::clone_sample: watch(editor_->clone_current_layer); break;
    case Binding::crop_aspect: watch(editor_->crop_aspect); break;
    case Binding::crop_destructive: watch(editor_->crop_destructive); break;
    case Binding::shape_sides: watch(editor_->polygon_sides); break;
    case Binding::shape_rotation: watch(editor_->polygon_rotation); break;
    case Binding::shape_center: watch(editor_->polygon_center); break;
    case Binding::shape_fill: watch(editor_->polygon_fill); break;
    case Binding::shape_outline: watch(editor_->polygon_outline); break;
    case Binding::shape_stroke: watch(editor_->polygon_stroke_width); break;
    case Binding::shape_opacity: watch(editor_->polygon_opacity); break;
    case Binding::gradient_type: watch(editor_->gradient_settings->field_revision(native::GradientSettings::Field::kind)); break;
    case Binding::gradient_repeat: watch(editor_->gradient_settings->field_revision(native::GradientSettings::Field::spread)); break;
    case Binding::gradient_blend: watch(editor_->gradient_settings->field_revision(native::GradientSettings::Field::blend)); break;
    case Binding::rulers_visible: watch(editor_->rulers_visible); break;
    case Binding::none: break;
    }
    refresh();
}

void ToolPanelDesignRow::refresh() {
    const auto keep_alive = weak_from_this().lock();
    ++refresh_count_;
    const auto numeric = [&](double value, double low, double high, std::string_view unit) {
        value_label.set(number(value, unit)); position.set(std::clamp((value-low)/(high-low), 0., 1.));
    };
    switch (binding_) {
    case Binding::parameter:
        value_label.set(editor_->parameters->label(*parameter_)); position.set(editor_->parameters->normalized(*parameter_));
        checked.set(editor_->parameters->value(*parameter_) != 0);
        if (parameter_choice) choice_index.set(int(editor_->parameters->value(*parameter_) - native::ToolSettings::spec(*parameter_).minimum));
        break;
    case Binding::pressure_opacity: checked.set(editor_->pressure_enabled.get()); break;
    case Binding::selection_feather: numeric(editor_->selection_feather.get(),0,32," px"); break;
    case Binding::selection_antialias: checked.set(editor_->selection_antialias.get()); break;
    case Binding::magnetic_radius: numeric(editor_->magnetic_radius.get(),2,64," px"); break;
    case Binding::wand_tolerance: numeric(editor_->wand_tolerance.get()*100,0,100,"%"); break;
    case Binding::wand_contiguous: checked.set(editor_->wand_contiguous.get()); break;
    case Binding::wand_sample: value_label.set(editor_->wand_current_layer.get()?"Current layer":"All layers"); break;
    case Binding::fill_tolerance: numeric(editor_->fill_tolerance.get()*100,0,100,"%"); break;
    case Binding::fill_contiguous: checked.set(editor_->fill_contiguous.get()); break;
    case Binding::fill_antialias: checked.set(editor_->fill_antialias.get()); break;
    case Binding::fill_sample: value_label.set(editor_->fill_current_layer.get()?"Current layer":"All layers"); break;
    case Binding::fill_preserve: checked.set(editor_->fill_preserve_alpha.get()); break;
    case Binding::clone_aligned: checked.set(editor_->clone_aligned.get()); break;
    case Binding::clone_sample: value_label.set(editor_->clone_current_layer.get()?"Current layer":"Current and below"); break;
    case Binding::crop_aspect: {
        const auto value=editor_->crop_aspect.get(); value_label.set(value==1?"1:1":value==2?"4:3":value==3?"16:9":"Unconstrained"); break;
    }
    case Binding::crop_destructive: checked.set(editor_->crop_destructive.get()); break;
    case Binding::shape_sides: value_label.set(std::to_string(editor_->polygon_sides.get())); break;
    case Binding::shape_rotation: numeric(editor_->polygon_rotation.get(),-180,180,"°"); break;
    case Binding::shape_center: checked.set(editor_->polygon_center.get()); break;
    case Binding::shape_fill: value_label.set(editor_->polygon_fill.get()?"Foreground":"None"); break;
    case Binding::shape_outline: value_label.set(editor_->polygon_outline.get()?"Foreground":"None"); break;
    case Binding::shape_stroke: numeric(editor_->polygon_stroke_width.get(),0,64," px"); break;
    case Binding::shape_opacity: numeric(editor_->polygon_opacity.get()*100,0,100,"%"); break;
    case Binding::gradient_type: value_label.set(editor_->gradient_settings->kind()==iiSharedCanvas::GradientKind::Radial?"Radial":"Linear"); break;
    case Binding::gradient_repeat: value_label.set(editor_->gradient_settings->spread()==iiSharedCanvas::GradientSpread::Repeat?"Repeat":"None"); break;
    case Binding::gradient_blend: {
        const auto value=editor_->gradient_settings->blend();
        value_label.set(value==RasterBlendMode::Multiply?"Multiply":value==RasterBlendMode::Screen?"Screen":value==RasterBlendMode::Overlay?"Overlay":"Normal"); break;
    }
    case Binding::rulers_visible: checked.set(editor_->rulers_visible.get()); break;
    case Binding::none: break;
    }
}

void ToolPanelDesignRow::move(double value) {
    if (!editable) return;
    value=std::clamp(value,0.,1.);
    switch (binding_) {
    case Binding::parameter: editor_->set_parameter_normalized(*parameter_,value); break;
    case Binding::selection_feather: editor_->selection_feather.set(value*32); break;
    case Binding::magnetic_radius: editor_->magnetic_radius.set(2+value*62); break;
    case Binding::wand_tolerance: editor_->wand_tolerance.set(value); break;
    case Binding::fill_tolerance: editor_->fill_tolerance.set(value); break;
    case Binding::shape_opacity: editor_->polygon_opacity.set(value); break;
    default: break;
    }
}
void ToolPanelDesignRow::edit(std::string text) {
    if (!editable) return;
    const auto first=text.find_first_not_of(" \t");
    if(first==std::string::npos)return;
    const auto end=text.find_first_not_of("-+0123456789.eE",first);
    const auto parsed=ii::core::parse_decimal(std::string_view(text).substr(first,end-first));
    if(!parsed)return;
    const auto value=*parsed;
    switch (binding_) {
    case Binding::parameter: { const auto& spec=native::ToolSettings::spec(*parameter_); editor_->set_parameter(*parameter_,std::clamp(value,spec.minimum,spec.maximum)); break; }
    case Binding::selection_feather: editor_->selection_feather.set(std::clamp(value,0.,32.)); break;
    case Binding::magnetic_radius: editor_->magnetic_radius.set(std::clamp(value,2.,64.)); break;
    case Binding::wand_tolerance: editor_->wand_tolerance.set(std::clamp(value/100.,0.,1.)); break;
    case Binding::fill_tolerance: editor_->fill_tolerance.set(std::clamp(value/100.,0.,1.)); break;
    case Binding::shape_sides: editor_->polygon_sides.set(static_cast<std::int64_t>(std::clamp(value,3.,64.))); break;
    case Binding::shape_rotation: editor_->polygon_rotation.set(std::clamp(value,-180.,180.)); break;
    case Binding::shape_stroke: editor_->polygon_stroke_width.set(std::clamp(value,0.,64.)); break;
    case Binding::shape_opacity: editor_->polygon_opacity.set(std::clamp(value/100.,0.,1.)); break;
    default: break;
    }
}
void ToolPanelDesignRow::toggle(bool value) {
    if (!editable) return;
    switch (binding_) {
    case Binding::parameter:
        if (native::ToolSettings::spec(*parameter_).checkbox) editor_->set_parameter(*parameter_,value ? 1 : 0);
        break;
    case Binding::pressure_opacity: editor_->set_pressure_enabled(value); break;
    case Binding::selection_antialias: editor_->selection_antialias.set(value); break;
    case Binding::wand_contiguous: editor_->wand_contiguous.set(value); break;
    case Binding::fill_contiguous: editor_->fill_contiguous.set(value); break;
    case Binding::fill_antialias: editor_->fill_antialias.set(value); break;
    case Binding::fill_preserve: editor_->fill_preserve_alpha.set(value); break;
    case Binding::clone_aligned: editor_->clone_aligned.set(value); break;
    case Binding::crop_destructive: editor_->crop_destructive.set(value); break;
    case Binding::shape_center: editor_->polygon_center.set(value); break;
    case Binding::rulers_visible: editor_->rulers_visible.set(value); break;
    default: break;
    }
}
void ToolPanelDesignRow::activate() {
    if (!editable) return;
    switch (binding_) {
    case Binding::parameter: {
        const auto& spec = native::ToolSettings::spec(*parameter_);
        if (spec.dynamics_choice) {
            const auto next = editor_->parameters->value(*parameter_) + 1;
            editor_->set_parameter(*parameter_, next > spec.maximum ? spec.minimum : next);
        }
        break;
    }
    case Binding::wand_sample: editor_->wand_current_layer.set(!editor_->wand_current_layer.get()); break;
    case Binding::fill_sample: editor_->fill_current_layer.set(!editor_->fill_current_layer.get()); break;
    case Binding::clone_sample: editor_->clone_current_layer.set(!editor_->clone_current_layer.get()); break;
    case Binding::crop_aspect: editor_->crop_aspect.set((editor_->crop_aspect.get()+1)%4); break;
    case Binding::shape_fill: editor_->polygon_fill.set(!editor_->polygon_fill.get()); break;
    case Binding::shape_outline: editor_->polygon_outline.set(!editor_->polygon_outline.get()); break;
    case Binding::gradient_type: editor_->gradient_settings->set_radial(editor_->gradient_settings->kind()!=iiSharedCanvas::GradientKind::Radial); break;
    case Binding::gradient_repeat: editor_->gradient_settings->set_repeat(editor_->gradient_settings->spread()!=iiSharedCanvas::GradientSpread::Repeat); break;
    case Binding::gradient_blend: editor_->gradient_settings->cycle_blend(); break;
    default: break;
    }
}
void ToolPanelDesignRow::choose(int index) {
    if (!editable || !parameter_choice || index < 0 || std::size_t(index) >= options.size()) return;
    editor_->set_parameter(*parameter_,native::ToolSettings::spec(*parameter_).minimum + index);
}
} // namespace congregation::ui
