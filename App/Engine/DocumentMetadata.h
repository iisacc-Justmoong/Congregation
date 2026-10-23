#pragma once
#include "Data.h"
namespace congregation
{
Object psd_layer(const Object&, int width, int height, int index, bool background = false);
Object psd_manifest(int width, int height, Array layers);
std::string xmp_text(std::string_view xml, std::string_view local_name);
Array member_profiles(const Array&, std::string name, std::string image, bool host);
} // namespace congregation
