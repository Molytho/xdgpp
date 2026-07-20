#include "private/desktop-entry-parser.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string_view>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "desktop-entry/well-know-keys.h"
#include "private/string_helper.h"

using namespace std::string_view_literals;

using namespace std::filesystem;
using namespace xdg::desktop_entry_spec;

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
    struct parse_as_helper<types::entry_type> {
        static void parse(const key_value_match &val, types::entry_type &out) {
            auto parsed = types::entry_type_from_string(val.value);
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
    struct parse_as_helper<types::boolean> {
        static void parse(const key_value_match &val, types::boolean &out) {
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
    struct parse_as_helper<types::string> {
        static void parse(const key_value_match &val, types::string &out) {
            if (!val.locale.empty()) {
                throw std::runtime_error("String values cannot be localized");
            }
            out = std::string(val.value);
        }
    };

    template<>
    struct parse_as_helper<types::strings> {
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
    struct parse_as_helper<types::localestring> {
        static void parse(const key_value_match &val, types::localestring &out) {
            out.add(val.locale, std::string(val.value));
        }
    };

    template<>
    struct parse_as_helper<types::localestrings> {
        static void parse(const key_value_match &val, types::localestrings &out) {
            auto parsed = parse_as_helper<std::vector<std::string>>::parse_as_vector(val.value);
            out.add(val.locale, std::move(parsed));
        }
    };

    template<class... Args>
    consteval bool storage_contains_key(well_known_keys key) {
        std::array keys = {Args::key...};
        auto it         = std::ranges::find(keys, key);
        return it != keys.end();
    }

#define DEFINE_PARSE_CASE(key)                                                       \
    case well_known_keys::key:                                                       \
        if constexpr (storage_contains_key<Args...>(well_known_keys::key)) {         \
            parse_as_helper<well_known_key_type_t<well_known_keys::key>>::parse(val, \
                storage.template get<well_known_keys::key>().init_and_get());        \
            return true;                                                             \
        }                                                                            \
        return false;

    template<class... Args>
    bool parse_well_know_key_into(well_known_keys key, const key_value_match &val,
        well_known_key_storage<Args...> &storage) {
        switch (key) {
            DEFINE_PARSE_CASE(Actions)
            DEFINE_PARSE_CASE(Categories)
            DEFINE_PARSE_CASE(Comment)
            DEFINE_PARSE_CASE(DBusActivatable)
            DEFINE_PARSE_CASE(Exec)
            DEFINE_PARSE_CASE(GenericName)
            DEFINE_PARSE_CASE(Hidden)
            DEFINE_PARSE_CASE(Icon)
            DEFINE_PARSE_CASE(Implements)
            DEFINE_PARSE_CASE(Keywords)
            DEFINE_PARSE_CASE(MimeType)
            DEFINE_PARSE_CASE(Name)
            DEFINE_PARSE_CASE(NoDisplay)
            DEFINE_PARSE_CASE(NotShowIn)
            DEFINE_PARSE_CASE(OnlyShowIn)
            DEFINE_PARSE_CASE(Path)
            DEFINE_PARSE_CASE(PrefersNonDefaultGPU)
            DEFINE_PARSE_CASE(SingleMainWindow)
            DEFINE_PARSE_CASE(StartupNotify)
            DEFINE_PARSE_CASE(StartupWMClass)
            DEFINE_PARSE_CASE(Terminal)
            DEFINE_PARSE_CASE(TryExec)
            DEFINE_PARSE_CASE(Type)
            DEFINE_PARSE_CASE(URL)
            DEFINE_PARSE_CASE(Version)
        default:
            std::abort();
        }
    }

#undef DEFINE_PARSE_CASE

    bool key_is_valid(types::entry_type type) noexcept {
        return type >= xdg::desktop_entry_spec::types::entry_type::First
               && type <= xdg::desktop_entry_spec::types::entry_type::Last;
    }

    bool key_is_valid(std::string_view view) noexcept {
        return !view.empty();
    }

    [[maybe_unused]] bool key_is_valid(const std::vector<std::string> &vec) noexcept {
        return !vec.empty();
    }

    bool key_is_valid(const types::localestring &lstr) noexcept {
        return !lstr.get("").empty();
    }

    [[maybe_unused]] bool key_is_valid(const types::localestrings &lstrlist) noexcept {
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
            m_target->m_actions.emplace_back(std::make_shared<application_action>(m_target,
                std::string(action_name)));
            m_current_action = m_target->m_actions.back().get();
        }
    }

    void desktop_entry_parser::parse_key_value_line(std::string line) {
        auto parse_result = parse_key_value(line);
        if (m_is_main_section) {
            if (auto well_known_key = well_known_keys_from_string(parse_result.key); well_known_key) {
                if (well_known_key == well_known_keys::Actions) {
                    parse_as_helper<types::strings>::parse(parse_result, m_registered_actions);
                } else {
                    [[maybe_unused]] bool handled
                        = parse_well_know_key_into(*well_known_key, parse_result, m_target->m_storage);
                    // TODO: handled != true
                }
            } else {
                // TODO
            }
        } else if (m_current_action) {
            if (auto well_known_key = well_known_keys_from_string(parse_result.key); well_known_key) {
                [[maybe_unused]] bool handled
                    = parse_well_know_key_into(*well_known_key, parse_result, m_current_action->m_storage);
                // TODO: handled != true
            }
            // TODO: else and fallthrough
        } else {
            // TODO
        }
    }

    bool desktop_entry_parser::entry_has_action(std::string_view str) {
        return std::ranges::find(m_registered_actions, str) != m_registered_actions.end();
    }

    void desktop_entry_parser::check_for_required_keys() {
        bool valid = true;
        if (m_is_main_section) {
            valid = key_is_valid(m_target->get_type())
                    && key_is_valid(m_target->get_name())
                    && (m_target->get_type() != types::entry_type::Link || key_is_valid(m_target->get_url()));
        } else if (m_current_action) {
            valid = key_is_valid(m_current_action->get_name())
                    && (m_target->get_dbus_activatable() || key_is_valid(m_current_action->get_exec()));
        }

        if (!valid) {
            throw std::runtime_error("Required key missing");
        }
    }

    desktop_entry_parser::desktop_entry_parser(std::shared_ptr<desktop_entry> target) :
            m_target(std::move(target)) { }

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