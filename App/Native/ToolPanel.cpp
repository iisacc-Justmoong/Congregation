#include "ToolPanel.h"
#include "Style.h"
#include "ToolSettings.h"
#include "WidgetStyle.h"
#include <algorithm>

namespace congregation::native
{
ii::gui::Widget build_tool_panel(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent,
                                 std::shared_ptr<congregation::ui::EditorBackend> vm, bool expanded)
{
    using namespace widgets;
    using P = ToolParameter;
    Builder b(raw, "tool/");
    const auto root = b.column(parent, "tool-panel", 0, expanded ? Metric::toolExpandedHeight : 0,
                               Metric::space4, Metric::space4);
    if (!expanded)
        good(root.set_flex(1));
    borders(root, false, false, true);
    auto bindings = std::make_shared<ii::gui::BindingScope>();
    good(raw.retain(bindings));
    const auto head = b.column(root, "tool-heading", 0, Metric::toolHeadingHeight, Metric::space4,
                               Metric::space4);
    auto title = b.row(head);
    b.label(title, "Brush settings", Metric::textHeader, {}, Metric::weightSemiBold);
    b.space(title);
    b.label(title, "22 controls", Metric::textCaption);
    auto preset = take(b.controls.combo_box(
        head, "brush-preset",
        {.text = vm->preset.get(), .arrow = ii::gui::StepperArrow::down, .width = Metric::size336},
        {.clicked = [vm] { vm->choose_preset(); }}));
    good(ii::gui::bind_text(*bindings, preset, vm->preset));
    b.label(head, "Drag to adjust · − / + for fine steps", Metric::textCaption);
    borders(head, false, false, true);
    const auto scroll =
        take(raw.scroll_view(root, "tool-scroll",
                             {.width = Metric::size344,
                              .height = expanded ? double(Metric::toolExpandedScrollHeight)
                                                 : double(Metric::toolScrollHeight),
                              .enabled = !expanded,
                              .show_indicator = !expanded}));
    if (!expanded)
        good(scroll.set_flex(1));
    const auto content = b.column(scroll, "tool-parameters", 0, 0, Metric::space0, Metric::space4);
    const auto settings = vm->parameters;
    std::string_view group;
    for (const auto &spec : ToolSettings::specs())
    {
        if (spec.group != group)
        {
            group = spec.group;
            auto row = b.row(content, {}, 0, Metric::size30);
            b.label(row, std::string(group), Metric::textDescription, {}, Metric::weightSemiBold);
            b.space(row);
            const auto count =
                std::ranges::count(ToolSettings::specs(), group, &ParameterSpec::group);
            b.label(row, std::to_string(count), Metric::textCaption);
        }
        const auto key = std::string(spec.key);
        const auto block =
            take(b.controls.vstack(content, "parameter-" + key,
                                   {.spacing = Metric::space2,
                                    .alignment = ii::gui::StackAlignment::stretch,
                                    .insets = ii::gui::Insets{Metric::space0, Metric::space6,
                                                              Metric::space0, Metric::space6}}));
        good(block.set_extent({0, Metric::toolParameterHeight}));
        borders(block, false, false, true);
        const auto label = b.row(block, {}, 0, Metric::size22);
        b.glyph(label, std::string(spec.icon));
        auto name = b.label(label, std::string(spec.name));
        good(name.set_flex(1));
        const auto value = b.label(label, settings->label(spec.id), Metric::textHeader,
                                   "value-" + key, Metric::weightSemiBold);
        good(bindings->bind(value, settings->values,
                            [settings, id = spec.id](Widget w, const ToolValues &)
                            { return w.set_text(settings->label(id)); }));
        const auto decrease = b.button(
            label, spec.id == P::size ? "size-minus" : "decrease-" + key, "−",
            [vm, id = spec.id] { vm->step_parameter(id, -1); }, Metric::size28);
        const auto increase = b.button(
            label, spec.id == P::size ? "size-plus" : "increase-" + key, "+",
            [vm, id = spec.id] { vm->step_parameter(id, 1); }, Metric::size28);
        good(decrease.set_accessibility_name("Decrease " + std::string(spec.name)));
        good(increase.set_accessibility_name("Increase " + std::string(spec.name)));
        const auto slider = take(b.controls.slider(
            block, spec.id == P::size ? "brush-size" : "adjust-" + key,
            {.minimum = 0,
             .maximum = 1,
             .value = settings->normalized(spec.id),
             .width = Metric::size336,
             .accessibility_name = std::string(spec.name)},
            [vm, id = spec.id](double v) { vm->set_parameter_normalized(id, v); }));
        good(slider.set_layout_handler([slider](ii::gui::Rect r) { (void)r; }));
        good(bindings->bind(slider, settings->values,
                            [settings, id = spec.id](Widget w, const ToolValues &)
                            { return w.set_value(settings->normalized(id)); }));
        const auto range = b.row(block, {}, 0, Metric::size11);
        b.label(range, ii::gui::binding_text(spec.minimum) + std::string(spec.unit),
                Metric::textCaption);
        b.space(range);
        b.label(range, ii::gui::binding_text(spec.maximum) + std::string(spec.unit),
                Metric::textCaption);
    }
    const auto foot =
        b.row(root, "tool-actions", 0, Metric::size38, Metric::space6, Metric::space4);
    borders(foot, true);
    auto segments = take(b.controls.icon_segmented_control(foot, "brush-dynamics"));
    b.icon(segments, "pressure-enabled", "brush-pressure",
           [vm] { vm->set_pressure_enabled(!vm->pressure_enabled.get()); });
    b.icon(segments, "tilt-detail", "panel-tilt-angle",
           [scroll] { good(scroll.set_scroll_offset(Metric::toolDynamicsScrollOffset)); });
    b.icon(segments, "smoothing-detail", "brush-smoothing",
           [scroll] { good(scroll.set_scroll_offset(Metric::toolDynamicsScrollOffset)); });
    b.icon(segments, "clone-source", "clone-source", [vm] { vm->select_tool("clone-stamp"); });
    b.space(foot);
    b.icon(foot, "reset-tool", "generalundo", [vm] { vm->reset_parameters(); });
    b.icon(foot, "save-tool", "generalsave", [vm] { vm->save_parameters(); });
    return root;
}
} // namespace congregation::native
