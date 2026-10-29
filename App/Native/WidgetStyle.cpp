#include "WidgetStyle.h"
#include "EditorLayout.h"
#include "Style.h"
namespace congregation::native::widgets
{
void borders(Widget widget, bool top, bool right, bool bottom, bool left)
{
    good(widget.set_layout_handler(
        [widget, top, right, bottom, left](ii::gui::Rect r)
        {
            ii::gui::WidgetDrawing d;
            const auto color = editor::linear(editor::panel_edge_color());
            const bool drawable = r.width > 0 && r.height > 0;
            if (drawable && top)
                d.panels.push_back({{0, 0, r.width, Metric::stroke1}, color, color});
            if (drawable && right)
                d.panels.push_back(
                    {{r.width - Metric::stroke1, 0, Metric::stroke1, r.height}, color, color});
            if (drawable && bottom)
                d.panels.push_back(
                    {{0, r.height - Metric::stroke1, r.width, Metric::stroke1}, color, color});
            if (drawable && left)
                d.panels.push_back({{0, 0, Metric::stroke1, r.height}, color, color});
            auto result = widget.set_drawing(std::move(d));
            if (!result && result.error().code != ii::core::ErrorCode::closed)
                good(std::move(result));
        }));
}
std::string Builder::id(std::string value)
{
    return value.empty() ? prefix_ + std::to_string(next_++) : value;
}
Widget Builder::column(Widget p, std::string name, double w, double h, double gap, double pad)
{
    auto n = take(controls.vstack(
        p, id(std::move(name)),
        {.spacing = gap, .padding = pad, .alignment = ii::gui::StackAlignment::stretch}));
    good(n.set_extent({w, h}));
    return n;
}
Widget Builder::row(Widget p, std::string name, double w, double h, double gap, double pad)
{
    auto n = take(controls.hstack(
        p, id(std::move(name)),
        {.spacing = gap, .padding = pad, .alignment = ii::gui::StackAlignment::center}));
    good(n.set_extent({w, h}));
    return n;
}
Widget Builder::label(Widget p, std::string value, double size, std::string name, unsigned weight)
{
    return take(controls.label(p, id(std::move(name)),
                               {.text = std::move(value),
                                .font_size = size,
                                .selectable = false,
                                .role = size == Metric::textCaption  ? ii::gui::TextRole::caption
                                        : size == Metric::textHeader ? ii::gui::TextRole::header
                                                                     : ii::gui::TextRole::body,
                                .color = ink,
                                .line_height = size,
                                .font_weight = weight,
                                .elide_right = true}));
}
Widget Builder::icon(Widget p, std::string name, std::string source, ii::runtime::Task action)
{
    return take(controls.icon_button(p, id(std::move(name)),
                                     {.tone = ii::gui::ButtonTone::borderless,
                                      .icon_name = source,
                                      .accessibility_name = source,
                                      .icon_size = Metric::iconSize,
                                      .icon_mode = true,
                                      .height = Metric::size22,
                                      .horizontal_padding = Metric::space2},
                                     std::move(action)));
}
Widget Builder::button(Widget p, std::string name, std::string value, ii::runtime::Task action,
                       double width, ii::gui::ButtonTone tone)
{
    return take(controls.button(p, id(std::move(name)),
                                {.text = value,
                                 .tone = tone,
                                 .accessibility_name = value,
                                 .height = Metric::size22,
                                 .width = width > 0 ? std::optional<double>(width) : std::nullopt},
                                std::move(action)));
}
Widget Builder::glyph(Widget p, std::string source, double size)
{
    return take(raw.image(p, id(),
                          {.image = take(lvrs::Icons::image(source, Metric::iconRasterSize)),
                           .width = size,
                           .height = size,
                           .accessibility_name = source}));
}
Widget Builder::choice(Widget parent, std::string name, std::string value, std::string source,
                       ii::runtime::Task action)
{
    auto root = take(raw.action_surface(
        parent, id(std::move(name)),
        {.accessibility_name = value, .minimum_height = Metric::size22, .height = Metric::size22},
        std::move(action)));
    auto content = row(root, {}, 0, Metric::size22, Metric::space4);
    glyph(content, std::move(source));
    auto text = label(content, std::move(value));
    good(text.set_flex(1));
    return root;
}
Widget Builder::space(Widget p, double minimum) { return take(controls.spacer(p, id(), minimum)); }
} // namespace congregation::native::widgets
