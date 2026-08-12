#include "desktop-entry.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

#include "spawn_helper.h"

using namespace xdg::desktop_entry_spec;
using namespace std::string_literals;

[[noreturn]] void show_usage() {
    // TODO
    std::exit(1);
}

int main(int argc, char *argv[]) try {
    if (argc < 2) {
        show_usage();
    }

    std::string_view file   = argv[1];
    application_entry entry = [&]() -> application_entry {
        try {
            auto entry = desktop_entry::from_path(file);
            return entry.get_type() == xdg::desktop_entry_spec::types::entry_type::Application
                       ? application_entry(std::move(entry))
                       : throw std::runtime_error("path is not an application entry");
        } catch (const std::filesystem::filesystem_error &ex) {
            if (ex.code() != std::errc::no_such_file_or_directory) {
                throw;
            }
        }
        auto result = search_application_entry(file);
        return result ? std::move(result->entry)
                      : throw std::runtime_error("Could not find desktop entry: "s.append(file));
    }();

    std::vector<std::string> remaining_args {};
    remaining_args.resize(argc - 2);
    std::transform(&argv[2], &argv[argc], remaining_args.begin(), [](char *arg) {
        return std::string(arg);
    });

    spawn_as_service(entry, std::move(remaining_args));
    return 0;
} catch (const std::runtime_error &ex) {
    std::cerr << "Execution failed:\n" << ex.what() << '\n';
    return EXIT_FAILURE;
}