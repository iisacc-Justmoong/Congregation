#include <Native/InspectorSegments.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <array>
#include <stdexcept>
#include <string_view>

int main() {
    using congregation::native::inspector_segments;
    constexpr std::array<std::string_view, 4> names{"Tools", "Generative", "Document", "Asset Library"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (inspector_segments[i].name != names[i]) throw std::runtime_error("Incorrect segment order");
        const auto image = lvui::native::design_icon_image_for_logical_size(inspector_segments[i].icon);
        if (!image || !*image) throw std::runtime_error("Missing LVUI segment icon");
    }
}
