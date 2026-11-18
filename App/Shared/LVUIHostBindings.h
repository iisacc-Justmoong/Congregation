#pragma once
// Product composition of the current LVUI policies with iiGUI native hosts.
#include <LVUI>
#include <iiGUI>
namespace congregation::native::lvui_host {
using Button=lvui::native::ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
using CheckBox=lvui::native::CheckBoxBinding<ii::gui::BasicCheckBox,ii::gui::BasicNativeCheckBox,ii::gui::CachedSvgDecoder>;
using Slider=lvui::native::SliderViewBinding<ii::gui::BasicSlider,ii::gui::BasicNativeSlider,ii::gui::CachedSvgDecoder>;
using InputField=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;
using ComboBox=lvui::native::ComboBoxBinding<ii::gui::BasicComboBox,ii::gui::BasicNativeComboBox,ii::gui::Widget>;
using DropdownButton=lvui::native::DropdownBinding<ii::gui::BasicButton,ii::gui::BasicDropdownButton,ii::gui::BasicNativeDropdownButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
using Menu=lvui::native::MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuItem=lvui::native::MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using Popover=lvui::native::PopoverBinding<ii::gui::BasicPopover,ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativePopover>;
using Event=lvui::native::ButtonEventValue<ii::gui::Widget>;
using SegmentedControl=lvui::native::SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,lvui::native::ButtonMethodValue<Event>,lvui::native::ButtonMethodRegistryValue<Event>,lvui::native::ButtonMethodInvoker>;
using Card=lvui::native::CardBinding<ii::gui::BasicCard,ii::gui::BasicNativeCard,ii::gui::CachedSvgDecoder,ii::gui::WidgetFactory>;
using Material=lvui::native::MaterialBinding<ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativeMaterial,ii::gui::Widget>;
using TextMaterial=ii::gui::BasicTextFieldMaterial<lvui::native::text_field_material::Policy>;
using SearchField=lvui::native::SearchFieldBinding<ii::gui::BasicNativeSearchField,InputField,TextMaterial,ii::gui::CachedSvgDecoder,ii::gui::NativeTextAffinity>;
using ColorSurface=lvui::native::ColorSurfaceBinding<ii::gui::BasicColorSurface,ii::gui::BasicNativeColorSurface>;
using ColorPickerButton=lvui::native::ColorPickerButtonBinding<ii::gui::BasicColorPickerButton,ii::gui::BasicNativeColorPickerButton>;
using ButtonInput=lvui::native::ButtonInputValue<ii::gui::RasterImage>;
using DropdownInput=lvui::native::DropdownInputValue<ii::gui::RasterImage>;
using MenuItemInput=lvui::native::MenuItemInputValue<ii::gui::RasterImage>;
using CardInput=lvui::native::CardInputValue<ii::gui::RasterImage>;
}
