#include "desktop-entry.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_set>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "basedir.h"
#include "private/desktop-entry-parser.h"

using namespace std::string_view_literals;
using namespace xdg::desktop_entry_spec;
using namespace std::filesystem;

namespace xdg::desktop_entry_spec {
    namespace detail {
        std::string make_command_line(std::string_view exec, std::string_view name,
            [[maybe_unused]] std::vector<std::string> launch_args, [[maybe_unused]] bool are_uri) {
            // TODO: Support uris and files
            static const boost::regex ignored_keys {"(?<!%)((?:%%)*)%[fFuU]"};
            static const boost::regex percent_re {"%%"};
            static const boost::regex uneven_percents_re {"(?<!%)(?:%%)*%(?!%)"};
            // TODO: Some are unsupported
            static const boost::regex unsupported_re {"(?<!%)((?:%%)*)%[ik]"};
            static const boost::regex name_re {"(?<!%)((?:%%)*)%c"};
            std::string cmdline {exec};
            cmdline = boost::regex_replace(std::move(cmdline), ignored_keys, "$1");
            cmdline = boost::regex_replace(std::move(cmdline), unsupported_re, "$1");
            cmdline = boost::regex_replace(std::move(cmdline), name_re, name);
            if (boost::regex_search(cmdline, uneven_percents_re)) {
                throw std::runtime_error("Invalid desktop file Exec entry");
            }
            cmdline = boost::regex_replace(std::move(cmdline), percent_re, "%");
            return cmdline;
        }

        template<>
        API_PUBLIC std::string_view launch_impl<application_action>::get_path() const {
            auto entry = get_derived()->try_get_entry();
            if (!entry) {
                throw std::runtime_error("desktop_entry of application_action already destroyed");
            }
            return entry->get_path();
        }

        template<>
        API_PUBLIC std::string_view launch_impl<desktop_entry>::get_path() const {
            return get_derived()->get_path();
        }

        template<>
        API_PUBLIC bool launch_impl<application_action>::get_terminal() const {
            auto entry = get_derived()->try_get_entry();
            if (!entry) {
                throw std::runtime_error("desktop_entry of application_action already destroyed");
            }
            return entry->get_terminal();
        }

        template<>
        API_PUBLIC bool launch_impl<desktop_entry>::get_terminal() const {
            return get_derived()->get_terminal();
        }
    } // namespace detail

    std::shared_ptr<desktop_entry> desktop_entry::create() {
        return std::make_shared<desktop_entry>(constructor_tag {});
    }

    std::shared_ptr<desktop_entry> desktop_entry::from_istream(std::istream &is) {
        auto entry = desktop_entry::create();
        is >> entry;
        if (is.fail()) {
            throw std::runtime_error("Parsing desktop_entry failed");
        }
        return entry;
    }

    std::shared_ptr<desktop_entry> desktop_entry::from_store(std::filesystem::path store,
        std::filesystem::path relative_path) {
        std::ifstream is {store / relative_path};

        auto entry             = desktop_entry::from_istream(is);
        entry->m_store         = std::move(store);
        entry->m_relative_path = std::move(relative_path);
        return entry;
    }

    desktop_entry::desktop_entry(constructor_tag) { }

    std::string desktop_entry::get_id() const {
        if (m_relative_path.empty()) {
            return {};
        }
        std::string res = m_relative_path.string();
        std::ranges::replace(res, '/', '-');
        return res;
    }

    bool desktop_entry::should_show() const {
        if (get_no_display() || get_hidden()) {
            return false;
        }

        std::string_view current_desktop = std::getenv("XDG_CURRENT_DESKTOP");
        auto only_show_in                = get_only_show_in();
        if (only_show_in) {
            auto it = std::ranges::find(*only_show_in, current_desktop);
            return it != only_show_in->end();
        }

        auto no_show_in = get_not_show_in();
        if (no_show_in) {
            auto it = std::ranges::find(*no_show_in, current_desktop);
            return it == no_show_in->end();
        }

        return true;
    }

    std::istream &operator>>(std::istream &is, const std::shared_ptr<desktop_entry> &entry) {
        detail::desktop_entry_parser parser {entry};
        parser.parse(is);
        return is;
    }

    std::vector<std::shared_ptr<desktop_entry>> get_all_desktop_entries() {
        std::unordered_set<std::string> ids_read;
        std::vector<std::shared_ptr<desktop_entry>> result;

        for (auto &application_dir : xdg::basedir::data_dir_iterator()) {
            application_dir /= "applications";
            try {
                for (const auto &file : recursive_directory_iterator(application_dir,
                         directory_options::follow_directory_symlink | directory_options::skip_permission_denied)) {
                    if (file.path().extension() != ".desktop") {
                        continue;
                    }

                    std::shared_ptr<desktop_entry> entry;
                    try {
                        entry = desktop_entry::from_store(application_dir,
                            file.path().lexically_relative(application_dir));
                    } catch (const std::runtime_error &ex) {
                        // TODO: Specific exception
                        std::cerr << "Failed to parse desktop file: " << file << '\n';
                        continue;
                    }
                    auto emplace_res = ids_read.emplace(entry->get_id());
                    if (emplace_res.second) {
                        // Entry is new
                        result.emplace_back(std::move(entry));
                    }
                }
            } catch (const filesystem_error &ex) {
                if (ex.code() == std::errc::no_such_file_or_directory) {
                    continue;
                }
                throw;
            }
        }

        return result;
    }

    application_action::application_action(std::weak_ptr<desktop_entry> entry, std::string id) :
            m_entry(std::move(entry)), m_id(std::move(id)) {
        assert(!m_entry.expired());
    }
} // namespace xdg::desktop_entry_spec