#ifndef LIB_XDGPP_PRIVATE_DESKTOP_ENTRY_PRIVATE_H
#define LIB_XDGPP_PRIVATE_DESKTOP_ENTRY_PRIVATE_H

#include "desktop-entry.h"
#include "desktop-entry/well-known-keys.h"

namespace xdg::desktop_entry_spec::detail {
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

    template<class... Args>
    constexpr size_t calculate_well_known_key_index(well_known_keys key) {
        std::array keys {Args::key...};
        auto it = std::ranges::find(keys, key);
        return it != keys.end() ? std::distance(keys.begin(), it) : -1;
    }

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

    class desktop_entry_data {
    public:
        virtual ~desktop_entry_data()                       = default;
        virtual types::entry_type get_type() const noexcept = 0;

        desktop_entry_storage m_common_storage;
    };

    class application_action_data {
    public:
        const std::string &get_id() const noexcept { return m_id; }

        const types::localestring &get_name() const noexcept {
            return m_storage.get<well_known_keys::Name>();
        }

        const types::localestring *get_icon() const noexcept {
            return m_storage.get<well_known_keys::Icon>().get_or_nullptr();
        }

        std::string_view get_exec() const noexcept {
            auto &opt = m_storage.get<well_known_keys::Exec>();
            return opt ? opt.get() : std::string_view();
        }

        desktop_action_storage &get_storage() noexcept { return m_storage; }

        std::string m_id;
        desktop_action_storage m_storage {};
    };

    class application_entry_data : public desktop_entry_data {
    public:
        types::entry_type get_type() const noexcept override {
            return types::entry_type::Application;
        }

        bool get_dbus_activatable() const noexcept {
            return m_application_storage.get<well_known_keys::DBusActivatable>().get_or_default(false);
        }

        void add_actions(application_action new_action) {
            m_actions.emplace_back(std::move(new_action));
        }

        types::application_id m_id;
        application_entry_storage m_application_storage;
        std::vector<application_action> m_actions;
    };

    desktop_entry parse_desktop_entry(std::istream &is);
} // namespace xdg::desktop_entry_spec::detail

#endif