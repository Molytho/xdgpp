#ifndef LIB_XDGPP_DESKTOP_ENTRY_PARSER_H
# define LIB_XDGPP_DESKTOP_ENTRY_PARSER_H

#include "desktop-entry.h"

namespace xdg::desktop_entry_spec::detail {
    class desktop_entry_parser {
        std::shared_ptr<desktop_entry> m_target;

        bool m_is_main_section {false};
        bool m_skip_section {false};
        std::string m_current_section {};
        application_action *m_current_action {};

        void update_current_section(std::string_view section);
        void parse_key_value_line(std::string str);
        bool entry_has_action(std::string_view str);
        void reset_flags() noexcept;
        void check_for_required_keys();

    public:
        desktop_entry_parser(std::shared_ptr<desktop_entry> target);

        void parse(std::istream &is);
    };
}

#endif