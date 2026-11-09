#include <Native/CanvasSetupWindow.h>
#include <Native/OwnedWidgetStyle.h>
#include <Native/ScopedWidgetMount.h>
// The private WidgetState header can be newer than the staged public iiGUI API.
namespace ii::gui { struct TextInputRevision; }
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
    return std::move(*value);
}
void good(ii::core::Result<void> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id==suffix.substr(1)||node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("New canvas control did not mount: "+std::string(suffix));
    return take(tree->find(found->id));
}
void check_borderless_window(const ii::gui::Widget& root,ii::gui::Rect bounds){
    const auto scene=take(root.snapshot_scene());
    bool has_background=false,has_outline=false;
    const auto inspect=[&](const auto& panels){
        for(const auto& panel:panels)if(panel.bounds==bounds){
            if(panel.stroke_width>0)has_outline=true;
            else has_background=true;
        }
    };
    for(const auto& layer:scene.layers)inspect(layer.panels);
    require(has_background,"New canvas content must fill the window");
    require(!has_outline,"New canvas content must not have an outer border");
}
void input_length_adapter(){
    using namespace ii::gui;
    using congregation::native::owned_widgets::Controls;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"limited-host",{300,100}));
    std::optional<congregation::native::owned_widgets::InputHandle> field;std::vector<std::string> edits;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        // A content host stretches its root to the viewport. Place the field in
        // a flow container so its own intrinsic height determines its frame.
        auto container=builder.vstack({},"input-container");
        if(!container)return std::unexpected(container.error());
        Controls controls(builder);InputOptions options;options.text="A😀ZQ";options.maximum_length=3;
        InputOptions custom;custom.style=InputStyle{};
        auto rejected=controls.input_field(*container,"unsupported-style",std::move(custom),{},{});
        require(!rejected&&rejected.error().code==ii::core::ErrorCode::unsupported,
                "Product adapter accepted a custom input style");
        auto value=controls.input_field(*container,"limited",options,[&](std::string text){edits.push_back(std::move(text));},{});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    auto value=[&]{auto root=field->root();return take(root.text_content({0,take(root.snapshot()).text_length})).text;};
    const auto input_layout=take(field->root().snapshot());
    require(input_layout.implicit_size.height==22&&input_layout.size.height==22,
            "Product input did not use the LVUI compact preset");
    const auto input_drawing=take(tree->drawing_snapshot({300,100}));
    require(std::ranges::any_of(input_drawing.overlays,[](const auto& layer){return layer.backdrop&&
                std::abs(layer.backdrop->blur_radius-8)<.01&&layer.images.size()==1;}),
            "Product input did not use the LVUI rounded material");
    require(value()=="A😀Z", "Product adapter did not preserve the scalar limit");
    auto clear=widget(tree,"/limited.clear");
    require(take(clear.snapshot()).visible,"Product input did not expose the shared clear action");
    good(field->focus());good(clear.activate());
    require(value().empty()&&take(field->root().snapshot()).focused&&edits==std::vector<std::string>{""},
            "Product clear did not preserve input focus and emit one edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(value()=="A😀Z","Product input clear could not be undone");edits.clear();
    good(field->focus());good(tree->input({.kind=WidgetInputKind::text,.text="Q"}));
    require(value()=="A😀Z"&&edits.empty(), "Product adapter accepted input beyond the scalar limit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    good(tree->input({.kind=WidgetInputKind::text,.text="가😀Xmore"}));
    require(value()=="가😀X"&&edits==std::vector<std::string>{"가😀X"}, "Product adapter split Unicode or emitted the wrong edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(value()=="A😀Z", "Product adapter lost bounded undo");
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        Controls controls(builder);InputOptions options;options.text="A😀Z";options.maximum_length=0;
        auto result=controls.input_field({},"unlimited",options,{},{});
        if(!result)return std::unexpected(result.error());field=*result;return {};
    }));
    good(field->focus());good(tree->input({.kind=WidgetInputKind::text,.text="Q"}));
    require(value()=="A😀ZQ", "Product adapter treated zero as an empty-field limit");tree->close();
}
void generated_setup_view(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"generated-host",{1280,880}));
    congregation::native::CanvasSetupWindow window;
    congregation::native::CanvasSetupModel* model=nullptr;
    int creates=0;
    good(host.set_content([&](auto& builder){
        return window.build_view(builder,{},[&](auto& scope){
            model=&scope.backend_0;model->create_requested=[&]{++creates;};
        });
    }));
    tree->commit_publication();
    (void)take(tree->drawing_snapshot({1280,880}));
    require(model!=nullptr,"LVUI setup did not configure its backend");
    const auto native_host=take(widget(tree,"/canvas-setup-host").snapshot());
    auto root=widget(tree,"/setup-root");
    require(take(root.snapshot()).parent_id==native_host.id+"/@native-content",
            "Chooser content is not inside the LVUI NativeView");
    const auto prefix=native_host.id.substr(0,native_host.id.size()-std::string("canvas-setup-host").size());
    const auto lookup=[&](std::string_view id){
        return tree->find(id.starts_with(prefix)?std::string(id):prefix+std::string(id));
    };
    const auto snapshots=[&]{return tree->snapshots();};
    for(const char* id : {"setup-root","create-canvas","canvasPresetCard_01-01-02"})
        require(bool(congregation::native::find_scoped_widget(lookup,snapshots,
                     "canvas-setup-host/@native-content",id)),"Chooser lookup lost an LVUI child");
    good(widget(tree,"/canvasPresetCard_01-01-02").activate());
    require(model->new_height.get()=="1350","LVUI chooser did not use its scoped model");
    good(widget(tree,"/create-canvas").activate());
    require(creates==1,"LVUI chooser action did not reach its scoped model");
    good(host.set_extent({640,480}));
    (void)take(tree->drawing_snapshot({640,480}));
    require(take(root.snapshot()).bounds.width==640,"LVUI chooser did not resize its native surface");
    good(host.clear_content());
    require(!root.snapshot(),"Retired LVUI chooser retained a live widget");
    model=nullptr;
    good(host.set_content([&](auto& builder){
        return window.build_view(builder,{},[&](auto& scope){model=&scope.backend_0;});
    }));
    tree->commit_publication();
    (void)take(tree->drawing_snapshot({640,480}));
    require(model && take(widget(tree,"/setup-root").snapshot()).bounds.width==640,
            "A new LVUI chooser generation did not mount after retirement");
    good(host.clear_content());
    tree->close();
}
}
int main(){
    input_length_adapter();
    generated_setup_view();
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    congregation::native::CanvasSetupWindow window;
    congregation::native::CanvasSetupModel* model=nullptr;
    int creates=0;
    auto host=take(tree->content_host({},"host",{1280,880}));
    good(host.set_content([&](auto& builder){
        return window.build_view(builder,{},[&](auto& scope){
            model=&scope.backend_0;model->create_requested=[&]{++creates;};
        });
    }));
    require(model!=nullptr,"Chooser did not configure its LVUI model");
    tree->commit_publication();
    good(host.set_extent({1279,879}));
    (void)take(tree->drawing_snapshot({1279,879}));
    good(host.set_extent({1280,880}));
    (void)take(tree->drawing_snapshot({1280,880}));
    const auto root=widget(tree,"/setup-root");
    const auto desktop_root=take(root.snapshot());
    require(desktop_root.bounds.width==1280,"Chooser width changed");
    check_borderless_window(root,desktop_root.bounds);
    good(widget(tree,"/canvasPresetCard_01-01-02").activate());
    require(model->new_height.get()=="1350","Preset selection did not reach the model");
    auto search=widget(tree,"/canvasPresetSearch");
    good(search.focus());
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    auto clear=widget(tree,"/canvasSearchClear");
    require(model->query.empty()&&take(clear.snapshot()).visible,
            "Marked search text did not expose its clear action before commit");
    const auto clear_bounds=take(clear.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=clear_bounds.x+clear_bounds.width/2,
                      .y=clear_bounds.y+clear_bounds.height/2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=clear_bounds.x+clear_bounds.width/2,
                      .y=clear_bounds.y+clear_bounds.height/2}));
    require(model->query.empty()&&take(search.snapshot()).text_length==0&&take(search.snapshot()).focused,
            "Clearing marked search text changed the model or lost focus");
    good(tree->input({.kind=WidgetInputKind::text,.text="A4"}));
    require(model->query=="A4"&&take(widget(tree,"/canvasSearchClear").snapshot()).visible,
            "Editing search did not show its clear action or update results");
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(take(search.snapshot()).text_length==3,"Search composition did not start");
    require(model->choose("01-01-02"),"The search-refresh fixture preset is missing");
    require(model->query=="A4"&&take(search.snapshot()).text_length==3&&
                take(search.text_content({0,3})).text=="A4한",
            "An unrelated model refresh discarded the marked search text");
    good(tree->input({.kind=WidgetInputKind::composition,.text=""}));
    good(widget(tree,"/canvasSearchClear").activate());
    require(model->query.empty()&&take(search.snapshot()).focused,
            "Clear did not reset the query and retain input focus");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(model->query=="A4"&&take(search.snapshot()).focused,
            "Undo did not restore the query cleared by the user");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(model->query.empty(),"Redo did not repeat clearing the query");
    model->search("A4");
    require(model->result_count()==11 && model->unit=="mm","Search did not update the catalogue");
    good(widget(tree,"/canvasUnitsButton").activate());
    require(take(widget(tree,"/canvasUnitsMenu").snapshot()).opened,"Units menu did not open");
    good(widget(tree,"/canvasUnit-in").activate());
    require(model->unit=="in" && model->custom,"Units menu did not update the model");
    good(widget(tree,"/canvasBackgroundButton").activate());
    good(widget(tree,"/canvasBackground-Transparent").activate());
    require(model->background=="Transparent","Background menu did not update the model");
    auto width=widget(tree,"/canvas-width");
    good(width.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    good(tree->input({.kind=WidgetInputKind::text,.text="0"}));
    require(model->new_width.get()=="0" && take(width.snapshot()).focused,
            "Width editing did not reach the model or preserve focus");
    good(widget(tree,"/create-canvas").activate());
    require(creates==0 && !model->can_create.get(),"Invalid dimensions created a canvas");
    const auto bounds=take(width.snapshot()).bounds;
    const auto click_x=bounds.x+12,click_y=bounds.y+bounds.height/2;
    for(const auto time:{10.,10.2}) {
        good(tree->input({.kind=WidgetInputKind::pointer_down,.x=click_x,.y=click_y,.timestamp=time,.buttons=1}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=click_x,.y=click_y,.timestamp=time+.03}));
    }
    good(tree->input({.kind=WidgetInputKind::text,.text="8"}));
    require(model->new_width.get()=="8","Double-click did not select the width before replacement");
    good(widget(tree,"/create-canvas").activate());
    require(creates==1 && model->can_create.get(),"Valid dimensions did not create a canvas");
    good(host.set_extent({640,480}));
    (void)take(tree->drawing_snapshot({640,480}));
    auto resized=take(root.snapshot());
    check_borderless_window(root,resized.bounds);
    auto category=take(widget(tree,"/canvasCategoryDropdown").snapshot());
    require(resized.bounds.width==640 && category.visible,"Compact layout did not expose its category menu");
    auto compact_search=widget(tree,"/canvasPresetSearchCompact");
    good(compact_search.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    good(tree->input({.kind=WidgetInputKind::text,.text="Square"}));
    good(widget(tree,"/canvasSearchClearCompact").activate());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(model->query=="Square"&&take(compact_search.snapshot()).focused,
            "Compact search did not retain focus and undo its clear action");
    good(widget(tree,"/canvasCategoryDropdown").activate());
    require(take(widget(tree,"/canvasCategoryMenu").snapshot()).opened,"Compact category menu did not open");
    good(widget(tree,"/canvasCategoryChoice-0").activate());
    require(model->category_index==0 && !model->custom,"Compact category choice did not reach the model");
    good(host.set_extent({1280,880}));
    (void)take(tree->drawing_snapshot({1280,880}));
    require(!take(widget(tree,"/canvasCategoryDropdown").snapshot()).visible &&
                take(widget(tree,"/canvasCategoryScroll").snapshot()).visible,
            "Returning to desktop layout did not restore its category list");
    tree->close();
}
