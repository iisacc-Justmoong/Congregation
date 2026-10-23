#pragma once
#include <iiSharedCanvasNative.h>
#include <Engine/RecentCanvas.h>
namespace congregation::native
{
std::expected<iiSharedCanvas::Document, std::string>
migrate_legacy_canvas(const congregation::RecentCanvas&);
}
