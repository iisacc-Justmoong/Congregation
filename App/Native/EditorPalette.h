#pragma once
#include "Style.h"
#include <iiGUI/Appearance.h>

namespace congregation::native::editor
{
// Compose the LVUI window and accent tokens with the authored window tint.
[[nodiscard]] ii::gui::WidgetColor window_background();
// Panel edges and internal ruler/timeline details have separate color roles.
inline constexpr ii::gui::WidgetColor panel_edge = Color::canvas;
// LVRS Panel/surfacePanel, used by the dedicated divider in 77:19369.
inline constexpr ii::gui::WidgetColor surface_panel = Color::surfacePanel;

// Resolve an authored sRGB overlay on the known window before converting it
// to iiGUI's linear-light graphics colors. Other GPU blending stays unchanged.
[[nodiscard]] ii::gui::WidgetColor over_window(ii::gui::WidgetColor);
[[nodiscard]] ii::gui::WidgetColor panel_edge_color();
[[nodiscard]] ii::gui::WidgetColor detail_line_color();
} // namespace congregation::native::editor
