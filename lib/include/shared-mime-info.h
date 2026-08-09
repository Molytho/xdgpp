#ifndef LIB_XDGPP_SHARED_MIME_INFO_H
#define LIB_XDGPP_SHARED_MIME_INFO_H

#include <filesystem>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "helper.h"

namespace xdg::shared_mime_info {
    class API_PUBLIC mime_type {
        std::string m_data;

    public:
        mime_type();
        mime_type(std::string str);
        mime_type(std::string_view str);

        mime_type(const mime_type &);
        mime_type &operator=(const mime_type &);
        mime_type(mime_type &&);
        mime_type &operator=(mime_type &&);

        std::string_view str() const noexcept;

        bool is_subclass(const mime_type &subclass) const noexcept;
        std::vector<mime_type> get_parents() const;

        explicit operator bool() const noexcept;

        bool operator==(const mime_type &other) const noexcept;
        friend API_PUBLIC bool operator==(const mime_type &lhs, const std::string &rhs) noexcept;
        friend API_PUBLIC bool operator==(const std::string &lhs, const mime_type &rhs) noexcept;
        std::strong_ordering operator<=>(const mime_type &other) const noexcept;
    };

    std::ostream API_PUBLIC &operator<<(std::ostream &os, const mime_type &type);
} // namespace xdg::shared_mime_info

namespace std {
    template<>
    class hash<xdg::shared_mime_info::mime_type> {
        std::hash<std::string_view> m_hasher {};

    public:
        auto operator()(const xdg::shared_mime_info::mime_type &type) const noexcept {
            return m_hasher(type.str());
        }
    };
} // namespace std

namespace xdg::shared_mime_info {
    class API_PUBLIC mime_type_parent_iterator {
        std::unordered_set<mime_type> m_seen;
        std::vector<mime_type> m_next_layer;
        std::vector<mime_type> m_current_layer;

    public:
        using value_type      = const mime_type;
        using difference_type = std::ptrdiff_t;

        mime_type_parent_iterator(mime_type type);

        const mime_type &operator*() const;

        mime_type_parent_iterator &operator++();

        void operator++(int) { ++(*this); }

        mime_type_parent_iterator &begin() noexcept;

        std::default_sentinel_t end() const noexcept;

        bool operator==(std::default_sentinel_t) const noexcept;
    };

    static_assert(std::input_iterator<mime_type_parent_iterator>);

    API_PUBLIC mime_type determine_mime_type(const std::filesystem::path &path,
        bool follow_symlinks = true, bool filename_only = false);
    API_PUBLIC mime_type determine_mime_type(std::span<std::byte> data);
} // namespace xdg::shared_mime_info

#endif