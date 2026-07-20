#ifndef LIB_XDGPP_DESKTOP_ENTRY_H
#define LIB_XDGPP_DESKTOP_ENTRY_H

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "desktop-entry/well-know-keys.h"
#include "helper.h"

/* ---------- class launch_impl ---------- */
namespace xdg::desktop_entry_spec {
    class application_action;
    class desktop_entry;

    struct launch_parameters {
        std::string command_list;
        std::string_view working_directory;
        bool terminal;
    };

    namespace API_PUBLIC detail {
        std::string make_command_line(std::string_view exec, std::string_view name,
            std::vector<std::string> launch_args, bool are_uri);

        std::string_view launch_impl_get_path(const application_action *action);
        std::string_view launch_impl_get_path(const desktop_entry *entry);

        bool launch_impl_get_terminal(const application_action *action);
        bool launch_impl_get_terminal(const desktop_entry *entry);

        template<class Derived>
        class launch_impl {
            const Derived *get_derived() const noexcept {
                return static_cast<const Derived *>(this);
            }

        public:
            template<class F>
                requires std::is_invocable_v<F, launch_parameters &>
            void launch(F &&launcher, std::vector<std::string> args, bool are_uris) const {
                launch_parameters params {
                    .command_list
                    = make_command_line(get_derived()->get_exec(), get_derived()->get_name().get(), std::move(args), are_uris),
                    .working_directory = launch_impl_get_path(get_derived()),
                    .terminal          = launch_impl_get_terminal(get_derived())
                };
                std::invoke(std::forward<F>(launcher), params);
            }

            template<class F>
                requires std::is_invocable_v<F, launch_parameters &>
            void launch(F &&launcher) const {
                launch(std::forward<F>(launcher), {}, false);
            }
        };
    } // namespace API_PUBLIC detail
} // namespace xdg::desktop_entry_spec

namespace xdg::desktop_entry_spec {
    namespace detail {
        class desktop_entry_parser;
    } // namespace detail

    class application_action : public detail::launch_impl<application_action> {
    public:
        application_action(std::weak_ptr<desktop_entry> entry, std::string id);

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

        std::shared_ptr<desktop_entry> try_get_entry() const { return m_entry.lock(); }

    private:
        friend detail::desktop_entry_parser;

        std::weak_ptr<desktop_entry> m_entry;
        std::string m_id;
        desktop_action_storage m_storage;
    };

    class API_PUBLIC desktop_entry : public detail::launch_impl<desktop_entry> {
        struct constructor_tag {
            explicit constructor_tag() = default;
        };

    public:
        static std::shared_ptr<desktop_entry> create();
        static std::shared_ptr<desktop_entry> from_istream(std::istream &is);
        static std::shared_ptr<desktop_entry> from_store(std::filesystem::path store,
            std::filesystem::path relative_path);

        types::entry_type get_type() const noexcept {
            return m_storage.get<well_known_keys::Type>();
        }

        std::string_view get_version() const {
            auto &opt = m_storage.get<well_known_keys::Version>();
            return opt ? opt.get() : std::string_view();
        }

        const types::localestring &get_name() const noexcept {
            return m_storage.get<well_known_keys::Name>();
        }

        const types::localestring *get_generic_name() const noexcept {
            return m_storage.get<well_known_keys::GenericName>().get_or_nullptr();
        }

        bool get_no_display() const noexcept {
            return m_storage.get<well_known_keys::NoDisplay>().get_or_default(false);
        }

        const types::localestring *get_comment() const noexcept {
            return m_storage.get<well_known_keys::Comment>().get_or_nullptr();
        }

        const types::localestring *get_icon() const noexcept {
            return m_storage.get<well_known_keys::Icon>().get_or_nullptr();
        }

        bool get_hidden() const noexcept {
            return m_storage.get<well_known_keys::Hidden>().get_or_default(false);
        }

        const types::strings *get_only_show_in() const noexcept {
            return m_storage.get<well_known_keys::OnlyShowIn>().get_or_nullptr();
        }

        const types::strings *get_not_show_in() const noexcept {
            return m_storage.get<well_known_keys::NotShowIn>().get_or_nullptr();
        }

        bool get_dbus_activatable() const noexcept {
            return m_storage.get<well_known_keys::DBusActivatable>().get_or_default(false);
        }

        std::string_view get_try_exec() const noexcept {
            auto &opt = m_storage.get<well_known_keys::TryExec>();
            return opt ? opt.get() : std::string_view();
        }

        std::string_view get_exec() const noexcept {
            auto &opt = m_storage.get<well_known_keys::Exec>();
            return opt ? opt.get() : std::string_view();
        }

        std::string_view get_path() const noexcept {
            auto &opt = m_storage.get<well_known_keys::Path>();
            return opt ? opt.get() : std::string_view();
        }

        bool get_terminal() const noexcept {
            return m_storage.get<well_known_keys::Terminal>().get_or_default(false);
        }

        const std::vector<std::shared_ptr<application_action>> &get_actions() const noexcept {
            return m_actions;
        }

        const types::strings *get_mime_types() const noexcept {
            return m_storage.get<well_known_keys::MimeType>().get_or_nullptr();
        }

        const types::strings *get_categories() const noexcept {
            return m_storage.get<well_known_keys::Categories>().get_or_nullptr();
        }

        const std::vector<std::string> *get_implements() const noexcept {
            return m_storage.get<well_known_keys::Implements>().get_or_nullptr();
        }

        const types::localestrings *get_keywords() const noexcept {
            return m_storage.get<well_known_keys::Keywords>().get_or_nullptr();
        }

        bool get_startup_notify() const noexcept {
            return m_storage.get<well_known_keys::StartupNotify>().get_or_default(false);
        }

        std::string_view get_startup_wm_class() const noexcept {
            auto &opt = m_storage.get<well_known_keys::StartupWMClass>();
            return opt ? opt.get() : std::string_view();
        }

        std::string_view get_url() const {
            auto &opt = m_storage.get<well_known_keys::URL>();
            return opt ? opt.get() : std::string_view();
        }

        bool get_prefers_non_default_gpu() const noexcept {
            return m_storage.get<well_known_keys::PrefersNonDefaultGPU>().get_or_default(false);
        }

        bool get_single_main_window() const noexcept {
            return m_storage.get<well_known_keys::SingleMainWindow>().get_or_default(false);
        }

        std::string get_id() const;

        bool should_show() const;

        friend std::istream &operator>>(std::istream &is, const std::shared_ptr<desktop_entry> &entry);

        desktop_entry(constructor_tag);

    private:
        friend detail::desktop_entry_parser;

        std::filesystem::path m_store {};
        std::filesystem::path m_relative_path {};

        desktop_entry_storage m_storage;
        std::vector<std::shared_ptr<application_action>> m_actions {};
    };

    API_PUBLIC std::vector<std::shared_ptr<desktop_entry>> get_all_desktop_entries();
} // namespace xdg::desktop_entry_spec
#endif