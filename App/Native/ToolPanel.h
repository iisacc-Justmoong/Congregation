#pragma once
#include "EditorBackend.h"
namespace congregation::native
{
[[nodiscard]] ii::gui::Widget build_tool_panel(ii::gui::WidgetBuilder&, ii::gui::Widget,
                                               std::shared_ptr<congregation::ui::EditorBackend>,
                                               bool expanded = false);
}
