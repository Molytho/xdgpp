#ifndef LAUNCHER_SPAWN_HELPER_H

# include "desktop-entry.h"
# include <string>

# include "molytho-cpp-utils/systemd_utils.h"

[[nodiscard]] std::string make_id(const xdg::desktop_entry_spec::application_entry &entry);

template<class... Args>
void spawn_as_service(xdg::desktop_entry_spec::application_entry entry, Args &&...args) {
    entry.launch(
        [&](xdg::desktop_entry_spec::launch_parameters &params) {
            auto app_id = make_id(entry);
            molytho::utils::systemd::spawn_context context {
                .executable        = "sh",
                .arguments         = {"-c", std::move(params.command_list)},
                .environ           = {},
                .working_directory = std::string(params.working_directory),
                .unit_name = "app-" + app_id + "-" + molytho::utils::make_unique_identifier(),
                .slice     = "app-" + app_id + ".slice",
            };
            spawn_as_service(context);
        },
        std::forward<Args>(args)...
    );
}

#endif