#include "spawn_helper.h"

#include <cassert>
#include <string>

#include "molytho-cpp-utils/systemd_utils.h"

using namespace molytho::utils;

[[nodiscard]] std::string make_id(const xdg::desktop_entry_spec::application_entry &entry) {
    auto id = std::string(entry.get_id());
    if (id.empty()) {
        id = entry.get_name().get();
    } else {
        assert(id.ends_with(".desktop"));
        id.resize(id.size() - 8);
    }
    id = systemd::escape_string(id, false);
    return id;
}