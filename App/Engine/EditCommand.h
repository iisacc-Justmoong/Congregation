#pragma once
#include "Data.h"
namespace congregation
{
std::optional<Object> normalize_command(const Object&);
std::string encode_command(const Object&);
std::optional<Object> decode_command(std::string_view);
} // namespace congregation
