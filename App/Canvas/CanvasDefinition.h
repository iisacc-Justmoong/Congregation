#pragma once
#include <string>
#include <cstdint>

namespace congregation::native
{
inline constexpr int CanvasMaximumDimension = 32768;
inline constexpr std::uint64_t CanvasMaximumPixels = 256ULL * 1024 * 1024;
// Authored dimensions plus the checked pixel extent. This is document data,
// independent of the chooser's native controls and presentation tokens.
struct CanvasDefinition final
{
    double width = 0, height = 0, ppi = 300;
    int pixel_width = 0, pixel_height = 0;
    std::string unit = "px", background = "White";
    std::string name, preset_id, category, section;
};
} // namespace congregation::native
