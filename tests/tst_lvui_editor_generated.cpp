#include "EditorViewModel.generated.h"
#include "CanvasSetupViewModel.generated.h"
#include "TimelineViewModel.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value)
{
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value)
{
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
ii::gui::Widget named(const std::shared_ptr<ii::gui::detail::WidgetState>& state,
                      std::string_view name)
{
    auto snapshots = take(state->snapshots());
    auto found = std::ranges::find_if(snapshots, [&](const auto& item) {
        return item.id == name || item.id.ends_with(std::string("/") + std::string(name)) ||
               item.accessibility_name == name;
    });
    if (found == snapshots.end()) throw std::runtime_error("Expected LVUI widget was not built");
    return take(state->find(found->id));
}
}

int main()
{
    try
    {
        using View = lvui_generated::EditorViewModel;
        auto layers = std::make_shared<congregation::ui::PresentationList>();
        auto members = std::make_shared<congregation::ui::PresentationList>();
        require(layers->replace({{.key = "layer", .label = "Artwork"}}).value(),
                "initial layer was not published");
        require(members->replace({{.key = "peer", .label = "Artist"}}).value(),
                "initial member was not published");
        auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
        auto host = take(state->content_host({}, "host", {320, 400}));
        std::shared_ptr<View::Scope> scope;
        std::string selected;
        std::string selected_layer;
        int brush_changes = 0;
        good(host.set_content([&](auto& builder) {
            return View::build(builder, {}, [&](auto& activation) {
                activation.backend_0.set_presentation_lists(layers, members);
                activation.backend_0.status.set("Ready to paint");
                activation.backend_0.brush_changed = [&] { ++brush_changes; };
                activation.backend_0.selected = [&](std::string key) { selected_layer = std::move(key); };
                activation.backend_0.member_selected = [&](std::string key) { selected = std::move(key); };
            }, &scope);
        }));
        (void)take(state->drawing_snapshot({320, 400}));
        auto row = named(state, "Artist");
        good(named(state, "Artwork").activate());
        require(selected_layer == "layer" && layers->revision.get() == 1,
                "LVUI layer activation changed the source or lost the key");
        require(take(named(state, "status").snapshot()).text == "Ready to paint",
                "LVUI did not read the active backend's state");
        good(row.activate());
        require(selected == "peer" && members->revision.get() == 1,
                "LVUI member activation changed the source or lost the key");
        require(members->replace({{.key = "peer", .label = "Renamed artist"}}).value(),
                "member update failed");
        auto graph = scope->native_lifetime.lock();
        require(graph && take(graph->failures()).empty() &&
                    take(row.action_options()).accessibility_name == "Renamed artist",
                "LVUI did not refresh the retained member row");
        scope->backend_0.status.set("Painting");
        require(take(named(state, "status").snapshot()).text == "Painting",
                "LVUI did not observe a backend value");
        auto slider = named(state, "brush-size");
        scope->backend_0.set_brush_size(128);
        require(take(slider.snapshot()).value == 128 && brush_changes == 1,
                "LVUI did not observe the brush setting");
        good(slider.focus());
        good(state->input({.kind = ii::gui::WidgetInputKind::key_down,
                           .key = ii::gui::WidgetKey::right}));
        require(brush_changes > 1 && scope->backend_0.brush_size.get() > 128,
                "LVUI slider input did not reach the brush command");
        good(scope->backend_0.set_current_color({.2, .3, .4, 1}));
        require(scope->backend_0.current_color.get() == ii::gui::WidgetColor{.2, .3, .4, 1} &&
                    take(graph->failures()).empty(), "LVUI color binding failed");
        std::weak_ptr<View::Scope> weak = scope;
        good(host.clear_content());
        scope.reset();
        graph.reset();
        require(weak.expired() && !row.snapshot(), "LVUI scope retained retired view-model state");
        state->close();

        using Setup = lvui_generated::CanvasSetupViewModel;
        auto setup_tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
        auto setup_host = take(setup_tree->content_host({}, "setup-host", {320, 400}));
        std::shared_ptr<Setup::Scope> setup_scope;
        good(setup_host.set_content([&](auto& builder) {
            return Setup::build(builder, {}, [](auto&) {}, &setup_scope);
        }));
        auto selection = named(setup_tree, "selection");
        auto results = named(setup_tree, "results");
        auto create = named(setup_tree, "create");
        setup_scope->backend_0.search("A4");
        require(take(selection.snapshot()).text == setup_scope->backend_0.selection_name() &&
                    take(results.snapshot()).text == "11",
                "LVUI did not observe the coherent canvas search state");
        setup_scope->backend_0.custom_size();
        setup_scope->backend_0.set_new_width("0");
        require(!take(create.snapshot()).enabled,
                "Invalid canvas dimensions did not disable the LVUI action");
        setup_scope->backend_0.quick_size(1920, 1080);
        require(take(create.snapshot()).enabled &&
                    take(named(setup_tree, "dimensions").snapshot()).text == "1920 × 1080 px",
                "LVUI canvas setup did not recover after valid dimensions");
        good(setup_host.clear_content());
        setup_scope.reset();
        setup_tree->close();

        using Timeline = lvui_generated::TimelineViewModel;
        auto timeline_tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
        auto timeline_host = take(timeline_tree->content_host({}, "timeline-host", {320, 200}));
        std::shared_ptr<Timeline::Scope> timeline_scope;
        good(timeline_host.set_content([&](auto& builder) {
            return Timeline::build(builder, {}, [](auto&) {}, &timeline_scope);
        }));
        require(timeline_scope->backend_0.scrub(.5), "Timeline did not change position");
        require(take(named(timeline_tree, "time").snapshot()).text == "00:00.500" &&
                    take(named(timeline_tree, "position").snapshot()).value == .5,
                "LVUI did not observe the timeline position");
        good(named(timeline_tree, "advance").activate());
        require(timeline_scope->backend_0.frame() == 13 &&
                    take(named(timeline_tree, "time").snapshot()).text == "00:00.542",
                "LVUI timeline action did not advance a frame");
        good(timeline_host.clear_content());
        timeline_scope.reset();
        timeline_tree->close();
        std::cout << "LVUI_EDITOR_VIEWMODEL_OK\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
