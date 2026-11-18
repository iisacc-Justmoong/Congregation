#pragma once
#include <iiSDKCore>
#include <array>
#include <memory>
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
    shape_tilt_scale,
    transfer_opacity_jitter,
    noise_monochrome,
    tip_shape,
    blend_mode,
    symmetry_mode,
    pencil_pixel_aligned,
    pencil_auto_erase,
    pencil_snap,
    airbrush_radius,
    airbrush_rate,
    airbrush_stationary,
    dry_grain_amount,
    dry_grain_size,
    dry_dryness,
    bristle_length,
    bristle_thickness,
    bristle_stiffness,
    bristle_spread,
    mixer_sample_all,
    mixer_clean_after,
    mixer_load_after,
    mixing_active,
    absolute_spacing,
    pressure_opacity_enabled,
    pressure_curve_min,
    pressure_curve_mid,
    pressure_curve_max,
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
    static constexpr std::size_t profile_count = 9;
    struct TextureState final {
        int width = 0, height = 0;
        std::vector<std::uint8_t> alpha;
        std::string name;
    };
    struct PersistentState final {
        std::string values;
        std::array<TextureState, profile_count> textures;
        std::uint32_t background = 0xffffffffU;
    };
    ToolSettings();
    [[nodiscard]] static std::span<const ParameterSpec> specs();
    [[nodiscard]] static const ParameterSpec& spec(ToolParameter);
    [[nodiscard]] static std::string option_label(ToolParameter, int option);
    [[nodiscard]] double value(ToolParameter) const;
    [[nodiscard]] double normalized(ToolParameter) const;
    [[nodiscard]] std::string label(ToolParameter) const;
    // A change signal carries no second copy of the setting. The values array
    // remains authoritative, including for multi-parameter transactions.
    [[nodiscard]] const ii::core::Observable<std::uint64_t>& parameter_revision(ToolParameter) const;
    bool set(ToolParameter, double);
    bool set_normalized(ToolParameter, double);
    bool step(ToolParameter, int direction);
    void reset();
    // Activates an independent, lazily initialized brush profile.
    bool select_brush(std::string_view tool);
    [[nodiscard]] std::string serialize() const;
    bool restore(std::string_view);
    [[nodiscard]] PersistentState persistent_state() const;
    [[nodiscard]] static bool valid_persistent_state(const PersistentState&);
    bool restore_persistent_state(const PersistentState&);
    [[nodiscard]] iiSharedCanvas::BitmapBrush brush(std::string_view tool = "brush") const;
    void set_texture(std::string name, int width, int height, std::vector<std::uint8_t> alpha);
    void set_background(std::uint32_t argb);
    ii::core::Observable<std::string> texture_name{"Select texture"};
    ii::core::Observable<ToolValues> values;
  private:
    struct ParameterSignals final {
        std::array<ii::core::Observable<std::uint64_t>, static_cast<std::size_t>(ToolParameter::total)> revisions;
        ToolValues delivered{};
    };
    std::shared_ptr<ParameterSignals> parameter_signals_;
    ii::core::ObservableSubscription values_subscription_;
    std::array<ToolValues, profile_count> profiles_{};
    std::array<bool, profile_count> profile_present_{};
    std::array<TextureState, profile_count> profile_textures_{};
    std::size_t active_profile_ = 0;
    bool multiple_profiles_ = false;
    void select_texture_profile(std::size_t next);
    int texture_width_ = 0, texture_height_ = 0;
    std::vector<std::uint8_t> texture_alpha_;
    std::uint32_t background_ = 0xffffffffU;
};
} // namespace congregation::native
