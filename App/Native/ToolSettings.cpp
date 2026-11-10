#include "ToolSettings.h"
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <cmath>
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
    {P::wetness, "wetness", "Wetness", "Paint & mixing", "mixer-brush", "%", 0, 100, 20},
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
    {P::noise_monochrome, "noise-monochrome", "Monochrome", "Noise", "", "", 0, 1, 1, 1, true, false, true}};
ToolValues defaults()
{
    ToolValues v{};
    for (const auto& s : parameters)
        v[std::size_t(s.id)] = s.initial;
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
ToolSettings::ToolSettings() : values(defaults())
{
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
    values.set(defaults());
}
std::string ToolSettings::serialize() const
{
    std::string out = "congregation.tools=1\n";
    for (const auto& s : parameters)
        out += std::string(s.key) + "=" + ii::core::format_decimal(value(s.id)) + "\n";
    return out;
}
bool ToolSettings::restore(std::string_view text)
{
    if (text.size() > 8192 || !text.starts_with("congregation.tools=1\n"))
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
    for (std::size_t i = std::size_t(P::pose_tilt_x); i < seen.size(); ++i)
        seen[i] = true; // Older version-1 settings predate these optional controls.
    if (!std::ranges::all_of(seen, [](bool v) { return v; }) ||
        next[std::size_t(P::minimum_opacity)] > next[std::size_t(P::maximum_opacity)])
        return false;
    values.set(std::move(next));
    return true;
}
} // namespace congregation::native
