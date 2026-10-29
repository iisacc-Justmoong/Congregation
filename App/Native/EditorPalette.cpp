#include "EditorPalette.h"

namespace congregation::native::editor
{
ii::gui::WidgetColor over_window(ii::gui::WidgetColor color)
{
    const auto channel = [&](double foreground, double background)
    { return foreground * color.alpha + background * (1 - color.alpha); };
    return {channel(color.red, window_background.red),
            channel(color.green, window_background.green),
            channel(color.blue, window_background.blue), 1};
}
ii::gui::WidgetColor panel_edge_color() { return Color::panelEdgeResolved; }
ii::gui::WidgetColor detail_line_color() { return Color::detailLineResolved; }
} // namespace congregation::native::editor
