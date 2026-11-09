#include "OwnedWidgetStyle.h"
#include "EditorLayout.h"
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/NativeIconView.h>

namespace congregation::native::owned_widgets {
namespace {
struct IconSourceAdapter final : ii::gui::ButtonIconSource {
    std::uint64_t revision() const noexcept override { return lvui::design_icon_catalog()->revision(); }
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name, double size) const override {
        return lvui::native::icon_image_for_logical_size<ii::gui::CachedSvgDecoder>(lvui::design_icon_catalog(), name, size);
    }
};
ii::runtime::Task button_action(ii::runtime::Task action) {
    return action ? std::move(action) : ii::runtime::Task{[] {}};
}
}
void borders(Widget widget,bool top,bool right,bool bottom,bool left) {
    good(widget.set_layout_handler([widget,top,right,bottom,left](ii::gui::Rect bounds) {
        ii::gui::WidgetDrawing drawing;
        const auto color=editor::linear(editor::panel_edge_color());
        if(bounds.width>0&&bounds.height>0) {
            if(top)drawing.panels.push_back({{0,0,bounds.width,Metric::stroke1},color,color});
            if(right)drawing.panels.push_back({{bounds.width-Metric::stroke1,0,Metric::stroke1,bounds.height},color,color});
            if(bottom)drawing.panels.push_back({{0,bounds.height-Metric::stroke1,bounds.width,Metric::stroke1},color,color});
            if(left)drawing.panels.push_back({{0,0,Metric::stroke1,bounds.height},color,color});
        }
        auto result=widget.set_drawing(std::move(drawing));
        if(!result&&result.error().code!=ii::core::ErrorCode::closed)good(std::move(result));
    }));
}
ii::core::Result<Widget> Controls::event_listener(Widget parent,std::string_view id,ii::gui::Size size,
                                                   ii::gui::WidgetInputHandler input,
                                                   std::function<void(ii::gui::Rect)> resized) {
    auto control=ii::gui::NativeEventListener::create(raw_,parent,id,{size.width,size.height},
        {.input=std::move(input),.resized=std::move(resized)});
    if(!control)return std::unexpected(control.error());return control->root();
}
ii::core::Result<InputHandle> Controls::input_field(Widget parent,std::string_view id,ii::gui::InputOptions value,
                                                    std::function<void(std::string)> edited,
                                                    ii::runtime::Task submitted) {
    if(value.style)
        return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned InputField does not accept custom styles");
    lvui::InputFieldConfiguration options;
    lvui::apply_input_field_size(options,lvui::InputFieldSize::compact);
    options.surface=lvui::InputFieldSurface::rounded;
    options.clear_button_visible=true;
    if(value.maximum_length)options.maximum_length=value.maximum_length;
    options.length_unit=lvui::TextLengthUnit::unicode_scalars;
    options.text=std::move(value.text);options.placeholder=std::move(value.placeholder);
    options.enabled=value.enabled;options.read_only=value.read_only;options.width=value.width;
    options.multiline=value.multiline;options.password=value.password;
    if(!value.accessibility_name.empty())options.accessibility_name=std::move(value.accessibility_name);
    auto on_focus=[commit=value.commit_on_focus_loss,submitted](bool focused){if(commit&&!focused&&submitted)submitted();};
    auto control=lvui_host::InputField::create(raw_,parent,id,options,
        {.edited=std::move(edited),.submitted=std::move(submitted),.focus_changed=std::move(on_focus)});
    if(!control)return std::unexpected(control.error());return InputHandle(std::move(*control));
}
ii::core::Result<Widget> Controls::popover(Widget parent,std::string_view id,ii::gui::PopoverOptions value) {
    lvui::PopoverConfiguration options;options.width=value.width;options.height=value.height;options.padding=value.padding;
    options.margins=value.margins;options.corner_radius=value.corner_radius;options.surface_opacity=value.surface_opacity;
    if(value.surface_color)options.surface_color=lvui::native::popover_color(*value.surface_color);
    options.modal=value.modal;options.dim=value.dim;options.focus_on_open=value.focus_on_open;options.clip_content=value.clip_content;
    options.dismiss_on_escape=value.dismiss_on_escape;options.dismiss_on_outside_press=value.dismiss_on_outside_press;
    options.dismiss_on_context_request=value.dismiss_on_context_request;options.accessibility_name=std::move(value.accessibility_name);
    options.placement=value.placement==ii::gui::PopoverPlacement::centered?lvui::PopoverPlacement::centered:lvui::PopoverPlacement::anchored;
    auto control=lvui_host::Popover::create(raw_,parent,id,std::move(options));
    if(!control)return std::unexpected(control.error());return control->root();
}
ii::core::Result<ButtonHandle> Controls::button(Widget parent,std::string_view id,ii::gui::ButtonOptions value,
                                                 ii::runtime::Task clicked) {
    if(value.indicator||value.disabled_indicator||value.background_image||value.style)
        return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned Button does not accept indicator or background images or custom styles");
    lvui_host::ButtonInput input;auto& options=input.configuration;
    options.text=std::move(value.text);options.enabled=value.enabled;options.tone=lvui::native::button_tone(value.tone);
    options.accessibility_name=std::move(value.accessibility_name);options.icon_mode=value.icon_mode;options.icon_size=value.icon_size;
    if(value.width)options.width=*value.width;if(value.height)options.height=*value.height;
    if(value.horizontal_padding)options.horizontal_padding=*value.horizontal_padding;if(value.corner_radius)options.corner_radius=*value.corner_radius;
    options.checkable=value.checkable;options.checked=value.checked;input.icon=value.icon;input.icon_name=std::move(value.icon_name);
    auto control=lvui_host::Button::create(raw_,parent,id,std::move(input),button_action(std::move(clicked)));
    if(!control)return std::unexpected(control.error());return ButtonHandle(std::move(*control));
}
ii::core::Result<DropdownHandle> Controls::dropdown_button(Widget parent,std::string_view id,ii::gui::ButtonOptions value,
                                                          ii::runtime::Task clicked) {
    if(value.indicator||value.disabled_indicator||value.background_image||value.style)
        return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned Dropdown does not accept custom raster or style inputs");
    lvui_host::DropdownInput input;auto& options=input.configuration;
    options.text=std::move(value.text);options.accessibility_name=std::move(value.accessibility_name);
    options.enabled=value.enabled;options.icon_mode=value.icon_mode;options.tone=lvui::native::dropdown_tone(value.tone);
    options.icon_size=value.icon_size;if(value.width)options.width=*value.width;if(value.height)options.height=*value.height;
    if(value.corner_radius)options.corner_radius=*value.corner_radius;input.icon=value.icon;input.icon_name=std::move(value.icon_name);
    auto control=lvui_host::DropdownButton::create(raw_,parent,id,std::move(input),{.clicked=std::move(clicked)});
    if(!control)return std::unexpected(control.error());return DropdownHandle(std::move(*control));
}
ii::core::Result<MenuHandle> Controls::context_menu(Widget parent,std::string_view id,ii::gui::MenuOptions value) {
    lvui::native::MenuInput input;auto& options=input.configuration;
    if(value.item_width>0)options.item_width=value.item_width;options.item_spacing=value.item_spacing;
    options.left_padding=value.left_padding;options.right_padding=value.right_padding;
    if(value.top_padding)options.top_padding=*value.top_padding;if(value.bottom_padding)options.bottom_padding=*value.bottom_padding;
    options.edge_margin=value.edge_margin;options.corner_radius=value.corner_radius;options.opacity=value.opacity;
    options.auto_close_on_trigger=value.auto_close_on_trigger;options.dismiss_outside=value.dismiss_on_outside_press;
    options.dismiss_context=value.dismiss_on_context_request;options.compact_items=value.compact_items.value_or(true);
    options.show_icon_slot=false;if(value.background)options.background=lvui::native::menu_color(*value.background);
    input.selected_index=value.selected_index;
    auto control=lvui_host::Menu::create(raw_,parent,id,std::move(input));
    if(!control)return std::unexpected(control.error());return MenuHandle(std::move(*control));
}
ii::core::Result<Widget> MenuHandle::add(ii::gui::WidgetBuilder& builder,std::string_view id,ii::gui::MenuItemOptions value,
                                         ii::runtime::Task clicked)const {
    if(value.icon||value.chevron_right||value.chevron_left||value.chevron_up||value.chevron_down)
        return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned menu adapter does not accept raster indicators");
    lvui_host::MenuItemInput input;auto& options=input.configuration;
    options.label=std::move(value.label);if(value.shortcut)options.shortcut=*value.shortcut;
    options.key_placeholder=std::move(value.key_placeholder);options.icon_name=std::move(value.icon_name);
    options.enabled=value.enabled;options.compact=value.compact.value_or(true);
    options.state=lvui::native::menu_item_state(value.state);options.direction=lvui::native::menu_item_direction(value.direction);
    options.key_visible=value.key_visible;options.show_icon_slot=value.show_icon_slot;options.show_chevron=value.show_chevron;
    options.has_children=value.has_child_items;options.expanded=value.expanded;options.width=value.item_width;options.height=value.item_height;
    options.icon_size=value.icon_size;options.chevron_size=value.chevron_size;input.close_on_trigger=value.close_on_trigger;
    auto item=lvui_host::MenuItem::create(builder,value_.content_root(),id,std::move(input),{.clicked=std::move(clicked)});
    if(!item)return std::unexpected(item.error());if(auto result=value_.register_item(item->weak_group_port());!result)return std::unexpected(result.error());
    return item->root();
}
ii::core::Result<CheckHandle> Controls::check_box(Widget parent,std::string_view id,ii::gui::CheckOptions value,
                                                   std::function<void(bool)> changed) {
    if(!value.group.empty())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"CheckBox does not accept a radio group");
    lvui::CheckBoxConfiguration options;options.text=std::move(value.text);options.enabled=value.enabled;options.checked=value.checked;
    auto control=lvui_host::CheckBox::create(raw_,parent,id,options,std::move(changed));
    if(!control)return std::unexpected(control.error());return CheckHandle(std::move(*control));
}
ii::core::Result<SliderHandle> Controls::slider(Widget parent,std::string_view id,SliderSpec value,
                                                 std::function<void(double)> changed) {
    lvui::SliderConfiguration options;
    try{lvui::apply_slider_size(options,value.size);}
    catch(const std::invalid_argument&){return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Slider size");}
    options.minimum=value.minimum;options.maximum=value.maximum;options.value=value.value;
    options.step=value.step;options.width=value.width;options.enabled=value.enabled;
    if(!value.accessibility_name.empty())options.accessibility_name=std::move(value.accessibility_name);
    auto control=lvui_host::Slider::create(raw_,parent,id,options,std::move(changed));
    if(!control)return std::unexpected(control.error());return SliderHandle(std::move(*control));
}
ii::core::Result<ComboHandle> Controls::combo_box(Widget parent,std::string_view id,ii::gui::ComboBoxOptions value,
                                                  ii::gui::ControlEvents events) {
    if(value.indicator)return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned ComboBox has no raster indicator input");
    if(value.tone!=ii::gui::ButtonTone::primary&&value.tone!=ii::gui::ButtonTone::borderless)
        return ii::core::fail(ii::core::ErrorCode::unsupported,"Owned ComboBox does not support this tone");
    lvui::ComboBoxConfiguration options;options.text=std::move(value.text);options.width=value.width;options.enabled=value.enabled;
    options.tone=value.tone==ii::gui::ButtonTone::borderless?lvui::ButtonTone::borderless:lvui::ButtonTone::primary;
    options.arrow=lvui::native::combo_box_arrow(value.arrow);
    auto control=lvui_host::ComboBox::create(raw_,parent,id,options,
        {.clicked=std::move(events.clicked),.pressed=std::move(events.pressed),
         .released=std::move(events.released),.canceled=std::move(events.canceled)});
    if(!control)return std::unexpected(control.error());return ComboHandle(std::move(*control));
}
ii::core::Result<Widget> Controls::icon_segmented_control(Widget parent,std::string_view id,bool force_borderless_tone,bool vertical) {
    auto options=lvui::icon_segmented_configuration();
    options.orientation=vertical?lvui::SegmentedOrientation::vertical:lvui::SegmentedOrientation::horizontal;
    options.force_borderless_tone=force_borderless_tone;
    auto control=lvui_host::SegmentedControl::create(raw_,parent,id,options);
    if(!control)return std::unexpected(control.error());return control->root();
}
ii::core::Result<Widget> Controls::label_segmented_control(Widget parent,std::string_view id,bool force_borderless_tone) {
    lvui::SegmentedControlConfiguration options;
    options.force_borderless_tone=force_borderless_tone;
    auto control=lvui_host::SegmentedControl::create(raw_,parent,id,options);
    if(!control)return std::unexpected(control.error());return control->root();
}
std::string Builder::id(std::string value){return value.empty()?prefix_+std::to_string(next_++):value;}
Widget Builder::column(Widget parent,std::string name,double width,double height,double gap,double padding) {
    auto node=take(raw.vstack(parent,id(std::move(name)),{.spacing=gap,.padding=padding,.alignment=ii::gui::StackAlignment::stretch}));
    good(node.set_extent({width,height}));return node;
}
Widget Builder::row(Widget parent,std::string name,double width,double height,double gap,double padding) {
    auto node=take(raw.hstack(parent,id(std::move(name)),{.spacing=gap,.padding=padding,.alignment=ii::gui::StackAlignment::center}));
    good(node.set_extent({width,height}));return node;
}
Widget Builder::label(Widget parent,std::string text,double size,std::string name,unsigned weight) {
    return take(raw.label(parent,id(std::move(name)),{.text=std::move(text),.font_size=size,.selectable=false,
        .role=size==Metric::textCaption?ii::gui::TextRole::caption:size==Metric::textHeader?ii::gui::TextRole::header:ii::gui::TextRole::body,
        .color=ink,.line_height=size,.font_weight=weight,.elide_right=true}));
}
ButtonHandle Builder::icon(Widget parent,std::string name,std::string source,ii::runtime::Task action) {
    lvui_host::ButtonInput options;options.configuration=lvui::icon_button_configuration();
    options.configuration.tone=lvui::ButtonTone::borderless;
    options.configuration.accessibility_name=source;
    options.icon_name=std::move(source);options.icon_source=lvui::design_icon_catalog();
    return ButtonHandle(take(lvui_host::Button::create(raw,parent,id(std::move(name)),options,button_action(std::move(action)))));
}
ButtonHandle Builder::button(Widget parent,std::string name,std::string text,ii::runtime::Task action,double width,
                              ii::gui::ButtonTone tone) {
    lvui_host::ButtonInput options;options.configuration.text=text;options.configuration.accessibility_name=std::move(text);
    options.configuration.tone=lvui::native::button_tone(tone);options.configuration.height=Metric::size22;
    if(width>0)options.configuration.width=width;
    return ButtonHandle(take(lvui_host::Button::create(raw,parent,id(std::move(name)),options,button_action(std::move(action)))));
}
Widget Builder::glyph(Widget parent,std::string source) {
    return take(ii::gui::IconView::create(raw,parent,id(),{.name=std::move(source),.source=std::make_shared<IconSourceAdapter>()})).root();
}
Widget Builder::choice(Widget parent,std::string name,std::string text,std::string source,ii::runtime::Task action) {
    auto root=take(raw.action_surface(parent,id(std::move(name)),
        {.accessibility_name=text,.minimum_height=Metric::size22,.height=Metric::size22},std::move(action)));
    auto content=row(root,{},0,Metric::size22,Metric::space4);glyph(content,std::move(source));
    auto title=label(content,std::move(text));good(title.set_flex(1));return root;
}
Widget Builder::space(Widget parent,double minimum){return take(raw.spacer(parent,id(),minimum));}
} // namespace congregation::native::owned_widgets
