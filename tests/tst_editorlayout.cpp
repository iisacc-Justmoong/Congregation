#include <Native/EditorLayout.h>
#include <Native/EditorPalette.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
void check(bool b, const char* m)
{
    if (!b)
        throw std::runtime_error(m);
}
int main()
{
    try
    {
        auto f = editor::fit(1341, 692, 1536, 1024);
        check(std::abs(f.scale - 562. / 1024) < 1e-12 && std::abs(f.x - 235) < 1e-9 &&
                  std::abs(f.y - 66) < 1e-9,
              "Figma reference paper framing changed");
        f = editor::fit(1142, 820, 128, 96);
        check(f.scale > 1 && f.scale <= 32 && f.x > 0 && f.y > 0 &&
                  f.x + 128 * f.scale <= 1142 && f.y + 96 * f.scale <= 820,
              "Fit must enlarge a small document within the available viewport");
        f = editor::fit(1142, 820, 1, 1);
        check(f.scale == 32, "Fit must respect the maximum interactive zoom");
        f = editor::fit(640, 480, 640, 480);
        check(f.scale > 0 && f.scale < 1 && f.y >= 24, "Fit lost its canvas margins");
        f = editor::fit(1, 1, 100000, 100000);
        check(std::isfinite(f.scale) && f.scale > 0, "Tiny viewport generated invalid scale");
        f = editor::fit(0, 0, 0, 0);
        check(f.scale == 1, "Invalid viewport must have deterministic fallback");
        check(editor::pressure_value(6, 92) == 1 && editor::pressure_value(46, 92) == .5 &&
                  editor::pressure_value(86, 92) == 0,
              "Pressure graph axis changed");
        check(editor::pressure_value(-100, 92) == 1 && editor::pressure_value(1000, 92) == 0,
              "Pressure graph did not clamp input");
        check(editor::pressure_value(26, 92) == .75 && editor::pressure_value(66, 92) == .25,
              "Pressure graph grid intervals and input mapping diverged");
        check(editor::pressure_value(6, 172) == 1 && editor::pressure_value(86, 172) == .5 &&
                  editor::pressure_value(166, 172) == 0 && editor::pressure_value(0, 12) == 0,
              "Pressure mapping did not follow the allocated view height");
        check(editor::pressure_point(6, 316) == 0 && editor::pressure_point(158, 316) == 1 &&
                  editor::pressure_point(310, 316) == 2,
              "Pressure graph handles are not independently addressable");
        const auto c = editor::linear(editor::workspace);
        check(std::abs(c.red - .005092162612308555) < 1e-9,
              "Workspace must use the shared LVRS canvas color in linear light");
        const auto edge = editor::panel_edge_color();
        check(edge == Color::canvas, "Panel borders must use the LVRS canvas token");
        const auto linear_edge = editor::linear(edge);
        check(std::abs(linear_edge.red - .005092162612308555) < 1e-9 &&
                  std::abs(linear_edge.blue - .005287150125039124) < 1e-9,
              "Panel borders must be converted from shared sRGB to linear light");
        const ii::gui::WidgetColor window{26. / 255, 27. / 255, 29. / 255, 1};
        check(editor::window_background() == window, "Main window must preserve the authored blue accent tint");
        const auto detail = editor::detail_line_color();
        const auto dense = Color::materialSurfaceDenseEdge;
        check(std::abs(detail.red - (dense.red * dense.alpha + window.red * (1-dense.alpha))) < 1e-12 &&
                  std::abs(detail.green - (dense.green * dense.alpha + window.green * (1-dense.alpha))) < 1e-12 &&
                  std::abs(detail.blue - (dense.blue * dense.alpha + window.blue * (1-dense.alpha))) < 1e-12,
              "Timeline details must use the shared dense edge and window");
        check(editor::over_window({1, 1, 1, 0}) == window,
              "A transparent overlay changed the window color");
        check(editor::over_window({1, 0, 0, 1}) == ii::gui::WidgetColor{1, 0, 0, 1},
              "An opaque overlay retained the background tint");
        std::cout << "EDITOR_LAYOUT_OK fit=true bounded_upscale=true srgb=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
