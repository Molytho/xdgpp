#include "desktop-entry.h"
#include "mime-apps.h"
#include "shared-mime-info.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

#include <boost/regex.hpp>

#include "spawn_helper.h"

using namespace std::string_literals;
using namespace xdg::mime_apps;
using namespace xdg::shared_mime_info;
using namespace xdg::desktop_entry_spec;

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

    auto mime_type = determine_mime_type(path);
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