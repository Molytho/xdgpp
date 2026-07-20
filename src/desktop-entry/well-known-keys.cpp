#include "desktop-entry/well-know-keys.h"

#include <algorithm>
#include <array>
#include <string_view>

using namespace std::string_view_literals;

namespace {
    constexpr std::array well_known_keys_name {
        "Actions"sv,
        "Categories"sv,
        "Comment"sv,
        "DBusActivatable"sv,
        "Exec"sv,
        "GenericName"sv,
        "Hidden"sv,
        "Icon"sv,
        "Implements"sv,
        "Keywords"sv,
        "MimeType"sv,
        "Name"sv,
        "NoDisplay"sv,
        "NotShowIn"sv,
        "OnlyShowIn"sv,
        "Path"sv,
        "PrefersNonDefaultGPU"sv,
        "SingleMainWindow"sv,
        "StartupNotify"sv,
        "StartupWMClass"sv,
        "Terminal"sv,
        "TryExec"sv,
        "Type"sv,
        "URL"sv,
        "Version"sv
    };
    static_assert(std::ranges::is_sorted(well_known_keys_name));
} // namespace

namespace xdg::desktop_entry_spec {
    std::string_view to_string(well_known_keys val) noexcept {
        return well_known_keys_name.at(size_t(val));
    }

    std::optional<well_known_keys> well_known_keys_from_string(std::string_view str) noexcept {
        auto it = std::ranges::lower_bound(well_known_keys_name, str);
        if (it != well_known_keys_name.end() && *it == str) {
            return {static_cast<well_known_keys>(std::distance(well_known_keys_name.begin(), it))};
        } else {
            return {};
        }
    }

    std::ostream &operator<<(std::ostream &os, well_known_keys val) {
        return os << to_string(val);
    }

    std::istream &operator>>(std::istream &is, well_known_keys &out) {
        std::string str;
        is >> str;
        auto res = well_known_keys_from_string(str);
        if (res) {
            out = *res;
        } else {
            is.setstate(std::ios_base::failbit);
        }
        return is;
    }
} // namespace xdg::desktop_entry_spec