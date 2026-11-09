#pragma once

#include <iiGUI/NativeWidgets.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace congregation::native
{
// iiGUI popup anchors belong to their native content generation.  The LVUI
// shell and its workspace are separate generations, so convert their window
// coordinates when opening a shell popup from the workspace.
inline ii::core::Result<void> open_scoped_popup_for(ii::gui::Widget root, ii::gui::Widget popup,
                                                    ii::gui::Widget anchor, double x, double y)
{
    auto anchored = popup.open_for(anchor, x, y);
    if (anchored || anchored.error().code != ii::core::ErrorCode::invalid_argument) return anchored;
    auto target = anchor.snapshot();
    if (!target) return std::unexpected(target.error());
    if (!target->visible) return std::move(anchored);
    auto origin = root.snapshot();
    if (!origin) return std::unexpected(origin.error());
    return popup.open_at(target->bounds.x - origin->bounds.x + x,
                         target->bounds.y - origin->bounds.y + y);
}

struct ScopedContextMenu final
{
    std::string target;
    ii::gui::Widget popup;
};

inline ii::core::Result<void> bind_scoped_context_menus(ii::gui::Widget workspace,
                                                       ii::gui::Widget root,
                                                       std::vector<ScopedContextMenu> menus)
{
    return workspace.set_input_handler(
        [workspace, root, menus = std::move(menus)](const ii::gui::WidgetInput &event)
            -> ii::core::Result<bool>
        {
            if (event.kind != ii::gui::WidgetInputKind::context_request) return false;
            for (const auto &menu : menus)
            {
                const std::string_view target = event.target;
                const bool qualified = target.size() > menu.target.size() && target.ends_with(menu.target) &&
                                       target[target.size() - menu.target.size() - 1] == '/';
                if (target != menu.target && !qualified) continue;
                if (!event.local_position) return false;
                auto owner = workspace.snapshot();
                if (!owner) return std::unexpected(owner.error());
                auto origin = root.snapshot();
                if (!origin) return std::unexpected(origin.error());
                auto opened = menu.popup.open_at(owner->bounds.x - origin->bounds.x + event.local_position->x,
                                                 owner->bounds.y - origin->bounds.y + event.local_position->y);
                if (!opened) return std::unexpected(opened.error());
                return true;
            }
            return false;
        });
}

template<class Find, class Snapshot>
ii::core::Result<ii::gui::Widget> find_scoped_widget(Find &&find, Snapshot &&snapshot,
                                                     std::string_view slot_id, std::string_view id)
{
    auto direct = find(id);
    if (direct || direct.error().code != ii::core::ErrorCode::not_found) return direct;

    // The common workspace controls live directly in the NativeView content
    // generation. Resolve them without walking the tree on each input event.
    auto slot = find(slot_id);
    if (slot) {
        auto child = slot->content_item(id);
        if (child || child.error().code != ii::core::ErrorCode::not_found) return child;
    } else if (slot.error().code != ii::core::ErrorCode::not_found) {
        return std::unexpected(slot.error());
    }

    // Some chooser and menu controls are in additional dynamic generations.
    // Their fully qualified IDs are discovered only when direct lookup fails.
    auto nodes = snapshot();
    if (!nodes) return std::unexpected(nodes.error());
    const auto suffix = "/" + std::string(id);
    std::optional<std::string> matched;
    for (const auto &node : *nodes) {
        if (node.id != id && !node.id.ends_with(suffix)) continue;
        if (matched && *matched != node.id)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Widget id is ambiguous across LVUI generations");
        matched = node.id;
    }
    return matched ? find(*matched) : std::move(direct);
}

// Keeps the original iiGUI mount usable while resolving LVUI-owned content
// generations for application code that addresses a widget by its local ID.
enum class NativeSurfaceSlot { workspace, canvas_setup };

class ScopedWidgetMount final
{
  public:
    ScopedWidgetMount(ii::gui::WidgetMount mount, NativeSurfaceSlot slot)
        : mount_(std::move(mount)), slot_(slot) {}

    [[nodiscard]] ii::core::Result<ii::gui::Widget> find(std::string_view id) const
    {
        return find_scoped_widget([this](std::string_view value) { return mount_.find(value); },
                                  [this] { return mount_.snapshot(); }, slot_id(), id);
    }
    [[nodiscard]] auto snapshot() const { return mount_.snapshot(); }
    [[nodiscard]] auto dispatch_input(const ii::gui::WidgetInput &input) const
    { return mount_.dispatch_input(input); }
    [[nodiscard]] ii::gui::WidgetMount native() const { return mount_; }
    operator ii::gui::WidgetMount() const { return mount_; }

  private:
    [[nodiscard]] std::string_view slot_id() const noexcept
    {
        return slot_ == NativeSurfaceSlot::workspace ? "workspace-host/@native-content"
                                                     : "canvas-setup-host/@native-content";
    }
    ii::gui::WidgetMount mount_;
    NativeSurfaceSlot slot_;
};
} // namespace congregation::native
