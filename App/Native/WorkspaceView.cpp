#include "WorkspaceView.h"
#include "EditorLayout.h"
#include "Style.h"
#include "ToolPanel.h"
#include "WidgetStyle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iiSDKCore/Core/Text/Number.h>
#include <tuple>

namespace congregation::native
{
namespace
{
using namespace widgets;
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
                                        Metric::size3_5, Metric::size55, Metric::size11},
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
    if (vm->command)
        vm->command(std::move(name));
}
void paint_alpha_rail(Widget widget, const std::shared_ptr<ii::gui::ColorPickerModel> &model)
{
    const auto snapshot = widget.snapshot();
    if (!snapshot)
        return;
    const auto color = take(model->state()).color;
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
         Metric::radius1_5});
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
    using namespace widgets;
    Builder b(raw);
    good(raw.retain(bindings_));
    const auto root = b.column(parent, "workspace");
    good(root.set_flex(1));
    good(ii::gui::bind_visible(*bindings_, root, vm_->licensed));
    toolbar(raw, root);
    const auto body = take(b.controls.hstack(
        root, "work-area",
        {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
    good(body.set_flex(1));
    tools(raw, body);
    const auto center = b.column(body, "canvas-and-timeline");
    good(center.set_flex(1));
    canvas(raw, center);
    timeline_ = std::make_shared<TimelineView>(vm_, assets_);
    timeline_->build(raw, center);
    good(raw.retain(timeline_));
    inspector(raw, body);
    const auto status =
        b.row(root, "status-bar", 0, Metric::size20, Metric::space8, Metric::space4);
    borders(status, true);
    auto info = b.label(status, vm_->document_info.get(), Metric::textCaption, "document-status");
    good(ii::gui::bind_text(*bindings_, info, vm_->document_info));
    b.space(status);
    auto notice = b.label(status, vm_->status.get(), Metric::textCaption, "workspace-status");
    good(ii::gui::bind_text(*bindings_, notice, vm_->status));
    b.label(status, "Tool / Textured oil", Metric::textCaption);
    auto time = b.label(status, vm_->timeline->time_text(), Metric::textCaption);
    good(bindings_->bind(time, vm_->timeline->state,
                         [model = vm_->timeline](Widget w, const TimelinePosition &)
                         {
                             return w.set_text(
                                 model->time_text() + " / " +
                                 TimelineModel::format_time(model->state.get().duration));
                         }));
}
void WorkspaceView::toolbar(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    Builder b(raw, "global/");
    auto row = b.row(parent, "toolbar", 0, Metric::toolbarHeight, Metric::space4);
    good(row.set_stack_options({.spacing = Metric::space4,
                                .alignment = ii::gui::StackAlignment::center,
                                .insets = ii::gui::Insets{Metric::space4, Metric::space0,
                                                          Metric::space4, Metric::space0}}));
    borders(row, false, false, true);
    good(ii::gui::bind_visible(*bindings_, row, vm_->chrome_visible));
    for (const auto &[id, icon, action] :
         std::initializer_list<std::tuple<const char *, const char *, A>>{
             {"open", "generalopen", A::open},
             {"save", "generalsave", A::save},
             {"undo", "generalundo", A::undo},
             {"redo", "generalredo", A::redo}})
        b.icon(row, id, icon, [vm = vm_, action] { vm->send(action); });
    take(b.controls.combo_box(
        row, "transform-mode",
        {.text = "Free transform", .arrow = ii::gui::StepperArrow::down, .width = Metric::size148},
        {.clicked = [vm = vm_] { vm->select(); }}));
    for (const char *icon :
         {"align-left", "align-center", "align-right", "align-top", "align-middle", "align-bottom",
          "distribute-horizontal", "distribute-vertical", "flip-horizontal", "flip-vertical",
          "rotate-left", "rotate-right", "perspective-grid", "symmetry-paint", "grid", "rulers",
          "snap"})
        b.icon(row, "command-" + std::string(icon), icon, [vm = vm_, icon] { command(vm, icon); });
    b.space(row);
    take(b.controls.combo_box(row, "document-color-mode",
                              {.text = "sRGB / 8-bit",
                               .tone = ii::gui::ButtonTone::borderless,
                               .arrow = ii::gui::StepperArrow::down,
                               .width = Metric::size160},
                              {.clicked = [vm = vm_] { vm->option_tab.set(1); }}));
    b.icon(row, "brush-color", "color-picker", [vm = vm_] { vm->open_color(); });
    b.icon(row, "export", "generalexport", [vm = vm_] { vm->export_document(); });
    b.icon(row, "preferences", "settings", [vm = vm_] { vm->preferences(); });
}
void WorkspaceView::tools(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    Builder b(raw, "tools/");
    auto root =
        b.column(parent, "tool-rail", Metric::toolRailWidth, 0, Metric::space2, Metric::space4);
    borders(root, false, true);
    good(ii::gui::bind_visible(*bindings_, root, vm_->chrome_visible));
    const std::vector<std::vector<const char *>> groups = {
        {"move", "rect-marquee", "ellipse-marquee", "lasso", "polygon-lasso", "magnetic-lasso",
         "magic-wand", "crop", "perspective-crop", "color-picker"},
        {"brush", "pencil", "airbrush", "mixer-brush", "dry-brush", "bristle-brush", "eraser",
         "gradient", "paint-bucket", "smudge"},
        {"clone-stamp", "pattern-stamp", "healing-brush", "spot-heal", "blur", "sharpen", "dodge",
         "burn", "sponge", "liquify", "warp"},
        {"pen", "curvature-pen", "direct-select", "add-anchor", "delete-anchor", "rectangle",
         "ellipse", "polygon", "area-text", "path-text"},
        {"hand", "rotate-view"},
        {"color-picker", "quick-mask", "imagezoomIn", "rulers"}};
    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        Widget row;
        for (std::size_t i = 0; i < groups[g].size(); ++i)
        {
            if (i % 2 == 0)
                row = b.row(root, {}, Metric::size46, Metric::size22, Metric::space2);
            const std::string source = groups[g][i];
            std::string id = "tool-" + source;
            if (source == "brush" || source == "eraser")
                id = source;
            else if (source == "move")
                id = "select";
            else if (source == "hand")
                id = "pan";
            else if (source == "rectangle")
                id = "shape";
            else if (source == "paint-bucket")
                id = "fill";
            else if (source == "area-text")
                id = "text";
            else if (source == "imagezoomIn")
                id = "zoom-tool";
            if (g == 5 && i == 0)
                id = "foreground-color";
            if (g == 5 && i == 0)
            {
                take(b.controls.color_picker_button(
                    row, id,
                    {.model = vm_->color, .button_size = lvrs::ColorPickerButtonSize::small},
                    {.clicked = [vm = vm_] { vm->open_color(); }}));
                continue;
            }
            const auto button =
                b.icon(row, id, source, [vm = vm_, source] { vm->select_tool(source); });
            good(bindings_->bind(
                button, vm_->selected_tool,
                [source](Widget w, const std::string &selected)
                {
                    return w.set_button_options(
                        {.tone = source == selected ? ii::gui::ButtonTone::normal
                                                    : ii::gui::ButtonTone::borderless,
                         .icon = take(lvrs::Icons::image(source, Metric::iconRasterSize)),
                         .icon_name = source,
                         .accessibility_name = source,
                         .icon_size = Metric::size18,
                         .icon_mode = true,
                         .height = Metric::size22,
                         .horizontal_padding = Metric::space2});
                }));
        }
        if (g < 4)
        {
            auto divider = b.column(root, {}, Metric::size46, Metric::size3);
            borders(divider, false, false, true);
        }
    }
    b.space(root);
    auto bottom = b.row(root, {}, Metric::size46, Metric::size22, Metric::space2);
    b.icon(bottom, "tool-settings", "settings", [vm = vm_] { vm->preferences(); });
    b.icon(bottom, "tool-symmetry", "symmetry-paint",
           [vm = vm_] { command(vm, "symmetry-paint"); });
}
void WorkspaceView::canvas(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    Builder b(raw, "canvas/");
    auto root = b.column(parent, "canvas-document");
    borders(root, false, true);
    good(root.set_flex(1));
    constexpr auto center_width =
        Metric::windowWidth - Metric::toolRailWidth - Metric::inspectorWidth;
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
    auto foot = b.row(root, "canvas-footer", 0, Metric::size26, Metric::space4);
    b.icon(foot, "zoom-out", "imagezoomOut", [vm = vm_] { vm->zoom_out(); });
    auto zoom = take(b.controls.combo_box(foot, "canvas-zoom",
                                          {.text = vm_->zoom.get(),
                                           .tone = ii::gui::ButtonTone::borderless,
                                           .arrow = ii::gui::StepperArrow::down,
                                           .width = Metric::size97},
                                          {.clicked = [vm = vm_] { vm->fit(); }}));
    good(ii::gui::bind_text(*bindings_, zoom, vm_->zoom));
    b.icon(foot, "zoom-in", "imagezoomIn", [vm = vm_] { vm->zoom_in(); });
    b.icon(foot, "canvas-rotate", "rotate-view", [vm = vm_] { command(vm, "reset-view"); });
    take(b.controls.input_field(foot, "canvas-angle",
                                {.text = "0.0°",
                                 .read_only = true,
                                 .width = Metric::size56,
                                 .style = ii::gui::InputStyle{.height = Metric::size22}}));
    b.icon(foot, "canvas-flip", "flip-horizontal", [vm = vm_] { command(vm, "flip-horizontal"); });
    b.space(foot);
    auto info = b.label(foot, vm_->document_info.get(), Metric::textCaption);
    good(ii::gui::bind_text(*bindings_, info, vm_->document_info));
}
void WorkspaceView::inspector(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    Builder b(raw, "inspector/");
    auto root = b.column(parent, "inspector", Metric::inspectorWidth, 0);
    const auto weak = weak_from_this();
    auto options = b.column(root, "options", 0, Metric::size88, Metric::space2, Metric::space4);
    borders(options, false, false, true);
    auto heading = b.row(options);
    b.glyph(heading, "settings");
    b.label(heading, "Options", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(heading);
    b.icon(heading, "option-menu", "generalmoreVertical", [vm = vm_] { vm->option_tab.set(3); });
    auto segments = take(b.controls.label_segmented_control(options, "option-segments",
                                                            {.force_borderless_tone = false}));
    const char *names[]{"Tool", "Doc", "View", "More"};
    for (int i = 0; i < 4; ++i)
    {
        auto button = b.button(
            segments, "option-" + std::to_string(i), names[i],
            [vm = vm_, i] { vm->option_tab.set(i); }, 0,
            i == 0 ? ii::gui::ButtonTone::normal : ii::gui::ButtonTone::borderless);
        option_buttons_.push_back(button);
    }
    auto current = b.row(options);
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
    auto divider = b.column(color, "color-divider", 0, Metric::size1);
    editor::panel(divider, editor::surface_panel);
    auto colorTitle = b.row(color, {}, 0, Metric::size20);
    b.label(colorTitle, "Color picker");
    b.space(colorTitle);
    take(b.controls.combo_box(
        colorTitle, "color-space",
        {.text = "HSB", .arrow = ii::gui::StepperArrow::up_down, .width = Metric::size60}));
    auto fields = b.row(color, "color-fields", 0, Metric::size156, Metric::space8);
    take(b.controls.color_surface(fields, "color-saturation-brightness",
                                  {.model = vm_->color,
                                   .field = ii::gui::ColorField::saturation_brightness,
                                   .width = Metric::size276,
                                   .height = Metric::colorFieldHeight}));
    take(b.controls.color_surface(fields, "color-hue",
                                  {.model = vm_->color,
                                   .field = ii::gui::ColorField::hue_rail,
                                   .width = Metric::size22,
                                   .height = Metric::size156}));
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
                good(model->edit_channel(ii::gui::ColorChannel::alpha,
                                         std::clamp(1 - (event.y - rect.y) / rect.height, 0., 1.) *
                                             100));
                if (event.kind == K::pointer_down)
                    good(alphaRail.focus());
                return true;
            }
            if (event.kind == K::key_down &&
                (event.key == ii::gui::WidgetKey::up || event.key == ii::gui::WidgetKey::down))
            {
                good(model->edit_channel(
                    ii::gui::ColorChannel::alpha,
                    std::clamp(take(model->state()).color.alpha * 100 +
                                   (event.key == ii::gui::WidgetKey::up ? 1 : -1),
                               0., 100.)));
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
                                     .width = Metric::size106,
                                     .style = ii::gui::InputStyle{.height = Metric::size22}}));
    auto alpha = take(b.controls.slider(
        opacityGroup, "color-alpha",
        {.value = 1,
         .width = Metric::size336,
         .thumb_diameter = Metric::size12,
         .track_height = Metric::size4,
         .range_inset = Metric::space6,
         .accessibility_name = "Color opacity",
         .height = Metric::size22},
        [vm = vm_](double value)
        { good(vm->color->edit_channel(ii::gui::ColorChannel::alpha, value * 100)); }));
    color_port_ = std::make_shared<ii::gui::ColorChangePort>(
        [model = vm_->color, alphaValue, alpha, alphaRail](const auto &)
        {
            auto state = take(model->state());
            good(alphaValue.set_text(ii::core::format_decimal(std::round(state.color.alpha * 100)) +
                                     "%"));
            good(alpha.set_value(state.color.alpha));
            paint_alpha_rail(alphaRail, model);
        });
    good(vm_->color->observe(color_port_));
    (void)build_tool_panel(raw, tool, vm_);
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
    auto view = b.column(root, "view-detail", 0, 0, Metric::space12, Metric::space12);
    good(view.set_flex(1));
    option_panels_.push_back(view);
    b.label(view, "View", Metric::textHeader, {}, Metric::weightSemiBold);
    b.button(view, "fit-canvas", "Fit canvas", [vm = vm_] { vm->fit(); });
    auto grid = take(b.controls.check_box(view, "show-grid",
                                          {.text = "Grid", .checked = vm_->grid_visible.get()},
                                          [vm = vm_](bool v)
                                          {
                                              vm->grid_visible.set(v);
                                              command(vm, "update-view");
                                          }));
    good(ii::gui::bind_checked(*bindings_, grid, vm_->grid_visible));
    auto rulers = take(b.controls.check_box(
        view, "show-rulers", {.text = "Rulers", .checked = vm_->rulers_visible.get()},
        [vm = vm_](bool v) { vm->rulers_visible.set(v); }));
    good(ii::gui::bind_checked(*bindings_, rulers, vm_->rulers_visible));
    b.button(view, "present-canvas", "Presentation", [vm = vm_] { vm->presentation(); });
    auto more = b.column(root, "more-detail", 0, 0, Metric::space12, Metric::space12);
    good(more.set_flex(1));
    option_panels_.push_back(more);
    b.label(more, "More", Metric::textHeader, {}, Metric::weightSemiBold);
    b.button(more, "more-preferences", "Preferences…", [vm = vm_] { vm->preferences(); });
    b.button(more, "more-save", "Save", [vm = vm_] { vm->save(); });
    b.button(more, "more-assets", "Society Asset Library…",
             [vm = vm_] { vm->send(A::choose_asset_library); });
    good(bindings_->bind(
        root, vm_->option_tab,
        [weak](Widget, const int &index) -> ii::core::Result<void>
        {
            if (auto s = weak.lock())
                for (std::size_t i = 0; i < s->option_panels_.size(); ++i)
                {
                    good(s->option_panels_[i].set_visible(i == std::size_t(index)));
                    good(s->option_buttons_[i].set_button_options(
                        {.text = std::array<const char *, 4>{"Tool", "Doc", "View", "More"}[i],
                         .tone = i == std::size_t(index) ? ii::gui::ButtonTone::normal
                                                         : ii::gui::ButtonTone::borderless,
                         .height = Metric::size22}));
                }
            return {};
        }));
}
void WorkspaceView::refresh_document(const iiSharedCanvas::Document &doc,
                                     const std::string &selected)
{
    if (timeline_)
        timeline_->refresh_document(doc, selected);
}
} // namespace congregation::native
