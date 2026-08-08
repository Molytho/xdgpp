#include "desktop-entry.h"
#include "mime-apps.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

#include <boost/regex.hpp>

#include "checked_c_call.h"
#include "fd_helper.h"
#include "spawn_helper.h"

using namespace xdg::desktop_entry_spec;
using namespace xdg::mime_apps;
using namespace std::string_literals;
using namespace molytho::checked;

using owned_fd = molytho::ownership_wrapper::owned_fd;

std::pair<owned_fd, owned_fd> make_pipe() {
    std::array<int, 2> fds {-1, -1};
    check_zero(pipe(fds.data()));
    return {fds.at(0), fds.at(1)};
}

std::vector<char> read_fd(int fd) {
    constexpr size_t initial_buffer_size = 50;
    constexpr size_t resize_threshold    = 10;
    std::vector<char> buffer {};
    buffer.resize(initial_buffer_size);

    size_t current_index = 0;
    ssize_t res;
    while ((res = read(fd, &buffer.at(current_index), buffer.size() - current_index)) > 0) {
        current_index += res;
        if (buffer.size() - current_index < resize_threshold) {
            buffer.resize(buffer.size() * 2);
        }
    }
    check_ge_zero(res);

    return buffer;
}

mime_type get_mimetype(const std::filesystem::path &file) {
    auto [read_fd, write_fd] = make_pipe();
    pid_t pid                = check_ge_zero(fork());
    if (pid == 0) {
        // Child
        read_fd.reset();
        check_ge_zero(dup2(write_fd, 1));
        check_ge_zero(dup2(write_fd, 2));
        execlp("xdg-mime", "xdg-mime", "query", "filetype", file.native().c_str(), nullptr);
        throw std::system_error(errno, std::system_category());
    } else {
        // Parent
        write_fd.reset();
        int status {};
        check_ge_zero(waitpid(pid, &status, 0));
        bool child_success           = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        std::vector<char> chars_read = ::read_fd(read_fd);

        if (child_success) {
            auto it = std::ranges::find(chars_read, '\n');
            if (it != chars_read.end()) {
                std::string_view view {chars_read.begin(), it};
                return view;
            }
        }

        throw std::runtime_error(
            "xdg-mime failed: "s.append(std::string_view {chars_read.begin(), chars_read.end()})
        );
    }
}

[[noreturn]] void show_usage() {
    // TODO
    std::exit(1);
}

void launch_impl(std::string app_id, launch_parameters &params) {
    spawn_context context {
        .executable        = "sh",
        .arguments         = {"-c", std::move(params.command_list)},
        .environ           = {},
        .working_directory = std::string(params.working_directory),
        .unit_name         = "app-" + app_id + "-" + make_unique_identifier(),
        .slice             = "app-" + app_id + ".slice",
    };
    spawn_as_service(context);
}

[[nodiscard]] std::string make_id(const application_id &app_id) {
    auto id = std::string(app_id);
    assert(id.ends_with(".desktop"));
    id.resize(id.size() - 8);
    id = escape_systemd_string(id, false);
    return id;
}

auto make_launch_impl(const application_id &app_id) {
    return [app_id = make_id(app_id)](launch_parameters &params) {
        launch_impl(std::string(app_id), params);
    };
}

int open_file_path(std::filesystem::path path) {
    if (!exists(path)) {
        std::cerr << "Path not valid\n";
        return 4;
    }

    auto mime_type   = get_mimetype(path);
    auto default_app = xdg::mime_apps::get_default_app_for_mime_type(mime_type);
    if (default_app) {
        default_app->launch(make_launch_impl(default_app->get_id()), path, false);
        return 0;
    } else {
        std::cerr << "No application associated with mime_type: " << mime_type << '\n';
        return 3;
    }
}

int open_uri(std::string_view scheme, std::string uri) {
    mime_type mime_type = "x-scheme-handler/"s.append(scheme);
    auto default_app    = xdg::mime_apps::get_default_app_for_mime_type(mime_type);
    if (default_app) {
        default_app->launch(make_launch_impl(default_app->get_id()), uri, true);
        return 0;
    } else {
        std::cerr << "No application associated with mime_type: " << mime_type << '\n';
        return 3;
    }
}

std::string_view get_uri_schema(std::string_view str) {
    static const boost::regex uri_scheme_re {"^([[:alpha:]][[:alpha:][:digit:]+.-]*):"};
    boost::match_results<std::string_view::const_iterator> match;
    if (boost::regex_search(str.begin(), str.end(), match, uri_scheme_re)) {
        return {match[0].begin(), match[1].end()};
    }
    return {};
}

int main(int argc, char *argv[]) try {
    if (argc < 2) {
        show_usage();
    }

    std::string_view to_open = argv[1];
    auto uri_scheme          = get_uri_schema(to_open);
    if (uri_scheme == "file") {
        constexpr std::string_view file_scheme_start {"file://"};
        constexpr std::string_view localhost_authority {"localhost/"};
        if (!to_open.starts_with(file_scheme_start)) {
            std::cerr << "Invalid file uri\n";
            return 10;
        }
        auto path_component = to_open.substr(file_scheme_start.size());
        if (path_component.starts_with('/')) {
            to_open    = path_component;
            uri_scheme = {};
        } else if (path_component.starts_with(localhost_authority)) {
            // Keep the '/' char
            to_open    = path_component.substr(localhost_authority.size() - 1);
            uri_scheme = {};
        }
    }

    if (uri_scheme.empty()) {
        return open_file_path(to_open);
    } else {
        return open_uri(uri_scheme, std::string(to_open));
    }
} catch (const std::runtime_error &ex) {
    std::cerr << "Execution failed:\n" << ex.what() << '\n';
    return EXIT_FAILURE;
}