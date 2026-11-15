#pragma once

#include <iiGUI/NativeWidgets.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace congregation::native {

struct ToolShortcutGroup final {
    char key;
    std::span<const std::string_view> tools;
};

// Shared by the dispatcher and coverage tests. The order defines cycling.
[[nodiscard]] std::span<const ToolShortcutGroup> tool_shortcut_groups() noexcept;

struct ToolShortcutCommand final {
    enum class Kind { select_tool, toggle_rulers, toggle_symmetry, brush_size };
    Kind kind;
    std::string_view tool{};
    int step = 0;
};

// Keeps a cursor even when a requested tool is unavailable and the editor's
// selected tool therefore does not change. A mouse selection reanchors it.
class ToolShortcuts final {
public:
    [[nodiscard]] std::optional<ToolShortcutCommand>
    resolve(const ii::gui::WidgetInput&, std::string_view selected_tool);
    void observe_selection(std::string_view selected_tool);

private:
    std::optional<char> group_;
    std::size_t index_ = 0;
    std::string selected_;
};

} // namespace congregation::native
