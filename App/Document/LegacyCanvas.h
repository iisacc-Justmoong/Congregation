#pragma once
#include <iiSharedCanvas>
#include <Document/RecentCanvas.h>
namespace congregation::native
{
std::expected<iiSharedCanvas::Document, std::string>
migrate_legacy_canvas(const congregation::RecentCanvas&);
}
