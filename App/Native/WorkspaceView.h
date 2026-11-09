#pragma once
#include "TimelineView.h"
#include "OwnedWidgetStyle.h"
namespace iiSharedCanvas
{
struct Document;
}
namespace congregation::native
{
class WorkspaceView final : public std::enable_shared_from_this<WorkspaceView>
{
  public:
    WorkspaceView(std::shared_ptr<congregation::ui::EditorBackend>, std::filesystem::path);
    void build(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void refresh_document(const iiSharedCanvas::Document&, const std::string&);
    void select_layer(const std::string&);

  private:
    void canvas(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void inspector(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void refresh_assets();
    std::shared_ptr<congregation::ui::EditorBackend> vm_;
    std::filesystem::path assets_;
    std::shared_ptr<TimelineView> timeline_;
    std::shared_ptr<ii::gui::BindingScope> bindings_ = std::make_shared<ii::gui::BindingScope>();
    ii::gui::Widget asset_results_;
    std::vector<ii::gui::Widget> option_panels_;
    std::vector<owned_widgets::ButtonHandle> option_buttons_;
};
} // namespace congregation::native
