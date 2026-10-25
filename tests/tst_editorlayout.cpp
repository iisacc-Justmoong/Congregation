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
        check(f.scale == 1 && f.x == 493 && f.y == 362,
              "Small documents must fit without upscaling");
        f = editor::fit(640, 480, 640, 480);
        check(f.scale > 0 && f.scale < 1 && f.y >= 24, "Fit lost its canvas margins");
        f = editor::fit(1, 1, 100000, 100000);
        check(std::isfinite(f.scale) && f.scale > 0, "Tiny viewport generated invalid scale");
        f = editor::fit(0, 0, 0, 0);
        check(f.scale == 1, "Invalid viewport must have deterministic fallback");
        check(editor::pressure_value(6) == 1 && editor::pressure_value(46) == .5 &&
                  editor::pressure_value(86) == 0,
              "Pressure graph axis changed");
        check(editor::pressure_value(-100) == 1 && editor::pressure_value(1000) == 0,
              "Pressure graph did not clamp input");
        check(editor::pressure_point(6, 316) == 0 && editor::pressure_point(158, 316) == 1 &&
                  editor::pressure_point(310, 316) == 2,
              "Pressure graph handles are not independently addressable");
        const auto c = editor::linear(editor::workspace);
        check(std::abs(c.red - .00699541) < 1e-6,
              "Workspace encoded sRGB was treated as linear light");
        const auto edge = editor::panel_edge_color();
        check(std::abs(edge.red * 255 - 48.2) < 1e-5 && std::abs(edge.green * 255 - 50.84) < 1e-5 &&
                  std::abs(edge.blue * 255 - 52.6) < 1e-5 && edge.alpha == 1,
              "Figma sRGB edge was not composited before GPU conversion");
        check(editor::over_window({1, 1, 1, 0}) == editor::window_background,
              "A transparent overlay changed the window color");
        check(editor::over_window({1, 0, 0, 1}) == ii::gui::WidgetColor{1, 0, 0, 1},
              "An opaque overlay retained the background tint");
        std::cout << "EDITOR_LAYOUT_OK fit=true no_upscale=true srgb=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
