#include "mime-apps.h"

#include <fstream>
#include <map>
#include <memory>
#include <set>

#include <boost/regex.hpp>

#include "basedir.h"

#include "molytho-cpp-utils/string_spliterator.h"

using namespace xdg::mime_apps;

namespace xdg::mime_apps {
    class mimeapps_list_cache {
        std::map<std::filesystem::path, mimeapps_list_data> m_cache;
        std::map<std::filesystem::path, mimeinfo_cache_storage> m_mimeinfo_cache_cache;
        std::vector<std::filesystem::path> m_mimeapps_list_locations;
        std::vector<std::filesystem::path> m_mimeapps_list_locations_with_associations;

    public:
        mimeapps_list_cache();

        mimeapps_list_data &read(const std::filesystem::path &path);
        mimeinfo_cache_storage *read_mimeinfo_cache(const std::filesystem::path &path);

        const std::vector<std::filesystem::path> &get_mimeapps_list_locations() const noexcept {
            return m_mimeapps_list_locations;
        }

        const std::vector<std::filesystem::path> &get_mimeapps_list_locations_with_associations() const noexcept {
            return m_mimeapps_list_locations_with_associations;
        }
    };
} // namespace xdg::mime_apps

namespace {
    const boost::regex IsCommentRe {"^[ \\t]*(?:#|$)", boost::regex::optimize};
    const boost::regex ParseGroupHeadRe {"^\\[([ -Z\\\\^-~]+)\\]$", boost::regex::optimize};
    const boost::regex ParseKeyValueRe {
        "^[ \\t]*([A-Za-z0-9+_\\/.-]+)[ \\t]*=[ \\t]*(.*)$",
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

        void process_line(const std::string &line) {
            if (auto name = parse_as_group_header(line); !name.empty()) {
                update_section(name);
            } else {
                auto [key, value] = parse_as_key_value(line);
                m_current_section_handler->set_association(mime_type(key),
                    xdg::desktop_entry_spec::types::parse<std::vector<application_id>>(value));
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
            add_associations_handler(mimeapps_list_parser &parser) : m_parser(parser) { }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage.set_associations(std::move(type),
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
            removed_associations_handler(mimeapps_list_parser &parser) : m_parser(parser) { }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_parser.m_changed_mime_type_storage.remove_associations(std::move(type),
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
            default_applications_handler(mimeapps_list_parser &parser) : m_parser(parser) { }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new
                    = m_parser.m_applications_storage.set_associations(std::move(type), std::move(value));
                if (!is_new) {
                    std::cerr
                        << "mimetype appeard multiple times in \"Default Applications\" section\n";
                }
            }

        private:
            mimeapps_list_parser &m_parser;
        };

        bool m_mime_type_change_allowed;

        default_applications_storage m_applications_storage {};
        changed_mime_types_storage m_changed_mime_type_storage {};

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

    class mimeinfo_cache_parser : public file_parser_base {
        struct mime_cache_handler : section_handler {
            mime_cache_handler(mimeinfo_cache_storage &storage) : m_storage(storage) { }

            void set_association(mime_type type, std::vector<application_id> value) final {
                bool is_new = m_storage.set_associations(std::move(type), std::move(value));
                if (!is_new) {
                    std::cerr << "mimetype appeard multiple times in \"MIME Cache\" section\n";
                }
            }

        private:
            mimeinfo_cache_storage &m_storage;
        };

        mimeinfo_cache_storage m_storage {};

        void update_section(std::string_view name) final {
            if (name == "MIME Cache") {
                m_current_section_handler = std::make_unique<mime_cache_handler>(this->m_storage);
            } else {
                m_current_section_handler = std::make_unique<section_handler>();
            }
        }

    public:
        mimeinfo_cache_parser(std::istream &is) : file_parser_base(is), m_storage() { }

        mimeinfo_cache_storage get_result() && noexcept { return std::move(m_storage); }
    };

    std::vector<std::string> get_desktop_names() {
        auto envvar = std::getenv("XDG_CURRENT_DESKTOP");
        if (!envvar) {
            return {};
        }

        auto names = molytho::utils::split_string(envvar, ',');
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

    template<class T, class U>
    bool contains(const std::vector<T> &vec, const U &value) {
        auto it = std::ranges::find(vec, value);
        return it != vec.end();
    }

    template<class T, class U>
    bool contains(const std::vector<T> *vec, const U &value) {
        return vec && contains(*vec, value);
    }

    template<class R>
        requires(!std::is_pointer_v<R>)
    void add_to(std::vector<application_id> &vec, R &&range) {
        vec.insert(vec.cend(), std::ranges::begin(range), std::ranges::end(range));
    }

    template<class R>
        requires(!std::is_pointer_v<R>)
    void add_to(std::set<application_id> &vec, R &&range) {
        vec.insert(std::ranges::begin(range), std::ranges::end(range));
    }

    template<class T, class R>
        requires(std::same_as<T, std::set<application_id>> || std::same_as<T, std::vector<application_id>>)
                && std::is_pointer_v<R>
    void add_to(T &vec, R p) {
        if (p) {
            add_to(vec, *p);
        }
    }

    bool check_mime_type_allowed_for(mime_type initial_mime_type,
        const xdg::desktop_entry_spec::search_result &result, mimeapps_list_cache &cache) {
        using association_type = xdg::mime_apps::changed_mime_types_storage::association_type;

        for (const auto &mime_type : mime_type_parent_iterator(std::move(initial_mime_type))) {
            for (const auto &mimeapp_list : cache.get_mimeapps_list_locations_with_associations()) {
                auto &[default_apps, added_removed_types] = cache.read(mimeapp_list);

                if (added_removed_types) {
                    auto type = added_removed_types.get_association(mime_type, result.entry.get_id());
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

            if (contains(result.entry.get_mime_types(), mime_type)) {
                return true;
            }
        }

        return false;
    }

    bool is_application_store(const std::filesystem::path &path) {
        auto end = std::reverse_iterator(path.begin());
        auto it  = std::reverse_iterator(path.end());
        return it != end && (++it) != end && *it == "applications";
    }

    void read_associations_from_store(std::vector<application_id> &result, std::set<application_id> &blacklist,
        const mime_type &type, const std::filesystem::path &store, mimeapps_list_cache &cache) {
        if (auto mimeinfo_cache = cache.read_mimeinfo_cache(store / "mimeinfo.cache"); mimeinfo_cache) {
            add_to(result, mimeinfo_cache->get_associations(type));

            try {
                for (const auto &entry : std::filesystem::recursive_directory_iterator(store,
                         std::filesystem::directory_options::follow_directory_symlink)) {
                    if (entry.path().extension() != ".desktop") {
                        continue;
                    }
                    blacklist.emplace(entry.path().lexically_relative(store));
                }
            } catch (const std::filesystem::filesystem_error &ex) {
                if (ex.code() != std::errc::no_such_file_or_directory) {
                    throw;
                }
            }
        } else {
            auto is_not_blacklisted = [&blacklist](const std::filesystem::path &path) {
                application_id id {path};
                return !blacklist.contains(id);
            };
            auto desktop_entries
                = xdg::desktop_entry_spec::read_desktop_entries_from_predicated(store, is_not_blacklisted);

            for (auto application_entry : std::views::filter(desktop_entries, [](const auto &entry) {
                     return entry.get_type() == xdg::desktop_entry_spec::types::entry_type::Application;
                 }) | std::views::transform([](auto &entry) {
                     return xdg::desktop_entry_spec::application_entry(std::move(entry));
                 })) {
                if (contains(application_entry.get_mime_types(), type)) {
                    result.emplace_back(application_entry.get_id());
                }
                blacklist.emplace(application_entry.get_id());
            }
        }
    }

    void remove_association_from(std::map<mime_type, std::vector<application_id>> &map,
        const mime_type &type, const application_id &desktop_id) {
        auto it = map.find(type);
        if (it != map.end()) {
            std::erase(it->second, desktop_id);
        }
    }

    void add_association_to(std::map<mime_type, std::vector<application_id>> &map, mime_type type,
        application_id desktop_id) {
        auto [it, success] = map.try_emplace(std::move(type));
        it->second.emplace_back(std::move(desktop_id));
    }

    bool set_associations_in(std::map<mime_type, std::vector<application_id>> &map, mime_type type,
        std::vector<application_id> desktop_ids) {
        auto [it, is_new] = map.try_emplace(std::move(type));
        it->second        = std::move(desktop_ids);
        return is_new;
    }

    bool contains_association(const std::map<mime_type, std::vector<application_id>> &map,
        const mime_type &type, const application_id &desktop_id) {
        auto it = map.find(type);
        if (it != map.end()) {
            if (std::ranges::find(it->second, desktop_id) != it->second.end()) {
                return true;
            }
        }
        return false;
    }
} // namespace

namespace xdg::mime_apps {
    struct association_storage::data {
        std::map<mime_type, std::vector<application_id>> storage;
    };

    association_storage::data *association_storage::get_or_init_data() {
        if (!m_data) {
            m_data = std::make_unique<association_storage::data>();
        }
        return m_data.get();
    }

    association_storage::association_storage()                                  = default;
    association_storage::association_storage(association_storage &&)            = default;
    association_storage &association_storage::operator=(association_storage &&) = default;

    association_storage::~association_storage() = default;

    void association_storage::add_association(mime_type type, application_id desktop_id) {
        auto [it, success] = get_or_init_data()->storage.try_emplace(std::move(type));
        it->second.emplace_back(std::move(desktop_id));
    }

    void association_storage::add_association(mime_type type, std::vector<application_id> desktop_ids) {
        auto [it, success] = get_or_init_data()->storage.try_emplace(std::move(type));
        it->second.insert(it->second.cend(),
            std::move_iterator(desktop_ids.begin()),
            std::move_iterator(desktop_ids.end()));
    }

    bool association_storage::set_associations(mime_type type, std::vector<application_id> desktop_ids) {
        auto [it, is_new] = get_or_init_data()->storage.try_emplace(std::move(type));
        it->second        = std::move(desktop_ids);
        return is_new;
    }

    const std::vector<application_id> *association_storage::get_associations(const mime_type &type) const noexcept {
        if (!m_data) {
            return nullptr;
        }
        auto it = m_data->storage.find(type);
        return it != m_data->storage.end() ? std::addressof(it->second) : nullptr;
    }

    std::vector<application_id> *association_storage::get_associations(const mime_type &type) noexcept {
        if (!m_data) {
            return nullptr;
        }
        auto it = m_data->storage.find(type);
        return it != m_data->storage.end() ? std::addressof(it->second) : nullptr;
    }

    association_storage::operator bool() const noexcept {
        return bool(m_data);
    }

    struct changed_mime_types_storage::data {
        std::map<mime_type, std::vector<application_id>> added_associations;
        std::map<mime_type, std::vector<application_id>> removed_associations;
    };

    changed_mime_types_storage::data &changed_mime_types_storage::get_or_init_data() {
        if (!m_data) {
            m_data = std::make_unique<data>();
        }
        return *m_data;
    }

    changed_mime_types_storage::changed_mime_types_storage()                              = default;
    changed_mime_types_storage::changed_mime_types_storage(changed_mime_types_storage &&) = default;
    changed_mime_types_storage &changed_mime_types_storage::operator=(changed_mime_types_storage &&) = default;
    changed_mime_types_storage::~changed_mime_types_storage() = default;

    void changed_mime_types_storage::add_association(mime_type type, application_id desktop_id) {
        auto &data = get_or_init_data();
        remove_association_from(data.removed_associations, type, desktop_id);
        add_association_to(data.added_associations, std::move(type), std::move(desktop_id));
    }

    bool changed_mime_types_storage::set_associations(mime_type type, std::vector<application_id> desktop_ids) {
        auto &data = get_or_init_data();
        for (const application_id &id : desktop_ids) {
            remove_association_from(data.removed_associations, type, id);
        }
        return set_associations_in(data.added_associations, std::move(type), std::move(desktop_ids));
    }

    void changed_mime_types_storage::remove_association(mime_type type, application_id desktop_id) {
        auto &data = get_or_init_data();
        remove_association_from(data.added_associations, type, desktop_id);
        add_association_to(data.removed_associations, std::move(type), std::move(desktop_id));
    }

    bool changed_mime_types_storage::remove_associations(mime_type type, std::vector<application_id> desktop_ids) {
        auto &data = get_or_init_data();
        for (const application_id &id : desktop_ids) {
            remove_association_from(data.added_associations, type, id);
        }
        return set_associations_in(data.removed_associations, std::move(type), std::move(desktop_ids));
    }

    void changed_mime_types_storage::clear_association(const mime_type &type, const application_id &desktop_id) {
        if (m_data) {
            remove_association_from(m_data->added_associations, type, desktop_id);
            remove_association_from(m_data->removed_associations, type, desktop_id);
        }
    }

    changed_mime_types_storage::association_type changed_mime_types_storage::get_association(
        const mime_type &type, const application_id &desktop_id
    ) const {
        if (!m_data) {
            return association_type::Neutral;
        } else if (contains_association(m_data->added_associations, type, desktop_id)) {
            return association_type::Added;
        } else if (contains_association(m_data->removed_associations, type, desktop_id)) {
            return association_type::Removed;
        } else {
            return association_type::Neutral;
        }
    }

    const std::vector<application_id> *
        changed_mime_types_storage::get_added_associations(const mime_type &type) const noexcept {
        if (!m_data) {
            return nullptr;
        }
        auto it = m_data->added_associations.find(type);
        return it != m_data->added_associations.end() ? std::addressof(it->second) : nullptr;
    }

    const std::vector<application_id> *
        changed_mime_types_storage::get_removed_associations(const mime_type &type) const noexcept {
        if (!m_data) {
            return nullptr;
        }
        auto it = m_data->removed_associations.find(type);
        return it != m_data->removed_associations.end() ? std::addressof(it->second) : nullptr;
    }

    changed_mime_types_storage::operator bool() const noexcept {
        return bool(m_data);
    }

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
            // This is fine
        }

        auto [new_it, success] = m_cache.emplace(path, std::move(data));
        assert(success);
        return new_it->second;
    }

    mimeinfo_cache_storage *mimeapps_list_cache::read_mimeinfo_cache(const std::filesystem::path &path) {
        auto it = m_mimeinfo_cache_cache.find(path);
        if (it != m_mimeinfo_cache_cache.end()) {
            return std::addressof(it->second);
        }

        mimeinfo_cache_storage data;
        try {
            data = parse_mimeinfo_cache(path);
        } catch (const std::runtime_error &) {
            // This is fine
        }

        auto [new_it, success] = m_mimeinfo_cache_cache.emplace(path, std::move(data));
        assert(success);
        return std::addressof(new_it->second);
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

    mimeinfo_cache_storage parse_mimeinfo_cache(std::filesystem::path path) {
        std::ifstream file {path};
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file");
        }
        return parse_mimeinfo_cache(file);
    }

    mimeinfo_cache_storage parse_mimeinfo_cache(std::istream &is) {
        mimeinfo_cache_parser parser {is};
        parser.parse();
        return std::move(parser).get_result();
    }

    std::vector<application_id> get_available_applications_for_mime_type(const mime_type &type,
        mimeapps_list_cache &cache) {
        std::vector<application_id> result;
        std::set<application_id> blacklist;

        auto is_not_blacklisted = [&blacklist](const application_id &id) {
            return !blacklist.contains(id);
        };

        for (auto path : cache.get_mimeapps_list_locations_with_associations()) {
            if (auto &[ignore, edited_associations] = cache.read(path); edited_associations) {
                if (auto added = edited_associations.get_added_associations(type); added) {
                    add_to(result, std::views::filter(*added, is_not_blacklisted));
                }
                add_to(blacklist, edited_associations.get_removed_associations(type));
            }

            if (is_application_store(path)) {
                path.remove_filename();
                read_associations_from_store(result, blacklist, type, path, cache);
            }
        }

        return result;
    }

    std::vector<application_id> get_available_applications_for_mime_type(const mime_type &type) {
        mimeapps_list_cache cache;
        return get_available_applications_for_mime_type(type, cache);
    }

    std::optional<desktop_entry_spec::application_entry>
        get_default_app_for_mime_type(mime_type initial_mime_type, mimeapps_list_cache &cache) {
        for (const auto &mime_type : mime_type_parent_iterator(std::move(initial_mime_type))) {
            for (const auto &path : cache.get_mimeapps_list_locations()) {
                auto &[default_apps, ignore] = cache.read(path);
                if (!default_apps) {
                    continue;
                }

                const auto *associations = default_apps.get_associations(mime_type);
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

            auto associations = get_available_applications_for_mime_type(mime_type, cache);
            for (const auto &default_app : associations) {
                auto search_result = desktop_entry_spec::search_application_entry(default_app);
                if (search_result && check_mime_type_allowed_for(mime_type, *search_result, cache)) {
                    return std::move(search_result)->entry;
                }
            }
        }

        return std::nullopt;
    }

    std::optional<desktop_entry_spec::application_entry> get_default_app_for_mime_type(mime_type mime_type) {
        mimeapps_list_cache cache {};
        return get_default_app_for_mime_type(std::move(mime_type), cache);
    }

    std::unique_ptr<mimeapps_list_cache> make_cache() {
        return std::make_unique<mimeapps_list_cache>();
    }

} // namespace xdg::mime_apps