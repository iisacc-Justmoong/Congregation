#include "Artwork.h"
#include <Engine/Data.h>
#include <iiGraphicAdjust/Image.h>
#include <iiGraphicAdjust/MeshFilters.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
// Synchronous detached conversion. Color uses the SDK's normalized sRGB,
// straight-alpha contract; interpolation is performed in premultiplied form.
std::optional<RasterLayer> rectify(const RasterLayer& source,
                                   const iiGraphicAdjust::MeshQuad& quad,
                                   CanvasExtent output, std::string& error)
{
    auto image = iiGraphicAdjust::fromArgb32(source.width, source.height, source.pixels);
    if (!image) { error = std::string(image.error().message); return {}; }
    auto cropped = iiGraphicAdjust::perspectiveCrop(image->view(), quad, output.width, output.height,
                                                    iiGraphicAdjust::MeshInterpolation::Bilinear);
    if (!cropped) { error = std::string(cropped.error().message); return {}; }
    auto pixels = iiGraphicAdjust::toArgb32(cropped->view());
    if (!pixels) { error = std::string(pixels.error().message); return {}; }
    return RasterLayer{output.width, output.height, std::move(*pixels)};
}
}
bool Artwork::perspective_crop(const std::array<Point,4>& points, CanvasExtent output, bool discard_replaced_assets)
{
    if (stroke_active()) return fail("Finish the current stroke before cropping");
    if (draft_.canvasMode != CanvasMode::Finite) return fail("Perspective crop requires a finite canvas");
    const auto source = draft_.extent;
    constexpr std::uint64_t maxImagePixels = 4ULL * 1024 * 1024;
    constexpr std::uint64_t maxOutputPixels = 16ULL * 1024 * 1024;
    if (source.width <= 0 || source.height <= 0 || output.width <= 0 || output.height <= 0 ||
        std::uint64_t(output.width) * output.height > maxImagePixels ||
        draft_.layers.size() > 64 || draft_.timeline.frameCount > 64 ||
        std::uint64_t(output.width) * output.height * draft_.layers.size() * draft_.timeline.frameCount > maxOutputPixels)
        return fail("Perspective crop exceeds the working budget");
    if (std::ranges::any_of(points,[&](const auto& p) {
            return !std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.y<0 || p.x>source.width || p.y>source.height;
        })) return fail("Perspective crop requires a valid in-bounds quadrilateral");
    CanvasRegion region{{0,0},source};
    if(std::uint64_t(source.width)*source.height>maxImagePixels) {
        double left=source.width,top=source.height,right=0,bottom=0;
        for(const auto& p:points){left=std::min(left,p.x);top=std::min(top,p.y);right=std::max(right,p.x);bottom=std::max(bottom,p.y);}
        const int x=int(std::clamp(std::floor(left)-2.,0.,double(source.width)));
        const int y=int(std::clamp(std::floor(top)-2.,0.,double(source.height)));
        const int endX=int(std::clamp(std::ceil(right)+2.,0.,double(source.width)));
        const int endY=int(std::clamp(std::ceil(bottom)+2.,0.,double(source.height)));
        if(endX<=x || endY<=y || std::uint64_t(endX-x)*std::uint64_t(endY-y)>maxImagePixels)
            return fail("Perspective crop reference exceeds the working budget");
        region={{x,y},{endX-x,endY-y}};
    }
    if (!draft_.artboards.empty()) return fail("Perspective crop does not yet support artboards");
    for (const auto& layer : draft_.layers) {
        if (!std::holds_alternative<StaticBitmapLayer>(layer) && !std::holds_alternative<DynamicBitmapLayer>(layer) &&
            !std::holds_alternative<StaticVectorLayer>(layer) && !std::holds_alternative<DynamicVectorLayer>(layer))
            return fail("Perspective crop requires artwork layers");
        if (layerProperties(layer).blendMode == RasterBlendMode::Dissolve)
            return fail("Perspective crop does not yet support dissolve layers");
        const auto& filters=layerProperties(layer).appearance.filters;
        if(std::ranges::any_of(filters,[](const auto& filter){return filter.enabled && filter.opacity>0;})) {
            for(FrameIndex frame=0;frame<draft_.timeline.frameCount;++frame) {
                if(!layerExistsAt(draft_,layer,frame))continue;
                const auto* asset=resolveAssetAt(draft_,layer,frame);CanvasExtent extent{};
                if(const auto* raster=asset?std::get_if<RasterAsset>(asset):nullptr)extent={raster->pixels.width,raster->pixels.height};
                else if(const auto* vector=asset?std::get_if<VectorAsset>(asset):nullptr)extent=vector->viewport;
                else return fail("Cannot resolve a filtered crop layer");
                if(extent.width<=0||extent.height<=0||std::uint64_t(extent.width)*extent.height>maxImagePixels)
                    return fail("Filtered crop asset exceeds the working budget");
            }
        }
    }
    auto before = capture();
    if (before.empty()) return fail("Perspective crop exceeds the undo budget");
    try {
        iiGraphicAdjust::MeshQuad quad{};
        for (std::size_t i=0;i<points.size();++i) quad[i] = {points[i].x-region.origin.x,points[i].y-region.origin.y};
        // Validate the quadrilateral even for an empty document before staging edits.
        iiGraphicAdjust::PerspectiveWarpOptions validation;
        const iiGraphicAdjust::MeshQuad destination{{{0,0},{double(output.width),0},
            {double(output.width),double(output.height)},{0,double(output.height)}}};
        validation.planes.push_back({quad,destination});
        const auto mapped = iiGraphicAdjust::mapPerspectiveWarpPoint(
            {region.extent.width/2.,region.extent.height/2.},validation);
        if (!mapped) return fail("Perspective crop requires a valid in-bounds quadrilateral");
        std::unordered_set<std::string> replacedAssets;
        if (discard_replaced_assets)
            for (const auto& asset : draft_.assets)
                if (!assetReferences(draft_, assetId(asset)).empty()) replacedAssets.insert(assetId(asset));
        Document visible = draft_;
        for (auto& layer : visible.layers) layerProperties(layer).visible = true;
        Document next = draft_;
        next.extent = output;
        next.frames.clear();
        if (next.timeline.frameCount > 1 && !next.layers.empty()) {
            next.frames.resize(next.timeline.frameCount);
            for (FrameIndex frame=0;frame<next.timeline.frameCount;++frame) next.frames[frame].index=frame;
        }
        for (std::size_t i=0;i<draft_.layers.size();++i) {
            auto properties = layerProperties(draft_.layers[i]);
            properties.transform = {};
            for (auto& key : properties.motion) {
                key.value.position={}; key.value.scale={1,1}; key.value.anchor={}; key.value.rotationDegrees=0;
            }
            properties.appearance.rasterMask.reset(); properties.appearance.vectorMask.reset();
            properties.appearance.filterMask.reset(); properties.appearance.filters.clear();
            properties.appearance.maskStack.clear(); properties.appearance.maskStackEnabled=true;
            KeyframedSource track;
            std::string firstAsset;
            for (FrameIndex frame=0;frame<draft_.timeline.frameCount;++frame) {
                RasterLayer pixels = makeRasterLayer(output.width,output.height,0);
                if (layerExistsAt(visible,visible.layers[i],frame)) {
                    auto rendered=renderFrameLayerTiles(visible,frame,i,{{region,region.extent,RasterSampling::Nearest}});
                    if (!rendered.ok() || !rendered.visible || !rendered.spatial || rendered.tiles.size()!=1)
                        return fail(rendered.message.empty() ? "Cannot render a crop layer" : rendered.message);
                    std::string error;
                    auto result=rectify(rendered.tiles.front().pixels,quad,output,error);
                    if (!result) return fail(error.empty() ? "Cannot rectify the crop" : error);
                    pixels=std::move(*result);
                }
                auto id=uuid();
                if (frame==0) firstAsset=id;
                next.assets.emplace_back(RasterAsset{id,std::move(pixels)});
                if (next.timeline.frameCount>1) {
                    track.frameIndices.push_back(frame);
                    next.frames[frame].keyframes.push_back({properties.id,std::move(id)});
                }
            }
            if (next.timeline.frameCount==1) next.layers[i]=StaticBitmapLayer{std::move(properties),StaticSource{std::move(firstAsset)}};
            else next.layers[i]=DynamicBitmapLayer{std::move(properties),std::move(track)};
        }
        for (auto& frame : next.frames)
            std::ranges::sort(frame.keyframes, {}, &Keyframe::layerId);
        if (discard_replaced_assets)
            std::erase_if(next.assets, [&](const Asset& asset) {
                const auto& id = assetId(asset);
                return replacedAssets.contains(id) && assetReferences(next, id).empty();
            });
        std::optional<SelectionTransition> selection;
        if (selection_) {
            RasterMask coverage;
            if(selection_->tiled) {
                if(selection_->tiled->domain()!=CanvasRegion{canvasOrigin(draft_),source}) return fail("Selection no longer matches the canvas");
                auto bounded=selection_->tiled->materialize(region);if(!bounded)return fail(bounded.error());coverage=std::move(*bounded);
            } else {
                if(!selection_->dense || selection_->dense->extent!=source || selection_->dense->alpha.size()!=std::size_t(source.width)*source.height)
                    return fail("Selection no longer matches the canvas");
                coverage={region.extent,std::vector<std::uint8_t>(std::size_t(region.extent.width)*region.extent.height)};
                for(int y=0;y<region.extent.height;++y)std::copy_n(selection_->dense->alpha.begin()+std::size_t(region.origin.y+y)*source.width+region.origin.x,
                    region.extent.width,coverage.alpha.begin()+std::size_t(y)*region.extent.width);
            }
            RasterLayer mask = makeRasterLayer(region.extent.width,region.extent.height,0);
            for(std::size_t i=0;i<mask.pixels.size();++i) mask.pixels[i]=(std::uint32_t(coverage.alpha[i])<<24)|0x00ffffffU;
            std::string error;auto converted=rectify(mask,quad,output,error);
            if (!converted) return fail(error.empty()?"Cannot rectify the selection":error);
            auto updated=std::make_shared<RasterMask>();updated->extent=output;updated->alpha.resize(converted->pixels.size());
            for(std::size_t i=0;i<updated->alpha.size();++i) updated->alpha[i]=std::uint8_t(converted->pixels[i]>>24);
            selection=SelectionTransition{selection_,std::make_shared<SelectionValue>(SelectionValue{std::move(updated),{}})};
        }
        SerializationLimits limits; limits.maximumContainerBytes = history_budget_ / 2;
        const auto encoded = encodeIisc(next, limits);
        if (!encoded.ok()) return fail("Cannot encode perspective crop: " + encoded.error.message);
        const auto& after = encoded.bytes;
        const std::size_t maskBytes=selection ? selection->before->bytes()+selection->after->bytes() : 0;
        if (after.empty() || before.size()>history_budget_ || after.size()>history_budget_-before.size() ||
            maskBytes>history_budget_-before.size()-after.size()) {
            return fail("Perspective crop exceeds the undo budget");
        }
        draft_=std::move(next);
        return commit(std::move(before),selected_,true,std::move(selection));
    } catch(const std::bad_alloc&) {draft_=*file_.document();bind();return fail("Insufficient memory for perspective crop");}
}
} // namespace congregation::native
