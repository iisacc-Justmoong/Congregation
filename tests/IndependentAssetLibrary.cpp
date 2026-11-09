#include <Native/AssetLibraryView.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

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
    if(found==nodes.end())throw std::runtime_error("Asset control did not mount: "+std::string(suffix));
    return take(tree->find(found->id));
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::runtime_error("Expected the Figma resource directory");
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto model=std::make_shared<congregation::ui::EditorBackend>();
    auto view=std::make_shared<congregation::native::AssetLibraryView>(model,argv[1]);
    good(tree->construct([&](auto& builder)->ii::core::Result<void>{view->build(builder,{});return builder.retain(view);}));
    tree->commit_publication();
    (void)take(tree->drawing_snapshot({224,1080}));
    require(take(widget(tree,"/asset-selected-name").snapshot()).text.find("Asset Library")!=std::string::npos,
            "Published asset view did not select its first item");
    good(widget(tree,"/asset-2").activate());
    require(take(widget(tree,"/asset-selected-name").snapshot()).text.find("Brush")!=std::string::npos,
            "Asset selection did not update its preview");
    model->asset_root.set("/Volumes/Storage/Workspace/.no-such-asset-directory");
    require(take(widget(tree,"/asset-selected-name").snapshot()).text=="No supported images in this folder",
            "Unavailable library did not clear its preview");
    tree->close();view.reset();
}
