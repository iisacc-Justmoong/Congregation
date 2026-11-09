#pragma once

#include <LVUI/ColorSelectionSurface.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Core/Result/Result.h>

#include <cstdint>
#include <memory>
#include <thread>

namespace congregation::ui {

// One product color is shared by the toolbar, popup, canvas and detail panel.
// LVUI owns selection and color-space state; the observable announces edits
// after the native surface callback returns from the color-space operation.
class EditorColor final {
public:
    EditorColor();

    [[nodiscard]] ii::gui::WidgetColor current() const;
    [[nodiscard]] std::shared_ptr<lvui::ColorSelectionSurface> source() const { return surface_; }
    [[nodiscard]] ii::core::Result<void> set_color(ii::gui::WidgetColor);
    [[nodiscard]] ii::core::Result<void> set_alpha(double);
    [[nodiscard]] ii::core::Result<void> publish_surface_edit();

    ii::core::Observable<std::int64_t> revision{0};

private:
    [[nodiscard]] ii::core::Result<void> owner() const;
    std::thread::id owner_ = std::this_thread::get_id();
    std::shared_ptr<lvui::ColorSelection> selection_;
    std::shared_ptr<lvui::ColorSelectionSurface> surface_;
};

} // namespace congregation::ui
