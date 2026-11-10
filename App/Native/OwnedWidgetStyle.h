#pragma once

#include "EditorPalette.h"
#include "Style.h"
#include <LVUI/NativeButton.h>
#include <LVUI/NativeCheckBox.h>
#include <LVUI/NativeComboBox.h>
#include <LVUI/NativeDropdownButton.h>
#include "LVUIHostBindings.h"
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeMenu.h>
#include <LVUI/NativePopover.h>
#include <LVUI/NativeSegmentedControl.h>
#include <LVUI/NativeSlider.h>
#include <iiGUI/Bindings.h>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace congregation::native::owned_widgets {
using Widget=ii::gui::Widget;
template<class T> T take(ii::core::Result<T> result) {
    if(!result) {
        if(result.error().cause)std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
inline void good(ii::core::Result<void> result) {
    if(!result) {
        if(result.error().cause)std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
inline constexpr ii::gui::WidgetColor ink=Color::textBody;
void borders(Widget,bool top=false,bool right=false,bool bottom=false,bool left=false);

class ButtonHandle final {
    lvui_host::Button value_;
public:
    explicit ButtonHandle(lvui_host::Button value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.text=std::move(text);return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_accessibility_name(std::string name)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.accessibility_name=std::move(name);return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_enabled(bool enabled)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.enabled=enabled;return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_segment_selected(bool selected)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.checkable=true;
        options->configuration.checked=selected;
        options->configuration.selection_style=lvui::ButtonSelectionStyle::segmented;
        return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_tone(ii::gui::ButtonTone tone)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.tone=lvui::native::button_tone(tone);return value_.set_options(*options);
    }
};
class CheckHandle final {
    lvui_host::CheckBox value_;
public:
    explicit CheckHandle(lvui_host::CheckBox value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_checked(bool checked)const{return value_.set_checked(checked);}
    [[nodiscard]] ii::core::Result<void> set_enabled(bool enabled)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->enabled=enabled;return value_.set_options(*options);
    }
};
struct SliderSpec final {
    double minimum=0,maximum=1,value=.5,step=0,width=lvui::SliderConfiguration{}.width;
    bool enabled=true;
    std::string accessibility_name;
    lvui::SliderSize size=lvui::SliderSize::regular;
};
class SliderHandle final {
    lvui_host::Slider value_;
public:
    explicit SliderHandle(lvui_host::Slider value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_value(double value)const{return value_.set_value(value);}
    [[nodiscard]] ii::core::Result<void> set_flex(double value)const{return root().set_flex(value);}
    [[nodiscard]] ii::core::Result<void> set_enabled(bool enabled)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->enabled=enabled;return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_layout_handler(std::function<void(ii::gui::Rect)> handler)const {
        return root().set_layout_handler(std::move(handler));
    }
};
class InputHandle final {
    lvui_host::InputField value_;
public:
    explicit InputHandle(lvui_host::InputField value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const{return value_.set_text(std::move(text));}
    [[nodiscard]] ii::core::Result<void> set_enabled(bool enabled)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->enabled=enabled;return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> focus()const{return root().focus();}
};
class ComboHandle final {
    lvui_host::ComboBox value_;
public:
    explicit ComboHandle(lvui_host::ComboBox value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->text=std::move(text);return value_.set_options(*options);
    }
    [[nodiscard]] ii::core::Result<void> set_enabled(bool enabled)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->enabled=enabled;return value_.set_options(*options);
    }
};
class DropdownHandle final {
    lvui_host::DropdownButton value_;
public:
    explicit DropdownHandle(lvui_host::DropdownButton value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const {
        auto options=value_.options();if(!options)return std::unexpected(options.error());
        options->configuration.text=std::move(text);return value_.set_options(*options);
    }
};
class MenuHandle final {
    lvui_host::Menu value_;
public:
    explicit MenuHandle(lvui_host::Menu value):value_(std::move(value)){}
    [[nodiscard]] Widget root()const{return value_.root();}
    operator Widget()const{return root();}
    [[nodiscard]] ii::core::Result<Widget> add(ii::gui::WidgetBuilder&,std::string_view,ii::gui::MenuItemOptions,ii::runtime::Task)const;
    [[nodiscard]] ii::core::Result<void> finalize()const{return value_.finalize();}
};
inline Widget control_widget(Widget widget) { return widget; }
template<class T> Widget control_widget(const T& handle) { return handle.root(); }
template<class T>
ii::core::Result<void> bind_checked(ii::gui::BindingScope& bindings,CheckHandle handle,
                                   ii::core::Observable<T>& source) {
    return bindings.bind(handle.root(),source,[handle](Widget,const T& value){return handle.set_checked(bool(value));});
}
template<class T>
ii::core::Result<void> bind_text(ii::gui::BindingScope& bindings,ComboHandle handle,
                                ii::core::Observable<T>& source) {
    return bindings.bind(handle.root(),source,[handle](Widget,const T& value){return handle.set_text(value);});
}

// Transitional C++ composition boundary for product-owned views. Controls
// retain LVUI's state and paint providers; iiGUI supplies generic surfaces.
class Controls final {
    ii::gui::WidgetBuilder& raw_;
public:
    explicit Controls(ii::gui::WidgetBuilder& raw):raw_(raw){}
    [[nodiscard]] ii::core::Result<Widget> vstack(Widget parent,std::string_view id,ii::gui::StackOptions options={}) {
        return raw_.vstack(parent,id,std::move(options));
    }
    [[nodiscard]] ii::core::Result<Widget> hstack(Widget parent,std::string_view id,ii::gui::StackOptions options={}) {
        return raw_.hstack(parent,id,std::move(options));
    }
    [[nodiscard]] ii::core::Result<Widget> zstack(Widget parent,std::string_view id,ii::gui::StackOptions options={}) {
        return raw_.zstack(parent,id,std::move(options));
    }
    [[nodiscard]] ii::core::Result<Widget> event_listener(Widget,std::string_view,ii::gui::Size,
        ii::gui::WidgetInputHandler = {},std::function<void(ii::gui::Rect)> = {});
    [[nodiscard]] ii::core::Result<InputHandle> input_field(Widget,std::string_view,ii::gui::InputOptions,
        std::function<void(std::string)> = {},ii::runtime::Task = {});
    [[nodiscard]] ii::core::Result<Widget> popover(Widget,std::string_view,ii::gui::PopoverOptions);
    [[nodiscard]] ii::core::Result<Widget> label(Widget parent,std::string_view id,ii::gui::LabelOptions options={}) {
        return raw_.label(parent,id,std::move(options));
    }
    [[nodiscard]] ii::core::Result<ButtonHandle> button(Widget,std::string_view,ii::gui::ButtonOptions,ii::runtime::Task={});
    [[nodiscard]] ii::core::Result<DropdownHandle> dropdown_button(Widget,std::string_view,ii::gui::ButtonOptions,ii::runtime::Task={});
    [[nodiscard]] ii::core::Result<MenuHandle> context_menu(Widget,std::string_view,ii::gui::MenuOptions={});
    [[nodiscard]] ii::core::Result<Widget> context_menu_item(const MenuHandle& menu,std::string_view id,
                                                              ii::gui::MenuItemOptions options,ii::runtime::Task action={}) {
        return menu.add(raw_,id,std::move(options),std::move(action));
    }
    [[nodiscard]] ii::core::Result<CheckHandle> check_box(Widget,std::string_view,ii::gui::CheckOptions,
                                                         std::function<void(bool)> = {});
    [[nodiscard]] ii::core::Result<SliderHandle> slider(Widget,std::string_view,SliderSpec,
                                                       std::function<void(double)> = {});
    [[nodiscard]] ii::core::Result<ComboHandle> combo_box(Widget,std::string_view,ii::gui::ComboBoxOptions,
                                                         ii::gui::ControlEvents = {});
    [[nodiscard]] ii::core::Result<Widget> icon_segmented_control(Widget,std::string_view,
                                                                  bool force_borderless_tone=true,
                                                                  bool vertical=false);
    [[nodiscard]] ii::core::Result<Widget> label_segmented_control(Widget,std::string_view,
                                                                   bool force_borderless_tone=true);
};
class Builder final {
    std::string prefix_;
    std::size_t next_=0;
public:
    explicit Builder(ii::gui::WidgetBuilder& raw,std::string prefix="ui/")
        :raw(raw),controls(raw),prefix_(std::move(prefix)){}
    [[nodiscard]] std::string id(std::string value={});
    Widget column(Widget,std::string id={},double width=0,double height=0,double gap=Metric::space0,double padding=Metric::space0);
    Widget row(Widget,std::string id={},double width=0,double height=Metric::size22,double gap=Metric::space4,double padding=Metric::space0);
    Widget label(Widget,std::string text,double size=Metric::textBody,std::string id={},unsigned weight=Metric::weightMedium);
    ButtonHandle icon(Widget,std::string id,std::string name,ii::runtime::Task={});
    ButtonHandle button(Widget,std::string id,std::string text,ii::runtime::Task={},double width=0,
                        ii::gui::ButtonTone=ii::gui::ButtonTone::normal);
    Widget glyph(Widget,std::string name);
    Widget choice(Widget,std::string id,std::string text,std::string icon,ii::runtime::Task);
    Widget space(Widget,double minimum=Metric::space0);
    ii::gui::WidgetBuilder& raw;
    Controls controls;
};
} // namespace congregation::native::owned_widgets
