#pragma once

#include <iiGUI>

#include <chrono>
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

// An edit intent for the application dispatcher. Each kind uses its own
// payload so absolute opacity is independent of the relative size step.
struct ToolShortcutCommand final {
    enum class Kind { select_tool, toggle_rulers, toggle_symmetry, brush_size, brush_opacity };
    Kind kind;
    std::string_view tool{};
    int step = 0;
    int opacity_percent = 0;
};

// Keeps a cursor even when a requested tool is unavailable and the editor's
// selected tool therefore does not change. A mouse selection reanchors it.
class ToolShortcuts final {
public:
    using Clock = std::chrono::steady_clock;

    // Returns a command for a new key press. An explicit monotonic time lets
    // the caller exercise consecutive presses without delaying event delivery.
    [[nodiscard]] std::optional<ToolShortcutCommand>
    resolve(const ii::gui::WidgetInput&, std::string_view selected_tool,
            Clock::time_point now = Clock::now());
    // Records the actual selection after dispatching a tool-cycle command.
    void observe_selection(std::string_view selected_tool);

private:
    // The first digit has already been applied as a decade. Its lifetime is
    // limited to one more physical press on the same brush within the window.
    struct OpacityDigit final {
        std::string_view tool;
        int digit;
        Clock::time_point pressed_at;
    };

    // Applies the first digit immediately and resolves the next digit as an
    // exact percentage when it completes a pair.
    [[nodiscard]] ToolShortcutCommand opacity_digit(int digit, std::string_view tool, Clock::time_point now);

    std::optional<char> group_;
    std::size_t index_ = 0;
    std::string selected_;
    std::optional<OpacityDigit> opacity_digit_;
};

} // namespace congregation::native
