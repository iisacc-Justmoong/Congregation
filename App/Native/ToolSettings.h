#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSharedCanvasNative.h>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace congregation::native
{
enum class ToolParameter : std::size_t
{
    size,
    hardness,
    opacity,
    flow,
    roundness,
    angle,
    spacing,
    scatter,
    count,
    stabilization,
    post,
    pressure,
    tilt,
    velocity,
    wetness,
    mix,
    load,
    dilution,
    start,
    end,
    minimum_size,
    taper,
    total
};
struct ParameterSpec final
{
    ToolParameter id;
    std::string_view key, name, group, icon, unit;
    double minimum, maximum, initial, step = 1;
    bool discrete = true, logarithmic = false;
};
using ToolValues = std::array<double, static_cast<std::size_t>(ToolParameter::total)>;
// Tool preferences are view state, independent from document revisions/history.
class ToolSettings final
{
  public:
    ToolSettings();
    [[nodiscard]] static std::span<const ParameterSpec> specs();
    [[nodiscard]] static const ParameterSpec& spec(ToolParameter);
    [[nodiscard]] double value(ToolParameter) const;
    [[nodiscard]] double normalized(ToolParameter) const;
    [[nodiscard]] std::string label(ToolParameter) const;
    bool set(ToolParameter, double);
    bool set_normalized(ToolParameter, double);
    bool step(ToolParameter, int direction);
    void reset();
    [[nodiscard]] std::string serialize() const;
    bool restore(std::string_view);
    [[nodiscard]] iiSharedCanvas::BitmapBrush brush() const;
    ii::core::Observable<ToolValues> values;
};
} // namespace congregation::native
