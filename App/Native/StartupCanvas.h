#pragma once
#include "CanvasDefinition.h"
#include <iiGUI/Host.h>
#include <cstdint>
#include <expected>
#include <string>

namespace congregation::native {
// The SDK reports physical display pixels, so no backing-scale multiplier is
// applied here. Invalid displays never silently become a different canvas size.
inline std::expected<CanvasDefinition,std::string> startup_canvas_definition(ii::gui::DisplayPixels pixels) {
    if (pixels.width<1 || pixels.height<1 || pixels.width>CanvasMaximumDimension || pixels.height>CanvasMaximumDimension ||
        std::uint64_t(pixels.width)*std::uint64_t(pixels.height)>CanvasMaximumPixels)
        return std::unexpected("The display resolution exceeds the canvas size limit");
    CanvasDefinition definition;
    definition.width=definition.pixel_width=pixels.width;
    definition.height=definition.pixel_height=pixels.height;
    definition.background="Transparent";
    return definition;
}
}
