#pragma once
#include "TimelineView.h"
namespace congregation::native
{
class WorkspaceView final : public std::enable_shared_from_this<WorkspaceView>
{
  public:
    WorkspaceView(std::shared_ptr<congregation::ui::EditorBackend>, std::filesystem::path);
    void build(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void refresh_document(const iiSharedCanvas::Document&, const std::string&);

  private:
    void toolbar(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void tools(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void canvas(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void inspector(ii::gui::WidgetBuilder&, ii::gui::Widget);
    std::shared_ptr<congregation::ui::EditorBackend> vm_;
    std::filesystem::path assets_;
    std::shared_ptr<TimelineView> timeline_;
    std::shared_ptr<ii::gui::BindingScope> bindings_ = std::make_shared<ii::gui::BindingScope>();
    std::vector<ii::gui::Widget> option_panels_;
    std::vector<ii::gui::Widget> option_buttons_;
    std::shared_ptr<const ii::gui::ColorChangePort> color_port_;
};
} // namespace congregation::native
