#pragma once
#include <iiSharedCanvasNative.h>
#include <Stroke/StrokePoint.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace congregation::native {
// A detached mask edit session. It never writes the artwork document.
class QuickMask final {
public:
    static std::unique_ptr<QuickMask> create(iiSharedCanvas::CanvasExtent, const iiSharedCanvas::RasterMask*, std::string& error);
    static std::unique_ptr<QuickMask> create_tiled(iiSharedCanvas::TiledMask, std::string& error);
    bool begin(const ::StrokePoint&, iiSharedCanvas::BitmapBrush, bool subtract);
    bool extend(const ::StrokePoint&);
    bool end(const ::StrokePoint&);
    void cancel_stroke();
    bool undo();
    bool redo();
    bool stroke_active() const noexcept { return tiled_ ? tiled_->strokeActive() : editor_.strokeActive(); }
    bool changed() const;
    const ::RasterLayer* pixels() const noexcept { return tiled_ ? nullptr : editor_.pixels(); }
    bool tiled() const noexcept { return bool(tiled_); }
    ::DevicePixelRect dirty_bounds() const noexcept { return tiled_ ? ::DevicePixelRect{} : editor_.dirtyBounds(); }
    std::uint8_t coverage(std::int32_t x, std::int32_t y) const noexcept;
    const iiSharedCanvas::TiledMask* tiled_mask() const noexcept { return tiled_ ? &tiled_->mask() : nullptr; }
    std::optional<iiSharedCanvas::RasterMask> mask() const;
    const std::string& error() const noexcept { return error_; }
private:
    QuickMask() = default;
    iiSharedCanvas::Document document_;
    iiSharedCanvas::BitmapEditor editor_;
    std::unique_ptr<iiSharedCanvas::TiledMaskEditor> tiled_;
    std::vector<std::uint8_t> original_;
    std::string error_;
};
}
