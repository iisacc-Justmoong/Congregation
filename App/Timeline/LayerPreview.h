#pragma once

#include <iiGUI>
#include <iiSharedCanvas>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace congregation::native
{
// Identity only: the owning document invalidates previews on committed edits.
// Frame changes can retain a preview while its actual source is unchanged.
struct LayerPreviewKey final
{
    std::string asset;
    std::optional<iiSharedCanvas::FrameIndex> video_frame;
    friend bool operator==(const LayerPreviewKey&, const LayerPreviewKey&) = default;
};

[[nodiscard]] std::optional<LayerPreviewKey> layer_preview_key(
    const iiSharedCanvas::Document&, const iiSharedCanvas::Layer&, iiSharedCanvas::FrameIndex);
[[nodiscard]] std::shared_ptr<const ii::gui::RasterImage> render_layer_preview(
    const iiSharedCanvas::Document&, const iiSharedCanvas::Layer&, iiSharedCanvas::FrameIndex);
} // namespace congregation::native
