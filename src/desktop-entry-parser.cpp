#include "private/desktop-entry-parser.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string_view>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "private/string_helper.h"

using namespace std::string_view_literals;
using namespace xdg::desktop_entry_spec;
using namespace std::filesystem;

namespace {
    constexpr std::string_view MainSectionName      = "Desktop Entry";
    constexpr std::string_view ActionSectionPrelude = "Desktop Action ";

    boost::regex IsCommentRe {"^[ \\t]*(?:#.*)?$", boost::regex_constants::flag_type_::optimize};
    boost::regex IsGroupHeadRe {"^[ \\t]*\\[([ -Z\\\\^-~]+)\\][ \\t]*$", boost::regex_constants::flag_type_::optimize};
    boost::regex IsKeyParsingRe {
        "^[ \\t]*(?<key>[A-Za-z0-9-]+)(?:\\[(?<locale>[ -Z\\\\^-~]+)\\])?[ \\t]*=[ "
        "\\t]*(?<value>.*)$",
        boost::regex_constants::flag_type_::optimize
    };

    bool is_comment_line(std::string_view line) {
        return boost::regex_match(line.begin(), line.end(), IsCommentRe);
    }

    std::string_view parse_group_head(std::string_view line) {
        boost::match_results<std::string_view::const_iterator> match;
        if (boost::regex_match(line.begin(), line.end(), match, IsGroupHeadRe)) {
            return std::string_view(match[1].begin(), match[1].end());
        }
        return {};
    }

    struct key_value_match {
        std::string_view key;
        std::string_view value;
        std::string_view locale;
    };

    key_value_match parse_key_value(std::string_view line) {
        boost::match_results<std::string_view::const_iterator> match;
        if (!boost::regex_match(line.begin(), line.end(), match, IsKeyParsingRe)) {
            throw std::runtime_error("Line did not match");
        }
        auto &key_match    = match["key"];
        auto &value_match  = match["value"];
        auto &locale_match = match["locale"];
        return {
            .key    = std::string_view(key_match.begin(), key_match.end()),
            .value  = std::string_view(value_match.begin(), value_match.end()),
            .locale = std::string_view(locale_match.begin(), locale_match.end())
        };
    }

    template<class>
    struct parse_as_helper;

    template<>
    struct parse_as_helper<entry_type> {
        static constexpr bool is_type(well_known_keys val) { return val == well_known_keys::Type; }

        static void parse(const key_value_match &val, entry_type &out) {
            auto parsed = entry_type_from_string(val.value);
            if (!val.locale.empty()) {
                throw std::runtime_error("Type value cannot be localized");
            }
            if (!parsed) {
                throw std::runtime_error("Invalid type");
            }
            out = *parsed;
        }
    };

    template<>
    struct parse_as_helper<std::string> {
        static constexpr std::array string_types {
            well_known_keys::Exec,
            well_known_keys::Path,
            well_known_keys::StartupWMClass,
            well_known_keys::TryExec,
            well_known_keys::URL,
            well_known_keys::Version
        };
        static_assert(std::ranges::is_sorted(string_types));

        static constexpr bool is_type(well_known_keys val) {
            return std::ranges::binary_search(string_types, val);
        }

        static void parse(const key_value_match &val, std::string &out) {
            if (!val.locale.empty()) {
                throw std::runtime_error("String values cannot be localized");
            }
            out = std::string(val.value);
        }
    };

    template<>
    struct parse_as_helper<bool> {
        static constexpr std::array bool_types {
            well_known_keys::DBusActivatable,
            well_known_keys::Hidden,
            well_known_keys::NoDisplay,
            well_known_keys::PrefersNonDefaultGPU,
            well_known_keys::SingleMainWindow,
            well_known_keys::StartupNotify,
            well_known_keys::Terminal
        };
        static_assert(std::ranges::is_sorted(bool_types));

        static constexpr bool is_type(well_known_keys val) {
            return std::ranges::binary_search(bool_types, val);
        }

        static void parse(const key_value_match &val, bool &out) {
            if (!val.locale.empty()) {
                throw std::runtime_error("bool values cant be localized");
            }

            // TODO: Rework this
            if (val.value == "true") {
                out = true;
            } else if (val.value == "false") {
                out = false;
            } else {
                throw std::runtime_error("Invalid bool value");
            }
        }
    };

    template<>
    struct parse_as_helper<std::vector<std::string>> {
        static constexpr std::array string_list_types {
            well_known_keys::Actions,
            well_known_keys::Categories,
            well_known_keys::Implements,
            well_known_keys::MimeType,
            well_known_keys::NotShowIn,
            well_known_keys::OnlyShowIn
        };
        static_assert(std::ranges::is_sorted(string_list_types));

        static constexpr bool is_type(well_known_keys val) {
            return std::ranges::binary_search(string_list_types, val);
        }

        static std::vector<std::string> parse_as_vector(std::string_view value) {
            if (value.empty()) {
                return {};
            }

            std::vector<std::string> values;
            for (const auto &str : xdg::detail::utils::string_spliterator(value, ';')) {
                if (!str.empty()) {
                    values.emplace_back(str);
                }
            }
            return values;
        }

        static void parse(const key_value_match &val, std::vector<std::string> &out) {
            if (!val.locale.empty()) {
                throw std::runtime_error("string list values cant be localized");
            }

            out = parse_as_vector(val.value);
        }
    };

    template<>
    struct parse_as_helper<localized_string> {
        static constexpr std::array localized_string_type {
            well_known_keys::Comment,
            well_known_keys::GenericName,
            well_known_keys::Icon,
            well_known_keys::Name
        };
        static_assert(std::ranges::is_sorted(localized_string_type));

        static constexpr bool is_type(well_known_keys val) {
            return std::ranges::binary_search(localized_string_type, val);
        }

        static void parse(const key_value_match &val, localized_string &out) {
            out.add(val.locale, std::string(val.value));
        }
    };

    template<>
    struct parse_as_helper<localized_string_list> {
        static constexpr bool is_type(well_known_keys val) {
            return val == well_known_keys::Keywords;
        }

        static void parse(const key_value_match &val, localized_string_list &out) {
            auto parsed = parse_as_helper<std::vector<std::string>>::parse_as_vector(val.value);
            out.add(val.locale, std::move(parsed));
        }
    };

    template<class T, class... Args>
    void parse_into_impl(well_known_keys key, const key_value_match &val, std::any &out) {
        if (parse_as_helper<T>::is_type(key)) {
            if (!out.has_value()) {
                out = T();
            }
            parse_as_helper<T>::parse(val, std::any_cast<T &>(out));
        } else {
            if constexpr (sizeof...(Args) > 0) {
                parse_into_impl<Args...>(key, val, out);
            } else {
                throw std::runtime_error("Could not parse key");
            }
        }
    }

    void parse_well_know_key_into(well_known_keys key, const key_value_match &val, std::any &out) {
        parse_into_impl<entry_type, std::string, bool, std::vector<std::string>, localized_string, localized_string_list>(key,
            val,
            out);
    }

    template<class T>
    void parse_well_know_key(well_known_keys key, const key_value_match &val, T &out) {
        if (!parse_as_helper<T>::is_type(key)) {
            throw std::logic_error("Tried to parse well know key with wrong type");
        }
        parse_as_helper<T>::parse(val, out);
    }

    bool key_is_valid(entry_type type) noexcept {
        return type >= xdg::desktop_entry_spec::entry_type::First
               && type <= xdg::desktop_entry_spec::entry_type::Last;
    }

    bool key_is_valid(const std::string &str) noexcept {
        return !str.empty();
    }

    bool key_is_valid(std::string_view view) noexcept {
        return !view.empty();
    }

    [[maybe_unused]] bool key_is_valid(const std::vector<std::string> &vec) noexcept {
        return !vec.empty();
    }

    bool key_is_valid(const localized_string &lstr) noexcept {
        return !lstr.get("").empty();
    }

    [[maybe_unused]] bool key_is_valid(const localized_string_list &lstrlist) noexcept {
        return !lstrlist.get("").empty();
    }

    template<class T>
        requires requires(const T &ref) {
            { key_is_valid(ref) } -> std::same_as<bool>;
        }
    bool key_is_valid(const T *ptr) {
        return ptr && key_is_valid(*ptr);
    }
} // namespace

namespace xdg::desktop_entry_spec::detail {
    void desktop_entry_parser::reset_flags() noexcept {
        m_is_main_section = false;
        m_skip_section    = false;
        m_current_action  = nullptr;
    }

    void desktop_entry_parser::update_current_section(std::string_view section) {
        check_for_required_keys();
        m_current_section = section;
        reset_flags();
        if (m_current_section == MainSectionName) {
            m_is_main_section = true;
        } else if (m_current_section.starts_with(ActionSectionPrelude)) {
            auto action_name = std::string_view(m_current_section).substr(ActionSectionPrelude.size());
            if (!entry_has_action(action_name)) {
                std::cerr << "Warning: desktop entry has invalid action section\n";
                m_skip_section = true;
                return;
            }
            m_target.m_actions.emplace_back(std::make_shared<application_action>(std::string(action_name)));
            m_current_action = m_target.m_actions.back().get();
        }
    }

    void desktop_entry_parser::parse_key_value_line(std::string line) {
        auto parse_result = parse_key_value(line);
        if (m_is_main_section) {
            if (auto well_known_key = well_known_keys_from_string(parse_result.key); well_known_key) {
                parse_well_know_key_into(*well_known_key, parse_result, m_target.get_well_known_value(*well_known_key));
            } else {
                // TODO
            }
        } else if (m_current_action) {
            if (auto well_known_key = well_known_keys_from_string(parse_result.key); well_known_key) {
                switch (*well_known_key) {
                case well_known_keys::Name:
                    parse_well_know_key(well_known_keys::Name, parse_result, m_current_action->m_name);
                    return;
                case well_known_keys::Icon:
                    if (!m_current_action->m_icon) {
                        m_current_action->m_icon = std::make_unique<localized_string>();
                    }
                    parse_well_know_key(well_known_keys::Icon, parse_result, *m_current_action->m_icon);
                    return;
                case well_known_keys::Exec:
                    parse_well_know_key<std::string>(well_known_keys::Exec,
                        parse_result,
                        m_current_action->m_exec);
                    return;
                default:
                    break;
                }
            }
            // TODO: else and fallthrough
        } else {
            // TODO
        }
    }

    bool desktop_entry_parser::entry_has_action(std::string_view str) {
        auto actions = m_target.get_actions_key();
        if (!actions) {
            return false;
        }
        return std::ranges::find(*m_target.get_actions_key(), str) != actions->end();
    }

    void desktop_entry_parser::check_for_required_keys() {
        bool valid = true;
        if (m_is_main_section) {
            valid = key_is_valid(m_target.get_type())
                    && key_is_valid(m_target.get_name())
                    && (m_target.get_type() != entry_type::Link || key_is_valid(m_target.get_url()));
        } else if (m_current_action) {
            valid = key_is_valid(m_current_action->m_name)
                    && (m_target.get_dbus_activatable() || key_is_valid(m_current_action->m_exec));
        }

        if (!valid) {
            throw std::runtime_error("Required key missing");
        }
    }

    desktop_entry_parser::desktop_entry_parser(desktop_entry &target) : m_target(target) { }

    void desktop_entry_parser::parse(std::istream &is) try {
        for (std::string line; std::getline(is, line);) {
            if (is_comment_line(line)) {
                continue;
            } else if (std::string_view section_parsed = parse_group_head(line); !section_parsed.empty()) {
                update_current_section(section_parsed);
            } else if (!m_skip_section) {
                parse_key_value_line(std::move(line));
            }
        }
        if (is.eof()) {
            is.clear(std::ios::eofbit);
        }
    } catch (const std::runtime_error &ex) {
        is.setstate(std::ios::failbit);
    }
} // namespace xdg::desktop_entry_spec::detail