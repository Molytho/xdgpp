#ifndef LIB_XDGPP_DESKTOP_ENTRY_PARSER_H
#define LIB_XDGPP_DESKTOP_ENTRY_PARSER_H

#include "desktop-entry.h"

namespace xdg::desktop_entry_spec::detail {
    desktop_entry parse_desktop_entry(std::istream &is);
} // namespace xdg::desktop_entry_spec::detail

#endif