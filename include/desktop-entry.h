#ifndef LIB_XDGPP_DESKTOP_ENTRY_H
#define LIB_XDGPP_DESKTOP_ENTRY_H

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

#include "desktop-entry/well-know-keys.h"
#include "helper.h"

/* ---------- class launch_impl ---------- */
namespace xdg::desktop_entry_spec {
    struct launch_parameters {
        std::string command_list;
        std::string_view working_directory;
        bool terminal;
    };

    namespace API_PUBLIC detail {
        std::string make_command_line(std::string_view exec, std::string_view name,
            std::vector<std::string> launch_args, bool are_uri);

        template<class Derived>
        class launch_impl {
            const Derived *get_derived() const noexcept {
                return static_cast<const Derived *>(this);
            }

            std::string_view get_path() const;
            bool get_terminal() const;

        public:
            template<class F>
                requires std::is_invocable_v<F, launch_parameters &>
            void launch(F &&launcher, std::vector<std::string> args, bool are_uris) const {
                launch_parameters params {
                    .command_list
                    = make_command_line(get_derived()->get_exec(), get_derived()->get_name().get(), std::move(args), are_uris),
                    .working_directory = get_path(),
                    .terminal          = get_terminal()
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
    class desktop_entry;
    class application_action;
    class application_entry;

    class API_PUBLIC desktop_entry {
    public:
        static std::shared_ptr<desktop_entry> from_istream(std::istream &is);
        static std::shared_ptr<desktop_entry> from_store(std::filesystem::path store,
            std::filesystem::path relative_path);

        virtual ~desktop_entry() = default;

        virtual types::entry_type get_type() const noexcept = 0;

        std::string_view get_version() const {
            auto &opt = m_common_storage.get<well_known_keys::Version>();
            return opt ? opt.get() : std::string_view();
        }

        const types::localestring &get_name() const noexcept {
            return m_common_storage.get<well_known_keys::Name>();
        }

        const types::localestring *get_generic_name() const noexcept {
            return m_common_storage.get<well_known_keys::GenericName>().get_or_nullptr();
        }

        bool get_no_display() const noexcept {
            return m_common_storage.get<well_known_keys::NoDisplay>().get_or_default(false);
        }

        const types::localestring *get_comment() const noexcept {
            return m_common_storage.get<well_known_keys::Comment>().get_or_nullptr();
        }

        const types::localestring *get_icon() const noexcept {
            return m_common_storage.get<well_known_keys::Icon>().get_or_nullptr();
        }

        bool get_hidden() const noexcept {
            return m_common_storage.get<well_known_keys::Hidden>().get_or_default(false);
        }

        const types::strings *get_only_show_in() const noexcept {
            return m_common_storage.get<well_known_keys::OnlyShowIn>().get_or_nullptr();
        }

        const types::strings *get_not_show_in() const noexcept {
            return m_common_storage.get<well_known_keys::NotShowIn>().get_or_nullptr();
        }

        bool should_show() const noexcept;

        desktop_entry_storage &get_storage() noexcept { return m_common_storage; }

    protected:
        desktop_entry() = default;

        desktop_entry_storage m_common_storage;
    };

    std::istream &operator>>(std::istream &is, std::shared_ptr<desktop_entry> &entry_ptr);

    class API_PUBLIC application_action : public detail::launch_impl<application_action> {
    public:
        class data {
        public:
            data(std::string name) : m_id(std::move(name)) { }

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

        private:
            std::string m_id;
            desktop_action_storage m_storage;
        };

        application_action(std::shared_ptr<const application_entry> entry, std::shared_ptr<data> data);

        application_action(const application_action &)            = default;
        application_action &operator=(const application_action &) = default;

        application_action(application_action &&) noexcept            = default;
        application_action &operator=(application_action &&) noexcept = default;

        const std::string &get_id() const noexcept { return m_data->get_id(); }

        const types::localestring &get_name() const noexcept { return m_data->get_name(); }

        const types::localestring *get_icon() const noexcept { return m_data->get_icon(); }

        std::string_view get_exec() const noexcept { return m_data->get_exec(); }

        [[deprecated("try_get_entry can no longer fail, thus it was renamed to get_entry")]]
        std::shared_ptr<const application_entry> try_get_entry() const {
            return m_entry;
        }

        std::shared_ptr<const application_entry> get_entry() const { return m_entry; }

        desktop_action_storage &get_storage() noexcept { return m_data->get_storage(); }

    private:
        std::shared_ptr<const application_entry> m_entry;
        std::shared_ptr<data> m_data;
    };

    class API_PUBLIC application_entry :
            public desktop_entry,
            public std::enable_shared_from_this<application_entry>,
            public detail::launch_impl<application_entry> {
        struct constructor_tag {
            explicit constructor_tag() = default;
        };

    public:
        static std::shared_ptr<application_entry> create();

        application_entry(constructor_tag);

        types::entry_type get_type() const noexcept override {
            return types::entry_type::Application;
        }

        bool get_dbus_activatable() const noexcept {
            return m_application_storage.get<well_known_keys::DBusActivatable>().get_or_default(false);
        }

        std::string_view get_try_exec() const noexcept {
            auto &opt = m_application_storage.get<well_known_keys::TryExec>();
            return opt ? opt.get() : std::string_view();
        }

        std::string_view get_exec() const noexcept {
            auto &opt = m_application_storage.get<well_known_keys::Exec>();
            return opt ? opt.get() : std::string_view();
        }

        std::string_view get_path() const noexcept {
            auto &opt = m_application_storage.get<well_known_keys::Path>();
            return opt ? opt.get() : std::string_view();
        }

        bool get_terminal() const noexcept {
            return m_application_storage.get<well_known_keys::Terminal>().get_or_default(false);
        }

        void add_actions(std::shared_ptr<application_action::data> new_action) {
            m_actions.emplace_back(std::move(new_action));
        }

        std::vector<application_action> get_actions() const noexcept {
            auto view = std::views::transform(m_actions, [&](std::shared_ptr<application_action::data> data) {
                return application_action(shared_from_this(), std::move(data));
            });
            return {begin(view), end(view)};
        }

        const types::strings *get_mime_types() const noexcept {
            return m_application_storage.get<well_known_keys::MimeType>().get_or_nullptr();
        }

        const types::strings *get_categories() const noexcept {
            return m_application_storage.get<well_known_keys::Categories>().get_or_nullptr();
        }

        const types::strings *get_implements() const noexcept {
            return m_application_storage.get<well_known_keys::Implements>().get_or_nullptr();
        }

        const types::localestrings *get_keywords() const noexcept {
            return m_application_storage.get<well_known_keys::Keywords>().get_or_nullptr();
        }

        bool get_startup_notify() const noexcept {
            return m_application_storage.get<well_known_keys::StartupNotify>().get_or_default(false);
        }

        std::string_view get_startup_wm_class() const noexcept {
            auto &opt = m_application_storage.get<well_known_keys::StartupWMClass>();
            return opt ? opt.get() : std::string_view();
        }

        bool get_prefers_non_default_gpu() const noexcept {
            return m_application_storage.get<well_known_keys::PrefersNonDefaultGPU>().get_or_default(false);
        }

        bool get_single_main_window() const noexcept {
            return m_application_storage.get<well_known_keys::SingleMainWindow>().get_or_default(false);
        }

        std::string_view get_id() const noexcept { return m_id; }

        void set_id(std::string id) noexcept { m_id = std::move(id); }

        application_entry_storage &get_storage() noexcept { return m_application_storage; }

    private:
        std::string m_id;
        application_entry_storage m_application_storage;
        std::vector<std::shared_ptr<application_action::data>> m_actions {};
    };

    API_PUBLIC std::vector<std::shared_ptr<desktop_entry>> get_all_desktop_entries();
    API_PUBLIC std::vector<std::shared_ptr<application_entry>> get_all_application_entries();
} // namespace xdg::desktop_entry_spec
#endif