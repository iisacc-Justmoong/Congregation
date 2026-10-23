#include "Palette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace congregation
{
namespace
{
struct Bucket final
{
    PaletteEntry entry;
    int hue = 0;
    int saturation = 0;
    int lightness = 0;
};

Bucket bucket(const PaletteEntry& entry)
{
    const auto& color = entry.color;
    const double maximum = std::max({color.red, color.green, color.blue});
    const double minimum = std::min({color.red, color.green, color.blue});
    const double lightness = (maximum + minimum) / 2;
    double hue = 0;
    double saturation = 0;
    if (maximum != minimum)
    {
        const double delta = maximum - minimum;
        saturation =
            lightness > 0.5 ? delta / (2 - maximum - minimum) : delta / (maximum + minimum);
        if (maximum == color.red)
        {
            hue = (color.green - color.blue) / delta + (color.green < color.blue ? 6 : 0);
        }
        else if (maximum == color.green)
        {
            hue = (color.blue - color.red) / delta + 2;
        }
        else
        {
            hue = (color.red - color.green) / delta + 4;
        }
        hue /= 6;
    }
    return {entry, int(std::lround(hue * 360)), int(std::lround(saturation * 100)),
            int(std::lround(lightness * 100))};
}
} // namespace

std::vector<PaletteEntry> buildPalette(std::span<const PaletteEntry> primary,
                                       std::span<const PaletteEntry> extended)
{
    std::unordered_set<std::uint32_t> seen;
    std::vector<Bucket> neutrals;
    std::vector<Bucket> colored;
    const auto capacity = primary.size() + extended.size();
    seen.reserve(capacity);
    neutrals.reserve(capacity);
    colored.reserve(capacity);
    auto append = [&](const PaletteEntry& entry)
    {
        auto color = entry.color;
        if (!color.valid || !std::isfinite(color.red) || !std::isfinite(color.green) ||
            !std::isfinite(color.blue) || !std::isfinite(color.alpha))
        {
            return;
        }
        color.red = std::clamp(color.red, 0.0, 1.0);
        color.green = std::clamp(color.green, 0.0, 1.0);
        color.blue = std::clamp(color.blue, 0.0, 1.0);
        const auto rgb = (std::uint32_t(std::lround(color.red * 255)) << 16) |
                         (std::uint32_t(std::lround(color.green * 255)) << 8) |
                         std::uint32_t(std::lround(color.blue * 255));
        if (!seen.insert(rgb).second)
        {
            return;
        }
        auto value = bucket({entry.name, color});
        (value.saturation < 15 ? neutrals : colored).push_back(std::move(value));
    };
    for (const auto& entry : primary)
        append(entry);
    for (const auto& entry : extended)
        append(entry);
    std::stable_sort(neutrals.begin(), neutrals.end(), [](const Bucket& left, const Bucket& right)
                     { return left.lightness < right.lightness; });
    std::stable_sort(colored.begin(), colored.end(),
                     [](const Bucket& left, const Bucket& right)
                     {
                         return left.hue == right.hue ? left.lightness < right.lightness
                                                      : left.hue < right.hue;
                     });
    std::vector<PaletteEntry> result;
    result.reserve(seen.size());
    for (const auto& value : neutrals)
        if (value.lightness < 50)
            result.push_back(value.entry);
    for (const auto& value : colored)
        result.push_back(value.entry);
    for (const auto& value : neutrals)
        if (value.lightness >= 50)
            result.push_back(value.entry);
    return result;
}

std::vector<PaletteEntry> defaultPalette()
{
    const std::array primary{PaletteEntry{"Ink Black", Color::fromRgb(0x1a1a1a)},
                             PaletteEntry{"Signal Red", Color::fromRgb(0xe53935)},
                             PaletteEntry{"Amber", Color::fromRgb(0xfb8c00)},
                             PaletteEntry{"Sun Yellow", Color::fromRgb(0xfdd835)},
                             PaletteEntry{"Leaf Green", Color::fromRgb(0x43a047)},
                             PaletteEntry{"Sky Blue", Color::fromRgb(0x1e88e5)},
                             PaletteEntry{"Violet", Color::fromRgb(0x5e35b1)},
                             PaletteEntry{"Clay", Color::fromRgb(0x8d6e63)},
                             PaletteEntry{"Pure White", Color::fromRgb(0xffffff)},
                             PaletteEntry{"Pitch Black", Color::fromRgb(0x000000)}};
    const std::array extended{PaletteEntry{"Coral", Color::fromRgb(0xff7043)},
                              PaletteEntry{"Rose", Color::fromRgb(0xf06292)},
                              PaletteEntry{"Lilac", Color::fromRgb(0xba68c8)},
                              PaletteEntry{"Cerulean", Color::fromRgb(0x0091ea)},
                              PaletteEntry{"Seafoam", Color::fromRgb(0x26c6da)},
                              PaletteEntry{"Forest", Color::fromRgb(0x2e7d32)},
                              PaletteEntry{"Olive", Color::fromRgb(0x827717)},
                              PaletteEntry{"Burnt Sienna", Color::fromRgb(0xd84315)},
                              PaletteEntry{"Slate", Color::fromRgb(0x546e7a)}};
    return buildPalette(primary, extended);
}
} // namespace congregation
