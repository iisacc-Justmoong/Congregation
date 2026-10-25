#pragma once
#include "Style.h"
#include <iiGUI/NativeWidgets.h>

namespace congregation::native::editor
{
// Figma 83:6974: the OS title/menu bars stay outside the client composition.
inline constexpr double toolbar_height = Metric::toolbarHeight,
                        sidebar_width = Metric::sidebarWidth,
                        tool_rail_width = Metric::toolRailWidth,
                        inspector_width = Metric::inspectorWidth,
                        timeline_height = Metric::timelineHeight;
inline constexpr ii::gui::WidgetColor toolbar = Color::toolbar;
inline constexpr ii::gui::WidgetColor sidebar = Color::popup;
inline constexpr ii::gui::WidgetColor workspace = Color::workspace;
struct Fit final
{
    double scale, x, y;
};
[[nodiscard]] Fit fit(double width, double height, double document_width, double document_height);
[[nodiscard]] ii::gui::Color linear(ii::gui::WidgetColor);
void panel(ii::gui::Widget, ii::gui::WidgetColor);
void brush_preview(ii::gui::Widget, double size, ii::gui::WidgetColor);
void pressure_curve(ii::gui::Widget, double minimum, double center, double maximum);
[[nodiscard]] double pressure_value(double y);
[[nodiscard]] int pressure_point(double x, double width);
void tool_button(ii::gui::Widget, bool selected);
} // namespace congregation::native::editor
