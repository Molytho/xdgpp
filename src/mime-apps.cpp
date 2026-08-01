#include "mime-apps.h"

#include <cctype>
#include <fstream>
#include <memory>

#include <boost/regex.hpp>

#include "basedir.h"
#include "private/string_helper.h"

using namespace xdg::mime_apps;

namespace {
    const boost::regex IsCommentRe {"^[ \\t]*(?:#|$)", boost::regex::optimize};
    const boost::regex ParseGroupHeadRe {"^\\[([ -Z\\\\^-~]+)\\]$", boost::regex::optimize};
    const boost::regex ParseKeyValueRe {
        "^[ \\t]*([A-Za-z0-9+\\/.-]+)[ \\t]*=[ \\t]*(.*)$",
        boost::regex_constants::flag_type_::optimize
    };

    class file_parser_base {
    protected:
        struct section_handler {
            virtual ~section_handler() = default;

            virtual void set_association(mime_type, std::vector<application_id>) { }
        };

        std::istream &m_is;
        std::unique_ptr<section_handler> m_current_section_handler;

        static bool is_comment_line(const std::string &str) {
            return str.empty() || boost::regex_search(str.begin(), str.end(), IsCommentRe);
        }

        std::string get_non_blank_line() {
            for (std::string line; std::getline(m_is, line);) {
                if (!is_comment_line(line)) {
                    return line;
                }
            }
            return {};
        }

        [[nodiscard]] static std::string_view parse_as_group_header(const std::string &str) noexcept {
            boost::match_results<std::string::const_iterator> match;
            if (boost::regex_match(str.begin(), str.end(), match, ParseGroupHeadRe)) {
                return std::string_view(match[1].begin(), match[1].end());
            }
            return {};
        }

        [[nodiscard]] static std::pair<std::string_view, std::string_view>
            parse_as_key_value(const std::string &str) {
            boost::match_results<std::string::const_iterator> match;
            if (!boost::regex_match(str.begin(), str.end(), match, ParseKeyValueRe)) {
                throw std::runtime_error("Line did not match");
            }
            return {
                {match[1].begin(), match[1].end()},
                {match[2].begin(), match[2].end()}
            };
        }

        virtual void update_section(std::string_view name) = 0;

        [[nodiscard]] static std::vector<application_id> parse_desktop_id_list_single(std::string_view str) {
            constexpr char delimiter = ';';

            assert(!str.empty());
            if (str.at(0) == delimiter) {
                throw std::runtime_error("Invalid desktop id list");
            }

            size_t prev_pos    = 0;
            size_t current_pos = str.find(delimiter, prev_pos);
            while (current_pos != std::string_view::npos) {
                // TODO/FIXME: The escape characters need to be removed
                throw std::runtime_error("Unimplemented");

                if (str.at(current_pos - 1) != '\\') {
                    throw std::runtime_error("Invalid desktop id list");
                }

                prev_pos    = current_pos;
                current_pos = str.find(delimiter, prev_pos);
            }


            return {std::string(str)};
        }

        [[nodiscard]] static std::vector<application_id> parse_desktop_id_list(std::string_view str) {
            constexpr char delimiter = ';';
            if (str.empty()) {
                throw std::runtime_error("desktop id list can\'t be empty");
            } else if (str.ends_with(delimiter)) {
                auto view = xdg::detail::utils::string_spliterator(str.substr(0, str.size() - 1), delimiter)
                            | std::views::transform([](std::string_view str) -> application_id {
                                  return str;
                              });
                return {std::move_iterator(view.begin()), std::move_iterator(view.end())};
            } else {
                return parse_desktop_id_list_single(str);
            }
        }

        void process_line(const std::string &line) {
            if (auto name = parse_as_group_header(line); !name.empty()) {
                update_section(name);
            } else {
                auto [key, value] = parse_as_key_value(line);
                m_current_section_handler->set_association(mime_type(key), parse_desktop_id_list(value));
            }
        }

    public:
        file_parser_base(std::istream &is) : m_is(is) { }

        void parse() {
            std::string line = get_non_blank_line();
            if (line.empty()) {
                return;
            }

            update_section([&]() {
                auto group_header = parse_as_group_header(line);
                if (group_header.empty()) {
                    throw std::runtime_error("File must start with a section header");
                }
                return group_header;
            }());

            for (line = get_non_blank_line(); !line.empty(); line = get_non_blank_line()) {
                process_line(line);
            }
        }
    };

    class mimeapps_list_parser : public file_parser_base {
        struct add_associations_handler : section_handler {
            add_associations_handler(mimeapps_list_parser &parser) : m_parser(parser) {
                if (!m_parser.m_changed_mime_type_storage) {
                    m_parser.m_changed_mime_type_storage = std::make_unique<changed_mime_types_storage>();
                }
            }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage->set_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Added Associations\" section\n";
                }
            }

        private:
            mimeapps_list_parser &m_parser;
        };

        struct removed_associations_handler : section_handler {
            removed_associations_handler(mimeapps_list_parser &parser) : m_parser(parser) {
                if (!m_parser.m_changed_mime_type_storage) {
                    m_parser.m_changed_mime_type_storage = std::make_unique<changed_mime_types_storage>();
                }
            }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage->remove_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Removed Associations\" section\n";
                }
            }

        private:
            mimeapps_list_parser &m_parser;
        };

        struct default_applications_handler : section_handler {
            default_applications_handler(mimeapps_list_parser &parser) : m_parser(parser) {
                if (!m_parser.m_applications_storage) {
                    m_parser.m_applications_storage = std::make_unique<default_applications_storage>();
                }
            }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_parser.m_applications_storage->set_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Default Applications\" section\n";
                }
            }

        private:
            mimeapps_list_parser &m_parser;
        };

        bool m_mime_type_change_allowed;

        std::unique_ptr<default_applications_storage> m_applications_storage {};
        std::unique_ptr<changed_mime_types_storage> m_changed_mime_type_storage {};

        void update_section(std::string_view name) final {
            if (name == "Default Applications") {
                m_current_section_handler = std::make_unique<default_applications_handler>(*this);
            } else if (name == "Added Associations") {
                m_current_section_handler
                    = m_mime_type_change_allowed
                          ? std::make_unique<add_associations_handler>(*this)
                          : throw std::runtime_error("Added Assocations section not allowed");
            } else if (name == "Removed Associations") {
                m_current_section_handler
                    = m_mime_type_change_allowed
                          ? std::make_unique<removed_associations_handler>(*this)
                          : throw std::runtime_error("Removed Assocations section not allowed");

            } else {
                m_current_section_handler = std::make_unique<section_handler>();
            }
        }

    public:
        mimeapps_list_parser(std::istream &is, bool mime_type_change_allowed) :
                file_parser_base(is), m_mime_type_change_allowed(mime_type_change_allowed) { }

        mimeapps_list_data get_result() && noexcept {
            return {std::move(m_applications_storage), std::move(m_changed_mime_type_storage)};
        }
    };

    std::vector<std::string> get_desktop_names() {
        auto envvar = std::getenv("XDG_CURRENT_DESKTOP");
        if (!envvar) {
            return {};
        }

        auto names = xdg::detail::utils::split_string(envvar, ',');
        for (std::string &name : names) {
            for (char &c : name) {
                c = std::tolower(c);
            }
        }
        return names;
    }

    std::vector<std::filesystem::path> get_mimeapps_list_locations(bool allow_desktop_specific = true) {
        std::vector<std::string> desktop_names {};
        if (allow_desktop_specific) {
            desktop_names = get_desktop_names();
        }

        constexpr auto append_applications_component = [](std::filesystem::path &path) {
            path /= "applications";
        };

        auto config_home = xdg::basedir::get_config_home();
        auto config_dirs = xdg::basedir::get_config_dirs();
        auto data_home   = xdg::basedir::get_data_home();
        append_applications_component(data_home);
        auto data_dirs = xdg::basedir::get_data_dirs();
        std::ranges::for_each(data_dirs, append_applications_component);

        std::vector<std::filesystem::path> result;
        result.reserve((desktop_names.size() + 1) * (1 + config_dirs.size() + 1 + data_dirs.size()));
        auto add_dir = [&](std::filesystem::path &base) {
            if (allow_desktop_specific) {
                for (const auto &name : desktop_names) {
                    result.emplace_back(base / (name + "-mimeapps.list"));
                }
            }
            base.append("mimeapps.list");
            result.emplace_back(std::move(base));
        };
        auto add_dirs = [&](std::vector<std::filesystem::path> &vec) {
            if (allow_desktop_specific) {
                for (const std::filesystem::path &base : vec) {
                    for (const auto &name : desktop_names) {
                        result.emplace_back(base / (name + "-mimeapps.list"));
                    }
                }
            }
            for (std::filesystem::path &base : vec) {
                base.append("mimeapps.list");
                result.emplace_back(std::move(base));
            }
        };
        add_dir(config_home);
        add_dirs(config_dirs);
        add_dir(data_home);
        add_dirs(data_dirs);
        return result;
    }

    bool check_mime_type_allowed_for(const mime_type &mime_type,
        const xdg::desktop_entry_spec::search_result &result, mimeapps_list_cache &cache) {
        using association_type = xdg::mime_apps::changed_mime_types_storage::association_type;

        for (const auto &mimeapp_list : cache.get_mimeapps_list_locations_with_associations()) {
            auto &[default_apps, added_removed_types] = cache.read(mimeapp_list);

            if (added_removed_types) {
                auto type = added_removed_types->get_association(mime_type, result.entry.get_id());
                if (type == association_type::Added) {
                    return true;
                } else if (type == association_type::Removed) {
                    return false;
                }
            }

            if (mimeapp_list == result.store) {
                break;
            }
        }

        if (auto mime_types = result.entry.get_mime_types();
            mime_types && std::ranges::find(*mime_types, mime_type) != mime_types->end()) {
            return true;
        }

        return false;
    }
} // namespace

namespace xdg::mime_apps {
    mimeapps_list_cache::mimeapps_list_cache() :
            m_cache(), m_mimeapps_list_locations(::get_mimeapps_list_locations()),
            m_mimeapps_list_locations_with_associations(::get_mimeapps_list_locations(false)) { }

    mimeapps_list_data &mimeapps_list_cache::read(const std::filesystem::path &path) {
        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            return it->second;
        }

        mimeapps_list_data data;
        try {
            data = parse_mimeapps_list(path);
        } catch (const std::runtime_error &) {
            // This is find
        }

        auto [new_it, success] = m_cache.emplace(path, std::move(data));
        assert(success);
        return new_it->second;
    }

    mimeapps_list_data parse_mimeapps_list(std::filesystem::path path) {
        bool mime_type_change_allowed = path.filename() == "mimeapps.list";
        return parse_mimeapps_list(std::move(path), mime_type_change_allowed);
    }

    mimeapps_list_data parse_mimeapps_list(std::filesystem::path path, bool mime_type_change_allowed) {
        std::ifstream file {path};
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file");
        }
        return parse_mimeapps_list(file, mime_type_change_allowed);
    }

    mimeapps_list_data parse_mimeapps_list(std::istream &is, bool mime_type_change_allowed) {
        mimeapps_list_parser parser {is, mime_type_change_allowed};
        parser.parse();
        return std::move(parser).get_result();
    }

    std::optional<desktop_entry_spec::application_entry>
        get_default_app_for_mime_type(const mime_type &mime_type, mimeapps_list_cache &cache) {
        for (const auto &path : cache.get_mimeapps_list_locations()) {
            auto &[default_apps, ignore] = cache.read(path);
            if (!default_apps) {
                continue;
            }

            const auto *associations = default_apps->get_associations(mime_type);
            if (!associations) {
                continue;
            }

            for (const auto &default_app : *associations) {
                auto search_result = desktop_entry_spec::search_application_entry(default_app);
                if (search_result && check_mime_type_allowed_for(mime_type, *search_result, cache)) {
                    return std::move(search_result)->entry;
                }
            }
        }
        return {};
    }

    std::optional<desktop_entry_spec::application_entry> get_default_app_for_mime_type(const mime_type &mime_type) {
        mimeapps_list_cache cache {};
        return get_default_app_for_mime_type(std::move(mime_type), cache);
    }
} // namespace xdg::mime_apps