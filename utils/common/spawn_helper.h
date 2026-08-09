#ifndef LAUNCHER_SPAWN_HELPER_H

# include <string>
# include <vector>
#include "desktop-entry.h"

std::string escape_systemd_string(std::string_view str, bool start_of_string = true);

struct spawn_context {
    std::string executable;void spawn_as_service(xdg::desktop_entry_spec::application_entry entry);
    std::vector<std::string> arguments;
    std::vector<std::string> environ;
    std::string working_directory;
    std::string unit_name;
    std::string slice;
};

[[nodiscard]] std::string make_id(const xdg::desktop_entry_spec::application_entry &entry);

std::string make_unique_identifier();

void spawn_as_service(spawn_context &context);

template<class... Args>
void spawn_as_service(xdg::desktop_entry_spec::application_entry entry, Args &&...args) {
    entry.launch([&](xdg::desktop_entry_spec::launch_parameters &params) {
        auto app_id = make_id(entry);
        spawn_context context {
            .executable        = "sh",
            .arguments         = {"-c", std::move(params.command_list)},
            .environ           = {},
            .working_directory = std::string(params.working_directory),
            .unit_name         = "app-" + app_id + "-" + make_unique_identifier(),
            .slice             = "app-" + app_id + ".slice",
        };
        spawn_as_service(context);
    }, std::forward<Args>(args)...);
}

void spawn_as_scope(spawn_context &context);

#endif