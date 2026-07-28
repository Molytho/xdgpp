#ifndef LIB_XDGPP_PRIVATE_DESKTOP_ENTRY_PRIVATE_H
#define LIB_XDGPP_PRIVATE_DESKTOP_ENTRY_PRIVATE_H

#include "desktop-entry.h"
#include "desktop-entry/well-known-keys.h"

namespace xdg::desktop_entry_spec::detail {
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

        std::string m_id;
        application_entry_storage m_application_storage;
        std::vector<application_action> m_actions;
    };

    desktop_entry parse_desktop_entry(std::istream &is);
} // namespace xdg::desktop_entry_spec::detail

#endif