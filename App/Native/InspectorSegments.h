#pragma once
#include <array>
#include <string_view>

namespace congregation::native {
struct InspectorSegment final { std::string_view icon, name; };
inline constexpr std::array<InspectorSegment, 4> inspector_segments{{
    {"tool", "Tools"}, {"generator", "Generative"},
    {"documentation", "Document"}, {"asset-library", "Asset Library"}
}};
} // namespace congregation::native
