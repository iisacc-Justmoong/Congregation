#pragma once
#include "MainView.generated.h"
#include "ScopedWidgetMount.h"
#include <filesystem>
namespace iiSharedCanvas
{
struct Document;
}
namespace congregation::native
{
class WorkspaceView;
// LVUI owns the ApplicationWindow/options and scoped backend. The workspace
// composes LVUI-owned native widgets in that same generation and Core event loop.
class MainWindow final
{
  public:
    using Scope = lvui_generated::MainView::Scope;
    static ii::gui::HostOptions options() { return lvui_generated::MainView::options(); }
    // Builds the same scoped content used by attach, without creating an OS window.
    ii::core::Result<void> build_content(ii::gui::WidgetBuilder&, ii::gui::Widget parent,
                                        const std::function<void(Scope&)>& configure = {});
    ii::core::Result<void> attach(ii::gui::Host&,
                                  const std::function<void(Scope&)>& configure = {});
    ii::core::Result<void> detach(ii::gui::Host&);
    [[nodiscard]] ScopedWidgetMount widgets() const { return {mount_, NativeSurfaceSlot::workspace}; }
    void set_assets(std::filesystem::path path) { asset_path_ = std::move(path); }
    void refresh_document(const iiSharedCanvas::Document&, const std::string& selected);
    void select_layer(const std::string& selected);

  private:
    ii::gui::WidgetMount mount_;
    std::shared_ptr<Scope> scope_;
    std::shared_ptr<WorkspaceView> workspace_;
    std::filesystem::path asset_path_;
};
} // namespace congregation::native
