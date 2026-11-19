#pragma once
#include "Shared/Style.h"
#include <iiGUI>
#include <iiPaintEngine>
#include <iiSDKCore>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>

namespace congregation::native
{
// Rasterizes the two cursor graphics independently of artwork and retains
// their images so moving the pointer only changes image placement.
class CanvasPointerDrawing final
{
  public:
    struct Tip final {
        double diameter, roundness, angle;
        BrushTipShape shape;
        friend bool operator==(const Tip&, const Tip&) = default;
    };

    struct Sample final {
        std::uint32_t sampled, previous;
        friend bool operator==(const Sample&, const Sample&) = default;
    };

    // The diameter is expressed in viewport points and includes document zoom.
    std::optional<ii::gui::ImageRun> brush(Tip tip, ii::gui::WidgetPoint point, ii::gui::Size viewport)
    {
        if (!std::isfinite(tip.diameter) || tip.diameter <= 0 ||
            !std::isfinite(tip.roundness) || !std::isfinite(tip.angle))
            return std::nullopt;
        if (tip != tip_key_) {
            tip_image_ = outline(tip);
            tip_key_ = tip;
        }
        return place(tip_image_, point, viewport);
    }

    // Split the ring horizontally. Keeping the center open preserves a view
    // of the exact pixel being sampled throughout the gesture.
    std::optional<ii::gui::ImageRun> eyedropper(Sample sample, ii::gui::WidgetPoint point, ii::gui::Size viewport)
    {
        if (sample != sample_key_) {
            ring_image_ = comparison(sample);
            sample_key_ = sample;
        }
        return place(ring_image_, point, viewport);
    }

  private:
    struct Graphic final {
        double width, height;
        std::shared_ptr<const ii::gui::RasterImage> image;
    };

    static std::string number(double value) { return ii::core::format_decimal(value); }

    // Use the native SVG rasterizer for subpixel outlines. Cap the bitmap
    // resolution, retaining the original logical geometry through zoom.
    static Graphic rasterize(double width, double height, std::string shapes)
    {
        constexpr double max_raster_side = 512;
        const double scale = std::min(2., max_raster_side / std::max(width, height));
        const auto svg = "<svg xmlns='http://www.w3.org/2000/svg' width='" + number(width) +
                         "' height='" + number(height) + "' viewBox='" +
                         number(-width / 2) + " " + number(-height / 2) + " " +
                         number(width) + " " + number(height) + "'>" + shapes + "</svg>";
        auto image = ii::gui::svg_image(svg, std::uint32_t(std::ceil(width * scale)),
                                       std::uint32_t(std::ceil(height * scale)));
        return {width, height, image ? std::move(*image) : nullptr};
    }

    // Format sRGB source bytes for the rasterizer. Color swatch alpha is
    // preserved by the separate fill-opacity attribute.
    static std::string rgb(std::uint32_t argb)
    {
        return "rgb(" + std::to_string((argb >> 16) & 255) + "," +
               std::to_string((argb >> 8) & 255) + "," + std::to_string(argb & 255) + ")";
    }
    static std::string rgb(ii::gui::WidgetColor color)
    {
        const auto byte = [](double value) { return std::uint32_t(std::lround(value * 255)); };
        return rgb((byte(color.red) << 16) | (byte(color.green) << 8) | byte(color.blue));
    }

    // Stroke the selected shape twice for contrast on light and dark artwork.
    // The contour follows the same rotation and roundness as the brush tip.
    static Graphic outline(Tip tip)
    {
        const double rx = std::max<double>(Metric::stroke1, tip.diameter) / 2;
        const double ry = std::max<double>(Metric::stroke1, tip.diameter * std::clamp(tip.roundness, .01, 1.)) / 2;
        const double cosine = std::abs(std::cos(tip.angle)), sine = std::abs(std::sin(tip.angle));
        std::string shape;
        double half_width, half_height;
        switch (tip.shape) {
        case BrushTipShape::Square:
            shape = "<rect x='" + number(-rx) + "' y='" + number(-ry) +
                    "' width='" + number(2 * rx) + "' height='" + number(2 * ry) + "'";
            half_width = rx * cosine + ry * sine;
            half_height = rx * sine + ry * cosine;
            break;
        case BrushTipShape::Diamond:
            shape = "<path d='M" + number(-rx) + " 0 L0 " + number(-ry) + " L" +
                    number(rx) + " 0 L0 " + number(ry) + " Z'";
            half_width = std::max(rx * cosine, ry * sine);
            half_height = std::max(rx * sine, ry * cosine);
            break;
        default:
            shape = "<ellipse rx='" + number(rx) + "' ry='" + number(ry) + "'";
            half_width = std::hypot(rx * cosine, ry * sine);
            half_height = std::hypot(rx * sine, ry * cosine);
            break;
        }
        const auto stroke = [&](ii::gui::WidgetColor color, double width) {
            return shape + " stroke='" + rgb(color) + "' stroke-width='" + number(width) + "'/>";
        };
        return rasterize(2 * (half_width + Metric::stroke2), 2 * (half_height + Metric::stroke2),
                         "<g transform='rotate(" + number(tip.angle * 180 / std::numbers::pi) +
                         ")' fill='none' stroke-linejoin='round'>" +
                         stroke(Color::canvas, Metric::stroke2 + Metric::stroke1) +
                         stroke(Color::textBase, Metric::stroke1) + "</g>");
    }

    // Trace each open semicircle around the same hole, then draw the rims.
    static Graphic comparison(Sample sample)
    {
        constexpr double outer = Metric::size72 / 2. - Metric::stroke1;
        constexpr double inner = Metric::size48 / 2. + Metric::stroke1;
        const auto arc = [](double radius, bool upper, double end) {
            return " A" + number(radius) + " " + number(radius) + " 0 0 " +
                   (upper ? "1 " : "0 ") + number(end) + " 0";
        };
        const auto half = [&](std::uint32_t color, bool upper) {
            return "<path d='M" + number(-outer) + " 0" + arc(outer, upper, outer) +
                   " H" + number(inner) + arc(inner, !upper, -inner) + " Z' fill='" + rgb(color) +
                   "' fill-opacity='" + number(double(color >> 24) / 255) + "'/>";
        };
        const auto rim = [](double radius, ii::gui::WidgetColor color) {
            return "<circle r='" + number(radius) + "' fill='none' stroke='" + rgb(color) +
                   "' stroke-width='" + number(Metric::stroke2) + "'/>";
        };
        return rasterize(Metric::size72, Metric::size72, half(sample.sampled, true) +
                         half(sample.previous, false) + rim(outer, Color::textBase) + rim(inner, Color::canvas));
    }

    // Crop the image and its UVs together. Even at high document zoom the
    // retained ImageRun is bounded by the visible canvas widget.
    static std::optional<ii::gui::ImageRun> place(const Graphic& graphic, ii::gui::WidgetPoint point,
                                                  ii::gui::Size viewport)
    {
        if (!graphic.image) return std::nullopt;
        const auto [width, height, image] = graphic;
        const double x = point.x - width / 2, y = point.y - height / 2;
        const double left = std::max(0., x), top = std::max(0., y);
        const double w = std::min(x + width, viewport.width) - left;
        const double h = std::min(y + height, viewport.height) - top;
        if (w <= 0 || h <= 0) return std::nullopt;
        return ii::gui::ImageRun{.bounds = {float(left), float(top), float(w), float(h)}, .image = image,
                                 .source = {float((left - x) / width), float((top - y) / height),
                                            float(w / width), float(h / height)}};
    }

    std::optional<Tip> tip_key_;
    std::optional<Sample> sample_key_;
    Graphic tip_image_{}, ring_image_{};
};
} // namespace congregation::native
