#include "ToolSettings.h"
#include <iiSDKCore>
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace congregation::native
{
namespace
{
using P = ToolParameter;
constexpr ParameterSpec parameters[]{
    {P::size, "size", "Size", "Brush", "brush-size", " px", 1, 1024, 64, 1, true, true},
    {P::hardness, "hardness", "Hardness", "Brush", "brush-hardness", "%", 0, 100, 85},
    {P::opacity, "opacity", "Opacity", "Brush", "brush-opacity", "%", 0, 100, 100},
    {P::flow, "flow", "Flow", "Brush", "brush-flow", "%", 0, 100, 90},
    {P::roundness, "roundness", "Roundness", "Tip & spacing", "ellipse", "%", 1, 100, 100},
    {P::angle, "angle", "Angle", "Tip & spacing", "rotate-view", "°", -180, 180, 0},
    {P::spacing, "spacing", "Spacing Ratio", "Brush", "panel-brush-stroke", "%", 0, 100, 12},
    {P::scatter, "scatter", "Scatter", "Scattering", "pattern-stamp", "%", 0, 100, 5},
    {P::count, "count", "Bristle count", "Tip & spacing", "bristle-brush", "", 1, 64, 1},
    {P::stabilization, "stabilization", "Smoothing Strength", "Brush", "brush-smoothing", "%", 0, 100,
     18},
    {P::post, "post", "Smoothing Radius", "Smoothing", "pen", " px", 0, 20, 4},
    {P::pressure, "pressure", "Pressure sensitivity", "Dynamics", "brush-pressure", "%", 0, 100,
     85},
    {P::tilt, "tilt", "Tilt sensitivity", "Dynamics", "panel-tilt-angle", "%", 0, 100, 30},
    {P::minimum_opacity, "minimum-opacity", "Minimum opacity", "Transfer", "brush-opacity", "%", 0, 100, 0},
    {P::maximum_opacity, "maximum-opacity", "Maximum opacity", "Transfer", "brush-opacity", "%", 0, 100, 100},
    {P::wetness, "wetness", "Wetness", "Paint & mixing", "mixer-brush", "%", 0, 100, 0},
    {P::mix, "mix", "Mix", "Paint & mixing", "smudge", "%", 0, 100, 45},
    {P::load, "load", "Load", "Paint & mixing", "paint-bucket", "%", 0, 100, 80},
    {P::dilution, "dilution", "Dilution", "Paint & mixing", "gradient", "%", 0, 100, 12},
    {P::start, "start", "Start", "Taper & limits", "add-anchor", "%", 0, 100, 15},
    {P::end, "end", "End", "Taper & limits", "delete-anchor", "%", 0, 100, 30},
    {P::minimum_size, "minimum-size", "Minimum size", "Taper & limits", "brush-size", "%", 0, 100,
     8},
    {P::taper, "taper", "Taper strength", "Taper & limits", "curvature-pen", "%", 0, 100, 35},
    {P::pose_tilt_x, "pose-tilt-x", "Tilt X", "Brush Pose", "panel-tilt-angle", "%", -100, 100, 0},
    {P::pose_override_tilt_x, "pose-override-tilt-x", "Override Tilt X", "Brush Pose", "", "", 0, 1, 0, 1, true, false, true},
    {P::pose_tilt_y, "pose-tilt-y", "Tilt Y", "Brush Pose", "panel-tilt-angle", "%", -100, 100, 0},
    {P::pose_override_tilt_y, "pose-override-tilt-y", "Override Tilt Y", "Brush Pose", "", "", 0, 1, 0, 1, true, false, true},
    {P::pose_rotation, "pose-rotation", "Rotation", "Brush Pose", "rotate-view", "°", 0, 360, 0},
    {P::pose_override_rotation, "pose-override-rotation", "Override Rotation", "Brush Pose", "", "", 0, 1, 0, 1, true, false, true},
    {P::pose_pressure, "pose-pressure", "Pressure", "Brush Pose", "brush-pressure", "%", 0, 100, 100},
    {P::pose_override_pressure, "pose-override-pressure", "Override Pressure", "Brush Pose", "", "", 0, 1, 0, 1, true, false, true},
    {P::scatter_both_axes, "scatter-both-axes", "Both Axes", "Scattering", "", "", 0, 1, 0, 1, true, false, true},
    {P::scatter_count, "scatter-count", "Count", "Scattering", "pattern-stamp", "", 1, 64, 1},
    {P::scatter_count_jitter, "scatter-count-jitter", "Count Jitter", "Scattering", "pattern-stamp", "%", 0, 100, 0},
    {P::noise_enabled, "noise-enabled", "Enable Noise", "Noise", "", "", 0, 1, 0, 1, true, false, true},
    {P::noise_amount, "noise-amount", "Amount", "Noise", "pattern-stamp", "%", 0, 100, 20},
    {P::wet_edges_enabled, "wet-edges-enabled", "Enable Wet Edges", "Wet Edges", "", "", 0, 1, 0, 1, true, false, true},
    {P::wet_edges_strength, "wet-edges-strength", "Edge Strength", "Wet Edges", "mixer-brush", "%", 0, 100, 40},
    {P::shape_size_jitter, "shape-size-jitter", "Size Jitter", "Shape Dynamics", "brush-size", "%", 0, 100, 0},
    {P::shape_angle_jitter, "shape-angle-jitter", "Angle Jitter", "Shape Dynamics", "rotate-view", "%", 0, 100, 0},
    {P::shape_roundness_jitter, "shape-roundness-jitter", "Roundness Jitter", "Shape Dynamics", "ellipse", "%", 0, 100, 0},
    {P::shape_flip_x, "shape-flip-x", "Flip X Jitter", "Shape Dynamics", "", "", 0, 1, 0, 1, true, false, true},
    {P::shape_flip_y, "shape-flip-y", "Flip Y Jitter", "Shape Dynamics", "", "", 0, 1, 0, 1, true, false, true},
    {P::shape_project, "shape-project", "Brush Projection", "Shape Dynamics", "", "", 0, 1, 0, 1, true, false, true},
    {P::color_apply_per_tip, "color-per-tip", "Apply Per Tip", "Color Dynamics", "", "", 0, 1, 0, 1, true, false, true},
    {P::color_hue_jitter, "color-hue-jitter", "Hue Jitter", "Color Dynamics", "color-picker", "%", 0, 100, 0},
    {P::color_saturation_jitter, "color-saturation-jitter", "Saturation Jitter", "Color Dynamics", "color-picker", "%", 0, 100, 0},
    {P::color_brightness_jitter, "color-brightness-jitter", "Brightness Jitter", "Color Dynamics", "color-picker", "%", 0, 100, 0},
    {P::color_purity, "color-purity", "Purity", "Color Dynamics", "color-picker", "%", -100, 100, 0},
    {P::shape_size_control, "shape-size-control", "Size Control", "Shape Dynamics", "", "", 0, 7, 2, 1, true, false, false, true},
    {P::shape_minimum_diameter, "shape-minimum-diameter", "Minimum Diameter", "Shape Dynamics", "brush-size", "%", 0, 100, 0},
    {P::shape_angle_control, "shape-angle-control", "Angle Control", "Shape Dynamics", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::shape_roundness_control, "shape-roundness-control", "Roundness Control", "Shape Dynamics", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::shape_minimum_roundness, "shape-minimum-roundness", "Minimum Roundness", "Shape Dynamics", "ellipse", "%", 0, 100, 0},
    {P::scatter_control, "scatter-control", "Scatter Control", "Scattering", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::scatter_count_control, "scatter-count-control", "Count Jitter Control", "Scattering", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::transfer_flow_jitter, "transfer-flow-jitter", "Flow Jitter", "Transfer", "brush-flow", "%", 0, 100, 0},
    {P::transfer_flow_control, "transfer-flow-control", "Flow Control", "Transfer", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::transfer_flow_minimum, "transfer-flow-minimum", "Minimum Flow", "Transfer", "brush-flow", "%", 0, 100, 0},
    {P::transfer_wetness_jitter, "transfer-wetness-jitter", "Wetness Jitter", "Transfer", "mixer-brush", "%", 0, 100, 0},
    {P::transfer_wetness_control, "transfer-wetness-control", "Wetness Control", "Transfer", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::transfer_wetness_minimum, "transfer-wetness-minimum", "Minimum Wetness", "Transfer", "mixer-brush", "%", 0, 100, 0},
    {P::transfer_mix_jitter, "transfer-mix-jitter", "Mix Jitter", "Transfer", "smudge", "%", 0, 100, 0},
    {P::transfer_mix_control, "transfer-mix-control", "Mix Control", "Transfer", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::transfer_mix_minimum, "transfer-mix-minimum", "Minimum Mix", "Transfer", "smudge", "%", 0, 100, 0},
    {P::smoothing_enabled, "smoothing-enabled", "Enable Smoothing", "Smoothing", "", "", 0, 1, 1, 1, true, false, true},
    {P::smoothing_method, "smoothing-method", "Method", "Smoothing", "", "", 0, 1, 1, 1, true, false, false, true},
    {P::smoothing_catch_up, "smoothing-catch-up", "Catch Up at End", "Smoothing", "", "", 0, 1, 1, 1, true, false, true},
    {P::texture_invert, "texture-invert", "Invert", "Texture", "", "", 0, 1, 0, 1, true, false, true},
    {P::texture_scale, "texture-scale", "Scale", "Texture", "pattern-stamp", "%", 1, 1000, 100},
    {P::texture_brightness, "texture-brightness", "Brightness", "Texture", "pattern-stamp", "%", -100, 100, 0},
    {P::texture_contrast, "texture-contrast", "Contrast", "Texture", "pattern-stamp", "%", -100, 100, 0},
    {P::texture_each_tip, "texture-each-tip", "Texture Each Tip", "Texture", "", "", 0, 1, 0, 1, true, false, true},
    {P::texture_mode, "texture-mode", "Mode", "Texture", "", "", 0, 9, 8, 1, true, false, false, true},
    {P::texture_depth, "texture-depth", "Depth", "Texture", "pattern-stamp", "%", 0, 100, 100},
    {P::texture_minimum_depth, "texture-minimum-depth", "Minimum Depth", "Texture", "pattern-stamp", "%", 0, 100, 0},
    {P::texture_depth_jitter, "texture-depth-jitter", "Depth Jitter", "Texture", "pattern-stamp", "%", 0, 100, 0},
    {P::texture_depth_control, "texture-depth-control", "Depth Control", "Texture", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::color_mix_jitter, "color-mix-jitter", "Foreground/Background Jitter", "Color Dynamics", "color-picker", "%", 0, 100, 0},
    {P::color_mix_control, "color-mix-control", "Foreground/Background Control", "Color Dynamics", "", "", 0, 7, 0, 1, true, false, false, true},
    {P::noise_grain_size, "noise-grain-size", "Grain Size", "Noise", "pattern-stamp", " px", 1, 100, 1},
    {P::wet_edges_width, "wet-edges-width", "Edge Width", "Wet Edges", "mixer-brush", " px", 0, 100, 3},
    {P::wet_edges_diffusion, "wet-edges-diffusion", "Diffusion", "Wet Edges", "mixer-brush", "%", 0, 100, 20},
    {P::build_up_enabled, "build-up-enabled", "Enable Build-up", "Build-up", "", "", 0, 1, 0, 1, true, false, true},
    {P::build_up_rate, "build-up-rate", "Accumulation", "Build-up", "brush-flow", "%", 0, 100, 25},
    {P::build_up_limit, "build-up-limit", "Opacity Limit", "Build-up", "brush-opacity", "%", 0, 100, 100},
    {P::build_up_time, "build-up-time", "Time-based Build-up", "Build-up", "", "", 0, 1, 0, 1, true, false, true},
    {P::shape_tilt_scale, "shape-tilt-scale", "Tilt Scale", "Shape Dynamics", "panel-tilt-angle", "%", 0, 100, 100},
    {P::transfer_opacity_jitter, "transfer-opacity-jitter", "Opacity Jitter", "Transfer", "brush-opacity", "%", 0, 100, 0},
    {P::noise_monochrome, "noise-monochrome", "Monochrome", "Noise", "", "", 0, 1, 1, 1, true, false, true},
    {P::tip_shape, "tip-shape", "Tip shape", "Tip", "", "", 0, 2, 0, 1, true, false, false, true},
    {P::blend_mode, "brush-blend-mode", "Blend mode", "Paint", "", "", 0, 3, 0, 1, true, false, false, true},
    {P::symmetry_mode, "brush-symmetry", "Symmetry", "Stroke", "", "", 0, 5, 0, 1, true, false, false, true},
    {P::pencil_pixel_aligned, "pencil-pixel-edges", "Pixel-aligned edges", "Tip", "", "", 0, 1, 1, 1, true, false, true},
    {P::pencil_auto_erase, "pencil-auto-erase", "Auto erase", "Paint", "", "", 0, 1, 0, 1, true, false, true},
    {P::pencil_snap, "pencil-snap", "Snap to pixel grid", "Stroke", "", "", 0, 1, 1, 1, true, false, true},
    {P::airbrush_radius, "airbrush-radius", "Spray radius", "Tip", "airbrush", "%", 1, 200, 100},
    {P::airbrush_rate, "airbrush-rate", "Spray rate", "Paint", "airbrush", "%", 0, 100, 50},
    {P::airbrush_stationary, "airbrush-stationary", "Build up while stationary", "Paint", "", "", 0, 1, 1, 1, true, false, true},
    {P::dry_grain_amount, "dry-grain-amount", "Grain amount", "Dry media", "pattern-stamp", "%", 0, 100, 35},
    {P::dry_grain_size, "dry-grain-size", "Grain size", "Dry media", "pattern-stamp", " px", 1, 100, 2},
    {P::dry_dryness, "dry-dryness", "Dryness", "Dry media", "dry-brush", "%", 0, 100, 60},
    {P::bristle_length, "bristle-length", "Bristle length", "Bristles", "bristle-brush", "%", 0, 100, 60},
    {P::bristle_thickness, "bristle-thickness", "Bristle thickness", "Bristles", "bristle-brush", "%", 1, 100, 30},
    {P::bristle_stiffness, "bristle-stiffness", "Bristle stiffness", "Bristles", "bristle-brush", "%", 0, 100, 50},
    {P::bristle_spread, "bristle-spread", "Bristle spread", "Bristles", "bristle-brush", "%", 0, 100, 20},
    {P::mixer_sample_all, "mixer-sample-all", "Sample all layers", "Mixing", "", "", 0, 1, 0, 1, true, false, true},
    {P::mixer_clean_after, "mixer-clean-after", "Clean after each stroke", "Mixing", "", "", 0, 1, 0, 1, true, false, true},
    {P::mixer_load_after, "mixer-load-after", "Load after each stroke", "Mixing", "", "", 0, 1, 1, 1, true, false, true},
    {P::mixing_active, "mixing-active", "Mixing active", "Paint & mixing", "", "", 0, 1, 0, 1, true, false, true},
    {P::absolute_spacing, "absolute-spacing", "Spacing", "Brush", "", " px", 0, 64, 0, .1, false},
    {P::pressure_opacity_enabled, "pressure-opacity-enabled", "Pressure Opacity", "Brush", "", "", 0, 1, 1, 1, true, false, true},
    {P::pressure_curve_min, "pressure-curve-min", "Pressure Minimum", "Brush", "", "", 0, 1, 0, .01, false},
    {P::pressure_curve_mid, "pressure-curve-mid", "Pressure Center", "Brush", "", "", 0, 1, .5, .01, false},
    {P::pressure_curve_max, "pressure-curve-max", "Pressure Maximum", "Brush", "", "", 0, 1, 1, .01, false}};
constexpr std::array<std::string_view, ToolSettings::profile_count> brushNames{
    "brush", "pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush",
    "eraser", "smudge", "clone-stamp"};
constexpr std::size_t legacy_profile_count = 6;
ToolValues defaults()
{
    ToolValues v{};
    for (const auto& s : parameters)
        v[std::size_t(s.id)] = s.initial;
    return v;
}
ToolValues brush_defaults(std::size_t index)
{
    auto v = defaults();
    auto put = [&](P id, double n) { v[std::size_t(id)] = n; };
    if (index != 0) put(P::texture_each_tip,1);
    if (index == 1) {
        put(P::size,12); put(P::hardness,100); put(P::flow,100); put(P::spacing,10);
        put(P::scatter,0); put(P::stabilization,0); put(P::pressure_opacity_enabled,0);
    } else if (index == 2) {
        put(P::size,80); put(P::hardness,0); put(P::flow,30); put(P::spacing,5);
        put(P::scatter,0); put(P::stabilization,20); put(P::shape_minimum_diameter,10);
        put(P::build_up_enabled,1);
    } else if (index == 3) {
        put(P::size,40); put(P::hardness,50); put(P::flow,80); put(P::spacing,10);
        put(P::scatter,0); put(P::stabilization,20); put(P::wetness,50); put(P::mix,50); put(P::load,50);
    } else if (index == 4) {
        put(P::size,40); put(P::hardness,60); put(P::flow,70); put(P::spacing,15);
        put(P::roundness,80); put(P::scatter,10); put(P::stabilization,20);
    } else if (index == 5) {
        put(P::size,60); put(P::hardness,60); put(P::flow,80); put(P::spacing,10);
        put(P::count,32); put(P::scatter,0); put(P::stabilization,20); put(P::shape_angle_control,3);
        put(P::wetness,20); put(P::mixing_active,1);
    }
    return v;
}
} // namespace
void ToolSettings::set_texture(std::string name, int width, int height, std::vector<std::uint8_t> alpha)
{
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) != alpha.size())
        throw std::invalid_argument("Invalid texture dimensions");
    texture_width_ = width; texture_height_ = height; texture_alpha_ = std::move(alpha);
    texture_name.set(std::move(name));
}
void ToolSettings::set_background(std::uint32_t argb) { background_ = argb; }
ToolSettings::ToolSettings() : values(defaults()), parameter_signals_(std::make_shared<ParameterSignals>())
{
    parameter_signals_->delivered = values.get();
    // Dispatch one signal per changed field, including coupled edits and
    // multi-field profile/restore transactions. The delivery snapshot is only
    // a diff baseline; values remains the sole source of settings.
    values_subscription_ = values.observe([signals=parameter_signals_](const ToolValues& next, std::uint64_t revision) {
        const auto before = signals->delivered;
        signals->delivered = next;
        std::exception_ptr failure;
        for (std::size_t i = 0; i < next.size(); ++i) {
            if (before[i] == next[i]) continue;
            try { signals->revisions[i].set(revision); }
            catch (...) { if (!failure) failure = std::current_exception(); }
        }
        if (failure) std::rethrow_exception(failure);
    });
}
const ii::core::Observable<std::uint64_t>& ToolSettings::parameter_revision(P id) const
{
    (void)spec(id);
    return parameter_signals_->revisions[std::size_t(id)];
}
std::span<const ParameterSpec> ToolSettings::specs()
{
    return parameters;
}
const ParameterSpec& ToolSettings::spec(P id)
{
    const auto i = std::size_t(id);
    if (i >= std::size(parameters))
        throw std::out_of_range("Unknown Tool parameter");
    return parameters[i];
}
double ToolSettings::value(P id) const
{
    return values.get().at(std::size_t(id));
}
double ToolSettings::normalized(P id) const
{
    const auto& s = spec(id);
    const auto v = value(id);
    return s.logarithmic ? std::log(v / s.minimum) / std::log(s.maximum / s.minimum)
                         : (v - s.minimum) / (s.maximum - s.minimum);
}
std::string ToolSettings::option_label(P id, int option)
{
    if (id == P::tip_shape || id == P::blend_mode || id == P::symmetry_mode) {
        static constexpr std::string_view shapes[]{"Round", "Square", "Diamond"};
        static constexpr std::string_view modes[]{"Normal", "Multiply", "Screen", "Overlay"};
        static constexpr std::string_view symmetry[]{"Off", "Vertical", "Horizontal", "Dual Axis", "Radial", "Mandala"};
        const std::span<const std::string_view> choices = id == P::tip_shape ? std::span<const std::string_view>{shapes} :
            id == P::blend_mode ? std::span<const std::string_view>{modes} : std::span<const std::string_view>{symmetry};
        if (option < 0 || std::size_t(option) >= choices.size()) throw std::out_of_range("Unknown brush option");
        return std::string(choices[std::size_t(option)]);
    }
    if (id == P::texture_mode) {
        static constexpr std::string_view modes[]{"Multiply", "Subtract", "Darken", "Lighten", "Color Dodge", "Color Burn", "Linear Burn", "Hard Mix", "Height", "Linear Height"};
        if(option<0 || std::size_t(option)>=std::size(modes))throw std::out_of_range("Unknown texture mode");
        return std::string(modes[static_cast<std::size_t>(option)]);
    }
    if (id == P::smoothing_method) {
        if(option<0 || option>1)throw std::out_of_range("Unknown smoothing method");
        return option == 0 ? "Exponential" : "Pulled String";
    }
    if (spec(id).dynamics_choice) {
        static constexpr std::string_view names[]{"Off", "Fade", "Pen Pressure", "Pen Tilt", "Stylus Wheel", "Stylus Rotation", "Initial Direction", "Direction"};
        if(option<0 || std::size_t(option)>=std::size(names))throw std::out_of_range("Unknown dynamics control");
        return std::string(names[static_cast<std::size_t>(option)]);
    }
    throw std::invalid_argument("Parameter does not have choices");
}
std::string ToolSettings::label(P id) const
{
    if (spec(id).dynamics_choice) return option_label(id,static_cast<int>(value(id)));
    return ii::core::format_decimal(value(id)) + std::string(spec(id).unit);
}
bool ToolSettings::set(P id, double v)
{
    const auto& s = spec(id);
    if (!std::isfinite(v))
        return false;
    v = std::clamp(v, s.minimum, s.maximum);
    if (s.discrete)
        v = std::round(v);
    auto next = values.get();
    if (next[std::size_t(id)] == v)
        return false;
    next[std::size_t(id)] = v;
    if (id == P::spacing) next[std::size_t(P::absolute_spacing)] = 0;
    if (id == P::wetness || id == P::mix || id == P::load || id == P::dilution)
        next[std::size_t(P::mixing_active)] = next[std::size_t(P::wetness)] > 0 ? 1 : 0;
    if (id == P::minimum_opacity && v > next[std::size_t(P::maximum_opacity)])
        next[std::size_t(P::maximum_opacity)] = v;
    if (id == P::maximum_opacity && v < next[std::size_t(P::minimum_opacity)])
        next[std::size_t(P::minimum_opacity)] = v;
    values.set(std::move(next));
    return true;
}
bool ToolSettings::set_normalized(P id, double n)
{
    if (!std::isfinite(n))
        return false;
    const auto& s = spec(id);
    n = std::clamp(n, 0., 1.);
    return set(id, s.logarithmic ? s.minimum * std::pow(s.maximum / s.minimum, n)
                                 : s.minimum + n * (s.maximum - s.minimum));
}
bool ToolSettings::step(P id, int direction)
{
    return direction && set(id, value(id) + (direction > 0 ? 1 : -1) * spec(id).step);
}
void ToolSettings::reset()
{
    values.set(brush_defaults(active_profile_));
}
void ToolSettings::select_texture_profile(std::size_t next)
{
    auto& previousTexture = profile_textures_[active_profile_];
    previousTexture.width = texture_width_; previousTexture.height = texture_height_;
    previousTexture.alpha = std::move(texture_alpha_); previousTexture.name = texture_name.get();
    auto& nextTexture = profile_textures_[next];
    if (next == 4 && nextTexture.name.empty()) {
        nextTexture.width = nextTexture.height = 4;
        nextTexture.alpha = {255,100,220,150,80,210,140,240,230,130,250,90,160,240,110,200};
        nextTexture.name = "Paper";
    }
    texture_width_ = nextTexture.width; texture_height_ = nextTexture.height;
    texture_alpha_ = std::move(nextTexture.alpha);
    texture_name.set(nextTexture.name.empty() ? "Select texture" : nextTexture.name);
}
bool ToolSettings::select_brush(std::string_view tool)
{
    const auto found = std::ranges::find(brushNames, tool);
    if (found == brushNames.end()) return false;
    const auto next = std::size_t(found - brushNames.begin());
    if (next == active_profile_) return false;
    profiles_[active_profile_] = values.get(); profile_present_[active_profile_] = true;
    select_texture_profile(next);
    active_profile_ = next; multiple_profiles_ = true;
    values.set(profile_present_[next] ? profiles_[next] : brush_defaults(next));
    return true;
}
std::string ToolSettings::serialize() const
{
    if (multiple_profiles_) {
        std::string out = "congregation.tools=3\n";
        for (std::size_t i=0; i<brushNames.size(); ++i) {
            const auto profile = i==active_profile_ ? values.get() :
                (profile_present_[i] ? profiles_[i] : brush_defaults(i));
            for (const auto& s : parameters)
                out += std::string(brushNames[i]) + "." + std::string(s.key) + "=" +
                       ii::core::format_decimal(profile[std::size_t(s.id)]) + "\n";
        }
        return out;
    }
    std::string out = "congregation.tools=1\n";
    for (const auto& s : parameters)
        out += std::string(s.key) + "=" + ii::core::format_decimal(value(s.id)) + "\n";
    return out;
}
bool ToolSettings::restore(std::string_view text)
{
    if (text.size() > 131072) return false;
    const bool legacy_profiles = text.starts_with("congregation.tools=2\n");
    if (legacy_profiles || text.starts_with("congregation.tools=3\n")) {
        text.remove_prefix(std::string_view("congregation.tools=2\n").size());
        std::array<std::string, profile_count> payloads;
        for (auto& p : payloads) p = "congregation.tools=1\n";
        std::array<bool, profile_count> found{};
        while (!text.empty()) {
            const auto end = text.find('\n');
            const auto line = text.substr(0,end);
            text = end == text.npos ? std::string_view{} : text.substr(end+1);
            const auto dot = line.find('.');
            if (dot == line.npos) return false;
            const auto tool = std::ranges::find(brushNames, line.substr(0,dot));
            if (tool == brushNames.end()) return false;
            const auto index = std::size_t(tool-brushNames.begin());
            if (legacy_profiles && index >= legacy_profile_count) return false;
            found[index] = true;
            payloads[index] += std::string(line.substr(dot+1)) + "\n";
        }
        std::array<ToolValues, profile_count> parsed{};
        for (std::size_t i=0;i<parsed.size();++i) {
            if (legacy_profiles && i >= legacy_profile_count) { parsed[i] = brush_defaults(i); continue; }
            ToolSettings item;
            if (!found[i] || !item.restore(payloads[i])) return false;
            parsed[i] = item.values.get();
        }
        if (active_profile_ != 0) select_texture_profile(0);
        profiles_ = parsed; profile_present_.fill(true); active_profile_ = 0; multiple_profiles_ = true;
        values.set(profiles_[0]);
        return true;
    }
    if (!text.starts_with("congregation.tools=1\n"))
        return false;
    text.remove_prefix(std::string_view("congregation.tools=1\n").size());
    ToolValues next = defaults();
    std::array<bool, std::size(parameters)> seen{};
    bool legacyVelocity = false;
    while (!text.empty())
    {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);
        if (end == text.npos)
            text = {};
        else
            text.remove_prefix(end + 1);
        const auto eq = line.find('=');
        if (eq == line.npos)
            return false;
        const auto key = line.substr(0, eq);
        if (key == "velocity")
        {
            auto parsed = ii::core::parse_decimal(line.substr(eq + 1));
            if (legacyVelocity || !parsed || !std::isfinite(*parsed) || *parsed < 0 ||
                *parsed > 100 || std::trunc(*parsed) != *parsed)
                return false;
            legacyVelocity = true;
            continue;
        }
        const auto s = std::ranges::find(parameters, key, &ParameterSpec::key);
        if (s == std::end(parameters))
            return false;
        auto parsed = ii::core::parse_decimal(line.substr(eq + 1));
        if (!parsed || !std::isfinite(*parsed) || *parsed < s->minimum || *parsed > s->maximum ||
            (s->discrete && std::trunc(*parsed) != *parsed))
            return false;
        const auto i = std::size_t(s->id);
        if (seen[i])
            return false;
        seen[i] = true;
        next[i] = *parsed;
    }
    if (legacyVelocity && !seen[std::size_t(P::minimum_opacity)] &&
        !seen[std::size_t(P::maximum_opacity)])
    {
        seen[std::size_t(P::minimum_opacity)] = true;
        seen[std::size_t(P::maximum_opacity)] = true;
    }
    // Legacy normal brushes did not apply their saved wetness; preserve their dry result.
    if (!seen[std::size_t(P::mixing_active)]) next[std::size_t(P::wetness)] = 0;
    for (std::size_t i = std::size_t(P::pose_tilt_x); i < seen.size(); ++i)
        seen[i] = true; // Older version-1 settings predate these optional controls.
    if (!std::ranges::all_of(seen, [](bool v) { return v; }) ||
        next[std::size_t(P::minimum_opacity)] > next[std::size_t(P::maximum_opacity)])
        return false;
    if (active_profile_ != 0) select_texture_profile(0);
    profile_present_.fill(false); active_profile_ = 0; multiple_profiles_ = false;
    values.set(std::move(next));
    return true;
}

ToolSettings::PersistentState ToolSettings::persistent_state() const
{
    PersistentState result;
    result.values = serialize();
    result.textures = profile_textures_;
    result.textures[active_profile_] = {texture_width_, texture_height_, texture_alpha_, texture_name.get()};
    result.background = background_;
    return result;
}

bool ToolSettings::valid_persistent_state(const PersistentState& state)
{
    ToolSettings validated;
    if (!validated.restore(state.values)) return false;
    for (const auto& texture : state.textures) {
        if (texture.name.size() > 4096 || texture.width < 0 || texture.height < 0 ||
            texture.width > 4096 || texture.height > 4096) return false;
        if ((texture.width == 0) != (texture.height == 0) ||
            std::size_t(texture.width) * std::size_t(texture.height) != texture.alpha.size()) return false;
    }
    return true;
}

bool ToolSettings::restore_persistent_state(const PersistentState& state)
{
    if (!valid_persistent_state(state)) return false;
    if (!restore(state.values)) return false;
    profile_textures_ = state.textures;
    auto& active = profile_textures_[active_profile_];
    texture_width_ = active.width; texture_height_ = active.height;
    texture_alpha_ = std::move(active.alpha);
    texture_name.set(active.name.empty() ? "Select texture" : active.name);
    background_ = state.background;
    return true;
}
} // namespace congregation::native
