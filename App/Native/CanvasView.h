#pragma once
#include "Artwork.h"
#include "CanvasTilePolicy.h"
#include "QuickMask.h"
#include "StrokeProfile.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Image.h>
#include <map>
#include <tuple>
#include <vector>

namespace congregation::native
{
struct Viewport final
{
    double scale = 1, x = 0, y = 0, width = 1, height = 1;
    std::size_t visible_tiles = 0;
    std::uint64_t uploads = 0;
    // Successful CPU composition calls, independent of the number of tiles.
    std::uint64_t composition_batches = 0;
};
class CanvasView final
{
  public:
    explicit CanvasView(Artwork& artwork, std::size_t layer_budget = canvas_tile_layer_budget);
    ~CanvasView();
    CanvasView(const CanvasView&) = delete;
    CanvasView& operator=(const CanvasView&) = delete;
    void attach(ii::gui::Widget, ii::gui::Widget paint_host = {});
    void resize(ii::gui::Rect);
    void refresh(bool document_changed = true);
    void render();
    void fit();
    void zoom(double factor, double x = -1, double y = -1);
    void set_frame(int);
    ii::gui::WidgetInput localize_input(const ii::gui::WidgetInput&) const;
    ii::core::Result<bool> input(const ii::gui::WidgetInput&);
    const Viewport& viewport() const { return viewport_; }
    bool begin_quick_mask();
    bool commit_quick_mask();
    bool quick_mask_active() const noexcept { return bool(quick_mask_); }
    [[nodiscard]] std::optional<SelectionPreview> clone_preview() const;
    bool quick_mask_undo();
    bool quick_mask_redo();
    std::optional<iiSharedCanvas::RasterMask> quick_mask_preview() const { return quick_mask_ ? quick_mask_->mask() : std::nullopt; }
    bool polygon_active() const noexcept { return polygon_active_; }
    bool crop_active() const noexcept { return crop_.has_value() || perspective_.has_value(); }
    // A pending license denial must not discard an in-flight pointer gesture.
    bool interaction_active() const noexcept { return drag_; }
    bool apply_crop();
    void cancel_crop() { if (crop_ || perspective_) cancel(); }
    void cancel_interaction() { cancel(); }
    std::string tool = "brush", text = "Text";
    std::uint32_t color = 0xff1a1a1a;
    double brush_size = 2, opacity = 1;
    // elapsed is monotonic seconds since the active airbrush gesture began.
    [[nodiscard]] bool airbrush_active() const;
    [[nodiscard]] bool airbrush_tick(double elapsed);
    void cancel_airbrush_stroke();
    [[nodiscard]] std::uint64_t airbrush_generation() const { return airbrush_generation_; }
    bool presentation = false;
    bool grid = false;
    PolygonOptions polygon_options;
    double selection_feather = 0, magnetic_radius = 12, crop_aspect = 0;
    bool selection_antialias = true, crop_destructive = false;
    double wand_tolerance = 0;
    bool wand_contiguous = true, wand_current_layer = false;
    bool clone_aligned = true, clone_current_layer = true;
    double fill_tolerance = 0;
    bool fill_contiguous = true, fill_current_layer = false, fill_target_current = false, fill_preserve_alpha = false, fill_antialias = false;
    std::vector<iiSharedCanvas::GradientStop> gradient_stops;
    iiSharedCanvas::GradientKind gradient_kind = iiSharedCanvas::GradientKind::Linear;
    iiSharedCanvas::GradientSpread gradient_spread = iiSharedCanvas::GradientSpread::Pad;
    RasterBlendMode gradient_blend = RasterBlendMode::SourceOver;
    iiSharedCanvas::BitmapBrush settings;
    StrokeTaper taper;
    std::function<void(std::string)> status;
    std::function<void(std::uint32_t)> color_picked;
    std::function<void(bool)> quick_mask_changed;

  private:
    iiSharedCanvas::Point world(double x, double y) const;
    bool hit(iiSharedCanvas::Point);
    void cancel();
    void finish_polygon(bool manual = false);
    void update_crop(iiSharedCanvas::Point);
    void update_perspective(iiSharedCanvas::Point);
    void schedule_stroke_render();
    void invalidate_stroke_tiles(const StrokeDamage&);
    void resize_paint_layers();
    void present_drawing(ii::gui::WidgetDrawing, std::size_t tile_count);
    [[nodiscard]] std::optional<iiSharedCanvas::CanvasRegion> stroke_tile_region(const StrokeDamage&) const;
    void render_stroke(std::optional<iiSharedCanvas::CanvasRegion>);
    std::optional<std::vector<iiSharedCanvas::Point>> magnetic_segment(iiSharedCanvas::Point, bool manual);
    ::StrokePoint stroke_sample(const ii::gui::WidgetInput&, iiSharedCanvas::Point, bool first);
    Artwork& artwork_;
    std::size_t paint_layer_budget_;
    StrokeProfile stroke_profile_;
    std::unique_ptr<QuickMask> clone_mask_;
    std::optional<::RasterLayer> clone_reference_;
    std::optional<iiSharedCanvas::Point> clone_source_;
    std::optional<iiSharedCanvas::CanvasOrigin> clone_aligned_offset_;
    iiSharedCanvas::CanvasOrigin clone_offset_{};
    iiSharedCanvas::CanvasExtent clone_source_extent_{};
    std::string clone_source_layer_, clone_selected_;
    std::filesystem::path clone_document_;
    std::uint64_t clone_revision_ = 0;
    std::shared_ptr<const iiSharedCanvas::RasterMask> clone_selection_;
    std::shared_ptr<const iiSharedCanvas::TiledMask> clone_tiled_selection_;
    std::unique_ptr<QuickMask> quick_mask_;
    std::shared_ptr<const iiSharedCanvas::RasterMask> quick_mask_dense_before_;
    std::shared_ptr<const iiSharedCanvas::TiledMask> quick_mask_tiled_before_;
    std::uint64_t quick_mask_revision_ = 0;
    ii::gui::Widget widget_;
    ii::gui::Widget paint_host_, paint_overlay_;
    std::vector<ii::gui::Widget> paint_layers_;
    std::vector<std::vector<ii::gui::ImageRun>> presented_tiles_;
    std::shared_ptr<int> frame_lifetime_ = std::make_shared<int>(0);
    Viewport viewport_;
    std::map<std::tuple<int, int, int, int>, std::shared_ptr<const ii::gui::RasterImage>> cache_;
    std::optional<bool> stroke_preview_available_;
    std::optional<iiSharedCanvas::CanvasRegion> stroke_preview_region_;
    struct OutlineKey final {
        std::shared_ptr<const iiSharedCanvas::RasterMask> mask;
        std::shared_ptr<const iiSharedCanvas::TiledMask> tiled;
        double x=0,y=0,scale=0,width=0,height=0;
        int origin_x=0,origin_y=0;
        friend bool operator==(const OutlineKey&,const OutlineKey&)=default;
    };
    std::optional<OutlineKey> outline_key_;
    std::shared_ptr<const ii::gui::RasterImage> outline_image_;
    int frame_ = 0;
    bool fitted_ = true, attached_ = false, drag_ = false, panning_ = false, zooming_ = false, space_ = false,
         selecting_ = false;
    bool frame_handler_ready_ = false, stroke_render_pending_ = false, committing_stroke_ = false,
         stroke_preview_unbounded_ = false;
    iiSharedCanvas::Point down_{}, last_{};
    bool polygon_regular_preview_ = false;
    std::vector<iiSharedCanvas::Point> lasso_path_, polygon_path_, magnetic_preview_;
    std::vector<std::size_t> magnetic_anchors_;
    struct CropRect { double left=0,top=0,right=0,bottom=0; };
    enum class CropDrag { None, Resize, Move };
    std::optional<CropRect> crop_, crop_saved_;
    std::optional<std::array<iiSharedCanvas::Point,4>> perspective_, perspective_saved_;
    // -3 idle, -2 creating, -1 moving, 0..3 dragging one corner.
    int perspective_drag_ = -3;
    CropDrag crop_drag_ = CropDrag::None;
    iiSharedCanvas::Point crop_anchor_{}, crop_drag_start_{};
    std::uint64_t crop_revision_ = 0;
    std::optional<RasterLayer> magnetic_reference_;
    bool magnetic_tiled_ = false;
    std::uint64_t magnetic_revision_ = 0;
    iiSharedCanvas::Point polygon_cursor_{};
    iiSharedCanvas::MaskOperation polygon_operation_ = iiSharedCanvas::MaskOperation::Replace;
    double polygon_last_time_ = 0, polygon_last_x_ = 0, polygon_last_y_ = 0;
    bool polygon_active_ = false;
    double stroke_clock_ = 0, stroke_native_elapsed_ = 0, stroke_last_timestamp_ = 0;
    ::StrokePoint last_stroke_sample_{};
    std::uint64_t airbrush_generation_ = 0;
    bool airbrush_stroke_ = false;
};
} // namespace congregation::native
