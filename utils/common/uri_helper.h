#ifndef COMMON_URI_HELPER_H
#define COMMON_URI_HELPER_H

#include <string_view>

std::string_view get_uri_schema(std::string_view str);
bool is_uri(std::string_view str);

#endif