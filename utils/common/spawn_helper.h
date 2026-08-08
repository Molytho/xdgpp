#ifndef LAUNCHER_SPAWN_HELPER_H

# include <string>
# include <vector>

std::string escape_systemd_string(std::string_view str, bool start_of_string = true);

struct spawn_context {
    std::string executable;
    std::vector<std::string> arguments;
    std::vector<std::string> environ;
    std::string working_directory;
    std::string unit_name;
    std::string slice;
};

void spawn_as_service(spawn_context &context);
void spawn_as_scope(spawn_context &context);

std::string make_unique_identifier();

#endif