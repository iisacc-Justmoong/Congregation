#include "Shapes.h"
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace congregation::native
{
using namespace iiSharedCanvas;
VectorPath shape_path(std::string_view kind, double w, double h, std::uint32_t color, double stroke,
                      bool filled)
{
    VectorPath path;
    if (filled)
        path.fill = SolidPaint{color};
    else
        path.stroke = StrokeStyle{{color}, stroke};
    const auto polygon = [&](std::initializer_list<Point> points)
    {
        bool first = true;
        for (auto p : points)
        {
            if (first)
                path.commands.emplace_back(MoveTo{p});
            else
                path.commands.emplace_back(LineTo{p});
            first = false;
        }
        path.commands.emplace_back(ClosePath{});
    };
    if (kind == "line")
        path.commands = {MoveTo{{0, 0}}, LineTo{{w, h}}};
    else if (kind == "ellipse")
    {
        const double rx = w / 2, ry = h / 2, k = .5522847498307936;
        path.commands = {MoveTo{{w, ry}},
                         CubicTo{{w, ry + k * ry}, {rx + k * rx, h}, {rx, h}},
                         CubicTo{{rx - k * rx, h}, {0, ry + k * ry}, {0, ry}},
                         CubicTo{{0, ry - k * ry}, {rx - k * rx, 0}, {rx, 0}},
                         CubicTo{{rx + k * rx, 0}, {w, ry - k * ry}, {w, ry}},
                         ClosePath{}};
    }
    else if (kind == "triangle")
        polygon({{w / 2, 0}, {w, h}, {0, h}});
    else if (kind == "diamond")
        polygon({{w / 2, 0}, {w, h / 2}, {w / 2, h}, {0, h / 2}});
    else if (kind == "star")
    {
        for (int i = 0; i < 10; ++i)
        {
            const auto angle = -std::numbers::pi / 2 + i * std::numbers::pi / 5;
            const auto factor = i % 2 ? .45 : 1.;
            Point p{w / 2 + std::cos(angle) * w / 2 * factor,
                    h / 2 + std::sin(angle) * h / 2 * factor};
            if (i == 0)
                path.commands.emplace_back(MoveTo{p});
            else
                path.commands.emplace_back(LineTo{p});
        }
        path.commands.emplace_back(ClosePath{});
    }
    else if (kind == "rectanglebubble")
    {
        const auto body = std::max(1., h - std::min(std::max(4., h * .24), h * .35));
        polygon(
            {{0, 0}, {w, 0}, {w, body}, {w * .44, body}, {w * .18, h}, {w * .26, body}, {0, body}});
    }
    else if (kind == "ellipsebubble")
    {
        const auto body = std::max(1., h - std::min(std::max(4., h * .24), h * .35));
        for (int i = 0; i <= 32; ++i)
        {
            const auto angle = 2.15 + (1.70 + 2 * std::numbers::pi - 2.15) * i / 32;
            Point p{w / 2 + std::cos(angle) * w / 2, body / 2 + std::sin(angle) * body / 2};
            if (i == 0)
                path.commands.emplace_back(MoveTo{p});
            else
                path.commands.emplace_back(LineTo{p});
        }
        path.commands.emplace_back(LineTo{{w * .18, h}});
        path.commands.emplace_back(ClosePath{});
    }
    else if (kind == "rectangle")
        polygon({{0, 0}, {w, 0}, {w, h}, {0, h}});
    else
        throw std::invalid_argument("Unsupported shape kind");
    return path;
}
} // namespace congregation::native
