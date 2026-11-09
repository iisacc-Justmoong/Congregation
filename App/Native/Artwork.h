#pragma once
#include "CanvasDefinition.h"
#include "Shapes.h"
#include <iiSharedCanvasNative.h>
#include <Stroke/StrokePoint.h>
#include <array>
#include <filesystem>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <vector>
#include <variant>
#include <string_view>

namespace congregation::native
{
std::filesystem::path native_path(std::string_view);
std::string path_text(const std::filesystem::path&);
enum class LayerTransform
{
    flip_horizontal,
    flip_vertical,
    rotate_left,
    rotate_right
};
// Synchronous borrowed tile views: pixel storage remains owned by this value.
struct SelectionOptions final { bool antialias = true; double feather = 0; };
// Percentages are represented as fractions of the complete stroke length.
struct StrokeTaper final { double start = 0, end = 0, minimum = 1, strength = 0; };
// A conservative, half-open document-space rectangle affected by a stroke.
struct StrokeDamage final { double left = 0, top = 0, right = 0, bottom = 0; };
struct FloodOptions final { double tolerance = 0; bool contiguous = true; bool current_layer = false; bool target_current = false; bool preserve_alpha = false; bool antialias = false; };
struct SelectionPreview final
{
    SelectionPreview() = default;
    SelectionPreview(const SelectionPreview&) = delete;
    SelectionPreview& operator=(const SelectionPreview&) = delete;
    SelectionPreview(SelectionPreview&&) noexcept = default;
    SelectionPreview& operator=(SelectionPreview&&) noexcept = default;
    std::string asset_id;
    std::vector<std::vector<std::uint32_t>> pixels;
    std::vector<iiSharedCanvas::BitmapPreviewTile> tiles;
};
// Product editing policy. Canonical storage/rasterization remain SDK services.
class Artwork final
{
  public:
    explicit Artwork(std::filesystem::path recovery_directory, std::size_t history_budget = 64 * 1024 * 1024);
    bool create(int width = 1024, int height = 768, bool infinite = false);
    bool create(const CanvasDefinition&);
    const std::optional<CanvasDefinition>& creation() const { return creation_; }
    bool open(const std::filesystem::path&);
    bool recover(const std::filesystem::path&);
    bool replace(iiSharedCanvas::Document);
    bool save(const std::filesystem::path&);
    bool export_image(const std::filesystem::path&, std::string format);
    bool import_image(const std::filesystem::path&, double x = 0, double y = 0);
    bool import_pixels(::RasterLayer, std::string name, double x, double y);
    bool add_layer(std::string name = "Paint layer");
    bool duplicate_layer();
    bool remove_layer();
    bool rename(std::string key, std::string name);
    bool visible(std::string key, bool);
    bool opacity(std::string key, double);
    bool cycle_blend(std::string key);
    bool timing(int rate, int frames);
    bool move(std::string key, std::string before);
    bool transform(::AffineTransform);
    bool transform(LayerTransform);
    bool begin(::DocumentPoint, double, iiSharedCanvas::BitmapBrush,
               iiSharedCanvas::FrameIndex frame = 0);
    bool extend(::DocumentPoint, double);
    bool end(::DocumentPoint, double);
    bool begin_input(::StrokePoint, iiSharedCanvas::BitmapBrush, iiSharedCanvas::FrameIndex frame = 0, StrokeTaper taper = {});
    bool extend_input(::StrokePoint);
    bool end_input(::StrokePoint);
    void cancel();
    [[nodiscard]] std::optional<StrokeDamage> take_stroke_damage() noexcept;
    bool shape(std::string kind, iiSharedCanvas::Point from, iiSharedCanvas::Point to,
               std::uint32_t color, double width, bool filled, PolygonOptions polygon = {},
               iiSharedCanvas::FrameIndex frame = 0);
    bool styled_shape(std::string kind, iiSharedCanvas::Point from, iiSharedCanvas::Point to, std::uint32_t color, ShapeStyle, iiSharedCanvas::FrameIndex frame = 0);
    // Immutable source snapshot in document coordinates for a clone gesture.
    bool clone_reference(iiSharedCanvas::FrameIndex, bool current_layer, ::RasterLayer&);
    bool clone_stamp(const ::RasterLayer& source, const iiSharedCanvas::RasterMask& stroke,
                     iiSharedCanvas::CanvasOrigin source_offset, iiSharedCanvas::FrameIndex);
    [[nodiscard]] std::optional<SelectionPreview> clone_preview(const ::RasterLayer& source, const ::RasterLayer& stroke,
                     iiSharedCanvas::CanvasOrigin source_offset, iiSharedCanvas::FrameIndex, ::DevicePixelRect dirty) const;
    bool gradient(iiSharedCanvas::GradientDefinition, iiSharedCanvas::FrameIndex frame = 0,
                  RasterBlendMode blend = RasterBlendMode::SourceOver);
    bool fill(std::uint32_t color);
    bool fill_at(iiSharedCanvas::Point, std::uint32_t color,
                 iiSharedCanvas::FrameIndex frame = 0, FloodOptions options = {});
    // Selection is session state: selection-only history does not change document revision.
    bool set_selection(iiSharedCanvas::SelectionGeometry, iiSharedCanvas::Point, iiSharedCanvas::Point,
                       iiSharedCanvas::MaskOperation = iiSharedCanvas::MaskOperation::Replace, SelectionOptions = {});
    bool set_polygon_selection(std::span<const iiSharedCanvas::Point>,
                               iiSharedCanvas::MaskOperation = iiSharedCanvas::MaskOperation::Replace, SelectionOptions = {});
    bool set_wand_selection(iiSharedCanvas::Point, iiSharedCanvas::FrameIndex, double tolerance, bool contiguous,
                            iiSharedCanvas::MaskOperation = iiSharedCanvas::MaskOperation::Replace,
                            bool current_layer = false, SelectionOptions options = {false, 0});
    bool apply_selection_mask(iiSharedCanvas::RasterMask);
    bool apply_tiled_selection_mask(iiSharedCanvas::TiledMask);
    bool reframe_canvas(iiSharedCanvas::CanvasRegion);
    bool perspective_crop(const std::array<iiSharedCanvas::Point, 4>&, iiSharedCanvas::CanvasExtent, bool discard_replaced_assets = false);
    bool clear_selection();
    bool invert_selection();
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] const iiSharedCanvas::RasterMask* selection_mask() const noexcept;
    [[nodiscard]] std::shared_ptr<const iiSharedCanvas::RasterMask> selection_mask_handle() const noexcept;
    [[nodiscard]] std::shared_ptr<const iiSharedCanvas::TiledMask> tiled_selection_handle() const noexcept;
    [[nodiscard]] std::optional<SelectionPreview> selection_preview(
        std::optional<iiSharedCanvas::CanvasRegion> document_region = std::nullopt) const;
    const iiSharedCanvas::Document& committed_document() const { return *file_.document(); }
    bool undo();
    bool redo();
    bool select(std::string key);
    std::string snapshot() const;
    bool receive(std::string_view);
    const iiSharedCanvas::Document& document() const { return draft_; }
    std::uint64_t revision() const { return file_.revision(); }
    const std::string& selected() const { return selected_; }
    const std::string& error() const { return error_; }
    const std::filesystem::path& source() const { return source_; }
    const std::filesystem::path& recovery() const { return recovery_; }
    bool stroke_active() const { return raster_.strokeActive() || chunks_.strokeActive(); }
    std::size_t history_bytes() const { return history_bytes_; }
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    std::function<void()> changed, selection_changed;

  private:
    const std::size_t history_budget_;
    struct SelectionValue final {
        std::shared_ptr<const iiSharedCanvas::RasterMask> dense;
        std::shared_ptr<const iiSharedCanvas::TiledMask> tiled;
        std::size_t bytes() const noexcept { return sizeof(SelectionValue) + (dense ? dense->alpha.size() : 0) + (tiled ? tiled->storage_bytes() : 0); }
    };
    struct SelectionTransition final
    {
        std::shared_ptr<const SelectionValue> before, after;
    };
    struct FrameSourceHistory final
    {
        bool dynamic = false;
        std::string asset;
        std::vector<iiSharedCanvas::KeyframePlacement> keyframes;
        std::size_t bytes() const noexcept {
            std::size_t n=sizeof(*this)+asset.capacity()+keyframes.capacity()*sizeof(iiSharedCanvas::KeyframePlacement);
            for(const auto& key:keyframes)n+=key.assetId.capacity();
            return n;
        }
    };
    struct ChunkFrameHistory final
    {
        std::string layer, prior, replacement;
        FrameSourceHistory before, after;
        std::size_t bytes() const noexcept {return sizeof(*this)+layer.capacity()+prior.capacity()+replacement.capacity()+before.bytes()+after.bytes();}
    };
    struct RasterInsertion final
    {
        std::string asset_id, layer_id, name;
        std::variant<::RasterLayer, iiSharedCanvas::ChunkedRasterAsset> pixels;
        iiSharedCanvas::CanvasOrigin origin;
        iiSharedCanvas::FrameIndex frame = 0;
        RasterBlendMode blend = RasterBlendMode::SourceOver;
        std::size_t asset_index = 0, layer_index = 0;
        std::size_t bytes() const noexcept {
            std::size_t payload=0;
            if(const auto* raster=std::get_if<::RasterLayer>(&pixels)) payload=raster->pixels.capacity()*sizeof(std::uint32_t);
            else {const auto& chunks=std::get<iiSharedCanvas::ChunkedRasterAsset>(pixels);payload=chunks.id.capacity()+chunks.chunks.capacity()*sizeof(iiSharedCanvas::RasterChunk);for(const auto& chunk:chunks.chunks)payload+=chunk.pixels.pixels.capacity()*sizeof(std::uint32_t);}
            return sizeof(*this)+asset_id.capacity()+layer_id.capacity()+name.capacity()+payload;
        }
    };
    struct RasterStrokeMetadata final
    {
        iiSharedCanvas::DocumentAuthorship before, after;
        iiSharedCanvas::FormatVersion before_version, after_version;
        std::size_t bytes() const noexcept { return sizeof(*this) + before.dump().size() + after.dump().size(); }
    };
    struct History final
    {
        std::vector<std::uint8_t> before, after;
        std::string before_selection, after_selection;
        std::shared_ptr<const SelectionValue> mask_before, mask_after;
        bool mask_change = false;
        std::shared_ptr<iiSharedCanvas::ChunkedBitmapEditor> tile_editor;
        std::shared_ptr<iiSharedCanvas::BitmapEditor> raster_editor;
        std::shared_ptr<const RasterStrokeMetadata> raster_metadata;
        std::optional<iiSharedCanvas::CanvasRegion> canvas_before, canvas_after;
        std::shared_ptr<const ChunkFrameHistory> chunk_frame;
        std::shared_ptr<const RasterInsertion> raster_insertion;
        std::size_t bytes() const noexcept {
            return before.size()+after.size()+(tile_editor ? tile_editor->historyStatistics().retainedBytes : 0)+
                (raster_editor ? raster_editor->historyStatistics().retainedBytes : 0)+
                (raster_metadata ? raster_metadata->bytes() : 0)+
                (canvas_before ? 2*sizeof(iiSharedCanvas::CanvasRegion) : 0)+(chunk_frame ? chunk_frame->bytes() : 0)+
                (raster_insertion ? raster_insertion->bytes() : 0)+(mask_change ?
                (mask_before ? mask_before->bytes() : 0)+(mask_after ? mask_after->bytes() : 0) : 0);
        }
    };
    bool clone_target_ready(iiSharedCanvas::FrameIndex);
    bool shape_impl(std::string, iiSharedCanvas::Point, iiSharedCanvas::Point, std::uint32_t, double, bool,
                    PolygonOptions, iiSharedCanvas::FrameIndex, std::optional<ShapeStyle>);
    bool edit(const std::function<bool(iiSharedCanvas::DocumentEditor&)>&);
    bool commit(std::vector<std::uint8_t> before, std::string before_selection,
                bool history = true, std::optional<SelectionTransition> selection = std::nullopt);
    bool apply_history(bool redo);
    bool apply_raster_insertion(const RasterInsertion&, bool forward);
    bool insert_raster_result(std::variant<::RasterLayer, iiSharedCanvas::ChunkedRasterAsset>, std::string, iiSharedCanvas::CanvasOrigin,
                              iiSharedCanvas::FrameIndex, RasterBlendMode);
    bool sparse_gradient(iiSharedCanvas::GradientDefinition, iiSharedCanvas::FrameIndex, RasterBlendMode);
    bool finish_compact_chunk_stroke(const ::BrushState*);
    bool commit_raster_stroke_history(std::shared_ptr<iiSharedCanvas::BitmapEditor>, std::string);
    bool commit_compact_chunk_history(std::shared_ptr<iiSharedCanvas::ChunkedBitmapEditor>, std::string,
                                      std::optional<iiSharedCanvas::CanvasRegion> before,
                                      std::optional<iiSharedCanvas::CanvasRegion> after,
                                      std::optional<ChunkFrameHistory> frame = std::nullopt);
    static std::optional<FrameSourceHistory> capture_frame_source(const iiSharedCanvas::Document&, const std::string&);
    bool restore_frame_source(const std::string&, const FrameSourceHistory&);
    bool change_selection(std::shared_ptr<const iiSharedCanvas::RasterMask>);
    bool change_selection_value(std::shared_ptr<const SelectionValue>);
    [[nodiscard]] std::uint8_t selection_coverage(double world_x, double world_y) const noexcept;
    [[nodiscard]] std::optional<iiSharedCanvas::CanvasRegion> selection_bounds() const noexcept;
    [[nodiscard]] std::uint8_t stroke_chunk_coverage(double local_x, double local_y) const noexcept;
    bool combine_selection(iiSharedCanvas::RasterMask, iiSharedCanvas::MaskOperation);
    bool combine_tiled_selection(iiSharedCanvas::TiledMask, iiSharedCanvas::MaskOperation);
    bool reference_pixels(iiSharedCanvas::FrameIndex, bool current_layer, ::RasterLayer&);
    bool bounded_current_reference(iiSharedCanvas::FrameIndex, iiSharedCanvas::CanvasRegion&, ::RasterLayer&);
    bool bounded_composite_reference(iiSharedCanvas::FrameIndex, iiSharedCanvas::CanvasRegion&, ::RasterLayer&, std::uint32_t& exterior);
    bool sparse_current_wand(iiSharedCanvas::Point, iiSharedCanvas::FrameIndex, double, iiSharedCanvas::TiledMask&);
    bool tiled_wand_mask(iiSharedCanvas::Point, iiSharedCanvas::FrameIndex, double, bool, bool, bool, iiSharedCanvas::TiledMask&);
    bool sparse_fill(const iiSharedCanvas::TiledMask&, std::uint32_t, iiSharedCanvas::FrameIndex);
    bool fill_tiled_new_layer(iiSharedCanvas::Point, std::uint32_t, iiSharedCanvas::FrameIndex, FloodOptions);
    bool fill_tiled_selection(iiSharedCanvas::Point, std::uint32_t, iiSharedCanvas::FrameIndex, FloodOptions);
    bool fill_tiled_chunks(iiSharedCanvas::Point, std::uint32_t, iiSharedCanvas::FrameIndex, FloodOptions, const iiSharedCanvas::ChunkedRasterAsset&, const ::AffineTransform&);
    bool bind();
    // Copy-on-write a bitmap for exactly one timeline frame; caller owns rollback.
    bool replace_frame_bitmap(iiSharedCanvas::DocumentEditor&, iiSharedCanvas::FrameIndex,
                              ::RasterLayer, std::string& asset_id);
    bool replace_frame_chunked(iiSharedCanvas::DocumentEditor&, iiSharedCanvas::FrameIndex,
                               iiSharedCanvas::ChunkedRasterAsset, std::string& asset_id);
    bool grow(::DocumentPoint, double);
    std::vector<std::uint8_t> capture() const;
    bool fail(std::string);
    bool reject_stroke(std::string);
    void record_stroke_damage(::DevicePixelRect) noexcept;
    void collect_stroke_damage() noexcept;
    std::filesystem::path directory_, recovery_, source_;
    std::optional<CanvasDefinition> creation_;
    iiSharedCanvas::DocumentFile file_;
    iiSharedCanvas::Document draft_;
    iiSharedCanvas::BitmapEditor raster_;
    iiSharedCanvas::ChunkedBitmapEditor chunks_;
    ::AffineTransform stroke_transform_{};
    ::DevicePixelRect stroke_dirty_total_{}, stroke_dirty_pending_{};
    double legacy_stroke_time_ = 0;
    std::string selected_, error_, stroke_selection_;
    std::vector<std::uint8_t> stroke_before_;
    std::shared_ptr<const SelectionValue> selection_;
    std::optional<::RasterLayer> stroke_original_;
    std::optional<iiSharedCanvas::ChunkedRasterAsset> stroke_original_chunks_;
    std::optional<iiSharedCanvas::CanvasRegion> stroke_asset_region_;
    std::optional<ChunkFrameHistory> stroke_frame_history_;
    bool stroke_frame_copy_ = false;
    bool stroke_compact_ = false;
    bool stroke_raster_history_ = false;
    struct TaperStroke final {
        StrokeTaper options;
        std::vector<::StrokePoint> samples;
        bool eraser = false;
    };
    std::optional<TaperStroke> taper_stroke_;
    std::deque<History> undo_, redo_;
    std::size_t history_bytes_ = 0;
};
} // namespace congregation::native
