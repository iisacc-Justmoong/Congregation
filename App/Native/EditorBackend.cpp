#include "EditorBackend.h"
#include "EditorIcons.h"
namespace congregation::ui
{
EditorBackend::EditorBackend()
{
    using native::editor::icon;
    pan_icon=icon("panHand");move_icon=icon("translateObject");
    add_icon=icon("addFile");open_icon=icon("generalopen");save_icon=icon("generalsave");
    zoom_icon=icon("generalsearch");brush_icon=icon("showCode");eraser_icon=icon("eraser");
    shape_icon=icon("rectangle");fill_icon=icon("fillbucket");text_icon=icon("typeAlias");
    minus_icon=icon("imagezoomOut");plus_icon=icon("imagezoomIn");
    sync_parameter_fields();
}
void EditorBackend::sync_parameter_fields()
{
    using P = native::ToolParameter;
    brush_size.set(parameters->value(P::size));
    opacity.set(parameters->value(P::opacity) / 100);
    flow.set(parameters->value(P::flow) / 100);
    hardness.set(parameters->value(P::hardness) / 100);
    spacing.set(parameters->value(P::spacing) / 100);
    stabilizer.set(parameters->value(P::stabilization) / 100);
}
void EditorBackend::set_parameter(native::ToolParameter id, double value)
{
    if (parameters->set(id, value))
    {
        sync_parameter_fields();
        if (brush_changed)
            brush_changed();
    }
}
void EditorBackend::set_parameter_normalized(native::ToolParameter id, double value)
{
    if (parameters->set_normalized(id, value))
    {
        sync_parameter_fields();
        if (brush_changed)
            brush_changed();
    }
}
void EditorBackend::step_parameter(native::ToolParameter id, int direction)
{
    if (parameters->step(id, direction))
    {
        sync_parameter_fields();
        if (brush_changed)
            brush_changed();
    }
}
void EditorBackend::reset_parameters()
{
    parameters->reset();
    sync_parameter_fields();
    if (brush_changed)
        brush_changed();
}
void EditorBackend::select_tool(std::string name)
{
    if (tool_selected)
        tool_selected(std::move(name));
    else
        selected_tool.set(std::move(name));
}
void EditorBackend::choose_preset()
{
    if (preset.get().starts_with("Textured"))
    {
        parameters->reset();
        parameters->set(native::ToolParameter::size, 24);
        parameters->set(native::ToolParameter::hardness, 100);
        parameters->set(native::ToolParameter::wetness, 0);
        preset.set("Graphite pencil · Normal");
    }
    else
    {
        parameters->reset();
        preset.set("Textured oil · Normal");
    }
    sync_parameter_fields();
    if (brush_changed)
        brush_changed();
}
}
