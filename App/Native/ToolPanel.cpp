#include "ToolPanel.h"
#include "Style.h"
#include "ToolSettings.h"
#include "EditorLayout.h"
#include "OwnedWidgetStyle.h"
#include <LVUI/IconButtonDefaults.generated.h>
#include <LVUI/SegmentedControl.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
#include <iiSDKCore/Core/Text/Number.h>

namespace congregation::native
{
namespace { bool shape_tool(const std::string& value) { return value == "polygon" || value == "rectangle" || value == "ellipse"; } bool area_selection(const std::string& value) { return value == "rect-marquee" || value == "ellipse-marquee" || value == "lasso" || value == "polygon-lasso" || value == "magnetic-lasso" || value == "magic-wand"; } }
ii::gui::Widget build_tool_panel(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent,
                                 std::shared_ptr<congregation::ui::EditorBackend> vm, bool expanded)
{
    using namespace owned_widgets;
    using P = ToolParameter;
    Builder b(raw, "tool/");
    constexpr auto panel_spacing = Metric::space4, panel_padding = Metric::space4;
    constexpr auto parameter_spacing = Metric::space2, parameter_inset = Metric::space2;
    constexpr auto section_spacing = Metric::space16;
    constexpr auto content_padding = Metric::space4, group_heading_height = Metric::size30;
    constexpr auto content_width = Metric::inspectorWidth - 2 * (panel_padding + content_padding);
    const auto footer_height = lvui::icon_button_defaults::extent +
                               2 * lvui::icon_segmented_configuration().vertical_padding + 2 * panel_padding;
    constexpr auto parameter_slider_size = lvui::SliderSize::regular;
    lvui::SliderConfiguration slider_defaults;
    lvui::apply_slider_size(slider_defaults, parameter_slider_size);
    const auto parameter_height = lvui::icon_button_defaults::extent + slider_defaults.height +
                                  Metric::textCaptionLineHeight + 2 * parameter_spacing + 2 * parameter_inset;
    const auto available_specs = ToolSettings::specs();
    std::vector<ParameterSpec> specs(available_specs.begin(), available_specs.end());
    const auto group_order = [](std::string_view group) {
        constexpr std::string_view groups[]{"Brush", "Shape Dynamics", "Scattering", "Texture", "Color Dynamics",
                                             "Transfer", "Brush Pose", "Noise", "Wet Edges", "Build-up", "Smoothing",
                                             "Tip & spacing", "Dynamics", "Paint & mixing", "Taper & limits"};
        const auto found = std::ranges::find(groups, group);
        return std::distance(std::begin(groups), found);
    };
    const auto within_group = [](ToolParameter id) {
        using enum ToolParameter;
        constexpr ToolParameter order[]{
            size, flow, opacity, hardness, spacing, stabilization,
            shape_size_jitter, shape_size_control, shape_minimum_diameter, shape_angle_jitter,
            shape_angle_control, shape_roundness_jitter, shape_roundness_control, shape_minimum_roundness,
            shape_flip_x, shape_flip_y, shape_project,
            scatter, scatter_both_axes, scatter_control, scatter_count, scatter_count_jitter, scatter_count_control,
            texture_invert, texture_scale, texture_brightness, texture_contrast, texture_each_tip, texture_mode,
            texture_depth, texture_minimum_depth, texture_depth_jitter, texture_depth_control,
            color_apply_per_tip, color_mix_jitter, color_mix_control, color_hue_jitter,
            color_saturation_jitter, color_brightness_jitter, color_purity,
            minimum_opacity, maximum_opacity, transfer_flow_jitter, transfer_flow_control, transfer_flow_minimum,
            transfer_wetness_jitter, transfer_wetness_control, transfer_wetness_minimum,
            transfer_mix_jitter, transfer_mix_control, transfer_mix_minimum,
            pose_tilt_x, pose_override_tilt_x, pose_tilt_y, pose_override_tilt_y,
            pose_rotation, pose_override_rotation, pose_pressure, pose_override_pressure,
            noise_enabled, noise_amount, noise_grain_size, wet_edges_enabled, wet_edges_strength,
            wet_edges_width, wet_edges_diffusion, build_up_enabled, build_up_rate, build_up_limit, build_up_time,
            smoothing_enabled, smoothing_method, post, smoothing_catch_up};
        return std::distance(std::begin(order), std::ranges::find(order, id));
    };
    std::ranges::stable_sort(specs, [&](const auto& a, const auto& b) {
        const auto ga = group_order(a.group), gb = group_order(b.group);
        return ga != gb ? ga < gb : within_group(a.id) < within_group(b.id);
    });
    std::size_t group_count = 0;
    std::string_view previous_group;
    for (const auto &spec : specs)
        if (spec.group != previous_group)
        {
            ++group_count;
            previous_group = spec.group;
        }
    const auto expanded_content_height = 2 * content_padding + group_count * group_heading_height +
                                         (group_count ? group_count - 1 : 0) * section_spacing +
                                         specs.size() * parameter_height;
    const auto expanded_height = Metric::toolHeadingHeight + expanded_content_height + footer_height +
                                 2 * panel_spacing + 2 * panel_padding;
    const auto root = b.column(parent, "tool-panel", 0, expanded ? expanded_height : 0,
                               panel_spacing, panel_padding);
    if (!expanded)
        good(root.set_flex(1));
    borders(root, false, false, true);
    auto bindings = std::make_shared<ii::gui::BindingScope>();
    good(raw.retain(bindings));
    const auto head = b.column(root, "tool-heading", 0, Metric::toolHeadingHeight, Metric::space4,
                               Metric::space4);
    auto title = b.row(head);
    const auto heading = b.label(title, "Brush settings", Metric::textHeader, {}, Metric::weightSemiBold);
    b.space(title);
    const auto count_label = b.label(title, std::to_string(specs.size()) + " controls", Metric::textCaption);
    good(bindings->bind(heading, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_text((value == "crop" || value == "perspective-crop") ? "Crop" : value == "magic-wand" ? "Magic wand" : value == "clone-stamp" ? "Clone stamp" : value == "polygon" ? "Polygon" : value == "rectangle" ? "Rectangle" : value == "ellipse" ? "Ellipse" : area_selection(value) ? "Selection" : value == "gradient" ? "Gradient" : value == "paint-bucket" ? "Paint bucket" : "Brush settings"); }));
    good(bindings->bind(count_label, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_text((value == "crop" || value == "perspective-crop") ? "Crop and aspect" : value == "magic-wand" ? "5 controls" : value == "polygon" ? "7 controls" : shape_tool(value) ? "5 controls" : area_selection(value) ? (value == "magnetic-lasso" ? "3 controls" : "2 controls") : value == "gradient" ? "Stops and modes" : value == "paint-bucket" ? "6 controls" : std::to_string(ToolSettings::specs().size()) + " controls"); }));
    auto preset = take(b.controls.combo_box(
        head, "brush-preset",
        {.text = vm->preset.get(), .arrow = ii::gui::StepperArrow::down, .width = content_width},
        {.clicked = [vm] { vm->choose_preset(); }}));
    good(bind_text(*bindings, preset, vm->preset));
    const auto hint = b.label(head, "Drag to adjust · − / + for fine steps", Metric::textCaption);
    good(bindings->bind(preset, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value != "crop" && value != "perspective-crop" && !area_selection(value) && value != "magic-wand" && value != "gradient" && value != "paint-bucket" && !shape_tool(value)); }));
    good(bindings->bind(hint, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value != "crop" && value != "perspective-crop" && !area_selection(value) && value != "magic-wand" && value != "gradient" && value != "paint-bucket" && !shape_tool(value)); }));
    borders(head, false, false, true);
    const auto scroll =
        take(raw.scroll_view(root, "tool-scroll",
                             {.width = Metric::inspectorWidth - 2 * panel_padding,
                              .height = expanded ? expanded_content_height
                                                 : double(Metric::toolScrollHeight),
                              .enabled = !expanded,
                              .show_indicator = !expanded}));
    if (!expanded)
        good(scroll.set_flex(1));
    const auto body = b.column(scroll, "tool-content", 0, 0, Metric::space0, Metric::space0);
    const auto clone = b.column(body, "clone-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(clone,vm->selected_tool,[](Widget w,const std::string& value){return w.set_visible(value=="clone-stamp");}));
    b.label(clone,"Alt-click to set source",Metric::textCaption);
    const auto aligned=take(b.controls.check_box(clone,"clone-aligned",{.text="Aligned",.checked=vm->clone_aligned.get()},[vm](bool v){vm->clone_aligned.set(v);}));
    good(bind_checked(*bindings,aligned,vm->clone_aligned));
    const auto cloneReference=take(b.controls.check_box(clone,"clone-current-layer",{.text="Current layer only",.checked=vm->clone_current_layer.get()},[vm](bool v){vm->clone_current_layer.set(v);}));
    good(bind_checked(*bindings,cloneReference,vm->clone_current_layer));
    const auto content = b.column(body, "tool-parameters", 0, 0, Metric::space0, content_padding);
    const auto polygon = b.column(body, "polygon-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(polygon, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(shape_tool(value)); }));
    const auto sides_row=b.row(polygon,{},0,Metric::size30);b.label(sides_row,"Sides",Metric::textDescription,{},Metric::weightSemiBold);b.space(sides_row);
    const auto sides_value=b.label(sides_row,"5",Metric::textHeader);
    good(bindings->bind(sides_value,vm->polygon_sides,[](Widget w,const std::int64_t& value){return w.set_text(std::to_string(value));}));
    const auto sides=take(b.controls.slider(polygon,"polygon-sides",{.minimum=3,.maximum=64,.value=double(vm->polygon_sides.get()),.width=content_width,.accessibility_name="Polygon sides"},[vm](double value){vm->polygon_sides.set(std::int64_t(std::lround(std::clamp(value,3.,64.))));}));
    good(bindings->bind(sides,vm->polygon_sides,[sides](Widget w,const std::int64_t& value){return sides.set_value(double(value));}));
    const auto rotation_row=b.row(polygon,{},0,Metric::size30);b.label(rotation_row,"Rotation",Metric::textDescription,{},Metric::weightSemiBold);b.space(rotation_row);
    const auto rotation_value=b.label(rotation_row,"0°",Metric::textHeader);
    good(bindings->bind(rotation_value,vm->polygon_rotation,[](Widget w,const double& value){return w.set_text(ii::core::format_decimal(std::round(value))+"°");}));
    const auto rotation=take(b.controls.slider(polygon,"polygon-rotation",{.minimum=-180,.maximum=180,.value=vm->polygon_rotation.get(),.width=content_width,.accessibility_name="Polygon rotation"},[vm](double value){vm->polygon_rotation.set(std::clamp(value,-180.,180.));}));
    good(bindings->bind(rotation,vm->polygon_rotation,[rotation](Widget w,const double& value){return rotation.set_value(value);}));
    for(auto control:{sides_row,control_widget(sides),rotation_row,control_widget(rotation)})
        good(bindings->bind(control,vm->selected_tool,[](Widget w,const std::string& value){return w.set_visible(value=="polygon");}));
    const auto center=take(b.controls.check_box(polygon,"polygon-center",{.text="Draw from center",.checked=vm->polygon_center.get()},[vm](bool value){vm->polygon_center.set(value);}));
    good(bind_checked(*bindings,center,vm->polygon_center));
    const auto polygon_fill_control=take(b.controls.check_box(polygon,"polygon-fill",{.text="Fill",.checked=vm->polygon_fill.get()},[vm](bool value){vm->polygon_fill.set(value);}));
    good(bind_checked(*bindings,polygon_fill_control,vm->polygon_fill));
    const auto outline=take(b.controls.check_box(polygon,"polygon-outline",{.text="Outline",.checked=vm->polygon_outline.get()},[vm](bool value){vm->polygon_outline.set(value);}));
    good(bind_checked(*bindings,outline,vm->polygon_outline));
    const auto width_row=b.row(polygon,{},0,Metric::size30);b.label(width_row,"Outline width",Metric::textDescription,{},Metric::weightSemiBold);b.space(width_row);
    const auto width_value=b.label(width_row,"1 px",Metric::textHeader);
    good(bindings->bind(width_value,vm->polygon_stroke_width,[](Widget w,const double& value){return w.set_text(ii::core::format_decimal(std::round(value))+" px");}));
    const auto width=take(b.controls.slider(polygon,"polygon-stroke-width",{.minimum=0,.maximum=64,.value=vm->polygon_stroke_width.get(),.width=content_width,.accessibility_name="Shape outline width"},[vm](double value){vm->polygon_stroke_width.set(std::clamp(value,0.,64.));}));
    good(bindings->bind(width,vm->polygon_stroke_width,[width](Widget w,const double& value){return width.set_value(value);}));
    const auto opacity_row=b.row(polygon,{},0,Metric::size30);b.label(opacity_row,"Opacity",Metric::textDescription,{},Metric::weightSemiBold);b.space(opacity_row);
    const auto opacity_value=b.label(opacity_row,"100%",Metric::textHeader);
    good(bindings->bind(opacity_value,vm->polygon_opacity,[](Widget w,const double& value){return w.set_text(ii::core::format_decimal(std::round(value*100))+"%");}));
    const auto polygon_opacity=take(b.controls.slider(polygon,"polygon-opacity",{.minimum=0,.maximum=1,.value=vm->polygon_opacity.get(),.width=content_width,.accessibility_name="Shape opacity"},[vm](double value){vm->polygon_opacity.set(std::clamp(value,0.,1.));}));
    good(bindings->bind(polygon_opacity,vm->polygon_opacity,[polygon_opacity](Widget w,const double& value){return polygon_opacity.set_value(value);}));
    b.label(polygon,"Shift: constrain proportions",Metric::textCaption);
    const auto selection = b.column(body, "selection-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(selection, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(area_selection(value)); }));
    const auto feather_row = b.row(selection, {}, 0, Metric::size30);
    b.label(feather_row, "Feather", Metric::textDescription, {}, Metric::weightSemiBold); b.space(feather_row);
    const auto feather_value = b.label(feather_row, "0 px", Metric::textHeader);
    good(bindings->bind(feather_value, vm->selection_feather, [](Widget w, const double& value) { return w.set_text(ii::core::format_decimal(std::round(value)) + " px"); }));
    const auto feather = take(b.controls.slider(selection, "selection-feather", {.minimum=0,.maximum=32,.value=vm->selection_feather.get(),.width=content_width,.accessibility_name="Selection feather radius"}, [vm](double value) { vm->selection_feather.set(std::clamp(value,0.,32.)); }));
    good(bindings->bind(feather, vm->selection_feather, [feather](Widget w, const double& value) { return feather.set_value(value); }));
    const auto antialias = take(b.controls.check_box(selection, "selection-antialias", {.text="Antialias",.checked=vm->selection_antialias.get()}, [vm](bool value) { vm->selection_antialias.set(value); }));
    good(bind_checked(*bindings, antialias, vm->selection_antialias));
    const auto radius_row = b.row(selection, "magnetic-radius-row", 0, Metric::size30);
    b.label(radius_row, "Edge radius", Metric::textDescription, {}, Metric::weightSemiBold); b.space(radius_row);
    const auto radius_value = b.label(radius_row, "12 px", Metric::textHeader);
    good(bindings->bind(radius_value, vm->magnetic_radius, [](Widget w, const double& value) { return w.set_text(ii::core::format_decimal(std::round(value)) + " px"); }));
    const auto radius = take(b.controls.slider(selection, "magnetic-radius", {.minimum=2,.maximum=64,.value=vm->magnetic_radius.get(),.width=content_width,.accessibility_name="Magnetic edge radius"}, [vm](double value) { vm->magnetic_radius.set(std::clamp(value,2.,64.)); }));
    good(bindings->bind(radius, vm->magnetic_radius, [radius](Widget w, const double& value) { return radius.set_value(value); }));
    good(bindings->bind(radius_row, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "magnetic-lasso"); }));
    good(bindings->bind(radius, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "magnetic-lasso"); }));
    b.label(selection, "Shift: add · Alt: subtract · Shift + Alt: intersect", Metric::textCaption);
    const auto crop = b.column(body, "crop-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(crop, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "crop" || value == "perspective-crop"); }));
    const auto aspect_row=b.row(crop,{},0,Metric::size30);b.label(aspect_row,"Aspect ratio",Metric::textDescription,{},Metric::weightSemiBold);b.space(aspect_row);
    const auto aspect=b.button(aspect_row,"crop-aspect","Free",[vm]{vm->crop_aspect.set((vm->crop_aspect.get()+1)%4);});
    good(bindings->bind(aspect,vm->crop_aspect,[aspect](Widget w,const std::int64_t& value) {return aspect.set_text(value==1 ? "1:1" : value==2 ? "4:3" : value==3 ? "16:9" : "Free");}));
    const auto cropHint=b.label(crop,"Drag a rectangle; move it or drag a corner",Metric::textCaption);
    const auto mode=b.button(crop,"crop-mode","Keep cropped pixels",[vm]{vm->crop_destructive.set(!vm->crop_destructive.get());});
    good(bindings->bind(mode,vm->selected_tool,[](Widget w,const std::string& value){return w.set_visible(value=="crop");}));
    good(bindings->bind(mode,vm->crop_destructive,[mode](Widget w,bool value){return mode.set_text(value ? "Delete cropped pixels" : "Keep cropped pixels");}));
    const auto normalHint=b.label(crop,"Non-destructive · applies to all frames",Metric::textCaption);
    good(bindings->bind(normalHint,vm->crop_destructive,[](Widget w,bool value){return w.set_text(value ? "Rasterizes all layers and removes replaced sources · all frames" : "Non-destructive · applies to all frames");}));
    const auto perspectiveHint=b.label(crop,"Rectifies and rasterizes layers · all frames",Metric::textCaption);
    good(bindings->bind(normalHint,vm->selected_tool,[](Widget w,const std::string& value){return w.set_visible(value=="crop");}));
    good(bindings->bind(perspectiveHint,vm->selected_tool,[](Widget w,const std::string& value){return w.set_visible(value=="perspective-crop");}));
    (void)cropHint;
    const auto crop_actions=b.row(crop,{},0,Metric::size30);
    b.button(crop_actions,"crop-apply","Apply",[vm]{vm->send(ui::Action::apply_crop);});
    b.button(crop_actions,"crop-cancel","Cancel",[vm]{vm->send(ui::Action::cancel_crop);});
    const auto wand = b.column(body, "wand-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(content, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value != "crop" && value != "perspective-crop" && !area_selection(value) && value != "magic-wand" && value != "gradient" && value != "paint-bucket" && !shape_tool(value)); }));
    good(bindings->bind(wand, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "magic-wand"); }));
    auto wand_row = b.row(wand, {}, 0, Metric::size30);
    b.label(wand_row, "Tolerance", Metric::textDescription, {}, Metric::weightSemiBold); b.space(wand_row);
    const auto wand_value = b.label(wand_row, "0%", Metric::textHeader);
    good(bindings->bind(wand_value, vm->wand_tolerance, [](Widget w, const double& value) { return w.set_text(ii::core::format_decimal(std::round(value * 100)) + "%"); }));
    const auto wand_slider = take(b.controls.slider(wand, "wand-tolerance", {.minimum=0,.maximum=1,.value=vm->wand_tolerance.get(),.width=content_width,.accessibility_name="Wand tolerance"}, [vm](double value) { vm->wand_tolerance.set(std::clamp(value, 0., 1.)); }));
    good(bindings->bind(wand_slider, vm->wand_tolerance, [wand_slider](Widget w, const double& value) { return wand_slider.set_value(value); }));
    const auto contiguous = take(b.controls.check_box(wand, "wand-contiguous", {.text="Contiguous",.checked=vm->wand_contiguous.get()}, [vm](bool value) { vm->wand_contiguous.set(value); }));
    good(bind_checked(*bindings, contiguous, vm->wand_contiguous));
    const auto reference = take(b.controls.check_box(wand, "wand-current-layer", {.text="Current layer only",.checked=vm->wand_current_layer.get()}, [vm](bool value) { vm->wand_current_layer.set(value); }));
    good(bind_checked(*bindings, reference, vm->wand_current_layer));
    b.label(wand, "Shift: add · Alt: subtract · Shift + Alt: intersect", Metric::textCaption);
    const auto fill = b.column(body, "fill-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(fill, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "paint-bucket"); }));
    auto fill_row = b.row(fill, {}, 0, Metric::size30);
    b.label(fill_row, "Tolerance", Metric::textDescription, {}, Metric::weightSemiBold); b.space(fill_row);
    const auto fill_value = b.label(fill_row, "0%", Metric::textHeader);
    good(bindings->bind(fill_value, vm->fill_tolerance, [](Widget w, const double& value) { return w.set_text(ii::core::format_decimal(std::round(value*100)) + "%"); }));
    const auto fill_slider = take(b.controls.slider(fill, "fill-tolerance", {.minimum=0,.maximum=1,.value=vm->fill_tolerance.get(),.width=content_width,.accessibility_name="Fill tolerance"}, [vm](double value) { vm->fill_tolerance.set(std::clamp(value,0.,1.)); }));
    good(bindings->bind(fill_slider, vm->fill_tolerance, [fill_slider](Widget w, const double& value) { return fill_slider.set_value(value); }));
    const auto fill_contiguous = take(b.controls.check_box(fill, "fill-contiguous", {.text="Contiguous",.checked=vm->fill_contiguous.get()}, [vm](bool value) { vm->fill_contiguous.set(value); }));
    good(bind_checked(*bindings, fill_contiguous, vm->fill_contiguous));
    const auto fill_antialias = take(b.controls.check_box(fill, "fill-antialias", {.text="Antialias boundary",.checked=vm->fill_antialias.get()}, [vm](bool value) { vm->fill_antialias.set(value); }));
    good(bind_checked(*bindings, fill_antialias, vm->fill_antialias));
    const auto fill_reference = take(b.controls.check_box(fill, "fill-current-layer", {.text="Current layer only",.checked=vm->fill_current_layer.get()}, [vm](bool value) { vm->fill_current_layer.set(value); }));
    good(bind_checked(*bindings, fill_reference, vm->fill_current_layer));
    const auto fill_target = take(b.controls.check_box(fill, "fill-target-current", {.text="Edit current layer",.checked=vm->fill_target_current.get()}, [vm](bool value) { vm->fill_target_current.set(value); }));
    good(bind_checked(*bindings, fill_target, vm->fill_target_current));
    const auto fill_alpha = take(b.controls.check_box(fill, "fill-preserve-alpha", {.text="Preserve layer alpha",.checked=vm->fill_preserve_alpha.get()}, [vm](bool value) { vm->fill_preserve_alpha.set(value); }));
    good(bind_checked(*bindings, fill_alpha, vm->fill_preserve_alpha));
    good(bindings->bind(fill_alpha, vm->fill_target_current, [fill_alpha](Widget, const bool& value) { return fill_alpha.set_enabled(value); }));
    b.label(fill, "Direct edits affect only the current frame", Metric::textCaption);
    const auto gradient = b.column(body, "gradient-parameters", 0, 0, Metric::space6, Metric::space4);
    good(bindings->bind(gradient, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value == "gradient"); }));
    const auto gs = vm->gradient_settings;
    const auto stop_heading = b.row(gradient, {}, 0, Metric::size30);
    const auto stop_label = b.label(stop_heading, "Stop 1 / 2", Metric::textDescription, {}, Metric::weightSemiBold);
    good(bindings->bind(stop_label, gs->revision, [gs](Widget w, const std::uint64_t&) { return w.set_text("Stop " + std::to_string(gs->selected()+1) + " / " + std::to_string(gs->stops().size())); }));
    b.space(stop_heading);
    b.button(stop_heading, "gradient-previous-stop", "‹", [gs] { if (gs->selected() > 0) gs->select(gs->selected()-1); }, Metric::size28);
    b.button(stop_heading, "gradient-next-stop", "›", [gs] { gs->select(gs->selected()+1); }, Metric::size28);
    b.button(stop_heading, "gradient-add-stop", "+", [gs] { gs->add(); }, Metric::size28);
    b.button(stop_heading, "gradient-remove-stop", "−", [gs] { gs->remove(); }, Metric::size28);
    const auto position_row = b.row(gradient, {}, 0, Metric::size22);
    b.label(position_row, "Position"); b.space(position_row);
    const auto position_label = b.label(position_row, "0%", Metric::textCaption);
    good(bindings->bind(position_label, gs->revision, [gs](Widget w, const std::uint64_t&) { return w.set_text(ii::core::format_decimal(std::round(gs->stops()[gs->selected()].position * 100)) + "%"); }));
    const auto position = take(b.controls.slider(gradient, "gradient-stop-position", {.minimum=0,.maximum=1,.value=gs->stops()[gs->selected()].position,.width=content_width,.accessibility_name="Gradient stop position"}, [gs](double value) { gs->set_position(value); }));
    good(bindings->bind(position, gs->revision, [gs, position](Widget, const std::uint64_t&) -> ii::core::Result<void> { if (auto v=position.set_value(gs->stops()[gs->selected()].position); !v) return v; return position.set_enabled(gs->selected()>0 && gs->selected()+1<gs->stops().size()); }));
    const auto color_row = b.row(gradient, {}, 0, Metric::size30);
    const auto color_label = b.label(color_row, "#000000", Metric::textDescription);
    good(bindings->bind(color_label, gs->revision, [gs](Widget w, const std::uint64_t&) { std::ostringstream value; value << '#' << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << (gs->stops()[gs->selected()].argb & 0xffffffU); return w.set_text(value.str()); }));
    b.space(color_row);
    b.button(color_row, "gradient-use-current-color", "Use picker color", [vm, gs] { const auto c=vm->current_color.get(); const auto channel=[](double v){return std::uint32_t(std::lround(std::clamp(v,0.,1.)*255));}; gs->set_color((channel(c.alpha)<<24)|(channel(c.red)<<16)|(channel(c.green)<<8)|channel(c.blue)); });
    const auto alpha_row = b.row(gradient, {}, 0, Metric::size22);
    b.label(alpha_row, "Stop opacity"); b.space(alpha_row);
    const auto alpha_label = b.label(alpha_row, "100%", Metric::textCaption);
    good(bindings->bind(alpha_label, gs->revision, [gs](Widget w, const std::uint64_t&) { return w.set_text(ii::core::format_decimal(std::round((gs->stops()[gs->selected()].argb >> 24) * 100. / 255)) + "%"); }));
    const auto alpha_slider = take(b.controls.slider(gradient, "gradient-stop-alpha", {.minimum=0,.maximum=1,.value=double(gs->stops()[gs->selected()].argb >> 24)/255,.width=content_width,.accessibility_name="Gradient stop opacity"}, [gs](double value) { gs->set_alpha(value); }));
    good(bindings->bind(alpha_slider, gs->revision, [gs, alpha_slider](Widget w, const std::uint64_t&) { return alpha_slider.set_value(double(gs->stops()[gs->selected()].argb >> 24)/255); }));
    const auto radial = take(b.controls.check_box(gradient, "gradient-radial", {.text="Radial",.checked=gs->kind()==iiSharedCanvas::GradientKind::Radial}, [gs](bool value) { gs->set_radial(value); }));
    good(bindings->bind(radial, gs->revision, [gs, radial](Widget w, const std::uint64_t&) { return radial.set_checked(gs->kind()==iiSharedCanvas::GradientKind::Radial); }));
    const auto repeat = take(b.controls.check_box(gradient, "gradient-repeat", {.text="Repeat",.checked=gs->spread()==iiSharedCanvas::GradientSpread::Repeat}, [gs](bool value) { gs->set_repeat(value); }));
    good(bindings->bind(repeat, gs->revision, [gs, repeat](Widget w, const std::uint64_t&) { return repeat.set_checked(gs->spread()==iiSharedCanvas::GradientSpread::Repeat); }));
    const auto blend_row = b.row(gradient, {}, 0, Metric::size30); b.label(blend_row, "Blend"); b.space(blend_row);
    const auto blend_label = b.label(blend_row, "Normal", Metric::textDescription);
    good(bindings->bind(blend_label, gs->revision, [gs](Widget w, const std::uint64_t&) { const auto mode=gs->blend(); return w.set_text(mode==RasterBlendMode::Multiply ? "Multiply" : mode==RasterBlendMode::Screen ? "Screen" : mode==RasterBlendMode::Overlay ? "Overlay" : "Normal"); }));
    b.button(blend_row, "gradient-blend", "Next", [gs] { gs->cycle_blend(); }, Metric::size60);
    const auto settings = vm->parameters;
    Widget tilt_parameter, smoothing_parameter;
    std::string_view group;
    for (const auto &spec : specs)
    {
        if (spec.group != group)
        {
            if (group == "Brush") {
                auto pressure = take(b.controls.check_box(content, "tool-pressure-opacity",
                    {.text="Pressure Opacity", .checked=vm->pressure_enabled.get()},
                    [vm](bool value) { vm->set_pressure_enabled(value); }));
                good(bind_checked(*bindings, pressure, vm->pressure_enabled));
                auto spacingRow = b.row(content, "tool-absolute-spacing-row");
                b.label(spacingRow, "Spacing"); b.space(spacingRow);
                auto spacingValue = b.label(spacingRow, vm->spacing_label.get(), Metric::textCaption);
                good(ii::gui::bind_text(*bindings, spacingValue, vm->spacing_label));
                auto spacingSlider = take(b.controls.slider(content, "tool-absolute-spacing",
                    {.minimum=0, .maximum=64, .value=vm->spacing_pixels.get(), .width=content_width,
                     .accessibility_name="Spacing", .size=lvui::SliderSize::mini},
                    [vm](double value) { vm->set_spacing_pixels(value); }));
                good(bindings->bind(spacingSlider, vm->spacing_pixels, [spacingSlider](Widget, double value) {
                    return spacingSlider.set_value(value);
                }));
                b.label(content, "Pressure Curve", Metric::textDescription);
                auto curve = take(b.controls.event_listener(content, "tool-pressure-curve", {content_width, Metric::size92}));
                auto handle = std::make_shared<int>(-1);
                const auto draw = [curve, vm] { editor::pressure_curve(curve, vm->pressure_min.get(), vm->pressure_mid.get(), vm->pressure_max.get()); };
                good(curve.set_layout_handler([draw](ii::gui::Rect) { draw(); }));
                for (auto* observable : {&vm->pressure_min, &vm->pressure_mid, &vm->pressure_max})
                    good(bindings->bind(curve, *observable, [draw](Widget, double) -> ii::core::Result<void> { draw(); return {}; }));
                good(curve.set_input_handler([curve, vm, handle](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    using K = ii::gui::WidgetInputKind;
                    auto bounds = curve.snapshot(); if (!bounds) return false;
                    if (event.kind == K::pointer_down) *handle = editor::pressure_point(event.x - bounds->bounds.x, bounds->bounds.width);
                    if (event.kind == K::focus_lost || event.kind == K::pointer_leave) { *handle = -1; return false; }
                    if (*handle >= 0 && (event.kind == K::pointer_down || event.kind == K::pointer_move || event.kind == K::pointer_up)) {
                        const auto value = editor::pressure_value(event.y - bounds->bounds.y, bounds->bounds.height);
                        if (*handle == 0) vm->set_pressure_min(value); else if (*handle == 1) vm->set_pressure_mid(value); else vm->set_pressure_max(value);
                        if (event.kind == K::pointer_up) *handle = -1;
                        return true;
                    }
                    return false;
                }));
                for (const auto& [name, observable] : std::initializer_list<std::pair<const char*, ii::core::Observable<std::string>*>>{
                         {"Pressure Minimum", &vm->pressure_min_label}, {"Pressure Center", &vm->pressure_mid_label}, {"Pressure Maximum", &vm->pressure_max_label}}) {
                    auto line = b.row(content); b.label(line, name, Metric::textCaption); b.space(line);
                    auto value = b.label(line, observable->get(), Metric::textCaption);
                    good(ii::gui::bind_text(*bindings, value, *observable));
                }
            }
            if (!group.empty())
                b.row(content, "section-gap-" + std::string(spec.key), 0, section_spacing);
            group = spec.group;
            auto row = b.row(content, {}, 0, group_heading_height);
            b.label(row, std::string(group), Metric::textDescription, {}, Metric::weightSemiBold);
            b.space(row);
            const auto count =
                std::ranges::count(specs, group, &ParameterSpec::group);
            b.label(row, std::to_string(count), Metric::textCaption);
            if (group == "Texture") {
                auto select = b.row(content, "texture-selector", 0, Metric::size30);
                b.label(select, "Texture"); b.space(select);
                auto choice = take(b.controls.combo_box(select, "choose-texture",
                    {.text = settings->texture_name.get(), .arrow = ii::gui::StepperArrow::down, .width = 2 * Metric::size60 + Metric::space16},
                    {.clicked = [vm] { vm->send(congregation::ui::Action::choose_texture); }}));
                good(bind_text(*bindings, choice, settings->texture_name));
                b.icon(select, "add-texture", "generaladd", [vm] { vm->send(congregation::ui::Action::choose_texture); });
            }
            if (group == "Color Dynamics") {
                auto bg = b.row(content, "background-color-row", 0, Metric::size30);
                b.label(bg, "Background color"); b.space(bg);
                b.button(bg, "set-background-color", "Use current color", [vm, settings] {
                    const auto c = vm->current_color.get();
                    const auto byte = [](double x) { return std::uint32_t(std::lround(std::clamp(x, 0., 1.) * 255)); };
                    settings->set_background((byte(c.alpha)<<24) | (byte(c.red)<<16) | (byte(c.green)<<8) | byte(c.blue));
                    if (vm->brush_changed) vm->brush_changed();
                });
            }
        }
        const auto key = std::string(spec.key);
        if (spec.dynamics_choice)
        {
            const auto row = b.row(content, "parameter-" + key, 0, Metric::size30);
            b.label(row, std::string(spec.name));
            b.space(row);
            const auto choice = take(b.controls.combo_box(
                row, "adjust-" + key,
                {.text = settings->label(spec.id), .arrow = ii::gui::StepperArrow::down,
                 .width = 2 * Metric::size60},
                {.clicked = [vm, settings, id = spec.id] {
                    const auto& s = ToolSettings::spec(id);
                    const double next = settings->value(id) >= s.maximum ? s.minimum : settings->value(id) + 1;
                    vm->set_parameter(id, next);
                }}));
            good(bindings->bind(choice, settings->values,
                                [settings, id = spec.id](Widget w, const ToolValues&)
                                { return w.set_text(settings->label(id)); }));
            continue;
        }
        if (spec.checkbox)
        {
            const auto checkbox = take(b.controls.check_box(
                content, key,
                {.text = std::string(spec.name), .checked = settings->value(spec.id) != 0},
                [vm, id = spec.id](bool checked) { vm->set_parameter_normalized(id, checked ? 1.0 : 0.0); }));
            good(bindings->bind(checkbox, settings->values,
                                [settings, id = spec.id, checkbox](Widget, const ToolValues&)
                                { return checkbox.set_checked(settings->value(id) != 0); }));
            continue;
        }
        const bool compact = spec.group == "Shape Dynamics" || spec.group == "Scattering" ||
                             spec.group == "Texture" || spec.group == "Color Dynamics" ||
                             spec.group == "Transfer" || spec.group == "Brush Pose" ||
                             spec.group == "Noise" || spec.group == "Wet Edges" || spec.group == "Build-up";
        if (compact) {
            auto block = b.column(content, "parameter-" + key, 0, Metric::size30 + Metric::space12, Metric::space4);
            auto line = b.row(block, {}, 0, Metric::textDescription, Metric::space0);
            b.label(line, std::string(spec.name), Metric::textDescription, {}, Metric::weightSemiBold);
            b.space(line);
            auto value = b.label(line, settings->label(spec.id), Metric::textCaption, "value-" + key);
            good(bindings->bind(value, settings->values, [settings, id = spec.id](Widget w, const ToolValues&) {
                return w.set_text(settings->label(id));
            }));
            auto slider = take(b.controls.slider(block, "adjust-" + key,
                {.minimum=0, .maximum=1, .value=settings->normalized(spec.id), .width=content_width,
                 .accessibility_name=std::string(spec.name), .size=lvui::SliderSize::mini},
                [vm, id=spec.id](double v) { vm->set_parameter_normalized(id, v); }));
            good(bindings->bind(slider, settings->values, [settings, id=spec.id, slider](Widget, const ToolValues&) {
                return slider.set_value(settings->normalized(id));
            }));
            continue;
        }
        const auto block =
            take(b.controls.vstack(content, "parameter-" + key,
                                   {.spacing = parameter_spacing,
                                    .alignment = ii::gui::StackAlignment::stretch,
                                    .insets = ii::gui::Insets{Metric::space0, parameter_inset,
                                                              Metric::space0, parameter_inset}}));
        good(block.set_extent({0, parameter_height}));
        if (spec.id == P::tilt) tilt_parameter = block;
        if (spec.id == P::stabilization) smoothing_parameter = block;
        borders(block, false, false, true);
        const auto label = b.row(block, {}, 0, lvui::icon_button_defaults::extent);
        b.glyph(label, std::string(spec.icon));
        auto name = b.label(label, std::string(spec.name));
        good(name.set_flex(1));
        const auto value = b.label(label, settings->label(spec.id), Metric::textHeader,
                                   "value-" + key, Metric::weightSemiBold);
        good(bindings->bind(value, settings->values,
                            [settings, id = spec.id](Widget w, const ToolValues &)
                            { return w.set_text(settings->label(id)); }));
        const auto decrease = b.button(
            label, spec.id == P::size ? "size-minus" : "decrease-" + key, "−",
            [vm, id = spec.id] { vm->step_parameter(id, -1); }, Metric::size28);
        const auto increase = b.button(
            label, spec.id == P::size ? "size-plus" : "increase-" + key, "+",
            [vm, id = spec.id] { vm->step_parameter(id, 1); }, Metric::size28);
        good(decrease.set_accessibility_name("Decrease " + std::string(spec.name)));
        good(increase.set_accessibility_name("Increase " + std::string(spec.name)));
        const auto slider = take(b.controls.slider(
            block, spec.id == P::size ? "brush-size" : "adjust-" + key,
            {.minimum = 0,
             .maximum = 1,
             .value = settings->normalized(spec.id),
             .width = content_width,
             .accessibility_name = std::string(spec.name),
             .size = parameter_slider_size},
            [vm, id = spec.id](double v) { vm->set_parameter_normalized(id, v); }));
        good(slider.set_layout_handler([slider](ii::gui::Rect r) { (void)r; }));
        good(bindings->bind(slider, settings->values,
                            [settings, id = spec.id, slider](Widget, const ToolValues &)
                            { return slider.set_value(settings->normalized(id)); }));
        const auto range = b.row(block, {}, 0, Metric::textCaptionLineHeight);
        b.label(range, ii::gui::binding_text(spec.minimum) + std::string(spec.unit),
                Metric::textCaption);
        b.space(range);
        b.label(range, ii::gui::binding_text(spec.maximum) + std::string(spec.unit),
                Metric::textCaption);
    }
    const auto foot =
        b.row(root, "tool-actions", 0, footer_height, Metric::space6, panel_padding);
    borders(foot, true);
    good(bindings->bind(foot, vm->selected_tool, [](Widget w, const std::string& value) { return w.set_visible(value != "crop" && value != "perspective-crop" && !area_selection(value) && value != "magic-wand" && value != "gradient" && value != "paint-bucket" && !shape_tool(value)); }));
    auto segments = take(b.controls.icon_segmented_control(foot, "brush-dynamics"));
    const auto reveal_parameter = [scroll](Widget target)
    {
        const auto viewport = scroll.snapshot(), parameter = target.snapshot();
        if (!viewport || !parameter) return;
        const auto offset = viewport->scroll_offset + parameter->bounds.y - viewport->bounds.y;
        auto result = scroll.set_scroll_offset(std::max(0.0, double(offset)));
        if (!result && result.error().code != ii::core::ErrorCode::closed) good(std::move(result));
    };
    b.icon(segments, "pressure-enabled", "brush-pressure",
           [vm] { vm->set_pressure_enabled(!vm->pressure_enabled.get()); });
    b.icon(segments, "tilt-detail", "panel-tilt-angle",
           [reveal_parameter, tilt_parameter] { reveal_parameter(tilt_parameter); });
    b.icon(segments, "smoothing-detail", "brush-smoothing",
           [reveal_parameter, smoothing_parameter] { reveal_parameter(smoothing_parameter); });
    b.icon(segments, "clone-source", "clone-source", [vm] { vm->select_tool("clone-stamp"); });
    b.space(foot);
    b.icon(foot, "reset-tool", "generalundo", [vm] { vm->reset_parameters(); });
    b.icon(foot, "save-tool", "generalsave", [vm] { vm->save_parameters(); });
    return root;
}
} // namespace congregation::native
