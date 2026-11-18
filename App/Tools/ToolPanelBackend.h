#pragma once

#include "Workspace/EditorBackend.h"
#include "ToolPanelDesignRow.h"
#include <iiGUI>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace congregation::ui {

// A parameter is an observable projection of the existing brush model, not a
// second copy of its state. Each row only notifies when its own display changes.
class ToolParameterRow final : public std::enable_shared_from_this<ToolParameterRow> {
public:
    ToolParameterRow(std::shared_ptr<EditorBackend>, const native::ParameterSpec&);
    const std::string key, name, icon, minimum_label, maximum_label, choice_label;
    std::vector<std::string> options;
    ii::core::Observable<double> normalized{0}, raw_value{0};
    ii::core::Observable<std::string> value_label{""};
    ii::core::Observable<bool> enabled{true}, checked{false};
    ii::core::Observable<int> choice_index{0};
    void refresh();
    [[nodiscard]] std::uint64_t refresh_count() const noexcept { return refresh_count_; }
    void move(double value);
    void set_value(double value);
    void toggle(bool value);
    void choose_boolean(int value);
    void flip();
    void choose(int value);
    void decrease();
    void increase();
private:
    std::shared_ptr<EditorBackend> editor_;
    native::ToolParameter parameter_;
    std::uint64_t refresh_count_ = 0;
    std::vector<ii::core::ObservableSubscription> subscriptions_;
};

// Independent gradient controls publish only the displayed fields that change.
class GradientPanelModel final : public std::enable_shared_from_this<GradientPanelModel> {
public:
    explicit GradientPanelModel(std::shared_ptr<EditorBackend>);
    ii::core::Observable<std::string> selection_label{""}, position_label{""}, color_label{""}, alpha_label{""};
    ii::core::Observable<double> position{0}, alpha{0};
    ii::core::Observable<bool> position_enabled{false};
    void previous();
    void next();
    void add();
    void remove();
    void set_position(double);
    void use_current_color();
    void set_alpha(double);
private:
    void refresh_selection();
    void refresh_position();
    void refresh_color();
    void refresh_alpha();
    std::shared_ptr<EditorBackend> editor_;
    std::vector<ii::core::ObservableSubscription> subscriptions_;
};

class PressureCurveModel final : public std::enable_shared_from_this<PressureCurveModel> {
public:
    explicit PressureCurveModel(std::shared_ptr<EditorBackend>);
    void attach(ii::gui::Widget);
    void resized(ii::gui::Rect);
    ii::core::Result<bool> input(const ii::gui::WidgetInput&);
private:
    void draw();
    std::shared_ptr<EditorBackend> editor_;
    ii::gui::Widget curve_;
    int pressure_handle_ = -1;
    std::vector<ii::core::ObservableSubscription> subscriptions_;
};

class BrushPanelActions final {
public:
    explicit BrushPanelActions(std::shared_ptr<EditorBackend> editor) : editor_(std::move(editor)) {}
    void choose_texture();
    void set_background();
    void reset();
    void save();
private:
    std::shared_ptr<EditorBackend> editor_;
};

// The LVUI activation borrows the editor and its observables for its lifetime.
class ToolPanelBackend final {
public:
    explicit ToolPanelBackend(std::shared_ptr<EditorBackend>, bool expanded = false);
    ToolPanelBackend(const ToolPanelBackend&) = delete;
    ToolPanelBackend& operator=(const ToolPanelBackend&) = delete;
    [[nodiscard]] std::shared_ptr<ToolParameterRow> row(const std::string&) const;
    std::shared_ptr<EditorBackend> editor;
    const bool expanded;
    ii::core::Observable<std::string>& selected_tool;
    ii::core::Observable<std::string>& preset;
    ii::core::Observable<std::string>& spacing_label;
    ii::core::Observable<std::string>& pressure_min_label;
    ii::core::Observable<std::string>& pressure_mid_label;
    ii::core::Observable<std::string>& pressure_max_label;
    ii::core::Observable<std::string>& texture_name;
    ii::core::Observable<bool>& pressure_enabled;
    ii::core::Observable<bool>& clone_aligned;
    ii::core::Observable<bool>& clone_current_layer;
    ii::core::Observable<bool>& selection_antialias;
    ii::core::Observable<bool>& wand_contiguous;
    ii::core::Observable<bool>& wand_current_layer;
    ii::core::Observable<bool>& fill_contiguous;
    ii::core::Observable<bool>& fill_current_layer;
    ii::core::Observable<bool>& fill_target_current;
    ii::core::Observable<bool>& fill_preserve_alpha;
    ii::core::Observable<bool>& fill_antialias;
    ii::core::Observable<bool>& crop_destructive;
    ii::core::Observable<bool>& polygon_center;
    ii::core::Observable<bool>& polygon_fill;
    ii::core::Observable<bool>& polygon_outline;
    ii::core::Observable<double>& spacing_pixels;
    ii::core::Observable<double>& selection_feather;
    ii::core::Observable<double>& magnetic_radius;
    ii::core::Observable<double>& wand_tolerance;
    ii::core::Observable<double>& fill_tolerance;
    ii::core::Observable<double>& polygon_rotation;
    ii::core::Observable<double>& polygon_stroke_width;
    ii::core::Observable<double>& polygon_opacity;
    ii::core::Observable<std::int64_t>& polygon_sides;
    ii::core::Observable<std::int64_t>& crop_aspect;
    ii::core::Observable<std::uint64_t>& gradient_revision;
    ii::core::Observable<std::shared_ptr<GradientPanelModel>> gradient_panel;
    ii::core::Observable<std::shared_ptr<PressureCurveModel>> pressure_curve;
    ii::core::Observable<std::shared_ptr<BrushPanelActions>> brush_actions;
    ii::core::Observable<std::shared_ptr<ToolParameterRow>> pressure_row{nullptr};
    ii::core::Observable<std::shared_ptr<ToolParameterRow>> absolute_spacing_row{nullptr};
    ii::core::Observable<std::vector<std::shared_ptr<ToolPanelDesignRow>>> design_rows{
        std::vector<std::shared_ptr<ToolPanelDesignRow>>{}};

    [[nodiscard]] bool is_brush(const std::string&) const;
    [[nodiscard]] bool is_shape(const std::string&) const;
    [[nodiscard]] bool is_selection(const std::string&) const;
    [[nodiscard]] std::string heading(const std::string&) const;
    [[nodiscard]] std::string icon(const std::string&) const;
    [[nodiscard]] std::string count(const std::string&) const;
    [[nodiscard]] std::string aspect_label(std::int64_t) const;
    [[nodiscard]] std::string crop_mode_label(bool) const;
    [[nodiscard]] std::string percentage(double) const;
    [[nodiscard]] std::string pixels(double) const;
    [[nodiscard]] std::string integer(std::int64_t) const;
    [[nodiscard]] std::string degrees(double) const;
    void choose_preset();
    void set_pressure(bool);
    void set_pressure_choice(int);
    void set_spacing(double);
    void choose_texture();
    void set_background();
    void reset();
    void save();
    void toggle_pressure();
    void select_clone();
    void cycle_crop_aspect();
    void toggle_crop_mode();
    void apply_crop();
    void cancel_crop();
    void set_polygon_sides(double);

    [[nodiscard]] std::string gradient_stop(std::uint64_t) const;
    [[nodiscard]] double gradient_position(std::uint64_t) const;
    [[nodiscard]] bool gradient_position_enabled(std::uint64_t) const;
    [[nodiscard]] std::string gradient_color(std::uint64_t) const;
    [[nodiscard]] double gradient_alpha(std::uint64_t) const;
    [[nodiscard]] bool gradient_radial(std::uint64_t) const;
    [[nodiscard]] bool gradient_repeat(std::uint64_t) const;
    [[nodiscard]] std::string gradient_blend(std::uint64_t) const;
    void gradient_previous();
    void gradient_next();
    void gradient_add();
    void gradient_remove();
    void set_gradient_position(double);
    void gradient_use_current_color();
    void set_gradient_alpha(double);
    void set_gradient_radial(bool);
    void set_gradient_repeat(bool);
    void cycle_gradient_blend();

    void attach(ii::gui::Widget curve, ii::gui::Widget scroll,
                ii::gui::Widget tilt, ii::gui::Widget smoothing);
    void curve_resized(ii::gui::Rect);
    ii::core::Result<bool> curve_input(const ii::gui::WidgetInput&);
    void reveal_tilt();
    void reveal_smoothing();
private:
    void refresh_design();
    void reveal(ii::gui::Widget);
    std::unordered_map<std::string,std::shared_ptr<ToolParameterRow>> rows_;
    ii::core::ObservableSubscription tool_lease_;
    ii::gui::Widget scroll_, tilt_, smoothing_;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};
} // namespace congregation::ui
