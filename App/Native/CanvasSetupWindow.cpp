#include "CanvasSetupWindow.h"
#include "EditorLayout.h"
#include "Style.h"
#include "WidgetStyle.h"
#include <algorithm>
#include <cmath>
#include <iiGUI/Text.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <sstream>
#include <tuple>

namespace congregation::native
{
namespace
{
using namespace widgets;
using ii::gui::Rect;
using ii::gui::Widget;
void checked(ii::core::Result<void> result)
{
    if (!result)
    {
        if (result.error().cause)
            std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
Rect rect(double x, double y, double w, double h)
{
    return {float(x), float(y), float(std::max(0., w)), float(std::max(0., h))};
}
void place(Widget w, double x, double y, double width, double height)
{
    checked(w.set_frame(rect(x, y, width, height)));
}
Widget label(ii::gui::WidgetBuilder& raw, Widget parent, std::string id, std::string text,
             double size = Metric::textCaption, bool wrap = false)
{
    const bool heading = size >= Metric::textSubheader;
    const auto weight = size == Metric::textSubtitle  ? Metric::weightBold
                        : size == Metric::textCaption ? Metric::weightRegular
                        : size == Metric::textBody    ? Metric::weightMedium
                                                      : Metric::weightSemiBold;
    const auto ink = heading                           ? Color::textTitleHeader
                     : size == Metric::textCaption     ? Color::textCaption
                     : size == Metric::textDescription ? Color::textDescription
                                                       : Color::textBody;
    return take(lvrs::Widgets(raw).label(parent, id,
                                         {.text = std::move(text),
                                          .font_size = size,
                                          .selectable = false,
                                          .color = ink,
                                          .line_height = size,
                                          .font_weight = unsigned(weight),
                                          .elide_right = !wrap}));
}
void set_text(Widget widget, const std::string& text)
{
    if (auto s = widget.snapshot(); s && s->text != text)
        checked(widget.set_text(text));
}
double text_width(const std::string& text, double font, unsigned weight = Metric::weightRegular)
{
    auto metrics = take(ii::gui::measure_text_layout({.text = text,
                                                      .font_size = float(font),
                                                      .font_weight = weight,
                                                      .font_family = "Pretendard",
                                                      .line_height = float(font)}));
    return metrics.width;
}
std::string wrapped(const std::string& text, double width, double font,
                    unsigned weight = Metric::weightRegular, std::size_t maximum_lines = 0)
{
    std::istringstream words(text);
    std::string result, line;
    std::size_t lines = 1;
    for (std::string word; words >> word;)
    {
        const auto next = line.empty() ? word : line + " " + word;
        if (!line.empty() && text_width(next, font, weight) > width)
        {
            if (maximum_lines && lines >= maximum_lines)
            {
                while (!line.empty() && text_width(line + "…", font, weight) > width)
                {
                    auto last = line.size() - 1;
                    while (last && (static_cast<unsigned char>(line[last]) & 0xc0) == 0x80)
                        --last;
                    line.resize(last);
                }
                return result + line + "…";
            }
            result += line + '\n';
            line.clear();
            ++lines;
        }
        if (!line.empty())
            line += ' ';
        line += word;
    }
    return result + line;
}
double lines_height(const std::string& text, double line_height)
{
    return (1 + std::count(text.begin(), text.end(), '\n')) * line_height;
}
struct Preview final
{
    int width = 1, height = 1;
    bool selected = false, ratio = false;
    std::string text;
};

} // namespace

struct CanvasSetupWindow::Content final : std::enable_shared_from_this<Content>
{
    std::shared_ptr<CanvasSetupModel> model;
    Widget root, header, footer, body_host, body, compact_content;
    Widget title, subtitle, search, compact_search, search_icon, compact_search_icon, search_clear,
        compact_search_clear, custom, close, category_dropdown;
    Widget summary, background_summary, cancel, create;
    Widget categories_scroll, categories_column, gallery_scroll, gallery_backdrop, details_scroll,
        details_background, gallery_host, gallery, details, detail_column;
    Widget gallery_title, gallery_subtitle, count_label, custom_preview, custom_hint, no_results;
    Widget detail_preview, selection_name, selection_path, width_field, height_field, units,
        ppi_group, ppi_field, pixels, aspect, background, error, help;
    Widget category_menu, units_menu, background_menu;
    std::vector<Widget> category_items, quick_buttons;
    Widget category_custom;
    std::vector<std::shared_ptr<ii::gui::WidgetInteractionPort>> focus_ports, category_ports;
    struct Card
    {
        std::string id;
        Widget root, preview, info;
        int width = 1, height = 1;
        Rect frame;
    };
    struct Section
    {
        Widget heading, count;
        std::vector<Card> cards;
    };
    std::vector<Section> groups;
    ii::gui::Size size{};
    std::optional<ii::gui::Size> pending_size;
    bool compact = false, body_ready = false, updating = false;
    double gallery_width = 0, gallery_height = 0;

    // These providers run during UI updates, before native draw-list submission.
    // Retained drawings allow batching while preserving ordinary widget input.
    using RetainedDrawing =
        std::function<ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>(
            const ii::gui::WidgetPaintState&)>;
    struct Painter final : std::enable_shared_from_this<Painter>
    {
        Widget widget, owner;
        bool separate_owner = false;
        RetainedDrawing provider;
        std::shared_ptr<ii::gui::WidgetInteractionPort> port;
        bool refresh()
        {
            auto state = widget.interaction();
            if (!state || !state->alive)
                return false;
            if (!state->visible || state->bounds.width <= 0 || state->bounds.height <= 0)
            {
                checked(widget.set_drawing({}));
                return true;
            }
            auto interaction = owner.interaction();
            if (!interaction)
                return false;
            auto drawing = provider({{state->bounds.width, state->bounds.height},
                                     state->enabled,
                                     interaction->hovered,
                                     interaction->pressed,
                                     interaction->focused});
            if (!drawing)
            {
                if (drawing.error().cause)
                    std::rethrow_exception(drawing.error().cause);
                throw std::runtime_error(drawing.error().message);
            }
            checked(widget.set_drawing(*drawing ? **drawing : ii::gui::WidgetDrawing{}));
            return true;
        }
        void observe()
        {
            const auto weak = weak_from_this();
            port = std::make_shared<ii::gui::WidgetInteractionPort>(
                [weak](const auto& state)
                {
                    if (state.alive)
                        if (auto self = weak.lock())
                            self->refresh();
                });
            checked(widget.observe_interaction(port));
            if (separate_owner)
                checked(owner.observe_interaction(port));
            refresh();
        }
    };
    std::vector<std::shared_ptr<Painter>> painters;
    bool redrawing = false;
    void paint(Widget widget, RetainedDrawing provider, Widget owner = {})
    {
        auto item = std::make_shared<Painter>();
        item->widget = widget;
        item->separate_owner = !owner.is_null();
        item->owner = owner.is_null() ? widget : owner;
        item->provider = std::move(provider);
        painters.push_back(item);
        item->observe();
    }
    void redraw()
    {
        if (std::exchange(redrawing, true))
            return;
        struct Reset
        {
            bool& value;
            ~Reset() { value = false; }
        } reset{redrawing};
        const auto current = painters;
        for (const auto& painter : current)
            if (!painter->refresh())
                std::erase(painters, painter);
    }
    void surface(Widget w, ii::gui::WidgetColor color, double radius = Metric::radius0,
                 bool border = false)
    {
        paint(w,
              [color, radius, border](const ii::gui::WidgetPaintState& s)
                  -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
              {
                  auto d = std::make_shared<ii::gui::WidgetDrawing>();
                  if (s.size.width <= 0 || s.size.height <= 0)
                      return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
                  const auto c = editor::linear(color);
                  d->panels.push_back(
                      {rect(0, 0, s.size.width, s.size.height), c, c,
                       float(std::min(radius, std::min(s.size.width, s.size.height) / 2))});
                  if (border)
                  {
                      const auto edge = editor::linear(Color::canvasChooserBorder);
                      d->panels.push_back(
                          {rect(0, 0, s.size.width, s.size.height), edge, edge,
                           float(std::min(radius, std::min(s.size.width, s.size.height) / 2)),
                           float(std::min<double>(Metric::stroke1,
                                                  std::min(s.size.width, s.size.height) / 2))});
                  }
                  return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
              });
    }

    Widget preview(ii::gui::WidgetBuilder& raw, Widget parent, std::string id, double height,
                   std::function<Preview()> state, bool draw = true)
    {
        auto w = take(lvrs::Widgets(raw).event_listener(parent, id, {0, height}));
        if (!draw)
            return w;
        paint(w,
              [state = std::move(state)](const ii::gui::WidgetPaintState& paint)
                  -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
              {
                  auto d = std::make_shared<ii::gui::WidgetDrawing>();
                  if (paint.size.width <= 0 || paint.size.height <= 0)
                      return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
                  const auto value = state();
                  const auto bg = editor::linear(Color::workspace),
                             fg = editor::linear(value.selected ? Color::primary
                                                                : Color::canvasChooserPaper);
                  d->panels.push_back(
                      {rect(0, 0, paint.size.width, paint.size.height), bg, bg,
                       float(std::min<double>(Metric::radius8,
                                              std::min(paint.size.width, paint.size.height) / 2))});
                  const auto ratio =
                      value.width > 0 && value.height > 0 ? double(value.width) / value.height : 1.;
                  const auto width =
                                 std::min(paint.size.width * Metric::canvasChooserPreviewWidthRatio,
                                          paint.size.height *
                                              Metric::canvasChooserPreviewHeightRatio * ratio),
                             height = width / ratio;
                  const auto x = (paint.size.width - width) / 2,
                             y = (paint.size.height - height) / 2;
                  d->panels.push_back(
                      {rect(x, y, width, height), fg, fg,
                       float(std::min({double(Metric::radius8), height / 3, width / 2}))});
                  if (value.ratio)
                      d->texts.push_back({rect(x, y + (height - Metric::textCaption) / 2, width,
                                               Metric::textCaption),
                                          value.text, editor::linear(Color::textBase),
                                          Metric::textCaption, Metric::weightRegular,
                                          ii::gui::TextAlignment::center, "Pretendard",
                                          Metric::textCaption, true});
                  return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
              });
        return w;
    }
    void install_input(Widget widget)
    {
        const auto weak = weak_from_this();
        checked(widget.set_input_handler(
            [weak](const ii::gui::WidgetInput& e) -> ii::core::Result<bool>
            {
                if (auto s = weak.lock(); s && e.kind == ii::gui::WidgetInputKind::key_down)
                {
                    if (e.key == ii::gui::WidgetKey::escape)
                    {
                        for (auto popup : {s->category_menu, s->units_menu, s->background_menu})
                            if (auto p = popup.snapshot(); p && p->opened)
                            {
                                checked(popup.close_popup());
                                return true;
                            }
                        if (s->model->close_requested)
                            s->model->close_requested();
                        return true;
                    }
                    if ((e.meta || e.control) && e.key == ii::gui::WidgetKey::character &&
                        (e.text == "f" || e.text == "F"))
                    {
                        checked((s->compact ? s->compact_search : s->search).focus());
                        return true;
                    }
                    if ((e.meta || e.control) && e.key == ii::gui::WidgetKey::enter)
                    {
                        if (s->model->can_create.get() && s->model->create_requested)
                            s->model->create_requested();
                        return true;
                    }
                }
                return false;
            }));
        checked(widget.set_escape_action(
            [weak]
            {
                if (auto s = weak.lock(); s && s->model->close_requested)
                    s->model->close_requested();
            }));
    }
    void build(ii::gui::WidgetBuilder& raw, Widget parent)
    {
        root = take(raw.absolute_stack(parent, "setup-root",
                                       {Metric::canvasChooserWidth, Metric::canvasChooserHeight}));
        surface(root, Color::canvasChooserWindow, Metric::radius0, true);
        header = take(raw.absolute_stack(root, "canvasHeader", {}));
        surface(header, Color::canvasChooserSurface);
        footer = take(raw.absolute_stack(root, "canvasFooter", {}));
        surface(footer, Color::canvasChooserSurface);
        body_host = take(raw.content_host(root, "canvasBody"));
        title = label(raw, header, "canvasTitle", "New canvas", Metric::textSubtitle);
        subtitle = label(raw, header, "canvasSubtitle", "Start with a size. Make it your own.");
        const auto weak = weak_from_this();
        Builder b(raw, "canvas/");
        const auto search_options = ii::gui::InputOptions{
            .placeholder = "Search sizes or formats",
            .width = Metric::canvasChooserSearch,
            .accessibility_name = "Search sizes or formats",
            .style = ii::gui::InputStyle{.height = Metric::size22,
                                         .horizontal_padding = Metric::space24}};
        search = take(b.controls.input_field(header, "canvasPresetSearch", search_options,
                                             [weak](std::string text)
                                             {
                                                 if (auto s = weak.lock())
                                                     s->model->search(std::move(text));
                                             }));
        compact_search =
            take(b.controls.input_field(header, "canvasPresetSearchCompact", search_options,
                                        [weak](std::string text)
                                        {
                                            if (auto s = weak.lock())
                                                s->model->search(std::move(text));
                                        }));
        auto image = lvrs::Icons::image("inputFieldSearch", Metric::size24);
        if (!image)
            image = lvrs::Icons::image("generalsearch", Metric::size24);
        const auto search_image = take(std::move(image));
        search_icon = take(raw.image(
            header, "canvasSearchIcon",
            {.image = search_image, .width = Metric::space12, .height = Metric::space12}));
        compact_search_icon = take(raw.image(
            header, "canvasSearchIconCompact",
            {.image = search_image, .width = Metric::space12, .height = Metric::space12}));
        const auto clear_search = [weak]
        {
            if (auto s = weak.lock())
            {
                s->model->search("");
                checked((s->compact ? s->compact_search : s->search).focus());
            }
        };
        const auto clear_button = [&](std::string_view id)
        {
            return take(b.controls.button(
                header, id,
                {.text = "×",
                 .tone = ii::gui::ButtonTone::borderless,
                 .accessibility_name = "Clear search",
                 .height = Metric::space12,
                 .corner_radius = Metric::radius6,
                 .horizontal_padding = Metric::space0,
                 .style = ii::gui::ButtonStyle{.fill = Color::textDescription,
                                               .hover_fill = Color::textDescription,
                                               .pressed_fill = Color::canvasChooserClearPressed,
                                               .text = Color::surfaceInput,
                                               .line_height = Metric::space12,
                                               .font_size = Metric::space12},
                 .width = Metric::space12},
                clear_search));
        };
        search_clear = clear_button("canvasSearchClear");
        compact_search_clear = clear_button("canvasSearchClearCompact");
        custom = b.button(header, "canvasCustomButton", "Custom size",
                          [weak]
                          {
                              if (auto s = weak.lock())
                                  s->model->custom_size();
                          });
        close = b.button(header, "canvasCloseButton", "Close",
                         [weak]
                         {
                             if (auto s = weak.lock(); s && s->model->close_requested)
                                 s->model->close_requested();
                         });
        category_dropdown = take(b.controls.dropdown_button(
            header, "canvasCategoryDropdown",
            {.text = "Social media", .tone = ii::gui::ButtonTone::normal},
            [weak]
            {
                if (auto s = weak.lock())
                    checked(s->category_menu.open_for(
                        s->category_dropdown, 0,
                        take(s->category_dropdown.snapshot()).bounds.height + Metric::space2));
            }));
        summary = label(raw, footer, "canvasSelectionSummary", "", Metric::textDescription, true);
        background_summary =
            label(raw, footer, "canvasBackgroundSummary", "", Metric::textCaption, true);
        cancel = b.button(footer, "canvasCancelButton", "Cancel",
                          [weak]
                          {
                              if (auto s = weak.lock(); s && s->model->close_requested)
                                  s->model->close_requested();
                          });
        create = b.button(
            footer, "create-canvas", "Create canvas",
            [weak]
            {
                if (auto s = weak.lock();
                    s && s->model->can_create.get() && s->model->create_requested)
                    s->model->create_requested();
            },
            0, ii::gui::ButtonTone::primary);
        category_menu = take(b.controls.context_menu(
            root, "canvasCategoryMenu", {.item_width = Metric::canvasChooserCategories}));
        for (std::size_t i = 0; i < model->catalogue().categories().size(); ++i)
            take(b.controls.context_menu_item(category_menu,
                                              "canvasCategoryChoice-" + std::to_string(i),
                                              {.label = model->catalogue().categories()[i].name,
                                               .key_visible = false,
                                               .show_icon_slot = false,
                                               .show_chevron = false,
                                               .has_child_items = false},
                                              [weak, i]
                                              {
                                                  if (auto s = weak.lock())
                                                      s->model->choose_category(i);
                                              }));
        install_input(root);
        checked(root.set_layout_handler(
            [weak](Rect r)
            {
                if (auto s = weak.lock())
                    s->layout({r.width, r.height});
            }));
        model->changed = [weak](CanvasSetupChange change)
        {
            if (auto s = weak.lock())
                s->refresh(change);
        };
        layout({Metric::canvasChooserWidth, Metric::canvasChooserHeight});
    }
    void build_body(ii::gui::WidgetBuilder& raw)
    {
        Builder b(raw, "chooser-body/");
        const auto weak = weak_from_this();
        body = take(raw.absolute_stack({}, "canvasBodyLayout", {}));
        units_menu = take(b.controls.context_menu(body, "canvasUnitsMenu",
                                                  {.item_width = Metric::canvasChooserCategories}));
        for (const auto& [name, value] : std::initializer_list<std::pair<const char*, const char*>>{
                 {"Pixels (px)", "px"}, {"Millimeters (mm)", "mm"}, {"Inches (in)", "in"}})
            take(b.controls.context_menu_item(units_menu, "canvasUnit-" + std::string(value),
                                              {.label = name,
                                               .key_visible = false,
                                               .show_icon_slot = false,
                                               .show_chevron = false,
                                               .has_child_items = false},
                                              [weak, value]
                                              {
                                                  if (auto s = weak.lock())
                                                      s->model->change_unit(value);
                                              }));
        background_menu = take(b.controls.context_menu(
            body, "canvasBackgroundMenu", {.item_width = Metric::canvasChooserCategories}));
        for (const auto* name : {"White", "Transparent", "Black"})
            take(b.controls.context_menu_item(background_menu,
                                              "canvasBackground-" + std::string(name),
                                              {.label = name,
                                               .key_visible = false,
                                               .show_icon_slot = false,
                                               .show_chevron = false,
                                               .has_child_items = false},
                                              [weak, name]
                                              {
                                                  if (auto s = weak.lock())
                                                      s->model->set_background(name);
                                              }));
        category_items.clear();
        category_ports.clear();
        install_input(body);
        gallery_backdrop = take(raw.absolute_stack(body, "canvasGalleryBackdrop", {}));
        paint(gallery_backdrop,
              [weak](const ii::gui::WidgetPaintState& paint)
                  -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
              {
                  auto d = std::make_shared<ii::gui::WidgetDrawing>();
                  if (auto s = weak.lock())
                  {
                      const auto viewport = s->gallery_scroll.snapshot();
                      if (!viewport)
                          return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
                      const auto offset = viewport->scroll_offset;
                      for (const auto& group : s->groups)
                          for (const auto& card : group.cards)
                          {
                              auto box = card.frame;
                              box.y -= float(offset);
                              if (box.y + box.height <= 0 || box.y >= paint.size.height ||
                                  box.width <= 0)
                                  continue;
                              const auto bg = editor::linear(Color::canvasChooserSurface),
                                         edge = editor::linear(Color::primary);
                              d->panels.push_back({box, bg, bg, Metric::radius8});
                              const auto interaction = card.root.interaction();
                              const bool focused = interaction && interaction->focused;
                              const bool selected = !s->model->custom && s->model->selected &&
                                                    s->model->selected->id == card.id;
                              if (focused || selected)
                                  d->panels.push_back(
                                      {box, edge, edge, Metric::radius8,
                                       float(focused ? Metric::stroke2 : Metric::stroke1)});
                              const auto area =
                                  rect(box.x + Metric::space12, box.y + Metric::space12,
                                       box.width - 2 * Metric::space12,
                                       Metric::canvasChooserCardPreview);
                              const auto preview_bg = editor::linear(Color::workspace),
                                         preview_fg = editor::linear(
                                             selected ? Color::primary : Color::canvasChooserPaper);
                              d->panels.push_back({area, preview_bg, preview_bg, Metric::radius8});
                              const auto ratio = double(card.width) / card.height;
                              const auto w = std::min<double>(
                                             area.width * Metric::canvasChooserPreviewWidthRatio,
                                             area.height * Metric::canvasChooserPreviewHeightRatio *
                                                 ratio),
                                         h = w / ratio;
                              d->panels.push_back(
                                  {rect(area.x + (area.width - w) / 2,
                                        area.y + (area.height - h) / 2, w, h),
                                   preview_fg, preview_fg,
                                   float(std::min({double(Metric::radius8), w / 2, h / 3}))});
                          }
                  }
                  return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
              });
        if (compact)
        {
            gallery_scroll = take(raw.scroll_view(body, "canvasCompactScroll"));
            details_scroll = gallery_scroll;
            compact_content = take(raw.absolute_stack(gallery_scroll, "canvasCompactContent", {}));
            gallery_host = take(raw.content_host(compact_content, "canvasGalleryHost"));
        }
        else
        {
            categories_scroll = take(raw.scroll_view(body, "canvasCategoryScroll",
                                                     {.width = Metric::canvasChooserCategories}));
            auto column = b.column(categories_scroll, "canvasCategories",
                                   Metric::canvasChooserCategories, 0, Metric::space2);
            categories_column = column;
            paint(column,
                  [weak](const ii::gui::WidgetPaintState&)
                      -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
                  {
                      auto d = std::make_shared<ii::gui::WidgetDrawing>();
                      if (auto s = weak.lock())
                      {
                          const auto parent = s->categories_column.snapshot();
                          if (parent)
                              for (std::size_t i = 0; i < s->category_items.size(); ++i)
                              {
                                  const auto state = s->category_items[i].interaction();
                                  if (!state || !state->visible)
                                      continue;
                                  const bool selected = !s->model->custom &&
                                                        s->model->query.empty() &&
                                                        s->model->category_index == i;
                                  if (!selected && !state->hovered && !state->pressed)
                                      continue;
                                  const auto fill =
                                      editor::linear(selected         ? Color::canvasChooserSelected
                                                     : state->pressed ? Color::canvasChooserPressed
                                                                      : Color::surfaceAlt);
                                  const auto r = rect(state->bounds.x - parent->bounds.x,
                                                      state->bounds.y - parent->bounds.y,
                                                      state->bounds.width, state->bounds.height);
                                  d->panels.push_back({r, fill, fill, Metric::radius4});
                              }
                      }
                      return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(d));
                  });
            auto heading = label(raw, column, "canvasCategoriesHeading", "CATEGORIES");
            b.column(column, {}, 0, Metric::space8);
            for (std::size_t i = 0; i < model->catalogue().categories().size(); ++i)
            {
                const auto name = model->catalogue().categories()[i].name;
                auto item = take(raw.action_surface(
                    column, "canvasCategory_" + std::to_string(i),
                    {.accessibility_name = name, .height = Metric::canvasChooserCategoryRow},
                    [weak, i]
                    {
                        if (auto s = weak.lock())
                            s->model->choose_category(i);
                    }));
                auto row = b.row(item, {}, 0, Metric::canvasChooserCategoryRow, Metric::space8);
                checked(row.set_stack_options(
                    {.spacing = Metric::space8,
                     .alignment = ii::gui::StackAlignment::center,
                     .insets = ii::gui::Insets{Metric::space4, Metric::space0, Metric::space4,
                                               Metric::space0}}));
                b.glyph(row, "imagefitContent");
                label(raw, row, "canvasCategoryLabel_" + std::to_string(i), name, Metric::textBody);
                auto port = std::make_shared<ii::gui::WidgetInteractionPort>(
                    [weak](const auto& state)
                    {
                        if (state.alive)
                            if (auto s = weak.lock())
                                s->redraw();
                    });
                checked(item.observe_interaction(port));
                category_ports.push_back(std::move(port));
                category_items.push_back(item);
            }
            b.column(column, {}, 0, Metric::space8);
            label(raw, column, "canvasPresetTotal",
                  std::to_string(model->catalogue().presets().size()) + " canvas presets");
            category_custom = b.button(column, "canvasCustomDimensions", "Custom dimensions",
                                       [weak]
                                       {
                                           if (auto s = weak.lock())
                                               s->model->custom_size();
                                       });
            gallery_scroll = take(raw.scroll_view(body, "canvasGalleryScroll"));
            gallery_host = take(raw.content_host(gallery_scroll, "canvasGalleryHost"));
            details_background = take(raw.absolute_stack(body, "canvasDetailsPanel", {}));
            surface(details_background, Color::canvasChooserSurface);
            details_scroll = take(raw.scroll_view(details_background, "canvasDetailsScroll",
                                                  {.width = Metric::canvasChooserDetails}));
        }
        checked(gallery_scroll.set_scroll_handler(
            [weak](const ii::gui::ScrollViewport&)
            {
                if (auto s = weak.lock())
                    s->redraw();
            }));
        details = b.column(compact ? compact_content : details_scroll, "canvasDetails");
        surface(details, Color::canvasChooserSurface);
        detail_column =
            b.column(details, "canvasDetailsContent", 0, 0, Metric::space12, Metric::space16);
        label(raw, detail_column, "canvasDetailsHeading", "CANVAS DETAILS");
        detail_preview = preview(
            raw, detail_column, "canvasDetailsPreview", Metric::canvasChooserDetailPreview,
            [weak]
            {
                if (auto s = weak.lock())
                {
                    auto value = s->model->specification();
                    return Preview{value ? value->pixel_width : 1, value ? value->pixel_height : 1,
                                   bool(value), bool(value) && s->model->unit == "px",
                                   s->model->aspect_ratio()};
                }
                return Preview{};
            });
        selection_name =
            label(raw, detail_column, "canvasSelectionName", "", Metric::textSubheader, true);
        selection_path =
            label(raw, detail_column, "canvasSelectionPath", "", Metric::textCaption, true);
        auto dimensions = b.row(detail_column, "canvasDimensions", 0, 0, Metric::space12);
        auto width = b.column(dimensions, "canvasWidthGroup", 0, 0, Metric::space4);
        checked(width.set_flex(1));
        label(raw, width, "canvasWidthLabel", "Width");
        width_field = take(b.controls.input_field(
            width, "canvas-width",
            {.accessibility_name = "Width", .style = ii::gui::InputStyle{.height = Metric::size22}},
            [weak](std::string value)
            {
                if (auto s = weak.lock())
                    s->model->set_new_width(std::move(value));
            }));
        auto height = b.column(dimensions, "canvasHeightGroup", 0, 0, Metric::space4);
        checked(height.set_flex(1));
        label(raw, height, "canvasHeightLabel", "Height");
        height_field =
            take(b.controls.input_field(height, "canvas-height",
                                        {.accessibility_name = "Height",
                                         .style = ii::gui::InputStyle{.height = Metric::size22}},
                                        [weak](std::string value)
                                        {
                                            if (auto s = weak.lock())
                                                s->model->set_new_height(std::move(value));
                                        }));
        auto group = b.column(detail_column, "canvasUnitsGroup", 0, 0, Metric::space4);
        label(raw, group, "canvasUnitsLabel", "Units");
        units = take(b.controls.dropdown_button(
            group, "canvasUnitsButton",
            {.text = "Pixels (px)", .tone = ii::gui::ButtonTone::normal},
            [weak]
            {
                if (auto s = weak.lock())
                    checked(s->units_menu.open_for(
                        s->units, 0, take(s->units.snapshot()).bounds.height + Metric::space2));
            }));
        ppi_group = b.column(detail_column, "canvasResolutionGroup", 0, 0, Metric::space4);
        label(raw, ppi_group, "canvasResolutionLabel", "Print resolution (PPI)");
        ppi_field =
            take(b.controls.input_field(ppi_group, "canvasPpiInput",
                                        {.accessibility_name = "Print resolution (PPI)",
                                         .style = ii::gui::InputStyle{.height = Metric::size22}},
                                        [weak](std::string value)
                                        {
                                            if (auto s = weak.lock())
                                                s->model->set_resolution(std::move(value));
                                        }));
        pixels = label(raw, ppi_group, "canvasPixelExtent", "");
        aspect = label(raw, detail_column, "canvasAspectRatio", "");
        group = b.column(detail_column, "canvasBackgroundGroup", 0, 0, Metric::space4);
        label(raw, group, "canvasBackgroundLabel", "Background");
        background = take(b.controls.dropdown_button(
            group, "canvasBackgroundButton", {.text = "White", .tone = ii::gui::ButtonTone::normal},
            [weak]
            {
                if (auto s = weak.lock())
                    checked(s->background_menu.open_for(
                        s->background, 0,
                        take(s->background.snapshot()).bounds.height + Metric::space2));
            }));
        error = take(b.controls.label(detail_column, "canvasValidationError",
                                      {.font_size = Metric::textCaption,
                                       .selectable = false,
                                       .color = Color::danger,
                                       .line_height = Metric::textCaption,
                                       .font_weight = Metric::weightRegular}));
        help = label(raw, detail_column, "canvasDetailsHelp", "", Metric::textCaption, true);
    }

    void build_gallery(ii::gui::WidgetBuilder& raw)
    {
        Builder b(raw, "chooser-gallery/");
        const auto weak = weak_from_this();
        gallery = take(raw.absolute_stack({}, "canvasGallery", {}));
        install_input(gallery);
        groups.clear();
        quick_buttons.clear();
        focus_ports.clear();
        gallery_title = label(raw, gallery, "canvasGalleryTitle", "", Metric::textHeader, true);
        count_label = label(raw, gallery, "canvasResultCount", "");
        gallery_subtitle =
            label(raw, gallery, "canvasGallerySubtitle", "", Metric::textDescription, true);
        if (model->custom)
        {
            custom_preview = preview(
                raw, gallery, "canvasCustomPreview",
                compact ? Metric::canvasChooserCompactPreview : Metric::canvasChooserCustomPreview,
                [weak]
                {
                    if (auto s = weak.lock())
                    {
                        const auto spec = s->model->specification();
                        return Preview{spec ? spec->pixel_width : 1, spec ? spec->pixel_height : 1,
                                       true, true, s->model->aspect_ratio()};
                    }
                    return Preview{};
                });
            custom_hint = label(raw, gallery, "canvasCustomHint",
                                "Use px for digital graphics, mm or inches for print.",
                                Metric::textCaption, true);
            for (const auto& [name, w, h] :
                 std::initializer_list<std::tuple<const char*, int, int>>{
                     {"Square · 1:1", 1080, 1080},
                     {"Landscape · 16:9", 1920, 1080},
                     {"Portrait · 9:16", 1080, 1920}})
                quick_buttons.push_back(b.button(
                    gallery, "canvasQuick-" + std::to_string(w) + "-" + std::to_string(h), name,
                    [weak, w, h]
                    {
                        if (auto s = weak.lock())
                            s->model->quick_size(w, h);
                    }));
        }
        else if (!model->result_count())
        {
            no_results = b.column(gallery, "canvasNoResults", 0, 0, Metric::space12);
            checked(no_results.set_stack_options(
                {.spacing = Metric::space12, .alignment = ii::gui::StackAlignment::leading}));
            label(raw, no_results, "canvasNoResultsTitle", "No formats found", Metric::textHeader);
            label(raw, no_results, "canvasNoResultsHelp",
                  "Try a platform, format or size, such as Instagram, A4 or 1920 × 1080.",
                  Metric::textDescription, true);
            b.button(no_results, "canvasClearSearchButton", "Clear search",
                     [weak]
                     {
                         if (auto s = weak.lock())
                             s->model->search("");
                     });
        }
        else
            for (const auto& section : model->sections())
            {
                Section group;
                group.heading =
                    label(raw, gallery, "canvasSection-" + std::to_string(groups.size()),
                          section.name, Metric::textSubheader, true);
                group.count =
                    label(raw, gallery, "canvasSectionCount-" + std::to_string(groups.size()),
                          std::to_string(section.presets.size()) + " formats");
                for (auto index : section.presets)
                {
                    const auto preset = model->catalogue().presets()[index];
                    Card card;
                    card.id = preset.id;
                    card.width = preset.pixel_width;
                    card.height = preset.pixel_height;
                    card.root = take(raw.action_surface(
                        gallery, "canvasPresetCard_" + preset.id,
                        {.accessibility_name = preset.name,
                         .focus_ring = false,
                         .height = Metric::canvasChooserCard,
                         .accessibility_description =
                             ii::core::format_decimal(preset.width) + " × " +
                             ii::core::format_decimal(preset.height) + " " + preset.unit},
                        [weak, id = preset.id]
                        {
                            if (auto s = weak.lock())
                                s->select_card(id);
                        }));
                    auto content =
                        b.column(card.root, "canvasPresetContent_" + preset.id, 0,
                                 Metric::canvasChooserCard, Metric::space8, Metric::space12);
                    card.preview = preview(
                        raw, content, "canvasPresetPreview_" + preset.id,
                        Metric::canvasChooserCardPreview,
                        [weak, preset]
                        {
                            auto s = weak.lock();
                            return Preview{preset.pixel_width,
                                           preset.pixel_height,
                                           s && !s->model->custom && s->model->selected &&
                                               s->model->selected->id == preset.id,
                                           false,
                                           {}};
                        },
                        false);
                    card.info = b.column(content, "canvasPreset_" + preset.id, 0, Metric::size44,
                                         Metric::space4);
                    checked(card.info.set_stack_options(
                        {.spacing = Metric::space4,
                         .alignment = ii::gui::StackAlignment::stretch,
                         .insets = ii::gui::Insets{Metric::space12, Metric::space8, Metric::space12,
                                                   Metric::space8}}));
                    label(raw, card.info, "canvasPresetLabel_" + preset.id, preset.name,
                          Metric::textBody);
                    label(raw, card.info, "canvasPresetDescription_" + preset.id,
                          ii::core::format_decimal(preset.width) + " × " +
                              ii::core::format_decimal(preset.height) + " " + preset.unit);
                    auto port = std::make_shared<ii::gui::WidgetInteractionPort>(
                        [weak, id = preset.id](const auto& state)
                        {
                            if (state.alive && state.focused)
                                if (auto p = weak.lock())
                                {
                                    p->reveal(id);
                                    p->redraw();
                                }
                        });
                    checked(card.root.observe_interaction(port));
                    focus_ports.push_back(std::move(port));
                    group.cards.push_back(std::move(card));
                }
                groups.push_back(std::move(group));
            }
    }

    void select_card(std::string_view id)
    {
        for (const auto& group : groups)
            for (const auto& card : group.cards)
                if (card.id == id)
                    checked(card.root.focus());
        model->choose(id);
        reveal(id);
        redraw();
    }
    void reveal(std::string_view id)
    {
        const auto viewport = gallery_scroll.snapshot();
        if (!viewport)
            return;
        for (const auto& group : groups)
            for (const auto& card : group.cards)
            {
                if (card.id != id)
                    continue;
                auto top = double(card.frame.y), height = double(card.frame.height);
                if (height > viewport->bounds.height)
                {
                    top += Metric::space12 + Metric::canvasChooserCardPreview + Metric::space8;
                    height = Metric::size44;
                }
                const auto offset = viewport->scroll_offset;
                if (top < offset)
                    checked(gallery_scroll.set_scroll_offset(std::max(0., top - Metric::space16)));
                else if (top + height > offset + viewport->bounds.height)
                    checked(gallery_scroll.set_scroll_offset(
                        std::max(0., top + height - viewport->bounds.height + Metric::space16)));
            }
    }
    void layout(ii::gui::Size next)
    {
        if (next.width <= 0 || next.height <= 0)
            return;
        if (updating)
        {
            pending_size = next;
            return;
        }
        pending_size.reset();
        const bool next_compact = next.width < Metric::canvasChooserCompactThreshold;
        if (body_ready && size == next && compact == next_compact)
            return;
        const bool rebuild = !body_ready || compact != next_compact;
        updating = true;
        size = next;
        compact = next_compact;
        const auto width = size.width - 2 * Metric::space12;
        const auto hh = compact ? Metric::canvasChooserCompactHeader : Metric::canvasChooserHeader;
        const auto fh = compact ? Metric::canvasChooserCompactFooter : Metric::canvasChooserFooter;
        place(header, Metric::space12, Metric::space12, width, hh);
        place(footer, Metric::space12, size.height - Metric::space12 - fh, width, fh);
        place(body_host, Metric::space12, Metric::space12 + hh, width,
              size.height - 2 * Metric::space12 - hh - fh);
        const auto pad = compact ? Metric::space12 : Metric::space24;
        const auto custom_width =
            compact ? Metric::canvasChooserCompactCustom : Metric::canvasChooserCustomButton;
        const auto close_width =
            compact ? Metric::canvasChooserCompactClose : Metric::canvasChooserCloseButton;
        const auto right = width - pad;
        const double row_height =
            compact ? Metric::size22 : Metric::textSubtitle + Metric::space2 + Metric::textCaption;
        place(title, pad, pad,
              width - 2 * pad - custom_width - close_width - 2 * Metric::space12 -
                  (compact ? 0 : Metric::canvasChooserSearch + Metric::space12),
              Metric::textSubtitle);
        place(subtitle, pad, pad + Metric::textSubtitle + Metric::space2, width - 2 * pad,
              Metric::textCaption);
        place(close, right - close_width, pad + (row_height - Metric::size22) / 2, close_width,
              Metric::size22);
        place(custom, right - close_width - Metric::space12 - custom_width,
              pad + (row_height - Metric::size22) / 2, custom_width, Metric::size22);
        if (!compact)
            place(search,
                  right - close_width - custom_width - 2 * Metric::space12 -
                      Metric::canvasChooserSearch,
                  pad + (row_height - Metric::size22) / 2, Metric::canvasChooserSearch,
                  Metric::size22);
        place(compact_search, pad, pad + Metric::size22 + Metric::space8, width - 2 * pad,
              Metric::size22);
        const auto desktop_search_x =
            right - close_width - custom_width - 2 * Metric::space12 - Metric::canvasChooserSearch;
        const auto desktop_search_y = pad + (row_height - Metric::size22) / 2;
        if (!compact)
            place(search_icon, desktop_search_x + Metric::space8,
                  desktop_search_y + (Metric::size22 - Metric::space12) / 2, Metric::space12,
                  Metric::space12);
        place(compact_search_icon, pad + Metric::space8,
              pad + Metric::size22 + Metric::space8 + (Metric::size22 - Metric::space12) / 2,
              Metric::space12, Metric::space12);
        if (!compact)
            place(search_clear,
                  desktop_search_x + Metric::canvasChooserSearch - Metric::space8 - Metric::space12,
                  desktop_search_y + (Metric::size22 - Metric::space12) / 2, Metric::space12,
                  Metric::space12);
        place(compact_search_clear, width - pad - Metric::space8 - Metric::space12,
              pad + Metric::size22 + Metric::space8 + (Metric::size22 - Metric::space12) / 2,
              Metric::space12, Metric::space12);
        place(category_dropdown, pad, pad + 2 * (Metric::size22 + Metric::space8), width - 2 * pad,
              Metric::size22);
        checked(search.set_visible(!compact));
        checked(compact_search.set_visible(compact));
        checked(subtitle.set_visible(!compact));
        checked(category_dropdown.set_visible(compact));
        set_text(custom, compact ? "Custom" : "Custom size");
        checked(search_icon.set_visible(!compact));
        checked(compact_search_icon.set_visible(compact));
        const auto cancel_width =
            compact ? Metric::canvasChooserCompactCancel : Metric::canvasChooserCancel;
        const auto create_width =
            compact ? Metric::canvasChooserCompactCreate : Metric::canvasChooserCustomButton;
        place(create, right - create_width, pad, create_width, Metric::size22);
        place(cancel, right - create_width - Metric::space12 - cancel_width, pad, cancel_width,
              Metric::size22);
        const auto summary_width =
            std::max(0., width - 2 * pad - create_width - cancel_width - 2 * Metric::space12);
        place(summary, pad, pad, summary_width,
              compact ? 2 * Metric::textDescription : Metric::textDescription);
        place(background_summary, pad,
              pad + (compact ? 2 : 1) * Metric::textDescription + Metric::space2, summary_width,
              compact ? 2 * Metric::textCaption : Metric::textCaption);
        if (rebuild)
        {
            const auto weak = weak_from_this();
            checked(body_host.set_content(
                [weak](ii::gui::WidgetBuilder& raw) -> ii::core::Result<void>
                {
                    if (auto s = weak.lock())
                        s->build_body(raw);
                    return {};
                }));
            body_ready = true;
            rebuild_gallery();
        }
        layout_body();
        updating = false;
        refresh(CanvasSetupChange::dimensions);
    }
    void layout_body()
    {
        const auto bounds = take(body_host.snapshot()).bounds;
        checked(body.set_extent({bounds.width, bounds.height}));
        const auto margin = compact ? Metric::space12 : Metric::space24;
        const auto height = std::max(0., double(bounds.height) - 2 * margin);
        if (compact)
        {
            place(gallery_backdrop, margin, margin, bounds.width - 2 * margin, height);
            place(gallery_scroll, margin, margin, bounds.width - 2 * margin, height);
            gallery_width = std::max(0., double(bounds.width) - 2 * margin);
        }
        else
        {
            gallery_width =
                std::max(0., double(bounds.width) - 4 * margin - Metric::canvasChooserCategories -
                                 Metric::canvasChooserDetails);
            place(categories_scroll, margin, margin, Metric::canvasChooserCategories, height);
            place(gallery_backdrop, 2 * margin + Metric::canvasChooserCategories, margin,
                  gallery_width, height);
            place(gallery_scroll, 2 * margin + Metric::canvasChooserCategories, margin,
                  gallery_width, height);
            place(details_background, bounds.width - margin - Metric::canvasChooserDetails, margin,
                  Metric::canvasChooserDetails, height);
            place(details_scroll, 0, 0, Metric::canvasChooserDetails, height);
        }
        layout_gallery();
        layout_details();
    }
    void rebuild_gallery()
    {
        const auto weak = weak_from_this();
        checked(gallery_host.set_content(
            [weak](ii::gui::WidgetBuilder& raw) -> ii::core::Result<void>
            {
                if (auto s = weak.lock())
                    s->build_gallery(raw);
                return {};
            }));
        checked(gallery_scroll.set_scroll_offset(0));
    }
    void layout_gallery()
    {
        if (gallery.is_null())
            return;
        const auto width = gallery_width;
        const auto& m = *model;
        const auto count_width =
            m.custom ? 0
                     : text_width(std::to_string(m.result_count()) + " sizes", Metric::textCaption);
        const auto heading = wrapped(
            m.custom           ? "Custom dimensions"
            : !m.query.empty() ? "Search results"
                               : m.catalogue().categories()[m.category_index].name,
            width - count_width - Metric::space8, Metric::textHeader, Metric::weightSemiBold);
        set_text(gallery_title, heading);
        const auto heading_height = lines_height(heading, Metric::textHeader);
        place(gallery_title, 0, 0, std::max(0., width - count_width - Metric::space8),
              heading_height);
        place(count_label, std::max(0., width - count_width),
              (heading_height - Metric::textCaption) / 2, count_width, Metric::textCaption);
        const auto description =
            wrapped(m.custom           ? "A blank canvas with your exact dimensions."
                    : !m.query.empty() ? "Matching formats across all categories."
                                       : m.catalogue().categories()[m.category_index].subtitle,
                    width, Metric::textDescription, Metric::weightSemiBold);
        set_text(gallery_subtitle, description);
        const auto description_height = lines_height(description, Metric::textDescription);
        place(gallery_subtitle, 0, heading_height + Metric::space16, width, description_height);
        double y = heading_height + 2 * Metric::space16 + description_height;
        if (model->custom)
        {
            const auto ph =
                compact ? Metric::canvasChooserCompactPreview : Metric::canvasChooserCustomPreview;
            place(custom_preview, 0, y, width, ph);
            y += ph + Metric::space16;
            const auto hint = wrapped("Use px for digital graphics, mm or inches for print.", width,
                                      Metric::textCaption);
            set_text(custom_hint, hint);
            const auto hint_height = lines_height(hint, Metric::textCaption);
            place(custom_hint, 0, y, width, hint_height);
            y += hint_height + Metric::space16;
            const auto button_width = compact ? width : (width - 2 * Metric::space12) / 3;
            for (std::size_t i = 0; i < quick_buttons.size(); ++i)
                place(quick_buttons[i], compact ? 0 : i * (button_width + Metric::space12),
                      y + (compact ? i * (Metric::size22 + Metric::space8) : 0), button_width,
                      Metric::size22);
            y += compact ? 3 * Metric::size22 + 2 * Metric::space8 : Metric::size22;
        }
        else if (groups.empty())
        {
            y += Metric::space24;
            place(no_results, 0, y, width, Metric::canvasChooserNoResultsHeight);
            y += Metric::canvasChooserNoResultsHeight;
        }
        else
        {
            const auto columns = std::clamp(
                int((width + Metric::space12) / Metric::canvasChooserCardThreshold), 1, 3);
            const auto card_width = (width - (columns - 1) * Metric::space12) / columns;
            for (auto& section : groups)
            {
                const auto count_text = take(section.count.snapshot()).text;
                const auto count_width = text_width(count_text, Metric::textCaption);
                const auto heading_text = wrapped(take(section.heading.snapshot()).text,
                                                  width - count_width - Metric::space8,
                                                  Metric::textSubheader, Metric::weightSemiBold);
                set_text(section.heading, heading_text);
                const auto height = lines_height(heading_text, Metric::textSubheader);
                place(section.heading, 0, y, std::max(0., width - count_width - Metric::space8),
                      height);
                place(section.count, std::max(0., width - count_width),
                      y + (height - Metric::textCaption) / 2, count_width, Metric::textCaption);
                y += height + Metric::space12;
                for (std::size_t i = 0; i < section.cards.size(); ++i)
                {
                    auto& card = section.cards[i];
                    card.frame =
                        rect((i % columns) * (card_width + Metric::space12),
                             y + (i / columns) * (Metric::canvasChooserCard + Metric::space12),
                             card_width, Metric::canvasChooserCard);
                    checked(card.root.set_frame(card.frame));
                }
                const auto rows = (section.cards.size() + columns - 1) / columns;
                y += rows * Metric::canvasChooserCard + (rows ? rows - 1 : 0) * Metric::space12 +
                     Metric::space16;
            }
        }
        gallery_height = y;
        checked(gallery.set_extent({width, y}));
        checked(gallery_host.set_extent({width, y}));
        if (compact)
            place(gallery_host, 0, 0, width, y);
    }
    void layout_footer()
    {
        const auto width = size.width - 2 * Metric::space12;
        const auto pad = compact ? Metric::space12 : Metric::space24;
        const auto footer_height =
            compact ? Metric::canvasChooserCompactFooter : Metric::canvasChooserFooter;
        const auto cancel_width =
            compact ? Metric::canvasChooserCompactCancel : Metric::canvasChooserCancel;
        const auto create_width =
            compact ? Metric::canvasChooserCompactCreate : Metric::canvasChooserCustomButton;
        const auto text_space =
            std::max(1., width - 2 * pad - create_width - cancel_width - 2 * Metric::space12);
        const auto spec = model->specification();
        const auto summary_text =
            wrapped(spec ? model->selection_name() + " · " + model->dimension_text()
                         : "Choose a canvas size",
                    text_space, Metric::textDescription, Metric::weightSemiBold, 2);
        auto background_name = model->background;
        std::ranges::transform(background_name, background_name.begin(), [](char c)
                               { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; });
        const auto background_text = wrapped("Blank canvas · " + background_name + " background",
                                             text_space, Metric::textCaption);
        set_text(summary, summary_text);
        set_text(background_summary, background_text);
        const auto summary_height = lines_height(summary_text, Metric::textDescription),
                   background_height = lines_height(background_text, Metric::textCaption);
        const auto top = (footer_height - summary_height - Metric::space2 - background_height) / 2;
        place(summary, pad, std::max(0., top), text_space, summary_height);
        place(background_summary, pad, std::max(0., top) + summary_height + Metric::space2,
              text_space, background_height);
        place(create, width - pad - create_width, (footer_height - Metric::size22) / 2,
              create_width, Metric::size22);
        place(cancel, width - pad - create_width - Metric::space12 - cancel_width,
              (footer_height - Metric::size22) / 2, cancel_width, Metric::size22);
    }
    void layout_details()
    {
        const double width = compact ? gallery_width : Metric::canvasChooserDetails;
        const auto inner_width = std::max(0., width - 2 * Metric::space16);
        set_text(help, "Select a preset or customize its dimensions before creating.");
        set_text(error, model->status.get());
        const auto height = take(detail_column.measure(width)).height;
        checked(details.set_extent({width, height}));
        if (compact)
        {
            place(details, 0, gallery_height + Metric::space16, width, height);
            const auto total = gallery_height + Metric::space16 + height;
            checked(compact_content.set_extent({width, total}));
        }
    }
    void refresh(CanvasSetupChange change)
    {
        if (!body_ready || updating)
            return;
        updating = true;
        if (change == CanvasSetupChange::gallery)
            rebuild_gallery();
        const auto& m = *model;
        const auto spec = m.specification();
        set_text(search, m.query);
        set_text(compact_search, m.query);
        checked(search_clear.set_visible(!compact && !m.query.empty()));
        checked(compact_search_clear.set_visible(compact && !m.query.empty()));
        set_text(category_dropdown, m.custom ? "Custom dimensions"
                                             : m.catalogue().categories()[m.category_index].name);
        set_text(gallery_title, m.custom ? "Custom dimensions"
                                : !m.query.empty()
                                    ? "Search results"
                                    : m.catalogue().categories()[m.category_index].name);
        set_text(gallery_subtitle, m.custom ? "A blank canvas with your exact dimensions."
                                   : !m.query.empty()
                                       ? "Matching formats across all categories."
                                       : m.catalogue().categories()[m.category_index].subtitle);
        set_text(count_label, std::to_string(m.result_count()) + " sizes");
        checked(count_label.set_visible(!m.custom));
        set_text(selection_name, m.selection_name());
        set_text(selection_path, m.custom     ? "Custom size · Your dimensions"
                                 : m.selected ? m.selected->category + " · " + m.selected->section
                                              : "Select a format or set custom dimensions.");
        set_text(width_field, m.new_width.get());
        set_text(height_field, m.new_height.get());
        set_text(ppi_field, m.resolution);
        checked(width_field.set_enabled(m.custom || m.selected));
        checked(height_field.set_enabled(m.custom || m.selected));
        set_text(units, m.unit == "px"   ? "Pixels (px)"
                        : m.unit == "mm" ? "Millimeters (mm)"
                                         : "Inches (in)");
        checked(ppi_group.set_visible(m.unit != "px"));
        checked(aspect.set_visible(m.unit == "px"));
        set_text(pixels, spec ? std::to_string(spec->pixel_width) + " × " +
                                    std::to_string(spec->pixel_height) + " px"
                              : "—");
        set_text(aspect, "Aspect ratio: " + m.aspect_ratio());
        set_text(background, m.background);
        checked(error.set_visible(!m.status.get().empty()));
        checked(create.set_enabled(bool(spec)));
        set_text(summary,
                 spec ? m.selection_name() + " · " + m.dimension_text() : "Choose a canvas size");
        auto background_name = m.background;
        std::ranges::transform(background_name, background_name.begin(), [](char c)
                               { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; });
        set_text(background_summary, "Blank canvas · " + background_name + " background");
        checked(detail_preview.request_repaint());
        if (m.custom && !custom_preview.is_null())
            checked(custom_preview.request_repaint());
        for (const auto& group : groups)
            for (const auto& card : group.cards)
            {
                checked(card.root.request_repaint());
                checked(card.preview.request_repaint());
            }
        if (!compact)
            if (auto state = category_custom.snapshot();
                state && state->tone != (m.custom ? ii::gui::ButtonTone::primary
                                                  : ii::gui::ButtonTone::normal))
                checked(category_custom.set_button_options(
                    {.text = "Custom dimensions",
                     .tone = m.custom ? ii::gui::ButtonTone::primary : ii::gui::ButtonTone::normal,
                     .height = Metric::size22}));
        for (std::size_t i = 0; i < category_items.size(); ++i)
        {
            checked(category_items[i].request_repaint());
        }
        layout_gallery();
        layout_details();
        layout_footer();
        redraw();
        updating = false;
        if (pending_size)
            layout(std::exchange(pending_size, {}).value());
        if (change == CanvasSetupChange::selection)
            for (const auto& group : groups)
                for (const auto& card : group.cards)
                    if (auto state = card.root.snapshot(); state && state->focused)
                        reveal(card.id);
    }
};

ii::core::Result<void> CanvasSetupWindow::attach(ii::gui::Host& host,
                                                 const std::function<void(Scope&)>& configure)
{
    auto appearance = lvui_generated::CanvasSetupView::appearance();
    appearance.window = Color::canvasChooserWindow;
    auto result = host.mount_widgets(
        [&](ii::gui::WidgetBuilder& raw) -> ii::core::Result<void>
        {
            try
            {
                auto root = widgets::take(raw.zstack(
                    {}, "canvasWindow",
                    {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
                auto built = lvui_generated::CanvasSetupView::build(raw, root, configure, &scope_);
                if (!built)
                    return built;
                content_ = std::make_shared<Content>();
                content_->model = std::shared_ptr<CanvasSetupModel>(scope_, &scope_->backend_0);
                content_->build(raw, root);
                checked(raw.retain(content_));
                return {};
            }
            catch (...)
            {
                return ii::core::fail(ii::core::ErrorCode::failed,
                                      "Cannot construct native New canvas window",
                                      std::current_exception());
            }
        },
        appearance);
    if (!result)
    {
        content_.reset();
        scope_.reset();
        return std::unexpected(result.error());
    }
    mount_ = *result;
    content_->redraw();
    checked(content_->root.focus());
    return {};
}
ii::core::Result<void> CanvasSetupWindow::detach(ii::gui::Host& host)
{
    if (mount_.snapshot())
    {
        auto result = host.unmount_widgets(mount_);
        if (!result)
            return result;
    }
    mount_ = {};
    content_.reset();
    scope_.reset();
    return {};
}
} // namespace congregation::native
