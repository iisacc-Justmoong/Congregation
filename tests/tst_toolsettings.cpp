#include <Native/ToolSettings.h>
#include <iiSharedCanvasNative.h>
#include <Native/TimelineModel.h>
#include <Brush/BrushDynamics.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;
void require(bool b, const char* m)
{
    if (!b)
        throw std::runtime_error(m);
}

int main()
{
    try
    {
        ToolSettings settings;
        require(ToolSettings::specs().size() >= 34,
                "The Tool panel must expose pose and scattering settings");
        require(settings.value(ToolParameter::size) == 64 &&
                    settings.label(ToolParameter::hardness) == "85%",
                "Figma parameter defaults changed");
        require(std::abs(settings.normalized(ToolParameter::size) - .6) < 1e-10,
                "Brush size must use a logarithmic slider");
        require(settings.set_normalized(ToolParameter::size, .7) &&
                    settings.value(ToolParameter::size) == 128,
                "Dragging size must update the physical value");
        settings.step(ToolParameter::size, -1);
        require(settings.value(ToolParameter::size) == 127,
                "Step buttons must adjust one physical unit");
        settings.set(ToolParameter::count, 1000);
        require(settings.value(ToolParameter::count) == 64, "Discrete parameter upper bound lost");
        settings.set(ToolParameter::count, -5);
        require(settings.value(ToolParameter::count) == 1, "Discrete parameter lower bound lost");
        auto before = settings.serialize();
        require(!settings.set(ToolParameter::opacity, std::numeric_limits<double>::quiet_NaN()) &&
                    settings.serialize() == before,
                "Invalid input must preserve settings");
        ToolSettings restored;
        require(restored.restore(before) && restored.serialize() == before,
                "Tool settings must round trip without a document");
        require(!restored.restore("congregation.tools=1\nsize=12\n") &&
                    restored.serialize() == before,
                "Partial or malformed preferences must be atomic");
        auto brush = settings.brush();
        using P = ToolParameter;
        settings.set(P::pose_tilt_x, -35);
        settings.set(P::pose_tilt_y, 42);
        settings.set(P::pose_rotation, 90);
        settings.set(P::pose_pressure, 30);
        settings.set(P::pose_override_tilt_x, 1);
        settings.set(P::pose_override_rotation, 1);
        settings.set(P::pose_override_pressure, 1);
        settings.set(P::scatter, 30);
        settings.set(P::scatter_both_axes, 0);
        settings.set(P::scatter_count, 3);
        settings.set(P::scatter_count_jitter, 40);
        settings.set(P::noise_enabled, 1);
        settings.set(P::noise_amount, 20);
        settings.set(P::wet_edges_enabled, 1);
        settings.set(P::wet_edges_strength, 40);
        settings.set(P::shape_size_jitter, 25);
        settings.set(P::shape_size_control, 2);
        settings.set(P::shape_minimum_diameter, 15);
        settings.set(P::shape_angle_jitter, 30);
        settings.set(P::shape_roundness_jitter, 20);
        settings.set(P::shape_flip_x, 1);
        settings.set(P::shape_project, 1);
        settings.set(P::color_hue_jitter, 35);
        settings.set(P::color_purity, -25);
        settings.set(P::color_apply_per_tip, 1);
        settings.set(P::transfer_flow_control, 2);
        settings.set(P::transfer_flow_minimum, 25);
        settings.set(P::transfer_wetness_control, 2);
        settings.set(P::transfer_mix_control, 2);
        settings.set(P::smoothing_enabled, 1);
        settings.set(P::smoothing_method, 0);
        settings.set(P::smoothing_catch_up, 0);
        settings.set(P::pose_override_pressure, 0);
        const auto transfer_brush = settings.brush("mixer-brush");
        BrushDynamicsInput low_pressure, high_pressure;
        low_pressure.pressure = 0; high_pressure.pressure = 1;
        auto low_transfer = resolveBrushDynamics(transfer_brush.engineState->dynamics, low_pressure);
        auto high_transfer = resolveBrushDynamics(transfer_brush.engineState->dynamics, high_pressure);
        require(low_transfer.flowScale < high_transfer.flowScale && low_transfer.wetnessScale < high_transfer.wetnessScale && low_transfer.mixScale < high_transfer.mixScale,
                "Transfer controls must change engine flow, wetness and mix with pressure");
        require(transfer_brush.engineState->rasterizer.smoothingEnabled &&
                transfer_brush.engineState->rasterizer.smoothingMode == BrushSmoothingMode::Exponential &&
                !transfer_brush.engineState->rasterizer.smoothingCatchUpOnEnd,
                "Smoothing controls must independently select mode and catch-up");
        settings.set(P::pose_override_pressure, 1);
        settings.set_texture("test texture", 2, 2, {0, 64, 128, 255});
        bool invalid_texture = false;
        try { settings.set_texture("invalid", 4097, 1, std::vector<std::uint8_t>(4097)); }
        catch (const std::invalid_argument&) { invalid_texture = true; }
        require(invalid_texture && settings.texture_name.get() == "test texture",
                "Invalid texture must not replace the selected asset");
        settings.set_background(0xff123456U);
        settings.set(P::texture_scale, 200);
        settings.set(P::texture_invert, 1);
        settings.set(P::texture_brightness, 20);
        settings.set(P::texture_depth, 60);
        settings.set(P::texture_depth_jitter, 40);
        settings.set(P::color_mix_jitter, 50);
        settings.set(P::noise_grain_size, 4);
        settings.set(P::wet_edges_width, 3);
        settings.set(P::wet_edges_diffusion, 20);
        settings.set(P::build_up_enabled, 1);
        settings.set(P::build_up_rate, 25);
        settings.set(P::build_up_limit, 80);
        settings.set(P::build_up_time, 1);
        const auto feature_brush = settings.brush();
        require(feature_brush.engineState->material.texture.enabled && feature_brush.engineState->material.texture.alpha.size() == 4 &&
                feature_brush.engineState->material.texture.invert && std::abs(feature_brush.engineState->material.texture.scale-2)<1e-8 &&
                std::abs(feature_brush.engineState->material.texture.depth-.6)<1e-8,
                "Texture asset and controls must reach paint engine");
        require(feature_brush.engineState->rasterizer.color.enabled && feature_brush.engineState->rasterizer.color.secondaryArgb == 0xff123456U &&
                std::abs(feature_brush.engineState->rasterizer.color.mixJitter-.5)<1e-8,
                "Background and color mixing dynamics must reach paint engine");
        require(feature_brush.engineState->rasterizer.noise.size == 4 && feature_brush.engineState->rasterizer.wetEdges.width == 3 &&
                feature_brush.engineState->rasterizer.buildUp.enabled && feature_brush.engineState->rasterizer.airbrushEnabled &&
                std::abs(feature_brush.engineState->rasterizer.buildUp.rate-.25)<1e-8,
                "Material effects must reach paint engine");
        const auto advanced = settings.brush();
        const auto& pose = advanced.engineState->dynamics.pose;
        const auto& scatter = advanced.engineState->material.scatter;
        require(pose.enabled && pose.overrideTiltX && !pose.overrideTiltY &&
                    pose.overrideRotation && pose.overridePressure &&
                    std::abs(pose.tiltX + .35) < 1e-8 && std::abs(pose.tiltY - .42) < 1e-8 &&
                    std::abs(pose.rotationRadians - std::acos(-1.)/2) < 1e-8 &&
                    std::abs(pose.pressure - .3) < 1e-8,
                "Brush pose controls must reach engine pose input overrides");
        BrushDynamicsInput pen;
        pen.pressure=.9; pen.tiltX=.8; pen.tiltY=.25; pen.rotationRadians=0;
        const auto overridden=applyBrushPose(pose, pen);
        require(std::abs(overridden.pressure-.3)<1e-8 && std::abs(overridden.tiltX+.35)<1e-8 &&
                    std::abs(overridden.tiltY-.25)<1e-8 &&
                    std::abs(overridden.rotationRadians-std::acos(-1.)/2)<1e-8,
                "Pose must override only explicitly selected pen channels");
        require(scatter.enabled && scatter.axes == BrushScatterAxes::Perpendicular &&
                    scatter.count == 3 && std::abs(scatter.countJitter - .4) < 1e-8,
                "Scattering controls must reach the engine scatter state");
        require(advanced.engineState->rasterizer.noiseEnabled &&
                    std::abs(advanced.engineState->rasterizer.noiseStrength-.2)<1e-8 &&
                    advanced.engineState->rasterizer.wetEdgesEnabled &&
                    std::abs(advanced.engineState->rasterizer.wetEdgesStrength-.4)<1e-8,
                "Noise and wet edge controls must reach the rasterizer");
        const auto& dynamics=advanced.engineState->dynamics;
        require(dynamics.sizeVariation.enabled && std::abs(dynamics.sizeVariation.jitter-.25)<1e-8 &&
                    dynamics.sizeVariation.control == BrushDynamicsControl::PenPressure &&
                    std::abs(dynamics.sizeVariation.minimum-.15)<1e-8 &&
                    dynamics.angleVariation.enabled && std::abs(dynamics.angleVariation.jitter-.3)<1e-8 &&
                    dynamics.roundnessVariation.enabled && std::abs(dynamics.roundnessVariation.jitter-.2)<1e-8 &&
                    dynamics.flipXJitter && !dynamics.flipYJitter && dynamics.projectTip,
                "Shape dynamics controls must reach the brush engine");
        const auto& color=advanced.engineState->rasterizer.color;
        require(color.enabled && !color.perStroke && std::abs(color.hueJitter-.35)<1e-8 &&
                    std::abs(color.purity+.25)<1e-8,
                "Color dynamics controls must reach brush color settings");
        const auto savedAdvanced = settings.serialize();
        ToolSettings advancedRestore;
        require(advancedRestore.restore(savedAdvanced) && advancedRestore.serialize() == savedAdvanced,
                "Pose and scattering settings must persist");
        auto prePose = savedAdvanced;
        for (const auto& spec : ToolSettings::specs()) {
            if (static_cast<std::size_t>(spec.id) < static_cast<std::size_t>(P::pose_tilt_x)) continue;
            const auto begin = prePose.find(std::string(spec.key) + "=");
            require(begin != std::string::npos, "new preference key missing");
            const auto end = prePose.find('\n', begin);
            prePose.erase(begin, end - begin + 1);
        }
        require(advancedRestore.restore(prePose) &&
                    advancedRestore.value(P::pose_override_pressure) == 0 &&
                    advancedRestore.value(P::scatter_count) == 1 &&
                    advancedRestore.value(P::noise_enabled) == 0,
                "Older preferences must load with safe advanced defaults");
        settings.set(P::pose_override_pressure, 0);
        require(std::abs(brush.hardness - .85) < 1e-10 && std::abs(brush.flow - .9) < 1e-10,
                "Controls are disconnected from the paint engine");
        settings.set(ToolParameter::minimum_opacity, 20);
        settings.set(ToolParameter::maximum_opacity, 80);
        brush = settings.brush();
        for (double pressure : {0.0, 0.5, 1.0}) {
            BrushDynamicsInput motion;
            motion.pressure = pressure;
            motion.velocity = 0;
            const auto slow = resolveBrushDynamics(brush.engineState->dynamics, motion).opacityScale;
            motion.velocity = 5000;
            const auto fast = resolveBrushDynamics(brush.engineState->dynamics, motion).opacityScale;
            require(std::abs(slow - (.2 + .6 * pressure)) < 1e-8 && std::abs(slow-fast)<1e-8,
                    "Opacity must follow pressure bounds and remain independent of velocity");
        }
        settings.set(ToolParameter::minimum_opacity, 90);
        require(settings.value(ToolParameter::maximum_opacity) == 90,
                "Raising minimum must preserve an ordered opacity range");
        settings.set(ToolParameter::maximum_opacity, 30);
        require(settings.value(ToolParameter::minimum_opacity) == 30,
                "Lowering maximum must preserve an ordered opacity range");
        const auto updated = settings.serialize();
        require(restored.restore(updated) && restored.serialize() == updated,
                "Opacity bounds must round trip");
        auto legacy = updated;
        for (const auto key : {"minimum-opacity=30\n", "maximum-opacity=30\n"}) {
            const auto pos = legacy.find(key);
            require(pos != std::string::npos, "Missing opacity field");
            legacy.erase(pos, std::string_view(key).size());
        }
        legacy += "velocity=10\n";
        require(restored.restore(legacy) && restored.value(ToolParameter::minimum_opacity) == 0 &&
                    restored.value(ToolParameter::maximum_opacity) == 100,
                "Legacy settings must migrate without speed opacity");
        const auto migrated = restored.serialize();
        auto invalid = updated;
        invalid.replace(invalid.find("minimum-opacity=30"), std::string_view("minimum-opacity=30").size(),
                        "minimum-opacity=90");
        require(!restored.restore(invalid) && restored.serialize() == migrated,
                "Inverted saved bounds must be rejected atomically");
        settings.set(ToolParameter::angle, 90);
        settings.set(ToolParameter::roundness, 50);
        brush = settings.brush();
        require(std::abs(brush.engineState->rasterizer.shape.angleRadians - std::acos(-1.) / 2) <
                        1e-10 &&
                    brush.engineState->rasterizer.shape.roundness == .5,
                "Tip shape parameters do not reach iiPaintEngine");
        settings.set(ToolParameter::count, 8);
        auto bristle = settings.brush("bristle-brush");
        require(bristle.engineState && bristle.engineState->material.bristle.enabled &&
                    bristle.engineState->material.bristle.count == 8 &&
                    !bristle.engineState->rasterizer.brushAlpha.empty(),
                "Bristle tool must generate a count-dependent brush tip");
        auto smudge = settings.brush("smudge");
        require(smudge.engineState && smudge.engineState->material.simulation.enabled &&
                    smudge.engineState->material.simulation.model == BrushSimulationModel::Smudge &&
                    std::abs(smudge.engineState->material.simulation.smudgeStrength - .45) < 1e-10,
                "Smudge tool must apply the configured mix strength");
        auto mixer = settings.brush("mixer-brush");
        require(mixer.engineState && mixer.engineState->material.simulation.enabled &&
                    mixer.engineState->material.simulation.model == BrushSimulationModel::Mixer,
                "Mixer tool must use the mixer material");
        auto spray = settings.brush("airbrush");
        require(spray.engineState && spray.engineState->rasterizer.airbrushEnabled,
                "Airbrush must use time-based placement");
        require(!settings.brush("brush").engineState->material.simulation.enabled,
                "Ordinary brush inherited another tool's simulation");
        TimelineModel timeline;
        require(timeline.time_text() == "00:01.000", "Timeline must display elapsed time");
        timeline.scrub(.251);
        require(timeline.frame() == 6, "Timeline dragging must snap to frame ticks");
        timeline.scrub(100);
        require(timeline.frame() == 48, "Timeline end clamp lost");
        timeline.scrub(-1);
        require(timeline.frame() == 0, "Timeline start clamp lost");
        timeline.set_rate(30);
        timeline.scrub(1);
        require(timeline.frame() == 30 && timeline.time_text() == "00:01.000",
                "FPS changes must preserve time units");
        require(TimelineModel::format_time(3661.125) == "61:01.125",
                "Long timeline formatting lost minute precision");
        std::cout << "TOOL_SETTINGS_OK parameters=" << ToolSettings::specs().size()
                  << " log_size=true limits=true persistent=true "
                     "engine=true time_ticks=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
