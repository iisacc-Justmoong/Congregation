#include <Tools/ToolShortcuts.h>

#include <algorithm>
#include <chrono>
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

// Drive key presses against a fixed clock so the timing boundary is reproducible.
void brush_opacity_shortcuts() {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::time_point{};
    ToolShortcuts shortcuts;
    const auto expect_percent = [&](const WidgetInput& event, auto time, int percent,
                                    std::string_view tool = "brush") {
        const auto command = shortcuts.resolve(event, tool, time);
        check(command && command->kind == ToolShortcutCommand::Kind::brush_opacity &&
                  command->opacity_percent == percent,
              "Unexpected opacity for " + event.text + ": expected " + std::to_string(percent));
    };

    expect_percent(letter("7"), start, 70);
    expect_percent(letter("2"), start + 301ms, 20);
    auto released = letter("2");
    released.kind = WidgetInputKind::key_up;
    check(!shortcuts.resolve(released, "brush", start + 350ms), "Key release changed opacity");
    expect_percent(letter("2"), start + 410ms, 22);
    expect_percent(letter("0"), start + 520ms, 100);
    expect_percent(letter("0"), start + 810ms, 0);
    expect_percent(letter("0"), start + 1000ms, 100, "pencil");
    expect_percent(letter("5"), start + 1090ms, 5, "pencil");
    expect_percent(letter("9"), start + 1500ms, 90, "airbrush");
    auto repeat = letter("9");
    repeat.key_repeat = true;
    check(!shortcuts.resolve(repeat, "airbrush", start + 1560ms), "A held digit was treated as a second press");
    expect_percent(letter("3"), start + 1800ms, 93, "airbrush");

    auto keypad = letter("7");
    keypad.numeric_keypad = true;
    auto editing = letter("7");
    editing.text_input_active = true;
    auto control = letter("7");
    control.control = true;
    for (const auto& ignored : {keypad, editing, released, control, letter("7", true),
                                letter("7", false, true), letter("7", false, false, true), letter("22")})
        check(!shortcuts.resolve(ignored, "brush", start + 1900ms),
              "Only unmodified number-row key presses may set brush opacity");
    check(!shortcuts.resolve(letter("7"), "move", start + 1900ms),
          "A digit changed opacity while Move was selected");
    check(!shortcuts.resolve(letter("7"), "eraser", start + 1900ms),
          "A digit changed opacity outside the Brush tool group");

    expect_percent(letter("4"), start + 2000ms, 40);
    check(!shortcuts.resolve(editing, "brush", start + 2050ms), "An editor key changed opacity");
    expect_percent(letter("5"), start + 2110ms, 50);
    expect_percent(letter("6"), start + 2150ms, 60, "pencil");
    const WidgetInput press{.kind = WidgetInputKind::pointer_down};
    check(!shortcuts.resolve(press, "pencil", start + 2200ms), "Pointer input was consumed");
    expect_percent(letter("5"), start + 2260ms, 50, "pencil");
    expect_percent(letter("6"), start + 2310ms, 56, "pencil");
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
    brush_opacity_shortcuts();
    std::cout << "Tool mapping, brush opacity, modifiers and focus protection passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
