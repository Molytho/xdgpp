#include "desktop-entry.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "basedir.h"
#include "private/desktop-entry-parser.h"

using namespace std::string_view_literals;
using namespace xdg::desktop_entry_spec;
using namespace std::filesystem;

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

    constexpr std::array entry_type_name {"Application"sv, "Link"sv, "Directory"sv};

    class xdg_data_dir_iterator {
        std::forward_list<path> m_str;

    public:
        xdg_data_dir_iterator() : m_str({xdg::basedir::get_data_home()}) {
            auto dirs = xdg::basedir::get_data_dirs();
            m_str.insert_after(m_str.begin(),
                std::make_move_iterator(dirs.begin()),
                std::make_move_iterator(dirs.end()));
        }

        path &operator*() {
            assert(!m_str.empty());
            return m_str.front();
        }

        path *operator->() {
            assert(!m_str.empty());
            return std::addressof(m_str.front());
        }

        xdg_data_dir_iterator &operator++() {
            assert(!m_str.empty());
            m_str.pop_front();
            return *this;
        }

        bool operator==(std::default_sentinel_t) const noexcept { return m_str.empty(); }
    };

    xdg_data_dir_iterator begin(xdg_data_dir_iterator it) {
        return it;
    }

    std::default_sentinel_t end(const xdg_data_dir_iterator &) {
        return {};
    }

    class alternative_locales_iterator {
        std::string_view m_modifier;
        locale m_locale;

    public:
        alternative_locales_iterator(locale locale) :
                m_modifier(locale.parse().modifier), m_locale(std::move(locale)) {
            m_locale.strip_encoding();
        }

        alternative_locales_iterator(std::string_view str) :
                alternative_locales_iterator(locale(str)) { }

        const locale &operator*() const noexcept { return m_locale; }

        alternative_locales_iterator &operator++() {
            auto parsed_current = m_locale.parse();

            assert(!parsed_current.lang.empty());
            if (parsed_current.lang.empty()) {
                throw std::logic_error("Tried to increment end iterator");
            } else if (parsed_current.modifier.empty() && parsed_current.country.empty()) {
                m_locale = {};
            } else if (!parsed_current.modifier.empty()) {
                std::string new_str {parsed_current.lang};
                if (!parsed_current.country.empty()) {
                    new_str.push_back('_');
                    new_str.append(parsed_current.country);
                }
                m_locale = locale(std::move(new_str));
            } else if (!parsed_current.country.empty()) {
                std::string new_str {parsed_current.lang};
                if (!m_modifier.empty()) {
                    new_str.push_back('@');
                    new_str.append(m_modifier);
                }
                m_locale = locale(std::move(new_str));
            }

            return *this;
        }

        bool operator==(std::default_sentinel_t) const noexcept { return m_locale.str().empty(); }
    };

    inline alternative_locales_iterator begin(alternative_locales_iterator it) {
        return it;
    }

    constexpr std::default_sentinel_t end(const alternative_locales_iterator &) {
        return {};
    }
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

    std::string_view to_string(entry_type val) noexcept {
        return entry_type_name.at(size_t(val));
    }

    std::optional<entry_type> entry_type_from_string(std::string_view str) noexcept {
        for (size_t i = 0; i < entry_type_name.size(); i++) {
            if (str == entry_type_name.at(i)) {
                return static_cast<entry_type>(i);
            }
        }
        return {};
    }

    std::ostream &operator<<(std::ostream &os, entry_type type) {
        return os << to_string(type);
    }

    std::istream &operator>>(std::istream &is, entry_type &out) {
        std::string str;
        is >> str;
        auto res = entry_type_from_string(str);
        if (res) {
            out = *res;
        } else {
            is.setstate(std::ios_base::failbit);
        }
        return is;
    }

    parsed_locale locale::parse() const noexcept {
        parsed_locale res {};

        std::string_view lang {m_str};

        auto delimiter = lang.find_first_of("_.@");
        res.lang       = lang.substr(0, delimiter);

        if (delimiter != std::string_view::npos && lang.at(delimiter) == '_') {
            auto start_pos = delimiter + 1;
            delimiter      = lang.find_first_of(".@", start_pos);
            res.country    = lang.substr(start_pos, delimiter - start_pos);
        }

        if (delimiter != std::string_view::npos && lang.at(delimiter) == '.') {
            auto start_pos = delimiter + 1;
            delimiter      = lang.find_first_of("@", start_pos);
            res.encoding   = lang.substr(start_pos, delimiter - start_pos);
        }

        if (delimiter != std::string_view::npos) {
            assert(lang.at(delimiter) == '@');
            res.modifier = lang.substr(delimiter + 1);
        }

        return res;
    }

    void locale::strip_encoding() {
        auto parsed = parse();
        std::string res {parsed.lang};
        if (!parsed.country.empty()) {
            res.push_back('_');
            res.append(parsed.country);
        }
        if (!parsed.modifier.empty()) {
            res.push_back('@');
            res.append(parsed.modifier);
        }
        m_str = std::move(res);
    }

    namespace detail {
        template<class T>
        void localized_data<T>::add(std::string_view lang, T val) {
            if (lang.empty()) {
                set_generic(std::move(val));
            } else {
                add_translated(lang, std::move(val));
            }
        }

        template<class T>
        void localized_data<T>::set_generic(T val) {
            m_generic = std::move(val);
        }

        template<class T>
        void localized_data<T>::add_translated(std::string_view lang, T val) {
            m_translations.emplace(lang, std::move(val));
        }

        template<class T>
        const T &localized_data<T>::get() const {
            auto locale = std::setlocale(LC_MESSAGES, "");
            if (!locale) {
                throw std::runtime_error("Failed to get locale");
            }
            return get(locale);
        }

        template<class T>
        const T &localized_data<T>::get(std::string_view locale_str) const {
            if (!locale_str.empty()) {
                for (const auto &locale : alternative_locales_iterator(locale_str)) {
                    if (auto it = m_translations.find(locale); it != m_translations.end()) {
                        return it->second;
                    }
                }
            }
            return m_generic;
        }

        template class localized_data<std::string>;
        template class localized_data<std::vector<std::string>>;
    } // namespace detail

    desktop_entry::desktop_entry() = default;

    desktop_entry::desktop_entry(std::istream &is) : desktop_entry() {
        detail::desktop_entry_parser parser {*this};
        parser.parse(is);
        if (is.fail()) {
            throw std::runtime_error("Parsing failed");
        }
        if (!check_required_keys()) {
            throw std::runtime_error("desktop_entry has missing keys");
        }
    }

    desktop_entry::desktop_entry(std::istream &&is) : desktop_entry(is) {
        if (!check_required_keys()) {
            throw std::runtime_error("desktop_entry has missing keys");
        }
    }

    desktop_entry::desktop_entry(path store, path relative_path) :
            desktop_entry(std::ifstream(store / relative_path)) {
        m_store         = std::move(store);
        m_relative_path = std::move(relative_path);
    }

    bool desktop_entry::check_required_keys() const noexcept {
        return get_well_known_value(well_known_keys::Type).has_value()
               && get_well_known_value(well_known_keys::Name).has_value()
               && (get_type() != entry_type::Link || get_well_known_value(well_known_keys::Type).has_value());
    }

    std::string desktop_entry::make_command_line([[maybe_unused]] std::vector<std::string> launch_args,
        [[maybe_unused]] bool are_uri) const {
        // TODO: Support uris and files
        static const boost::regex ignored_keys {"(?<!%)((?:%%)*)%[fFuU]"};
        static const boost::regex percent_re {"%%"};
        static const boost::regex uneven_percents_re {"(?<!%)(?:%%)*%(?!%)"};
        // TODO: Some are unsupported
        static const boost::regex unsupported_re {"(?<!%)((?:%%)*)%[ik]"};
        static const boost::regex name_re {"(?<!%)((?:%%)*)%c"};
        std::string cmdline {get_exec()};
        cmdline = boost::regex_replace(std::move(cmdline), ignored_keys, "$1");
        cmdline = boost::regex_replace(std::move(cmdline), unsupported_re, "$1");
        cmdline = boost::regex_replace(std::move(cmdline), name_re, get_name().get());
        if (boost::regex_search(cmdline, uneven_percents_re)) {
            throw std::runtime_error("Invalid desktop file Exec entry");
        }
        cmdline = boost::regex_replace(std::move(cmdline), percent_re, "%");
        return cmdline;
    }

    std::string desktop_entry::get_id() const {
        if (m_relative_path.empty()) {
            return {};
        }
        std::string res = m_relative_path.string();
        std::ranges::replace(res, '/', '-');
        return res;
    }

    bool desktop_entry::should_show() const {
        if (get_no_display() || get_hidden()) {
            return false;
        }

        std::string_view current_desktop = std::getenv("XDG_CURRENT_DESKTOP");
        auto only_show_in                = get_only_show_in();
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

    std::vector<std::unique_ptr<desktop_entry>> get_all_desktop_entries() {
        std::unordered_set<std::string> ids_read;
        std::vector<std::unique_ptr<desktop_entry>> result;

        for (auto &application_dir : xdg_data_dir_iterator()) {
            application_dir /= "applications";
            try {
                for (const auto &file : recursive_directory_iterator(application_dir,
                         directory_options::follow_directory_symlink | directory_options::skip_permission_denied)) {
                    if (file.path().extension() != ".desktop") {
                        continue;
                    }

                    std::unique_ptr<desktop_entry> entry;
                    try {
                        entry = std::make_unique<desktop_entry>(application_dir,
                            file.path().lexically_relative(application_dir));
                    } catch (const std::runtime_error &ex) {
                        // TODO: Specific exception
                        std::cerr << "Failed to parse desktop file: " << file << '\n';
                        continue;
                    }
                    auto emplace_res = ids_read.emplace(entry->get_id());
                    if (emplace_res.second) {
                        // Entry is new
                        result.emplace_back(std::move(entry));
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

    application_action::application_action(std::string id) : m_id(std::move(id)) { }
} // namespace xdg::desktop_entry_spec