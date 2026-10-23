#pragma once
#include <iiGUI/Image.h>
namespace congregation::native::editor
{
// Preserved Vincent/LVRS vectors; independent of later SDK icon recoloring.
[[nodiscard]] std::shared_ptr<const ii::gui::RasterImage> icon(std::string_view name);
}
