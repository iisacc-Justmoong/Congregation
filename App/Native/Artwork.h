#pragma once
#include "CanvasDefinition.h"
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <deque>
#include <functional>
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
// Product editing policy. Canonical storage/rasterization remain SDK services.
class Artwork final
{
  public:
    explicit Artwork(std::filesystem::path recovery_directory);
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
    void cancel();
    bool shape(std::string kind, iiSharedCanvas::Point from, iiSharedCanvas::Point to,
               std::uint32_t color, double width, bool filled);
    bool fill(std::uint32_t color);
    bool fill_at(iiSharedCanvas::Point, std::uint32_t color,
                 iiSharedCanvas::FrameIndex frame = 0);
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
    struct History final
    {
        std::vector<std::uint8_t> before, after;
        std::string before_selection, after_selection;
    };
    bool edit(const std::function<bool(iiSharedCanvas::DocumentEditor&)>&);
    bool commit(std::vector<std::uint8_t> before, std::string before_selection,
                bool history = true);
    bool apply_history(bool redo);
    bool bind();
    bool grow(::DocumentPoint, double);
    std::vector<std::uint8_t> capture() const;
    bool fail(std::string);
    std::filesystem::path directory_, recovery_, source_;
    std::optional<CanvasDefinition> creation_;
    iiSharedCanvas::DocumentFile file_;
    iiSharedCanvas::Document draft_;
    iiSharedCanvas::BitmapEditor raster_;
    iiSharedCanvas::ChunkedBitmapEditor chunks_;
    ::AffineTransform stroke_transform_{};
    std::string selected_, error_, stroke_selection_;
    std::vector<std::uint8_t> stroke_before_;
    std::deque<History> undo_, redo_;
    std::size_t history_bytes_ = 0;
};
} // namespace congregation::native
