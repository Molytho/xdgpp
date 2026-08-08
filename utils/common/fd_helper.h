#ifndef MOLYTHO_FD_HELPER_HPP
#define MOLYTHO_FD_HELPER_HPP

#include <stdexcept>
#include <unistd.h>

namespace molytho::ownership_wrapper {
    class owned_fd {
        static constexpr int INVALID_FD = -1;

        int m_fd {INVALID_FD};

    public:
        owned_fd() { }

        owned_fd(int fd) : m_fd(fd) {
            if (fd < 0) {
                throw std::runtime_error("Tried to create owned_fd with invalid fd");
            }
        }

        owned_fd(owned_fd &&other) noexcept : m_fd(other.m_fd) { other.m_fd = INVALID_FD; }

        owned_fd &operator=(owned_fd &&other) noexcept {
            swap(*this, other);
            return *this;
        }

        friend void swap(owned_fd &lhs, owned_fd &rhs) noexcept {
            using namespace std;
            swap(lhs.m_fd, rhs.m_fd);
        }

        ~owned_fd() { reset(); }

        void reset(int fd = INVALID_FD) noexcept {
            if (m_fd != INVALID_FD) {
                close(m_fd);
            }
            m_fd = fd;
        }

        explicit operator bool() const noexcept { return m_fd >= 0; }

        operator int() const {
            if (!(*this)) {
                throw std::logic_error("Tried to use invalid file descriptor");
            }
            return m_fd;
        }
    };
} // namespace molytho::ownership_wrapper

#endif
