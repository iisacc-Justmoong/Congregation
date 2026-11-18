#pragma once

#include <Serialization/IiscCodec.h>
#include <memory>
#include <optional>
#include <string>

namespace iiSharedCanvas { enum class LayerChangeField; }
namespace congregation::native
{
// Identifies the smallest document projection that must be recomputed.
// An empty layer or asset denotes an unknown scope and requires a safe fallback.
enum class DocumentChangeKind { reset, pixels, structure, name, properties, selection, frames };
struct DocumentChange final
{
    DocumentChangeKind kind = DocumentChangeKind::reset;
    std::string layer;
    std::string asset;
    unsigned frame = 0;
    // Asset-local bounds, when the edit can be represented as a pixel patch.
    std::optional<iiSharedCanvas::CanvasRegion> region;
    std::optional<iiSharedCanvas::LayerChangeField> property;
    // Present on durable runtime events. The initialization event has no edit.
    std::shared_ptr<const std::vector<std::uint8_t>> edit;
    std::shared_ptr<const iiSharedCanvas::IiscEditImpact> impact;
    bool timing = false;
    bool geometry = false;
    friend bool operator==(const DocumentChange&, const DocumentChange&) = default;
};
} // namespace congregation::native
