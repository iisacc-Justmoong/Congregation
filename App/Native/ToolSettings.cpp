#include "ToolSettings.h"
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace congregation::native
{
namespace
{
using P = ToolParameter;
constexpr ParameterSpec parameters[]{
    {P::size, "size", "Size", "Stroke", "brush-size", " px", 1, 1024, 64, 1, true, true},
    {P::hardness, "hardness", "Hardness", "Stroke", "brush-hardness", "%", 0, 100, 85},
    {P::opacity, "opacity", "Opacity", "Stroke", "brush-opacity", "%", 0, 100, 100},
    {P::flow, "flow", "Flow", "Stroke", "brush-flow", "%", 0, 100, 90},
    {P::roundness, "roundness", "Roundness", "Tip & spacing", "ellipse", "%", 1, 100, 100},
    {P::angle, "angle", "Angle", "Tip & spacing", "rotate-view", "°", -180, 180, 0},
    {P::spacing, "spacing", "Spacing", "Tip & spacing", "panel-brush-stroke", "%", 0, 100, 12},
    {P::scatter, "scatter", "Scatter", "Tip & spacing", "pattern-stamp", "%", 0, 100, 5},
    {P::count, "count", "Bristle count", "Tip & spacing", "bristle-brush", "", 1, 64, 1},
    {P::stabilization, "stabilization", "Stabilization", "Dynamics", "brush-smoothing", "", 0, 100,
     18},
    {P::post, "post", "Post correction", "Dynamics", "pen", "", 0, 20, 4},
    {P::pressure, "pressure", "Pressure sensitivity", "Dynamics", "brush-pressure", "%", 0, 100,
     85},
    {P::tilt, "tilt", "Tilt sensitivity", "Dynamics", "panel-tilt-angle", "%", 0, 100, 30},
    {P::velocity, "velocity", "Velocity response", "Dynamics", "move", "%", 0, 100, 10},
    {P::wetness, "wetness", "Wetness", "Paint & mixing", "mixer-brush", "%", 0, 100, 20},
    {P::mix, "mix", "Mix", "Paint & mixing", "smudge", "%", 0, 100, 45},
    {P::load, "load", "Load", "Paint & mixing", "paint-bucket", "%", 0, 100, 80},
    {P::dilution, "dilution", "Dilution", "Paint & mixing", "gradient", "%", 0, 100, 12},
    {P::start, "start", "Start", "Taper & limits", "add-anchor", "%", 0, 100, 15},
    {P::end, "end", "End", "Taper & limits", "delete-anchor", "%", 0, 100, 30},
    {P::minimum_size, "minimum-size", "Minimum size", "Taper & limits", "brush-size", "%", 0, 100,
     8},
    {P::taper, "taper", "Taper strength", "Taper & limits", "curvature-pen", "%", 0, 100, 35}};
ToolValues defaults()
{
    ToolValues v{};
    for (const auto& s : parameters)
        v[std::size_t(s.id)] = s.initial;
    return v;
}
} // namespace
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
std::string ToolSettings::label(P id) const
{
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
    ToolValues next{};
    std::array<bool, std::size(parameters)> seen{};
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
    if (!std::ranges::all_of(seen, [](bool v) { return v; }))
        return false;
    values.set(std::move(next));
    return true;
}
iiSharedCanvas::BitmapBrush ToolSettings::brush() const
{
    const auto ratio = [&](P id) { return value(id) / 100.; };
    iiSharedCanvas::BitmapBrush b;
    b.size = value(P::size);
    b.opacity = ratio(P::opacity);
    b.flow = ratio(P::flow);
    b.hardness = ratio(P::hardness);
    b.spacingRatio = ratio(P::spacing);
    BrushState e;
    e.rasterizer.shape.angleMode = BrushAngleMode::Fixed;
    e.rasterizer.shape.angleRadians = value(P::angle) * std::numbers::pi / 180;
    e.rasterizer.shape.roundness = ratio(P::roundness);
    e.rasterizer.smoothingEnabled = value(P::stabilization) > 0 || value(P::post) > 0;
    e.rasterizer.smoothing = ratio(P::stabilization);
    e.rasterizer.smoothingRadius = value(P::post);
    e.rasterizer.smoothingMode =
        value(P::post) > 0 ? BrushSmoothingMode::PulledString : BrushSmoothingMode::Exponential;
    e.rasterizer.smoothingCatchUp = value(P::post) > 0;
    e.rasterizer.smoothingCatchUpOnEnd = value(P::post) > 0;
    e.dynamics.pressureToSize = ratio(P::pressure);
    e.dynamics.pressureToFlow = ratio(P::pressure);
    e.dynamics.tiltToEllipse = ratio(P::tilt);
    e.dynamics.velocityToOpacity = ratio(P::velocity);
    e.material.scatter.enabled = value(P::scatter) > 0;
    e.material.scatter.radius = ratio(P::scatter);
    e.material.scatter.relativeToSize = true;
    e.material.bristle.count = std::uint32_t(value(P::count));
    e.material.simulation.wetness = ratio(P::wetness);
    e.material.simulation.mixStrength = ratio(P::mix);
    e.material.simulation.load = ratio(P::load);
    e.material.simulation.pickup = ratio(P::dilution);
    e.material.simulation.deposit = 1 - ratio(P::dilution);
    e.rasterizer.warmupDistance = b.size * ratio(P::start) * ratio(P::taper);
    e.rasterizer.taperMinimum = ratio(P::minimum_size);
    e.rasterizer.warmupTaperShape = StrokeTaperShape::SmoothStep;
    b.engineState = std::move(e);
    return b;
}
} // namespace congregation::native
