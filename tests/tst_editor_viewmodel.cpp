#include <Native/EditorBackend.h>
#include <Native/ChildWindowGeneration.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void titlebar_viewmodel()
{
    using namespace congregation::ui;
    using namespace ii::gui;
    EditorBackend backend;
    unsigned drags=0;
    backend.titlebar_drag=[&]() -> ii::core::Result<void> { ++drags; return {}; };
    require(backend.titlebar_input({.kind=WidgetInputKind::pointer_down,.device=PointerDevice::mouse,.buttons=1}).value() && drags==1,
            "A primary titlebar press must start the native window drag");
    require(!backend.titlebar_input({.kind=WidgetInputKind::pointer_move,.device=PointerDevice::mouse,.buttons=1}).value() &&
            !backend.titlebar_input({.kind=WidgetInputKind::pointer_down,.device=PointerDevice::mouse,.buttons=2}).value() &&
            !backend.titlebar_input({.kind=WidgetInputKind::pointer_down,.device=PointerDevice::touch,.buttons=1}).value() && drags==1,
            "Titlebar drag must not consume movement, secondary presses or touch");
    backend.set_agent_prompt("hello");
    backend.submit_agent();
    require(backend.agent_prompt.get()=="hello" && backend.status.get().find("not available")!=std::string::npos,
            "The Agent field must retain input and identify unavailable submission");
    Action action=Action::quit;
    backend.perform=[&](Action value){action=value;};
    backend.titlebar_close();
    require(action==Action::window_close,"Titlebar close did not use the window-close action");
    backend.titlebar_minimize();
    require(action==Action::window_minimize,"Titlebar minimize did not use the window-minimize action");
    backend.titlebar_maximize();
    require(action==Action::window_toggle_maximize,"Titlebar maximize did not use the toggle action");
    require(backend.titlebar_profile_input({.kind=WidgetInputKind::pointer_up}).value() && action==Action::profile,
            "The titlebar avatar must open the account action");
}

void child_window_generations()
{
    using congregation::native::ChildWindowGeneration;
    ChildWindowGeneration windows;
    std::vector<ChildWindowGeneration::Ticket> closed;
    for (int i = 0; i < 256; ++i)
    {
        auto ticket = windows.begin();
        require(windows.accepts(ticket) && windows.close(ticket) && !windows.close(ticket),
                "A child window did not close exactly once");
        closed.push_back(ticket);
    }
    auto current = windows.begin();
    for (const auto& ticket : closed)
        require(!windows.retire(ticket) && !windows.close(ticket) && windows.accepts(current),
                "A stale child window callback retired the current window");
    require(windows.close(current) && !windows.accepts(current) && windows.owns(current) &&
                windows.retire(current) && !windows.owns(current),
            "The current child window did not finish its deferred cleanup");
}

void callback_replacement()
{
    using namespace congregation::ui;
    EditorBackend backend;
    auto marker = std::make_shared<int>(1);
    std::weak_ptr<int> weak = marker;
    bool retained = false;
    backend.perform = [&, hold = marker](Action) {
        auto* model = &backend;
        auto* result = &retained;
        std::weak_ptr<int> observed = hold;
        model->perform = {};
        *result = !observed.expired();
    };
    marker.reset();
    backend.send(Action::preferences);
    require(retained && weak.expired(), "An action callback was destroyed while executing");

    marker = std::make_shared<int>(2);
    weak = marker;
    retained = false;
    backend.input = [&, hold = marker](const ii::gui::WidgetInput&) -> ii::core::Result<bool> {
        auto* model = &backend;
        auto* result = &retained;
        std::weak_ptr<int> observed = hold;
        model->input = {};
        *result = !observed.expired();
        return true;
    };
    marker.reset();
    auto result = backend.canvas_input({.kind = ii::gui::WidgetInputKind::pointer_move});
    require(result && *result && retained && weak.expired(),
            "An input callback was destroyed while executing");
}

void reentrant_viewmodel_changes()
{
    using namespace congregation::ui;
    EditorBackend backend;
    auto first_layers = std::make_shared<PresentationList>();
    auto first_members = std::make_shared<PresentationList>();
    auto next_layers = std::make_shared<PresentationList>();
    auto next_members = std::make_shared<PresentationList>();
    require(first_layers->replace({{.key = "first"}}).value() &&
                first_members->replace({{.key = "first"}}).value() &&
                next_layers->replace({{.key = "one"}}).value() &&
                next_layers->replace({{.key = "two"}}).value() &&
                next_members->replace({{.key = "one"}}).value() &&
                next_members->replace({{.key = "two"}}).value(),
            "Could not prepare list generations");
    bool switched = false;
    auto list_observer = backend.layer_revision.observe([&](const auto&, std::uint64_t) {
        if (!std::exchange(switched, true)) backend.set_presentation_lists(next_layers, next_members);
    });
    backend.set_presentation_lists(first_layers, first_members);
    require(switched && backend.layer_source == next_layers && backend.member_source == next_members &&
                backend.layer_revision.get() == 2 && backend.member_revision.get() == 2,
            "An older list publication overwrote the replacement generation");

    auto previous = std::make_shared<EditorColor>();
    auto replacement = std::make_shared<EditorColor>();
    require(replacement->set_color({.1, .2, .3, 1}).has_value() &&
                replacement->set_color({.3, .2, .1, 1}).has_value(),
            "Could not prepare replacement color");
    backend.set_color_model(previous);
    std::weak_ptr<EditorColor> old_color = previous;
    previous.reset();
    switched = false;
    bool retained = false;
    auto color_observer = backend.current_color.observe([&](const auto&, std::uint64_t) {
        if (!std::exchange(switched, true)) {
            backend.set_color_model(replacement);
            retained = !old_color.expired();
        }
    });
    require(backend.set_current_color({.4, .5, .6, 1}).has_value() && switched && retained &&
                old_color.expired() && backend.color == replacement &&
                backend.current_color.get() == replacement->current() &&
                backend.color_revision.get() == replacement->revision.get(),
            "A reentrant color change lost its owner or overwrote the replacement");

    auto edited_during_publish = std::make_shared<EditorColor>();
    require(edited_during_publish->set_color({.6, .1, .2, 1}).has_value(),
            "Could not prepare a color publication");
    bool edited = false;
    auto edit_observer = backend.current_color.observe([&](const auto&, std::uint64_t) {
        if (!std::exchange(edited, true))
            require(edited_during_publish->set_color({.7, .2, .1, 1}).has_value(),
                    "Reentrant color edit failed");
    });
    backend.set_color_model(edited_during_publish);
    require(edited && backend.current_color.get() == edited_during_publish->current() &&
                backend.color_revision.get() == edited_during_publish->revision.get(),
            "An older color revision overwrote a reentrant edit");

    auto dying = std::make_shared<EditorBackend>();
    auto color = dying->color;
    auto dying_observer = dying->current_color.observe([&](const auto&, std::uint64_t) { dying.reset(); });
    require(color->set_color({.2, .3, .4, 1}).has_value() && !dying,
            "Retiring a viewmodel during color notification did not finish safely");

    dying = std::make_shared<EditorBackend>();
    auto* raw = dying.get();
    auto dying_list_observer = dying->layer_revision.observe([&](const auto&, std::uint64_t) { dying.reset(); });
    raw->set_presentation_lists(first_layers, first_members);
    require(!dying, "Retiring a viewmodel during list publication did not finish safely");
}
}

int main()
{
    try
    {
        titlebar_viewmodel();
        child_window_generations();
        callback_replacement();
        reentrant_viewmodel_changes();
        using namespace congregation::ui;
        auto layers = std::make_shared<PresentationList>();
        auto members = std::make_shared<PresentationList>();
        auto color = std::make_shared<EditorColor>();
        EditorBackend second;
        second.set_presentation_lists(layers, members);
        second.set_color_model(color);
        {
            EditorBackend first;
            first.set_presentation_lists(layers, members);
            first.set_color_model(color);
            require(layers->replace({{.key = "a", .label = "Layer", .editable = true}}).value(),
                    "layer source did not publish");
            require(members->replace({{.key = "peer", .label = "Artist"}}).value(),
                    "member source did not publish");
            require(first.layer_revision.get() == 1 && second.layer_revision.get() == 1 &&
                        first.member_revision.get() == 1 && second.member_revision.get() == 1,
                    "shared source changes did not reach both LVUI scopes");
            auto other_layers = std::make_shared<PresentationList>();
            auto other_members = std::make_shared<PresentationList>();
            first.set_presentation_lists(other_layers, other_members);
            require(layers->replace({{.key = "b", .label = "Another layer"}}).value() &&
                        first.layer_revision.get() == 0 && second.layer_revision.get() == 2,
                    "a detached list source still updated the old scope");
            first.set_presentation_lists(layers, members);
            std::string selected;
            first.selected = [&](std::string key) { selected = std::move(key); };
            first.activate_layer(0, layers->snapshot().value()->entries.front());
            require(selected == "b", "LVUI layer activation did not preserve the item key");
            first.member_selected = [&](std::string key) { selected = std::move(key); };
            first.activate_member(0, members->snapshot().value()->entries.front());
            require(selected == "peer", "LVUI member activation did not preserve the item key");
            require(color->set_color({.2, .4, .6, .8}).has_value(), "shared color was rejected");
            require(first.current_color.get() == ii::gui::WidgetColor{.2, .4, .6, .8} &&
                        second.current_color.get() == first.current_color.get(),
                    "the LVUI color did not follow the shared product model");
            auto replacement = std::make_shared<EditorColor>();
            first.set_color_model(replacement);
            require(first.set_current_color({.1, .2, .3, 1}).has_value(), "LVUI color command failed");
            require(first.current_color.get() == ii::gui::WidgetColor{.1, .2, .3, 1},
                    "replacement color model did not publish");
            require(color->set_color({.7, .6, .5, 1}).has_value(), "original color update failed");
            require(first.current_color.get() == ii::gui::WidgetColor{.1, .2, .3, 1} &&
                        second.current_color.get() == ii::gui::WidgetColor{.7, .6, .5, 1},
                    "a detached color source still updated the old scope");
        }
        require(members->replace({{.key = "peer-2", .label = "Another artist"}}).value() &&
                    second.member_revision.get() == 2,
                "retired scope prevented later list updates");
        require(color->set_color({.3, .2, .1, 1}).has_value() &&
                    second.current_color.get() == ii::gui::WidgetColor{.3, .2, .1, 1},
                "retired scope prevented later color updates");
        std::cout << "EDITOR_VIEWMODEL_OK\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
