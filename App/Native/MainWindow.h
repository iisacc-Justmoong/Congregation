#pragma once
#include "MainView.generated.h"
#include <filesystem>
namespace congregation::native
{
class WorkspaceView;
// LVUI owns the ApplicationWindow/options and scoped backend. The workspace
// composes native LVRS widgets in that same generation and Core event loop.
class MainWindow final
{
  public:
    using Scope = lvui_generated::MainView::Scope;
    static ii::gui::HostOptions options() { return lvui_generated::MainView::options(); }
    ii::core::Result<void> attach(ii::gui::Host&,
                                  const std::function<void(Scope&)>& configure = {});
    ii::core::Result<void> detach(ii::gui::Host&);
    [[nodiscard]] ii::gui::WidgetMount widgets() const { return mount_; }
    void set_assets(std::filesystem::path path) { asset_path_ = std::move(path); }
    void refresh_document(const iiSharedCanvas::Document&, const std::string& selected);

  private:
    ii::gui::WidgetMount mount_;
    std::shared_ptr<Scope> scope_;
    std::shared_ptr<WorkspaceView> workspace_;
    std::filesystem::path asset_path_;
};
} // namespace congregation::native
