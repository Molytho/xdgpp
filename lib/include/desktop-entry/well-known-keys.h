#ifndef WELL_KNOWN_KEY_STORAGE_H
#define WELL_KNOWN_KEY_STORAGE_H

#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

#include "helper.h"
#include "types.h"

#define DEFINE_TYPE_OF_WELL_KNOWN_KEY(key, T)          \
    template<>                                         \
    struct well_known_key_type<well_known_keys::key> { \
        using type = types::T;                         \
    };

namespace xdg::desktop_entry_spec {
    enum class well_known_keys : uint8_t {
        Actions,
        Categories,
        Comment,
        DBusActivatable,
        Exec,
        GenericName,
        Hidden,
        Icon,
        Implements,
        Keywords,
        MimeType,
        Name,
        NoDisplay,
        NotShowIn,
        OnlyShowIn,
        Path,
        PrefersNonDefaultGPU,
        SingleMainWindow,
        StartupNotify,
        StartupWMClass,
        Terminal,
        TryExec,
        Type,
        URL,
        Version,
        First = Actions,
        Last  = Version,
        Count = Last - First + 1
    };
    API_PUBLIC std::string_view to_string(well_known_keys val) noexcept;
    API_PUBLIC std::optional<well_known_keys> well_known_keys_from_string(std::string_view str) noexcept;
    API_PUBLIC std::ostream &operator<<(std::ostream &os, well_known_keys val);
    API_PUBLIC std::istream &operator>>(std::istream &is, well_known_keys &out);

    template<well_known_keys key>
    struct well_known_key_type {
        static_assert("Type not defined");
    };

    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Actions, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Categories, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Comment, localestring);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(DBusActivatable, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Exec, string);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(GenericName, localestring);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Hidden, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Icon, iconstring);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Implements, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Keywords, localestrings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(MimeType, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Name, localestring);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(NoDisplay, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(NotShowIn, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(OnlyShowIn, strings);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Path, string);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(PrefersNonDefaultGPU, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(SingleMainWindow, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(StartupNotify, boolean)
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(StartupWMClass, string);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Terminal, boolean);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(TryExec, string);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Type, entry_type);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(URL, string);
    DEFINE_TYPE_OF_WELL_KNOWN_KEY(Version, string);

    template<well_known_keys key>
    using well_known_key_type_t = typename well_known_key_type<key>::type;
} // namespace xdg::desktop_entry_spec

#undef DEFINE_TYPE_OF_WELL_KNOWN_KEY

#endif