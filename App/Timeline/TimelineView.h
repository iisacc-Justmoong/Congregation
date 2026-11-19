#pragma once
#include "Workspace/EditorBackend.h"
#include "Shared/OwnedWidgetStyle.h"
#include "LayerPreview.h"
#include "Document/DocumentChange.h"
#include <filesystem>
#include <utility>
namespace iiSharedCanvas
{
struct Document;
}
namespace congregation::native
{
class TimelineView final : public std::enable_shared_from_this<TimelineView>
{
  public:
    TimelineView(std::shared_ptr<congregation::ui::EditorBackend>, std::filesystem::path);
    ii::gui::Widget build(ii::gui::WidgetBuilder&, ii::gui::Widget);
    void refresh_document(const iiSharedCanvas::Document&, const std::string&);
    void refresh_frame(const iiSharedCanvas::Document&, int);
    void apply_document_change(const iiSharedCanvas::Document&, const std::string&, const DocumentChange&);
    void select_layer(const std::string&);

  private:
    struct Row
    {
        std::string id, name, icon, mode;
        double opacity = 1;
        bool visible = true;
        std::vector<int> keys;
        bool audio = false;
        std::optional<LayerPreviewKey> preview_key;
        std::shared_ptr<const ii::gui::RasterImage> preview;
        // Occupied cells are independent of pixel opacity and playhead position.
        std::optional<iiSharedCanvas::LayerFrameRange> content_frames;
        friend bool operator==(const Row&, const Row&) = default;
    };
    // Project one document layer onto its retained timeline row.
    void update_layer(Row&, const iiSharedCanvas::Document&, const iiSharedCanvas::Layer&,
                      iiSharedCanvas::FrameIndex, bool force_pixels, bool update_keys);
    void paint(bool update_rows = true);
    void paint_row(std::size_t);
    void paint_rows(const std::vector<std::size_t>&);
    void begin_rename();
    void commit_rename();
    ii::core::Result<bool> input(const ii::gui::WidgetInput&, bool ruler);
    std::shared_ptr<congregation::ui::EditorBackend> vm_;
    std::filesystem::path assets_;
    std::vector<Row> rows_;
    std::string selected_;
    ii::gui::Widget ruler_, body_, scroll_, play_, rename_, playhead_;
    std::vector<ii::gui::Widget> row_slots_;
    std::vector<std::size_t> slot_rows_;
    std::optional<owned_widgets::InputHandle> time_, rename_field_;
    std::string rename_text_, rename_key_, drag_layer_;
    double drag_start_y_ = 0;
    std::shared_ptr<ii::gui::BindingScope> bindings_ = std::make_shared<ii::gui::BindingScope>();
    std::shared_ptr<const ii::gui::RasterImage> marker_;
    double zoom_ = 1, offset_ = 0, down_y_ = 0;
    int down_rate_ = 24;
    std::optional<std::pair<double, double>> row_timing_;
    bool dragging_ = false, rate_drag_ = false;
    std::optional<ii::runtime::Clock::time_point> last_frame_;
};
} // namespace congregation::native
