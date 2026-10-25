#pragma once
#include "CanvasSetupView.generated.h"
#include <functional>
#include <memory>

namespace congregation::native
{
// The content matches Dreamscapes' chooser. Host::show owns the nonmodal child
// window boundary; no modal/dimming widget is introduced in the main editor.
class CanvasSetupWindow final
{
  public:
    using Scope = lvui_generated::CanvasSetupView::Scope;
    static ii::gui::HostOptions options() { return lvui_generated::CanvasSetupView::options(); }
    ii::core::Result<void> attach(ii::gui::Host&, const std::function<void(Scope&)>& = {});
    ii::core::Result<void> detach(ii::gui::Host&);
    ii::gui::WidgetMount widgets() const { return mount_; }

  private:
    struct Content;
    std::shared_ptr<Content> content_;
    std::shared_ptr<Scope> scope_;
    ii::gui::WidgetMount mount_;
};
} // namespace congregation::native
