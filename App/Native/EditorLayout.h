#pragma once
#include "Style.h"
#include <LVUI/IconButtonDefaults.generated.h>
#include <iiGUI/NativeWidgets.h>

namespace congregation::native::editor
{
// Figma 83:6974: the OS title/menu bars stay outside the client composition.
inline constexpr double tool_pair_width = 2 * lvui::icon_button_defaults::extent + Metric::space2;
inline constexpr double tool_rail_width = tool_pair_width + 2 * Metric::space4;
inline constexpr ii::gui::WidgetColor toolbar = Color::surfaceSolid;
inline constexpr ii::gui::WidgetColor workspace = Color::canvas;
struct Fit final
{
    double scale, x, y;
};
inline constexpr double maximum_canvas_zoom = 32.0;
[[nodiscard]] Fit fit(double width, double height, double document_width, double document_height);
[[nodiscard]] ii::gui::Color linear(ii::gui::WidgetColor);
void panel(ii::gui::Widget, ii::gui::WidgetColor);
void brush_preview(ii::gui::Widget, double size, ii::gui::WidgetColor);
void pressure_curve(ii::gui::Widget, double minimum, double center, double maximum);
[[nodiscard]] double pressure_value(double y, double height);
[[nodiscard]] int pressure_point(double x, double width);
} // namespace congregation::native::editor
