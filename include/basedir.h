#ifndef LIB_XDGPP_BASEDIR_H
#define LIB_XDGPP_BASEDIR_H

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "helper.h"

namespace xdg::basedir {
    struct mandatory_environment_variable_missing : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct mandatory_environment_variable_relative : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    API_PUBLIC std::filesystem::path get_data_home();
    API_PUBLIC std::filesystem::path get_config_home();
    API_PUBLIC std::filesystem::path get_state_home();
    API_PUBLIC std::filesystem::path get_cache_home();
    API_PUBLIC std::filesystem::path get_bin_home();

    API_PUBLIC std::string get_data_dirs_raw();
    API_PUBLIC std::string get_config_dirs_raw();
    API_PUBLIC std::vector<std::filesystem::path> get_data_dirs();
    API_PUBLIC std::vector<std::filesystem::path> get_config_dirs();

    API_PUBLIC std::filesystem::path get_runtime_dir();

    // Includes data_home
    API_PUBLIC std::vector<std::filesystem::path> get_data_dirs_by_priority();
    // Includes config_home
    API_PUBLIC std::vector<std::filesystem::path> get_config_dirs_by_priority();
} // namespace xdg::basedir

#endif