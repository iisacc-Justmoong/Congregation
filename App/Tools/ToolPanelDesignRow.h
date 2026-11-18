#pragma once

#include "ToolPanelCatalog.h"
#include "ToolSettings.h"
#include <iiSDKCore>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace congregation::ui {
class EditorBackend;

// A Figma-authored control backed by the existing editor only where that
// control has a corresponding implementation. Other controls are view-only.
class ToolPanelDesignRow final : public std::enable_shared_from_this<ToolPanelDesignRow> {
public:
    ToolPanelDesignRow(std::shared_ptr<EditorBackend>, std::string tool,
                       const native::tool_panels::Field&, std::size_t index);
    const std::string id, label, kind;
    ii::core::Observable<std::string> value_label;
    ii::core::Observable<double> position;
    ii::core::Observable<bool> checked;
    ii::core::Observable<int> choice_index{0};
    std::vector<std::string> options;
    bool parameter_choice = false;
    bool editable = false;
    void refresh();
    [[nodiscard]] std::uint64_t refresh_count() const noexcept { return refresh_count_; }
    void move(double);
    void edit(std::string);
    void toggle(bool);
    void activate();
    void choose(int);
private:
    enum class Binding {
        none, parameter, pressure_opacity, selection_feather, selection_antialias,
        magnetic_radius, wand_tolerance, wand_contiguous, wand_sample,
        fill_tolerance, fill_contiguous, fill_antialias, fill_sample, fill_preserve,
        clone_aligned, clone_sample, crop_aspect, crop_destructive,
        shape_sides, shape_rotation, shape_center, shape_fill, shape_outline,
        shape_stroke, shape_opacity, gradient_type, gradient_repeat, gradient_blend,
        rulers_visible
    };
    static Binding binding_for(std::string_view, const native::tool_panels::Field&,
                               std::optional<native::ToolParameter>&);
    template<class T> void watch(const ii::core::Observable<T>& observable) {
        subscriptions_.push_back(observable.observe([this](const T&, auto) { refresh(); }));
    }
    std::shared_ptr<EditorBackend> editor_;
    std::string tool_;
    std::optional<native::ToolParameter> parameter_;
    Binding binding_;
    std::uint64_t refresh_count_ = 0;
    std::vector<ii::core::ObservableSubscription> subscriptions_;
};

} // namespace congregation::ui
