#include "CanvasView.h"
#include "Style.h"
#include "EditorLayout.h"
#include <iiGUI/Text.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <numbers>
#include <new>

namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
using ProfileClock = std::chrono::steady_clock;
std::uint64_t profile_elapsed(ProfileClock::time_point started) noexcept
{
    return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(ProfileClock::now() - started).count());
}
bool stroke_profiling_enabled() noexcept
{
    const auto *value = std::getenv("CONGREGATION_PAINT_PROFILE");
    return value && value[0] == '1' && value[1] == '\0';
}
void report_stroke_profile(const StrokeProfileReport& report) noexcept
{
    if (!report.recorded) return;
    const auto u = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
    std::fprintf(stderr,
        "CONGREGATION_PAINT_PROFILE ok=%d moves=%llu renders=%llu begin_us=%llu "
        "move_p50_upper_us=%llu move_p95_upper_us=%llu move_p99_upper_us=%llu move_max_us=%llu "
        "render_p50_upper_us=%llu render_p95_upper_us=%llu render_p99_upper_us=%llu render_max_us=%llu "
        "finish_us=%llu max_input_gap_us=%llu\n",
        report.succeeded ? 1 : 0, u(report.moves), u(report.renders), u(report.begin_us),
        u(report.move_p50_upper_us), u(report.move_p95_upper_us), u(report.move_p99_upper_us), u(report.move_max_us),
        u(report.render_p50_upper_us), u(report.render_p95_upper_us), u(report.render_p99_upper_us), u(report.render_max_us),
        u(report.finish_us), u(report.max_input_gap_us));
    std::fflush(stderr);
}
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
void require(ii::core::Result<void> result)
{
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
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
bool hasNonlocalFilters(const Document& document)
{
    return std::ranges::any_of(document.layers, [](const auto& layer) {
        return std::ranges::any_of(layerProperties(layer).appearance.filters, [](const auto& filter) {
            return filter.enabled && filter.opacity > 0;
        });
    });
}
} // namespace
CanvasView::CanvasView(Artwork& artwork, std::size_t layer_budget)
    : artwork_(artwork),
      paint_layer_budget_(std::clamp<std::size_t>(layer_budget, 1, canvas_tile_layer_budget)),
      stroke_profile_(stroke_profiling_enabled()) {}
CanvasView::~CanvasView()
{
    frame_lifetime_.reset();
    if (attached_) (void)widget_.set_frame_handler({}, false);
}
void CanvasView::attach(ii::gui::Widget widget, ii::gui::Widget paint_host)
{
    if (attached_) (void)widget_.set_frame_handler({}, false);
    widget_ = widget;
    paint_host_ = paint_host;
    paint_overlay_ = {};
    paint_layers_.clear();
    presented_tiles_.clear();
    const auto lifetime = std::weak_ptr<int>(frame_lifetime_);
    frame_handler_ready_ = bool(widget_.set_frame_handler(
        [this, lifetime](ii::runtime::Clock::time_point) -> ii::core::Result<bool> {
            if (lifetime.expired()) return false;
            if (stroke_render_pending_)
                render_stroke(stroke_preview_unbounded_ ? std::nullopt : stroke_preview_region_);
            return false;
        }, false));
    auto bounds = take(widget.snapshot()).bounds;
    viewport_.width = bounds.width;
    viewport_.height = bounds.height;
    if (!paint_host_.is_null()) {
        require(paint_host_.set_content_input_passthrough(true));
        resize_paint_layers();
    }
    attached_ = true;
    fit();
}
Point CanvasView::world(double x, double y) const
{
    return {(x - viewport_.x) / viewport_.scale, (y - viewport_.y) / viewport_.scale};
}
ii::gui::WidgetInput CanvasView::localize_input(const ii::gui::WidgetInput& event) const
{
    auto local = event;
    if (event.local_position) {
        local.x = event.local_position->x;
        local.y = event.local_position->y;
    } else if (attached_) {
        // Directly injected inputs may use window coordinates and do not have
        // the widget-local position populated by iiGUI's input dispatcher.
        const auto bounds = take(widget_.snapshot()).bounds;
        local.x -= bounds.x;
        local.y -= bounds.y;
    }
    return local;
}
void CanvasView::resize(ii::gui::Rect r)
{
    const auto old_width=viewport_.width,old_height=viewport_.height;
    viewport_.width = r.width;
    viewport_.height = r.height;
    if (attached_) {
        resize_paint_layers();
        if(fitted_)fit();
        else { viewport_.x+=(r.width-old_width)/2;viewport_.y+=(r.height-old_height)/2;render(); }
    }
}

void CanvasView::resize_paint_layers()
{
    if (paint_host_.is_null()) return;
    const ii::gui::Rect frame{0, 0, float(viewport_.width), float(viewport_.height)};
    require(paint_host_.set_frame(frame));
    for (const auto& layer : paint_layers_) require(layer.set_frame(frame));
    if (!paint_overlay_.is_null()) require(paint_overlay_.set_frame(frame));
}

void CanvasView::present_drawing(ii::gui::WidgetDrawing drawing, std::size_t tile_count)
{
    const auto present = [](ii::gui::Widget widget, ii::gui::WidgetDrawing value) {
        auto result = widget.set_drawing(std::move(value));
        if (!result && result.error().code != ii::core::ErrorCode::closed) require(std::move(result));
    };
    if (paint_host_.is_null()) {
        present(widget_, std::move(drawing));
        return;
    }
    const auto group_size = canvas_tile_group_size(tile_count, paint_layer_budget_);
    const auto needed_layers = tile_count / group_size + (tile_count % group_size != 0);
    if (paint_overlay_.is_null() || needed_layers > paint_layers_.size()) {
        std::vector<ii::gui::Widget> layers;
        ii::gui::Widget overlay;
        const auto count = std::max(needed_layers, paint_layers_.size());
        const ii::gui::Size size{viewport_.width, viewport_.height};
        const ii::gui::Rect frame{0, 0, float(viewport_.width), float(viewport_.height)};
        require(paint_host_.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
            auto root = builder.absolute_stack({}, "canvas-layer-root", size);
            if (!root) return std::unexpected(root.error());
            layers.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto layer = builder.absolute_stack(*root, "canvas-tile-" + std::to_string(i), size);
                if (!layer) return std::unexpected(layer.error());
                if (auto placed = layer->set_frame(frame); !placed) return placed;
                layers.push_back(*layer);
            }
            auto top = builder.absolute_stack(*root, "canvas-overlay", size);
            if (!top) return std::unexpected(top.error());
            if (auto placed = top->set_frame(frame); !placed) return placed;
            overlay = *top;
            return {};
        }));
        paint_layers_ = std::move(layers);
        paint_overlay_ = overlay;
        presented_tiles_.assign(paint_layers_.size(), {});
    }
    ii::gui::WidgetDrawing overlay;
    overlay.images_before_text = true;
    overlay.images.assign(std::make_move_iterator(drawing.images.begin() + tile_count),
                          std::make_move_iterator(drawing.images.end()));
    overlay.texts = std::move(drawing.texts);
    for (std::size_t i = 0; i < paint_layers_.size(); ++i) {
        std::vector<ii::gui::ImageRun> images;
        if (i < needed_layers) {
            const auto first = i * group_size;
            const auto last = first + std::min(group_size, tile_count - first);
            images.assign(drawing.images.begin() + first, drawing.images.begin() + last);
        }
        if (presented_tiles_[i] == images) continue;
        ii::gui::WidgetDrawing tile;
        tile.images_before_text = true;
        tile.images = images;
        present(paint_layers_[i], std::move(tile));
        presented_tiles_[i] = std::move(images);
    }
    drawing.images.clear();
    present(widget_, std::move(drawing));
    present(paint_overlay_, std::move(overlay));
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
    viewport_.scale = std::clamp(viewport_.scale * factor, 1. / 64, editor::maximum_canvas_zoom);
    viewport_.x = x - point.x * viewport_.scale;
    viewport_.y = y - point.y * viewport_.scale;
    render();
}
void CanvasView::refresh(bool changed)
{
    if (committing_stroke_) return;
    if (changed)
        cache_.clear();
    render();
}

void CanvasView::invalidate_stroke_tiles(const StrokeDamage& damage)
{
    if (hasNonlocalFilters(artwork_.document()) || !std::isfinite(damage.left) || !std::isfinite(damage.top) ||
        !std::isfinite(damage.right) || !std::isfinite(damage.bottom)) {
        cache_.clear();
        return;
    }
    for (auto it = cache_.begin(); it != cache_.end();) {
        const auto [lod, column, row, frame] = it->first;
        const int tile = lod >= 0 ? 512 << lod : 512 >> (-lod);
        const double left = double(column) * tile, top = double(row) * tile;
        if (frame == frame_ && damage.left < left + tile && damage.right > left &&
            damage.top < top + tile && damage.bottom > top)
            it = cache_.erase(it);
        else
            ++it;
    }
}

std::optional<CanvasRegion> CanvasView::stroke_tile_region(const StrokeDamage& damage) const
{
    const auto& document = artwork_.document();
    const auto scale = viewport_.scale;
    if (document.canvasMode != CanvasMode::Finite || hasNonlocalFilters(document) ||
        document.extent.width <= 0 || document.extent.height <= 0 ||
        !std::isfinite(scale) || scale <= 0 || !std::isfinite(damage.left) ||
        !std::isfinite(damage.top) || !std::isfinite(damage.right) || !std::isfinite(damage.bottom))
        return std::nullopt;
    const int lod = scale < 1 ? std::min(6, int(std::floor(std::log2(1 / scale))))
                              : -std::min(5, int(std::floor(std::log2(scale))));
    const int tile = lod >= 0 ? 512 << lod : 512 >> (-lod);
    const int max_column = (document.extent.width - 1) / tile;
    const int max_row = (document.extent.height - 1) / tile;
    const auto index = [tile](double value, int limit) {
        return int(std::clamp(std::floor(value / tile), 0., double(limit)));
    };
    const int left = std::max(index(damage.left, max_column), index(-viewport_.x / scale, max_column));
    const int top = std::max(index(damage.top, max_row), index(-viewport_.y / scale, max_row));
    const int right = std::min(index(damage.right, max_column),
                               index((viewport_.width - viewport_.x) / scale, max_column));
    const int bottom = std::min(index(damage.bottom, max_row),
                                index((viewport_.height - viewport_.y) / scale, max_row));
    if (right < left || bottom < top) return std::nullopt;
    const int x = left * tile, y = top * tile;
    const int x2 = int(std::min<std::int64_t>(document.extent.width, std::int64_t(right + 1) * tile));
    const int y2 = int(std::min<std::int64_t>(document.extent.height, std::int64_t(bottom + 1) * tile));
    if (x2 <= x || y2 <= y) return std::nullopt;
    return CanvasRegion{{x, y}, {x2 - x, y2 - y}};
}

void CanvasView::schedule_stroke_render()
{
    const auto damage = artwork_.take_stroke_damage();
    if (!damage || !attached_) return;
    invalidate_stroke_tiles(*damage);
    if (!stroke_render_pending_) {
        stroke_preview_region_.reset();
        stroke_preview_unbounded_ = false;
    }
    const auto region = stroke_tile_region(*damage);
    if (!region) {
        stroke_preview_unbounded_ = true;
        stroke_preview_region_.reset();
    } else if (!stroke_preview_unbounded_) {
        if (!stroke_preview_region_) stroke_preview_region_ = *region;
        else {
            const auto& prior = *stroke_preview_region_;
            const int left = std::min(prior.origin.x, region->origin.x);
            const int top = std::min(prior.origin.y, region->origin.y);
            const auto right = std::max(std::int64_t(prior.origin.x) + prior.extent.width,
                                        std::int64_t(region->origin.x) + region->extent.width);
            const auto bottom = std::max(std::int64_t(prior.origin.y) + prior.extent.height,
                                         std::int64_t(region->origin.y) + region->extent.height);
            stroke_preview_region_ = CanvasRegion{{left, top}, {int(right-left), int(bottom-top)}};
        }
    }
    if (stroke_render_pending_) return;
    stroke_render_pending_ = true;
    if (!frame_handler_ready_ || !widget_.request_frames()) {
        stroke_render_pending_ = false;
        render();
    }
}
void CanvasView::set_frame(int value)
{
    value = std::clamp(value, 0, std::max(0, int(artwork_.document().timeline.frameCount) - 1));
    if (value == frame_)
        return;
    if (crop_) { crop_.reset();crop_saved_.reset();crop_drag_=CropDrag::None; }
    perspective_.reset();perspective_saved_.reset();perspective_drag_=-3;
    if (quick_mask_) { quick_mask_->cancel_stroke(); quick_mask_.reset(); quick_mask_dense_before_.reset(); quick_mask_tiled_before_.reset(); if (quick_mask_changed) quick_mask_changed(false); }
    clone_mask_.reset();clone_reference_.reset();clone_selection_.reset();clone_tiled_selection_.reset();
    // A content gesture belongs to the frame where it began. A frame switch
    // cancels it before another frame can be displayed or receive the release.
    if (artwork_.stroke_active() || polygon_active_ || (drag_ && !panning_ && !zooming_))
    {
        stroke_profile_.cancel();
        artwork_.cancel();
        drag_ = panning_ = zooming_ = selecting_ = false;
        lasso_path_.clear();
        polygon_path_.clear();
        magnetic_preview_.clear(); magnetic_anchors_.clear(); magnetic_reference_.reset(); magnetic_tiled_=false;
        polygon_active_ = false;
        cache_.clear();
    }
    frame_ = value;
    render();
}
std::optional<SelectionPreview> CanvasView::clone_preview() const
{
    if(!clone_mask_ || !clone_reference_ || !clone_mask_->pixels() || artwork_.recovery()!=clone_document_ || artwork_.revision()!=clone_revision_ || artwork_.selected()!=clone_selected_ ||
       artwork_.selection_mask_handle()!=clone_selection_ || artwork_.tiled_selection_handle()!=clone_tiled_selection_)return std::nullopt;
    return artwork_.clone_preview(*clone_reference_,*clone_mask_->pixels(),clone_offset_,static_cast<FrameIndex>(frame_),clone_mask_->dirty_bounds());
}
void CanvasView::render() { render_stroke(std::nullopt); }

void CanvasView::render_stroke(std::optional<CanvasRegion> document_region)
{
    if (committing_stroke_) return;
    stroke_render_pending_ = false;
    stroke_preview_region_.reset();
    stroke_preview_unbounded_ = false;
    if (!attached_ || viewport_.width <= 0 || viewport_.height <= 0)
        return;
    const bool profiling = stroke_profile_.active();
    const auto profile_start = profiling ? ProfileClock::now() : ProfileClock::time_point{};
    ii::gui::WidgetDrawing drawing;
    auto preview = artwork_.selection_preview(document_region);
    if(clone_mask_) {preview=clone_preview();cache_.clear();}
    if (artwork_.stroke_active()) {
        bool available = bool(preview);
        if (stroke_preview_available_ && *stroke_preview_available_ != available) {
            cache_.clear();
            if (document_region) {
                preview = artwork_.selection_preview();
                available = bool(preview);
            }
        }
        stroke_preview_available_ = available;
    } else {
        stroke_preview_available_.reset();
    }
    const bool needsPreview = artwork_.stroke_active() && artwork_.has_selection();
    const auto& doc = needsPreview && !preview ? artwork_.committed_document() : artwork_.document();
    std::optional<FrameBitmapOverlay> overlay;
    if (preview) overlay.emplace(FrameBitmapOverlay{preview->asset_id,preview->tiles});
    const auto overlays = overlay ? std::span<const FrameBitmapOverlay>(&*overlay,1) : std::span<const FrameBitmapOverlay>{};
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
             editor::linear(Color::textBase),
             editor::linear(Color::textBase),
             Metric::space0});
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
    const auto batch_size = canvas_composition_batch_size(doc.layers.size());
    std::vector<FrameRenderTileRequest> requests;
    std::array<CanvasTileKey, canvas_composition_max_tiles> keys;
    std::array<std::size_t, canvas_composition_max_tiles> image_positions;
    const auto compose = [&] {
        if (requests.empty()) return;
        auto result = renderFrameTiles(doc, frame_, requests, overlays);
        if (!result.ok()) throw std::runtime_error(result.message);
        if (result.tiles.size() != requests.size())
            throw std::runtime_error("canvas composition returned the wrong number of tiles");
        ++viewport_.composition_batches;
        for (std::size_t i = 0; i < requests.size(); ++i) {
            auto [found, inserted] = cache_.emplace(keys[i], image(result.tiles[i].pixels));
            drawing.images[image_positions[i]].image = found->second;
            if (inserted) ++viewport_.uploads;
        }
        requests.clear();
    };
    viewport_.visible_tiles = 0;
    for (int row = top; row <= bottom; ++row)
        for (int column = left; column <= right; ++column)
        {
            const auto key = std::tuple{lod, column, row, frame_};
            auto found = cache_.find(key);
            const int x = column * tile, y = row * tile,
                      w = infinite ? tile : std::min(tile, doc.extent.width - x),
                      h = infinite ? tile : std::min(tile, doc.extent.height - y);
            drawing.images.push_back(
                {{float(viewport_.x + x * scale), float(viewport_.y + y * scale), float(w * scale),
                  float(h * scale)},
                 found == cache_.end() ? nullptr : found->second});
            if (found == cache_.end())
            {
                const auto index = requests.size();
                keys[index] = key;
                image_positions[index] = drawing.images.size() - 1;
                requests.push_back({{{x, y}, {w, h}},
                                    {std::max(1, w >> std::max(0, lod)),
                                     std::max(1, h >> std::max(0, lod))},
                                    RasterSampling::Smooth});
                if (requests.size() == batch_size) compose();
            }
            ++viewport_.visible_tiles;
        }
    compose();
    if (drag_ && !panning_ && !zooming_ && tool != "lasso" && tool != "polygon-lasso" && tool != "gradient" && tool != "polygon" && tool != "crop" && tool != "perspective-crop" && !artwork_.stroke_active() && !quick_mask_)
    {
        const auto to = world(last_.x, last_.y);
        auto left=std::min(down_.x,to.x),top=std::min(down_.y,to.y),width=std::abs(down_.x-to.x),height=std::abs(down_.y-to.y);
        if(tool=="rectangle" || tool=="ellipse") {auto options=polygon_options;options.regular=options.regular || polygon_regular_preview_;if(auto bounds=polygon_bounds(down_,to,options)){left=bounds->left;top=bounds->top;width=bounds->width;height=bounds->height;}}
        const auto x = viewport_.x + left * scale, y = viewport_.y + top * scale,
                   w = std::max<double>(Metric::stroke1,width * scale), h = std::max<double>(Metric::stroke1,height * scale);
        static const auto blue = []
        {
            const auto c = Color::blue;
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
    if (crop_ && tool == "crop") {
        const auto c=Color::blue;
        const auto swatch=take(ii::gui::rgba_image(1,1,{std::uint8_t(std::lround(c.red*255)),std::uint8_t(std::lround(c.green*255)),std::uint8_t(std::lround(c.blue*255)),std::uint8_t(std::lround(c.alpha*255))}));
        const float left=float(viewport_.x+crop_->left*scale),top=float(viewport_.y+crop_->top*scale);
        const float right=float(viewport_.x+crop_->right*scale),bottom=float(viewport_.y+crop_->bottom*scale);
        const float handle=Metric::space6,stroke=Metric::stroke1;
        const auto draw=[&](ii::gui::Rect r) {const auto x=std::max(0.f,r.x),y=std::max(0.f,r.y),x2=std::min(float(viewport_.width),r.x+r.width),y2=std::min(float(viewport_.height),r.y+r.height);if(x2>x&&y2>y)drawing.images.push_back({{x,y,x2-x,y2-y},swatch});};
        draw({left,top,std::max(0.f,right-left),stroke});draw({left,bottom,std::max(0.f,right-left),stroke});
        draw({left,top,stroke,std::max(0.f,bottom-top)});draw({right,top,stroke,std::max(0.f,bottom-top)});
        for(auto [x,y]:std::array<std::pair<float,float>,4>{{{left,top},{right,top},{right,bottom},{left,bottom}}})
            draw({x-handle/2,y-handle/2,handle,handle});
    }
    auto preview_path = lasso_path_;
    if (polygon_active_ && (tool == "polygon-lasso" || tool == "magnetic-lasso")) {
        preview_path = polygon_path_;
        if (tool == "magnetic-lasso") {
            if (magnetic_preview_.size() > 1) preview_path.insert(preview_path.end(), magnetic_preview_.begin()+1, magnetic_preview_.end());
        } else preview_path.push_back(polygon_cursor_);
    } else if (drag_ && tool == "gradient" && !panning_ && !zooming_) {
        preview_path = {down_, world(last_.x, last_.y)};
    } else if (drag_ && tool == "polygon" && !panning_ && !zooming_) {
        preview_path.clear();
        auto options=polygon_options;options.regular=options.regular || polygon_regular_preview_;
        if(auto bounds=polygon_bounds(down_,world(last_.x,last_.y),options)) {preview_path=polygon_points(*bounds,options);if(!preview_path.empty())preview_path.push_back(preview_path.front());}
    } else if (perspective_ && tool == "perspective-crop") {
        preview_path.assign(perspective_->begin(),perspective_->end());preview_path.push_back(perspective_->front());
    }
    if (((drag_ && (tool == "lasso" || tool == "gradient" || tool == "polygon")) || (polygon_active_ && (tool == "polygon-lasso" || tool == "magnetic-lasso")) || (perspective_ && tool == "perspective-crop")) && preview_path.size() > 1)
    {
        const int w = int(std::ceil(std::clamp(viewport_.width, 1., 2048.)));
        const int h = int(std::ceil(std::clamp(viewport_.height, 1., 2048.)));
        std::vector<std::uint8_t> rgba(std::size_t(w) * h * 4, 0);
        const auto color = Color::blue;
        const auto plot = [&](int x, int y) {
            auto* p = rgba.data() + (std::size_t(y) * w + x) * 4;
            p[0] = std::uint8_t(std::lround(color.red * 255));
            p[1] = std::uint8_t(std::lround(color.green * 255));
            p[2] = std::uint8_t(std::lround(color.blue * 255));
            p[3] = std::uint8_t(std::lround(color.alpha * 255));
        };
        for (std::size_t i = 1; i < preview_path.size(); ++i) {
            const auto& a = preview_path[i-1]; const auto& b = preview_path[i];
            const double x = viewport_.x + a.x * scale, y = viewport_.y + a.y * scale;
            const double dx = (b.x - a.x) * scale, dy = (b.y - a.y) * scale;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx) || !std::isfinite(dy)) continue;
            double lo = 0, hi = 1;
            const auto clip = [&](double p, double q) {
                if (p == 0) return q >= 0;
                const auto t = q / p;
                if (p < 0) lo = std::max(lo, t); else hi = std::min(hi, t);
                return lo <= hi;
            };
            if (!clip(-dx, x) || !clip(dx, viewport_.width - x) ||
                !clip(-dy, y) || !clip(dy, viewport_.height - y)) continue;
            const auto px = [&](double n) { return std::clamp(int(std::floor(std::clamp(n, 0., viewport_.width) / viewport_.width * w)), 0, w - 1); };
            const auto py = [&](double n) { return std::clamp(int(std::floor(std::clamp(n, 0., viewport_.height) / viewport_.height * h)), 0, h - 1); };
            int x0 = px(x + lo * dx), y0 = py(y + lo * dy), x1 = px(x + hi * dx), y1 = py(y + hi * dy);
            const int ddx = std::abs(x1-x0), ddy = -std::abs(y1-y0), stepx = x0<x1 ? 1 : -1, stepy = y0<y1 ? 1 : -1;
            int error = ddx + ddy;
            for (;;) {
                plot(x0,y0); if (x0==x1 && y0==y1) break;
                const int twice = 2*error;
                if (twice >= ddy) { error += ddy; x0 += stepx; }
                if (twice <= ddx) { error += ddx; y0 += stepy; }
            }
        }
        drawing.images.push_back({{0, 0, float(viewport_.width), float(viewport_.height)},
                                  take(ii::gui::rgba_image(w, h, std::move(rgba)))});
    }
    if (perspective_ && tool == "perspective-crop") {
        const auto c=Color::blue;
        const auto swatch=take(ii::gui::rgba_image(1,1,{std::uint8_t(std::lround(c.red*255)),std::uint8_t(std::lround(c.green*255)),std::uint8_t(std::lround(c.blue*255)),std::uint8_t(std::lround(c.alpha*255))}));
        const float side=Metric::space6;
        for (const auto& p:*perspective_) {
            const float x=float(viewport_.x+p.x*scale)-side/2,y=float(viewport_.y+p.y*scale)-side/2;
            const float left=std::max(0.f,x),top=std::max(0.f,y),right=std::min(float(viewport_.width),x+side),bottom=std::min(float(viewport_.height),y+side);
            if(right>left && bottom>top)drawing.images.push_back({{left,top,right-left,bottom-top},swatch});
        }
    }
    if (quick_mask_)
    {
        const auto origin = canvasOrigin(doc); const auto color = Color::redOverlay;
        const int w=int(std::ceil(std::clamp(viewport_.width,1.,2048.)));
        const int h=int(std::ceil(std::clamp(viewport_.height,1.,2048.)));
        std::vector<std::uint8_t> rgba(std::size_t(w)*h*4,0);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            const auto point=world((x+.5)*viewport_.width/w,(y+.5)*viewport_.height/h);
            const double mx=point.x-origin.x,my=point.y-origin.y;
            if(!std::isfinite(mx)||!std::isfinite(my)||mx<0||my<0||mx>=doc.extent.width||my>=doc.extent.height)continue;
            const auto coverage=quick_mask_->tiled() ? quick_mask_->coverage(int(std::floor(point.x)),int(std::floor(point.y))) :
                quick_mask_->coverage(int(std::floor(mx)),int(std::floor(my)));
            auto* p=rgba.data()+(std::size_t(y)*w+x)*4;
            p[0]=std::uint8_t(std::lround(color.red*255));p[1]=std::uint8_t(std::lround(color.green*255));p[2]=std::uint8_t(std::lround(color.blue*255));
            p[3]=std::uint8_t(std::lround((255-coverage)*color.alpha));
        }
        drawing.images.push_back({{0,0,float(viewport_.width),float(viewport_.height)},take(ii::gui::rgba_image(w,h,std::move(rgba)))});
        outline_key_.reset();outline_image_.reset();
    }
    else if (auto selection=artwork_.selection_mask_handle(); selection || artwork_.tiled_selection_handle())
    {
        const auto tiled=artwork_.tiled_selection_handle();
        const auto origin=canvasOrigin(doc);
        const OutlineKey next{selection,tiled,viewport_.x,viewport_.y,viewport_.scale,viewport_.width,viewport_.height,origin.x,origin.y};
        if (!outline_key_ || *outline_key_!=next || !outline_image_)
        {
            const int w=int(std::ceil(std::clamp(viewport_.width,1.,2048.)));
            const int h=int(std::ceil(std::clamp(viewport_.height,1.,2048.)));
            std::vector<std::uint8_t> rgba(std::size_t(w)*h*4,0);
            const auto inside=[&](double x,double y) {
                const auto p=world(x,y);
                if(tiled) {
                    if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<std::numeric_limits<std::int32_t>::min()||p.y<std::numeric_limits<std::int32_t>::min()||
                       p.x>=double(std::numeric_limits<std::int32_t>::max())+1 || p.y>=double(std::numeric_limits<std::int32_t>::max())+1) return false;
                    return tiled->coverage(std::int32_t(std::floor(p.x)),std::int32_t(std::floor(p.y)))!=0;
                }
                const double mx=p.x-origin.x,my=p.y-origin.y;
                return std::isfinite(mx)&&std::isfinite(my)&&mx>=0&&my>=0&&mx<selection->extent.width&&my<selection->extent.height &&
                    selection->alpha[std::size_t(my)*selection->extent.width+std::size_t(mx)]!=0;
            };
            const auto color=Color::blue;
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                const double sx=(x+.5)*viewport_.width/w,sy=(y+.5)*viewport_.height/h;
                if(inside(sx,sy) && (!inside(sx-viewport_.width/w,sy) || !inside(sx+viewport_.width/w,sy) ||
                    !inside(sx,sy-viewport_.height/h) || !inside(sx,sy+viewport_.height/h))) {
                    auto* pixel=rgba.data()+(std::size_t(y)*w+x)*4;
                    pixel[0]=std::uint8_t(std::lround(color.red*255));pixel[1]=std::uint8_t(std::lround(color.green*255));
                    pixel[2]=std::uint8_t(std::lround(color.blue*255));pixel[3]=std::uint8_t(std::lround(color.alpha*255));
                }
            }
            auto image=take(ii::gui::rgba_image(w,h,std::move(rgba)));
            outline_image_=std::move(image);outline_key_=next;
        }
        drawing.images.push_back({{0,0,float(viewport_.width),float(viewport_.height)},outline_image_});
    } else { outline_key_.reset(); outline_image_.reset(); }
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
    present_drawing(std::move(drawing), viewport_.visible_tiles);
    if (profiling) stroke_profile_.render(profile_elapsed(profile_start));
    // Never evict a tile still visible in the submitted scene, even if a very
    // large viewport exceeds the ordinary offscreen cache capacity.
    trim_canvas_tile_cache(cache_, {lod, left, top, right, bottom, frame_, viewport_.visible_tiles}, 96);
}
void CanvasView::update_crop(Point point)
{
    if (!crop_ || crop_drag_==CropDrag::None || !std::isfinite(point.x) || !std::isfinite(point.y)) return;
    const auto extent=artwork_.document().extent;
    point.x=std::clamp(point.x,0.,double(extent.width));point.y=std::clamp(point.y,0.,double(extent.height));
    if (crop_drag_==CropDrag::Move && crop_saved_) {
        const auto& old=*crop_saved_;
        const auto dx=std::clamp(point.x-crop_drag_start_.x,-old.left,double(extent.width)-old.right);
        const auto dy=std::clamp(point.y-crop_drag_start_.y,-old.top,double(extent.height)-old.bottom);
        *crop_={old.left+dx,old.top+dy,old.right+dx,old.bottom+dy};return;
    }
    double dx=point.x-crop_anchor_.x,dy=point.y-crop_anchor_.y;
    if (std::isfinite(crop_aspect) && crop_aspect>0 && (std::abs(dx)>0 || std::abs(dy)>0)) {
        const double sx=dx<0?-1:1,sy=dy<0?-1:1;
        const double maxWidth=sx<0?crop_anchor_.x:extent.width-crop_anchor_.x;
        const double maxHeight=sy<0?crop_anchor_.y:extent.height-crop_anchor_.y;
        const double width=std::min({std::max(std::abs(dx),std::abs(dy)*crop_aspect),maxWidth,maxHeight*crop_aspect});
        dx=sx*width;dy=sy*width/crop_aspect;
    }
    *crop_={std::min(crop_anchor_.x,crop_anchor_.x+dx),std::min(crop_anchor_.y,crop_anchor_.y+dy),
            std::max(crop_anchor_.x,crop_anchor_.x+dx),std::max(crop_anchor_.y,crop_anchor_.y+dy)};
}
void CanvasView::update_perspective(Point point)
{
    if (!perspective_ || perspective_drag_ == -3 || !std::isfinite(point.x) || !std::isfinite(point.y)) return;
    const auto extent=artwork_.document().extent;
    point.x=std::clamp(point.x,0.,double(extent.width));point.y=std::clamp(point.y,0.,double(extent.height));
    if (perspective_drag_ == -2) {
        const double left=std::min(crop_anchor_.x,point.x),top=std::min(crop_anchor_.y,point.y);
        const double right=std::max(crop_anchor_.x,point.x),bottom=std::max(crop_anchor_.y,point.y);
        *perspective_={Point{left,top},Point{right,top},Point{right,bottom},Point{left,bottom}};
    } else if (perspective_drag_ == -1 && perspective_saved_) {
        double left=extent.width,right=0,top=extent.height,bottom=0;
        for(const auto& p:*perspective_saved_){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
        const auto dx=std::clamp(point.x-crop_drag_start_.x,-left,double(extent.width)-right);
        const auto dy=std::clamp(point.y-crop_drag_start_.y,-top,double(extent.height)-bottom);
        for(std::size_t i=0;i<4;++i)(*perspective_)[i]={(*perspective_saved_)[i].x+dx,(*perspective_saved_)[i].y+dy};
    } else if (perspective_drag_>=0 && perspective_drag_<4) (*perspective_)[std::size_t(perspective_drag_)]=point;
}
bool CanvasView::apply_crop()
{
    if (perspective_ && tool == "perspective-crop") {
        if (perspective_drag_!=-3) {if(status)status("Finish the current crop gesture");return false;}
        if (artwork_.revision()!=crop_revision_) {cancel();if(status)status("The document changed; crop was canceled");return false;}
        const auto& p=*perspective_;
        const auto length=[](Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);};
        double w=(length(p[0],p[1])+length(p[3],p[2]))/2;
        double h=(length(p[0],p[3])+length(p[1],p[2]))/2;
        if (std::isfinite(crop_aspect) && crop_aspect>0 && w>0 && h>0) { const auto area=w*h;w=std::sqrt(area*crop_aspect);h=w/crop_aspect; }
        if (!std::isfinite(w) || !std::isfinite(h) || w<1 || h<1 || w>32768 || h>32768) {if(status)status("Perspective crop must have a positive bounded size");return false;}
        if (!artwork_.perspective_crop(p,{int(std::lround(w)),int(std::lround(h))})) {if(status)status(artwork_.error());return false;}
        perspective_.reset();perspective_saved_.reset();perspective_drag_=-3;fit();if(status)status("Perspective crop applied; layers rasterized");return true;
    }
    if (!crop_) {if(status)status("Draw a crop rectangle first");return false;}
    if (crop_drag_!=CropDrag::None) {if(status)status("Finish the current crop gesture");return false;}
    if (artwork_.revision()!=crop_revision_) {cancel();if(status)status("The document changed; crop was canceled");return false;}
    const auto& extent=artwork_.document().extent;
    const int left=std::clamp(int(std::floor(crop_->left)),0,extent.width),top=std::clamp(int(std::floor(crop_->top)),0,extent.height);
    const int right=std::clamp(int(std::ceil(crop_->right)),0,extent.width),bottom=std::clamp(int(std::ceil(crop_->bottom)),0,extent.height);
    if(right<=left || bottom<=top) {if(status)status("Crop rectangle must have a positive size");return false;}
    bool applied = false;
    if (crop_destructive) {
        const std::array<Point,4> corners{{{double(left),double(top)},{double(right),double(top)},
            {double(right),double(bottom)},{double(left),double(bottom)}}};
        applied = artwork_.perspective_crop(corners,{right-left,bottom-top},true);
    } else applied = artwork_.reframe_canvas({{left,top},{right-left,bottom-top}});
    if (!applied) {if(status)status(artwork_.error());return false;}
    crop_.reset();crop_saved_.reset();crop_drag_=CropDrag::None;fit();
    if(status)status(crop_destructive ? "Canvas cropped; layers rasterized and original sources removed" : "Canvas cropped non-destructively");return true;
}
void CanvasView::cancel()
{
    stroke_profile_.cancel();
    const bool paint = artwork_.stroke_active() && !quick_mask_ && !clone_mask_;
    crop_.reset();crop_saved_.reset();crop_drag_=CropDrag::None;
    perspective_.reset();perspective_saved_.reset();perspective_drag_=-3;
    artwork_.cancel();
    drag_ = panning_ = zooming_ = selecting_ = space_ = false;
    lasso_path_.clear();
    polygon_path_.clear();
    magnetic_preview_.clear(); magnetic_anchors_.clear(); magnetic_reference_.reset(); magnetic_tiled_=false;
    polygon_active_ = false;
    if (quick_mask_) { quick_mask_->cancel_stroke(); quick_mask_.reset(); quick_mask_dense_before_.reset(); quick_mask_tiled_before_.reset(); if (quick_mask_changed) quick_mask_changed(false); }
    clone_mask_.reset();clone_reference_.reset();clone_selection_.reset();clone_tiled_selection_.reset();
    if (paint) schedule_stroke_render();
    else refresh();
}
bool CanvasView::begin_quick_mask()
{
    if (quick_mask_) return true;
    if (artwork_.stroke_active() || drag_ || polygon_active_) cancel();
    const auto& doc=artwork_.document();const auto origin=canvasOrigin(doc);
    const bool tiled=doc.canvasMode==CanvasMode::Infinite || std::uint64_t(doc.extent.width)*doc.extent.height>64ULL*1024*1024;
    std::string error;
    if(tiled) {
        auto current=artwork_.tiled_selection_handle();TiledMask initial;
        if(current)initial=*current;
        else {
            auto full=TiledMask::rectangle({origin,doc.extent},{double(origin.x),double(origin.y)},
                {double(origin.x)+doc.extent.width,double(origin.y)+doc.extent.height},false);
            if(!full){if(status)status(full.error);return false;}initial=std::move(*full);
        }
        quick_mask_=QuickMask::create_tiled(std::move(initial),error);
    } else quick_mask_=QuickMask::create(doc.extent,artwork_.selection_mask(),error);
    if (!quick_mask_) { if (status) status(error); return false; }
    quick_mask_revision_=artwork_.revision();quick_mask_dense_before_=artwork_.selection_mask_handle();quick_mask_tiled_before_=artwork_.tiled_selection_handle();
    if (quick_mask_changed) quick_mask_changed(true);
    refresh(false); return true;
}
bool CanvasView::commit_quick_mask()
{
    if (!quick_mask_) return false;
    if (quick_mask_->stroke_active()) { if (status) status("Finish the current mask stroke before applying"); return false; }
    if(artwork_.revision()!=quick_mask_revision_ || artwork_.selection_mask_handle()!=quick_mask_dense_before_ || artwork_.tiled_selection_handle()!=quick_mask_tiled_before_) {
        if(status)status("The document or selection changed; cancel quick mask and try again");return false;
    }
    if (quick_mask_->changed()) {
        if(quick_mask_->tiled()) {
            const auto* mask=quick_mask_->tiled_mask();
            if(!mask || !artwork_.apply_tiled_selection_mask(*mask)) {if(status)status(artwork_.error());return false;}
        } else {
            auto mask=quick_mask_->mask();
            if (!mask) { if (status) status("Insufficient memory for quick mask"); return false; }
            if (!artwork_.apply_selection_mask(std::move(*mask))) { if (status) status(artwork_.error()); return false; }
        }
    }
    quick_mask_.reset();quick_mask_dense_before_.reset();quick_mask_tiled_before_.reset();if (quick_mask_changed) quick_mask_changed(false);
    refresh(false);return true;
}
bool CanvasView::quick_mask_undo()
{
    if (!quick_mask_ || !quick_mask_->undo()) return false;
    render();return true;
}
bool CanvasView::quick_mask_redo()
{
    if (!quick_mask_ || !quick_mask_->redo()) return false;
    render();return true;
}
std::optional<std::vector<Point>> CanvasView::magnetic_segment(Point point, bool manual)
{
    if ((!magnetic_reference_ && !magnetic_tiled_) || polygon_path_.empty()) return std::nullopt;
    if (artwork_.revision()!=magnetic_revision_) {
        cancel();if(status)status("The reference changed; magnetic selection was canceled");return std::nullopt;
    }
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {if(status)status("Invalid magnetic selection point");return std::nullopt;}
    const auto& doc=artwork_.document();const auto origin=canvasOrigin(doc);
    const auto clampWorld=[&](Point p) {return Point{std::clamp(p.x,double(origin.x),double(origin.x)+doc.extent.width-1),
                                                      std::clamp(p.y,double(origin.y),double(origin.y)+doc.extent.height-1)};};
    const auto from=clampWorld(polygon_path_.back()),to=clampWorld(point);
    if (manual) return std::vector<Point>{from,to};
    MagneticPathOptions options;options.radius=std::clamp(magnetic_radius/viewport_.scale,1.,64.);options.maxSearchPixels=131072;
    std::optional<RasterLayer> bounded;const RasterLayer* reference=magnetic_reference_ ? &*magnetic_reference_ : nullptr;
    auto refOrigin=origin;
    if (magnetic_tiled_) {
        const auto pad=std::ceil(options.radius)+2.;
        const double left=std::max(double(origin.x),std::floor(std::min(from.x,to.x))-pad);
        const double top=std::max(double(origin.y),std::floor(std::min(from.y,to.y))-pad);
        const double right=std::min(double(origin.x)+doc.extent.width,std::floor(std::max(from.x,to.x))+pad+1);
        const double bottom=std::min(double(origin.y)+doc.extent.height,std::floor(std::max(from.y,to.y))+pad+1);
        if(right<=left || bottom<=top || (right-left)*(bottom-top)>1048576.) {
            if(status)status("Magnetic reference exceeds the working budget");return std::nullopt;
        }
        const CanvasRegion region{{int(left),int(top)},{int(right-left),int(bottom-top)}};
        auto rendered=renderFrameRegion(doc,static_cast<FrameIndex>(frame_),region,region.extent);
        if(!rendered.ok()){if(status)status(rendered.message);return std::nullopt;}
        bounded=std::move(rendered.pixels);reference=&*bounded;refOrigin=region.origin;
    }
    if (!reference) return std::nullopt;
    auto traced=magneticSelectionPath(*reference,{from.x-refOrigin.x,from.y-refOrigin.y},
                                       {to.x-refOrigin.x,to.y-refOrigin.y},options);
    if(!traced.ok()){if(status)status(traced.error);return std::nullopt;}
    for(auto& value:traced.points){value.x+=refOrigin.x;value.y+=refOrigin.y;}
    return std::move(traced.points);
}

void CanvasView::finish_polygon(bool manual)
{
    if (tool == "magnetic-lasso" && magnetic_anchors_.size() < 3) {
        if (status) status("The magnetic lasso needs at least three anchors"); return;
    }
    if (tool == "magnetic-lasso" && artwork_.revision() != magnetic_revision_) {
        cancel(); if (status) status("The reference changed; magnetic selection was canceled"); return;
    }
    if (tool == "magnetic-lasso") {
        try {
            auto closing=magnetic_segment(polygon_path_.front(),manual);
            if(!closing || closing->empty()) return;
            if(closing->size()-1>4096-polygon_path_.size()) {if(status)status("Magnetic selection path has too many points");return;}
            polygon_path_.insert(polygon_path_.end(),closing->begin()+1,closing->end());
        } catch(const std::bad_alloc&) {cancel();if(status)status("Insufficient memory for magnetic selection");return;}
    }
    if (polygon_path_.size() < 3) {
        if (status) status("The polygon lasso needs at least three points");
        return;
    }
    auto path = std::move(polygon_path_);
    magnetic_preview_.clear(); magnetic_anchors_.clear(); magnetic_reference_.reset(); magnetic_tiled_=false;
    polygon_active_ = false;
    if (!artwork_.set_polygon_selection(path, polygon_operation_, {selection_antialias, selection_feather}) && status) status(artwork_.error());
    refresh(false);
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
            document, frame_, i - 1,
            {{{{int(std::floor(point.x)), int(std::floor(point.y))}, {1, 1}}, {1, 1}}});
        if (rendered.ok() && !rendered.tiles.empty() && !rendered.tiles[0].pixels.pixels.empty() &&
            (rendered.tiles[0].pixels.pixels[0] >> 24) > 8)
            return artwork_.select(layerProperties(layer).id);
    }
    return false;
}

StrokePoint CanvasView::stroke_sample(const ii::gui::WidgetInput& event, Point point, bool first)
{
    const bool has_time = std::isfinite(event.timestamp) && event.timestamp > 0;
    if (first) { stroke_clock_ = stroke_native_elapsed_ = 0; stroke_last_timestamp_ = has_time ? event.timestamp : 0; }
    else {
        const double delta = has_time && stroke_last_timestamp_ > 0 && event.timestamp >= stroke_last_timestamp_
            ? event.timestamp - stroke_last_timestamp_ : 1.0 / 120.0;
        stroke_native_elapsed_ += delta;
        stroke_clock_ = std::max(stroke_clock_, stroke_native_elapsed_);
        stroke_last_timestamp_ = has_time ? event.timestamp : 0;
    }
    StrokePoint sample;
    sample.position = {point.x, point.y};
    sample.pressure = event.device == ii::gui::PointerDevice::mouse ? 1 : event.pressure;
    sample.time = stroke_clock_;
    sample.tiltX = event.tilt_x / 90.0; sample.tiltY = event.tilt_y / 90.0;
    sample.rotationRadians = event.rotation * std::numbers::pi / 180.0;
    last_stroke_sample_ = sample;
    return sample;
}
bool CanvasView::airbrush_active() const
{
    return airbrush_stroke_ && tool == "brush" && drag_ && (quick_mask_ ? quick_mask_->stroke_active() : artwork_.stroke_active());
}
void CanvasView::cancel_airbrush_stroke()
{
    const bool paint = !quick_mask_ && artwork_.stroke_active();
    if (quick_mask_ && quick_mask_->stroke_active()) quick_mask_->cancel_stroke();
    else if (paint) artwork_.cancel();
    airbrush_stroke_ = false;
    drag_ = panning_ = zooming_ = selecting_ = false;
    if (paint) schedule_stroke_render();
    else { cache_.clear(); render(); }
}
bool CanvasView::airbrush_tick(double elapsed)
{
    if (!airbrush_active() || !std::isfinite(elapsed) || elapsed < 0) return false;
    if (elapsed <= stroke_clock_) return true;
    auto sample = last_stroke_sample_;
    sample.time = elapsed;
    bool ok;
    if (quick_mask_) {
        const auto origin = canvasOrigin(artwork_.document());
        if(!quick_mask_->tiled()) {sample.position.x -= origin.x; sample.position.y -= origin.y;}
        ok = quick_mask_->extend(sample);
        if (!ok && status) status(quick_mask_->error());
    } else {
        ok = artwork_.extend_input(sample);
        if (!ok && status) status(artwork_.error());
        schedule_stroke_render();
    }
    if (!ok) { airbrush_stroke_ = false; return false; }
    stroke_clock_ = elapsed;
    if (quick_mask_) render();
    return true;
}
ii::core::Result<bool> CanvasView::input(const ii::gui::WidgetInput& event)
{
    using K = ii::gui::WidgetInputKind;
    using Key = ii::gui::WidgetKey;
    if (event.kind == K::focus_lost || event.kind == K::pointer_cancel)
    {
        // A native focus change can mean clicking a tool control or beginning
        // an iPad navigation gesture. Cancel only the in-flight mask stroke;
        // keep the session so the toolbar can apply it after focus moves.
        if (quick_mask_) {
            quick_mask_->cancel_stroke();
            drag_ = panning_ = zooming_ = selecting_ = space_ = false;
            render();
        } else if (crop_) {
            if(crop_drag_!=CropDrag::None) crop_=crop_saved_;
            crop_saved_.reset();crop_drag_=CropDrag::None;drag_=panning_=zooming_=selecting_=space_=false;render();
        } else if (perspective_) {
            if(perspective_drag_!=-3) perspective_=perspective_saved_;
            perspective_saved_.reset();perspective_drag_=-3;drag_=panning_=zooming_=selecting_=space_=false;render();
        } else cancel();
        return false;
    }
    if (event.kind == K::key_down && event.key == Key::escape)
    {
        cancel();
        return true;
    }
    if (event.kind == K::key_down && !event.text_input_active && ((crop_ && tool == "crop") || (perspective_ && tool == "perspective-crop")) && event.key == Key::enter) {
        apply_crop();return true;
    }
    if (event.kind == K::key_down && !event.text_input_active && polygon_active_ && tool == "magnetic-lasso")
    {
        if (event.key == Key::backspace || event.key == Key::delete_forward) {
            if (magnetic_anchors_.size() <= 1) { cancel(); return true; }
            magnetic_anchors_.pop_back(); polygon_path_.resize(magnetic_anchors_.back()); magnetic_preview_.clear(); render(); return true;
        }
        if (event.key == Key::enter) { finish_polygon(event.control || event.meta); return true; }
    }
    if (event.kind == K::key_down && !event.text_input_active && polygon_active_ && tool == "polygon-lasso")
    {
        if (event.key == Key::backspace || event.key == Key::delete_forward) {
            polygon_path_.pop_back();
            if (polygon_path_.empty()) polygon_active_ = false;
            render();
            return true;
        }
        if (event.key == Key::enter) { finish_polygon(); return true; }
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
    if (event.kind == K::drop && quick_mask_) { if (status) status("Finish quick mask before importing content"); return true; }
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
    if (tool == "magnetic-lasso" && !space_ && !(event.buttons & 4))
    {
        if (event.kind == K::pointer_down && (!event.buttons || (event.buttons & 1))) {
            const auto point=world(event.x,event.y);
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) return true;
            try {
                if (!polygon_active_) {
                    const auto& doc=artwork_.document();
                    magnetic_tiled_=doc.canvasMode==CanvasMode::Infinite ||
                        std::uint64_t(doc.extent.width)*doc.extent.height>64ULL*1024*1024;
                    if (!magnetic_tiled_) {
                        auto reference=renderFrame(doc,static_cast<FrameIndex>(frame_));
                        if(!reference.ok()) {if(status)status(reference.message);return true;}
                        magnetic_reference_=std::move(reference.pixels);
                    }
                    magnetic_revision_=artwork_.revision();
                    polygon_path_.clear();magnetic_anchors_.clear();magnetic_preview_.clear();polygon_path_.push_back(point);
                    auto initial=magnetic_segment(point,event.control || event.meta);
                    if(!initial || initial->empty()) {polygon_path_.clear();magnetic_reference_.reset(); magnetic_tiled_=false;return true;}
                    polygon_path_.front()=initial->back();magnetic_anchors_.push_back(1);polygon_active_=true;
                    polygon_operation_=event.shift && event.alt ? MaskOperation::Intersect : event.shift ? MaskOperation::Add : event.alt ? MaskOperation::Subtract : MaskOperation::Replace;
                } else if (magnetic_anchors_.size()>=3 && event.timestamp>0 && polygon_last_time_>0 &&
                           event.timestamp>=polygon_last_time_ && event.timestamp-polygon_last_time_<=.4 &&
                           std::hypot(event.x-polygon_last_x_,event.y-polygon_last_y_)<=4) {
                    finish_polygon(event.control || event.meta);return true;
                } else {
                    auto segment=magnetic_segment(point,event.control || event.meta);
                    if(!segment || segment->empty()) return true;
                    if(segment->size()-1>4096-polygon_path_.size()) {if(status)status("Magnetic selection path has too many points");return true;}
                    polygon_path_.insert(polygon_path_.end(),segment->begin()+1,segment->end());
                    magnetic_anchors_.push_back(polygon_path_.size());magnetic_preview_.clear();
                }
                polygon_last_time_=event.timestamp;polygon_last_x_=event.x;polygon_last_y_=event.y;render();
            } catch(const std::bad_alloc&) {cancel();if(status)status("Insufficient memory for magnetic selection");}
            return true;
        }
        if (event.kind == K::pointer_up && !drag_) return true;
        if (event.kind == K::pointer_move && polygon_active_ && !drag_) {
            try { auto segment=magnetic_segment(world(event.x,event.y),event.control || event.meta);
                  if(segment && polygon_active_) {
                      if(segment->size()-1>4096-polygon_path_.size()) {magnetic_preview_.clear();if(status)status("Magnetic selection path has too many points");}
                      else magnetic_preview_=std::move(*segment);
                  } render(); }
            catch(const std::bad_alloc&) {cancel();if(status)status("Insufficient memory for magnetic selection");}
            return true;
        }
    }
    if (tool == "polygon-lasso" && !space_ && !(event.buttons & 4))
    {
        if (event.kind == K::pointer_down && (!event.buttons || (event.buttons & 1))) {
            const auto point = world(event.x, event.y);
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) return true;
            if (!polygon_active_) {
                polygon_active_ = true;
                polygon_path_.clear();
                polygon_operation_ = event.shift && event.alt ? MaskOperation::Intersect : event.shift ? MaskOperation::Add : event.alt ? MaskOperation::Subtract : MaskOperation::Replace;
            } else if (polygon_path_.size() >= 3 && event.timestamp > 0 && polygon_last_time_ > 0 &&
                       event.timestamp >= polygon_last_time_ && event.timestamp - polygon_last_time_ <= 0.4 &&
                       std::hypot(event.x - polygon_last_x_, event.y - polygon_last_y_) <= 4) {
                finish_polygon();
                return true;
            }
            if (polygon_path_.size() >= 4096) {
                if (status) status("The polygon lasso path has too many points");
                cancel();
                return true;
            }
            polygon_path_.push_back(point);
            polygon_cursor_ = point;
            polygon_last_time_ = event.timestamp; polygon_last_x_ = event.x; polygon_last_y_ = event.y;
            render();
            return true;
        }
        if (event.kind == K::pointer_up && !drag_) return true;
        if (event.kind == K::pointer_move && polygon_active_ && !drag_) {
            polygon_cursor_ = world(event.x, event.y);
            render();
            return true;
        }
    }
    if (event.kind == K::pointer_down)
    {
        if (event.buttons && !(event.buttons & 1) && !(event.buttons & 4))
            return false;
        down_ = world(event.x, event.y);
        last_ = {event.x, event.y};
        if(tool=="polygon" || tool=="rectangle" || tool=="ellipse")polygon_regular_preview_=event.shift;
        zooming_ = tool == "zoom" || (space_ && (event.control || event.meta));
        panning_ = (space_ && !zooming_) || tool == "pan" || (event.buttons & 4);
        drag_ = true;
        if (panning_ || zooming_)
            return true;
        if (tool == "perspective-crop") {
            if (artwork_.document().canvasMode != CanvasMode::Finite) {drag_=false;if(status)status("Perspective crop requires a finite canvas");return true;}
            if (perspective_ && artwork_.revision()!=crop_revision_) {cancel();if(status)status("The document changed; crop was canceled");return true;}
            const auto extent=artwork_.document().extent;auto point=down_;
            if(!std::isfinite(point.x)||!std::isfinite(point.y)) {drag_=false;if(status)status("Invalid crop point");return true;}
            point.x=std::clamp(point.x,0.,double(extent.width));point.y=std::clamp(point.y,0.,double(extent.height));
            perspective_saved_=perspective_;crop_drag_start_=point;perspective_drag_=-2;
            if(perspective_) {
                std::size_t nearest=0;double distance=std::numeric_limits<double>::infinity();
                double left=extent.width,right=0,top=extent.height,bottom=0;
                for(std::size_t i=0;i<4;++i){const auto& p=(*perspective_)[i];const auto d=std::hypot(point.x-p.x,point.y-p.y);if(d<distance){distance=d;nearest=i;}left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
                if(distance*viewport_.scale<=Metric::space6) perspective_drag_=int(nearest);
                else if(point.x>=left&&point.x<=right&&point.y>=top&&point.y<=bottom) perspective_drag_=-1;
                else perspective_=std::array<Point,4>{point,point,point,point};
            } else {perspective_=std::array<Point,4>{point,point,point,point};crop_revision_=artwork_.revision();}
            crop_anchor_=point;render();return true;
        }
        if (tool == "crop") {
            if (artwork_.document().canvasMode != CanvasMode::Finite) {drag_=false;if(status)status("Crop requires a finite canvas");return true;}
            if (crop_ && artwork_.revision()!=crop_revision_) {cancel();if(status)status("The document changed; crop was canceled");return true;}
            const auto extent=artwork_.document().extent;auto point=down_;
            if(!std::isfinite(point.x) || !std::isfinite(point.y)) {drag_=false;if(status)status("Invalid crop point");return true;}
            point.x=std::clamp(point.x,0.,double(extent.width));point.y=std::clamp(point.y,0.,double(extent.height));
            crop_saved_=crop_;crop_drag_start_=point;crop_drag_=CropDrag::Resize;
            if (crop_) {
                const auto old=*crop_;
                const std::array<Point,4> corners{{{old.left,old.top},{old.right,old.top},{old.right,old.bottom},{old.left,old.bottom}}};
                std::size_t nearest=0;double distance=std::numeric_limits<double>::infinity();
                for(std::size_t i=0;i<corners.size();++i){const auto d=std::hypot(point.x-corners[i].x,point.y-corners[i].y);if(d<distance){distance=d;nearest=i;}}
                if(distance*viewport_.scale<=Metric::space6) crop_anchor_=corners[(nearest+2)%4];
                else if(point.x>=old.left&&point.x<=old.right&&point.y>=old.top&&point.y<=old.bottom) {crop_drag_=CropDrag::Move;crop_anchor_=point;}
                else {crop_=CropRect{point.x,point.y,point.x,point.y};crop_anchor_=point;}
            } else {crop_=CropRect{point.x,point.y,point.x,point.y};crop_anchor_=point;crop_revision_=artwork_.revision();}
            render();return true;
        }
        if (quick_mask_)
        {
            if (tool != "brush" && tool != "eraser") { drag_=false; if(status) status("Use a brush or eraser in quick mask"); return true; }
            BitmapBrush brush=settings;brush.size=brush_size;brush.opacity=opacity;
            auto sample=stroke_sample(event,down_,true);const auto origin=canvasOrigin(artwork_.document());
            if(!quick_mask_->tiled()) {sample.position.x-=origin.x;sample.position.y-=origin.y;}
            const bool subtract=tool=="brush" && event.device!=ii::gui::PointerDevice::eraser;
            if (!quick_mask_->begin(sample,brush,subtract)) { drag_=false; if(status) status(quick_mask_->error()); }
            airbrush_stroke_ = drag_ && brush.engineState && brush.engineState->rasterizer.airbrushEnabled;
            if (airbrush_stroke_) ++airbrush_generation_;
            render();return true;
        }
        if (tool == "clone-stamp") {
            const auto& doc=artwork_.document();const auto x=std::floor(down_.x),y=std::floor(down_.y);
            if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=doc.extent.width||y>=doc.extent.height){drag_=false;if(status)status("Choose a point inside the canvas");return true;}
            if(event.alt) {clone_source_=Point{x,y};clone_source_extent_=doc.extent;clone_source_layer_=artwork_.selected();clone_document_=artwork_.recovery();clone_aligned_offset_.reset();drag_=false;if(status)status("Clone source set");return true;}
            const auto sourcePresent=std::ranges::any_of(doc.layers,[&](const auto& item){return layerProperties(item).id==clone_source_layer_;});
            if(!clone_source_ || !sourcePresent || clone_source_extent_!=doc.extent || clone_document_!=artwork_.recovery()){clone_source_.reset();clone_aligned_offset_.reset();drag_=false;if(status)status("Alt-click to set a clone source");return true;}
            const auto dx=std::int64_t(clone_source_->x)-std::int64_t(x),dy=std::int64_t(clone_source_->y)-std::int64_t(y);
            if(dx<std::numeric_limits<int>::min() || dx>std::numeric_limits<int>::max() || dy<std::numeric_limits<int>::min() || dy>std::numeric_limits<int>::max()){drag_=false;if(status)status("Clone source is out of range");return true;}
            clone_offset_=clone_aligned && clone_aligned_offset_ ? *clone_aligned_offset_ : CanvasOrigin{int(dx),int(dy)};
            ::RasterLayer reference;if(!artwork_.clone_reference(static_cast<FrameIndex>(frame_),clone_current_layer,reference)){drag_=false;if(status)status(artwork_.error());return true;}
            try {RasterMask empty{doc.extent,std::vector<std::uint8_t>(std::size_t(doc.extent.width)*doc.extent.height,0)};std::string error;
                clone_mask_=QuickMask::create(doc.extent,&empty,error);if(!clone_mask_){drag_=false;if(status)status(error);return true;}
                clone_reference_=std::move(reference);clone_revision_=artwork_.revision();clone_selected_=artwork_.selected();clone_selection_=artwork_.selection_mask_handle();clone_tiled_selection_=artwork_.tiled_selection_handle();
                BitmapBrush brush=settings;brush.size=brush_size;brush.opacity=opacity;
                if(!clone_mask_->begin(stroke_sample(event,down_,true),brush,false)){auto message=clone_mask_->error();clone_mask_.reset();clone_reference_.reset();drag_=false;if(status)status(message);return true;}
            } catch(const std::bad_alloc&) {clone_mask_.reset();clone_reference_.reset();drag_=false;if(status)status("Insufficient memory for clone stroke");return true;}
            render();return true;
        }
        if (tool == "magic-wand")
        {
            drag_ = false;
            const auto op = event.shift && event.alt ? MaskOperation::Intersect : event.shift ? MaskOperation::Add : event.alt ? MaskOperation::Subtract : MaskOperation::Replace;
            if (!artwork_.set_wand_selection(down_, static_cast<FrameIndex>(frame_), wand_tolerance,
                                             wand_contiguous, op, wand_current_layer, {selection_antialias, selection_feather}) && status)
                status(artwork_.error());
            refresh(false);
            return true;
        }
        if (tool == "eyedropper")
        {
            auto pixel = renderFrameRegion(
                artwork_.document(), frame_,
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
        if (tool == "rect-marquee" || tool == "ellipse-marquee") return true;
        if (tool == "lasso") { lasso_path_.clear(); lasso_path_.push_back(down_); return true; }
        if (tool == "select")
        {
            selecting_ = hit(down_);
            return true;
        }
        if (tool == "brush" || tool == "eraser")
        {
            // A lost release can be followed by a new pointer-down. Present
            // the canceled stroke's damage before begin_input resets it or
            // changes the transform used to map that damage to the canvas.
            if (artwork_.stroke_active()) {
                stroke_profile_.cancel();
                artwork_.cancel();
                schedule_stroke_render();
            }
            BitmapBrush brush = settings;
            brush.argb = color;
            brush.size = brush_size;
            brush.opacity = opacity;
            brush.eraser = tool == "eraser" || event.device == ii::gui::PointerDevice::eraser;
            auto taper_options = taper;
            if (artwork_.document().canvasMode != CanvasMode::Finite && taper_options.strength > 0 &&
                taper_options.minimum < 1 && (taper_options.start > 0 || taper_options.end > 0)) {
                taper_options.strength = 0;
                if (status) status("Whole-stroke taper is not yet available for infinite canvases");
            }
            const auto profile_start = stroke_profile_.enabled() ? ProfileClock::now() : ProfileClock::time_point{};
            const auto ok = artwork_.begin_input(stroke_sample(event, down_, true), brush,
                                                 static_cast<FrameIndex>(frame_), taper_options);
            if (ok && stroke_profile_.enabled())
                stroke_profile_.begin(profile_elapsed(profile_start), event.timestamp);
            if (!ok && status)
                status(artwork_.error());
            airbrush_stroke_ = ok && !brush.eraser && brush.engineState && brush.engineState->rasterizer.airbrushEnabled;
            if (airbrush_stroke_) ++airbrush_generation_;
            schedule_stroke_render();
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
            if (!artwork_.fill_at(down_, color, static_cast<FrameIndex>(frame_),
                                  {fill_tolerance, fill_contiguous, fill_current_layer, fill_target_current, fill_preserve_alpha, fill_antialias}) && status)
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
        else if (tool == "perspective-crop" && perspective_drag_!=-3)
        {
            update_perspective(world(event.x,event.y));
        }
        else if (tool == "crop" && crop_drag_!=CropDrag::None)
        {
            update_crop(world(event.x,event.y));
        }
        else if (tool == "lasso")
        {
            if (std::hypot(event.x - last_.x, event.y - last_.y) >= 0.5) {
                if (lasso_path_.size() >= 4096) {
                    if (status) status("The lasso path has too many points");
                    cancel(); return true;
                }
                lasso_path_.push_back(world(event.x, event.y));
            }
        }
        else if (clone_mask_ && clone_mask_->stroke_active()) {
            if(artwork_.recovery()!=clone_document_ || artwork_.revision()!=clone_revision_ || artwork_.selected()!=clone_selected_ || artwork_.selection_mask_handle()!=clone_selection_ || artwork_.tiled_selection_handle()!=clone_tiled_selection_) {cancel();if(status)status("The document or selection changed; clone was canceled");return true;}
            if(!clone_mask_->extend(stroke_sample(event,world(event.x,event.y),false))){auto message=clone_mask_->error();cancel();if(status)status(message);return true;}
        }
        else if (quick_mask_ && quick_mask_->stroke_active())
        {
            auto point=world(event.x,event.y);auto sample=stroke_sample(event,point,false);const auto origin=canvasOrigin(artwork_.document());
            if(!quick_mask_->tiled()) {sample.position.x-=origin.x;sample.position.y-=origin.y;}
            if (!quick_mask_->extend(sample) && status) status(quick_mask_->error());
        }
        else if (artwork_.stroke_active())
        {
            auto point = world(event.x, event.y);
            const bool profiling = stroke_profile_.active();
            const auto profile_start = profiling ? ProfileClock::now() : ProfileClock::time_point{};
            const bool extended = artwork_.extend_input(stroke_sample(event, point, false));
            if (profiling) stroke_profile_.move(profile_elapsed(profile_start), event.timestamp);
            if (!extended) {
                stroke_profile_.cancel();
                if (status) status(artwork_.error());
            }
            last_ = {event.x, event.y};
            schedule_stroke_render();
            return true;
        }
        last_ = {event.x, event.y};
        if(tool=="polygon" || tool=="rectangle" || tool=="ellipse")polygon_regular_preview_=event.shift;
        render();
        return true;
    }
    if (event.kind == K::pointer_up && drag_)
    {
        auto point = world(event.x, event.y);
        const bool was_pan = panning_ || zooming_;
        drag_ = panning_ = zooming_ = false;
        if (!was_pan && tool == "perspective-crop" && perspective_drag_!=-3)
        {
            update_perspective(point);perspective_drag_=-3;perspective_saved_.reset();render();return true;
        }
        if (!was_pan && tool == "crop" && crop_drag_!=CropDrag::None)
        {
            update_crop(point);crop_drag_=CropDrag::None;crop_saved_.reset();render();return true;
        }
        if (!was_pan && clone_mask_) {
            const bool valid=artwork_.recovery()==clone_document_ && artwork_.revision()==clone_revision_ && artwork_.selected()==clone_selected_ && artwork_.selection_mask_handle()==clone_selection_ && artwork_.tiled_selection_handle()==clone_tiled_selection_;
            if(!valid) {if(status)status("The document or selection changed; clone was canceled");}
            else if(!clone_mask_->end(stroke_sample(event,point,false))) {if(status)status(clone_mask_->error());}
            else if(auto mask=clone_mask_->mask();!mask) {if(status)status("Cannot read the clone stroke mask");}
            else if(!artwork_.clone_stamp(*clone_reference_,*mask,clone_offset_,static_cast<FrameIndex>(frame_))) {if(status)status(artwork_.error());}
            else if(clone_aligned)clone_aligned_offset_=clone_offset_;
            clone_mask_.reset();clone_reference_.reset();clone_selection_.reset();clone_tiled_selection_.reset();refresh();return true;
        }
        if (!was_pan && quick_mask_)
        {
            if (quick_mask_->stroke_active()) {
                auto sample=stroke_sample(event,point,false);const auto origin=canvasOrigin(artwork_.document());
                if(!quick_mask_->tiled()) {sample.position.x-=origin.x;sample.position.y-=origin.y;}
                if (!quick_mask_->end(sample) && status) status(quick_mask_->error());
            }
            render();return true;
        }
        if (artwork_.stroke_active())
        {
            const bool profiling = stroke_profile_.active();
            const auto profile_start = profiling ? ProfileClock::now() : ProfileClock::time_point{};
            struct CommitGuard {
                bool& flag;
                explicit CommitGuard(bool& value) : flag(value) { flag = true; }
                ~CommitGuard() { flag = false; }
            };
            bool ok;
            {
                CommitGuard guard(committing_stroke_);
                ok = artwork_.end_input(stroke_sample(event, point, false));
            }
            if (profiling) report_stroke_profile(stroke_profile_.finish(profile_elapsed(profile_start), ok));
            if (!ok && status)
                status(artwork_.error());
            schedule_stroke_render();
            return true;
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
        else if (!was_pan && tool == "gradient")
        {
            if (std::hypot(point.x - down_.x, point.y - down_.y) > 1e-9) {
                GradientDefinition gradient;
                gradient.start = down_; gradient.end = point;
                gradient.stops = gradient_stops.empty() ? std::vector<GradientStop>{{0, color}, {1, color & 0x00ffffffU}} : gradient_stops;
                gradient.kind = gradient_kind; gradient.spread = gradient_spread;
                gradient.opacity = opacity;
                if (!artwork_.gradient(std::move(gradient), static_cast<FrameIndex>(frame_), gradient_blend) && status)
                    status(artwork_.error());
            }
        }
        else if (!was_pan && tool == "lasso")
        {
            if (lasso_path_.size() >= 4096) {
                lasso_path_.clear();
                if (status) status("The lasso path has too many points");
                refresh(false); return true;
            }
            lasso_path_.push_back(point);
            const auto op = event.shift && event.alt ? MaskOperation::Intersect : event.shift ? MaskOperation::Add : event.alt ? MaskOperation::Subtract : MaskOperation::Replace;
            if (lasso_path_.size() < 3) { if (status) status("The lasso needs at least three points"); }
            else if (!artwork_.set_polygon_selection(lasso_path_, op, {selection_antialias, selection_feather}) && status) status(artwork_.error());
            lasso_path_.clear();
        }
        else if (!was_pan && (tool == "rect-marquee" || tool == "ellipse-marquee"))
        {
            const auto shape = tool == "rect-marquee" ? SelectionGeometry::Rectangle : SelectionGeometry::Ellipse;
            const auto op = event.shift && event.alt ? MaskOperation::Intersect : event.shift ? MaskOperation::Add : event.alt ? MaskOperation::Subtract : MaskOperation::Replace;
            if (!artwork_.set_selection(shape, down_, point, op, {selection_antialias, selection_feather}) && status) status(artwork_.error());
        }
        else if (!was_pan && tool != "brush" && tool != "eraser" && tool != "select" &&
                 tool != "text" && tool != "fill")
        {
            auto options=polygon_options;options.regular=options.regular || event.shift;
            bool applied;
            if(tool=="rectangle" || tool=="ellipse") {
                ShapeStyle style{options.from_center,options.regular,options.fill,options.outline,options.stroke_width,options.opacity};
                applied=artwork_.styled_shape(tool,down_,point,color,style,static_cast<FrameIndex>(frame_));
            } else applied=artwork_.shape(tool,down_,point,color,brush_size,event.shift,options,static_cast<FrameIndex>(frame_));
            if(!applied && status)status(artwork_.error());
        }
        refresh(!was_pan);
        return true;
    }
    return false;
}
} // namespace congregation::native
