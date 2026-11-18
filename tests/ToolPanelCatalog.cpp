#include <Tools/ToolPanelCatalog.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

using namespace congregation::native::tool_panels;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    const auto all = panels();
    require(all.size() == 47, "The Figma tool panel inventory is incomplete");
    std::unordered_set<std::string_view> identifiers;
    for (const auto& panel : all) {
        require(!panel.tool.empty() && !panel.title.empty() && !panel.figma_node.empty(), "A tool panel lacks its identity");
        require(identifiers.insert(panel.tool).second, "Two tool panels share an identity");
        require(panel.tool == "brush" || !panel.fields.empty(), "A tool-specific panel has no content");
    }
    const auto has = [](const Panel& panel, std::string_view label) {
        return std::ranges::any_of(panel.fields, [&](const auto& field) { return field.label == label; });
    };
    const auto* move = find("move");
    const auto* pencil = find("pencil");
    const auto* hand = find("hand");
    const auto* lasso = find("lasso");
    const auto* polygon = find("polygon-lasso");
    require(move && has(*move, "Auto select") && has(*move, "Position X"), "Move panel differs from Figma");
    require(pencil && has(*pencil, "Pixel-aligned edges") && has(*pencil, "Pressure controls size"), "Pencil panel is incomplete");
    require(hand && has(*hand, "Enable inertia") && !has(*hand, "Pixel-aligned edges"), "Hand panel reused the wrong tool");
    require(lasso && polygon && has(*lasso, "Minimum sample distance") && has(*polygon, "Angle increment"), "Lasso variants were merged");
    require(pencil->color_picker && !move->color_picker && !hand->color_picker, "Color picker visibility differs from Figma");
    require(find("brush") && find("brush")->color_picker && !find("nonexistent"), "Brush or unknown panel lookup is incorrect");
    std::cout << "TOOL_PANEL_CATALOG_OK\n";
}
