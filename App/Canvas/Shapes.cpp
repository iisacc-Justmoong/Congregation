#include "Shapes.h"
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace congregation::native
{
using namespace iiSharedCanvas;
std::optional<PolygonBounds> polygon_bounds(Point from, Point to, const PolygonOptions& options)
{
    if(options.sides<3 || options.sides>64 || !std::isfinite(options.rotation_degrees) ||
       !std::isfinite(options.stroke_width) || options.stroke_width<0 || options.stroke_width>256 ||
       !std::isfinite(options.opacity) || options.opacity<0 || options.opacity>1 ||
       (!options.fill && (!options.outline || options.stroke_width == 0)) ||
       !std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y))return std::nullopt;
    auto dx=to.x-from.x,dy=to.y-from.y;
    if(!std::isfinite(dx) || !std::isfinite(dy))return std::nullopt;
    if(options.regular) {const auto side=std::max(std::abs(dx),std::abs(dy));dx=std::copysign(side,dx);dy=std::copysign(side,dy);}
    const auto x0=options.from_center ? from.x-std::abs(dx) : std::min(from.x,from.x+dx);
    const auto x1=options.from_center ? from.x+std::abs(dx) : std::max(from.x,from.x+dx);
    const auto y0=options.from_center ? from.y-std::abs(dy) : std::min(from.y,from.y+dy);
    const auto y1=options.from_center ? from.y+std::abs(dy) : std::max(from.y,from.y+dy);
    const auto w=x1-x0,h=y1-y0;
    if(!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(w) || !std::isfinite(h) || w<=0 || h<=0 || w>32768 || h>32768)return std::nullopt;
    return PolygonBounds{x0,y0,w,h};
}
std::vector<Point> polygon_points(const PolygonBounds& bounds, const PolygonOptions& options)
{
    if(options.sides<3 || options.sides>64 || !std::isfinite(options.rotation_degrees))throw std::invalid_argument("Invalid polygon settings");
    std::vector<Point> result;result.reserve(std::size_t(options.sides));
    const auto rotation=std::remainder(options.rotation_degrees,360.)*std::numbers::pi/180.;
    for(int i=0;i<options.sides;++i) {
        const auto angle=-std::numbers::pi/2+rotation+2*std::numbers::pi*i/options.sides;
        result.push_back({bounds.left+bounds.width*(.5+.5*std::cos(angle)),bounds.top+bounds.height*(.5+.5*std::sin(angle))});
    }
    return result;
}
VectorPath shape_path(std::string_view kind, double w, double h, std::uint32_t color, double stroke,
                      bool filled, const PolygonOptions& options, std::optional<ShapeStyle> style)
{
    VectorPath path;
    if (filled)
        path.fill = SolidPaint{color};
    else
        path.stroke = StrokeStyle{{color}, stroke};
    if(style) {
        path.fill.reset();path.stroke.reset();
        if(style->fill)path.fill=SolidPaint{color};
        if(style->outline && style->stroke_width>0)path.stroke=StrokeStyle{{color},style->stroke_width};
    }
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
    if (kind == "polygon") {
        path.fill.reset();path.stroke.reset();
        if(options.fill)path.fill=SolidPaint{color};
        if(options.outline && options.stroke_width > 0)path.stroke=StrokeStyle{{color},options.stroke_width};
        const auto points=polygon_points({0,0,w,h},options);
        for(std::size_t i=0;i<points.size();++i) {
            if(i==0)path.commands.emplace_back(MoveTo{points[i]});
            else path.commands.emplace_back(LineTo{points[i]});
        }
        path.commands.emplace_back(ClosePath{});
    }
    else if (kind == "line")
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
