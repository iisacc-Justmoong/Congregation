#include "CanvasToolBindings.h"
#include "Canvas/CanvasView.h"
#include "Workspace/EditorBackend.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace congregation::native {
CanvasToolBindings::CanvasToolBindings(ui::EditorBackend& editor, CanvasView& canvas)
{
    subscriptions_.reserve(28);
    const auto observe = [this]<class T, class F>(const ii::core::Observable<T>& source, F apply) {
        subscriptions_.push_back(source.observe(
            [apply = std::move(apply)](const T& value, std::uint64_t) { apply(value); }, true));
    };
    const auto link = [&observe]<class T>(const ii::core::Observable<T>& source, T& target) {
        observe(source, [destination = &target](const T& value) { *destination = value; });
    };

    link(editor.brush_size, canvas.brush_size);
    link(editor.opacity, canvas.opacity);
    link(editor.text_value, canvas.text);
    link(editor.selection_feather, canvas.selection_feather);
    link(editor.magnetic_radius, canvas.magnetic_radius);
    link(editor.clone_aligned, canvas.clone_aligned);
    link(editor.clone_current_layer, canvas.clone_current_layer);
    observe(editor.polygon_sides, [&canvas](std::int64_t value) {
        canvas.polygon_options.sides = static_cast<int>(std::clamp<std::int64_t>(value, 3, 64));
    });
    link(editor.polygon_rotation, canvas.polygon_options.rotation_degrees);
    link(editor.polygon_center, canvas.polygon_options.from_center);
    link(editor.polygon_fill, canvas.polygon_options.fill);
    link(editor.polygon_outline, canvas.polygon_options.outline);
    link(editor.polygon_stroke_width, canvas.polygon_options.stroke_width);
    link(editor.polygon_opacity, canvas.polygon_options.opacity);
    observe(editor.crop_aspect, [&canvas](std::int64_t value) {
        canvas.crop_aspect = value == 1 ? 1. : value == 2 ? 4. / 3. : value == 3 ? 16. / 9. : 0.;
    });
    link(editor.crop_destructive, canvas.crop_destructive);
    link(editor.selection_antialias, canvas.selection_antialias);
    link(editor.wand_tolerance, canvas.wand_tolerance);
    link(editor.wand_contiguous, canvas.wand_contiguous);
    link(editor.wand_current_layer, canvas.wand_current_layer);
    link(editor.fill_tolerance, canvas.fill_tolerance);
    link(editor.fill_contiguous, canvas.fill_contiguous);
    link(editor.fill_current_layer, canvas.fill_current_layer);
    link(editor.fill_target_current, canvas.fill_target_current);
    link(editor.fill_preserve_alpha, canvas.fill_preserve_alpha);
    link(editor.fill_antialias, canvas.fill_antialias);

    const auto gradient = editor.gradient_settings;
    observe(gradient->revision, [gradient, &canvas](std::uint64_t) {
        canvas.gradient_stops = gradient->stops();
        canvas.gradient_kind = gradient->kind();
        canvas.gradient_spread = gradient->spread();
        canvas.gradient_blend = gradient->blend();
    });
    observe(editor.current_color, [gradient, &canvas](const ii::gui::WidgetColor& color) {
        const auto channel = [](double value) { return std::uint32_t(std::lround(value * 255)); };
        canvas.color = (channel(color.alpha) << 24) | (channel(color.red) << 16) |
                       (channel(color.green) << 8) | channel(color.blue);
        gradient->set_default_start(canvas.color);
    });
}
}
