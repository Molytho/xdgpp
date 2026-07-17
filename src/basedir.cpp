#include "basedir.h"

#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

#include "private/string_helper.h"

using namespace xdg::basedir;

namespace {
    constexpr char XdgDataHomeVariable[]   = "XDG_DATA_HOME";
    constexpr char XdgConfigHomeVariable[] = "XDG_CONFIG_HOME";
    constexpr char XdgStateHomeVariable[]  = "XDG_STATE_HOME";
    constexpr char XdgCacheHomeVariable[]  = "XDG_CACHE_HOME";
    constexpr char XdgDataDirsVariable[]   = "XDG_DATA_DIRS";
    constexpr char XdgConfigDirsVariable[] = "XDG_CONFIG_DIRS";
    constexpr char XdgRuntimeDirVariable[] = "XDG_RUNTIME_DIR";
    constexpr char HomeVariable[]          = "HOME";
    constexpr char PathSeparator           = ':';

    std::filesystem::path check_for_absolute_path(std::filesystem::path path, const char *var_name) {
        if (!path.is_absolute()) {
            throw mandatory_environment_variable_relative(var_name);
        }
        return path;
    }

    std::filesystem::path get_home_dir() {
        auto env = std::getenv(HomeVariable);
        return env ? check_for_absolute_path(env, HomeVariable)
                   : throw mandatory_environment_variable_missing(HomeVariable);
    }

    std::filesystem::path get_home_var(const char *name, std::string_view default_value) {
        auto env = std::getenv(name);
        if (env) {
            return check_for_absolute_path(env, name);
        } else {
            return get_home_dir() / default_value;
        }
    }
} // namespace

namespace xdg::basedir {
    std::filesystem::path get_data_home() {
        return get_home_var(XdgDataHomeVariable, ".local/share");
    }

    std::filesystem::path get_config_home() {
        return get_home_var(XdgConfigHomeVariable, ".config");
    }

    std::filesystem::path get_state_home() {
        return get_home_var(XdgStateHomeVariable, ".local/state");
    }

    std::filesystem::path get_cache_home() {
        return get_home_var(XdgCacheHomeVariable, ".cache");
    }

    std::filesystem::path get_bin_home() {
        return get_home_dir() / ".local/bin";
    }

    std::string get_data_dirs_raw() {
        auto env = std::getenv(XdgDataDirsVariable);
        return env ? env : "/usr/local/share/:/usr/share/";
    }

    std::string get_config_dirs_raw() {
        auto env = std::getenv(XdgConfigDirsVariable);
        return env ? env : "/etc/xdg";
    }

    std::vector<std::filesystem::path> get_data_dirs() {
        auto strs = get_data_dirs_raw();
        std::vector<std::filesystem::path> result;
        for (const auto &str : xdg::detail::utils::string_spliterator {strs, PathSeparator}) {
            result.emplace_back(check_for_absolute_path(str, XdgDataDirsVariable));
        }
        return result;
    }

    std::vector<std::filesystem::path> get_config_dirs() {
        auto strs = get_config_dirs_raw();
        std::vector<std::filesystem::path> result;
        for (const auto &str : xdg::detail::utils::string_spliterator {strs, PathSeparator}) {
            result.emplace_back(check_for_absolute_path(str, XdgConfigDirsVariable));
        }
        return result;
    }

    std::filesystem::path get_runtime_dir() {
        auto env = std::getenv(XdgRuntimeDirVariable);
        return env ? check_for_absolute_path(env, XdgRuntimeDirVariable)
                   : throw mandatory_environment_variable_missing(XdgRuntimeDirVariable);
    }


    data_dir_iterator::data_dir_iterator() : m_paths() {
        auto system_dirs = xdg::basedir::get_data_dirs();
        m_paths.reserve(system_dirs.size() + 1);
        m_paths.insert(m_paths.cend(),
            std::move_iterator(system_dirs.rbegin()),
            std::move_iterator(system_dirs.rend()));
        m_paths.emplace_back(get_data_home());
    }

    data_dir_iterator::data_dir_iterator(const data_dir_iterator &) = default;

    data_dir_iterator::data_dir_iterator(data_dir_iterator &&other) noexcept :
            m_paths(std::move(other.m_paths)) { }

    data_dir_iterator &data_dir_iterator::operator=(const data_dir_iterator &rhs) {
        m_paths = rhs.m_paths;
        return *this;
    }

    data_dir_iterator &data_dir_iterator::operator=(data_dir_iterator &&rhs) noexcept {
        m_paths = std::move(rhs.m_paths);
        return *this;
    }

    std::filesystem::path &data_dir_iterator::operator*() {
        assert(!m_paths.empty());
        return m_paths.back();
    }

    std::filesystem::path *data_dir_iterator::operator->() {
        assert(!m_paths.empty());
        return std::addressof(m_paths.back());
    }

    data_dir_iterator &data_dir_iterator::operator++() {
        assert(!m_paths.empty());
        m_paths.pop_back();
        return *this;
    }

    bool data_dir_iterator::operator==(std::default_sentinel_t) const noexcept {
        return m_paths.empty();
    }

    data_dir_iterator begin(data_dir_iterator it) {
        return it;
    }

    std::default_sentinel_t end(const data_dir_iterator &) {
        return {};
    }

} // namespace xdg::basedir