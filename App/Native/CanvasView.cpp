#include "CanvasView.h"
#include "Style.h"
#include "EditorLayout.h"
#include <iiGUI/Text.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
template <class T> T take(ii::core::Result<T> result)
{
    if (!result)
    {
        if (result.error().cause)
            std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
std::shared_ptr<const ii::gui::RasterImage> image(const RasterLayer& pixels)
{
    std::vector<std::uint8_t> rgba(pixels.pixels.size() * 4);
    for (std::size_t i = 0; i < pixels.pixels.size(); ++i)
    {
        const auto c = pixels.pixels[i];
        auto* p = rgba.data() + i * 4;
        p[0] = std::uint8_t(c >> 16);
        p[1] = std::uint8_t(c >> 8);
        p[2] = std::uint8_t(c);
        p[3] = std::uint8_t(c >> 24);
    }
    return take(ii::gui::rgba_image(pixels.width, pixels.height, std::move(rgba)));
}
} // namespace
void CanvasView::attach(ii::gui::Widget widget)
{
    widget_ = widget;
    auto bounds = take(widget.snapshot()).bounds;
    viewport_.width = bounds.width;
    viewport_.height = bounds.height;
    attached_ = true;
    fit();
}
Point CanvasView::world(double x, double y) const
{
    return {(x - viewport_.x) / viewport_.scale, (y - viewport_.y) / viewport_.scale};
}
void CanvasView::resize(ii::gui::Rect r)
{
    const auto old_width=viewport_.width,old_height=viewport_.height;
    viewport_.width = r.width;
    viewport_.height = r.height;
    if (attached_) {
        if(fitted_)fit();
        else { viewport_.x+=(r.width-old_width)/2;viewport_.y+=(r.height-old_height)/2;render(); }
    }
}
void CanvasView::fit()
{
    fitted_=true;
    const auto& extent = artwork_.document().extent;
    const auto fitted=editor::fit(viewport_.width,viewport_.height,extent.width,extent.height);
    viewport_.scale=fitted.scale;
    const auto origin = canvasOrigin(artwork_.document());
    viewport_.x = fitted.x - origin.x * viewport_.scale;
    viewport_.y = fitted.y - origin.y * viewport_.scale;
    render();
}
void CanvasView::zoom(double factor, double x, double y)
{
    if (!std::isfinite(factor) || factor <= 0)
        return;
    if (x < 0)
        x = viewport_.width / 2;
    if (y < 0)
        y = viewport_.height / 2;
    fitted_=false;
    const auto point = world(x, y);
    viewport_.scale = std::clamp(viewport_.scale * factor, 1. / 64, 32.);
    viewport_.x = x - point.x * viewport_.scale;
    viewport_.y = y - point.y * viewport_.scale;
    render();
}
void CanvasView::refresh(bool changed)
{
    if (changed)
        cache_.clear();
    render();
}
void CanvasView::set_frame(int value)
{
    value = std::clamp(value, 0, std::max(0, int(artwork_.document().timeline.frameCount) - 1));
    if (value == frame_)
        return;
    frame_ = value;
    render();
}
void CanvasView::render()
{
    if (!attached_ || viewport_.width <= 0 || viewport_.height <= 0)
        return;
    ii::gui::WidgetDrawing drawing;
    const auto& doc = artwork_.document();
    const auto scale = viewport_.scale;
    const bool infinite = doc.canvasMode == CanvasMode::Infinite;
    ii::gui::Rect paper =
        infinite ? ii::gui::Rect{0, 0, float(viewport_.width), float(viewport_.height)}
                 : ii::gui::Rect{float(viewport_.x), float(viewport_.y),
                                 float(doc.extent.width * scale), float(doc.extent.height * scale)};
    const auto paper_left = std::clamp(paper.x, 0.f, float(viewport_.width)),
               paper_top = std::clamp(paper.y, 0.f, float(viewport_.height)),
               paper_right = std::clamp(paper.x + paper.width, 0.f, float(viewport_.width)),
               paper_bottom = std::clamp(paper.y + paper.height, 0.f, float(viewport_.height));
    if (paper_right > paper_left && paper_bottom > paper_top)
        drawing.panels.push_back(
            {{paper_left, paper_top, paper_right - paper_left, paper_bottom - paper_top},
             editor::linear(Color::paper),
             editor::linear(Color::paper),
             Metric::radius0});
    const int lod = scale < 1 ? std::min(6, int(std::floor(std::log2(1 / scale))))
                              : -std::min(5, int(std::floor(std::log2(scale))));
    const int tile = lod >= 0 ? 512 << lod : 512 >> (-lod);
    const auto cell = [tile](double position)
    {
        const double bound = std::numeric_limits<int>::max() / tile - 1;
        return int(std::clamp(std::floor(position), -bound, bound));
    };
    const int left = infinite ? cell(-viewport_.x / scale / tile)
                              : std::clamp(cell(-viewport_.x / scale / tile), 0,
                                           std::max(0, (doc.extent.width - 1) / tile));
    const int top = infinite ? cell(-viewport_.y / scale / tile)
                             : std::clamp(cell(-viewport_.y / scale / tile), 0,
                                          std::max(0, (doc.extent.height - 1) / tile));
    const int right = infinite ? cell((viewport_.width - viewport_.x) / scale / tile)
                               : std::min((doc.extent.width - 1) / tile,
                                          cell((viewport_.width - viewport_.x) / scale / tile));
    const int bottom = infinite ? cell((viewport_.height - viewport_.y) / scale / tile)
                                : std::min((doc.extent.height - 1) / tile,
                                           cell((viewport_.height - viewport_.y) / scale / tile));
    viewport_.visible_tiles = 0;
    for (int row = top; row <= bottom; ++row)
        for (int column = left; column <= right; ++column)
        {
            const auto key = std::tuple{lod, column, row, frame_};
            auto found = cache_.find(key);
            const int x = column * tile, y = row * tile,
                      w = infinite ? tile : std::min(tile, doc.extent.width - x),
                      h = infinite ? tile : std::min(tile, doc.extent.height - y);
            if (found == cache_.end())
            {
                auto result = renderFrameRegion(
                    doc, frame_, {{x, y}, {w, h}},
                    {std::max(1, w >> std::max(0, lod)), std::max(1, h >> std::max(0, lod))},
                    RasterSampling::Smooth);
                if (!result.ok())
                    throw std::runtime_error(result.message);
                found = cache_.emplace(key, image(result.pixels)).first;
                ++viewport_.uploads;
            }
            drawing.images.push_back(
                {{float(viewport_.x + x * scale), float(viewport_.y + y * scale), float(w * scale),
                  float(h * scale)},
                 found->second});
            ++viewport_.visible_tiles;
        }
    if (drag_ && !panning_ && !zooming_ && !artwork_.stroke_active())
    {
        const auto to = world(last_.x, last_.y);
        const auto x = viewport_.x + std::min(down_.x, to.x) * scale,
                   y = viewport_.y + std::min(down_.y, to.y) * scale,
                   w = std::max<double>(Metric::stroke1, std::abs(down_.x - to.x) * scale),
                   h = std::max<double>(Metric::stroke1, std::abs(down_.y - to.y) * scale);
        static const auto blue = []
        {
            const auto c = Color::canvasSelection;
            return take(ii::gui::rgba_image(1, 1,
                                            {std::uint8_t(std::lround(c.red * 255)),
                                             std::uint8_t(std::lround(c.green * 255)),
                                             std::uint8_t(std::lround(c.blue * 255)),
                                             std::uint8_t(std::lround(c.alpha * 255))}));
        }();
        for (auto r :
             {ii::gui::Rect{float(x), float(y), float(w), Metric::stroke1},
              ii::gui::Rect{float(x), float(y + h - Metric::stroke1), float(w), Metric::stroke1},
              ii::gui::Rect{float(x), float(y), Metric::stroke1, float(h)},
              ii::gui::Rect{float(x + w - Metric::stroke1), float(y), Metric::stroke1, float(h)}})
        {
            const auto left = std::max(0.f, r.x), top = std::max(0.f, r.y),
                       right = std::min(float(viewport_.width), r.x + r.width),
                       bottom = std::min(float(viewport_.height), r.y + r.height);
            if (right > left && bottom > top)
                drawing.images.push_back({{left, top, right - left, bottom - top}, blue});
        }
    }
    if (grid)
    {
        const auto color = editor::linear(Color::materialSurfaceDenseEdge);
        const auto step =
            std::max<double>(Metric::gridMinimumStep, Metric::gridDocumentStep * scale);
        for (double x = std::fmod(viewport_.x, step); x < viewport_.width; x += step)
            drawing.panels.push_back(
                {{float(x), 0, Metric::stroke1, float(viewport_.height)}, color, color});
        for (double y = std::fmod(viewport_.y, step); y < viewport_.height; y += step)
            drawing.panels.push_back(
                {{0, float(y), float(viewport_.width), Metric::stroke1}, color, color});
    }
    auto result = widget_.set_drawing(std::move(drawing));
    if (!result && result.error().code != ii::core::ErrorCode::closed)
        throw std::runtime_error(result.error().message + " (canvas " +
                                 std::to_string(viewport_.width) + "x" +
                                 std::to_string(viewport_.height) +
                                 ", tiles=" + std::to_string(viewport_.visible_tiles) + ")");
    // Entries retain immutable tiles used by the submitted scene. Bound the
    // additional cache independently of canvas dimensions or pan distance.
    while (cache_.size() > 96)
        cache_.erase(cache_.begin());
}
void CanvasView::cancel()
{
    artwork_.cancel();
    drag_ = panning_ = zooming_ = selecting_ = space_ = false;
    refresh();
}
bool CanvasView::hit(Point point)
{
    const auto& document = artwork_.document();
    for (std::size_t i = document.layers.size(); i > 0; --i)
    {
        const auto& layer = document.layers[i - 1];
        if (!layerProperties(layer).visible)
            continue;
        auto rendered = renderFrameLayerTiles(
            document, 0, i - 1,
            {{{{int(std::floor(point.x)), int(std::floor(point.y))}, {1, 1}}, {1, 1}}});
        if (rendered.ok() && !rendered.tiles.empty() && !rendered.tiles[0].pixels.pixels.empty() &&
            (rendered.tiles[0].pixels.pixels[0] >> 24) > 8)
            return artwork_.select(layerProperties(layer).id);
    }
    return false;
}

ii::core::Result<bool> CanvasView::input(const ii::gui::WidgetInput& event)
{
    using K = ii::gui::WidgetInputKind;
    using Key = ii::gui::WidgetKey;
    if (event.kind == K::focus_lost)
    {
        cancel();
        return false;
    }
    if (event.kind == K::key_down && event.key == Key::escape)
    {
        cancel();
        return true;
    }
    if ((event.kind == K::key_down || event.kind == K::key_up) && event.key == Key::space &&
        !event.text_input_active)
    {
        space_ = event.kind == K::key_down;
        return true;
    }
    if (event.kind == K::scroll)
    {
        if (event.control || event.meta)
            zoom(std::exp(event.scroll_y * .002), event.x, event.y);
        else
        {
            viewport_.x += event.scroll_x;
            viewport_.y += event.scroll_y;
            render();
        }
        return true;
    }
    if (event.kind == K::drop && event.content)
    {
        auto point = world(event.x, event.y);
        if (!event.content->image.empty())
        {
            auto decoded = decodeBitmap(event.content->image);
            if (decoded.ok())
            {
                if (!artwork_.import_pixels(std::move(decoded.asset.pixels), "Dropped image",
                                            point.x, point.y) &&
                    status)
                    status(artwork_.error());
            }
            else if (status)
                status(decoded.result.message);
        }
        for (const auto& path : event.content->files)
            if (!artwork_.import_image(native_path(path), point.x, point.y) && status)
                status(artwork_.error());
        return true;
    }
    if (presentation && (event.kind == K::pointer_down || event.kind == K::pointer_move || event.kind == K::pointer_up))
        return true;
    if (event.kind == K::pointer_down)
    {
        if (event.buttons && !(event.buttons & 1) && !(event.buttons & 4))
            return false;
        down_ = world(event.x, event.y);
        last_ = {event.x, event.y};
        zooming_ = tool == "zoom" || (space_ && (event.control || event.meta));
        panning_ = (space_ && !zooming_) || tool == "pan" || (event.buttons & 4);
        drag_ = true;
        if (panning_ || zooming_)
            return true;
        if (tool == "eyedropper")
        {
            auto pixel = renderFrameRegion(
                artwork_.document(), 0,
                {{int(std::floor(down_.x)), int(std::floor(down_.y))}, {1, 1}}, {1, 1});
            drag_ = false;
            if (pixel.ok() && !pixel.pixels.pixels.empty())
            {
                color = pixel.pixels.pixels[0];
                tool = "brush";
                if (color_picked)
                    color_picked(color);
            }
            else if (status)
                status("Choose a point inside the canvas");
            return true;
        }
        if (tool == "select")
        {
            selecting_ = hit(down_);
            return true;
        }
        if (tool == "brush" || tool == "eraser")
        {
            BitmapBrush brush = settings;
            brush.argb = color;
            brush.size = brush_size;
            brush.opacity = opacity;
            brush.eraser = tool == "eraser" || event.device == ii::gui::PointerDevice::eraser;
            const auto ok = artwork_.begin(
                {down_.x, down_.y},
                event.device == ii::gui::PointerDevice::mouse ? 1 : event.pressure, brush);
            if (!ok && status)
                status(artwork_.error());
            refresh();
            return true;
        }
        if (tool == "text")
        {
            drag_ = false;
            const auto linear = [](unsigned v)
            {
                const auto n = v / 255.f;
                return n <= .04045f ? n / 12.92f : std::pow((n + .055f) / 1.055f, 2.4f);
            };
            ii::gui::TextRun run{.bounds = {0, 0, 1024, 256},
                                 .text = text,
                                 .color = {linear((color >> 16) & 255), linear((color >> 8) & 255),
                                           linear(color & 255), float((color >> 24) & 255) / 255},
                                 .font_size = float(std::max(12., brush_size * 3)),
                                 .font_family = "Pretendard"};
            auto bitmap = ii::gui::render_text(run);
            if (!bitmap)
                return std::unexpected(bitmap.error());
            auto pixels = makeRasterLayer(bitmap->width, bitmap->height, 0);
            for (std::size_t i = 0; i < pixels.pixels.size(); ++i)
            {
                auto* p = bitmap->rgba.data() + i * 4;
                pixels.pixels[i] = (std::uint32_t(p[3]) << 24) | (std::uint32_t(p[0]) << 16) |
                                   (std::uint32_t(p[1]) << 8) | p[2];
            }
            if (!artwork_.import_pixels(std::move(pixels), "Text: " + text, down_.x, down_.y) &&
                status)
                status(artwork_.error());
            drag_ = false;
            return true;
        }
        if (tool == "fill")
        {
            drag_ = false;
            if (!artwork_.fill_at(down_, color) && status)
                status(artwork_.error());
            drag_ = false;
            return true;
        }
        return true;
    }
    if (event.kind == K::pointer_move && drag_)
    {
        if (zooming_)
        {
            zoom(std::exp((event.x - last_.x - event.y + last_.y) * .006), last_.x, last_.y);
        }
        else if (panning_)
        {
            fitted_=false;
            viewport_.x += event.x - last_.x;
            viewport_.y += event.y - last_.y;
        }
        else if (artwork_.stroke_active())
        {
            auto point = world(event.x, event.y);
            if (!artwork_.extend({point.x, point.y}, event.device == ii::gui::PointerDevice::mouse
                                                         ? 1
                                                         : event.pressure) &&
                status)
                status(artwork_.error());
            cache_.clear();
        }
        last_ = {event.x, event.y};
        render();
        return true;
    }
    if (event.kind == K::pointer_up && drag_)
    {
        auto point = world(event.x, event.y);
        const bool was_pan = panning_ || zooming_;
        drag_ = panning_ = zooming_ = false;
        if (artwork_.stroke_active())
        {
            if (!artwork_.end({point.x, point.y},
                              event.device == ii::gui::PointerDevice::mouse ? 1 : event.pressure) &&
                status)
                status(artwork_.error());
        }
        else if (selecting_)
        {
            selecting_ = false;
            for (const auto& value : artwork_.document().layers)
                if (layerProperties(value).id == artwork_.selected())
                {
                    auto transform = layerProperties(value).transform;
                    if (event.shift || event.alt)
                    {
                        double w = artwork_.document().extent.width,
                               h = artwork_.document().extent.height;
                        if (auto* source = staticLayerSource(value))
                            if (auto* asset = findAsset(artwork_.document(), source->assetId))
                                std::visit(
                                    [&](const auto& data)
                                    {
                                        using T = std::decay_t<decltype(data)>;
                                        if constexpr (std::same_as<T, RasterAsset>)
                                        {
                                            w = data.pixels.width;
                                            h = data.pixels.height;
                                        }
                                        else if constexpr (std::same_as<T, VectorAsset>)
                                        {
                                            w = data.viewport.width;
                                            h = data.viewport.height;
                                        }
                                    },
                                    *asset);
                        const auto cx = transform.m11 * w / 2 + transform.m21 * h / 2 +
                                        transform.translationX,
                                   cy = transform.m12 * w / 2 + transform.m22 * h / 2 +
                                        transform.translationY;
                        const auto angle = event.alt ? std::atan2(point.y - cy, point.x - cx) -
                                                           std::atan2(down_.y - cy, down_.x - cx)
                                                     : 0.;
                        const auto factor =
                            event.shift ? std::clamp(std::hypot(point.x - cx, point.y - cy) /
                                                         std::max(1., std::hypot(down_.x - cx,
                                                                                 down_.y - cy)),
                                                     .05, 20.)
                                        : 1.;
                        const auto a = std::cos(angle) * factor, b = std::sin(angle) * factor;
                        const auto old = transform;
                        transform.m11 = a * old.m11 - b * old.m12;
                        transform.m12 = b * old.m11 + a * old.m12;
                        transform.m21 = a * old.m21 - b * old.m22;
                        transform.m22 = b * old.m21 + a * old.m22;
                        transform.translationX = cx - transform.m11 * w / 2 - transform.m21 * h / 2;
                        transform.translationY = cy - transform.m12 * w / 2 - transform.m22 * h / 2;
                    }
                    else
                    {
                        transform.translationX += point.x - down_.x;
                        transform.translationY += point.y - down_.y;
                    }
                    artwork_.transform(transform);
                    break;
                }
        }
        else if (!was_pan && tool != "brush" && tool != "eraser" && tool != "select" &&
                 tool != "text" && tool != "fill")
        {
            if (!artwork_.shape(tool, down_, point, color, brush_size, event.shift) && status)
                status(artwork_.error());
        }
        refresh(!was_pan);
        return true;
    }
    return false;
}
} // namespace congregation::native
