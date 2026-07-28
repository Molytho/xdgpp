#ifndef DESKTOP_ENTRY_TYPES_H
#define DESKTOP_ENTRY_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "helper.h"

/* ---------- class locale ---------- */
namespace xdg::desktop_entry_spec {
    struct parsed_locale {
        std::string_view lang;
        std::string_view country;
        std::string_view encoding;
        std::string_view modifier;
    };

    class API_PUBLIC locale {
        std::string m_str;

    public:
        locale() : m_str() { }

        explicit locale(std::string_view str) : m_str(str) { }

        explicit locale(std::string str) : m_str(std::move(str)) { }

        locale(const locale &other)                = default;
        locale &operator=(const locale &other)     = default;
        locale(locale &&other) noexcept            = default;
        locale &operator=(locale &&other) noexcept = default;

        std::string_view str() const noexcept { return m_str; }

        parsed_locale parse() const noexcept;

        void strip_encoding();

        bool operator==(const locale &other) const noexcept = default;
    };
} // namespace xdg::desktop_entry_spec

namespace std {
    template<>
    struct hash<xdg::desktop_entry_spec::locale> {
        hash<std::string_view> m_str_hash {};

        size_t operator()(const xdg::desktop_entry_spec::locale &locale) const {
            return m_str_hash(locale.str());
        }
    };
} // namespace std

/* ---------- Types ---------- */
namespace xdg::desktop_entry_spec::types {
    namespace detail {
        template<class T>
        class API_PUBLIC localized_data {
            T m_generic {};
            std::unordered_map<locale, T> m_translations {};

        public:
            localized_data() = default;

            void add(std::string_view lang, T val);

            void set_generic(T val);

            void add_translated(std::string_view lang, T val);

            const T &get() const;

            const T &get(std::string_view locale_str) const;
        };
    } // namespace detail

    enum class entry_type : uint8_t {
        Application,
        Link,
        Directory,
        First = Application,
        Last  = Directory
    };
    API_PUBLIC std::string_view to_string(entry_type val) noexcept;
    API_PUBLIC std::optional<entry_type> entry_type_from_string(std::string_view str) noexcept;
    API_PUBLIC std::ostream &operator<<(std::ostream &os, entry_type val);
    API_PUBLIC std::istream &operator>>(std::istream &is, entry_type &out);

    // TODO: This should only allow ASCII chars
    using string        = std::string;
    using strings       = std::vector<string>;
    using localestring  = detail::localized_data<std::string>;
    using localestrings = detail::localized_data<std::vector<std::string>>;
    using iconstring    = localestring;
    using boolean       = bool;
    using numeric       = float;
} // namespace xdg::desktop_entry_spec::types

#endif