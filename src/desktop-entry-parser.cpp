#include "private/desktop-entry-private.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <optional>
#include <string_view>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "desktop-entry/well-known-keys.h"
#include "private/desktop-entry-private.h"
#include "private/string_helper.h"

using namespace std::string_view_literals;

using namespace std::filesystem;
using namespace xdg::desktop_entry_spec;

namespace {
    constexpr std::string_view MainSectionName      = "Desktop Entry";
    constexpr std::string_view ActionSectionPrelude = "Desktop Action ";

    const boost::regex IsCommentRe {"^[ \\t]*(?:#|$)", boost::regex::optimize};
    const boost::regex ParseGroupHeadRe {"^[ \\t]*\\[([ -Z\\\\^-~]+)\\][ \\t]*$", boost::regex::optimize};
    const boost::regex ParseKeyValueRe {
        "^[ \\t]*(?<key>[A-Za-z0-9-]+)(?:\\[(?<locale>[ -Z\\\\^-~]+)\\])?[ \\t]*=[ "
        "\\t]*(?<value>.*)$",
        boost::regex_constants::flag_type_::optimize
    };

    struct key_value_match {
        std::string_view key;
        std::string_view value;
        std::string_view locale;

        void assert_locale_empty() const {
            if (!locale.empty()) {
                throw std::runtime_error("value cannot be localized");
            }
        }

        std::vector<std::string> parse_as_vector() const {
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

        void parse_into(types::entry_type &out) const {
            assert_locale_empty();
            auto parsed = types::entry_type_from_string(value);
            if (!parsed) {
                throw std::runtime_error("Invalid type");
            }
            out = *parsed;
        }

        void parse_into(types::boolean &out) const {
            assert_locale_empty();
            // TODO: Rework this
            if (value == "true") {
                out = true;
            } else if (value == "false") {
                out = false;
            } else {
                throw std::runtime_error("Invalid bool value");
            }
        }

        void parse_into(types::string &out) const {
            assert_locale_empty();
            out = std::string(value);
        }

        void parse_into(types::strings &out) const {
            assert_locale_empty();
            out = parse_as_vector();
        }

        void parse_into(types::localestring &out) const { out.add(locale, std::string(value)); }

        void parse_into(types::localestrings &out) const {
            auto parsed = parse_as_vector();
            out.add(locale, std::move(parsed));
        }

        std::optional<well_known_keys> parse_as_well_known_key() const noexcept {
            return well_known_keys_from_string(key);
        }
    };

    struct input_line {
        std::string m_str;

        constexpr input_line() = default;

        input_line(input_line &&other) noexcept : m_str(std::move(other.m_str)) { }

        input_line &operator=(input_line &&rhs) noexcept {
            swap(m_str, rhs.m_str);
            return *this;
        }

        input_line(std::string str) noexcept : m_str(std::move(str)) { }

        input_line &operator=(std::string str) noexcept {
            m_str = std::move(str);
            return *this;
        }

        explicit operator bool() const noexcept { return !m_str.empty(); }

        [[nodiscard]] bool is_comment_line() const noexcept {
            return m_str.empty() || boost::regex_search(m_str.cbegin(), m_str.cend(), IsCommentRe);
        }

        [[nodiscard]] bool is_type_line() const noexcept { return m_str.starts_with("Type="); }

        [[nodiscard]] std::string_view parse_as_group_header() const noexcept {
            boost::match_results<std::string::const_iterator> match;
            if (boost::regex_match(m_str.begin(), m_str.end(), match, ParseGroupHeadRe)) {
                return std::string_view(match[1].begin(), match[1].end());
            }
            return {};
        }

        [[nodiscard]] key_value_match parse_as_key_value() const {
            boost::match_results<std::string::const_iterator> match;
            if (!boost::regex_match(m_str.begin(), m_str.end(), match, ParseKeyValueRe)) {
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
    };

    template<class... Args>
    consteval bool storage_contains_key(well_known_keys key) {
        std::array keys = {Args::key...};
        auto it         = std::ranges::find(keys, key);
        return it != keys.end();
    }

#define DEFINE_PARSE_CASE(key)                                                           \
    case well_known_keys::key:                                                           \
        if constexpr (storage_contains_key<Args...>(well_known_keys::key)) {             \
            val.parse_into(storage.template get<well_known_keys::key>().init_and_get()); \
            return true;                                                                 \
        }                                                                                \
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

    struct parser_interface {
        virtual ~parser_interface() = default;

        virtual void handle_section_change(std::string_view name) = 0;
        virtual void handle_key_value(key_value_match &key_value) = 0;

        virtual desktop_entry get_result() && noexcept = 0;
    };

    class parser_common : public parser_interface {
    protected:
        std::shared_ptr<detail::desktop_entry_data> m_entry;
        bool m_is_main_section {true};

        parser_common(std::shared_ptr<detail::desktop_entry_data> entry) :
                m_entry(std::move(entry)) { }

        void handle_section_change(std::string_view name) override {
            m_is_main_section = name == MainSectionName;
            // TODO
        }

        void handle_main_section_well_know_key(well_known_keys key, key_value_match &key_value) {
            if (!parse_well_know_key_into(key, key_value, m_entry->m_common_storage)) {
                throw std::runtime_error("Invalid well known key");
            }
        }

        void handle_main_section_other_key([[maybe_unused]] key_value_match &key_value) {
            // TODO
        }

        void handle_main_section_key_value(key_value_match &key_value) {
            if (auto opt_well_known_key = key_value.parse_as_well_known_key(); opt_well_known_key) {
                handle_main_section_well_know_key(*opt_well_known_key, key_value);
            } else {
                handle_main_section_other_key(key_value);
            }
        }

        void handle_other_section_key_value([[maybe_unused]] key_value_match &key_value) {
            // TODO
        }

        void handle_key_value(key_value_match &key_value) override {
            if (m_is_main_section) {
                handle_main_section_key_value(key_value);
            } else {
                handle_other_section_key_value(key_value);
            }
        }

        desktop_entry get_result() && noexcept final { return std::move(m_entry); }
    };

    class parser_application_entry : public parser_common {
        bool m_skip_section {false};
        std::shared_ptr<detail::application_action_data> m_current_action {};
        std::bitset<size_t(well_known_keys::Count)> m_present_well_known_keys;

        types::strings m_registered_actions {};

        detail::application_entry_data *get_ptr() noexcept {
            return static_cast<detail::application_entry_data *>(m_entry.get());
        }

        void assert_required_keys() {
            bool valid = true;
            if (m_is_main_section || m_current_action) {
                valid = m_present_well_known_keys.test(size_t(well_known_keys::Name))
                        && (m_present_well_known_keys.test(size_t(well_known_keys::Exec))
                            || get_ptr()->get_dbus_activatable());
            }

            if (!valid) {
                throw std::runtime_error("Required key missing");
            }
        }

        bool has_action(std::string_view name) {
            return std::ranges::find(m_registered_actions, name) != m_registered_actions.end();
        }

        void handle_section_change(std::string_view name) final {
            assert_required_keys();

            m_skip_section   = false;
            m_current_action = {};
            m_present_well_known_keys.reset();

            parser_common::handle_section_change(name);

            if (name.starts_with(ActionSectionPrelude)) {
                auto action_name = name.substr(ActionSectionPrelude.size());
                if (!has_action(action_name)) {
                    std::cerr << "Warning: desktop entry has invalid action section\n";
                    m_skip_section = true;
                    return;
                }

                m_current_action = std::make_shared<detail::application_action_data>(std::string(action_name));
                get_ptr()->add_actions(m_current_action);
            }
        }

        void handle_action_section_well_known_key(well_known_keys key, key_value_match &key_value) {
            m_present_well_known_keys.set(size_t(key));
            if (!parse_well_know_key_into(key, key_value, m_current_action->m_storage)) {
                throw std::runtime_error(
                    "Encountered unexpected well known key in application action section"
                );
            }
        }

        void handle_action_section_other_key([[maybe_unused]] key_value_match &key_value) {
            // TODO
        }

        void handle_action_section_key_value(key_value_match &key_value) {
            if (auto opt_well_known_key = key_value.parse_as_well_known_key(); opt_well_known_key) {
                handle_action_section_well_known_key(*opt_well_known_key, key_value);
            } else {
                handle_action_section_other_key(key_value);
            }
        }

        void handle_main_section_well_known_key(well_known_keys key, key_value_match &key_value) {
            m_present_well_known_keys.set(size_t(key));
            if (key == xdg::desktop_entry_spec::well_known_keys::Actions) {
                key_value.parse_into(m_registered_actions);
            } else if (!parse_well_know_key_into(key, key_value, get_ptr()->m_application_storage)) {
                parser_common::handle_main_section_well_know_key(key, key_value);
            }
        }

        void handle_main_section_key_value(key_value_match &key_value) {
            if (auto opt_well_known_key = key_value.parse_as_well_known_key(); opt_well_known_key) {
                handle_main_section_well_known_key(*opt_well_known_key, key_value);
            } else {
                parser_common::handle_main_section_other_key(key_value);
            }
        }

        void handle_key_value(key_value_match &key_value) override {
            if (m_skip_section) {
                return;
            } else if (m_current_action) {
                handle_action_section_key_value(key_value);
            } else if (m_is_main_section) {
                handle_main_section_key_value(key_value);
            } else {
                parser_common::handle_other_section_key_value(key_value);
            }
        }


    public:
        parser_application_entry() :
                parser_common(std::make_shared<detail::application_entry_data>()) { }
    };

    struct parser {
        std::istream &is;
        std::unique_ptr<parser_interface> impl {};
        std::vector<input_line> line_buffer {};

        input_line get_non_blank_line() {
            for (input_line line; std::getline(is, line.m_str);) {
                if (!line.is_comment_line()) {
                    return line;
                }
            }
            return {};
        }

        input_line assert_getline() {
            if (is.eof()) {
                throw std::runtime_error("Unexpected EOF");
            }
            auto line = get_non_blank_line();
            if (!line) {
                throw std::runtime_error("Unexpected EOF");
            }
            return line;
        }

        void assert_main_section() {
            auto line = assert_getline();
            if (line.parse_as_group_header() != MainSectionName) {
                throw std::runtime_error("Expected main section");
            }
        }

        types::entry_type discover_type() {
            input_line line = assert_getline();
            while (!line.is_type_line()) {
                if (!line.parse_as_group_header().empty()) {
                    throw std::runtime_error("desktop entry has no type");
                }

                line_buffer.emplace_back(std::move(line));

                line = assert_getline();
            }

            types::entry_type type;
            auto match = line.parse_as_key_value();
            assert(match.key == to_string(well_known_keys::Type));
            match.parse_into(type);
            return type;
        }

        void init_impl(types::entry_type type) {
            switch (type) {
            case types::entry_type::Application:
                impl = std::make_unique<parser_application_entry>();
                break;
            case types::entry_type::Link:
            case types::entry_type::Directory:
                throw std::runtime_error("Unsupported");
            }
        }

        void consume_line(input_line &line) {
            if (std::string_view section_name = line.parse_as_group_header(); !section_name.empty()) {
                impl->handle_section_change(section_name);
            } else {
                auto key_value = line.parse_as_key_value();
                impl->handle_key_value(key_value);
            }
        }

        desktop_entry parse() {
            assert_main_section();

            auto type = discover_type();
            init_impl(type);

            for (input_line &line : line_buffer) {
                consume_line(line);
            }
            line_buffer.clear();

            input_line line = get_non_blank_line();
            while (line) {
                consume_line(line);
                line = get_non_blank_line();
            }
            if (is.eof()) {
                is.clear(std::ios::eofbit);
            }

            return std::move(*impl).get_result();
        }
    };
} // namespace

namespace xdg::desktop_entry_spec::detail {
    desktop_entry parse_desktop_entry(std::istream &is) try {
        parser parser {is};
        return parser.parse();
    } catch (const std::runtime_error &ex) {
        is.setstate(std::ios::failbit);
        return {};
    }
} // namespace xdg::desktop_entry_spec::detail