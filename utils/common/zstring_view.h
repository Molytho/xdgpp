#ifndef ZSTRING_VIEW_H
#define ZSTRING_VIEW_H

#include <cassert>
#include <stdexcept>
#include <string>

namespace string_utils {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_zstring_view {
        using basic_string_view = std::basic_string_view<CharT, Traits>;
        basic_string_view m_view;

        constexpr basic_zstring_view(basic_string_view view) : m_view(view) {}

    public:
        using traits_type            = Traits;
        using value_type             = CharT;
        using pointer                = CharT *;
        using const_pointer          = const CharT *;
        using reference              = CharT &;
        using const_reference        = const CharT &;
        using const_iterator         = basic_string_view::const_iterator;
        using iterator               = const_iterator;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator       = const_reverse_iterator;
        using size_type              = std::size_t;
        using difference_type        = std::ptrdiff_t;

        constexpr basic_zstring_view() : m_view("") {}

        constexpr basic_zstring_view(const basic_zstring_view &other) noexcept = default;

        constexpr basic_zstring_view(const CharT *s, size_type count) : m_view(s, count) {
            if (!Traits::eq(s[count], 0)) {
                throw std::invalid_argument("s is not a null-terminated string");
            }
        }

        constexpr basic_zstring_view(const CharT *s) : m_view(s) {}

        template<class Allocator>
        constexpr basic_zstring_view(const std::basic_string<CharT, Traits, Allocator> &str) :
                basic_zstring_view(str.empty() ? "" : str.c_str(), str.size()) {}

        constexpr basic_zstring_view(std::nullptr_t) = delete;

        constexpr basic_zstring_view &operator=(const basic_zstring_view &) noexcept = default;

        constexpr const_iterator begin() const noexcept { return m_view.begin(); }

        constexpr const_iterator cbegin() const noexcept { return m_view.cbegin(); }

        constexpr const_iterator end() const noexcept { return m_view.end(); }

        constexpr const_iterator cend() const noexcept { return m_view.cend(); }

        constexpr const_iterator rbegin() const noexcept { return m_view.rbegin(); }

        constexpr const_iterator crbegin() const noexcept { return m_view.crbegin(); }

        constexpr const_iterator rend() const noexcept { return m_view.rend(); }

        constexpr const_iterator crend() const noexcept { return m_view.crend(); }

        constexpr const_reference operator[](size_type pos) const { return m_view[pos]; }

        constexpr const_reference at(size_type pos) const { return m_view.at(pos); }

        constexpr const_reference front() const { return m_view.front(); }

        constexpr const_reference back() const { return m_view.back(); }

        constexpr const_pointer data() const noexcept { return m_view.data(); }

        constexpr const_pointer c_str() const noexcept { return data(); }

        constexpr operator basic_string_view() const noexcept { return m_view; }

        constexpr size_type size() const noexcept { return m_view.size(); }

        constexpr size_type length() const noexcept { return m_view.length(); }

        constexpr size_type max_size() const noexcept { return m_view.max_size(); }

        constexpr bool empty() const noexcept { return m_view.empty(); }

        constexpr void remove_prefix(size_type n) { return m_view.remove_prefix(n); }

        // remove_suffix not supported

        constexpr void swap(basic_zstring_view &other) noexcept {
            using namespace std;
            swap(m_view, other.m_view);
        }

        size_type copy(CharT *dest, size_type count, size_type pos = 0) const {
            return m_view.copy(dest, count, pos);
        }

        constexpr basic_zstring_view substr(size_type pos = 0) const {
            return basic_string_view(m_view.substr(pos));
        }

        constexpr basic_string_view substr(size_type pos, size_type count) const {
            return m_view.substr(pos, count);
        }

        constexpr int compare(basic_string_view v) const noexcept { return m_view.compare(v); }

        constexpr int compare(size_type pos1, size_type count1, basic_string_view v) const {
            return m_view.compare(pos1, count1, v);
        }

        constexpr int compare(size_type pos1, size_type count1, basic_string_view v, size_type pos2,
            size_type count2) const {
            return m_view.compare(pos1, count1, v, pos2, count2);
        }

        constexpr int compare(const CharT *s) const { return m_view.compare(s); }

        constexpr int compare(size_type pos1, size_type count1, const CharT *s) const {
            return m_view.compare(pos1, count1, s);
        }

        constexpr int compare(size_type pos1, size_type count1, const CharT *s, size_type count2) const {
            return m_view.compare(pos1, count1, s, count2);
        }

        constexpr bool starts_with(basic_string_view sv) const noexcept {
            return m_view.starts_with(sv);
        }

        constexpr bool starts_with(CharT ch) const noexcept { return m_view.starts_with(ch); }

        constexpr bool starts_with(const CharT *s) const { return m_view.starts_with(s); }

        constexpr bool ends_with(basic_string_view sv) const noexcept {
            return m_view.ends_with(sv);
        }

        constexpr bool ends_with(CharT ch) const noexcept { return m_view.ends_with(ch); }

        constexpr bool ends_with(const CharT *s) const { return m_view.ends_with(s); }

        constexpr size_type find(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.find(v, pos);
        }

        constexpr size_type find(CharT ch, size_type pos = 0) const noexcept {
            return m_view.find(ch, pos);
        }

        constexpr size_type find(const CharT *s, size_type pos, size_type count) const {
            return m_view.find(s, pos, count);
        }

        constexpr size_type find(const CharT *s, size_type pos = 0) const {
            return m_view.find(s, pos);
        }

        constexpr size_type rfind(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.rfind(v, pos);
        }

        constexpr size_type rfind(CharT ch, size_type pos = 0) const noexcept {
            return m_view.rfind(ch, pos);
        }

        constexpr size_type rfind(const CharT *s, size_type pos, size_type count) const {
            return m_view.rfind(s, pos, count);
        }

        constexpr size_type rfind(const CharT *s, size_type pos = 0) const {
            return m_view.rfind(s, pos);
        }

        constexpr size_type find_first_of(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.find_first_of(v, pos);
        }

        constexpr size_type find_first_of(CharT ch, size_type pos = 0) const noexcept {
            return m_view.find_first_of(ch, pos);
        }

        constexpr size_type find_first_of(const CharT *s, size_type pos, size_type count) const {
            return m_view.find_first_of(s, pos, count);
        }

        constexpr size_type find_first_of(const CharT *s, size_type pos = 0) const {
            return m_view.find_first_of(s, pos);
        }

        constexpr size_type find_last_of(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.find_last_of(v, pos);
        }

        constexpr size_type find_last_of(CharT ch, size_type pos = 0) const noexcept {
            return m_view.find_last_of(ch, pos);
        }

        constexpr size_type find_last_of(const CharT *s, size_type pos, size_type count) const {
            return m_view.find_last_of(s, pos, count);
        }

        constexpr size_type find_last_of(const CharT *s, size_type pos = 0) const {
            return m_view.find_last_of(s, pos);
        }

        constexpr size_type find_first_not_of(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.find_first_not_of(v, pos);
        }

        constexpr size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept {
            return m_view.find_first_not_of(ch, pos);
        }

        constexpr size_type find_first_not_of(const CharT *s, size_type pos, size_type count) const {
            return m_view.find_first_not_of(s, pos, count);
        }

        constexpr size_type find_first_not_of(const CharT *s, size_type pos = 0) const {
            return m_view.find_first_not_of(s, pos);
        }

        constexpr size_type find_last_not_of(basic_string_view v, size_type pos = 0) const noexcept {
            return m_view.find_last_not_of(v, pos);
        }

        constexpr size_type find_last_not_of(CharT ch, size_type pos = 0) const noexcept {
            return m_view.find_last_not_of(ch, pos);
        }

        constexpr size_type find_last_not_of(const CharT *s, size_type pos, size_type count) const {
            return m_view.find_last_not_of(s, pos, count);
        }

        constexpr size_type find_last_not_of(const CharT *s, size_type pos = 0) const {
            return m_view.find_last_not_of(s, pos);
        }

        static constexpr size_type npos = basic_string_view::npos;
    };

    template<class CharT, class Traits>
    constexpr bool operator==(
        const basic_zstring_view<CharT, Traits> &lhs, const basic_zstring_view<CharT, Traits> &rhs) {
        return std::basic_string_view<CharT, Traits>(lhs) == std::basic_string_view<CharT, Traits>(rhs);
    }

    template<class CharT, class Traits>
    constexpr bool operator==(const basic_zstring_view<CharT, Traits> &lhs,
        const std::basic_string_view<CharT, Traits> &rhs) {
        return std::basic_string_view<CharT, Traits>(lhs) == rhs;
    }

    template<class CharT, class Traits>
    constexpr bool operator==(const std::basic_string_view<CharT, Traits> &lhs,
        const basic_zstring_view<CharT, Traits> &rhs) {
        return lhs == std::basic_string_view<CharT, Traits>(rhs);
    }

    using zstring_view = basic_zstring_view<char>;

    namespace literals {
        constexpr zstring_view operator""_zsv(const char *str, std::size_t len) noexcept {
            return zstring_view(str, len);
        }
    } // namespace literals
} // namespace string_utils

template<class CharT, class Traits>
inline constexpr bool std::ranges::enable_borrowed_range<string_utils::basic_zstring_view<CharT, Traits>> = true;

template<class CharT, class Traits>
inline constexpr bool std::ranges::enable_view<string_utils::basic_zstring_view<CharT, Traits>> = true;

#endif