#include "desktop-entry/types.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string_view>

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
    } // namespace types
} // namespace xdg::desktop_entry_spec