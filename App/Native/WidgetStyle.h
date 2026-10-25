#pragma once
#include "EditorPalette.h"
#include "Style.h"
#include <LVRS/Widgets.h>
#include <iiGUI/Bindings.h>
#include <string>

namespace congregation::native::widgets
{
template <class T> T take(ii::core::Result<T> r)
{
    if (!r)
    {
        if (r.error().cause)
            std::rethrow_exception(r.error().cause);
        throw std::runtime_error(r.error().message);
    }
    return std::move(*r);
}
inline void good(ii::core::Result<void> r)
{
    if (!r)
        throw std::runtime_error(r.error().message);
}
using Widget = ii::gui::Widget;
inline constexpr ii::gui::WidgetColor ink = Color::textBody;
void borders(Widget, bool top = false, bool right = false, bool bottom = false, bool left = false);
class Builder final
{
  public:
    explicit Builder(ii::gui::WidgetBuilder &b, std::string prefix = "ui/")
        : raw(b), controls(b), prefix_(std::move(prefix))
    {
    }
    Widget column(Widget, std::string id = {}, double width = 0, double height = 0,
                  double gap = Metric::space0, double padding = Metric::space0);
    Widget row(Widget, std::string id = {}, double width = 0, double height = Metric::size22,
               double gap = Metric::space4, double padding = Metric::space0);
    Widget label(Widget, std::string text, double size = Metric::textBody, std::string id = {},
                 unsigned weight = Metric::weightMedium);
    Widget icon(Widget, std::string id, std::string name, ii::runtime::Task = {});
    Widget button(Widget, std::string id, std::string text, ii::runtime::Task = {},
                  double width = 0, ii::gui::ButtonTone = ii::gui::ButtonTone::normal);
    Widget glyph(Widget, std::string name, double size = Metric::iconSize);
    Widget choice(Widget, std::string id, std::string text, std::string icon, ii::runtime::Task);
    Widget space(Widget, double minimum = Metric::space0);
    [[nodiscard]] std::string id(std::string value = {});
    ii::gui::WidgetBuilder &raw;
    lvrs::Widgets controls;

  private:
    std::string prefix_;
    std::size_t next_ = 0;
};
} // namespace congregation::native::widgets
