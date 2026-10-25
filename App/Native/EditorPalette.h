#pragma once
#include "Style.h"
#include <iiGUI/Appearance.h>

namespace congregation::native::editor
{
// Figma 83:6979: median rendered ApplicationWindow color, including its
// 2% accent and fine grain. Structural panels inherit this opaque backdrop.
inline constexpr ii::gui::WidgetColor window_background = Color::window;
// LVRS Surface/Dense/Edge, as authored in 84:7063 and 130:1852.
inline constexpr ii::gui::WidgetColor panel_edge = Color::materialSurfaceDenseEdge;
// LVRS Panel/surfacePanel, used by the dedicated divider in 77:19369.
inline constexpr ii::gui::WidgetColor surface_panel = Color::surfacePanel;

// Resolve an authored sRGB overlay on the known window before converting it
// to iiGUI's linear-light graphics colors. Other GPU blending stays unchanged.
[[nodiscard]] ii::gui::WidgetColor over_window(ii::gui::WidgetColor);
[[nodiscard]] ii::gui::WidgetColor panel_edge_color();
} // namespace congregation::native::editor
