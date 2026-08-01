#include "desktop-entry/types.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <iostream>
#include <string_view>

#include <boost/regex.hpp>

#include "private/string_helper.h"

using namespace std::string_view_literals;
using namespace xdg::desktop_entry_spec;

namespace {
    constexpr std::array entry_type_name {"Application"sv, "Link"sv, "Directory"sv};

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

    std::string calculate_id_from_path(const std::filesystem::path &path) {
        std::string result = path.string();
        std::ranges::replace(result, '/', '-');
        return result;
    }

    std::filesystem::path calculate_path_from_id(std::string str) {
        std::ranges::replace(str, '-', '/');
        return str;
    }

    [[noreturn]] void throw_parsing_error(std::string_view type, std::string_view value) {
        static constexpr std::string_view format {"Could not parse \"{}\" as \"{}\""};
        throw types::parsing_error(std::format(format, value, type));
    }

    template<class T>
    T throw_parsing_error_if_nullopt(std::optional<T> &opt, std::string_view type, std::string_view value) {
        if (!opt) {
            throw_parsing_error(type, value);
        }
        return *std::move(opt);
    }

    auto split_semicolon_delimited_list(std::string_view str) noexcept {
        static const boost::regex unescaped_semicolon_re {"(?<!\\\\)(?:\\\\\\\\)*(;)"};
        return xdg::detail::utils::re_string_spliterator(str, unescaped_semicolon_re);
    }

    bool is_ascii_control(char c) {
        return c < ' ' || c > '~';
    }
} // namespace

namespace xdg::desktop_entry_spec {
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

    namespace types {
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

        application_id::application_id() = default;

        application_id::application_id(std::string_view str) : application_id(std::string(str)) { }

        application_id::application_id(std::string id) : m_id(std::move(id)) { }

        application_id::application_id(const std::filesystem::path &relative_path) :
                application_id(calculate_id_from_path(relative_path)) { }

        application_id::application_id(const application_id &)                = default;
        application_id &application_id::operator=(const application_id &)     = default;
        application_id::application_id(application_id &&) noexcept            = default;
        application_id &application_id::operator=(application_id &&) noexcept = default;

        std::filesystem::path application_id::to_path() const {
            return calculate_path_from_id(m_id);
        }

        application_id::operator std::string_view() const noexcept {
            return m_id;
        }

        bool application_id::operator==(const application_id &) const noexcept = default;

        namespace detail {
            entry_type parse(parse_tag<entry_type>, std::string_view str) {
                auto parsed = types::entry_type_from_string(str);
                return throw_parsing_error_if_nullopt(parsed, "entry_type", str);
            }

            string parse(parse_tag<string>, std::string_view str, bool allow_utf8, bool handle_semicolon) {
                if (!allow_utf8 && std::ranges::any_of(str, is_ascii_control)) {
                    throw_parsing_error("string", str);
                }

                std::string result {str};

                size_t prev_pos = 0;
                size_t pos      = result.find('\\', prev_pos);
                if (pos == std::string::npos) {
                    return result;
                }

                auto do_escape = [&](char c) {
                    result.at(pos)     = c;
                    result.at(pos + 1) = '\0';
                };
                while (pos != std::string::npos && pos + 1 < result.size()) {
                    switch (result.at(pos + 1)) {
                    case 's':
                        do_escape(' ');
                        break;
                    case 'n':
                        do_escape('\n');
                        break;
                    case 't':
                        do_escape('\t');
                        break;
                    case 'r':
                        do_escape('\r');
                        break;
                    case '\\':
                        do_escape('\\');
                        break;
                    case ';':
                        if (handle_semicolon) {
                            do_escape(';');
                        }
                        break;
                    default:
                        break;
                    }

                    prev_pos = pos + 1;
                    pos      = str.find('\\', prev_pos);
                }
                std::erase(result, '\0');
                return result;
            }

            string parse(parse_tag<string> t, std::string_view str, bool allow_utf8) {
                return parse(t, str, allow_utf8, false);
            }

            strings parse(parse_tag<strings>, std::string_view str, bool allow_utf8) {
                auto view = split_semicolon_delimited_list(str) | std::views::transform([allow_utf8](std::string_view str) {
                    return parse(parse_tag<string> {}, str, allow_utf8, true);
                });
                return {std::move_iterator(view.begin()), std::move_iterator(view.end())};
            }

            boolean parse(parse_tag<boolean>, std::string_view str) {
                if (str == "true") {
                    return true;
                } else if (str == "false") {
                    return false;
                } else {
                    throw_parsing_error("boolean", str);
                }
            }

            std::vector<application_id> parse(parse_tag<std::vector<application_id>>, std::string_view str) {
                auto view = split_semicolon_delimited_list(str) | std::views::transform([](std::string_view str) {
                    return application_id(parse(parse_tag<string> {}, str, true, true));
                });
                return {std::move_iterator(view.begin()), std::move_iterator(view.end())};
            }
        } // namespace detail
    } // namespace types
} // namespace xdg::desktop_entry_spec