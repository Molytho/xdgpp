#include "shared-mime-info.h"

#include <memory>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>

// TODO: Figure out a good way to override this prefix in the library build
#define XDG_PREFIX xdg_test
#include "xdgmime.h"

using namespace std::string_literals;

namespace xdg::shared_mime_info {
    mime_type::mime_type() = default;

    mime_type::mime_type(std::string str) : m_data(std::move(str)) {
        if (!xdg_mime_is_valid_mime_type(m_data.c_str())) {
            throw std::runtime_error("mime_type is invalid");
        }
    }

    mime_type::mime_type(std::string_view str) : mime_type(std::string(str)) { }

    mime_type::mime_type(const mime_type &)            = default;
    mime_type &mime_type::operator=(const mime_type &) = default;
    mime_type::mime_type(mime_type &&)                 = default;
    mime_type &mime_type::operator=(mime_type &&)      = default;

    bool mime_type::is_subclass(const mime_type &subclass) const noexcept {
        return xdg_mime_mime_type_subclass(subclass.m_data.c_str(), m_data.c_str());
    }

    std::string_view mime_type::str() const noexcept {
        return m_data;
    }

    std::vector<mime_type> mime_type::get_parents() const {
        constexpr auto free_delete = [](auto *ptr) {
            free(ptr);
        };
        std::unique_ptr<const char *[], decltype(free_delete)> parents {
            const_cast<const char **>(xdg_mime_list_mime_parents(m_data.c_str()))
        };
        if (!parents) {
            return {};
        }

        std::vector<mime_type> result {};
        for (const char **it = parents.get(); *it; it++) {
            result.emplace_back(std::string(*it));
        }
        return result;
    }

    mime_type::operator bool() const noexcept {
        return !m_data.empty();
    }

    bool mime_type::operator==(const mime_type &other) const noexcept {
        return xdg_mime_mime_type_equal(m_data.c_str(), other.m_data.c_str());
    }

    bool operator==(const mime_type &lhs, const std::string &rhs) noexcept {
        return xdg_mime_mime_type_equal(lhs.m_data.c_str(), rhs.c_str());
    }

    bool operator==(const std::string &lhs, const mime_type &rhs) noexcept {
        return xdg_mime_mime_type_equal(lhs.c_str(), rhs.m_data.c_str());
    }

    std::strong_ordering mime_type::operator<=>(const mime_type &other) const noexcept = default;

    std::ostream &operator<<(std::ostream &os, const mime_type &type) {
        return os << type.str();
    }

    mime_type_parent_iterator::mime_type_parent_iterator(mime_type type) :
            m_current_layer({type ? std::move(type) : throw std::runtime_error("Invalid mime_type")}) { }

    const mime_type &mime_type_parent_iterator::operator*() const {
        if (*this == end()) {
            throw std::logic_error("Tried to dereference end iterator");
        }
        return m_current_layer.back();
    }

    mime_type_parent_iterator &mime_type_parent_iterator::operator++() {
        auto current = std::move(m_current_layer.back());
        m_current_layer.pop_back();

        auto parents = current.get_parents();
        m_next_layer.insert(m_next_layer.cend(),
            std::move_iterator(parents.begin()),
            std::move_iterator(parents.end()));

        m_seen.emplace(std::move(current));

        while (!m_current_layer.empty() && m_seen.contains(m_current_layer.back())) {
            m_current_layer.pop_back();
        }

        if (m_current_layer.empty()) {
            swap(m_current_layer, m_next_layer);

            while (!m_current_layer.empty() && m_seen.contains(m_current_layer.back())) {
                m_current_layer.pop_back();
            }
        }

        return *this;
    }

    mime_type_parent_iterator &mime_type_parent_iterator::begin() noexcept {
        return *this;
    }

    std::default_sentinel_t mime_type_parent_iterator::end() const noexcept {
        return {};
    }

    bool mime_type_parent_iterator::operator==(std::default_sentinel_t) const noexcept {
        return m_current_layer.empty();
    }

    mime_type determine_mime_type(const std::filesystem::path &path, bool follow_symlinks, bool filename_only) {
        const char *result;
        if (filename_only) {
            auto basename = path.filename();
            result        = xdg_mime_get_mime_type_from_file_name(basename.c_str());
        } else {
            // xdgmime does not support the inode types. Work around this by doing a stat ourselves
            struct stat statbuf {};
            int res = fstatat(AT_FDCWD, path.c_str(), &statbuf, follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW);
            if (res != 0) {
                throw std::system_error(errno, std::system_category());
            }
            if (S_ISBLK(statbuf.st_mode)) {
                return "inode/blockdevice"s;
            } else if (S_ISCHR(statbuf.st_mode)) {
                return "inode/chardevice"s;
            } else if (S_ISDIR(statbuf.st_mode)) {
                return "inode/directory"s;
            } else if (S_ISFIFO(statbuf.st_mode)) {
                return "inode/fifo"s;
            } else if (S_ISSOCK(statbuf.st_mode)) {
                return "inode/socket"s;
            } else if (S_ISLNK(statbuf.st_mode)) {
                return "inode/symlink"s;
            } else {
                result = xdg_mime_get_mime_type_for_file(path.c_str(), &statbuf);
            }
        }
        if (!result) {
            throw std::runtime_error("Failed to determine_mime_type");
        }
        return std::string(result);
    }

    mime_type determine_mime_type(std::span<std::byte> data) {
        const char *result = xdg_mime_get_mime_type_for_data(data.data(), data.size(), nullptr);
        if (!result) {
            throw std::runtime_error("Failed to determine_mime_type");
        }
        return std::string(result);
    }
} // namespace xdg::shared_mime_info