#pragma once

#include "CanvasDocument.h"

#include <span>
#include <vector>

namespace congregation
{
struct PaletteEntry final
{
    std::string name;
    Color color;
};
[[nodiscard]] std::vector<PaletteEntry> buildPalette(std::span<const PaletteEntry> primary,
                                                     std::span<const PaletteEntry> extended);
[[nodiscard]] std::vector<PaletteEntry> defaultPalette();
} // namespace congregation
