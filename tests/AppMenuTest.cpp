#include <Application/AppMenu.h>
#include <array>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using congregation::ui::Action;

// Describe a separator, a command, or a submenu with one current command.
struct ExpectedRow {
    std::string_view title;
    std::string_view key;
    bool alt = false;
    bool shift = false;
    bool meta = true;
    bool control = false;
    std::optional<Action> action;
    std::string_view child_title;
};

// Validate placement and shortcut hints, then exercise only implemented commands.
void check_menu(const ii::gui::AppMenuItem& menu, std::span<const ExpectedRow> expected, Action& invoked)
{
    const auto& rows = menu.children;
    if (rows.size() != expected.size())
        throw std::runtime_error(menu.title + " menu has the wrong number of rows");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& spec = expected[i];
        const auto& row = rows[i];
        if (row.separator != spec.title.empty() || row.title != spec.title)
            throw std::runtime_error(menu.title + " menu has an unexpected row at " + std::to_string(i));
        if (row.separator) continue;
        const auto* command = &row;
        if (!spec.child_title.empty()) {
            if (!row.enabled || row.triggered || row.shortcut || row.children.size() != 1 ||
                row.children.front().title != spec.child_title)
                throw std::runtime_error(row.title + " has an incorrect submenu");
            command = &row.children.front();
        }
        if (bool(command->shortcut) != !spec.key.empty() ||
            (command->shortcut && (command->shortcut->key != spec.key || command->shortcut->meta != spec.meta ||
                                   command->shortcut->control != spec.control || command->shortcut->alt != spec.alt ||
                                   command->shortcut->shift != spec.shift)))
            throw std::runtime_error(command->title + " has an incorrect shortcut");
        if (!command->children.empty() || command->enabled != spec.action.has_value() ||
            bool(command->triggered) != spec.action.has_value())
            throw std::runtime_error(command->title + " has an incorrect command state");
        if (spec.action) {
            invoked = *spec.action == Action::quit ? Action::open : Action::quit;
            command->triggered();
            if (invoked != *spec.action)
                throw std::runtime_error(command->title + " dispatches the wrong action");
        }
    }
}

constexpr std::string_view unavailable = "No commands available";

constexpr ExpectedRow file_rows[]{
    {.title="New document…", .key="n", .action=Action::new_document},
    {.title="Add canvas…", .action=Action::add_canvas},
    {.title="Open…", .key="o", .action=Action::open},
    {.title="Open As…", .key="o", .alt=true, .shift=true},
    {.title="Open Recent", .child_title=unavailable}, {},
    {.title="Close", .key="w"},
    {.title="Close All", .key="w", .alt=true},
    {.title="Close Others", .key="p", .alt=true},
    {.title="Save", .key="s", .action=Action::save},
    {.title="Save As…", .key="s", .shift=true, .action=Action::save_as},
    {.title="Save a Copy…", .key="s", .alt=true},
    {.title="Revert", .key="\uF70F", .meta=false}, {},
    {.title="Export", .action=Action::export_document, .child_title="Export…"}, {},
    {.title="Place Embedded…", .action=Action::image}, {"Place Linked…"}, {},
    {.title="Automate", .child_title=unavailable},
    {.title="Scripts", .child_title=unavailable},
    {.title="Import", .child_title=unavailable}, {},
    {.title="File Info…", .key="i", .alt=true, .shift=true}, {},
    {.title="Print…", .key="p"},
    {.title="Print One Copy", .key="p", .alt=true, .shift=true},
#if !defined(__APPLE__)
    {}, {.title="Quit Congregation", .key="q", .action=Action::quit},
#endif
};

constexpr ExpectedRow edit_rows[]{
    {.title="Undo", .key="z", .action=Action::undo},
    {.title="Redo", .key="z", .shift=true, .action=Action::redo},
    {.title="Toggle Last State", .key="z", .alt=true}, {},
    {.title="Fade…", .key="f", .shift=true}, {},
    {.title="Cut", .key="x"},
    {.title="Copy", .action=Action::copy},
    {.title="Copy Merged", .action=Action::copy},
    {.title="Paste", .action=Action::paste},
    {.title="Paste Special", .child_title=unavailable}, {"Clear"}, {},
    {.title="Search", .key="f"}, {"Check Spelling…"}, {"Find and Replace Text…"}, {},
    {.title="Fill…", .key="\uF708", .shift=true, .meta=false}, {"Stroke…"}, {"Content-Aware Fill…"}, {},
    {.title="Content-Aware Scale", .key="c", .alt=true, .shift=true},
    {"Puppet Warp"}, {"Perspective Warp"}, {.title="Free Transform", .key="t"},
    {.title="Transform", .child_title=unavailable},
    {"Auto-Align Layers…"}, {"Auto-Blend Layers…"}, {"Sky Replacement…"}, {},
    {"Define Brush Preset…"}, {"Define Pattern…"}, {"Define Custom Shape…"}, {},
    {.title="Purge", .child_title=unavailable}, {},
    {.title="Presets", .child_title=unavailable}, {},
    {.title="Color Settings…", .key="k", .shift=true}, {"Assign Profile…"}, {"Convert to Profile…"}, {},
    {.title="Keyboard Shortcuts…", .key="k", .alt=true, .shift=true},
    {.title="Menus…", .key="m", .alt=true, .shift=true}, {"Toolbar…"},
    {.title="Preferences", .key=",", .action=Action::preferences, .child_title="Preferences…"},
};

constexpr ExpectedRow select_rows[]{
    {.title="All", .key="a"},
    {.title="Deselect", .key="d", .action=Action::clear_selection},
    {.title="Reselect", .key="d", .shift=true},
    {.title="Inverse", .key="i", .shift=true, .action=Action::invert_selection}, {},
    {.title="All Layers", .key="a", .alt=true}, {"Deselect Layers"},
    {.title="Find Layers", .key="f", .alt=true, .shift=true}, {"Isolate Layers"}, {},
    {"Color Range…"}, {"Focus Area…"}, {"Subject"}, {"Sky"}, {},
    {.title="Select and Mask…", .key="r", .alt=true},
    {.title="Modify", .child_title=unavailable}, {},
    {"Grow"}, {"Similar"}, {}, {"Transform Selection"}, {},
    {.title="Edit in Quick Mask Mode", .action=Action::toggle_quick_mask}, {},
    {"Load Selection…"}, {"Save Selection…"}, {}, {"New 3D Extrusion"},
};

constexpr ExpectedRow view_rows[]{
    {.title="Proof Setup", .child_title=unavailable},
    {.title="Proof Colors", .key="y"},
    {.title="Gamut Warning", .key="y", .shift=true},
    {.title="Pixel Aspect Ratio", .child_title=unavailable},
    {"Pixel Aspect Ratio Correction"}, {"32-bit Preview Options…"}, {},
    {.title="Zoom In", .key="+", .action=Action::zoom_in},
    {.title="Zoom Out", .key="-", .action=Action::zoom_out},
    {.title="Fit on Screen", .key="0", .action=Action::fit},
    {"Fit Layer on Screen"}, {"Fit Artboard on Screen"}, {"100%", "1"}, {"200%"},
    {"Print Size"}, {"Actual Size"}, {"Flip Horizontal"}, {"Pattern Preview"}, {},
    {.title="Screen Mode", .action=Action::presentation, .child_title="Presentation"}, {},
    {.title="Extras", .key="h", .meta=false, .control=true},
    {.title="Show", .child_title=unavailable}, {},
    {.title="Rulers", .key="r", .action=Action::toggle_rulers}, {},
    {.title="Snap", .key=";", .shift=true}, {.title="Snap To", .child_title=unavailable}, {},
    {.title="Guides", .child_title=unavailable}, {},
    {"Lock Slices"}, {"Clear Slices"},
};

constexpr ExpectedRow layer_rows[]{
    {.title="New", .action=Action::add_layer, .child_title="New Layer"},
    {"Copy CSS"}, {"Copy SVG"}, {.title="Duplicate Layer", .action=Action::duplicate_layer},
    {.title="Delete", .action=Action::delete_layer, .child_title="Delete Layer"}, {},
    {.title="Quick Export as JPG", .key="'", .shift=true},
    {.title="Export As…", .key="'", .alt=true, .shift=true}, {},
    {"Rename Layer…"}, {.title="Layer Style", .child_title=unavailable}, {},
    {.title="New Fill Layer", .child_title=unavailable},
    {.title="New Adjustment Layer", .child_title=unavailable}, {"Layer Content Options…"}, {},
    {.title="Layer Mask", .child_title=unavailable}, {.title="Vector Mask", .child_title=unavailable},
    {.title="Create Clipping Mask", .key="g", .alt=true}, {"Mask All Objects"}, {},
    {.title="Video Layers", .child_title=unavailable}, {.title="Rasterize", .child_title=unavailable}, {},
    {"New Layer-Based Slice"}, {},
    {.title="Group Layers", .key="g"}, {.title="Ungroup Layers", .key="g", .shift=true},
    {.title="Toggle Layer Visibility", .action=Action::toggle_layer_visibility}, {},
    {.title="Arrange", .child_title=unavailable}, {.title="Combine Shapes", .child_title=unavailable}, {},
    {.title="Align", .child_title=unavailable}, {.title="Distribute", .child_title=unavailable}, {},
    {.title="Lock Layers…", .key="/"}, {},
    {"Link Layers"}, {"Select Linked Layers"}, {},
    {.title="Merge Layers", .key="e"}, {.title="Merge Visible", .key="e", .shift=true}, {"Flatten Image"}, {},
    {.title="Matting", .child_title=unavailable},
};

constexpr ExpectedRow sequence_rows[]{
    {"Sequence Settings…"}, {},
    {.title="Render Effects In to Out", .key="\r", .meta=false},
    {"Render In to Out"}, {"Render Selection"}, {"Render Audio"},
    {"Delete Render Files"}, {"Delete Render Files In to Out"}, {},
    {.title="Match Frame", .key="f", .meta=false},
    {.title="Reverse Match Frame", .key="r", .shift=true, .meta=false}, {},
    {.title="Add Edit", .key="k"}, {.title="Add Edit to All Tracks", .key="k", .shift=true},
    {.title="Trim Edit", .key="t", .shift=true, .meta=false},
    {.title="Extend Selected Edit to Playhead", .key="e", .meta=false}, {},
    {.title="Apply Video Transition", .key="d"}, {.title="Apply Audio Transition", .key="d", .shift=true},
    {.title="Apply Default Transitions to Selection", .key="d", .shift=true, .meta=false}, {},
    {.title="Lift", .key=";", .meta=false}, {"Extract"}, {},
    {.title="Zoom In", .key="=", .meta=false}, {.title="Zoom Out", .key="-", .meta=false}, {},
    {"Close Gap"}, {.title="Go to Gap", .child_title=unavailable}, {},
    {.title="Snap in Timeline", .key="s", .meta=false}, {"Linked Selection"},
    {"Selection Follows Playhead"}, {"Show Through Edits"}, {},
    {"Normalize Mix Track…"}, {}, {.title="Make Subsequence", .key="u", .shift=true, .meta=false}, {},
    {"Auto Reframe Sequence…"},
};

constexpr ExpectedRow image_rows[]{
    {.title="Mode", .child_title=unavailable}, {},
    {.title="Adjustments", .child_title=unavailable}, {},
    {.title="Auto Tone", .key="l", .shift=true},
    {.title="Auto Contrast", .key="l", .alt=true, .shift=true},
    {.title="Auto Color", .key="b", .shift=true}, {},
    {.title="Image Size…", .key="i", .alt=true},
    {.title="Canvas Size…", .key="c", .alt=true},
    {.title="Image Rotation", .child_title=unavailable}, {"Crop"}, {"Trim…"}, {"Reveal All"}, {},
    {"Duplicate…"}, {"Apply Image…"}, {"Calculations…"}, {},
    {.title="Variables", .child_title=unavailable}, {"Apply Data Set…"}, {},
    {"Trap…"}, {}, {.title="Analysis", .child_title=unavailable}, {},
    {.title="Place image…", .action=Action::image}, {.title="Draw line", .action=Action::line},
};

constexpr ExpectedRow filter_rows[]{
    {.title="Gaussian Blur", .key="f", .alt=true}, {},
    {"Generative filter"}, {},
    {"Filter Gallery…"},
    {.title="Adaptive Wide Angle…", .key="a", .alt=true, .shift=true},
    {"Lens Correction…"}, {.title="Liquify…", .key="x", .shift=true},
    {.title="Vanishing Point…", .key="v", .alt=true}, {},
    {.title="3D", .child_title=unavailable}, {.title="Noise", .child_title=unavailable},
    {.title="Render", .child_title=unavailable}, {.title="Video", .child_title=unavailable},
    {.title="Sharpen", .child_title=unavailable}, {.title="Stylize", .child_title=unavailable},
    {.title="Distort", .child_title=unavailable}, {.title="Pixelate", .child_title=unavailable},
    {.title="Blur", .child_title=unavailable}, {.title="Blur Gallery", .child_title=unavailable},
    {.title="Other", .child_title=unavailable},
};

constexpr ExpectedRow three_d_rows[]{
    {"New 3D Layer from File…"}, {"Merge 3D Layers"}, {"Export 3D Layer…"},
    {.title="Spherical Panorama", .child_title=unavailable}, {},
    {"Download Content…"}, {},
    {"New 3D Extrusion from Selected Layer"},
    {"New 3D Extrusion from Selected Path"},
    {"New 3D Extrusion from Current Selection"},
    {.title="New Mesh from Layer", .child_title=unavailable}, {},
    {"Group Objects"}, {"Group All Objects in Scene"},
    {"Move Objects to Ground Plane"}, {"Pack Objects on Ground Plane"}, {},
    {"New Tiled Painting from Layer"}, {"Generate UVs…"}, {"Paint Falloff…"},
    {.title="Paint System", .child_title=unavailable},
    {.title="Paint on Target Texture", .child_title=unavailable},
    {"Select Paintable Areas"},
    {.title="Create Painting Overlay", .child_title=unavailable}, {},
    {"Split Extrusion"}, {"Apply Cross Section to Scene"}, {"Unify Scene for 3D Printing"},
    {"Simplify Mesh…"}, {"Make Work Path from 3D Layer"}, {},
    {.title="Render 3D Layer", .key="r", .alt=true, .shift=true},
    {"Render Document to File…"}, {"Sketch with Current Brush"}, {},
    {"3D Print Settings…"}, {"3D Print…"},
};

} // namespace

int main()
{
    using congregation::ui::Action;
    Action invoked = Action::quit;
    const auto menu = congregation::native::application_menu([&](Action action) { invoked = action; });
    constexpr std::array<std::string_view, 11> expected{
        "File", "Edit", "Select", "View", "Layer", "Image", "Sequence", "Filter", "3D", "Window", "Help"};
    if (menu.size() != expected.size()) throw std::runtime_error("incorrect menu count");
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (menu[i].title != expected[i] || menu[i].children.empty())
            throw std::runtime_error("incorrect menu order or content");
    check_menu(menu[0], file_rows, invoked);
    check_menu(menu[1], edit_rows, invoked);
    check_menu(menu[2], select_rows, invoked);
    check_menu(menu[3], view_rows, invoked);
    check_menu(menu[4], layer_rows, invoked);
    check_menu(menu[5], image_rows, invoked);
    check_menu(menu[6], sequence_rows, invoked);
    check_menu(menu[7], filter_rows, invoked);
    check_menu(menu[8], three_d_rows, invoked);
    if (menu.back().children[0].enabled)
        throw std::runtime_error("Unavailable commands must be disabled");
}
