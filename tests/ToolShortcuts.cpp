#include <Native/ToolShortcuts.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>

using namespace congregation::native;
using namespace ii::gui;

namespace {
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
WidgetInput letter(std::string value, bool shift = false, bool alt = false,
                   bool command = false) {
    return {.kind = WidgetInputKind::key_down, .key = WidgetKey::character,
            .shift = shift, .text = std::move(value), .alt = alt, .meta = command};
}
void selected(ToolShortcuts& shortcuts, std::string_view name) {
    shortcuts.observe_selection(name);
}
void expect(ToolShortcuts& shortcuts, WidgetInput event, std::string_view active,
            std::string_view tool) {
    const auto command = shortcuts.resolve(event, active);
    check(command && command->kind == ToolShortcutCommand::Kind::select_tool &&
              command->tool == tool,
          "Unexpected shortcut result for " + event.text + ": expected " + std::string(tool));
}
}

int main(int argc, char** argv) try {
    check(argc == 2, "Expected the declarative toolbar path");
    std::set<char> keys;
    std::set<std::string> tools;
    for (const auto& group : tool_shortcut_groups()) {
        check(keys.insert(group.key).second, "Duplicate tool-group key");
        check(!group.tools.empty(), "Empty tool group");
        for (const auto tool : group.tools)
            check(tools.insert(std::string(tool)).second, "Duplicate tool: " + std::string(tool));
    }

    std::ifstream view(argv[1]);
    check(bool(view), "Could not open the declarative toolbar");
    const std::string source{std::istreambuf_iterator<char>(view), {}};
    const std::regex binding(R"(onClick:\s*vm\.choose_tool_([A-Za-z0-9_]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), binding), end; it != end; ++it) {
        std::string id = (*it)[1];
        std::replace(id.begin(), id.end(), '_', '-');
        if (id == "rulers") continue; // A separate Command/Ctrl toggle.
        check(tools.erase(id) == 1, "Toolbar tool has no unique shortcut: " + id);
    }
    check(tools.empty(), "Shortcut references a tool absent from the toolbar");

    ToolShortcuts already_selected;
    expect(already_selected, letter("b"), "brush", "pencil");
    ToolShortcuts shortcuts;
    expect(shortcuts, letter("b"), "move", "brush");
    selected(shortcuts, "brush");
    expect(shortcuts, letter("B"), "brush", "pencil");
    selected(shortcuts, "pencil");
    expect(shortcuts, letter("b"), "pencil", "airbrush");
    selected(shortcuts, "airbrush");
    // A mouse-selected tool in the same group becomes the new cycle anchor.
    expect(shortcuts, letter("b"), "bristle-brush", "brush");
    selected(shortcuts, "brush");
    expect(shortcuts, letter("m"), "brush", "rect-marquee");
    selected(shortcuts, "rect-marquee");
    expect(shortcuts, letter("m"), "rect-marquee", "ellipse-marquee");
    selected(shortcuts, "ellipse-marquee");
    expect(shortcuts, letter("l"), "ellipse-marquee", "lasso");
    selected(shortcuts, "lasso");

    // If an unavailable tool leaves selection unchanged, cycling must still advance.
    expect(shortcuts, letter("j"), "brush", "spot-heal");
    selected(shortcuts, "brush");
    expect(shortcuts, letter("j"), "brush", "healing-brush");
    selected(shortcuts, "brush");
    expect(shortcuts, letter("j"), "brush", "spot-heal");
    selected(shortcuts, "brush");
    // A selection made elsewhere supersedes the remembered unavailable item.
    expect(shortcuts, letter("b"), "airbrush", "mixer-brush");
    selected(shortcuts, "mixer-brush");

    auto bad = letter("b", true);
    check(!shortcuts.resolve(bad, "brush"), "Shift must not cycle tools");
    bad = letter("b", false, true);
    check(!shortcuts.resolve(bad, "brush"), "Alt must not cycle tools");
    bad = letter("b", true, true);
    check(!shortcuts.resolve(bad, "brush"), "Shift+Alt must not choose a tool");
    bad = letter("b"); bad.text_input_active = true;
    check(!shortcuts.resolve(bad, "brush"), "Text input must suppress tool shortcuts");
    bad = letter("b"); bad.kind = WidgetInputKind::key_up;
    check(!shortcuts.resolve(bad, "brush"), "Key release must not choose a tool");
    bad = letter("b", false, true, true);
    check(!shortcuts.resolve(bad, "brush"), "Command+Alt must not choose a tool");
    check(!shortcuts.resolve(letter("s", false, false, true), "brush"),
          "Command+S must remain available to Save");

    auto ruler = shortcuts.resolve(letter("r", false, false, true), "brush");
    check(ruler && ruler->kind == ToolShortcutCommand::Kind::toggle_rulers,
          "Command+R did not toggle rulers");
    auto symmetry = shortcuts.resolve(letter("r", true, false, true), "brush");
    check(symmetry && symmetry->kind == ToolShortcutCommand::Kind::toggle_symmetry,
          "Command+Shift+R did not toggle symmetry");
    WidgetInput up{.kind = WidgetInputKind::key_down, .key = WidgetKey::up, .control = true};
    auto plus = shortcuts.resolve(up, "brush");
    check(plus && plus->kind == ToolShortcutCommand::Kind::brush_size && plus->step == 1,
          "Control+Up did not request the precise positive step");
    up.key = WidgetKey::down;
    auto minus = shortcuts.resolve(up, "brush");
    check(minus && minus->kind == ToolShortcutCommand::Kind::brush_size && minus->step == -1,
          "Control+Down did not request the precise negative step");
    up.shift = true;
    check(!shortcuts.resolve(up, "brush"), "Undocumented modified size step was accepted");
    std::cout << "Tool shortcut mapping, toolbar coverage, modifiers and focus protection passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
