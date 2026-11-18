#pragma once

#include <cstdint>

namespace congregation
{
struct Color final
{
    double red = 0;
    double green = 0;
    double blue = 0;
    double alpha = 1;
    bool valid = true;

    static constexpr Color fromRgb(std::uint32_t rgb)
    {
        return {double((rgb >> 16) & 255) / 255, double((rgb >> 8) & 255) / 255,
                double(rgb & 255) / 255, 1};
    }
    bool operator==(const Color&) const = default;
};
} // namespace congregation
