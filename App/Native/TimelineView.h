#pragma once
#include "EditorBackend.h"
#include <filesystem>
namespace congregation::native
{
class TimelineView final : public std::enable_shared_from_this<TimelineView>
{
  public:
    TimelineView(std::shared_ptr<congregation::ui::EditorBackend>, std::filesystem::path);
    ii::gui::Widget build(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void refresh_document(const iiSharedCanvas::Document&, const std::string&);

  private:
    struct Row
    {
        std::string id, name, icon, mode;
        double opacity = 1;
        bool visible = true;
        std::vector<int> keys;
    };
    void paint();
    void begin_rename();
    void commit_rename();
    ii::core::Result<bool> input(const ii::gui::WidgetInput&, bool ruler);
    std::shared_ptr<congregation::ui::EditorBackend> vm_;
    std::filesystem::path assets_;
    std::vector<Row> rows_;
    std::string selected_;
    ii::gui::Widget ruler_, body_, scroll_, time_, play_;
    ii::gui::Widget rename_, rename_field_;
    std::string rename_text_, rename_key_, drag_layer_;
    double drag_start_y_ = 0;
    std::shared_ptr<ii::gui::BindingScope> bindings_ = std::make_shared<ii::gui::BindingScope>();
    std::shared_ptr<const ii::gui::RasterImage> marker_;
    double zoom_ = 1, offset_ = 0, down_y_ = 0;
    int down_rate_ = 24;
    bool dragging_ = false, rate_drag_ = false;
    std::optional<ii::runtime::Clock::time_point> last_frame_;
};
} // namespace congregation::native
