#include "EditorPalette.h"
#include <cmath>

namespace congregation::native::editor
{
ii::gui::WidgetColor window_background()
{
    const auto base = Color::window;
    const auto accent = Color::blue;
    const auto mix = [](double background, double primary)
    { return std::round((background * .98 + primary * .02) * 255) / 255; };
    return {mix(base.red, accent.red), mix(base.green, accent.green), mix(base.blue, accent.blue), 1};
}
ii::gui::WidgetColor over_window(ii::gui::WidgetColor color)
{
    const auto background = window_background();
    const auto channel = [&](double foreground, double background)
    { return foreground * color.alpha + background * (1 - color.alpha); };
    return {channel(color.red, background.red),
            channel(color.green, background.green),
            channel(color.blue, background.blue), 1};
}
ii::gui::WidgetColor panel_edge_color() { return Color::canvas; }
ii::gui::WidgetColor detail_line_color() { return over_window(Color::materialSurfaceDenseEdge); }
} // namespace congregation::native::editor
