#pragma once
#include <iiSharedCanvasNative.h>
#include <string_view>
#include <optional>
#include <vector>
namespace congregation::native
{
struct ShapeStyle final { bool from_center = false, regular = false, fill = true, outline = false; double stroke_width = 1, opacity = 1; };
struct PolygonOptions final { int sides = 5; double rotation_degrees = 0; bool from_center = false, regular = false, fill = true, outline = false; double stroke_width = 1, opacity = 1; };
struct PolygonBounds final { double left, top, width, height; };
std::optional<PolygonBounds> polygon_bounds(iiSharedCanvas::Point, iiSharedCanvas::Point, const PolygonOptions&);
std::vector<iiSharedCanvas::Point> polygon_points(const PolygonBounds&, const PolygonOptions&);
iiSharedCanvas::VectorPath shape_path(std::string_view kind, double width, double height,
                                      std::uint32_t color, double stroke, bool filled, const PolygonOptions& polygon = {},
                                      std::optional<ShapeStyle> style = std::nullopt);
}
