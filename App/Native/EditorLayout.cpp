#include "EditorLayout.h"
#include "Style.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace congregation::native::editor
{
namespace
{
void good(ii::core::Result<void> value)
{
    if (!value)
        throw std::runtime_error(value.error().message);
}
void fill(ii::gui::Widget widget, ii::gui::Rect bounds, ii::gui::WidgetColor color)
{
    ii::gui::WidgetDrawing drawing;
    if (bounds.width > 0 && bounds.height > 0)
        drawing.panels.push_back(
            {{0, 0, bounds.width, bounds.height}, linear(color), linear(color), Metric::radius0});
    good(widget.set_drawing(std::move(drawing)));
}
} // namespace
Fit fit(double width, double height, double document_width, double document_height)
{
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(document_width) ||
        !std::isfinite(document_height) || width <= 0 || height <= 0 || document_width <= 0 ||
        document_height <= 0)
        return {1, 0, 0};
    const double horizontal = std::max<double>(
        Metric::canvasInsetMinimum, std::round(width * Metric::canvasHorizontalInsetRatio));
    const double top = std::max<double>(Metric::canvasInsetMinimum,
                                        std::round(height * Metric::canvasTopInsetRatio));
    const double bottom = std::max<double>(Metric::canvasInsetMinimum,
                                           std::round(height * Metric::canvasBottomInsetRatio));
    const double scale =
        std::max(1. / 65536, std::min({1., std::max(1., width - 2 * horizontal) / document_width,
                                       std::max(1., height - top - bottom) / document_height}));
    return {scale,
            (width - document_width * scale) / 2 -
                (width > Metric::canvasCenterThreshold ? Metric::canvasCenterShift : 0),
            (height - document_height * scale) / 2 + (scale < 1 ? Metric::stroke1 : 0)};
}
ii::gui::Color linear(ii::gui::WidgetColor c)
{
    const auto channel = [](double v)
    { return float(v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4)); };
    return {channel(c.red), channel(c.green), channel(c.blue), float(c.alpha)};
}
void panel(ii::gui::Widget widget, ii::gui::WidgetColor color)
{
    if (auto s = widget.snapshot())
        fill(widget, s->bounds, color);
    good(widget.set_layout_handler([widget, color](ii::gui::Rect r) { fill(widget, r, color); }));
}
void brush_preview(ii::gui::Widget widget, double size, ii::gui::WidgetColor color)
{
    ii::gui::WidgetDrawing drawing;
    const auto diameter = Metric::brushPreviewMinDiameter +
                          std::clamp((size - 1) / 47, 0., 1.) * Metric::brushPreviewDiameterSpan;
    drawing.panels.push_back({{0, 0, Metric::size28, Metric::size28},
                              linear(Color::brushOutlineTop),
                              linear(Color::brushOutlineBottom),
                              Metric::radius14,
                              Metric::stroke1});
    drawing.panels.push_back(
        {{float((Metric::size28 - diameter) / 2), float((Metric::size28 - diameter) / 2),
          float(diameter), float(diameter)},
         linear(color),
         linear(color),
         float(diameter / 2)});
    good(widget.set_drawing(std::move(drawing)));
}
double pressure_value(double y)
{
    return std::clamp(1 - (y - Metric::pressureGraphInset) / Metric::pressureGraphHeight, 0., 1.);
}
int pressure_point(double x, double width) { return x < width / 4 ? 0 : x > width * 3 / 4 ? 2 : 1; }
void pressure_curve(ii::gui::Widget widget, double minimum, double center, double maximum)
{
    const auto snapshot = widget.snapshot();
    if (!snapshot)
        return;
    const double width = snapshot->bounds.width > Metric::space12 ? snapshot->bounds.width
                                                                  : Metric::size316,
                 left = Metric::pressureGraphInset, right = width - Metric::pressureGraphInset;
    const double values[]{minimum, center, maximum};
    const auto primary = linear(Color::curveAccent);
    ii::gui::WidgetDrawing drawing;
    for (int i = 0; i < 5; ++i)
        drawing.panels.push_back(
            {{Metric::space6, float(Metric::pressureGraphInset + i * Metric::pressureGridStep),
              float(width - Metric::space12), Metric::stroke1},
             linear(Color::materialSurfaceDenseEdge),
             linear(Color::materialSurfaceDenseEdge)});
    for (int i = 0; i <= int(right - left); ++i)
    {
        const double position = i / (right - left), segment = position < .5 ? 0 : 1,
                     t = position * 2 - segment;
        const double value = values[int(segment)] * (1 - t) + values[int(segment) + 1] * t;
        drawing.panels.push_back(
            {{float(left + i - Metric::size1_5),
              float(Metric::pressureGraphInset + (1 - value) * Metric::pressureGraphHeight -
                    Metric::size1_5),
              Metric::size3, Metric::size3},
             primary,
             primary,
             Metric::radius1_5});
    }
    for (int i = 0; i < 3; ++i)
    {
        const auto x = left + i * (right - left) / 2,
                   y = Metric::pressureGraphInset + (1 - values[i]) * Metric::pressureGraphHeight;
        drawing.panels.push_back({{float(x - Metric::space6), float(y - Metric::space6),
                                   Metric::space12, Metric::space12},
                                  linear(Color::curveHandle),
                                  linear(Color::curveHandle),
                                  Metric::radius6});
        drawing.panels.push_back({{float(x - Metric::space6), float(y - Metric::space6),
                                   Metric::space12, Metric::space12},
                                  primary,
                                  primary,
                                  Metric::radius6,
                                  Metric::stroke2});
    }
    good(widget.set_drawing(std::move(drawing)));
}
void tool_button(ii::gui::Widget widget, bool selected)
{
    good(widget.set_button_style({.fill = selected ? Color::toolSelected : Color::transparent,
                                  .hover_fill = Color::toolHover,
                                  .pressed_fill = Color::toolPressed}));
}
} // namespace congregation::native::editor
