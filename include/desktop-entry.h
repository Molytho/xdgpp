#ifndef LIB_XDGPP_DESKTOP_ENTRY_H
#define LIB_XDGPP_DESKTOP_ENTRY_H

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "desktop-entry/types.h"
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
    namespace detail {
        class desktop_entry_data;
        class application_action_data;
        class application_entry_data;
    } // namespace detail

    class desktop_entry;
    class application_action;
    class application_entry;
    class application_entry_action;

    class API_PUBLIC desktop_entry {
    public:
        static desktop_entry from_istream(std::istream &is);
        static desktop_entry from_store(std::filesystem::path store, std::filesystem::path relative_path);

        desktop_entry() = default;
        desktop_entry(std::shared_ptr<detail::desktop_entry_data>);

        desktop_entry(const desktop_entry &);
        desktop_entry &operator=(const desktop_entry &);
        desktop_entry(desktop_entry &&);
        desktop_entry &operator=(desktop_entry &&);

        types::entry_type get_type() const noexcept;
        std::string_view get_version() const noexcept;
        const types::localestring &get_name() const noexcept;
        const types::localestring *get_generic_name() const noexcept;
        bool get_no_display() const noexcept;
        const types::localestring *get_comment() const noexcept;
        const types::localestring *get_icon() const noexcept;
        bool get_hidden() const noexcept;
        const types::strings *get_only_show_in() const noexcept;
        const types::strings *get_not_show_in() const noexcept;

        bool should_show() const noexcept;

    protected:
        std::shared_ptr<detail::desktop_entry_data> m_data;
    };

    class API_PUBLIC application_action {
    public:
        application_action() = default;

        application_action(std::string name);
        application_action(std::shared_ptr<detail::application_action_data> data);

        application_action(const application_action &);
        application_action &operator=(const application_action &);
        application_action(application_action &&) noexcept;
        application_action &operator=(application_action &&) noexcept;

        explicit operator bool() const noexcept;

        const std::string &get_id() const noexcept;
        const types::localestring &get_name() const noexcept;
        const types::localestring *get_icon() const noexcept;
        std::string_view get_exec() const noexcept;

    private:
        std::shared_ptr<detail::application_action_data> m_data;
    };

    class API_PUBLIC application_entry : public desktop_entry, public detail::launch_impl<application_entry> {
    public:
        application_entry();

        application_entry(const application_entry &);
        application_entry &operator=(const application_entry &);
        application_entry(application_entry &&);
        application_entry &operator=(application_entry &&);

        explicit application_entry(desktop_entry entry);

        bool get_dbus_activatable() const noexcept;
        std::string_view get_try_exec() const noexcept;
        std::string_view get_exec() const noexcept;
        std::string_view get_path() const noexcept;
        bool get_terminal() const noexcept;
        void add_actions(application_action new_action);
        std::vector<application_entry_action> get_actions() const;
        const types::strings *get_mime_types() const noexcept;
        const types::strings *get_categories() const noexcept;
        const types::strings *get_implements() const noexcept;
        const types::localestrings *get_keywords() const noexcept;
        bool get_startup_notify() const noexcept;
        std::string_view get_startup_wm_class() const noexcept;
        bool get_prefers_non_default_gpu() const noexcept;
        bool get_single_main_window() const noexcept;
        const types::application_id &get_id() const noexcept;
        void set_id(types::application_id id) noexcept;

    private:
        detail::application_entry_data *get_ptr() const noexcept;
    };

    class API_PUBLIC application_entry_action :
            public application_action,
            public detail::launch_impl<application_entry_action> {
    public:
        application_entry_action(application_entry entry, application_action data);

        application_entry_action(const application_entry_action &);
        application_entry_action &operator=(const application_entry_action &);
        application_entry_action(application_entry_action &&) noexcept;
        application_entry_action &operator=(application_entry_action &&) noexcept;

        [[deprecated("try_get_entry can no longer fail, thus it was renamed to get_entry")]]
        application_entry try_get_entry() const;
        application_entry get_entry() const;

    private:
        application_entry m_entry;
    };

    std::istream &operator>>(std::istream &is, desktop_entry &entry);

    API_PUBLIC std::vector<desktop_entry> get_all_desktop_entries();
    API_PUBLIC std::vector<application_entry> get_all_application_entries();
    API_PUBLIC application_entry search_application_entry(types::application_id id);
} // namespace xdg::desktop_entry_spec
#endif