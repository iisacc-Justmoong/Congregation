#pragma once
#include <LVRS/Widgets.h>
#include <LVRS/ListModel.h>
#include <iiGUI/Bindings.h>
#include <functional>
#include <charconv>
#include "ToolSettings.h"
#include "TimelineModel.h"

namespace congregation::ui
{
enum class Action
{
    toggle_layer_visibility,
    close_notice,
    open_join,
    general_tab,
    profile_tab,
    members_tab,
    open_avatar,
    restore_license,
    open_shapes,
    size_minus,
    size_plus,
    zoom_tool,
    presentation,
    eyedropper,
    remove_member,
    open_color,
    close_color,
    create_canvas,
    cancel_canvas,
    choose_profile,
    clear_profile,
    diamond,
    star,
    rectanglebubble,
    ellipsebubble,
    new_document,
    open,
    save,
    save_as,
    export_document,
    undo,
    redo,
    preferences,
    add_layer,
    duplicate_layer,
    delete_layer,
    brush,
    eraser,
    line,
    rectangle,
    ellipse,
    triangle,
    fill,
    text,
    image,
    select,
    pan,
    zoom_in,
    zoom_out,
    fit,
    copy,
    paste,
    activate,
    forget_license,
    profile,
    check_update,
    install_update,
    host_session,
    join_session,
    leave_session,
    invite,
    accept_invite,
    reject_invite,
    quit,
    save_tool_settings,
    choose_asset_library,
    insert_asset,
    open_asset_library
};
class EditorBackend final
{
  public:
    EditorBackend();
    std::shared_ptr<native::ToolSettings> parameters = std::make_shared<native::ToolSettings>();
    std::shared_ptr<native::TimelineModel> timeline = std::make_shared<native::TimelineModel>();
    ii::core::Observable<std::string> preset{"Textured oil · Normal"}, selected_tool{"brush"},
        document_info{"1536 × 1024 · RGB / 16-bit · sRGB"}, asset_root{""};
    ii::core::Observable<int> option_tab{0};
    ii::core::Observable<bool> grid_visible{false}, rulers_visible{true};
    std::function<void(std::string)> tool_selected;
    std::function<void(std::filesystem::path)> asset_inserted;
    std::function<void(std::string)> builtin_asset_inserted;
    std::function<void(std::string, double)> layer_opacity_changed;
    std::function<void(std::string)> layer_mode_changed;
    std::function<void(int)> frame_rate_changed;
    std::function<void(std::string)> command;
    void set_parameter(native::ToolParameter, double);
    void set_parameter_normalized(native::ToolParameter, double);
    void step_parameter(native::ToolParameter, int);
    void reset_parameters();
    void sync_parameter_fields();
    void select_tool(std::string);
    void choose_preset();
    void save_parameters() { send(Action::save_tool_settings); }
    std::shared_ptr<const ii::gui::RasterImage> pan_icon, move_icon, add_icon, open_icon, save_icon, zoom_icon, brush_icon, eraser_icon, shape_icon, fill_icon, text_icon, minus_icon, plus_icon;
    ii::core::Observable<std::string> status{"Ready"}, title{"Untitled"}, tool{"Brush"},
        zoom{"100%"}, email{""}, license_key{""}, text_value{"Text"}, session_address{""},
        display_name{"Guest"}, update_status{""}, account_email{"Not connected"}, size_label{"2 px"}, flow_label{"100%"}, opacity_label{"100%"},
        hardness_label{"100%"}, spacing_label{"0.0 px"}, spacing_ratio_label{"15%"}, stabilizer_label{"0%"}, pressure_min_label{"0%"}, pressure_mid_label{"50%"}, pressure_max_label{"100%"};
    ii::core::Observable<bool> licensed{false}, needs_license{true}, can_activate{true},
        start_recent{false}, discover{true}, pressure_enabled{true},
        layer_controls{false}, chrome_visible{true}, general_visible{true}, profile_visible{false}, members_visible{false},
        start_new{true}, can_invite{false};
    ii::core::Observable<double> brush_size{64}, opacity{1}, flow{.9}, hardness{.85}, spacing{.12},
        spacing_pixels{0}, stabilizer{.18}, pressure_min{0}, pressure_mid{.5}, pressure_max{1};
    std::shared_ptr<lvrs::ListModel> layers = std::make_shared<lvrs::ListModel>(),
                                     members = std::make_shared<lvrs::ListModel>();
    std::shared_ptr<ii::gui::ColorPickerModel> color =
        std::make_shared<ii::gui::ColorPickerModel>();
    std::function<void(Action)> perform;
    std::function<void(std::string)> selected, member_selected;
    std::function<void(std::string, std::string)> renamed;
    std::function<void(std::string, std::string, std::string)> moved;
    std::function<void(std::string, bool)> checked;
    std::function<ii::core::Result<bool>(const ii::gui::WidgetInput&)> input;
    std::function<void(ii::gui::Rect)> resized;
    std::function<ii::core::Result<bool>(const ii::gui::WidgetInput&)> pressure_input;
    std::function<void()> preference_changed, name_changed, brush_changed;
    void send(Action value)
    {
        if (perform)
            perform(value);
    }
    void general_tab() { send(Action::general_tab); }
    void profile_tab() { send(Action::profile_tab); }
    void members_tab() { send(Action::members_tab); }
    void restore_license() { send(Action::restore_license); }
    void open_shapes() { send(Action::open_shapes); }
    ii::core::Result<bool> shape_arrow_input(const ii::gui::WidgetInput& e)
    {
        if(e.kind==ii::gui::WidgetInputKind::pointer_up){send(Action::open_shapes);return true;}
        return e.kind==ii::gui::WidgetInputKind::pointer_down;
    }
    void size_minus() { send(Action::size_minus); }
    void size_plus() { send(Action::size_plus); }
    void zoom_tool() { send(Action::zoom_tool); }
    void presentation() { send(Action::presentation); }
    ii::core::Result<bool> profile_input(const ii::gui::WidgetInput& e)
    {
        if(e.kind==ii::gui::WidgetInputKind::pointer_up) { send(Action::open_avatar); return true; }
        return false;
    }
    void eyedropper() { send(Action::eyedropper); }
    void toggle_layer_visibility() { send(Action::toggle_layer_visibility); }
    void close_notice() { send(Action::close_notice); }
    void remove_member() { send(Action::remove_member); }
    void select_member(std::string value)
    {
        if (member_selected)
            member_selected(std::move(value));
    }
    void open_color() { send(Action::open_color); }
    void close_color() { send(Action::close_color); }
    void accept_color(ii::gui::WidgetColor) { send(Action::close_color); }
    void create_canvas() { send(Action::create_canvas); }
    void cancel_canvas() { send(Action::cancel_canvas); }
    void choose_profile() { send(Action::choose_profile); }
    void clear_profile() { send(Action::clear_profile); }
    void diamond() { send(Action::diamond); }
    void star() { send(Action::star); }
    void rectanglebubble() { send(Action::rectanglebubble); }
    void ellipsebubble() { send(Action::ellipsebubble); }
    void new_document() { send(Action::new_document); }
    void open() { send(Action::open); }
    void save() { send(Action::save); }
    void export_document() { send(Action::export_document); }
    void undo() { send(Action::undo); }
    void redo() { send(Action::redo); }
    void preferences() { send(Action::preferences); }
    void add_layer() { send(Action::add_layer); }
    void duplicate_layer() { send(Action::duplicate_layer); }
    void delete_layer() { send(Action::delete_layer); }
    void brush() { send(Action::brush); }
    void eraser() { send(Action::eraser); }
    void line() { send(Action::line); }
    void rectangle() { send(Action::rectangle); }
    void ellipse() { send(Action::ellipse); }
    void triangle() { send(Action::triangle); }
    void fill() { send(Action::fill); }
    void text() { send(Action::text); }
    void image() { send(Action::image); }
    void select() { send(Action::select); }
    void pan() { send(Action::pan); }
    void zoom_in() { send(Action::zoom_in); }
    void zoom_out() { send(Action::zoom_out); }
    void fit() { send(Action::fit); }
    void activate() { send(Action::activate); }
    void forget_license() { send(Action::forget_license); }
    void profile() { send(Action::profile); }
    void check_update() { send(Action::check_update); }
    void install_update() { send(Action::install_update); }
    void host_session() { send(Action::host_session); }
    void join_session() { send(Action::open_join); }
    void join_submit() { send(Action::join_session); }
    void leave_session() { send(Action::leave_session); }
    void invite() { send(Action::invite); }
    void accept_invite() { send(Action::accept_invite); }
    void reject_invite() { send(Action::reject_invite); }
    void quit() { send(Action::quit); }
    void select_layer(std::string key)
    {
        if (selected)
            selected(std::move(key));
    }
    void rename_layer(std::string key, std::string text)
    {
        if (renamed)
            renamed(std::move(key), std::move(text));
    }
    void move_layer(std::string key, std::string parent, std::string before)
    {
        if (moved)
            moved(std::move(key), std::move(parent), std::move(before));
    }
    void check_layer(std::string key, bool value)
    {
        if (checked)
            checked(std::move(key), value);
    }
    ii::core::Result<bool> canvas_input(const ii::gui::WidgetInput& value)
    {
        return input ? input(value) : ii::core::Result<bool>{false};
    }
    void canvas_resize(ii::gui::Rect value)
    {
        if (resized)
            resized(value);
    }
    void set_email(std::string value) { email.set(std::move(value)); }
    void set_license_key(std::string value) { license_key.set(std::move(value)); }
    void set_text(std::string value) { text_value.set(std::move(value)); }
    void set_address(std::string value) { session_address.set(std::move(value)); }
    void set_name(std::string value)
    {
        display_name.set(std::move(value));
        if (name_changed)
            name_changed();
    }
    ii::core::Result<bool> pressure_curve_input(const ii::gui::WidgetInput& e) { return pressure_input?pressure_input(e):ii::core::Result<bool>{false}; }
    void set_spacing_pixels(double value) { spacing_pixels.set(value); if(brush_changed)brush_changed(); }
    void set_flow(double value)
    {
        flow.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_hardness(double value)
    {
        hardness.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_spacing(double value)
    {
        spacing.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_stabilizer(double value)
    {
        stabilizer.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_pressure_enabled(bool value)
    {
        pressure_enabled.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_pressure_min(double value)
    {
        pressure_min.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_pressure_mid(double value)
    {
        pressure_mid.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_pressure_max(double value)
    {
        pressure_max.set(value);
        if (brush_changed)
            brush_changed();
    }
    void set_brush_size(double value) { brush_size.set(value); if (brush_changed) brush_changed(); }
    void set_opacity(double value) { opacity.set(value); if(brush_changed)brush_changed(); }
    void set_start_recent(bool value)
    {
        start_recent.set(value);
        start_new.set(!value);
        if (preference_changed)
            preference_changed();
    }
    void set_start_new(bool value) { if(value) set_start_recent(false); }
    void set_can_invite(bool value) { can_invite.set(value); if(preference_changed) preference_changed(); }
    void set_discover(bool value)
    {
        discover.set(value);
        if (preference_changed)
            preference_changed();
    }
};
} // namespace congregation::ui
