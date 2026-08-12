#include "uri_helper.h"

#include <boost/regex.hpp>

std::string_view get_uri_schema(std::string_view str) {
    static const boost::regex uri_scheme_re {"^([[:alpha:]][[:alpha:][:digit:]+.-]*):"};
    boost::match_results<std::string_view::const_iterator> match;
    if (boost::regex_search(str.begin(), str.end(), match, uri_scheme_re)) {
        return {match[0].begin(), match[1].end()};
    }
    return {};
}

bool is_uri(std::string_view str) {
    return !get_uri_schema(str).empty();
}
