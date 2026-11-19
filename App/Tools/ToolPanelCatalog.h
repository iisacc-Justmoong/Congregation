#pragma once

#include <span>
#include <string_view>

namespace congregation::native::tool_panels {

enum class FieldKind { section, value, slider, choice, checkbox };

struct Field final {
    std::string_view label;
    std::string_view value;
    FieldKind kind;
    bool checked;
    double position;
};

struct Panel final {
    std::string_view tool;
    std::string_view title;
    std::string_view figma_node;
    bool color_picker;
    std::span<const Field> fields;
};

[[nodiscard]] std::span<const Panel> panels() noexcept;
[[nodiscard]] const Panel* find(std::string_view tool) noexcept;
// These brushes share the LVUI parameter section and the inspector's Opacity control.
[[nodiscard]] bool uses_brush_parameters(std::string_view tool) noexcept;

} // namespace congregation::native::tool_panels
