#include <Native/MainWindow.h>
#include <Native/EditorLayout.h>
#include <Native/EditorPalette.h>
#include <Native/ScopedWidgetMount.h>
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
void validate_scene(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, ii::gui::Size size,
                    const char* state) {
    auto frame = take(tree->drawing_snapshot(size));
    if (ii::gui::valid_canvas(frame)) return;
    std::cerr << "INVALID_SCENE state=" << state << " size=" << size.width << 'x' << size.height
              << " overlays=" << frame.overlays.size() << " panels=" << frame.panels.size()
              << " texts=" << frame.texts.size() << " images=" << frame.images.size() << '\n';
    auto isolated = frame;
    isolated.overlays.clear();
    if (!ii::gui::valid_canvas(isolated)) std::cerr << "INVALID_SCENE base\n";
    isolated.panels.clear(); isolated.texts.clear(); isolated.images.clear();
    for (std::size_t i = 0; i < frame.overlays.size(); ++i) {
        isolated.overlays = {frame.overlays[i]};
        if (!ii::gui::valid_canvas(isolated))
            std::cerr << "INVALID_SCENE overlay=" << i << " panels=" << frame.overlays[i].panels.size()
                      << " texts=" << frame.overlays[i].texts.size()
                      << " images=" << frame.overlays[i].images.size() << '\n';
    }
    throw std::runtime_error("Main window produced an invalid native draw list");
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix,
                       std::string_view parent_suffix = {}) {
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& node) {
        return node.id.ends_with(suffix) && (parent_suffix.empty() || node.parent_id.ends_with(parent_suffix));
    });
    if (found == nodes.end()) throw std::runtime_error("Main window control did not mount: " + std::string(suffix));
    return take(tree->find(found->id));
}
void validate_window_background(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, ii::gui::Size size) {
    const auto frame = take(tree->drawing_snapshot(size));
    const auto bounds = take(widget(tree, "/application-window").snapshot()).bounds;
    const auto color = congregation::native::editor::window_background();
    const auto linear = [](double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-4; };
    require(near(bounds.width, size.width) && near(bounds.height, size.height), "Window material did not fill its host");
    const auto flat = std::ranges::find_if(frame.overlays, [&](const auto& layer) {
        return !layer.backdrop && !layer.shadow && std::ranges::any_of(layer.panels, [&](const auto& panel) {
            const auto match = [&](const auto& value) {
                return near(value.red, linear(color.red)) && near(value.green, linear(color.green)) &&
                       near(value.blue, linear(color.blue)) && near(value.alpha, 1);
            };
            return panel.bounds == bounds && near(panel.radius, 0) && near(panel.stroke_width, 0) &&
                   match(panel.top) && match(panel.bottom);
        });
    });
    require(flat != frame.overlays.end(), "Window background was not a flat opaque application color");
}
}

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("Expected the Figma resource directory");
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {1999, 1080}));
    congregation::native::MainWindow window;
    window.set_assets(argv[1]);
    congregation::ui::EditorBackend* model = nullptr;
    congregation::native::MainWindow::Scope* view_scope = nullptr;
    std::string selected;
    std::string command;
    auto action = congregation::ui::Action::quit;
    good(host.set_content([&](auto& builder) {
        return window.build_content(builder, {}, [&](auto& scope) {
            view_scope = &scope;
            model = &scope.backend_0;
            model->perform = [&](auto value) { action = value; };
            model->command = [&](std::string value) { command = std::move(value); };
            model->tool_selected = [&](std::string value) {
                selected = std::move(value);
                model->selected_tool.set(selected);
            };
        });
    }));
    require(model != nullptr, "Main window did not configure its shared backend");
    const auto workspace_host = take(widget(tree, "/workspace-host").snapshot());
    const auto workspace_node = take(widget(tree, "/workspace").snapshot());
    require(workspace_node.parent_id == workspace_host.id + "/@native-content",
            "The workspace is not mounted inside the generated LVUI NativeView");
    const auto toolbar = take(widget(tree, "/toolbar").snapshot());
    require(toolbar.parent_id != workspace_node.id,
            "The main toolbar must be owned by the generated LVUI view");
    const auto tool_rail = take(widget(tree, "/tool-rail").snapshot());
    require(tool_rail.parent_id == take(widget(tree, "/workspace-body").snapshot()).id,
            "The tool rail must be owned by the generated LVUI view");
    const auto status_bar = take(widget(tree, "/status-bar").snapshot());
    require(status_bar.parent_id == take(widget(tree, "/workspace-screen").snapshot()).id,
            "The status bar must be owned by the generated LVUI view");
    const auto prefix = workspace_host.id.substr(0, workspace_host.id.size() - std::string("workspace-host").size());
    const auto lookup = [&](std::string_view id) {
        return tree->find(id.starts_with(prefix) ? std::string(id) : prefix + std::string(id));
    };
    int snapshot_lookups = 0;
    const auto snapshots = [&] { ++snapshot_lookups; return tree->snapshots(); };
    for (const char* id : {"canvas", "canvas-paint-layers", "shape", "brush-color", "layers", "tool-scroll"})
        require(bool(congregation::native::find_scoped_widget(lookup, snapshots,
                    "workspace-host/@native-content", id)), "Application lookup lost a workspace widget");
    require(snapshot_lookups == 0, "Ordinary input lookup scanned the complete widget tree");
    const auto missing = [](std::string_view) -> ii::core::Result<ii::gui::Widget> {
        return ii::core::fail(ii::core::ErrorCode::not_found, "Missing test widget");
    };
    const auto duplicate_names = []() -> ii::core::Result<std::vector<ii::gui::WidgetSnapshot>> {
        std::vector<ii::gui::WidgetSnapshot> nodes(2);
        nodes[0].id = "first/repeated"; nodes[1].id = "second/repeated";
        return nodes;
    };
    auto ambiguous = congregation::native::find_scoped_widget(missing, duplicate_names, "missing", "repeated");
    require(!ambiguous && ambiguous.error().code == ii::core::ErrorCode::invalid_argument,
            "Ambiguous nested widget ids did not fail explicitly");
    validate_scene(tree, {1999, 1080}, "activation");
    validate_window_background(tree, {1999, 1080});
    require(take(widget(tree, "/application-window").snapshot()).visible, "Window material was not mounted");
    require(take(widget(tree, "/activation").snapshot()).visible &&
                !take(widget(tree, "/tool-rail").snapshot()).visible,
            "Unlicensed window did not display its activation view");
    good(widget(tree, "/activate").activate());
    require(action == congregation::ui::Action::restore_license, "Activation action did not reach the backend");
    model->licensed.set(true);
    model->needs_license.set(false);
    validate_scene(tree, {1999, 1080}, "workspace");
    validate_window_background(tree, {1999, 1080});
    good(host.set_extent({1200, 720}));
    validate_scene(tree, {1200, 720}, "workspace-minimum");
    validate_window_background(tree, {1200, 720});
    good(host.set_extent({1999, 1080}));
    validate_scene(tree, {1999, 1080}, "workspace-restored");
    require(!take(widget(tree, "/activation").snapshot()).visible, "Licensed editor still shows activation");
    require(take(widget(tree, "/tool-rail").snapshot()).visible &&
                take(widget(tree, "/timeline").snapshot()).visible,
            "The main window did not mount its workspace and timeline");
    const auto status_bounds = take(widget(tree, "/status-bar").snapshot()).bounds;
    require(status_bounds.x == 0 && status_bounds.width == 1999 && status_bounds.height == 20,
            "Status bar no longer spans the full workspace width");
    const auto canvas_bounds = take(widget(tree, "/canvas").snapshot()).bounds;
    const auto timeline_bounds = take(widget(tree, "/timeline").snapshot()).bounds;
    require(canvas_bounds.x == 82 && canvas_bounds.y == 46 && canvas_bounds.width == 1565 &&
                canvas_bounds.height == 692 && timeline_bounds.x == 54 && timeline_bounds.width == 1593 &&
                timeline_bounds.height == 296,
            "Declarative workspace changed canvas or timeline geometry");
    model->timeline->scrub(.5);
    require(take(widget(tree, "/timeline-status").snapshot()).text == "00:00.500 / 00:02.000",
            "Declarative status bar did not follow the timeline");
    model->timeline->scrub(1);
    require(take(widget(tree, "/tool-rail").snapshot()).bounds.width == 54 &&
                take(widget(tree, "/eraser").snapshot()).bounds.width == 22,
            "Declarative tool rail lost its shared button geometry");
    for (const auto& [id, tool] : std::initializer_list<std::pair<const char*, const char*>>{
             {"/select", "move"}, {"/tool-rect-marquee", "rect-marquee"},
             {"/tool-ellipse-marquee", "ellipse-marquee"}, {"/tool-lasso", "lasso"},
             {"/tool-polygon-lasso", "polygon-lasso"}, {"/tool-magnetic-lasso", "magnetic-lasso"},
             {"/tool-magic-wand", "magic-wand"}, {"/tool-crop", "crop"},
             {"/tool-perspective-crop", "perspective-crop"}, {"/tool-color-picker", "color-picker"},
             {"/brush", "brush"}, {"/tool-pencil", "pencil"}, {"/tool-airbrush", "airbrush"},
             {"/tool-mixer-brush", "mixer-brush"}, {"/tool-dry-brush", "dry-brush"},
             {"/tool-bristle-brush", "bristle-brush"}, {"/eraser", "eraser"},
             {"/tool-gradient", "gradient"}, {"/fill", "paint-bucket"}, {"/tool-smudge", "smudge"},
             {"/tool-clone-stamp", "clone-stamp"}, {"/tool-pattern-stamp", "pattern-stamp"},
             {"/tool-healing-brush", "healing-brush"}, {"/tool-spot-heal", "spot-heal"},
             {"/tool-blur", "blur"}, {"/tool-sharpen", "sharpen"}, {"/tool-dodge", "dodge"},
             {"/tool-burn", "burn"}, {"/tool-sponge", "sponge"}, {"/tool-liquify", "liquify"},
             {"/tool-warp", "warp"}, {"/tool-pen", "pen"}, {"/tool-curvature-pen", "curvature-pen"},
             {"/tool-direct-select", "direct-select"}, {"/tool-add-anchor", "add-anchor"},
             {"/tool-delete-anchor", "delete-anchor"}, {"/shape", "rectangle"},
             {"/tool-ellipse", "ellipse"}, {"/tool-polygon", "polygon"}, {"/text", "area-text"},
             {"/tool-path-text", "path-text"}, {"/pan", "hand"}, {"/tool-rotate-view", "rotate-view"},
             {"/tool-quick-mask", "quick-mask"}, {"/zoom-tool", "imagezoomIn"}, {"/tool-rulers", "rulers"}}) {
        selected.clear();
        good(widget(tree, id).activate());
        require(selected == tool && model->selected_tool.get() == tool,
                "Declarative tool rail changed a tool command");
    }
    const auto tool_background = [&](std::string_view id) {
        const auto bounds = take(widget(tree, id).snapshot()).bounds;
        const auto frame = take(tree->drawing_snapshot({1999, 1080}));
        const auto contains = [&](const auto& panels) {
            return std::ranges::any_of(panels, [&](const auto& panel) {
                return panel.bounds == bounds && panel.top.alpha > 0;
            });
        };
        return contains(frame.panels) || std::ranges::any_of(frame.overlays, [&](const auto& layer) {
            return contains(layer.panels);
        });
    };
    good(widget(tree, "/eraser").activate());
    require(selected == "eraser" && tool_background("/eraser"),
            "Declarative tool rail did not select or highlight the eraser");
    good(widget(tree, "/brush").activate());
    require(selected == "brush" && !tool_background("/eraser") && tool_background("/brush"),
            "Declarative tool rail did not restore the previous tool state");
    model->quick_mask_mode.set(true);
    require(tool_background("/tool-quick-mask"), "Quick mask did not show its independent active state");
    model->quick_mask_mode.set(false);
    require(!tool_background("/tool-quick-mask"), "Quick mask active state did not clear");
    const auto before_foreground = action;
    good(widget(tree, "/foreground-color").activate());
    require(action == congregation::ui::Action::open_color, "Declarative tool rail lost its foreground color action");
    action = before_foreground;
    for (const auto& name : {"align-left", "align-center", "align-right", "align-top", "align-middle",
                             "align-bottom", "distribute-horizontal", "distribute-vertical", "flip-horizontal",
                             "flip-vertical", "rotate-left", "rotate-right", "perspective-grid", "symmetry-paint",
                             "grid", "rulers", "snap"}) {
        command.clear();
        good(widget(tree, std::string{"/command-"} + name).activate());
        require(command == name, "Declarative toolbar changed a command");
    }
    good(widget(tree, "/document-color-mode").activate());
    require(model->option_tab.get() == 2, "Declarative toolbar lost its document panel action");
    model->option_tab.set(0);
    const auto prior_action = action;
    good(widget(tree, "/brush-color").activate());
    require(action == congregation::ui::Action::open_color, "Declarative toolbar lost its color action");
    action = prior_action;
    model->chrome_visible.set(false);
    require(!take(widget(tree, "/toolbar").snapshot()).visible,
            "Declarative toolbar did not follow the chrome visibility state");
    model->chrome_visible.set(true);
    require(take(widget(tree, "/toolbar").snapshot()).visible,
            "Declarative toolbar did not restore the chrome visibility state");
    auto active_source = view_scope->backend_1.source.get();
    auto rejected_source = std::make_shared<congregation::native::NativeSurfaceSource>(
        [](ii::gui::WidgetBuilder&) -> ii::core::Result<void> {
            return ii::core::fail(ii::core::ErrorCode::failed, "Deliberate candidate failure");
        });
    view_scope->backend_1.source.set(rejected_source);
    const auto failures = take(view_scope->binding_failures());
    require(!failures.empty() && take(widget(tree, "/tool-rail").snapshot()).visible,
            "A failed native view candidate replaced the active workspace");
    view_scope->backend_1.source.set(active_source);
    auto brush_popup=widget(tree,"/brush-popup");
    good(brush_popup.open_at(500,150));
    auto pressure=widget(tree,"/pressure-curve");
    congregation::native::editor::pressure_curve(pressure,0,.5,1);
    const auto ink=congregation::native::editor::linear(congregation::native::Color::purple);
    const auto curve_frame=take(tree->drawing_snapshot({1999,1080}));
    const auto curve_ink=[&](const auto& panels){return std::ranges::count_if(panels,[&](const auto& panel){
        return panel.bounds.width==congregation::native::Metric::stroke2&&
               panel.bounds.height==congregation::native::Metric::stroke2&&panel.stroke_width==0&&
               std::abs(panel.top.red-ink.red)<.0001&&std::abs(panel.top.green-ink.green)<.0001&&
               std::abs(panel.top.blue-ink.blue)<.0001;
    });};
    std::size_t curve_points=curve_ink(curve_frame.panels);
    for(const auto& layer:curve_frame.overlays)curve_points+=curve_ink(layer.panels);
    require(curve_points>100,"Pressure curve did not use the shared LVUI stroke width");
    good(brush_popup.close_popup());
    const std::string text = "A long text value for the canvas that wraps across multiple visual lines in the text tool.";
    model->text_value.set(text);
    auto text_popup = widget(tree, "/text-popup");
    good(text_popup.open_at(500, 150));
    validate_scene(tree, {1999, 1080}, "text-popup");
    auto text_field = widget(tree, "/text-value");
    good(text_field.focus());
    const auto focused_text = take(text_field.snapshot());
    require(focused_text.focused && focused_text.text_input,
            "Focused LVUI text editor did not expose its native text input capability");
    good(tree->input({.kind = ii::gui::WidgetInputKind::key_down, .key = ii::gui::WidgetKey::select_all}));
    good(tree->input({.kind = ii::gui::WidgetInputKind::key_down, .key = ii::gui::WidgetKey::left}));
    good(tree->input({.kind = ii::gui::WidgetInputKind::key_down, .key = ii::gui::WidgetKey::end}));
    const auto visual_end = take(text_field.snapshot()).text_selection.location;
    require(visual_end > 0 && visual_end < text.size(), "Text tool did not wrap at the end of its first visual line");
    good(tree->input({.kind = ii::gui::WidgetInputKind::text, .text = "X"}));
    auto edited = text; edited.insert(visual_end, "X");
    require(model->text_value.get() == edited, "Wrapped text tool edit did not reach the editor backend");
    good(text_popup.close_popup());
    auto shape_menu = widget(tree, "/shape-menu");
    good(shape_menu.open_at(500, 150));
    validate_scene(tree, {1999, 1080}, "shape-menu");
    auto ellipse = widget(tree, "/ellipse", "/shape-menu/content");
    const auto ellipse_bounds = take(ellipse.snapshot()).bounds;
    const auto ellipse_x = ellipse_bounds.x + ellipse_bounds.width / 2;
    const auto ellipse_y = ellipse_bounds.y + ellipse_bounds.height / 2;
    good(tree->input({.kind = ii::gui::WidgetInputKind::pointer_move, .x = ellipse_x, .y = ellipse_y}));
    require(take(ellipse.snapshot()).focused && take(shape_menu.snapshot()).opened &&
                action == congregation::ui::Action::restore_license,
            "Hovering a shape did not focus it or invoked its action");
    good(tree->input({.kind = ii::gui::WidgetInputKind::pointer_down, .x = ellipse_x, .y = ellipse_y, .buttons = 1}));
    good(tree->input({.kind = ii::gui::WidgetInputKind::pointer_up, .x = ellipse_x, .y = ellipse_y}));
    require(action == congregation::ui::Action::ellipse, "A click after hovering a shape did not dispatch its action");
    require(!take(shape_menu.snapshot()).opened, "A click after hovering a shape did not close its menu");
    good(widget(tree, "/eraser").activate());
    require(selected == "eraser" && model->selected_tool.get() == "eraser", "Workspace lost the main backend");
    good(model->set_current_color({.2, .4, .6, .5}));
    require(take(widget(tree, "/color-hue").snapshot()).accessibility_description == "#33669980",
            "Main window did not share color with its workspace");
    good(brush_popup.open_at(500,150));
    const auto after_color_frame = take(tree->drawing_snapshot({1999,1080}));
    std::size_t after_color_curve_points = curve_ink(after_color_frame.panels);
    for (const auto& layer : after_color_frame.overlays) after_color_curve_points += curve_ink(layer.panels);
    require(after_color_curve_points > 100,
            "Color changes removed the pressure curve from the brush popup");
    good(brush_popup.close_popup());
    auto retired_workspace = widget(tree, "/workspace");
    good(host.clear_content());
    require(!retired_workspace.snapshot(), "Retired workspace remained live");
    model = nullptr;
    good(host.set_content([&](auto& builder) {
        return window.build_content(builder, {}, [&](auto& scope) {
            model = &scope.backend_0;
            model->licensed.set(true);
            model->needs_license.set(false);
        });
    }));
    require(model != nullptr && take(widget(tree, "/workspace").snapshot()).visible,
            "A new LVUI workspace generation did not mount after retirement");
    good(host.clear_content());
    tree->close();
}
