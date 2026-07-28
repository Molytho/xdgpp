#ifndef LIB_XDGPP_BASEDIR_H
#define LIB_XDGPP_BASEDIR_H

#include <assert.h>
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

    class API_PUBLIC data_dir_iterator {
        std::vector<std::filesystem::path> m_paths;

    public:
        data_dir_iterator();

        data_dir_iterator(const data_dir_iterator &other);
        data_dir_iterator &operator=(const data_dir_iterator &rhs);

        data_dir_iterator(data_dir_iterator &&other) noexcept;
        data_dir_iterator &operator=(data_dir_iterator &&rhs) noexcept;

        std::filesystem::path &operator*();

        std::filesystem::path *operator->();

        data_dir_iterator &operator++();

        bool operator==(std::default_sentinel_t) const noexcept;
    };

    API_PUBLIC data_dir_iterator begin(data_dir_iterator it);
    API_PUBLIC std::default_sentinel_t end(const data_dir_iterator &);
} // namespace xdg::basedir

#endif