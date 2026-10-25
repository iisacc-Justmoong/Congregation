#include "MainWindow.h"
#include "WidgetStyle.h"
#include "WorkspaceView.h"
#include <LVRS/WindowMaterial.h>
namespace congregation::native
{
ii::core::Result<void> MainWindow::attach(ii::gui::Host &host,
                                          const std::function<void(Scope &)> &configure)
{
    auto result = host.mount_widgets(
        [&](ii::gui::WidgetBuilder &raw) -> ii::core::Result<void>
        {
            try
            {
                widgets::Builder b(raw);
                auto root = widgets::take(b.controls.zstack(
                    {}, "root",
                    {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
                auto material = widgets::take(
                    lvrs::WindowMaterial::create(raw, root, "application-window",
                                                 {.color = editor::window_background,
                                                  .tint_opacity = Metric::opacityOpaque,
                                                  .radius = Metric::radius0,
                                                  .border_width = Metric::stroke0,
                                                  .intense_opacity = Metric::opacityClear,
                                                  .faint_opacity = Metric::opacityClear}));
                auto generated = lvui_generated::MainView::build(raw, root, configure, &scope_);
                if (!generated)
                    return generated;
                auto vm =
                    std::shared_ptr<congregation::ui::EditorBackend>(scope_, &scope_->backend_0);
                workspace_ = std::make_shared<WorkspaceView>(vm, asset_path_);
                workspace_->build(raw, root);
                widgets::good(raw.retain(workspace_));
                return {};
            }
            catch (...)
            {
                return ii::core::fail(ii::core::ErrorCode::failed,
                                      "Cannot construct Congregation's native ApplicationWindow",
                                      std::current_exception());
            }
        },
        lvui_generated::MainView::appearance());
    if (!result)
    {
        workspace_.reset();
        scope_.reset();
        return std::unexpected(result.error());
    }
    mount_ = *result;
    return {};
}
ii::core::Result<void> MainWindow::detach(ii::gui::Host &host)
{
    if (mount_.snapshot())
    {
        auto r = host.unmount_widgets(mount_);
        if (!r)
            return r;
    }
    mount_ = {};
    workspace_.reset();
    scope_.reset();
    return {};
}
void MainWindow::refresh_document(const iiSharedCanvas::Document &doc, const std::string &selected)
{
    if (workspace_)
        workspace_->refresh_document(doc, selected);
}
} // namespace congregation::native
