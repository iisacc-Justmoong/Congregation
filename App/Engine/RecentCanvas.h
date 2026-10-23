#pragma once
#include "Data.h"
#include <expected>
namespace congregation
{
struct EmbeddedAsset final
{
    int objectId = 0;
    std::uint8_t kind = 1;
    std::string png;
};
struct RecentCanvas final
{
    std::string document;
    Array objects;
    std::vector<EmbeddedAsset> assets;
    bool background = true;
};
inline constexpr std::size_t maximum_canvas_bytes = 512ULL * 1024 * 1024;
std::expected<std::string, std::string> encode_canvas(const RecentCanvas&);
std::expected<RecentCanvas, std::string> decode_canvas(std::string_view);
} // namespace congregation
