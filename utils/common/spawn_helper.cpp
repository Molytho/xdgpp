#include "spawn_helper.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>

#include "checked_c_call.h"
#include "fd_helper.h"
#include "zstring_view.h"

using owned_fd = molytho::ownership_wrapper::owned_fd;
using namespace molytho::checked;

namespace {
    using namespace string_utils;

    constexpr zstring_view SliceFormatString = "--property=Slice={}";

    constexpr zstring_view RunExecutable = "systemd-run";
    constexpr zstring_view ArgUser       = "--user";
    constexpr zstring_view ArgUnit       = "-u";
    constexpr zstring_view ArgEnd        = "--";

    const char *to_cstr(const std::string &str) {
        return str.c_str();
    }

    struct pre_execve_data {
        std::string cwd;
    };

    struct make_parameters {
        virtual void make_args(std::vector<const char *> &, spawn_context &) const { };
        virtual void make_environ(std::vector<const char *> &, spawn_context &) const { };
        virtual void pre_execve_setup(spawn_context &) const { };
    };

    struct make_service_parameters : make_parameters {
        static constexpr zstring_view PropertyType     = "--property=Type=exec";
        static constexpr zstring_view PropertyExitType = "--property=ExitType=cgroup";
        static constexpr zstring_view PropertyRestart  = "--property=Restart=no";

        static constexpr zstring_view WorkingDirectoryFormatString
            = "--property=WorkingDirectory={}";

        static constexpr zstring_view EnvironmentPrefixString = "--property=Environment=";

        void make_args(std::vector<const char *> &args, spawn_context &context) const final {
            args.insert(args.cend(),
                {
                    PropertyType.c_str(),
                    PropertyExitType.c_str(),
                    PropertyRestart.c_str(),
                });

            std::ranges::for_each(context.environ, [](auto &str) {
                str.insert(str.begin(), EnvironmentPrefixString.begin(), EnvironmentPrefixString.end());
            });
            std::ranges::transform(context.environ, std::back_inserter(args), to_cstr);

            if (!context.working_directory.empty()) {
                context.working_directory
                    = std::format(WorkingDirectoryFormatString, std::move(context.working_directory));
                args.push_back(context.working_directory.c_str());
            }
        }
    };

    constexpr make_service_parameters make_service_parameters_instance {};

    struct make_scope_parameters : make_parameters {
        static constexpr zstring_view ArgScope = "--scope";

        void make_args(std::vector<const char *> &args, spawn_context &) const final {
            args.push_back(ArgScope.c_str());
        }

        void make_environ(std::vector<const char *> &environ, spawn_context &context) const final {
            environ.reserve(environ.size() + context.environ.size());
            std::ranges::transform(context.environ, std::back_inserter(environ), to_cstr);
        }

        void pre_execve_setup(spawn_context &data) const final {
            if (!data.working_directory.empty()) {
                check_zero(chdir(data.working_directory.c_str()));
            }
        }
    };

    constexpr make_scope_parameters make_scope_parameters_instance {};

    std::vector<const char *> make_args(const make_parameters &impl, spawn_context &context) {
        std::vector<const char *>
            res {RunExecutable.c_str(), ArgUser.c_str(), ArgUnit.c_str(), context.unit_name.c_str()};

        if (!context.slice.empty()) {
            context.slice = std::format(SliceFormatString, std::move(context.slice));
            res.push_back(context.slice.c_str());
        }

        impl.make_args(res, context);

        res.push_back(ArgEnd.c_str());

        // Application args
        res.push_back(context.executable.c_str());
        std::ranges::transform(context.arguments, std::back_inserter(res), to_cstr);
        res.push_back(nullptr);

        return res;
    }

    std::vector<const char *> make_environ(const make_parameters &impl, spawn_context &context) {
        char **begin = environ;
        char **end   = [&]() {
            char **it = environ;
            assert(it);
            while (*it != nullptr) {
                it++;
            }
            return it;
        }();

        std::vector<const char *> res {begin, end};
        impl.make_environ(res, context);
        res.push_back(nullptr);
        return res;
    }

    pid_t spawn(const make_parameters &impl, const char *const argv[], const char *const envp[],
        spawn_context &context) {
        assert(argv[0] != nullptr);
        pid_t pid = fork();
        if (pid < 0) {
            throw std::system_error(errno, std::system_category());
        } else if (pid == 0) {
            // Child
            impl.pre_execve_setup(context);
            execvpe(argv[0], const_cast<char *const *>(argv), const_cast<char *const *>(envp));
            perror("execve failed:");
            std::abort();
        } else {
            // Parent
            return pid;
        }
        return pid;
    }

    void spawn_as_impl(const make_parameters &impl, spawn_context &context) {
        if (context.executable.empty()) {
            throw std::logic_error("executable is empty");
        }
        if (context.unit_name.empty()) {
            throw std::logic_error("unit_name is empty");
        }

        auto args    = make_args(impl, context);
        auto environ = make_environ(impl, context);
        spawn(impl, args.data(), environ.data(), context);
    }
} // namespace

std::string escape_systemd_string(std::string_view str, bool start_of_string) {
    if (str.empty()) {
        return {};
    }

    std::string result;
    result.reserve(str.size());
    // Dot at first position needs to be escaped
    if (start_of_string && str.at(0) == '.') {
        result.append("\\x2e");
        str = str.substr(1);
    }
    for (char c : str) {
        if (std::isalnum(c) || c == ':' || c == '_' || c == '.') {
            result.push_back(c);
        } else if (c == '/') {
            result.push_back('-');
        } else {
            result.append("\\x");
            std::array<char, 3> buffer {0, 0, 0};
            std::to_chars(buffer.data(), buffer.data() + 2, (unsigned char)(c), 16);
            result.append(buffer.data());
        }
    }
    return result;
}

void spawn_as_service(spawn_context &context) {
    spawn_as_impl(make_service_parameters_instance, context);
}

void spawn_as_scope(spawn_context &context) {
    spawn_as_impl(make_scope_parameters_instance, context);
}

std::string make_unique_identifier() {
    pid_t pid   = getpid();
    owned_fd fd = check_ge_zero(syscall(SYS_pidfd_open, pid, 0));

    struct stat buffer {};
    if (fstat(fd, &buffer) < 0) {
        throw std::system_error(errno, std::system_category());
    }

    std::stringstream sstr {};
    sstr << 'p' << std::to_string(pid) << "-i" << std::to_string(buffer.st_ino);
    return std::move(sstr).str();
}