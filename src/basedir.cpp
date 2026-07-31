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

    std::vector<std::filesystem::path> split_multivar_variable(std::string str) {
        auto view = xdg::detail::utils::string_spliterator {str, PathSeparator}
                    | std::views::transform([](std::string_view str) {
                          return check_for_absolute_path(str, XdgDataDirsVariable);
                      });
        return {std::move_iterator(view.begin()), std::move_iterator(view.end())};
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
        return split_multivar_variable(get_data_dirs_raw());
    }

    std::vector<std::filesystem::path> get_config_dirs() {
        return split_multivar_variable(get_config_dirs_raw());
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

    data_dir_iterator::data_dir_iterator(const data_dir_iterator &)                   = default;
    data_dir_iterator &data_dir_iterator::operator=(const data_dir_iterator &rhs)     = default;
    data_dir_iterator::data_dir_iterator(data_dir_iterator &&other) noexcept          = default;
    data_dir_iterator &data_dir_iterator::operator=(data_dir_iterator &&rhs) noexcept = default;

    const std::filesystem::path &data_dir_iterator::operator*() const {
        if (m_paths.empty()) {
            throw std::logic_error("Tried to dereference end iterator");
        }
        return m_paths.back();
    }

    void data_dir_iterator::operator++(int) {
        if (m_paths.empty()) {
            throw std::logic_error("Tried to increment end iterator");
        }
        m_paths.pop_back();
    }

    data_dir_iterator &data_dir_iterator::operator++() {
        (*this)++;
        return *this;
    }

    bool data_dir_iterator::operator==(std::default_sentinel_t) const noexcept {
        return m_paths.empty();
    }
} // namespace xdg::basedir