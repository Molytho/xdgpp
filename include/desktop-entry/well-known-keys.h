#ifndef WELL_KNOWN_KEY_STORAGE_H
#define WELL_KNOWN_KEY_STORAGE_H

#include <cstdint>
#include <iostream>
#include <memory>
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

    template<class T>
    struct required_container {
        T m_value;

    public:
        constexpr explicit operator bool() const noexcept { return true; }

        constexpr operator const T &() const & noexcept { return m_value; }

        constexpr const T &get() const noexcept { return m_value; }

        constexpr T &init_and_get() noexcept { return m_value; }
    };

    template<class T>
    struct optional_container {
        std::unique_ptr<T> m_value;

    public:
        explicit operator bool() const noexcept { return static_cast<bool>(m_value); }

        const T &get() const {
            if (!m_value) {
                throw std::logic_error("Tried to access null object");
            }
            return *m_value;
        }

        const T *get_or_nullptr() const { return *this ? std::addressof(get()) : nullptr; }

        T &init_and_get() {
            if (!m_value) {
                m_value = std::make_unique<T>();
            }
            return *m_value;
        }
    };

    template<>
    struct optional_container<types::boolean> {
        bool m_value;
        bool m_set;

    public:
        constexpr explicit operator bool() const noexcept { return m_set; }

        constexpr bool get() const noexcept { return m_value; }

        constexpr bool get_or_default(bool def) const noexcept { return m_set ? m_value : def; }

        constexpr bool &init_and_get() noexcept {
            if (!m_set) {
                m_set = true;
            }
            return m_value;
        }
    };

    template<well_known_keys k>
    struct required {
        static constexpr well_known_keys key = k;
        using container                      = required_container<well_known_key_type_t<k>>;
    };

    template<well_known_keys k>
    struct optional {
        static constexpr well_known_keys key = k;
        using container                      = optional_container<well_known_key_type_t<k>>;
    };

    namespace detail {
        template<class... Args>
        constexpr size_t calculate_well_known_key_index(well_known_keys key) {
            std::array keys {Args::key...};
            auto it = std::ranges::find(keys, key);
            return it != keys.end() ? std::distance(keys.begin(), it) : -1;
        }
    } // namespace detail

    template<class... Ts>
    class well_known_key_storage {
        std::tuple<typename Ts::container...> m_storage;

    public:
        template<well_known_keys key>
        constexpr decltype(auto) get() const {
            constexpr size_t Index = detail::calculate_well_known_key_index<Ts...>(key);
            if constexpr (Index != -1) {
                return std::get<Index>(m_storage);
            } else {
                throw std::logic_error("Tried to get well_know_key that is not contained");
            }
        }

        template<well_known_keys key>
        constexpr decltype(auto) get() {
            constexpr size_t Index = detail::calculate_well_known_key_index<Ts...>(key);
            if constexpr (Index != -1) {
                return std::get<Index>(m_storage);
            } else {
                throw std::logic_error("Tried to get well_know_key that is not contained");
            }
        }
    };

    // clang-format off
    using desktop_entry_storage = well_known_key_storage<
        required<well_known_keys::Name>,

        optional<well_known_keys::NoDisplay>,
        optional<well_known_keys::Hidden>,

        optional<well_known_keys::Version>,
        optional<well_known_keys::GenericName>,
        optional<well_known_keys::Comment>,
        optional<well_known_keys::Icon>,
        optional<well_known_keys::OnlyShowIn>,
        optional<well_known_keys::NotShowIn>
    >;

    using application_entry_storage = well_known_key_storage<
        optional<well_known_keys::DBusActivatable>,
        optional<well_known_keys::Terminal>,
        optional<well_known_keys::StartupNotify>,
        optional<well_known_keys::PrefersNonDefaultGPU>,
        optional<well_known_keys::SingleMainWindow>,

        optional<well_known_keys::TryExec>,
        optional<well_known_keys::Exec>,
        optional<well_known_keys::Path>,
        // Actions are not needed
        optional<well_known_keys::MimeType>,
        optional<well_known_keys::Categories>,
        optional<well_known_keys::Implements>,
        optional<well_known_keys::Keywords>,
        optional<well_known_keys::StartupWMClass>
    >;

    using desktop_action_storage = well_known_key_storage<
        required<well_known_keys::Name>,
        optional<well_known_keys::Icon>,
        optional<well_known_keys::Exec>
    >;

    using link_entry_storage = well_known_key_storage<
        required<well_known_keys::URL>
    >;
    // clang-format on
} // namespace xdg::desktop_entry_spec

#undef DEFINE_TYPE_OF_WELL_KNOWN_KEY

#endif