#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace iiSharedCanvas { struct BitmapBrush; }

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
    minimum_opacity,
    maximum_opacity,
    wetness,
    mix,
    load,
    dilution,
    start,
    end,
    minimum_size,
    taper,
    pose_tilt_x,
    pose_override_tilt_x,
    pose_tilt_y,
    pose_override_tilt_y,
    pose_rotation,
    pose_override_rotation,
    pose_pressure,
    pose_override_pressure,
    scatter_both_axes,
    scatter_count,
    scatter_count_jitter,
    noise_enabled,
    noise_amount,
    wet_edges_enabled,
    wet_edges_strength,
    shape_size_jitter,
    shape_angle_jitter,
    shape_roundness_jitter,
    shape_flip_x,
    shape_flip_y,
    shape_project,
    color_apply_per_tip,
    color_hue_jitter,
    color_saturation_jitter,
    color_brightness_jitter,
    color_purity,
    shape_size_control,
    shape_minimum_diameter,
    shape_angle_control,
    shape_roundness_control,
    shape_minimum_roundness,
    scatter_control,
    scatter_count_control,
    transfer_flow_jitter,
    transfer_flow_control,
    transfer_flow_minimum,
    transfer_wetness_jitter,
    transfer_wetness_control,
    transfer_wetness_minimum,
    transfer_mix_jitter,
    transfer_mix_control,
    transfer_mix_minimum,
    smoothing_enabled,
    smoothing_method,
    smoothing_catch_up,
    texture_invert,
    texture_scale,
    texture_brightness,
    texture_contrast,
    texture_each_tip,
    texture_mode,
    texture_depth,
    texture_minimum_depth,
    texture_depth_jitter,
    texture_depth_control,
    color_mix_jitter,
    color_mix_control,
    noise_grain_size,
    wet_edges_width,
    wet_edges_diffusion,
    build_up_enabled,
    build_up_rate,
    build_up_limit,
    build_up_time,
    total
};
struct ParameterSpec final
{
    ToolParameter id;
    std::string_view key, name, group, icon, unit;
    double minimum, maximum, initial, step = 1;
    bool discrete = true, logarithmic = false;
    bool checkbox = false;
    bool dynamics_choice = false;
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
    [[nodiscard]] iiSharedCanvas::BitmapBrush brush(std::string_view tool = "brush") const;
    void set_texture(std::string name, int width, int height, std::vector<std::uint8_t> alpha);
    void set_background(std::uint32_t argb);
    ii::core::Observable<std::string> texture_name{"Select texture"};
    ii::core::Observable<ToolValues> values;
  private:
    int texture_width_ = 0, texture_height_ = 0;
    std::vector<std::uint8_t> texture_alpha_;
    std::uint32_t background_ = 0xffffffffU;
};
} // namespace congregation::native
