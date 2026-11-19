#include "TimelineView.h"
#include "Shared/EditorLayout.h"
#include "Shared/Style.h"
#include "Shared/OwnedWidgetStyle.h"
#include <LVUI>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iiFileProvider>
#include <iiSharedCanvas>
#include <iiSDKCore>
#include <tuple>
#include <limits>
#include <unordered_map>
#include <utility>

namespace congregation::native
{
namespace
{
using namespace owned_widgets;
using ii::gui::Rect;
constexpr double columns = Metric::timelineColumnsWidth;
// Figma layer rows contain standard LVUI icon buttons at their intrinsic height.
constexpr double row_height = lvui::icon_button_defaults::extent;
constexpr double transport_height = Metric::size24;
constexpr double ruler_height = Metric::size44;
constexpr double timeline_padding = Metric::space4;
constexpr double timeline_height = transport_height + ruler_height + Metric::timelineBodyHeight +
                                   2 * timeline_padding;
constexpr double body_text_inset = (row_height - Metric::textBodyLineHeight) / 2.0;

// Shared geometry for the ruler, row cells and playhead hit testing. Short
// documents use the overview scale and continue into empty future frames.
struct TimelineGrid final
{
    TimelineGrid(const TimelinePosition& state, double width, double zoom)
        : time_width(std::max(1., width - columns)), rate(state.rate),
          visible_frames(std::max(2., std::max(state.duration, TimelinePosition{}.duration) * rate / zoom)),
          frame_width(time_width / visible_frames) {}

    [[nodiscard]] double x(double frame) const { return columns + frame * frame_width; }
    [[nodiscard]] double time_at(double x) const { return (x - columns) / (frame_width * rate); }

    // Group consecutive divisions in dense views and keep each mark on its grid.
    [[nodiscard]] std::int64_t stride(double divisions) const {
        const auto limit = std::clamp(time_width / Metric::stroke1, 1., 1000.);
        return std::max<std::int64_t>(1, std::int64_t(std::ceil(divisions / limit)));
    }

    double time_width, rate, visible_frames, frame_width;
};

void text(ii::gui::WidgetDrawing &d, Rect rect, std::string value,
          double size = Metric::textCaption, unsigned weight = Metric::weightRegular)
{
    d.texts.push_back({rect, std::move(value), editor::linear(ink), float(size), weight,
                       ii::gui::TextAlignment::left, "Pretendard", float(size), true});
}
void image(ii::gui::WidgetDrawing &d, Rect r, std::string_view name)
{
    d.images.push_back({r, take(lvui::native::design_icon_image_for_logical_size<ii::gui::CachedSvgDecoder>(name))});
}
void line(ii::gui::WidgetDrawing &d, Rect r,
          ii::gui::WidgetColor color = editor::detail_line_color())
{
    if (r.width <= 0 || r.height <= 0) return;
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
    const auto tint = Color::blue;
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
    using namespace owned_widgets;
    using A = congregation::ui::Action;
    Builder b(raw, "timeline/");
    const auto root =
        b.column(parent, "timeline", 0, timeline_height, Metric::space0, timeline_padding);
    borders(root, true, true);
    const auto weak = weak_from_this();
    good(raw.retain(bindings_));
    marker_ = marker(assets_);
    auto transport = b.row(root, "timeline-transport", 0, transport_height, Metric::space4);
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
    b.icon(transport, "onion-skin", "onion-skin");
    b.icon(transport, "timeline-add-keyframe", "add-keyframe");
    b.icon(transport, "timeline-remove-keyframe", "delete-keyframe");
    time_ = take(b.controls.input_field(transport, "timeline-time",
                                        {.text = vm_->timeline->time_text(),
                                         .read_only = true,
                                         .width = Metric::size100,
                                         .accessibility_name = "Elapsed time"}));
    auto start = take(b.controls.slider(transport, "timeline-start",
                                        {.minimum = 0,
                                         .maximum = 1,
                                         .value = 0,
                                         .width = Metric::size160,
                                         .accessibility_name = "Timeline zoom",
                                         .size = lvui::SliderSize::mini},
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
         .accessibility_name = "Timeline position",
         .size = lvui::SliderSize::mini},
        [vm = vm_](double v) { vm->timeline->scrub(v * vm->timeline->state.get().duration); }));
    good(scale.set_flex(1));
    good(bindings_->bind(scale, vm_->timeline->state,
                         [model = vm_->timeline, scale](Widget, const TimelinePosition &state)
                         { return scale.set_value(model->seconds() / state.duration); }));
    b.space(transport);
    take(b.controls.dropdown_button(transport, "add-layer",
        {.tone = ii::gui::ButtonTone::borderless, .icon_name = "new-layer",
         .accessibility_name = "New layer", .icon_size = lvui::design_icon_default_extent, .icon_mode = true,
         .width = Metric::size36 + Metric::space4},
        [vm = vm_] { vm->open_new_layer_menu(); }));
    b.icon(transport, "duplicate-layer", "duplicate-layer", [vm = vm_] { vm->duplicate_layer(); });
    for (const auto &[id, icon] : std::initializer_list<std::pair<const char *, const char *>>{
             {"layer-groups", "group-layer"},
             {"layer-mask", "layer-mask"},
             {"clipping-mask", "clipping-mask"},
             {"layer-lock", "lock"}})
        b.icon(transport, id, icon);
    b.icon(transport, "delete-layer", "delete", [vm = vm_] { vm->delete_layer(); });
    b.icon(transport, "link-layers", "link-layers");
    b.icon(transport, "timeline-snap", "snap");
    b.icon(transport, "layer-stack", "panel-layers-stack",
           [weak]
           {
               if (auto s = weak.lock())
                   good(s->scroll_.set_scroll_offset(0));
           });
    b.icon(transport, "add-motion-keyframe", "add-keyframe");
    b.icon(transport, "remove-motion-keyframe", "delete-keyframe");
    b.icon(transport, "timeline-menu", "generalmoreVertical",
           [vm = vm_] { vm->option_tab.set(2); });
    ruler_ = take(b.controls.event_listener(
        root, "timeline-ruler", {Metric::size1361, ruler_height},
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
    // A bounded pool of independent retained rows lets one changed layer
    // update its own drawing without reconstructing every visible layer.
    const auto row_count = std::size_t(std::ceil(Metric::timelineBodyHeight / row_height)) + 2;
    row_slots_.reserve(row_count);
    slot_rows_.assign(row_count, std::numeric_limits<std::size_t>::max());
    for (std::size_t i = 0; i < row_count; ++i) {
        auto row = take(raw.absolute_stack(body_, "timeline/retained-row-" + std::to_string(i),
                                           {Metric::size1361, row_height}));
        good(row.set_visible(false));
        row_slots_.push_back(row);
    }
    playhead_ = take(raw.absolute_stack(body_, "timeline/body-playhead", {Metric::space2, Metric::timelineBodyHeight}));
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
                                 if (s->time_)
                                     good(s->time_->set_text(s->vm_->timeline->time_text()));
                                 const auto &state = s->vm_->timeline->state.get();
                                 const auto timing = std::pair{state.rate, state.duration};
                                 s->paint(!s->row_timing_ || *s->row_timing_ != timing);
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
    owned_widgets::good(rename_field_->set_text(rename_text_));
    owned_widgets::good(rename_.open_for(body_));
    owned_widgets::good(rename_field_->focus());
}
void TimelineView::commit_rename()
{
    if (rename_text_.empty())
        return;
    owned_widgets::good(rename_.close_popup());
    vm_->rename_layer(rename_key_, rename_text_);
}
void TimelineView::update_layer(Row &row, const iiSharedCanvas::Document &doc, const iiSharedCanvas::Layer &layer,
                                iiSharedCanvas::FrameIndex frame, bool force_pixels, bool update_keys)
{
    const auto &p = iiSharedCanvas::layerProperties(layer);
    row.id = p.id; row.name = p.name; row.opacity = p.opacity; row.visible = p.visible;
    row.mode = p.blendMode == RasterBlendMode::Multiply ? "Multiply" :
               p.blendMode == RasterBlendMode::Screen ? "Screen" : "Normal";
    const auto key = layer_preview_key(doc, layer, frame);
    row.content_frames = iiSharedCanvas::keyframedLayerSource(layer)
        ? p.frameRange
        : iiSharedCanvas::staticLayerTimelineRange(doc, layer);
    // A timed video clip occupies its active interval on the document timeline.
    if (std::holds_alternative<iiSharedCanvas::VideoLayer>(layer)) {
        const auto first = p.frameRange ? p.frameRange->firstFrame : 0;
        if (iiSharedCanvas::resolveAssetAt(doc, layer, first))
            row.content_frames = p.frameRange.value_or(
                iiSharedCanvas::LayerFrameRange{0, doc.timeline.frameCount - 1});
    }
    if (key != row.preview_key || force_pixels) {
        row.preview_key = key;
        row.preview = key ? render_layer_preview(doc, layer, frame) : nullptr;
    }
    if (update_keys) {
        row.keys.clear();
        for (const auto &f : doc.frames)
            if (std::ranges::any_of(f.keyframes, [&](const auto &k) { return k.layerId == row.id; }))
                row.keys.push_back(int(f.index));
    }
}
void TimelineView::refresh_document(const iiSharedCanvas::Document &doc,
                                    const std::string &selected)
{
    rows_.clear();
    selected_ = selected;
    const auto frame = static_cast<iiSharedCanvas::FrameIndex>(
        std::clamp(vm_->timeline->frame(), 0, std::max(0, int(doc.timeline.frameCount) - 1)));
    for (auto it = doc.layers.rbegin(); it != doc.layers.rend(); ++it)
    {
        auto &row = rows_.emplace_back();
        update_layer(row, doc, *it, frame, false, true);
    }
    for (auto it = doc.audioTracks.rbegin(); it != doc.audioTracks.rend(); ++it)
        rows_.push_back({it->id, it->name, "audio-track", "Audio", 1, !it->muted, {}, true});
    auto options = ii::gui::ScrollOptions{
        .height = Metric::timelineBodyHeight,
        .virtual_content_height = std::max<double>(
            Metric::timelineBodyHeight, double(rows_.size() * row_height))};
    owned_widgets::good(scroll_.set_scroll_options(options));
    paint();
}
void TimelineView::apply_document_change(const iiSharedCanvas::Document &doc,
                                         const std::string &selected,
                                         const DocumentChange &change)
{
    if (change.kind == DocumentChangeKind::reset) {
        refresh_document(doc, selected);
        return;
    }
    if (change.kind == DocumentChangeKind::selection) {
        select_layer(selected);
        return;
    }
    const auto frame = static_cast<iiSharedCanvas::FrameIndex>(
        std::clamp(vm_->timeline->frame(), 0, std::max(0, int(doc.timeline.frameCount) - 1)));
    std::vector<std::size_t> dirty;
    const auto old_selected = selected_;
    selected_ = selected;
    const auto scope = change.impact;
    const auto contains = [](const auto& ids, const std::string& id) {
        return std::ranges::find(ids, id) != ids.end();
    };
    const auto pixels_changed = [&](const Row& row) {
        return scope && row.preview_key && (contains(scope->assets, row.preview_key->asset) ||
            std::ranges::find(scope->pixels, row.preview_key->asset, &iiSharedCanvas::IiscRasterChange::asset) != scope->pixels.end());
    };
    if (change.kind == DocumentChangeKind::structure) {
        const auto previous = rows_;
        std::unordered_map<std::string, Row> retained;
        retained.reserve(rows_.size());
        for (auto &row : rows_) retained.emplace(row.id, std::move(row));
        rows_.clear(); rows_.reserve(doc.layers.size() + doc.audioTracks.size());
        for (auto it = doc.layers.rbegin(); it != doc.layers.rend(); ++it) {
            const auto &id = iiSharedCanvas::layerProperties(*it).id;
            auto old = retained.find(id);
            const bool added = old == retained.end() || old->second.audio;
            Row row = added ? Row{} : std::move(old->second);
            const bool modified = added || !scope || contains(scope->layers, id) ||
                (change.timing && std::holds_alternative<iiSharedCanvas::VideoLayer>(*it));
            const bool pixels = pixels_changed(row);
            if (modified || pixels) update_layer(row, doc, *it, frame, pixels, modified);
            rows_.push_back(std::move(row));
        }
        for (auto it = doc.audioTracks.rbegin(); it != doc.audioTracks.rend(); ++it) {
            auto old = retained.find(it->id);
            if (old != retained.end() && old->second.audio && scope && !contains(scope->audioTracks, it->id))
                rows_.push_back(std::move(old->second));
            else
                rows_.push_back(Row{it->id, it->name, "audio-track", "Audio", 1, !it->muted, {}, true});
        }
        if (previous.size() != rows_.size()) {
            auto options = ii::gui::ScrollOptions{.height = Metric::timelineBodyHeight,
                .virtual_content_height = std::max<double>(Metric::timelineBodyHeight,
                                                             double(rows_.size() * row_height))};
            owned_widgets::good(scroll_.set_scroll_options(options));
        }
        for (std::size_t i = 0; i < std::max(previous.size(), rows_.size()); ++i)
            if (i >= previous.size() || i >= rows_.size() || previous[i] != rows_[i]) dirty.push_back(i);
    } else {
        // An unknown asset must conservatively invalidate all matching content.
        std::string asset = change.asset;
        if (change.kind == DocumentChangeKind::pixels && asset.empty() && !change.layer.empty()) {
            const auto* item = iiSharedCanvas::findLayer(doc, change.layer);
            if (item) if (const auto* value = iiSharedCanvas::resolveAssetAt(doc, *item, change.frame))
                asset = iiSharedCanvas::assetId(*value);
        }
        for (std::size_t index = 0; index < rows_.size(); ++index) {
            auto &row = rows_[index];
            if (row.audio) {
                if ((change.kind == DocumentChangeKind::name || change.kind == DocumentChangeKind::properties) &&
                    (change.layer.empty() || change.layer == row.id)) {
                    if (const auto* track = iiSharedCanvas::findAudioTrack(doc, row.id)) {
                        if (row.name != track->name || row.visible == track->muted) dirty.push_back(index);
                        row.name = track->name; row.visible = !track->muted;
                    }
                }
                continue;
            }
            const bool targeted = change.layer.empty() || row.id == change.layer;
            const bool shared = !asset.empty() && row.preview_key && row.preview_key->asset == asset;
            const auto* item = iiSharedCanvas::findLayer(doc, row.id);
            if (!item) continue;
            const bool check = scope
                ? (contains(scope->layers, row.id) || pixels_changed(row) ||
                   (change.timing && std::holds_alternative<iiSharedCanvas::VideoLayer>(*item)))
                : change.kind == DocumentChangeKind::frames ||
                ((change.kind == DocumentChangeKind::name || change.kind == DocumentChangeKind::properties) && targeted) ||
                (change.kind == DocumentChangeKind::pixels && (targeted || shared || (change.layer.empty() && asset.empty())));
            if (!check) continue;
            {
                const auto previous = row;
                const auto current = layer_preview_key(doc, *item, frame);
                const bool force = scope ? pixels_changed(row) : change.kind == DocumentChangeKind::pixels &&
                    ((asset.empty() && targeted) || (current && current->asset == asset));
                update_layer(row, doc, *item, frame, force, scope ? contains(scope->layers, row.id) :
                             change.kind == DocumentChangeKind::frames ||
                             (change.kind == DocumentChangeKind::pixels && targeted));
                if (previous != row) dirty.push_back(index);
            }
        }
    }
    if (old_selected != selected_) {
        for (std::size_t i = 0; i < rows_.size(); ++i)
            if (rows_[i].id == old_selected || rows_[i].id == selected_) dirty.push_back(i);
    }
    std::ranges::sort(dirty);
    dirty.erase(std::unique(dirty.begin(), dirty.end()), dirty.end());
    paint_rows(dirty);
}
void TimelineView::refresh_frame(const iiSharedCanvas::Document &doc, int requested)
{
    const auto frame = static_cast<iiSharedCanvas::FrameIndex>(
        std::clamp(requested, 0, std::max(0, int(doc.timeline.frameCount) - 1)));
    std::vector<std::size_t> dirty;
    std::size_t i = 0;
    for (auto layer = doc.layers.rbegin(); layer != doc.layers.rend(); ++layer, ++i) {
        // Document rows follow the same top-to-bottom order. If a caller
        // missed a structural change, recover rather than using stale rows.
        if (i >= rows_.size() || rows_[i].audio ||
            rows_[i].id != iiSharedCanvas::layerProperties(*layer).id) {
            continue;
        }
        auto &row = rows_[i];
        const auto key = layer_preview_key(doc, *layer, frame);
        if (key == row.preview_key) continue;
        row.preview_key = key;
        row.preview = key ? render_layer_preview(doc, *layer, frame) : nullptr;
        dirty.push_back(i);
    }
    if (i < rows_.size() && !rows_[i].audio) { refresh_document(doc, selected_); return; }
    paint_rows(dirty);
}
void TimelineView::select_layer(const std::string &selected)
{
    if (selected_ == selected) return;
    const auto old = std::move(selected_);
    selected_ = selected;
    std::vector<std::size_t> dirty;
    for (std::size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == old || rows_[i].id == selected_) dirty.push_back(i);
    paint_rows(dirty);
}
void TimelineView::paint(bool update_rows)
{
    using namespace owned_widgets;
    const auto rs = ruler_.snapshot(), bs = body_.snapshot();
    if (!rs || !bs)
        return;
    const auto state = vm_->timeline->state.get();
    row_timing_ = std::pair{state.rate, state.duration};
    const double width = rs->bounds.width;
    const TimelineGrid grid(state, width, zoom_);
    const auto blue = Color::blue;
    ii::gui::WidgetDrawing header;
    image(header, {0, Metric::size22, lvui::design_icon_default_extent,
                   lvui::design_icon_default_extent}, "inspectionsEye");
    text(header, {Metric::space24, Metric::space16, Metric::size185, Metric::size22},
         "Layer / Track", Metric::textDescription, Metric::weightMedium);
    image(header, {Metric::size216, Metric::size22, lvui::design_icon_default_extent,
                   lvui::design_icon_default_extent}, "layer-mask");
    image(header, {Metric::size240, Metric::size22, lvui::design_icon_default_extent,
                   lvui::design_icon_default_extent}, "lock");
    text(header, {Metric::size264, Metric::space16, Metric::size90, Metric::size22}, "Mode");
    text(header, {Metric::size360, Metric::space16, Metric::size56, Metric::size22}, "Opacity");
    const auto step = grid.stride(grid.visible_frames);
    const int label_interval = int(std::ceil((Metric::size44 + Metric::space8) / (grid.frame_width * step)));
    for (std::int64_t frame = 0; frame <= grid.visible_frames; frame += step)
    {
        const auto x = grid.x(frame);
        const bool major = (frame / step) % label_interval == 0;
        line(header,
             {float(x), float(major ? Metric::size18 : Metric::size26),
              Metric::stroke1, float(major ? Metric::size26 : Metric::space10)},
             Color::textCaption);
        if (major && x + Metric::size44 <= width) {
            const auto seconds = std::round(1000. * frame / grid.rate) / 1000.;
            text(header, {float(x), 0, Metric::size44, Metric::size18}, ii::core::format_decimal(seconds) + "s");
        }
    }
    const auto play_x = grid.x(state.frame);
    header.images.push_back(
        {{float(play_x - Metric::size22 / 2.0), Metric::size18, Metric::size22, Metric::size26},
         marker_});
    good(ruler_.set_drawing(std::move(header)));
    if (update_rows) {
        std::vector<bool> visible(row_slots_.size(), false);
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const auto y = double(i) * row_height - offset_;
            if (y + row_height <= 0 || y >= bs->bounds.height) continue;
            visible[i % row_slots_.size()] = true;
            paint_row(i);
        }
        for (std::size_t slot = 0; slot < row_slots_.size(); ++slot) {
            if (!visible[slot]) {
                good(row_slots_[slot].set_visible(false));
                slot_rows_[slot] = std::numeric_limits<std::size_t>::max();
            }
        }
    }
    good(playhead_.set_frame({float(play_x - Metric::stroke1), 0, Metric::space2, bs->bounds.height}));
    ii::gui::WidgetDrawing playhead;
    line(playhead, {0, 0, Metric::space2, bs->bounds.height}, blue);
    good(playhead_.set_drawing(std::move(playhead)));
}
void TimelineView::paint_rows(const std::vector<std::size_t> &changed)
{
    for (const auto index : changed) paint_row(index);
}
void TimelineView::paint_row(std::size_t i)
{
    using namespace owned_widgets;
    if (row_slots_.empty()) return;
    const auto slot = i % row_slots_.size();
    if (i >= rows_.size()) {
        if (slot_rows_[slot] == i) {
            good(row_slots_[slot].set_visible(false));
            slot_rows_[slot] = std::numeric_limits<std::size_t>::max();
        }
        return;
    }
    const auto rs = ruler_.snapshot(), bs = body_.snapshot();
    if (!rs || !bs) return;
    const auto position = double(i) * row_height - offset_;
    if (position + row_height <= 0 || position >= bs->bounds.height) return;
    const auto state = vm_->timeline->state.get();
    const TimelineGrid grid(state, rs->bounds.width, zoom_);
    const auto document_frames = std::round(state.duration * grid.rate);
    const double y = 0;
    good(row_slots_[slot].set_frame({0, float(position), bs->bounds.width, float(row_height)}));
    good(row_slots_[slot].set_visible(true));
    slot_rows_[slot] = i;
    ii::gui::WidgetDrawing body;
    const auto &r = rows_[i];
    if (r.id == selected_)
        line(body, {Metric::space24, float(y), Metric::size190, Metric::size22}, Color::blueOverlay);
    image(body, {0, float(y + Metric::space2), lvui::design_icon_default_extent,
                 lvui::design_icon_default_extent}, r.visible ? "inspectionsEye" : "lock");
    const Rect preview_bounds{Metric::size32, float(y + Metric::space2),
                              lvui::design_icon_default_extent, lvui::design_icon_default_extent};
    if (r.audio) image(body, preview_bounds, r.icon);
    else {
        const auto background = editor::linear(Color::surfaceControl);
        body.panels.push_back({preview_bounds, background, background});
        if (r.preview) body.images.push_back({preview_bounds, r.preview});
    }
    text(body, {Metric::size52, float(y + body_text_inset), Metric::size160,
                Metric::textBodyLineHeight}, r.name, Metric::textBody, Metric::weightMedium);
    if (!r.audio)
        image(body, {Metric::size216, float(y + Metric::space2), lvui::design_icon_default_extent,
                     lvui::design_icon_default_extent}, "layer-mask");
    const auto control = editor::linear(Color::surfaceControl);
    body.panels.push_back({{Metric::size264, float(y + Metric::stroke1), Metric::size94, Metric::size20},
                           control, control, Metric::radius6});
    if (!r.audio)
        body.panels.push_back({{Metric::size360, float(y + Metric::stroke1), Metric::size56, Metric::size20},
                               control, control, Metric::radius6});
    text(body, {Metric::size272, float(y + body_text_inset), Metric::size84, Metric::textBodyLineHeight},
         r.mode, Metric::textBody, Metric::weightMedium);
    if (!r.audio)
        text(body, {Metric::size366, float(y + body_text_inset), Metric::size48, Metric::textBodyLineHeight},
             ii::core::format_decimal(std::round(r.opacity * 100)) + "%", Metric::textBody,
             Metric::weightMedium);
    if (r.content_frames) {
        const double first = r.content_frames->firstFrame;
        const auto one_past_last = double(r.content_frames->lastFrame) + 1;
        const auto end = std::min({one_past_last, document_frames, grid.visible_frames});
        if (first < end)
            line(body, {float(grid.x(first)), float(y), float(grid.x(end) - grid.x(first)), Metric::size20},
                 Color::surfaceControl);
    }
    constexpr double audio_divisions_per_second = 4;
    const auto division = r.audio ? grid.rate / audio_divisions_per_second : 1.;
    const auto visible_divisions = grid.visible_frames / division;
    const auto step = grid.stride(visible_divisions);
    for (std::int64_t i = 0; i < visible_divisions; i += step) {
        const auto x = grid.x(i * division);
        const auto w = grid.x(std::min(grid.visible_frames, (i + step) * division)) - x;
        line(body, {float(x + Metric::stroke1), float(y + Metric::size20),
                    float(std::max(0., w - Metric::space2)), Metric::stroke1});
        if ((i / step) % 4 == 0 && (r.audio || i < document_frames))
            text(body, {float(x + Metric::size3), float(y + Metric::space6), Metric::size30,
                        Metric::space12},
                 r.audio ? ii::core::format_decimal(i / audio_divisions_per_second) + "s"
                         : std::to_string(i + 1));
    }
    for (int key : r.keys) {
        if (key >= grid.visible_frames) continue;
        const auto x = grid.x(key + .5);
        constexpr auto icon_extent = lvui::design_icon_default_extent;
        image(body, {float(x - icon_extent / 2), float(y + (row_height - icon_extent) / 2),
                     icon_extent, icon_extent}, "keyframe");
    }
    good(row_slots_[slot].set_drawing(std::move(body)));
}
ii::core::Result<bool> TimelineView::input(const ii::gui::WidgetInput &e, bool ruler)
{
    using namespace owned_widgets;
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
            down_rate_ = int(std::lround(vm_->timeline->state.get().rate));
        }
        else if (!ruler)
        {
            const auto index =
                std::size_t(std::max(0., std::floor((y + offset_) / row_height)));
            if (index >= rows_.size())
                return false;
            const auto row = rows_[index];
            if (x < Metric::size22)
                vm_->check_layer(row.id, !row.visible);
            else if (row.audio && x >= Metric::size264)
                return true;
            else if (x >= Metric::size360)
            {
                if (auto callback = vm_->layer_opacity_changed)
                    callback(row.id, row.opacity <= .1 ? 1 : row.opacity - .1);
            }
            else if (x >= Metric::size264)
            {
                if (auto callback = vm_->layer_mode_changed)
                    callback(row.id);
            }
            else
            {
                if (!row.audio) drag_layer_ = row.id;
                drag_start_y_ = e.y;
                vm_->select_layer(row.id);
            }
            return true;
        }
    }
    if (!drag_layer_.empty() && e.kind == K::pointer_up)
    {
        const auto from = std::ranges::find(rows_, drag_layer_, &Row::id);
        auto target = std::size_t(std::clamp(std::floor((y + offset_) / row_height),
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
            if (e.kind == K::pointer_up)
                if (auto callback = vm_->frame_rate_changed) callback(rate);
        }
        else {
            const auto ruler_bounds = ruler ? bounds : take(ruler_.snapshot()).bounds;
            const TimelineGrid grid(vm_->timeline->state.get(), ruler_bounds.width, zoom_);
            vm_->timeline->scrub(grid.time_at(e.x - ruler_bounds.x));
        }
        if (e.kind == K::pointer_up)
            dragging_ = false;
        return true;
    }
    return false;
}
} // namespace congregation::native
