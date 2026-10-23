#pragma once
#include <iiGUI/NativeWidgets.h>

namespace congregation::native::editor
{
// Vincent's client area: platform title/menu bars are outside these coordinates.
inline constexpr double toolbar_height = 40, sidebar_width = 260;
inline constexpr ii::gui::WidgetColor toolbar{31./255,31./255,31./255,1};
inline constexpr ii::gui::WidgetColor sidebar{36./255,38./255,37./255,1};
inline constexpr ii::gui::WidgetColor workspace{20./255,20./255,20./255,1};
struct Fit final { double scale, x, y; };
[[nodiscard]] Fit fit(double width, double height, double document_width, double document_height);
[[nodiscard]] ii::gui::Color linear(ii::gui::WidgetColor);
void panel(ii::gui::Widget, ii::gui::WidgetColor);
void brush_preview(ii::gui::Widget, double size, ii::gui::WidgetColor);
void pressure_curve(ii::gui::Widget, double minimum, double center, double maximum);
[[nodiscard]] double pressure_value(double y);
[[nodiscard]] int pressure_point(double x, double width);
void tool_button(ii::gui::Widget, bool selected);
} // namespace congregation::native::editor
