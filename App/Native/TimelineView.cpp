#include "TimelineView.h"
#include "EditorLayout.h"
#include "Style.h"
#include "WidgetStyle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iiFileProviderNative.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <tuple>
#include <utility>

namespace congregation::native
{
namespace
{
using namespace widgets;
using ii::gui::Rect;
constexpr double columns = Metric::timelineColumnsWidth;
void text(ii::gui::WidgetDrawing &d, Rect rect, std::string value,
          double size = Metric::textCaption, unsigned weight = Metric::weightRegular)
{
    d.texts.push_back({rect, std::move(value), editor::linear(ink), float(size), weight,
                       ii::gui::TextAlignment::left, "Pretendard", float(size), true});
}
void image(ii::gui::WidgetDrawing &d, Rect r, std::string_view name)
{
    d.images.push_back({r, take(lvrs::Icons::image(name, Metric::iconRasterSize))});
}
void line(ii::gui::WidgetDrawing &d, Rect r,
          ii::gui::WidgetColor color = editor::panel_edge_color())
{
    const auto c = editor::linear(color);
    d.panels.push_back({r, c, c});
}
std::shared_ptr<const ii::gui::RasterImage> marker(const std::filesystem::path &root)
{
    const auto mask = take(ii::gui::svg_image(
        iiFileProvider::native::File::read(root / "playhead-marker.svg", 1024 * 1024),
        Metric::size44, Metric::size52));
    const auto color = take(ii::gui::svg_image(
        iiFileProvider::native::File::read(root / "playhead-line.svg", 1024 * 1024), Metric::size44,
        Metric::size52));
    auto pixels = std::vector<std::uint8_t>(color->rgba().begin(), color->rgba().end());
    // Exported SVGs supply geometry/coverage; the app token owns their color.
    const auto tint = Color::playhead;
    const std::array channels{std::uint8_t(std::lround(tint.red * 255)),
                              std::uint8_t(std::lround(tint.green * 255)),
                              std::uint8_t(std::lround(tint.blue * 255))};
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        for (std::size_t channel = 0; channel < channels.size(); ++channel)
            pixels[i + channel] = channels[channel];
        pixels[i + 3] = std::uint8_t(
            std::lround(unsigned(pixels[i + 3]) * mask->rgba()[i + 3] / 255 * tint.alpha));
    }
    return take(ii::gui::rgba_image(Metric::size44, Metric::size52, std::move(pixels)));
}
} // namespace
TimelineView::TimelineView(std::shared_ptr<congregation::ui::EditorBackend> vm,
                           std::filesystem::path assets)
    : vm_(std::move(vm)), assets_(std::move(assets))
{
}
ii::gui::Widget TimelineView::build(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    using A = congregation::ui::Action;
    Builder b(raw, "timeline/");
    const auto root =
        b.column(parent, "timeline", 0, Metric::timelineHeight, Metric::space0, Metric::space4);
    borders(root, true, true);
    const auto weak = weak_from_this();
    good(raw.retain(bindings_));
    marker_ = marker(assets_);
    auto transport = b.row(root, "timeline-transport", 0, Metric::size24, Metric::space4);
    b.icon(transport, "timeline-undo", "generalundo", [vm = vm_] { vm->undo(); });
    b.icon(transport, "timeline-redo", "generalredo", [vm = vm_] { vm->redo(); });
    b.icon(transport, "previous-frame", "previous-frame",
           [vm = vm_] { vm->timeline->advance(-1); });
    play_ = b.icon(transport, "timeline-play", "play",
                   [weak]
                   {
                       if (auto s = weak.lock())
                       {
                           s->vm_->timeline->playing(!s->vm_->timeline->state.get().playing);
                           s->last_frame_.reset();
                           good(s->body_.request_frames());
                       }
                   });
    b.icon(transport, "next-frame", "next-frame", [vm = vm_] { vm->timeline->advance(1); });
    good(b.icon(transport, "onion-skin", "onion-skin").set_enabled(false));
    good(b.icon(transport, "timeline-add-keyframe", "add-keyframe").set_enabled(false));
    good(b.icon(transport, "timeline-remove-keyframe", "delete-keyframe").set_enabled(false));
    time_ = take(b.controls.input_field(transport, "timeline-time",
                                        {.text = vm_->timeline->time_text(),
                                         .read_only = true,
                                         .width = Metric::size100,
                                         .accessibility_name = "Elapsed time",
                                         .style = ii::gui::InputStyle{.height = Metric::size22}}));
    auto start = take(b.controls.slider(transport, "timeline-start",
                                        {.minimum = 0,
                                         .maximum = 1,
                                         .value = 0,
                                         .width = Metric::size160,
                                         .thumb_diameter = Metric::size12,
                                         .track_height = Metric::size4,
                                         .range_inset = Metric::space6,
                                         .accessibility_name = "Timeline zoom",
                                         .height = Metric::size22},
                                        [weak](double v)
                                        {
                                            if (auto s = weak.lock())
                                            {
                                                s->zoom_ = 1 + v * 7;
                                                s->paint();
                                            }
                                        }));
    good(start.set_flex(1));
    auto scale = take(b.controls.slider(
        transport, "timeline-scale",
        {.minimum = 0,
         .maximum = 1,
         .value = vm_->timeline->seconds() / vm_->timeline->state.get().duration,
         .width = Metric::size160,
         .thumb_diameter = Metric::size12,
         .track_height = Metric::size4,
         .range_inset = Metric::space6,
         .accessibility_name = "Timeline position",
         .height = Metric::size22},
        [vm = vm_](double v) { vm->timeline->scrub(v * vm->timeline->state.get().duration); }));
    good(scale.set_flex(1));
    good(bindings_->bind(scale, vm_->timeline->state,
                         [model = vm_->timeline](Widget w, const TimelinePosition &state)
                         { return w.set_value(model->seconds() / state.duration); }));
    b.space(transport);
    b.icon(transport, "add-layer", "new-layer", [vm = vm_] { vm->add_layer(); });
    b.icon(transport, "duplicate-layer", "duplicate-layer", [vm = vm_] { vm->duplicate_layer(); });
    for (const auto &[id, icon] : std::initializer_list<std::pair<const char *, const char *>>{
             {"layer-groups", "group-layer"},
             {"layer-mask", "layer-mask"},
             {"clipping-mask", "clipping-mask"},
             {"layer-lock", "lock"}})
        good(b.icon(transport, id, icon).set_enabled(false));
    b.icon(transport, "delete-layer", "delete", [vm = vm_] { vm->delete_layer(); });
    good(b.icon(transport, "link-layers", "link-layers").set_enabled(false));
    good(b.icon(transport, "timeline-snap", "snap").set_enabled(false));
    b.icon(transport, "layer-stack", "panel-layers-stack",
           [weak]
           {
               if (auto s = weak.lock())
                   good(s->scroll_.set_scroll_offset(0));
           });
    good(b.icon(transport, "add-motion-keyframe", "add-keyframe").set_enabled(false));
    good(b.icon(transport, "remove-motion-keyframe", "delete-keyframe").set_enabled(false));
    b.icon(transport, "timeline-menu", "generalmoreVertical",
           [vm = vm_] { vm->option_tab.set(1); });
    ruler_ = take(b.controls.event_listener(
        root, "timeline-ruler", {Metric::size1361, Metric::size44},
        [weak](const ii::gui::WidgetInput &e)
        {
            if (auto s = weak.lock())
                return s->input(e, true);
            return ii::core::Result<bool>{false};
        },
        [weak](Rect)
        {
            if (auto s = weak.lock())
                s->paint();
        }));
    scroll_ = take(raw.scroll_view(root, "timeline-scroll",
                                   {.height = Metric::timelineBodyHeight,
                                    .virtual_content_height = Metric::timelineBodyHeight}));
    good(scroll_.set_flex(1));
    body_ = take(b.controls.event_listener(
        scroll_, "layers", {Metric::size1361, Metric::timelineBodyHeight},
        [weak](const ii::gui::WidgetInput &e)
        {
            if (auto s = weak.lock())
                return s->input(e, false);
            return ii::core::Result<bool>{false};
        },
        [weak](Rect)
        {
            if (auto s = weak.lock())
                s->paint();
        }));
    good(ruler_.set_clip_content(true));
    good(body_.set_clip_content(true));
    good(scroll_.set_scroll_handler(
        [weak](const ii::gui::ScrollViewport &viewport)
        {
            if (auto s = weak.lock())
            {
                s->offset_ = viewport.offset;
                s->paint();
            }
        }));
    good(bindings_->bind(root, vm_->timeline->state,
                         [weak](Widget, const TimelinePosition &) -> ii::core::Result<void>
                         {
                             if (auto s = weak.lock())
                             {
                                 if (!s->time_.is_null())
                                     good(s->time_.set_text(s->vm_->timeline->time_text()));
                                 s->paint();
                             }
                             return {};
                         }));
    good(body_.set_frame_handler(
        [weak](ii::runtime::Clock::time_point now) -> ii::core::Result<bool>
        {
            auto s = weak.lock();
            if (!s || !s->vm_->timeline->state.get().playing)
                return false;
            const double rate = s->vm_->timeline->state.get().rate;
            if (!s->last_frame_)
                s->last_frame_ = now;
            auto elapsed = std::chrono::duration<double>(now - *s->last_frame_).count();
            if (elapsed >= 1 / rate)
            {
                const auto count = int(elapsed * rate);
                s->last_frame_ = now;
                s->vm_->timeline->advance(count);
            }
            return true;
        }));
    rename_ = take(b.controls.popover(root, "rename-layer",
                                      {.width = Metric::size300,
                                       .height = Metric::size98,
                                       .padding = Metric::space12,
                                       .placement = ii::gui::PopoverPlacement::centered}));
    auto form = b.column(rename_);
    b.label(form, "Rename layer", Metric::textBody, {}, Metric::weightSemiBold);
    rename_field_ = take(b.controls.input_field(
        form, "layer-name", {.width = Metric::size276, .accessibility_name = "Layer name"},
        [weak](std::string name)
        {
            if (auto s = weak.lock())
                s->rename_text_ = std::move(name);
        },
        [weak]
        {
            if (auto s = weak.lock())
                s->commit_rename();
        }));
    b.button(form, "commit-layer-name", "Rename",
             [weak]
             {
                 if (auto s = weak.lock())
                     s->commit_rename();
             });
    return root;
}
void TimelineView::begin_rename()
{
    const auto found = std::ranges::find(rows_, selected_, &Row::id);
    if (found == rows_.end())
        return;
    rename_key_ = found->id;
    rename_text_ = found->name;
    widgets::good(rename_field_.set_text(rename_text_));
    widgets::good(rename_.open_for(body_));
    widgets::good(rename_field_.focus());
}
void TimelineView::commit_rename()
{
    if (rename_text_.empty())
        return;
    widgets::good(rename_.close_popup());
    vm_->rename_layer(rename_key_, rename_text_);
}
void TimelineView::refresh_document(const iiSharedCanvas::Document &doc,
                                    const std::string &selected)
{
    rows_.clear();
    selected_ = selected;
    for (auto it = doc.layers.rbegin(); it != doc.layers.rend(); ++it)
    {
        const auto &p = iiSharedCanvas::layerProperties(*it);
        Row r{p.id, p.name, "brush", "Normal", p.opacity, p.visible, {}};
        if (p.blendMode == RasterBlendMode::Multiply)
            r.mode = "Multiply";
        else if (p.blendMode == RasterBlendMode::Screen)
            r.mode = "Screen";
        if (iiSharedCanvas::layerRepresentation(*it) == iiSharedCanvas::LayerRepresentation::Vector)
            r.icon = "pen";
        for (const auto &f : doc.frames)
            if (std::ranges::any_of(f.keyframes, [&](const auto &k) { return k.layerId == p.id; }))
                r.keys.push_back(int(f.index));
        rows_.push_back(std::move(r));
    }
    auto options = ii::gui::ScrollOptions{
        .height = Metric::timelineBodyHeight,
        .virtual_content_height = std::max<double>(
            Metric::timelineBodyHeight, double(rows_.size() * Metric::timelineRowHeight))};
    widgets::good(scroll_.set_scroll_options(options));
    paint();
}
void TimelineView::paint()
{
    using namespace widgets;
    const auto rs = ruler_.snapshot(), bs = body_.snapshot();
    if (!rs || !bs)
        return;
    const auto state = vm_->timeline->state.get();
    const double visible_duration = state.duration / zoom_;
    const double width = rs->bounds.width, time_width = std::max(1., width - columns);
    const auto blue = Color::playhead;
    ii::gui::WidgetDrawing header;
    image(header, {0, Metric::size22, Metric::size18, Metric::size18}, "inspectionsEye");
    text(header, {Metric::space24, Metric::space16, Metric::size185, Metric::size22},
         "Layer / Track", Metric::textDescription, Metric::weightMedium);
    image(header, {Metric::size216, Metric::size22, Metric::size18, Metric::size18}, "layer-mask");
    image(header, {Metric::size240, Metric::size22, Metric::size18, Metric::size18}, "lock");
    text(header, {Metric::size264, Metric::space16, Metric::size90, Metric::size22}, "Mode");
    text(header, {Metric::size360, Metric::space16, Metric::size56, Metric::size22}, "Opacity");
    for (int i = 0; i <= 8; ++i)
    {
        const auto x = columns + time_width * i / 8;
        std::string label =
            ii::core::format_decimal(std::round(visible_duration * i / 8 * 100) / 100) + "s";
        text(header,
             {float(std::min(x, width - Metric::size36)), 0, Metric::size44, Metric::size18},
             label);
    }
    const int ticks = std::clamp(int(std::round(visible_duration * state.rate)), 1, 1000);
    for (int i = 0; i <= ticks; ++i)
    {
        const auto x = columns + time_width * i / ticks;
        line(header,
             {float(x), float(i % state.rate == 0 ? Metric::size18 : Metric::size26),
              Metric::stroke1, float(i % state.rate == 0 ? Metric::size26 : Metric::space10)},
             Color::timelineTick);
    }
    const auto play_x = columns + time_width * vm_->timeline->seconds() / visible_duration;
    header.images.push_back(
        {{float(play_x - Metric::size11), Metric::size18, Metric::size22, Metric::size26},
         marker_});
    good(ruler_.set_drawing(std::move(header)));
    ii::gui::WidgetDrawing body;
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        const auto y = double(i) * Metric::timelineRowHeight - offset_;
        if (y + Metric::timelineRowHeight <= 0 || y >= bs->bounds.height)
            continue;
        const auto &r = rows_[i];
        if (r.id == selected_)
            line(body, {Metric::space24, float(y), Metric::size190, Metric::size22},
                 Color::timelineSelection);
        image(body, {0, float(y + Metric::space2), Metric::size18, Metric::size18},
              r.visible ? "inspectionsEye" : "lock");
        image(body, {Metric::size32, float(y + Metric::space2), Metric::size18, Metric::size18},
              r.icon);
        text(body, {Metric::size52, float(y + Metric::size5_5), Metric::size160, Metric::space12},
             r.name, Metric::textBody, Metric::weightMedium);
        image(body, {Metric::size216, float(y + Metric::space2), Metric::size18, Metric::size18},
              "layer-mask");
        const auto control = editor::linear(Color::timelineControl);
        body.panels.push_back(
            {{Metric::size264, float(y + Metric::stroke1), Metric::size94, Metric::size20},
             control,
             control,
             Metric::radius6});
        body.panels.push_back(
            {{Metric::size360, float(y + Metric::stroke1), Metric::size56, Metric::size20},
             control,
             control,
             Metric::radius6});
        text(body, {Metric::size272, float(y + Metric::size4_5), Metric::size84, Metric::size13},
             r.mode, Metric::textBody, Metric::weightMedium);
        text(body, {Metric::size366, float(y + Metric::size4_5), Metric::size48, Metric::size13},
             ii::core::format_decimal(std::round(r.opacity * 100)) + "%", Metric::textBody,
             Metric::weightMedium);
        for (int f = 0; f < ticks; ++f)
        {
            const auto x = columns + time_width * f / ticks, w = time_width / ticks;
            line(body, {float(x + Metric::stroke1), float(y + Metric::size20),
                        float(std::max(0., w - Metric::space2)), Metric::stroke1});
            if (f % 4 == 0)
                text(body,
                     {float(x + Metric::size3), float(y + Metric::space6), Metric::size30,
                      Metric::space12},
                     std::to_string(f / 4 + 1));
        }
        for (int key : r.keys)
        {
            const auto x = columns + time_width * key / ticks;
            image(body,
                  {float(x - Metric::space6), float(y + Metric::size5), Metric::space12,
                   Metric::space12},
                  "keyframe");
        }
    }
    line(body, {float(play_x - Metric::stroke1), 0, Metric::space2, bs->bounds.height}, blue);
    good(body_.set_drawing(std::move(body)));
}
ii::core::Result<bool> TimelineView::input(const ii::gui::WidgetInput &e, bool ruler)
{
    using namespace widgets;
    using K = ii::gui::WidgetInputKind;
    auto widget = ruler ? ruler_ : body_;
    auto bounds = take(widget.snapshot()).bounds;
    const auto x = e.x - bounds.x, y = e.y - bounds.y;
    if (e.kind == K::focus_lost)
    {
        dragging_ = false;
        drag_layer_.clear();
        return false;
    }
    if (e.kind == K::key_down && e.key == ii::gui::WidgetKey::enter && !ruler)
    {
        begin_rename();
        return true;
    }
    if (e.kind == K::key_down)
    {
        if (e.key == ii::gui::WidgetKey::left || e.key == ii::gui::WidgetKey::right)
        {
            vm_->timeline->advance(e.key == ii::gui::WidgetKey::left ? -1 : 1);
            return true;
        }
        if (e.key == ii::gui::WidgetKey::space)
        {
            vm_->timeline->playing(!vm_->timeline->state.get().playing);
            good(body_.request_frames());
            return true;
        }
    }
    if (e.kind == K::pointer_down)
    {
        good(widget.focus());
        if (x >= columns)
        {
            dragging_ = true;
            rate_drag_ = ruler && e.alt;
            down_y_ = e.y;
            down_rate_ = vm_->timeline->state.get().rate;
        }
        else if (!ruler)
        {
            const auto index =
                std::size_t(std::max(0., std::floor((y + offset_) / Metric::timelineRowHeight)));
            if (index >= rows_.size())
                return false;
            const auto row = rows_[index];
            if (x < Metric::size22)
                vm_->check_layer(row.id, !row.visible);
            else if (x >= Metric::size360)
            {
                if (vm_->layer_opacity_changed)
                    vm_->layer_opacity_changed(row.id, row.opacity <= .1 ? 1 : row.opacity - .1);
            }
            else if (x >= Metric::size264)
            {
                if (vm_->layer_mode_changed)
                    vm_->layer_mode_changed(row.id);
            }
            else
            {
                drag_layer_ = row.id;
                drag_start_y_ = e.y;
                vm_->select_layer(row.id);
            }
            return true;
        }
    }
    if (!drag_layer_.empty() && e.kind == K::pointer_up)
    {
        const auto from = std::ranges::find(rows_, drag_layer_, &Row::id);
        auto target = std::size_t(std::clamp(std::floor((y + offset_) / Metric::timelineRowHeight),
                                             0., double(rows_.size())));
        if (from != rows_.end() && target < rows_.size() &&
            std::size_t(from - rows_.begin()) < target)
            ++target;
        auto key = std::exchange(drag_layer_, {});
        if (std::abs(e.y - drag_start_y_) >= Metric::space4)
            vm_->move_layer(std::move(key), "", target < rows_.size() ? rows_[target].id : "");
        return true;
    }
    if (dragging_ &&
        (e.kind == K::pointer_down || e.kind == K::pointer_move || e.kind == K::pointer_up))
    {
        if (rate_drag_)
        {
            const auto rate =
                std::clamp(down_rate_ + int(std::round((down_y_ - e.y) / Metric::space4)), 1, 240);
            vm_->timeline->set_rate(rate);
            if (e.kind == K::pointer_up && vm_->frame_rate_changed)
                vm_->frame_rate_changed(rate);
        }
        else
            vm_->timeline->scrub((x - columns) / std::max(1., double(bounds.width) - columns) *
                                 vm_->timeline->state.get().duration / zoom_);
        if (e.kind == K::pointer_up)
            dragging_ = false;
        return true;
    }
    return false;
}
} // namespace congregation::native
