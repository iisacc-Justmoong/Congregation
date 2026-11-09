#pragma once
#include <iiGUI/Graphics.h>
#include <algorithm>
#include <cstddef>
#include <map>
#include <tuple>

namespace congregation::native {
using CanvasTileKey = std::tuple<int, int, int, int>;

// Leave room in the same native frame for the rest of the application's UI.
inline constexpr std::size_t canvas_tile_layer_budget = ii::gui::maximum_canvas_overlays / 2;

// A batch retains one raster per visible document layer and output tile until
// composition completes. Limit that working set to at most sixteen 512px
// layer tiles when possible; a single tile is always allowed.
inline constexpr std::size_t canvas_composition_max_tiles = 4;
constexpr std::size_t canvas_composition_batch_size(std::size_t layer_count) noexcept {
    constexpr std::size_t layer_tile_budget = 16;
    return std::clamp(layer_tile_budget / std::max<std::size_t>(1, layer_count),
                      std::size_t{1}, canvas_composition_max_tiles);
}

constexpr std::size_t canvas_tile_group_size(std::size_t count, std::size_t budget) noexcept {
    budget = std::max<std::size_t>(1, budget);
    return std::max<std::size_t>(1, count / budget + (count % budget != 0));
}

struct CanvasVisibleTiles final {
    int lod, left, top, right, bottom, frame;
    std::size_t count;

    [[nodiscard]] bool contains(const CanvasTileKey& key) const noexcept {
        const auto [level, column, row, frame_index] = key;
        return level == lod && frame_index == frame && column >= left && column <= right &&
               row >= top && row <= bottom;
    }
};

template<class Value>
void trim_canvas_tile_cache(std::map<CanvasTileKey, Value>& cache, CanvasVisibleTiles visible,
                            std::size_t capacity) {
    const auto target = std::max(capacity, visible.count);
    for (auto it = cache.begin(); it != cache.end() && cache.size() > target;) {
        if (visible.contains(it->first)) ++it;
        else it = cache.erase(it);
    }
}
} // namespace congregation::native
