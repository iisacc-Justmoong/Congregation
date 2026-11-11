#pragma once

#include "EditorBackend.h"
#include <iiGUI/Desktop.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace congregation::native {

// The platform owns the macOS application menu; this list contains only
// application-provided menus, shared by native desktop hosts.
inline std::vector<ii::gui::AppMenuItem> application_menu(std::function<void(ui::Action)> dispatch)
{
    using ui::Action;
    auto item = [dispatch](std::string id, std::string title, Action action, std::string key = {}, bool shift = false) {
        ii::gui::AppMenuItem result{.id = std::move(id), .title = std::move(title)};
        if (!key.empty()) result.shortcut = ii::gui::AppShortcut{.key = std::move(key), .shift = shift};
        result.triggered = [dispatch, action] { if (dispatch) dispatch(action); };
        return result;
    };
    std::vector<ii::gui::AppMenuItem> menus{
        {.id="file", .title="File", .children={item("new", "New", Action::new_document, "n"),
            item("open", "Open…", Action::open, "o"), item("save", "Save", Action::save, "s"),
            item("save-as", "Save As…", Action::save_as), item("export", "Export…", Action::export_document)}},
        {.id="edit", .title="Edit", .children={item("undo", "Undo", Action::undo, "z"),
            item("redo", "Redo", Action::redo, "z", true), item("copy", "Copy canvas", Action::copy),
            item("paste", "Paste image", Action::paste), item("preferences", "Preferences…", Action::preferences, ",")}},
        {.id="selection", .title="Select", .children={item("deselect", "Deselect", Action::clear_selection, "d"),
            item("invert-selection", "Inverse", Action::invert_selection)}},
        {.id="view", .title="View", .children={item("fit", "Fit", Action::fit),
            item("zoom-in", "Zoom In", Action::zoom_in, "+"), item("zoom-out", "Zoom Out", Action::zoom_out, "-")}},
        {.id="layer", .title="Layer", .children={item("new-layer", "New Layer", Action::add_layer),
            item("duplicate-layer", "Duplicate Layer", Action::duplicate_layer), item("delete-layer", "Delete Layer", Action::delete_layer)}},
        {.id="image", .title="Image", .children={item("place-image", "Place image…", Action::image),
            item("line", "Draw line", Action::line)}},
        {.id="filter", .title="Filter", .children={{.id="filter-unavailable", .title="No filters available", .enabled=false}}},
        {.id="window", .title="Window", .children={item("presentation", "Presentation", Action::presentation)}},
        {.id="help", .title="Help", .children={{.id="help-unavailable", .title="Help unavailable", .enabled=false}}}
    };
#if !defined(__APPLE__)
    menus.front().children.push_back(item("quit", "Quit Congregation", Action::quit, "q"));
#endif
    return menus;
}
} // namespace congregation::native
