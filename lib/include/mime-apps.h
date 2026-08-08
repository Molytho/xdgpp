#ifndef LIB_XDGPP_MIME_APPS_H
#define LIB_XDGPP_MIME_APPS_H

#include "desktop-entry.h"

#include "helper.h"

namespace xdg::mime_apps {
    class API_PUBLIC mime_type {
        std::string m_data;

    public:
        mime_type();
        mime_type(std::string str);
        mime_type(std::string_view str);

        mime_type(const mime_type &);
        mime_type &operator=(const mime_type &);
        mime_type(mime_type &&);
        mime_type &operator=(mime_type &&);

        std::string_view str() const noexcept;

        void make_less_specific() noexcept;

        explicit operator bool() const noexcept;

        bool operator==(const mime_type &other) const noexcept;
        friend API_PUBLIC bool operator==(const mime_type &lhs, std::string_view rhs) noexcept;
        friend API_PUBLIC bool operator==(std::string_view lhs, const mime_type &rhs) noexcept;
        friend API_PUBLIC bool operator==(const mime_type &lhs, const std::string &rhs) noexcept;
        friend API_PUBLIC bool operator==(const std::string &lhs, const mime_type &rhs) noexcept;
        std::strong_ordering operator<=>(const mime_type &other) const noexcept;
    };

    std::ostream API_PUBLIC &operator<<(std::ostream &os, const mime_type &type);
} // namespace xdg::mime_apps

namespace std {
    template<>
    class hash<xdg::mime_apps::mime_type> {
        std::hash<std::string_view> m_hasher {};

    public:
        auto operator()(const xdg::mime_apps::mime_type &type) const noexcept {
            return m_hasher(type.str());
        }
    };
} // namespace std

namespace xdg::mime_apps {
    using application_id = desktop_entry_spec::types::application_id;

    class API_PUBLIC association_storage {
        struct data;
        std::unique_ptr<data> m_data;

        data *get_or_init_data();

    public:
        association_storage();
        association_storage(association_storage &&);
        association_storage &operator=(association_storage &&);
        ~association_storage();

        void add_association(mime_type type, application_id desktop_id);
        void add_association(mime_type type, std::vector<application_id> desktop_ids);
        bool set_associations(mime_type type, std::vector<application_id> desktop_ids);
        const std::vector<application_id> *get_associations(const mime_type &type) const noexcept;
        std::vector<application_id> *get_associations(const mime_type &type) noexcept;
        explicit operator bool() const noexcept;
    };

    class API_PUBLIC default_applications_storage : public association_storage { };

    class API_PUBLIC changed_mime_types_storage {
        struct data;
        std::unique_ptr<data> m_data;

        data &get_or_init_data();

    public:
        enum class association_type { Added, Removed, Neutral };

        changed_mime_types_storage();
        changed_mime_types_storage(changed_mime_types_storage &&);
        changed_mime_types_storage &operator=(changed_mime_types_storage &&);
        ~changed_mime_types_storage();

        void add_association(mime_type type, application_id desktop_id);
        bool set_associations(mime_type type, std::vector<application_id> desktop_ids);
        void remove_association(mime_type type, application_id desktop_id);
        bool remove_associations(mime_type type, std::vector<application_id> desktop_ids);
        void clear_association(const mime_type &type, const application_id &desktop_id);
        association_type get_association(const mime_type &type, const application_id &desktop_id) const;
        const std::vector<application_id> *get_added_associations(const mime_type &type) const noexcept;
        const std::vector<application_id> *get_removed_associations(const mime_type &type) const noexcept;
        explicit operator bool() const noexcept;
    };

    class API_PUBLIC mimeinfo_cache_storage : association_storage {
    public:
        using association_storage::get_associations;
        using association_storage::set_associations;
    };

    struct mimeapps_list_data {
        default_applications_storage default_apps;
        changed_mime_types_storage changed_mime_types;
    };

    class mimeapps_list_cache;

    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::filesystem::path path);
    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::filesystem::path path, bool mime_type_change_allowed);
    mimeapps_list_data API_PUBLIC parse_mimeapps_list(std::istream &is, bool mime_type_change_allowed);

    mimeinfo_cache_storage API_PUBLIC parse_mimeinfo_cache(std::filesystem::path path);
    mimeinfo_cache_storage API_PUBLIC parse_mimeinfo_cache(std::istream &is);

    std::vector<application_id> API_PUBLIC get_available_applications_for_mime_type(const mime_type &type,
        mimeapps_list_cache &cache);
    std::vector<application_id> API_PUBLIC get_available_applications_for_mime_type(const mime_type &type);

    std::optional<desktop_entry_spec::application_entry>
        API_PUBLIC get_default_app_for_mime_type(mime_type mime_type, mimeapps_list_cache &cache);
    std::optional<desktop_entry_spec::application_entry> API_PUBLIC get_default_app_for_mime_type(mime_type mime_type);

    std::unique_ptr<mimeapps_list_cache> make_cache();
} // namespace xdg::mime_apps

#endif
