#include "WorkspaceView.h"
#include "EditorLayout.h"
#include "Style.h"
#include "ToolPanel.h"
#include "InspectorSegments.h"
#include "SocietyAssets.h"
#include "OwnedWidgetStyle.h"
#include <LVUI/NativeColorSurface.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iiSDKCore/Core/Text/Number.h>
#include <tuple>

namespace congregation::native
{
namespace
{
using namespace owned_widgets;
using A = congregation::ui::Action;
void ruler(Widget widget, bool vertical)
{
    good(widget.set_layout_handler(
        [widget, vertical](ii::gui::Rect r)
        {
            ii::gui::WidgetDrawing d;
            const auto color = editor::linear(editor::detail_line_color());
            const auto inkColor = editor::linear(ink);
            const auto length = vertical ? r.height : r.width;
            for (int i = 0; i <= length / Metric::rulerTickStep; ++i)
            {
                if (vertical)
                {
                    d.panels.push_back({{Metric::size27, float(i * Metric::rulerTickStep),
                                         Metric::stroke1, Metric::rulerTickStep},
                                        color,
                                        color});
                    d.texts.push_back(
                        {{Metric::space2, float(i * Metric::rulerTickStep + Metric::space4),
                          Metric::size25, Metric::space12},
                         std::to_string(i * Metric::rulerDocumentStep),
                         inkColor,
                         Metric::textCaption});
                }
                else
                {
                    d.panels.push_back(
                        {{float(i * Metric::rulerTickStep), r.height - Metric::stroke1,
                          Metric::stroke1, Metric::stroke1},
                         color,
                         color});
                    d.texts.push_back({{float(i * Metric::rulerTickStep + Metric::space12),
                                        Metric::size3_5, Metric::size55, Metric::textCaptionLineHeight},
                                       std::to_string(i * Metric::rulerDocumentStep),
                                       inkColor,
                                       Metric::textCaption});
                }
            }
            good(widget.set_drawing(std::move(d)));
        }));
}
void command(std::shared_ptr<congregation::ui::EditorBackend> vm, std::string name)
{
    if (auto callback = vm->command)
        callback(std::move(name));
}
void paint_alpha_rail(Widget widget, const std::shared_ptr<congregation::ui::EditorColor> &model)
{
    const auto snapshot = widget.snapshot();
    if (!snapshot)
        return;
    const auto color = model->current();
    const auto height = snapshot->bounds.height;
    ii::gui::WidgetDrawing drawing;
    for (int y = 0; y < int(height); ++y)
    {
        auto pixel = color;
        pixel.alpha = 1 - double(y) / std::max(1., double(height) - 1);
        const auto linear = editor::linear(pixel);
        drawing.panels.push_back({{0, float(y), Metric::size22, Metric::stroke1}, linear, linear});
    }
    const auto white = editor::linear(Color::textTitleHeader);
    drawing.panels.push_back(
        {{Metric::space2, float((1 - color.alpha) * std::max(0., double(height) - Metric::size3)),
          Metric::size18, Metric::size3},
         white,
         white,
         Metric::size3 / 2.0f});
    good(widget.set_drawing(std::move(drawing)));
}
} // namespace
WorkspaceView::WorkspaceView(std::shared_ptr<congregation::ui::EditorBackend> vm,
                             std::filesystem::path assets)
    : vm_(std::move(vm)), assets_(std::move(assets))
{
}
void WorkspaceView::build(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace owned_widgets;
    Builder b(raw);
    good(raw.retain(bindings_));
    const auto root = b.column(parent, "workspace");
    good(root.set_flex(1));
    good(ii::gui::bind_visible(*bindings_, root, vm_->workspace_visible));
    const auto body = take(b.controls.hstack(
        root, "work-area",
        {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
    good(body.set_flex(1));
    const auto center = b.column(body, "canvas-and-timeline");
    good(center.set_flex(1));
    canvas(raw, center);
    timeline_ = std::make_shared<TimelineView>(vm_, assets_);
    timeline_->build(raw, center);
    good(raw.retain(timeline_));
    inspector(raw, body);
}
void WorkspaceView::canvas(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace owned_widgets;
    Builder b(raw, "canvas/");
    auto root = b.column(parent, "canvas-document");
    borders(root, false, true);
    good(root.set_flex(1));
    constexpr auto center_width =
        Metric::windowWidth - editor::tool_rail_width - Metric::inspectorWidth;
    auto horizontal = take(b.controls.event_listener(
        root, "horizontal-ruler", {center_width, Metric::size18}));
    ruler(horizontal, false);
    good(ii::gui::bind_visible(*bindings_, horizontal, vm_->rulers_visible));
    auto body = take(b.controls.hstack(
        root, "canvas-surface",
        {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
    good(body.set_flex(1));
    auto vertical =
        take(b.controls.event_listener(body, "vertical-ruler", {Metric::size28, Metric::size690}));
    ruler(vertical, true);
    good(ii::gui::bind_visible(*bindings_, vertical, vm_->rulers_visible));
    auto canvas = take(b.controls.event_listener(
        body, "canvas", {center_width - Metric::size28, Metric::size692}, [vm = vm_](const auto &e)
        { return vm->canvas_input(e); }, [vm = vm_](const auto &r) { vm->canvas_resize(r); }));
    good(canvas.set_flex(1));
    auto paint_layers = take(raw.content_host(canvas, "canvas-paint-layers",
                                              {center_width - Metric::size28, Metric::size692}));
    good(paint_layers.set_frame({0, 0, center_width - Metric::size28, Metric::size692}));
    good(paint_layers.set_content_input_passthrough(true));
    auto foot = b.row(root, "canvas-footer", 0, Metric::size26, Metric::space4);
    b.icon(foot, "zoom-out", "imagezoomOut", [vm = vm_] { vm->zoom_out(); });
    auto zoom = take(b.controls.combo_box(foot, "canvas-zoom",
                                          {.text = vm_->zoom.get(),
                                           .tone = ii::gui::ButtonTone::borderless,
                                           .arrow = ii::gui::StepperArrow::down,
                                           .width = Metric::size97},
                                          {.clicked = [vm = vm_] { vm->fit(); }}));
    good(bind_text(*bindings_, zoom, vm_->zoom));
    b.icon(foot, "zoom-in", "imagezoomIn", [vm = vm_] { vm->zoom_in(); });
    b.icon(foot, "canvas-rotate", "rotate-view", [vm = vm_] { command(vm, "reset-view"); });
    take(b.controls.input_field(foot, "canvas-angle",
                                {.text = "0.0°",
                                 .read_only = true,
                                 .width = Metric::size56}));
    b.icon(foot, "canvas-flip", "flip-horizontal", [vm = vm_] { command(vm, "flip-horizontal"); });
    b.space(foot);
    auto info = b.label(foot, vm_->document_info.get(), Metric::textCaption);
    good(ii::gui::bind_text(*bindings_, info, vm_->document_info));
}
void WorkspaceView::inspector(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace owned_widgets;
    Builder b(raw, "inspector/");
    auto root = b.column(parent, "inspector", Metric::inspectorWidth, 0);
    const auto weak = weak_from_this();
    auto options = b.column(root, "options", 0, Metric::size88, Metric::space2, Metric::space4);
    borders(options, false, false, true);
    auto heading = b.row(options);
    b.glyph(heading, "settings");
    b.label(heading, "Options", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(heading);
    auto segments = take(b.controls.icon_segmented_control(options, "option-segments", false));
    for (std::size_t i = 0; i < inspector_segments.size(); ++i)
    {
        auto button = b.icon(segments, "option-" + std::to_string(i), std::string(inspector_segments[i].icon),
            [vm = vm_, i] { vm->option_tab.set(static_cast<int>(i)); });
        good(button.set_accessibility_name(std::string(inspector_segments[i].name)));
        good(button.set_segment_selected(i == 0));
        option_buttons_.push_back(button);
    }
    auto current = b.row(options);
    good(bindings_->bind(current, vm_->option_tab, [](Widget row, int index) {
        return row.set_visible(index == 0);
    }));
    b.glyph(current, "brush");
    auto label = b.label(current, "Brush · Textured oil");
    const auto updateTool = [vm = vm_](Widget widget, const std::string &) -> ii::core::Result<void>
    {
        auto name = vm->selected_tool.get();
        std::ranges::replace(name, '-', ' ');
        if (!name.empty() && name[0] >= 'a' && name[0] <= 'z')
            name[0] -= 'a' - 'A';
        const auto preset = vm->preset.get();
        return widget.set_text(name + " · " + preset.substr(0, preset.find(" · ")));
    };
    good(bindings_->bind(label, vm_->selected_tool, updateTool));
    good(bindings_->bind(label, vm_->preset, updateTool));
    auto tool = b.column(root, "tool-detail");
    good(tool.set_flex(1));
    option_panels_.push_back(tool);
    auto color =
        b.column(tool, "color-picker-panel", 0, Metric::size265, Metric::space8, Metric::space8);
    borders(color, false, false, true);
    auto divider = b.column(color, "color-divider", 0, Metric::stroke1);
    editor::panel(divider, editor::surface_panel);
    auto colorTitle = b.row(color, {}, 0, Metric::size20);
    b.label(colorTitle, "Color picker");
    b.space(colorTitle);
    take(b.controls.combo_box(
        colorTitle, "color-space",
        {.text = "HSB", .arrow = ii::gui::StepperArrow::up_down, .width = Metric::size60}));
    auto fields = b.row(color, "color-fields", 0, Metric::size156, Metric::space8);
    const auto color_surface = [&](std::string_view id, lvui::ColorSurfaceKind kind,
                                   double width, double height) {
        lvui::native::ColorSurfaceInput input;
        input.source = vm_->color_source.get();
        input.configuration.kind = kind;
        input.configuration.width = width;
        input.configuration.height = height;
        auto surface = take(lvui_host::ColorSurface::create(
            raw, fields, id, input, [vm = vm_](ii::gui::WidgetColor value) { vm->color_edited(value); }));
        const auto update = [vm = vm_, surface, input]() {
            auto next = input;
            next.source = vm->color_source.get();
            next.source_revision = vm->color_revision.get();
            return surface.set_options(next);
        };
        good(bindings_->bind(surface.root(), vm_->color_revision,
                             [update](Widget, const auto&) { return update(); }));
        good(bindings_->bind(surface.root(), vm_->color_source,
                             [update](Widget, const auto&) { return update(); }));
    };
    color_surface("color-saturation-brightness", lvui::ColorSurfaceKind::saturation_brightness,
                  Metric::size276, Metric::colorFieldHeight);
    color_surface("color-hue", lvui::ColorSurfaceKind::hue_rail, Metric::size22, Metric::size156);
    auto alphaRail = take(
        b.controls.event_listener(fields, "color-transparency", {Metric::size22, Metric::size156}));
    good(alphaRail.set_layout_handler([alphaRail, model = vm_->color](ii::gui::Rect)
                                      { paint_alpha_rail(alphaRail, model); }));
    good(alphaRail.set_input_handler(
        [alphaRail, model = vm_->color](const ii::gui::WidgetInput &event) -> ii::core::Result<bool>
        {
            using K = ii::gui::WidgetInputKind;
            if (event.kind == K::pointer_down || event.kind == K::pointer_move ||
                event.kind == K::pointer_up)
            {
                if (event.kind == K::pointer_move && !event.buttons)
                    return false;
                const auto rect = take(alphaRail.snapshot()).bounds;
                if (rect.height <= 0) return false;
                good(model->set_alpha(std::clamp(1 - (event.y - rect.y) / rect.height, 0., 1.)));
                if (event.kind == K::pointer_down)
                    good(alphaRail.focus());
                return true;
            }
            if (event.kind == K::key_down &&
                (event.key == ii::gui::WidgetKey::up || event.key == ii::gui::WidgetKey::down))
            {
                good(model->set_alpha(std::clamp(model->current().alpha +
                                   (event.key == ii::gui::WidgetKey::up ? .01 : -.01), 0., 1.)));
                return true;
            }
            return false;
        }));
    auto opacityGroup = b.column(color, {}, 0, Metric::size48, Metric::space4);
    auto opacityRow = b.row(opacityGroup);
    b.label(opacityRow, "Opacity");
    b.space(opacityRow);
    auto alphaValue =
        take(b.controls.input_field(opacityRow, "color-alpha-value",
                                    {.text = "100%",
                                     .read_only = true,
                                     .width = Metric::size106}));
    auto alpha = take(b.controls.slider(
        opacityGroup, "color-alpha",
        {.value = 1,
         .width = Metric::inspectorWidth - 2 * Metric::space8,
         .accessibility_name = "Color opacity",
         .size = lvui::SliderSize::mini},
        [vm = vm_](double value)
        { good(vm->color->set_alpha(value)); }));
    good(bindings_->bind(alphaValue, vm_->current_color,
        [model = vm_->color, alphaValue, alpha, alphaRail](Widget, const ii::gui::WidgetColor &color)
        {
            good(alphaValue.set_text(ii::core::format_decimal(std::round(color.alpha * 100)) +
                                     "%"));
            good(alpha.set_value(color.alpha));
            paint_alpha_rail(alphaRail, model);
            return ii::core::Result<void>{};
        }));
    (void)build_tool_panel(raw, tool, vm_);
    auto generative = b.column(root, "generative-detail", 0, 0, Metric::space12, Metric::space12);
    good(generative.set_flex(1));
    option_panels_.push_back(generative);
    b.label(generative, "Generative", Metric::textHeader, {}, Metric::weightSemiBold);
    b.label(generative, "No generative tools available", Metric::textDescription);
    auto document = b.column(root, "document-detail", 0, 0, Metric::space12, Metric::space12);
    good(document.set_flex(1));
    option_panels_.push_back(document);
    b.label(document, "Document", Metric::textHeader, {}, Metric::weightSemiBold);
    auto info = b.label(document, vm_->document_info.get());
    good(ii::gui::bind_text(*bindings_, info, vm_->document_info));
    b.label(document, "Timeline", Metric::textDescription, {}, Metric::weightSemiBold);
    auto timing = b.label(document, vm_->timeline->time_text());
    good(bindings_->bind(timing, vm_->timeline->state,
                         [timeline = vm_->timeline](Widget w, const auto &)
                         {
                             return w.set_text(
                                 timeline->time_text() + " / " +
                                 TimelineModel::format_time(timeline->state.get().duration));
                         }));
    b.label(document, "Drag the timeline handle to change time.", Metric::textDescription);
    b.label(document, "Alt-drag its ticks vertically to change rate.", Metric::textDescription);
    b.button(document, "new", "New canvas…", [vm = vm_] { vm->new_document(); });
    b.button(document, "export-document", "Export…", [vm = vm_] { vm->export_document(); });
    auto view = b.column(document, "view-detail", 0, 0, Metric::space12, Metric::space0);
    b.label(view, "View", Metric::textHeader, {}, Metric::weightSemiBold);
    b.button(view, "fit-canvas", "Fit canvas", [vm = vm_] { vm->fit(); });
    auto grid = take(b.controls.check_box(view, "show-grid",
                                          {.text = "Grid", .checked = vm_->grid_visible.get()},
                                          [vm = vm_](bool v)
                                          {
                                              vm->grid_visible.set(v);
                                              command(vm, "update-view");
                                          }));
    good(bind_checked(*bindings_, grid, vm_->grid_visible));
    auto rulers = take(b.controls.check_box(
        view, "show-rulers", {.text = "Rulers", .checked = vm_->rulers_visible.get()},
        [vm = vm_](bool v) { vm->rulers_visible.set(v); }));
    good(bind_checked(*bindings_, rulers, vm_->rulers_visible));
    b.button(view, "present-canvas", "Presentation", [vm = vm_] { vm->presentation(); });
    b.button(document, "more-preferences", "Preferences…", [vm = vm_] { vm->preferences(); });
    b.button(document, "more-save", "Save", [vm = vm_] { vm->save(); });
    auto assets = b.column(root, "asset-detail", 0, 0, Metric::space12, Metric::space12);
    good(assets.set_flex(1));
    option_panels_.push_back(assets);
    b.label(assets, "Asset Library", Metric::textHeader, {}, Metric::weightSemiBold);
    b.label(assets, "Society Storage / Asset Library", Metric::textDescription);
    auto list_header = b.row(assets, "asset-list-header");
    b.label(list_header, "Assets", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(list_header);
    b.button(list_header, "refresh-assets", "Refresh", [weak] {
        if (auto s = weak.lock()) s->refresh_assets();
    });
    auto asset_scroll = take(raw.scroll_view(assets, "asset-list-scroll",
        {.width = Metric::inspectorWidth - 2 * Metric::space12, .height = Metric::size288}));
    asset_results_ = take(raw.content_host(asset_scroll, "society-asset-results",
        {Metric::inspectorWidth - 2 * Metric::space12, Metric::size288}));
    good(bindings_->bind(assets, vm_->asset_root, [weak](Widget, const std::string&) -> ii::core::Result<void> {
        if (auto s = weak.lock(); s && s->vm_->option_tab.get() == 3) s->refresh_assets();
        return {};
    }));
    b.button(assets, "open-assets", "Open Asset Library…",
             [vm = vm_] { vm->send(A::open_asset_library); });
    b.button(assets, "more-assets", "Choose Asset Library folder…",
             [vm = vm_] { vm->send(A::choose_asset_library); });
    good(bindings_->bind(
        root, vm_->option_tab,
        [weak](Widget, const int &index) -> ii::core::Result<void>
        {
            if (auto s = weak.lock()) {
                for (std::size_t i = 0; i < s->option_panels_.size(); ++i)
                {
                    good(s->option_panels_[i].set_visible(i == std::size_t(index)));
                    good(s->option_buttons_[i].set_segment_selected(i == std::size_t(index)));
                }
                if (index == 3) s->refresh_assets();
            }
            return {};
        }));
}
void WorkspaceView::refresh_assets()
{
    using namespace owned_widgets;
    const auto configured = vm_->asset_root.get();
    auto listing = society_assets(society_registry(), configured.empty() ? fs::path{} : fs::u8path(configured));
    const auto weak = weak_from_this();
    const auto row_count = listing.files.size();
    good(asset_results_.set_content([weak, listing = std::move(listing)](ii::gui::WidgetBuilder& raw) -> ii::core::Result<void> {
        try {
            Builder b(raw, "society-asset/");
            auto root = b.column({}, "society-asset-list", 0, 0, Metric::space4);
            if (!listing.error.empty()) b.label(root, listing.error, Metric::textDescription);
            else if (listing.files.empty()) b.label(root, "No assets in Society Asset Library", Metric::textDescription);
            for (std::size_t i = 0; i < listing.files.size(); ++i) {
                const auto path = listing.files[i];
                const auto name = path.lexically_relative(listing.root).string();
                auto ext = path.extension().string();
                std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                const bool image = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp";
                b.choice(root, "society-asset-" + std::to_string(i), name,
                         image ? "fileTypesimage" : "folder", [weak, path, image] {
                    if (auto s = weak.lock()) {
                        if (image && s->vm_->asset_inserted) s->vm_->asset_inserted(path);
                        else s->vm_->status.set("This asset type cannot be inserted as an image");
                    }
                });
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Cannot build Society asset list", std::current_exception());
        }
    }));
    good(asset_results_.set_extent({Metric::inspectorWidth - 2 * Metric::space12,
                                    std::max<double>(Metric::size288, row_count * Metric::size26)}));
}
void WorkspaceView::refresh_document(const iiSharedCanvas::Document &doc,
                                     const std::string &selected)
{
    if (timeline_)
        timeline_->refresh_document(doc, selected);
}
void WorkspaceView::select_layer(const std::string &selected)
{
    if (timeline_) timeline_->select_layer(selected);
}
} // namespace congregation::native
