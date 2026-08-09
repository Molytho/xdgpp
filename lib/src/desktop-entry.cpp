#include "desktop-entry.h"
#include "private/desktop-entry-private.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string_view>
#include <unordered_set>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "basedir.h"

using namespace std::string_view_literals;
using namespace xdg::desktop_entry_spec;
using namespace std::filesystem;

namespace {
    template<class F>
    std::vector<desktop_entry>
        read_desktop_entries_from_predicated_impl(std::vector<std::filesystem::path> stores, F &&predicate) {
        std::unordered_set<std::filesystem::path> files_read;
        std::vector<desktop_entry> result;

        for (const std::filesystem::path &application_dir : stores) {
            try {
                for (const auto &file : recursive_directory_iterator(application_dir,
                         directory_options::follow_directory_symlink)) {
                    if (file.path().extension() != ".desktop") {
                        continue;
                    }

                    auto relative_path = file.path().lexically_relative(application_dir);
                    if (auto [it, success] = files_read.emplace(relative_path); !success) {
                        // Already read from directory with higher priority
                        continue;
                    } else if (!predicate(relative_path)) {
                        // Ignored by caller
                        continue;
                    }

                    try {
                        result.emplace_back(desktop_entry::from_store(application_dir, relative_path));
                    } catch (const std::runtime_error &ex) {
                        std::cerr << "Failed to parse desktop file: " << file << '\n';
                        continue;
                    }
                }
            } catch (const filesystem_error &ex) {
                if (ex.code() == std::errc::no_such_file_or_directory) {
                    continue;
                }
                throw;
            }
        }

        return result;
    }
} // namespace

namespace xdg::desktop_entry_spec {
    template<class F>
    auto on_exit(F &&f) {
        struct impl {
            F f;

            ~impl() { std::move(f)(); }
        };

        return impl {std::forward<F>(f)};
    }

    namespace detail {
        std::string make_command_line(const make_command_line_interface &iface,
            std::vector<std::string> launch_args, bool are_uri) {
            auto exec = iface.get_exec();

            std::string cmdline {};
            cmdline.reserve(exec.size());

            auto current_it = exec.begin();
            auto next_pos   = exec.find('%');
            while (next_pos != exec.npos) {
                if (next_pos + 1 >= exec.size()) {
                    throw std::runtime_error("% at the end of the string");
                }

                auto next_it = exec.begin() + next_pos;
                cmdline.append(current_it, next_it);

                switch (*(next_it + 1)) {
                case 'f':
                    if (are_uri) {
                        throw std::runtime_error("Uris not supported");
                    }
                    [[fallthrough]];
                case 'u':
                    if (launch_args.size() > 1) {
                        throw std::runtime_error("Desktop file only supports one argument");
                    } else if (launch_args.size() == 1) {
                        cmdline.push_back('\"');
                        cmdline.append(launch_args.at(0));
                        cmdline.push_back('\"');
                    }
                    break;
                case 'F':
                    if (are_uri) {
                        throw std::runtime_error("Uris not supported");
                    }
                    [[fallthrough]];
                case 'U':
                    for (const auto &arg : launch_args) {
                        cmdline.push_back('\"');
                        cmdline.append(arg);
                        cmdline.push_back('\"');
                    }
                    break;
                case 'i': {
                    auto icon = iface.get_icon();
                    if (!icon.empty()) {
                        cmdline.append("--icon \"");
                        cmdline.append(icon);
                        cmdline.push_back('"');
                    }
                    break;
                }
                case 'c':
                    cmdline.append(iface.get_name());
                    break;
                case '%':
                    cmdline.push_back('%');
                    break;
                default:
                    throw std::runtime_error("Unsupported % escape");
                }

                current_it = next_it + 2;
                next_pos   = exec.find('%', next_pos + 1);
            }
            cmdline.append(current_it, exec.end());
            return cmdline;
        }

        template<>
        API_PUBLIC std::string_view launch_impl<application_entry_action>::get_path() const {
            return get_derived()->get_entry().get_path();
        }

        template<>
        API_PUBLIC std::string_view launch_impl<application_entry>::get_path() const {
            return get_derived()->get_path();
        }

        template<>
        API_PUBLIC bool launch_impl<application_entry_action>::get_terminal() const {
            return get_derived()->get_entry().get_terminal();
        }

        template<>
        API_PUBLIC bool launch_impl<application_entry>::get_terminal() const {
            return get_derived()->get_terminal();
        }

        template<class T>
        API_PUBLIC std::string launch_impl<T>::make_command_line(std::vector<std::string> args, bool are_uris) const {
            struct impl : make_command_line_interface {
                const T &action;

                impl(const T &a) : action(a) { }

                std::string_view get_exec() const override { return action.get_exec(); }

                std::string_view get_icon() const override {
                    auto icon = action.get_icon();
                    return icon ? icon->get() : std::string_view();
                }

                std::string_view get_name() const override { return action.get_name().get(); }
            };

            return xdg::desktop_entry_spec::detail::make_command_line(impl {*get_derived()}, std::move(args), are_uris);
        }

        template std::string launch_impl<application_entry>::make_command_line(std::vector<std::string> args,
            bool are_uris) const;
        template std::string launch_impl<application_entry_action>::make_command_line(
            std::vector<std::string> args, bool are_uris
        ) const;
    } // namespace detail

    desktop_entry desktop_entry::from_istream(std::istream &is) {
        on_exit([cur_state = is.exceptions(), &is]() { is.exceptions(cur_state); });
        auto entry = detail::parse_desktop_entry(is);
        // Throw if input failed
        is.exceptions(std::ios_base::failbit);
        return entry;
    }

    desktop_entry desktop_entry::from_path(std::filesystem::path path) {
        if (!exists(path)) {
            throw std::filesystem::filesystem_error("File does not exist",
                path,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        std::ifstream is {path};
        if (!is.is_open()) {
            throw std::runtime_error("Failed to open file");
        }

        return application_entry::from_istream(is);
    }

    desktop_entry desktop_entry::from_store(std::filesystem::path store, std::filesystem::path relative_path) {
        auto entry = from_path(store / relative_path);
        if (entry.get_type() == types::entry_type::Application) {
            application_entry aentry {std::move(entry)};
            aentry.set_id(types::application_id(relative_path));
            return aentry;
        } else {
            return entry;
        }
    }

    desktop_entry::desktop_entry(std::shared_ptr<detail::desktop_entry_data> data) :
            m_data(std::move(data)) { }

    desktop_entry::desktop_entry(const desktop_entry &)            = default;
    desktop_entry &desktop_entry::operator=(const desktop_entry &) = default;
    desktop_entry::desktop_entry(desktop_entry &&)                 = default;
    desktop_entry &desktop_entry::operator=(desktop_entry &&)      = default;

    types::entry_type desktop_entry::get_type() const noexcept {
        return m_data->get_type();
    }

    std::string_view desktop_entry::get_version() const noexcept {
        auto &opt = m_data->m_common_storage.get<well_known_keys::Version>();
        return opt ? opt.get() : std::string_view();
    }

    const types::localestring &desktop_entry::get_name() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::Name>();
    }

    const types::localestring *desktop_entry::get_generic_name() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::GenericName>().get_or_nullptr();
    }

    bool desktop_entry::get_no_display() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::NoDisplay>().get_or_default(false);
    }

    const types::localestring *desktop_entry::get_comment() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::Comment>().get_or_nullptr();
    }

    const types::localestring *desktop_entry::get_icon() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::Icon>().get_or_nullptr();
    }

    bool desktop_entry::get_hidden() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::Hidden>().get_or_default(false);
    }

    const types::strings *desktop_entry::get_only_show_in() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::OnlyShowIn>().get_or_nullptr();
    }

    const types::strings *desktop_entry::get_not_show_in() const noexcept {
        return m_data->m_common_storage.get<well_known_keys::NotShowIn>().get_or_nullptr();
    }

    bool desktop_entry::should_show() const noexcept {
        if (get_no_display() || get_hidden()) {
            return false;
        }

        auto current_desktop = []() {
            auto str = std::getenv("XDG_CURRENT_DESKTOP");
            return str ? std::string_view(str) : std::string_view();
        }();
        auto only_show_in = get_only_show_in();
        if (only_show_in) {
            auto it = std::ranges::find(*only_show_in, current_desktop);
            return it != only_show_in->end();
        }

        auto no_show_in = get_not_show_in();
        if (no_show_in) {
            auto it = std::ranges::find(*no_show_in, current_desktop);
            return it == no_show_in->end();
        }

        return true;
    }

    application_action::application_action(std::string name) :
            m_data(std::make_shared<detail::application_action_data>(name)) { }

    application_action::application_action(std::shared_ptr<detail::application_action_data> data) :
            m_data(std::move(data)) { }

    application_action::application_action(const application_action &)                = default;
    application_action &application_action::operator=(const application_action &)     = default;
    application_action::application_action(application_action &&) noexcept            = default;
    application_action &application_action::operator=(application_action &&) noexcept = default;

    application_action::operator bool() const noexcept {
        return m_data != nullptr;
    }

    const std::string &application_action::get_id() const noexcept {
        return m_data->get_id();
    }

    const types::localestring &application_action::get_name() const noexcept {
        return m_data->get_name();
    }

    const types::localestring *application_action::get_icon() const noexcept {
        return m_data->get_icon();
    }

    std::string_view application_action::get_exec() const noexcept {
        return m_data->get_exec();
    }

    application_entry::application_entry() :
            desktop_entry(std::make_shared<detail::application_entry_data>()) { }

    application_entry::application_entry(const application_entry &)            = default;
    application_entry &application_entry::operator=(const application_entry &) = default;
    application_entry::application_entry(application_entry &&)                 = default;
    application_entry &application_entry::operator=(application_entry &&)      = default;

    application_entry::application_entry(desktop_entry entry) :
            desktop_entry(
                entry.get_type() == types::entry_type::Application
                    ? std::move(entry)
                    : throw std::logic_error("Tried to cast invalid instance to application_entry")
            ) {
        assert(dynamic_cast<detail::application_entry_data *>(m_data.get()) != nullptr);
    }

    bool application_entry::get_dbus_activatable() const noexcept {
        return get_ptr()->get_dbus_activatable();
    }

    std::string_view application_entry::get_try_exec() const noexcept {
        auto &opt = get_ptr()->m_application_storage.get<well_known_keys::TryExec>();
        return opt ? opt.get() : std::string_view();
    }

    std::string_view application_entry::get_exec() const noexcept {
        auto &opt = get_ptr()->m_application_storage.get<well_known_keys::Exec>();
        return opt ? opt.get() : std::string_view();
    }

    std::string_view application_entry::get_path() const noexcept {
        auto &opt = get_ptr()->m_application_storage.get<well_known_keys::Path>();
        return opt ? opt.get() : std::string_view();
    }

    bool application_entry::get_terminal() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::Terminal>().get_or_default(false);
    }

    void application_entry::add_actions(application_action new_action) {
        get_ptr()->add_actions(std::move(new_action));
    }

    std::vector<application_entry_action> application_entry::get_actions() const {
        auto view = std::views::transform(get_ptr()->m_actions,
            [&](application_action data) { return application_entry_action(*this, std::move(data)); });
        return {begin(view), end(view)};
    }

    const types::strings *application_entry::get_mime_types() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::MimeType>().get_or_nullptr();
    }

    const types::strings *application_entry::get_categories() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::Categories>().get_or_nullptr();
    }

    const types::strings *application_entry::get_implements() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::Implements>().get_or_nullptr();
    }

    const types::localestrings *application_entry::get_keywords() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::Keywords>().get_or_nullptr();
    }

    bool application_entry::get_startup_notify() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::StartupNotify>().get_or_default(false);
    }

    std::string_view application_entry::get_startup_wm_class() const noexcept {
        auto &opt = get_ptr()->m_application_storage.get<well_known_keys::StartupWMClass>();
        return opt ? opt.get() : std::string_view();
    }

    bool application_entry::get_prefers_non_default_gpu() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::PrefersNonDefaultGPU>().get_or_default(false);
    }

    bool application_entry::get_single_main_window() const noexcept {
        return get_ptr()->m_application_storage.get<well_known_keys::SingleMainWindow>().get_or_default(false);
    }

    const types::application_id &application_entry::get_id() const noexcept {
        return get_ptr()->m_id;
    }

    void application_entry::set_id(types::application_id id) noexcept {
        get_ptr()->m_id = std::move(id);
    }

    detail::application_entry_data *application_entry::get_ptr() const noexcept {
        return static_cast<detail::application_entry_data *>(m_data.get());
    }

    application_entry_action::application_entry_action(application_entry entry, application_action data) :
            application_action(std::move(data)), m_entry(std::move(entry)) { }

    application_entry_action::application_entry_action(const application_entry_action &) = default;
    application_entry_action &application_entry_action::operator=(const application_entry_action &) = default;
    application_entry_action::application_entry_action(application_entry_action &&) noexcept = default;
    application_entry_action &application_entry_action::operator=(application_entry_action &&) noexcept = default;

    application_entry application_entry_action::try_get_entry() const {
        return m_entry;
    }

    application_entry application_entry_action::get_entry() const {
        return m_entry;
    }

    std::istream &operator>>(std::istream &is, desktop_entry &entry) {
        entry = detail::parse_desktop_entry(is);
        return is;
    }

    std::vector<desktop_entry> read_desktop_entries_from_predicated(std::vector<std::filesystem::path> stores,
        std::function<bool(const std::filesystem::path &)> predicate) {
        return read_desktop_entries_from_predicated_impl(std::move(stores), predicate);
    }

    std::vector<desktop_entry> read_desktop_entries_from_predicated(std::filesystem::path store,
        std::function<bool(const std::filesystem::path &)> predicate) {
        return read_desktop_entries_from_predicated_impl(std::vector {store}, predicate);
    }

    std::vector<desktop_entry> read_desktop_entries_from(std::vector<std::filesystem::path> stores) {
        return read_desktop_entries_from_predicated_impl(std::move(stores), [](auto) { return true; });
    }

    std::vector<desktop_entry> read_desktop_entries_from(std::filesystem::path store) {
        return read_desktop_entries_from(std::vector {store});
    }

    std::vector<desktop_entry> get_all_desktop_entries() {
        auto stores = basedir::get_data_dirs_by_priority();
        for (auto &store : stores) {
            store /= "applications";
        }
        return read_desktop_entries_from(std::move(stores));
    }

    std::vector<application_entry> get_all_application_entries() {
        auto view = std::views::filter(get_all_desktop_entries(), [](const desktop_entry &entry) {
            return entry.get_type() == types::entry_type::Application;
        }) | std::views::transform([](desktop_entry &entry) {
            return static_cast<application_entry>(std::move(entry));
        });
        return {begin(view), end(view)};
    }

    std::optional<search_result> search_application_entry(types::application_id id) {
        if (!std::string_view(id).ends_with(".desktop")) {
            return std::nullopt;
        }

        auto relative_paths = id.to_paths();
        for (auto &application_dir : basedir::get_data_dirs_by_priority()) {
            application_dir /= "applications";

            for (auto &relative_path : relative_paths) {
                try {
                    auto entry = desktop_entry::from_store(application_dir, relative_path);
                    if (entry.get_type() != types::entry_type::Application) {
                        return std::nullopt;
                    }
                    return std::optional<search_result> {
                        std::in_place,
                        std::move(application_dir),
                        std::move(relative_path),
                        application_entry(std::move(entry))
                    };
                } catch (const filesystem_error &ex) {
                    if (ex.code() != std::errc::no_such_file_or_directory) {
                        throw;
                    }
                } catch (const std::runtime_error &ex) {
                    std::cerr << "Failed to parse desktop file: " << application_dir / relative_path << '\n';
                    return std::nullopt;
                }
            }
        }
        return std::nullopt;
    }
} // namespace xdg::desktop_entry_spec
