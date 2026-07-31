#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <ranges>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace xdg::detail::utils {
    class string_spliterator : public std::ranges::view_interface<string_spliterator> {
        char m_delimiter;
        bool m_ended;
        std::string_view m_str;
        size_t m_end_pos;

    public:
        using value_type      = std::string_view;
        using difference_type = std::ptrdiff_t;

        constexpr string_spliterator() : m_delimiter(), m_ended(true), m_str(), m_end_pos() { }

        constexpr string_spliterator(std::string_view str, char delimiter) :
                m_delimiter(delimiter), m_ended(false), m_str(str), m_end_pos(m_str.find(delimiter)) { }

        constexpr string_spliterator(const string_spliterator &)            = default;
        constexpr string_spliterator &operator=(const string_spliterator &) = default;

        constexpr std::string_view operator*() const noexcept { return m_str.substr(0, m_end_pos); }

        constexpr string_spliterator &operator++() {
            if (m_ended) {
                throw std::logic_error("Tried to increment iterator while it is at the end");
            }

            if (m_end_pos == std::string_view::npos) {
                m_ended = true;
            } else {
                m_str     = m_str.substr(m_end_pos + 1);
                m_end_pos = m_str.find(m_delimiter);
            }
            return *this;
        }

        constexpr string_spliterator operator++(int) {
            string_spliterator cur = *this;
            ++*this;
            return cur;
        }

        const string_spliterator &begin() const noexcept { return *this; }

        string_spliterator &begin() noexcept { return *this; }

        static string_spliterator end() noexcept { return {}; }

        constexpr bool operator==(const string_spliterator &other) const noexcept {
            if (m_ended && other.m_ended) {
                return true;
            }

            return m_delimiter == other.m_delimiter
                   && m_ended == other.m_ended
                   && m_str == other.m_str
                   && m_end_pos == other.m_end_pos;
        }
    };

    static_assert(std::forward_iterator<string_spliterator>);
    static_assert(std::ranges::view<string_spliterator>);

    inline std::vector<std::string> split_string(std::string_view view, char delimiter) {
        auto spliterator = string_spliterator(view, delimiter);
        return {spliterator.begin(), spliterator.end()};
    }
} // namespace xdg::detail::utils

#endif