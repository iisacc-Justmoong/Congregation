#include <Native/EditorColor.h>
#include <LVUI/NativeColorSurface.h>
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
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
}

int main() {
    using namespace ii::gui;
    auto model = std::make_shared<congregation::ui::EditorColor>();
    require(model->current() == WidgetColor{1, 1, 1, 1}, "Initial editor color changed");
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 120}));
    lvui::native::ColorSurfaceInput plane_input, peer_input;
    plane_input.source = model->source();
    plane_input.configuration.kind = lvui::ColorSurfaceKind::saturation_brightness;
    plane_input.configuration.width = plane_input.configuration.height = 100;
    peer_input = plane_input;
    peer_input.configuration.kind = lvui::ColorSurfaceKind::hue_rail;
    peer_input.configuration.width = 20;
    std::optional<lvui::native::NativeColorSurface> plane, peer;
    int edits = 0;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto row = builder.hstack({}, "row", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!row) return std::unexpected(row.error());
        auto edited = [&](WidgetColor) { ++edits; good(model->publish_surface_edit()); };
        auto first = lvui::native::NativeColorSurface::create(builder, *row, "plane", plane_input, edited);
        if (!first) return std::unexpected(first.error());
        plane = *first;
        auto second = lvui::native::NativeColorSurface::create(builder, *row, "hue", peer_input, edited);
        if (!second) return std::unexpected(second.error());
        peer = *second;
        return {};
    }));
    auto lease = model->revision.observe([&](const std::int64_t& revision, std::uint64_t) {
        plane_input.source_revision = peer_input.source_revision = revision;
        good(plane->set_options(plane_input));
        good(peer->set_options(peer_input));
    });
    good(model->set_color({1, 0, 0, .5}));
    const auto changed = model->revision.get();
    good(model->set_color({1, 0, 0, .5}));
    require(model->revision.get() == changed, "A no-op color update published a change");
    require(!model->set_color({std::numeric_limits<double>::quiet_NaN(), 0, 0, 1}),
            "Invalid color was accepted");
    require(!model->set_alpha(-.1) && !model->set_alpha(1.1), "Invalid alpha was accepted");
    (void)take(tree->drawing_snapshot({240, 120}));
    const auto bounds = take(plane->root().snapshot()).bounds;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 50, .y = bounds.y + 50}));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = bounds.x + 60, .y = bounds.y + 50, .buttons = 1}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 60, .y = bounds.y + 50}));
    require(edits >= 2 && model->revision.get() > changed, "Surface drag did not publish shared edits");
    require(take(plane->root().snapshot()).accessibility_description ==
                take(peer->root().snapshot()).accessibility_description,
            "The second surface did not follow the edited color");
    good(model->set_alpha(.25));
    require(std::abs(model->current().alpha - .25) < 1e-9 &&
                take(peer->root().snapshot()).accessibility_description.ends_with("40"),
            "Alpha adjustment did not update the shared surfaces");
    bool foreign_rejected = false;
    std::thread other([&] { foreign_rejected = !model->set_color({0, 1, 0, 1}); });
    other.join();
    require(foreign_rejected && model->current().alpha == .25, "Foreign thread mutated the color");
    good(model->set_color({0, 0, 0, 1}));
    auto source = model->source();
    auto stale = source->revision();
    require(bool(source->edit_channel(lvui::ColorChannel::hue, 120, stale)), "Could not set latent hue");
    good(model->publish_surface_edit());
    require(!source->edit_channel(lvui::ColorChannel::brightness, 100, stale),
            "A stale color edit was accepted");
    require(bool(source->edit_channel(lvui::ColorChannel::saturation, 100, source->revision())),
            "Could not set saturation on black");
    good(model->publish_surface_edit());
    good(model->set_alpha(.5));
    require(bool(source->edit_channel(lvui::ColorChannel::brightness, 100, source->revision())),
            "Could not restore brightness");
    good(model->publish_surface_edit());
    const auto restored = model->current();
    require(restored.green > .99 && restored.red < .01 && restored.blue < .01 && restored.alpha == .5,
            "Alpha adjustment lost the latent hue or saturation");
    good(host.clear_content());
    tree->close();
}
