#include "ToolSettings.h"
#include <iiSharedCanvasNative.h>
#include <iiBrushTools/Controls.h>
#include <stdexcept>
#include <numbers>

namespace congregation::native
{
iiSharedCanvas::BitmapBrush ToolSettings::brush(std::string_view tool) const
{
    using P = ToolParameter;
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
    e.rasterizer.noiseEnabled = value(P::noise_enabled) != 0;
    e.rasterizer.noiseStrength = ratio(P::noise_amount);
    e.rasterizer.wetEdgesEnabled = value(P::wet_edges_enabled) != 0;
    e.rasterizer.wetEdgesStrength = ratio(P::wet_edges_strength);
    e.rasterizer.noise.size = value(P::noise_grain_size);
    e.rasterizer.wetEdges.width = value(P::wet_edges_width);
    e.rasterizer.wetEdges.diffusion = ratio(P::wet_edges_diffusion);
    e.rasterizer.buildUp.enabled = value(P::build_up_enabled) != 0;
    e.rasterizer.buildUp.rate = ratio(P::build_up_rate);
    e.rasterizer.buildUp.opacityLimit = ratio(P::build_up_limit);
    e.rasterizer.buildUp.timeBased = value(P::build_up_time) != 0;
    auto& texture = e.material.texture;
    texture.enabled = !texture_alpha_.empty(); texture.width = texture_width_; texture.height = texture_height_;
    texture.alpha = texture_alpha_; texture.invert = value(P::texture_invert) != 0;
    texture.scale = ratio(P::texture_scale); texture.brightness = ratio(P::texture_brightness);
    texture.contrast = ratio(P::texture_contrast); texture.depth = ratio(P::texture_depth);
    texture.minimumDepth = std::min(texture.depth, ratio(P::texture_minimum_depth));
    texture.textureEachTip = value(P::texture_each_tip) != 0;
    texture.blendMode = static_cast<BrushTextureBlendMode>(static_cast<int>(value(P::texture_mode)));
    auto& color = e.rasterizer.color;
    color.secondaryArgb = background_;
    color.mixJitter = ratio(P::color_mix_jitter);
    color.hueJitter = ratio(P::color_hue_jitter);
    color.saturationJitter = ratio(P::color_saturation_jitter);
    color.valueJitter = ratio(P::color_brightness_jitter);
    color.purity = ratio(P::color_purity);
    color.perStroke = value(P::color_apply_per_tip) == 0;
    color.enabled = color.hueJitter != 0 || color.saturationJitter != 0 ||
                    color.valueJitter != 0 || color.purity != 0 || color.mixJitter != 0 || value(P::color_mix_control) != 0;
    e.rasterizer.smoothingEnabled = value(P::smoothing_enabled) != 0;
    e.rasterizer.smoothing = ratio(P::stabilization);
    e.rasterizer.smoothingRadius = value(P::post);
    e.rasterizer.smoothingMode =
        value(P::smoothing_method) != 0 ? BrushSmoothingMode::PulledString : BrushSmoothingMode::Exponential;
    e.rasterizer.smoothingCatchUp = value(P::post) > 0;
    e.rasterizer.smoothingCatchUpOnEnd = value(P::smoothing_catch_up) != 0;
    e.dynamics.pressureToSize = ratio(P::pressure);
    e.dynamics.pressureToFlow = ratio(P::pressure);
    e.dynamics.tiltToEllipse = ratio(P::tilt);
    const auto variation = [&](BrushDynamicsVariation& v, P id) {
        v.jitter = ratio(id);
        v.enabled = v.jitter > 0;
    };
    variation(e.dynamics.sizeVariation, P::shape_size_jitter);
    variation(e.dynamics.angleVariation, P::shape_angle_jitter);
    variation(e.dynamics.roundnessVariation, P::shape_roundness_jitter);
    const auto configure = [&](BrushDynamicsVariation& v, P id) {
        v.control = static_cast<BrushDynamicsControl>(static_cast<int>(value(id)));
        v.enabled = v.enabled || v.control != BrushDynamicsControl::Off;
    };
    configure(e.dynamics.sizeVariation, P::shape_size_control);
    configure(e.dynamics.angleVariation, P::shape_angle_control);
    configure(e.dynamics.roundnessVariation, P::shape_roundness_control);
    e.dynamics.sizeVariation.minimum = ratio(P::shape_minimum_diameter);
    e.dynamics.roundnessVariation.minimum = ratio(P::shape_minimum_roundness);
    if (e.dynamics.sizeVariation.control == BrushDynamicsControl::PenPressure)
        e.dynamics.pressureToSize = 0;
    configure(e.dynamics.scatterVariation, P::scatter_control);
    configure(e.dynamics.scatterCountVariation, P::scatter_count_control);
    variation(e.dynamics.textureDepthVariation, P::texture_depth_jitter);
    configure(e.dynamics.textureDepthVariation, P::texture_depth_control);
    configure(e.dynamics.colorMixVariation, P::color_mix_control);
    const auto transfer = [&](BrushDynamicsVariation& v, P jitter, P control, P minimum) {
        variation(v, jitter);
        configure(v, control);
        v.minimum = ratio(minimum);
    };
    transfer(e.dynamics.flowVariation, P::transfer_flow_jitter, P::transfer_flow_control, P::transfer_flow_minimum);
    if (e.dynamics.flowVariation.control == BrushDynamicsControl::PenPressure)
        e.dynamics.pressureToFlow = 0;
    transfer(e.dynamics.wetnessVariation, P::transfer_wetness_jitter, P::transfer_wetness_control, P::transfer_wetness_minimum);
    transfer(e.dynamics.mixVariation, P::transfer_mix_jitter, P::transfer_mix_control, P::transfer_mix_minimum);
    e.dynamics.flipXJitter = value(P::shape_flip_x) != 0;
    e.dynamics.flipYJitter = value(P::shape_flip_y) != 0;
    e.dynamics.projectTip = value(P::shape_project) != 0;
    auto& pose = e.dynamics.pose;
    pose.tiltX = ratio(P::pose_tilt_x);
    pose.tiltY = ratio(P::pose_tilt_y);
    pose.rotationRadians = value(P::pose_rotation) * std::numbers::pi / 180;
    pose.pressure = ratio(P::pose_pressure);
    pose.overrideTiltX = value(P::pose_override_tilt_x) != 0;
    pose.overrideTiltY = value(P::pose_override_tilt_y) != 0;
    pose.overrideRotation = value(P::pose_override_rotation) != 0;
    pose.overridePressure = value(P::pose_override_pressure) != 0;
    pose.enabled = pose.overrideTiltX || pose.overrideTiltY || pose.overrideRotation || pose.overridePressure;
    BrushDynamicsBinding opacity;
    opacity.source = BrushDynamicsSource::Pressure;
    opacity.target = BrushDynamicsTarget::Opacity;
    const auto minimum = ratio(P::minimum_opacity);
    const auto maximum = ratio(P::maximum_opacity);
    opacity.curve = {true, minimum, (minimum + maximum) / 2, maximum};
    e.dynamics.bindings.push_back(std::move(opacity));
    e.material.scatter.enabled = value(P::scatter) > 0;
    e.material.scatter.radius = ratio(P::scatter);
    e.material.scatter.relativeToSize = true;
    e.material.scatter.axes = value(P::scatter_both_axes) != 0 ? BrushScatterAxes::Both : BrushScatterAxes::Perpendicular;
    e.material.scatter.count = static_cast<std::uint32_t>(value(P::scatter_count));
    e.material.scatter.countJitter = ratio(P::scatter_count_jitter);
    e.material.bristle.count = std::uint32_t(value(P::count));
    e.material.simulation.wetness = ratio(P::wetness);
    e.material.simulation.mixStrength = ratio(P::mix);
    e.material.simulation.load = ratio(P::load);
    e.material.simulation.pickup = ratio(P::dilution);
    e.material.simulation.deposit = 1 - ratio(P::dilution);
    e.material.simulation.enabled = tool == "mixer-brush" || tool == "smudge";
    e.material.simulation.model = tool == "smudge" ? BrushSimulationModel::Smudge : BrushSimulationModel::Mixer;
    if (tool == "smudge") {
        e.material.simulation.smudgeStrength = ratio(P::mix);
        // A smudge moves existing pigment; it does not inject the foreground color.
        e.material.simulation.deposit = 0;
    }
    if (tool == "pencil") {
        auto& grain = e.material.paperGrain;
        grain.enabled = true;
        grain.space = BrushTextureSpace::Document;
        grain.width = grain.height = 1;
        grain.alpha = {128};
        grain.strength = 1;
        grain.blendMode = BrushTextureBlendMode::HardMix;
    } else if (tool == "dry-brush") {
        auto& grain = e.material.paperGrain;
        grain.enabled = true;
        grain.space = BrushTextureSpace::Document;
        grain.width = grain.height = 4;
        grain.alpha = {255, 100, 220, 150, 80, 210, 140, 240,
                       230, 130, 250, 90, 160, 240, 110, 200};
        grain.strength = .6;
        grain.blendMode = BrushTextureBlendMode::Multiply;
    }
    e.rasterizer.airbrushEnabled = tool == "airbrush" || (e.rasterizer.buildUp.enabled && e.rasterizer.buildUp.timeBased);
    if (tool == "bristle-brush") {
        ii::brush::Controls controls;
        if (!controls.set(ii::brush::Parameter::BristleCount, value(P::count)) ||
            !controls.set(ii::brush::Parameter::Hardness, value(P::hardness)))
            throw std::logic_error("Invalid bristle tool settings");
        auto resolved = ii::brush::resolve(ii::brush::Tool::Bristle, controls);
        if (!resolved) throw std::runtime_error(resolved.error().message);
        e.material.bristle = resolved->state.material.bristle;
        auto& tip = resolved->state.rasterizer;
        e.rasterizer.brushWidth = tip.brushWidth;
        e.rasterizer.brushHeight = tip.brushHeight;
        e.rasterizer.brushAlpha = std::move(tip.brushAlpha);
        e.rasterizer.normalizeTipSize = tip.normalizeTipSize;
        e.rasterizer.proceduralTip = tip.proceduralTip;
    }
    b.engineState = std::move(e);
    return b;
}
} // namespace congregation::native
