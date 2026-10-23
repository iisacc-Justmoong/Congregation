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
}
}
