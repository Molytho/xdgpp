#include "desktop-entry.h"
#include "mime-apps.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

using namespace xdg::desktop_entry_spec;
using namespace xdg::mime_apps;
using namespace std::string_literals;

#define CHECKED_CALL(val, cond)                                     \
    if (auto __checked_call_res = (val); __checked_call_res cond) { \
        throw std::system_error(errno, std::system_category());     \
    }

class owned_fd {
    int m_fd;

public:
    owned_fd(int fd = -1) : m_fd(fd) { }

    owned_fd(owned_fd &&other) noexcept : m_fd(other.m_fd) { other.m_fd = -1; }

    owned_fd &operator=(owned_fd &&other) noexcept {
        reset(other.m_fd);
        other.m_fd = -1;
        return *this;
    }

    ~owned_fd() { reset(); }

    void reset(int fd = -1) noexcept {
        if (m_fd != -1) {
            close(m_fd);
        }
        m_fd = fd;
    }

    explicit operator bool() const noexcept { return m_fd >= 0; }

    operator int() const {
        if (!(*this)) {
            throw std::logic_error("Tried to use invalid file descriptor");
        }
        return m_fd;
    }
};

std::pair<owned_fd, owned_fd> make_pipe() {
    std::array<int, 2> fds {-1, -1};
    CHECKED_CALL(pipe(fds.data()), != 0)
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
    CHECKED_CALL(res, < 0)

    return buffer;
}

mime_type get_mimetype(const std::filesystem::path &file) {
    auto [read_fd, write_fd] = make_pipe();
    pid_t pid                = fork();
    if (pid < 0) {
        throw std::system_error(errno, std::system_category());
    } else if (pid == 0) {
        // Child
        read_fd.reset();
        CHECKED_CALL(dup2(write_fd, 1), == -1)
        CHECKED_CALL(dup2(write_fd, 2), == -1)
        execlp("xdg-mime", "xdg-mime", "query", "filetype", file.native().c_str(), nullptr);
        throw std::system_error(errno, std::system_category());
    } else {
        // Parent
        write_fd.reset();
        int status {};
        CHECKED_CALL(waitpid(pid, &status, 0), < 0);
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

void launch_impl(launch_parameters &params) {
    if (!params.working_directory.empty()) {
        std::string str {params.working_directory};
        CHECKED_CALL(chdir(str.c_str()), != 0)
    }
    execlp("/bin/sh", "/bin/sh", "-c", params.command_list.c_str(), nullptr);
    throw std::system_error(errno, std::system_category());
}

int run_desktop_file(std::filesystem::path file) {
    std::fstream fstr {file};
    auto entry = desktop_entry::from_istream(fstr);
    if (entry.get_type() != xdg::desktop_entry_spec::types::entry_type::Application) {
        std::cout << "Desktop file not an application file\n";
        return 5;
    }

    auto app_entry = application_entry(std::move(entry));
    app_entry.launch(launch_impl);

    return 0;
}

int main(int argc, char *argv[]) try {
    constexpr std::string_view application_file_mimetype = "application/x-desktop";
    if (argc < 2) {
        show_usage();
    }

    std::filesystem::path file = argv[1];
    if (!exists(file)) {
        std::cerr << "File not valid\n";
        return 4;
    }

    auto mime_type = get_mimetype(file);
    if (mime_type == application_file_mimetype) {
        return run_desktop_file(std::move(file));
    } else {
        auto default_app = xdg::mime_apps::get_default_app_for_mime_type(mime_type);
        if (default_app) {
            default_app->launch(launch_impl, file, false);
            return 0;
        } else {
            std::cerr << "No application associated with mime_type: " << mime_type << '\n';
            return 3;
        }
    }
} catch (const std::runtime_error &ex) {
    std::cerr << "Execution failed:\n" << ex.what() << '\n';
    return EXIT_FAILURE;
}