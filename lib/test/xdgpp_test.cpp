#include <basedir.h>
#include <desktop-entry.h>
#include <mime-apps.h>

#include <iostream>

using namespace xdg::desktop_entry_spec;
using namespace xdg::mime_apps;
using namespace std::string_view_literals;

void desktop_entry_stuff() {
    auto all = xdg::desktop_entry_spec::get_all_application_entries();
    if (all.empty()) {
        return;
    }
    for (const auto &it : all) {
        [[maybe_unused]] auto res = it.should_show();
        auto actions              = it.get_actions();
        if (!actions.empty()) {
            auto &action = actions.at(0);
            std::cout << action.get_name().get() << '\n';
            action.launch([&](const xdg::desktop_entry_spec::launch_parameters &params) {
                std::cout
                    << "Cmdline: "
                    << params.command_list
                    << "\nWorking Directory: "
                    << params.working_directory
                    << "\nStart in Terminal: "
                    << std::boolalpha
                    << params.terminal
                    << '\n';
            });
        }
        std::cout << res << '\n';
    }

    xdg::desktop_entry_spec::types::application_id firefox_id {"firefox.desktop"sv};
    auto result = xdg::desktop_entry_spec::search_application_entry(firefox_id);
    assert(result);
    auto &entry = result->entry;
    assert(entry.get_id() == firefox_id);

    [[maybe_unused]] auto type = entry.get_type();
    [[maybe_unused]] auto name = entry.get_name();

    [[maybe_unused]] auto current_locale        = name.get();
    [[maybe_unused]] auto name_de               = name.get("de");
    [[maybe_unused]] auto name_de_DE            = name.get("de_DE");
    [[maybe_unused]] auto name_de_DE_mod        = name.get("de_DE@Latn");
    [[maybe_unused]] auto name_de_DE_mod_failed = name.get("de_DE@nope");
    [[maybe_unused]] auto name_de_ZU            = name.get("de_ZU");
    [[maybe_unused]] auto name_de_AU            = name.get("de_AU");
    [[maybe_unused]] auto name_de_AU_mod        = name.get("de_AU@Latn");
    [[maybe_unused]] auto name_de_mod           = name.get("de@Latn");
    [[maybe_unused]] auto name_en               = name.get("en");

    [[maybe_unused]] auto generic_name = entry.get_generic_name();
    [[maybe_unused]] auto exec         = entry.get_exec();
    [[maybe_unused]] auto try_exec     = entry.get_try_exec();

    entry.launch([&](const xdg::desktop_entry_spec::launch_parameters &params) {
        std::cout
            << "Cmdline: "
            << params.command_list
            << "\nWorking Directory: "
            << params.working_directory
            << "\nStart in Terminal: "
            << std::boolalpha
            << params.terminal
            << '\n';
    });
}

void mime_type_stuff() {
    auto file = xdg::basedir::get_config_home() / "mimeapps.list";
    if (!exists(file)) {
        return;
    }
    auto [default_storage, changed_storage] = parse_mimeapps_list(file);

    mime_type png_type       = "image/png;i_dont_care"sv;
    auto default_app_for_png = xdg::mime_apps::get_default_app_for_mime_type(png_type);
    if (default_app_for_png) {
        std::cout
            << "Default application for "
            << png_type
            << " is:\n"
            << default_app_for_png->get_name().get()
            << '\n';
    } else {
        std::cout << "No default application for " << png_type << '\\';
    }

    return;
}

int main(int, char **) {
    desktop_entry_stuff();
    mime_type_stuff();

    return 0;
}
