#include "QuickMask.h"
#include <algorithm>
#include <new>
namespace congregation::native {
using namespace iiSharedCanvas;
std::unique_ptr<QuickMask> QuickMask::create(CanvasExtent extent, const RasterMask* initial, std::string& error)
{
    if (extent.width<=0 || extent.height<=0 || std::uint64_t(extent.width)*extent.height>64ULL*1024*1024 ||
        (initial && (initial->extent!=extent || initial->alpha.size()!=std::size_t(extent.width)*extent.height))) {
        error="Quick mask requires a finite canvas of at most 64 megapixels";return {};
    }
    try {
        auto result=std::unique_ptr<QuickMask>(new QuickMask);
        const auto count=std::size_t(extent.width)*extent.height;
        result->original_=initial ? initial->alpha : std::vector<std::uint8_t>(count,255);
        auto pixels=makeRasterLayer(extent.width,extent.height,0);
        for(std::size_t i=0;i<count;++i) if(result->original_[i]) pixels.pixels[i]=(std::uint32_t(result->original_[i])<<24)|0xffffffU;
        result->document_.extent=extent;
        result->document_.assets.emplace_back(RasterAsset{"quick-mask",std::move(pixels)});
        result->document_.layers.emplace_back(StaticBitmapLayer{{"quick-mask-layer","Quick mask"},StaticSource{"quick-mask"}});
        if(!result->editor_.bind(result->document_,"quick-mask")) {error=result->editor_.lastError();return {};}
        error.clear();return result;
    } catch(const std::bad_alloc&) {error="Insufficient memory for quick mask";return {};}
}
std::unique_ptr<QuickMask> QuickMask::create_tiled(TiledMask initial,std::string& error)
{
    if (!initial) {error="Invalid tiled quick mask";return {};}
    try {auto result=std::unique_ptr<QuickMask>(new QuickMask);result->tiled_=std::make_unique<TiledMaskEditor>(std::move(initial));error.clear();return result;}
    catch(const std::bad_alloc&) {error="Insufficient memory for quick mask";return {};}
}
std::uint8_t QuickMask::coverage(std::int32_t x,std::int32_t y) const noexcept
{
    if(tiled_)return tiled_->coverage(x,y);
    const auto* image=pixels();if(!image||x<0||y<0||x>=image->width||y>=image->height)return 0;
    return std::uint8_t(image->pixels[std::size_t(y)*image->width+x]>>24);
}
bool QuickMask::begin(const StrokePoint& point, BitmapBrush brush, bool subtract)
{
    brush.argb=0xffffffffU;brush.eraser=subtract;
    if (brush.engineState) brush.engineState->rasterizer.blendMode=RasterBlendMode::SourceOver;
    if(tiled_) {if(!tiled_->begin(point,brush)){error_=tiled_->error();return false;}}
    else if(!editor_.setBrush(brush)||!editor_.beginStrokeInput(point)){error_=editor_.lastError();return false;}
    error_.clear();return true;
}
bool QuickMask::extend(const StrokePoint& point)
{
    if(tiled_) {if(!tiled_->extend(point)){error_=tiled_->error();tiled_->cancel();return false;}}
    else if(!editor_.continueStrokeInput(point)){error_=editor_.lastError();editor_.cancelStroke();return false;}
    error_.clear();return true;
}
bool QuickMask::end(const StrokePoint& point)
{
    if(tiled_) {if(!tiled_->end(point)){error_=tiled_->error();tiled_->cancel();return false;}}
    else if(!editor_.endStrokeInput(point)){error_=editor_.lastError();editor_.cancelStroke();return false;}
    error_.clear();return true;
}
void QuickMask::cancel_stroke(){if(tiled_)tiled_->cancel();else if(editor_.strokeActive())editor_.cancelStroke();}
bool QuickMask::undo(){return !stroke_active()&&(tiled_?tiled_->undo():editor_.undo());}
bool QuickMask::redo(){return !stroke_active()&&(tiled_?tiled_->redo():editor_.redo());}
bool QuickMask::changed() const
{
    if(tiled_)return tiled_->changed();
    const auto* current=pixels();if(!current || current->pixels.size()!=original_.size())return true;
    for(std::size_t i=0;i<original_.size();++i)if((current->pixels[i]>>24)!=original_[i])return true;
    return false;
}
std::optional<RasterMask> QuickMask::mask() const
{
    const auto* current=pixels();if(!current)return std::nullopt;
    try {RasterMask result{{current->width,current->height},std::vector<std::uint8_t>(current->pixels.size())};
         for(std::size_t i=0;i<result.alpha.size();++i)result.alpha[i]=std::uint8_t(current->pixels[i]>>24);return result;}
    catch(const std::bad_alloc&){return std::nullopt;}
}
}
