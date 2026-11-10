#include "ToolPanel.h"
#include "ToolPanelBackend.h"
#include "ToolPanelView.generated.h"
#include "OwnedWidgetStyle.h"
#include <LVUI/NativeBackendFactories.h>

namespace congregation::native {
ii::gui::Widget build_tool_panel(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
                                 std::shared_ptr<ui::EditorBackend> editor, bool expanded) {
    using owned_widgets::good;
    using owned_widgets::take;
    auto host=take(builder.content_host(parent,"tool-panel-host"));
    if(!expanded)good(host.set_flex(1));
    std::shared_ptr<lvui_generated::ToolPanelView::Scope> scope;
    auto factories=lvui::native::BackendFactories{}.with<ui::ToolPanelBackend>(
        [editor,expanded] {return ui::ToolPanelBackend(editor,expanded);});
    good(host.set_content([&scope,&factories](ii::gui::WidgetBuilder& content) {
        return lvui_generated::ToolPanelView::build(content,{}, {}, &scope,factories);
    }));
    if(!scope)throw std::runtime_error("The declarative tool panel did not create its scope");
    scope->backend_0.attach(take(host.content_item("tool-pressure-curve")),
                            take(host.content_item("tool-scroll")),
                            take(host.content_item("parameter-tilt")),
                            take(host.content_item("parameter-stabilization")));
    if(expanded) {
        auto body=take(host.content_item("tool-content"));
        auto scroll=take(host.content_item("tool-scroll"));
        auto root=take(host.content_item("tool-panel"));
        const auto natural=take(body.measure(Metric::inspectorWidth-4*Metric::space4));
        good(scroll.set_extent({Metric::inspectorWidth-2*Metric::space4,natural.height}));
        const auto panel=take(root.measure(Metric::inspectorWidth));
        good(host.set_extent({0,panel.height}));
    }
    return host;
}
} // namespace congregation::native
