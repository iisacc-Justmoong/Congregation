#include "EditorIcons.h"
#include "EditorAssets.generated.h"
#include <map>
#include <stdexcept>
namespace congregation::native::editor
{
std::shared_ptr<const ii::gui::RasterImage> icon(std::string_view name)
{
    static const auto images=[] {
        std::map<std::string_view,std::shared_ptr<const ii::gui::RasterImage>,std::less<>> result;
        for(const auto& source:assets::icons) {
            auto image=ii::gui::svg_image(source.svg,32,32);
            if(!image)throw std::runtime_error(image.error().message);
            result.emplace(source.name,*image);
        }
        return result;
    }();
    auto found=images.find(name);
    if(found==images.end())throw std::runtime_error("Unknown Vincent editor icon: "+std::string(name));
    return found->second;
}
}
