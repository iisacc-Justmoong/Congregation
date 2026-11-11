#pragma once
#include "GradientSettings.h"
#include "EditorColor.h"
#include "PresentationList.h"
#include <LVUI/Button.h>
#include <LVUI/IconButtonDefaults.generated.h>
#include <iiGUI/Bindings.h>
#include <iiGUI/Image.h>
#include <functional>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <utility>
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
    clear_selection,
    invert_selection,
    apply_crop,
    cancel_crop,
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
    cancel_sign_in,
    open_society,
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
    choose_texture,
    insert_asset,
    open_asset_library,
    window_close,
    window_minimize,
    window_toggle_maximize
};
class EditorBackend
{
  public:
    EditorBackend();
    virtual ~EditorBackend() { lifetime_.reset(); }
    std::shared_ptr<native::ToolSettings> parameters = std::make_shared<native::ToolSettings>();
    std::shared_ptr<native::TimelineModel> timeline = std::make_shared<native::TimelineModel>();
    ii::core::Observable<double> tool_button_extent{lvui::icon_button_defaults::extent};
    ii::core::Observable<lvui::ButtonTone> active_tool_tone{lvui::ButtonTone::normal};
    ii::core::Observable<lvui::ButtonTone> inactive_tool_tone{lvui::ButtonTone::borderless};
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
    std::function<ii::core::Result<void>()> titlebar_drag;
    void set_parameter(native::ToolParameter, double);
    void set_parameter_normalized(native::ToolParameter, double);
    void step_parameter(native::ToolParameter, int);
    void reset_parameters();
    void sync_parameter_fields();
    void select_tool(std::string);
    void choose_tool_move() { select_tool("move"); }
    void choose_tool_rect_marquee() { select_tool("rect-marquee"); }
    void choose_tool_ellipse_marquee() { select_tool("ellipse-marquee"); }
    void choose_tool_lasso() { select_tool("lasso"); }
    void choose_tool_polygon_lasso() { select_tool("polygon-lasso"); }
    void choose_tool_magnetic_lasso() { select_tool("magnetic-lasso"); }
    void choose_tool_magic_wand() { select_tool("magic-wand"); }
    void choose_tool_crop() { select_tool("crop"); }
    void choose_tool_perspective_crop() { select_tool("perspective-crop"); }
    void choose_tool_color_picker() { select_tool("color-picker"); }
    void choose_tool_brush() { select_tool("brush"); }
    void choose_tool_pencil() { select_tool("pencil"); }
    void choose_tool_airbrush() { select_tool("airbrush"); }
    void choose_tool_mixer_brush() { select_tool("mixer-brush"); }
    void choose_tool_dry_brush() { select_tool("dry-brush"); }
    void choose_tool_bristle_brush() { select_tool("bristle-brush"); }
    void choose_tool_eraser() { select_tool("eraser"); }
    void choose_tool_gradient() { select_tool("gradient"); }
    void choose_tool_paint_bucket() { select_tool("paint-bucket"); }
    void choose_tool_smudge() { select_tool("smudge"); }
    void choose_tool_clone_stamp() { select_tool("clone-stamp"); }
    void choose_tool_pattern_stamp() { select_tool("pattern-stamp"); }
    void choose_tool_healing_brush() { select_tool("healing-brush"); }
    void choose_tool_spot_heal() { select_tool("spot-heal"); }
    void choose_tool_blur() { select_tool("blur"); }
    void choose_tool_sharpen() { select_tool("sharpen"); }
    void choose_tool_dodge() { select_tool("dodge"); }
    void choose_tool_burn() { select_tool("burn"); }
    void choose_tool_sponge() { select_tool("sponge"); }
    void choose_tool_liquify() { select_tool("liquify"); }
    void choose_tool_warp() { select_tool("warp"); }
    void choose_tool_pen() { select_tool("pen"); }
    void choose_tool_curvature_pen() { select_tool("curvature-pen"); }
    void choose_tool_direct_select() { select_tool("direct-select"); }
    void choose_tool_add_anchor() { select_tool("add-anchor"); }
    void choose_tool_delete_anchor() { select_tool("delete-anchor"); }
    void choose_tool_rectangle() { select_tool("rectangle"); }
    void choose_tool_ellipse() { select_tool("ellipse"); }
    void choose_tool_polygon() { select_tool("polygon"); }
    void choose_tool_area_text() { select_tool("area-text"); }
    void choose_tool_path_text() { select_tool("path-text"); }
    void choose_tool_hand() { select_tool("hand"); }
    void choose_tool_rotate_view() { select_tool("rotate-view"); }
    void choose_tool_quick_mask() { select_tool("quick-mask"); }
    void choose_tool_imagezoomIn() { select_tool("imagezoomIn"); }
    void choose_tool_rulers() { select_tool("rulers"); }
    void choose_preset();
    void save_parameters() { send(Action::save_tool_settings); }
    ii::core::Observable<std::string> status{"Ready"}, title{"Untitled"}, tool{"Brush"},
        zoom{"100%"}, account_status{"Open Society to check your Congregation license."}, text_value{"Text"}, session_address{""},
        display_name{"Guest"}, update_status{""}, account_email{"Not connected"}, agent_prompt{""},
        account_title{"Connect to Society"}, account_description{"Sign in to Society to verify your Congregation license."},
        account_button{"Open Society"}, account_guide{"Society verifies your iisacc account and license."},
        size_label{"2 px"}, flow_label{"100%"}, opacity_label{"100%"},
        hardness_label{"100%"}, spacing_label{"0.0 px"}, spacing_ratio_label{"15%"}, stabilizer_label{"0%"}, pressure_min_label{"0%"}, pressure_mid_label{"50%"}, pressure_max_label{"100%"};
    ii::core::Observable<bool> licensed{false}, workspace_visible{true}, needs_license{false}, can_activate{true}, signed_in{false}, signing_in{false},
        start_recent{false}, discover{true}, pressure_enabled{true},
        layer_controls{false}, chrome_visible{true}, custom_titlebar_visible{false}, native_titlebar_controls{false}, client_titlebar_controls{false}, general_visible{true}, profile_visible{false}, members_visible{false},
        start_new{true}, can_invite{false}, wand_contiguous{true}, wand_current_layer{false}, clone_aligned{true}, clone_current_layer{true},
        fill_contiguous{true}, fill_current_layer{false}, fill_target_current{false}, fill_preserve_alpha{false}, fill_antialias{false}, quick_mask_mode{false}, selection_antialias{true}, crop_destructive{false}, polygon_center{false}, polygon_fill{true}, polygon_outline{false};
    ii::core::Observable<double> brush_size{64}, opacity{1}, flow{.9}, hardness{.85}, spacing{.12},
        spacing_pixels{0}, stabilizer{.18}, pressure_min{0}, pressure_mid{.5}, pressure_max{1}, wand_tolerance{0}, fill_tolerance{0}, selection_feather{0}, magnetic_radius{12}, polygon_rotation{0}, polygon_stroke_width{1}, polygon_opacity{1};
    ii::core::Observable<double> brush_size_limit{native::ToolSettings::spec(native::ToolParameter::size).maximum};
    std::shared_ptr<native::GradientSettings> gradient_settings = std::make_shared<native::GradientSettings>();
    // LVUI consumes these independently of the presentation's widget library.
    std::shared_ptr<PresentationList> layer_source, member_source;
    ii::core::Observable<std::int64_t> layer_revision{0}, member_revision{0}, crop_aspect{0}, polygon_sides{5};
    ii::core::Observable<ii::gui::WidgetColor> current_color{{1, 1, 1, 1}};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> profile_avatar{};
    ii::core::Observable<std::shared_ptr<lvui::ColorSurfaceSource>> color_source{};
    ii::core::Observable<std::int64_t> color_revision{0};
    std::shared_ptr<EditorColor> color = std::make_shared<EditorColor>();
    void set_presentation_lists(std::shared_ptr<PresentationList> layers,
                                std::shared_ptr<PresentationList> members);
    void set_color_model(std::shared_ptr<EditorColor>);
    ii::core::Result<void> set_current_color(ii::gui::WidgetColor value) {
        auto model = color;
        return model->set_color(value);
    }
    void color_edited(ii::gui::WidgetColor) {
        auto model = color;
        if (auto result = model->publish_surface_edit(); !result)
            throw std::runtime_error(result.error().message);
    }
    void activate_layer(std::size_t, const lvui::ListEntry& value) { select_layer(value.key); }
    void activate_member(std::size_t, const lvui::ListEntry& value) { select_member(value.key); }
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
        invoke(perform, value);
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
        invoke(member_selected, std::move(value));
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
    void align_left() { invoke(command, std::string{"align-left"}); }
    void align_center() { invoke(command, std::string{"align-center"}); }
    void align_right() { invoke(command, std::string{"align-right"}); }
    void align_top() { invoke(command, std::string{"align-top"}); }
    void align_middle() { invoke(command, std::string{"align-middle"}); }
    void align_bottom() { invoke(command, std::string{"align-bottom"}); }
    void distribute_horizontal() { invoke(command, std::string{"distribute-horizontal"}); }
    void distribute_vertical() { invoke(command, std::string{"distribute-vertical"}); }
    void flip_horizontal() { invoke(command, std::string{"flip-horizontal"}); }
    void flip_vertical() { invoke(command, std::string{"flip-vertical"}); }
    void rotate_left() { invoke(command, std::string{"rotate-left"}); }
    void rotate_right() { invoke(command, std::string{"rotate-right"}); }
    void perspective_grid() { invoke(command, std::string{"perspective-grid"}); }
    void symmetry_paint() { invoke(command, std::string{"symmetry-paint"}); }
    void toggle_grid() { invoke(command, std::string{"grid"}); }
    void toggle_rulers() { invoke(command, std::string{"rulers"}); }
    void toggle_snap() { invoke(command, std::string{"snap"}); }
    void show_document_options() { option_tab.set(2); }
    [[nodiscard]] std::string timeline_status(const native::TimelinePosition& position) const {
        const auto seconds = position.rate > 0 ? double(position.frame) / position.rate : 0.0;
        return native::TimelineModel::format_time(seconds) + " / " + native::TimelineModel::format_time(position.duration);
    }
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
    void open_society() { send(Action::open_society); }
    void cancel_sign_in() { send(Action::cancel_sign_in); }
    void profile() { send(Action::profile); }
    void titlebar_close() { send(Action::window_close); }
    void titlebar_minimize() { send(Action::window_minimize); }
    void titlebar_maximize() { send(Action::window_toggle_maximize); }
    ii::core::Result<bool> titlebar_input(const ii::gui::WidgetInput& event)
    {
        if(event.kind!=ii::gui::WidgetInputKind::pointer_down || event.device!=ii::gui::PointerDevice::mouse || !(event.buttons&1))
            return false;
        auto callback=titlebar_drag;
        if(!callback)return false;
        if(auto result=callback();!result)return std::unexpected(result.error());
        return true;
    }
    ii::core::Result<bool> titlebar_profile_input(const ii::gui::WidgetInput& event)
    {
        if(event.kind==ii::gui::WidgetInputKind::pointer_up) { profile(); return true; }
        return event.kind==ii::gui::WidgetInputKind::pointer_down;
    }
    void set_agent_prompt(std::string value) { agent_prompt.set(std::move(value)); }
    void submit_agent() { status.set("Agent is not available yet."); }
    void show_notifications() { status.set("Notifications are not available yet."); }
    void show_learning() { status.set("Learning resources are not available yet."); }
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
        invoke(selected, std::move(key));
    }
    void rename_layer(std::string key, std::string text)
    {
        invoke(renamed, std::move(key), std::move(text));
    }
    void move_layer(std::string key, std::string parent, std::string before)
    {
        invoke(moved, std::move(key), std::move(parent), std::move(before));
    }
    void check_layer(std::string key, bool value)
    {
        invoke(checked, std::move(key), value);
    }
    ii::core::Result<bool> canvas_input(const ii::gui::WidgetInput& value)
    {
        auto callback = input;
        return callback ? callback(value) : ii::core::Result<bool>{false};
    }
    void canvas_resize(ii::gui::Rect value)
    {
        invoke(resized, value);
    }
    void set_text(std::string value) { text_value.set(std::move(value)); }
    void set_address(std::string value) { session_address.set(std::move(value)); }
    void set_name(std::string value)
    {
        display_name.set(std::move(value));
        invoke(name_changed);
    }
    ii::core::Result<bool> pressure_curve_input(const ii::gui::WidgetInput& e) {
        auto callback = pressure_input;
        return callback ? callback(e) : ii::core::Result<bool>{false};
    }
    void set_spacing_pixels(double value) { spacing_pixels.set(value); invoke(brush_changed); }
    void set_flow(double value) { set_parameter(native::ToolParameter::flow, value * 100); }
    void set_hardness(double value) { set_parameter(native::ToolParameter::hardness, value * 100); }
    void set_spacing(double value) { set_parameter(native::ToolParameter::spacing, value * 100); }
    void set_stabilizer(double value) { set_parameter(native::ToolParameter::stabilization, value * 100); }
    void set_pressure_enabled(bool value)
    {
        pressure_enabled.set(value);
        invoke(brush_changed);
    }
    void set_pressure_min(double value)
    {
        pressure_min.set(value);
        invoke(brush_changed);
    }
    void set_pressure_mid(double value)
    {
        pressure_mid.set(value);
        invoke(brush_changed);
    }
    void set_pressure_max(double value)
    {
        pressure_max.set(value);
        invoke(brush_changed);
    }
    void set_brush_size(double value) { set_parameter(native::ToolParameter::size, value); }
    void set_opacity(double value) { set_parameter(native::ToolParameter::opacity, value * 100); }
    void set_start_recent(bool value)
    {
        start_recent.set(value);
        start_new.set(!value);
        invoke(preference_changed);
    }
    void set_start_new(bool value) { if(value) set_start_recent(false); }
    void set_can_invite(bool value) { can_invite.set(value); invoke(preference_changed); }
    void set_discover(bool value)
    {
        discover.set(value);
        invoke(preference_changed);
    }

  private:
    // Keep the callable alive when it replaces the viewmodel that owns it.
    template<class Callback, class... Args>
    static void invoke(Callback callback, Args&&... args) {
        if (callback) callback(std::forward<Args>(args)...);
    }
    ii::core::ObservableSubscription layer_source_subscription_, member_source_subscription_;
    ii::core::ObservableSubscription color_subscription_;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    std::shared_ptr<int> lists_generation_, color_generation_;
};
} // namespace congregation::ui
