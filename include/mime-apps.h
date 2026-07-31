#ifndef LIB_XDGPP_MIME_APPS_H
#define LIB_XDGPP_MIME_APPS_H

#include <map>

#include "desktop-entry.h"

#include "helper.h"

namespace xdg::mime_apps {
    using mime_type      = std::string;
    using application_id = desktop_entry_spec::types::application_id;

    class API_PUBLIC default_applications_storage {
        std::map<mime_type, std::vector<application_id>> m_storage;

    public:
        void add_association(mime_type type, application_id desktop_id) {
            auto [it, success] = m_storage.try_emplace(std::move(type));
            it->second.emplace_back(std::move(desktop_id));
        }

        bool set_associations(mime_type type, std::vector<application_id> desktop_ids) {
            auto [it, is_new] = m_storage.try_emplace(std::move(type));
            it->second        = std::move(desktop_ids);
            return is_new;
        }

        const std::vector<application_id> *get_associations(const mime_type &type) const noexcept {
            auto it = m_storage.find(type);
            return it != m_storage.end() ? std::addressof(it->second) : nullptr;
        }

        std::vector<application_id> *get_associations(const mime_type &type) noexcept {
            auto it = m_storage.find(type);
            return it != m_storage.end() ? std::addressof(it->second) : nullptr;
        }
    };

    class API_PUBLIC changed_mime_types_storage {
        std::map<mime_type, std::vector<application_id>> m_added_associations;
        std::map<mime_type, std::vector<application_id>> m_removed_associations;

        static void remove_association_from(std::map<mime_type, std::vector<application_id>> &map,
            const mime_type &type, const application_id &desktop_id) {
            auto it = map.find(type);
            if (it != map.end()) {
                std::erase(it->second, desktop_id);
            }
        }

        static void add_association_to(std::map<mime_type, std::vector<application_id>> &map,
            mime_type type, application_id desktop_id) {
            auto [it, success] = map.try_emplace(std::move(type));
            it->second.emplace_back(std::move(desktop_id));
        }

        static bool set_associations_in(std::map<mime_type, std::vector<application_id>> &map,
            mime_type type, std::vector<application_id> desktop_ids) {
            auto [it, is_new] = map.try_emplace(std::move(type));
            it->second        = std::move(desktop_ids);
            return is_new;
        }

        static bool contains_association(const std::map<mime_type, std::vector<application_id>> &map,
            const mime_type &type, const application_id &desktop_id) {
            auto it = map.find(type);
            if (it != map.end()) {
                if (std::ranges::find(it->second, desktop_id) != it->second.end()) {
                    return true;
                }
            }
            return false;
        }

    public:
        enum class association_type { Added, Removed, Neutral };

        void add_association(mime_type type, application_id desktop_id) {
            remove_association_from(m_removed_associations, type, desktop_id);
            add_association_to(m_added_associations, std::move(type), std::move(desktop_id));
        }

        bool set_associations(mime_type type, std::vector<application_id> desktop_ids) {
            for (const application_id &id : desktop_ids) {
                remove_association_from(m_removed_associations, type, id);
            }
            return set_associations_in(m_added_associations, std::move(type), std::move(desktop_ids));
        }

        void remove_association(mime_type type, application_id desktop_id) {
            remove_association_from(m_added_associations, type, desktop_id);
            add_association_to(m_removed_associations, std::move(type), std::move(desktop_id));
        }

        bool remove_associations(mime_type type, std::vector<application_id> desktop_ids) {
            for (const application_id &id : desktop_ids) {
                remove_association_from(m_added_associations, type, id);
            }
            return set_associations_in(m_removed_associations, std::move(type), std::move(desktop_ids));
        }

        void clear_association(mime_type type, application_id desktop_id) {
            remove_association_from(m_added_associations, type, desktop_id);
            remove_association_from(m_removed_associations, type, desktop_id);
        }

        association_type get_association(mime_type type, application_id desktop_id) const {
            if (contains_association(m_added_associations, type, desktop_id)) {
                return association_type::Added;
            } else if (contains_association(m_removed_associations, type, desktop_id)) {
                return association_type::Removed;
            } else {
                return association_type::Neutral;
            }
        }
    };

    // TODO: Own type
    using mimeapps_list_data
        = std::pair<std::unique_ptr<default_applications_storage>, std::unique_ptr<changed_mime_types_storage>>;

    class API_PUBLIC mimeapps_list_cache {
        std::map<std::filesystem::path, mimeapps_list_data> m_cache;
        std::vector<std::filesystem::path> m_mimeapps_list_locations;
        std::vector<std::filesystem::path> m_mimeapps_list_locations_with_associations;

    public:
        mimeapps_list_cache();

        mimeapps_list_data &read(const std::filesystem::path &path);

        const std::vector<std::filesystem::path> &get_mimeapps_list_locations() const noexcept {
            return m_mimeapps_list_locations;
        }

        const std::vector<std::filesystem::path> &get_mimeapps_list_locations_with_associations() const noexcept {
            return m_mimeapps_list_locations_with_associations;
        }
    };

    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::filesystem::path path);
    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::filesystem::path path, bool mime_type_change_allowed);
    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::istream &is, bool mime_type_change_allowed);

    std::optional<desktop_entry_spec::application_entry>
        API_PUBLIC get_default_app_for_mime_type(const mime_type &mime_type, mimeapps_list_cache &cache);
    std::optional<desktop_entry_spec::application_entry>
        API_PUBLIC get_default_app_for_mime_type(const mime_type &mime_type);
} // namespace xdg::mime_apps

#endif
