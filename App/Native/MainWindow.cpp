#include "MainWindow.h"
#include "OwnedWidgetStyle.h"
#include "WorkspaceView.h"
#include <LVUI/NativeMaterial.h>
namespace congregation::native
{
ii::core::Result<void> MainWindow::build_content(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent,
                                                 const std::function<void(Scope &)> &configure)
{
    try
    {
        const auto window_color = editor::window_background();
        owned_widgets::Builder b(raw);
        auto root = owned_widgets::take(b.controls.zstack(
            parent, "root",
            {.spacing = Metric::space0, .alignment = ii::gui::StackAlignment::stretch}));
        auto material = owned_widgets::take(
            lvui_host::Material::create(raw, root, "application-window",
                                         {.kind = lvui::MaterialKind::window,
                                          .color = lvui::native::material_color(window_color),
                                          .tint_opacity = 1.0,
                                          .blur_radius = Metric::space0,
                                          .radius = Metric::space0,
                                          .inner_highlight_opacity = 0.0,
                                          .border_width = Metric::space0,
                                          .intense_opacity = 0.0,
                                          .faint_opacity = 0.0,
                                          .shadow_opacity = 0.0,
                                          .width = Metric::space0,
                                          .height = Metric::space0}));
        std::shared_ptr<Scope> candidate_scope;
        auto generated = lvui_generated::MainView::build(raw, root, configure, &candidate_scope);
        if (!generated)
            return generated;
        auto vm = std::shared_ptr<congregation::ui::EditorBackend>(candidate_scope, &candidate_scope->backend_0);
        auto candidate_workspace = std::make_shared<WorkspaceView>(vm, asset_path_);
        auto mounted = std::make_shared<bool>(false);
        auto source = std::make_shared<NativeSurfaceSource>(
            [weak = std::weak_ptr(candidate_workspace), mounted](ii::gui::WidgetBuilder &builder)
                -> ii::core::Result<void>
            {
                auto workspace = weak.lock();
                if (!workspace)
                    return ii::core::fail(ii::core::ErrorCode::closed, "Workspace has retired");
                workspace->build(builder, {});
                if (auto retained = builder.retain(workspace); !retained)
                    return retained;
                *mounted = true;
                return {};
            });
        candidate_scope->backend_1.source.set(std::move(source));
        if (!*mounted)
            return ii::core::fail(ii::core::ErrorCode::failed, "LVUI workspace surface was not mounted");
        scope_ = std::move(candidate_scope);
        workspace_ = std::move(candidate_workspace);
        return {};
    }
    catch (...)
    {
        return ii::core::fail(ii::core::ErrorCode::failed,
                              "Cannot construct Congregation's native ApplicationWindow",
                              std::current_exception());
    }
}
ii::core::Result<void> MainWindow::attach(ii::gui::Host &host,
                                          const std::function<void(Scope &)> &configure)
{
    auto appearance = lvui_generated::MainView::appearance();
    appearance.window = editor::window_background();
    auto result = host.mount_widgets(
        [&](ii::gui::WidgetBuilder &raw) -> ii::core::Result<void>
        {
            return build_content(raw, {}, configure);
        },
        appearance);
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
void MainWindow::select_layer(const std::string &selected)
{
    if (workspace_) workspace_->select_layer(selected);
}
} // namespace congregation::native
