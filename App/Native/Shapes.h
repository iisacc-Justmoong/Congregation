#pragma once
#include <iiSharedCanvasNative.h>
#include <string_view>
namespace congregation::native
{
iiSharedCanvas::VectorPath shape_path(std::string_view kind, double width, double height,
                                      std::uint32_t color, double stroke, bool filled);
}
