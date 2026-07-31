#include "mime-apps.h"

#include <fstream>
#include <memory>

#include <boost/regex.hpp>

#include "private/string_helper.h"

using namespace xdg::mime_apps;

namespace {
    const boost::regex IsCommentRe {"^[ \\t]*(?:#|$)", boost::regex::optimize};
    const boost::regex ParseGroupHeadRe {"^\\[([ -Z\\\\^-~]+)\\]$", boost::regex::optimize};
    const boost::regex ParseKeyValueRe {
        "^[ \\t]*([A-Za-z0-9+\\/.-]+)[ \\t]*=[ \\t]*(.*)$",
        boost::regex_constants::flag_type_::optimize
    };

    class file_parser {
        struct section_handler {
            section_handler(file_parser &parser) : m_parser(parser) { }

            virtual ~section_handler() = default;

            virtual void set_association(mime_type, std::vector<std::string>) { }

        protected:
            file_parser &m_parser;
        };

        struct add_associations_handler : section_handler {
            add_associations_handler(file_parser &parser) : section_handler(parser) {
                if (!m_parser.m_changed_mime_type_storage) {
                    m_parser.m_changed_mime_type_storage = std::make_unique<changed_mime_types_storage>();
                }
            }

            void set_association(mime_type type, std::vector<std::string> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage->set_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Added Associations\" section\n";
                }
            }
        };

        struct removed_associations_handler : section_handler {
            removed_associations_handler(file_parser &parser) : section_handler(parser) {
                if (!m_parser.m_changed_mime_type_storage) {
                    m_parser.m_changed_mime_type_storage = std::make_unique<changed_mime_types_storage>();
                }
            }

            void set_association(mime_type type, std::vector<std::string> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage->remove_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Removed Associations\" section\n";
                }
            }
        };

        struct default_applications_handler : section_handler {
            default_applications_handler(file_parser &parser) : section_handler(parser) {
                if (!m_parser.m_applications_storage) {
                    m_parser.m_applications_storage = std::make_unique<default_applications_storage>();
                }
            }

            void set_association(mime_type type, std::vector<std::string> value) final {
                bool is_new = m_parser.m_applications_storage->set_associations(std::move(type),
                    std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Default Applications\" section\n";
                }
            }
        };

        std::istream &m_is;
        std::unique_ptr<section_handler> m_current_section_handler;
        bool m_mime_type_change_allowed;

        std::unique_ptr<default_applications_storage> m_applications_storage {};
        std::unique_ptr<changed_mime_types_storage> m_changed_mime_type_storage {};

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

        void update_section(std::string_view name) {
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
                m_current_section_handler = std::make_unique<section_handler>(*this);
            }
        }

        [[nodiscard]] static std::vector<std::string> parse_desktop_id_list_single(std::string_view str) {
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

        [[nodiscard]] static std::vector<std::string> parse_desktop_id_list(std::string_view str) {
            constexpr char delimiter = ';';
            if (str.empty()) {
                throw std::runtime_error("desktop id list can\'t be empty");
            } else if (str.ends_with(delimiter)) {
                return xdg::detail::utils::split_string(str.substr(0, str.size() - 1), delimiter);
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
        file_parser(std::istream &is, bool mime_type_change_allowed) :
                m_is(is), m_mime_type_change_allowed(mime_type_change_allowed) { }

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

        std::pair<std::unique_ptr<default_applications_storage>, std::unique_ptr<changed_mime_types_storage>>
            get_result() && noexcept {
            return {std::move(m_applications_storage), std::move(m_changed_mime_type_storage)};
        }
    };
} // namespace

namespace xdg::mime_apps {
    std::pair<std::unique_ptr<default_applications_storage>, std::unique_ptr<changed_mime_types_storage>>
        parse_mimeapps_list(std::filesystem::path path) {
        bool mime_type_change_allowed = path.filename() == "mimeapps.list";
        return parse_mimeapps_list(std::move(path), mime_type_change_allowed);
    }

    std::pair<std::unique_ptr<default_applications_storage>, std::unique_ptr<changed_mime_types_storage>>
        parse_mimeapps_list(std::filesystem::path path, bool mime_type_change_allowed) {
        std::ifstream file {path};
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file");
        }
        return parse_mimeapps_list(file, mime_type_change_allowed);
    }

    std::pair<std::unique_ptr<default_applications_storage>, std::unique_ptr<changed_mime_types_storage>>
        parse_mimeapps_list(std::istream &is, bool mime_type_change_allowed) {
        file_parser parser {is, mime_type_change_allowed};
        parser.parse();
        return std::move(parser).get_result();
    }
} // namespace xdg::mime_apps