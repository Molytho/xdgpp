#ifndef MOLYTHO_CHECKED_C_CALL
#define MOLYTHO_CHECKED_C_CALL

#include <concepts>
#include <system_error>

namespace molytho::checked {
    template<class T, class U>
    concept result_checker = requires(const T &t, const U &u) {
        { t(u) } -> std::same_as<bool>;
    };

    struct zero_checker {
        bool operator()(auto val) const noexcept { return val == 0; }
    };

    struct gt_zero_checker {
        bool operator()(auto val) const noexcept { return val > 0; }
    };

    struct ge_zero_checker {
        bool operator()(auto val) const noexcept { return val >= 0; }
    };

    struct lt_zero_checker {
        bool operator()(auto val) const noexcept { return val < 0; }
    };

    struct le_zero_checker {
        bool operator()(auto val) const noexcept { return val <= 0; }
    };

    template<class T>
    T check(T &&result, result_checker<T> auto checker) {
        if (!checker(result)) {
            throw std::system_error(errno, std::system_category());
        }
        return result;
    }

    template<class T>
    T check_zero(T &&result) {
        return check(std::forward<T>(result), zero_checker {});
    }

    template<class T>
    T check_gt_zero(T &&result) {
        return check(std::forward<T>(result), gt_zero_checker {});
    }

    template<class T>
    T check_ge_zero(T &&result) {
        return check(std::forward<T>(result), ge_zero_checker {});
    }

    template<class T>
    T check_lt_zero(T &&result) {
        return check(std::forward<T>(result), lt_zero_checker {});
    }

    template<class T>
    T check_le_zero(T &&result) {
        return check(std::forward<T>(result), le_zero_checker {});
    }

} // namespace molytho::checked

#endif