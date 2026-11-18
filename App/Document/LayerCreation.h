#pragma once
#include <array>
#include <span>
#include <string_view>

namespace congregation::native
{
enum class NewLayerType {
    raster, vector, animated_raster, animated_vector, video, audio,
    semantic_segment, pose, depth, line_art, canny, scribble, mlsd,
    normal_map, shuffle, tile, reference, ip_adapter
};
enum class LayerSourceRequirement { none, native_layer };
struct NewLayerOption final {
    NewLayerType type;
    std::string_view id, label;
    LayerSourceRequirement source;
};
inline constexpr std::array layer_creation_options{
    NewLayerOption{NewLayerType::raster, "raster", "Raster layer", LayerSourceRequirement::none},
    NewLayerOption{NewLayerType::vector, "vector", "Vector layer", LayerSourceRequirement::none},
    NewLayerOption{NewLayerType::animated_raster, "animated-raster", "Animated raster", LayerSourceRequirement::none},
    NewLayerOption{NewLayerType::animated_vector, "animated-vector", "Animated vector", LayerSourceRequirement::none},
    NewLayerOption{NewLayerType::video, "video", "Video…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::audio, "audio", "Audio track", LayerSourceRequirement::none},
    NewLayerOption{NewLayerType::semantic_segment, "segment", "Segment…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::pose, "pose", "Pose…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::depth, "depth", "Depth…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::line_art, "line-art", "Line art…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::canny, "canny", "Canny…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::scribble, "scribble", "Scribble…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::mlsd, "mlsd", "MLSD…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::normal_map, "normal-map", "Normal map…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::shuffle, "shuffle", "Shuffle…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::tile, "tile", "Tile…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::reference, "reference", "Reference…", LayerSourceRequirement::native_layer},
    NewLayerOption{NewLayerType::ip_adapter, "ip-adapter", "IP-Adapter…", LayerSourceRequirement::native_layer}
};
inline constexpr std::span<const NewLayerOption> new_layer_options() { return layer_creation_options; }
} // namespace congregation::native
