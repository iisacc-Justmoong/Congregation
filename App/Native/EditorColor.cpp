#include "EditorColor.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace congregation::ui {

EditorColor::EditorColor()
    : selection_(std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1, 1, 1, 1})),
      surface_(std::make_shared<lvui::ColorSelectionSurface>(selection_)) {}

ii::core::Result<void> EditorColor::owner() const {
    if (owner_ != std::this_thread::get_id())
        return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Editor color requires its owning thread");
    return {};
}

ii::gui::WidgetColor EditorColor::current() const {
    if (auto valid = owner(); !valid) throw std::logic_error(valid.error().message);
    const auto value = selection_->snapshot().color;
    return {value.red, value.green, value.blue, value.alpha};
}

ii::core::Result<void> EditorColor::set_color(ii::gui::WidgetColor value) {
    if (auto valid = owner(); !valid) return valid;
    auto result = selection_->set_color({value.red, value.green, value.blue, value.alpha});
    if (!result) return ii::core::fail(ii::core::ErrorCode::invalid_argument, result.error().message);
    return publish_surface_edit();
}

ii::core::Result<void> EditorColor::set_alpha(double alpha) {
    if (auto valid = owner(); !valid) return valid;
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
        return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Alpha must be between zero and one");
    auto result = surface_->edit_channel(lvui::ColorChannel::alpha, alpha * 100, surface_->revision());
    if (!result) return ii::core::fail(ii::core::ErrorCode::failed, result.error().message);
    return publish_surface_edit();
}

ii::core::Result<void> EditorColor::publish_surface_edit() {
    if (auto valid = owner(); !valid) return valid;
    const auto value = selection_->revision();
    if (value > std::uint64_t(std::numeric_limits<std::int64_t>::max()))
        return ii::core::fail(ii::core::ErrorCode::out_of_range, "Editor color revision exhausted");
    try {
        revision.set(static_cast<std::int64_t>(value));
        return {};
    } catch (...) {
        return ii::core::fail(ii::core::ErrorCode::failed,
                              "Editor color changed but an observer failed", std::current_exception());
    }
}

} // namespace congregation::ui
