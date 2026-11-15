#include "ToolShortcuts.h"

#include <algorithm>
#include <array>

namespace congregation::native {
namespace {
using namespace std::literals;

constexpr std::array move{"move"sv};
constexpr std::array marquee{"rect-marquee"sv, "ellipse-marquee"sv};
constexpr std::array lasso{"lasso"sv, "polygon-lasso"sv, "magnetic-lasso"sv};
constexpr std::array wand{"magic-wand"sv};
constexpr std::array crop{"crop"sv, "perspective-crop"sv};
constexpr std::array picker{"color-picker"sv};
constexpr std::array brush{"brush"sv, "pencil"sv, "airbrush"sv, "mixer-brush"sv,
                           "dry-brush"sv, "bristle-brush"sv};
constexpr std::array eraser{"eraser"sv};
constexpr std::array fill{"gradient"sv, "paint-bucket"sv};
constexpr std::array soften{"smudge"sv, "blur"sv, "sharpen"sv};
constexpr std::array stamp{"clone-stamp"sv, "pattern-stamp"sv};
constexpr std::array healing{"spot-heal"sv, "healing-brush"sv};
constexpr std::array tone{"dodge"sv, "burn"sv, "sponge"sv};
constexpr std::array distort{"liquify"sv, "warp"sv};
constexpr std::array pen{"pen"sv, "curvature-pen"sv, "add-anchor"sv, "delete-anchor"sv};
constexpr std::array direct{"direct-select"sv};
constexpr std::array shape{"rectangle"sv, "ellipse"sv, "polygon"sv};
constexpr std::array text{"area-text"sv, "path-text"sv};
constexpr std::array hand{"hand"sv};
constexpr std::array rotate{"rotate-view"sv};
constexpr std::array zoom{"imagezoomIn"sv};
constexpr std::array mask{"quick-mask"sv};

constexpr std::array groups{
    ToolShortcutGroup{'v', move}, ToolShortcutGroup{'m', marquee},
    ToolShortcutGroup{'l', lasso}, ToolShortcutGroup{'w', wand},
    ToolShortcutGroup{'c', crop}, ToolShortcutGroup{'i', picker},
    ToolShortcutGroup{'b', brush}, ToolShortcutGroup{'e', eraser},
    ToolShortcutGroup{'g', fill}, ToolShortcutGroup{'k', soften},
    ToolShortcutGroup{'s', stamp}, ToolShortcutGroup{'j', healing},
    ToolShortcutGroup{'o', tone}, ToolShortcutGroup{'y', distort},
    ToolShortcutGroup{'p', pen}, ToolShortcutGroup{'a', direct},
    ToolShortcutGroup{'u', shape}, ToolShortcutGroup{'t', text},
    ToolShortcutGroup{'h', hand}, ToolShortcutGroup{'r', rotate},
    ToolShortcutGroup{'z', zoom}, ToolShortcutGroup{'q', mask},
};
} // namespace

std::span<const ToolShortcutGroup> tool_shortcut_groups() noexcept { return groups; }

std::optional<ToolShortcutCommand>
ToolShortcuts::resolve(const ii::gui::WidgetInput& event, std::string_view selected_tool) {
    using Kind = ToolShortcutCommand::Kind;
    using Key = ii::gui::WidgetKey;
    if (event.kind != ii::gui::WidgetInputKind::key_down || event.text_input_active)
        return std::nullopt;

    if (event.control || event.meta) {
        if (event.alt) return std::nullopt;
        if (!event.shift && event.key == Key::up)
            return ToolShortcutCommand{.kind = Kind::brush_size, .step = 1};
        if (!event.shift && event.key == Key::down)
            return ToolShortcutCommand{.kind = Kind::brush_size, .step = -1};
        if (event.key == Key::character && (event.text == "r" || event.text == "R"))
            return ToolShortcutCommand{.kind = event.shift ? Kind::toggle_symmetry : Kind::toggle_rulers};
        return std::nullopt;
    }

    if (event.key != Key::character || event.text.size() != 1 || event.shift || event.alt)
        return std::nullopt;
    const char input = event.text[0];
    const char key = input >= 'A' && input <= 'Z' ? char(input - 'A' + 'a') : input;
    const auto group = std::ranges::find(groups, key, &ToolShortcutGroup::key);
    if (group == groups.end()) return std::nullopt;

    std::size_t next = 0;
    const auto active = std::ranges::find(group->tools, selected_tool);
    const bool remembered = group_ == group->key && selected_ == selected_tool;
    if (remembered || active != group->tools.end()) {
        const auto current = remembered ? index_ : std::size_t(active - group->tools.begin());
        next = (current + 1) % group->tools.size();
    }
    group_ = group->key;
    index_ = next;
    return ToolShortcutCommand{.kind = Kind::select_tool, .tool = group->tools[next]};
}

void ToolShortcuts::observe_selection(std::string_view selected_tool) {
    selected_ = selected_tool;
}
} // namespace congregation::native
