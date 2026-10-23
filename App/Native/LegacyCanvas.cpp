#include "LegacyCanvas.h"
#include "Shapes.h"
#include <iiGUI/Text.h>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
std::uint32_t argb(std::string text)
{
    if (text.empty())
        return 0xff1a1a1a;
    if (text[0] != '#')
        throw std::invalid_argument("Invalid legacy color");
    text.erase(0, 1);
    if (text.size() == 3)
    {
        std::string full;
        for (auto c : text)
        {
            full += c;
            full += c;
        }
        text = std::move(full);
    }
    if (text.size() != 6 && text.size() != 8)
        throw std::invalid_argument("Invalid legacy color");
    std::uint32_t color = 0;
    auto result = std::from_chars(text.data(), text.data() + text.size(), color, 16);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid legacy color");
    return text.size() == 6 ? color | 0xff000000 : color;
}
RasterBlendMode blend(std::string value)
{
    static const std::pair<std::string_view, RasterBlendMode> modes[]{
        {"normal", RasterBlendMode::SourceOver},
        {"sourceover", RasterBlendMode::SourceOver},
        {"multiply", RasterBlendMode::Multiply},
        {"screen", RasterBlendMode::Screen},
        {"overlay", RasterBlendMode::Overlay},
        {"darken", RasterBlendMode::Darken},
        {"lighten", RasterBlendMode::Lighten},
        {"colorburn", RasterBlendMode::ColorBurn},
        {"linearburn", RasterBlendMode::LinearBurn},
        {"darkercolor", RasterBlendMode::DarkerColor},
        {"colordodge", RasterBlendMode::ColorDodge},
        {"lineardodge", RasterBlendMode::LinearDodge},
        {"lightercolor", RasterBlendMode::LighterColor},
        {"softlight", RasterBlendMode::SoftLight},
        {"hardlight", RasterBlendMode::HardLight},
        {"vividlight", RasterBlendMode::VividLight},
        {"linearlight", RasterBlendMode::LinearLight},
        {"pinlight", RasterBlendMode::PinLight},
        {"hardmix", RasterBlendMode::HardMix},
        {"difference", RasterBlendMode::Difference},
        {"exclusion", RasterBlendMode::Exclusion},
        {"subtract", RasterBlendMode::Subtract},
        {"divide", RasterBlendMode::Divide},
        {"hue", RasterBlendMode::Hue},
        {"saturation", RasterBlendMode::Saturation},
        {"color", RasterBlendMode::Color},
        {"luminosity", RasterBlendMode::Luminosity},
        {"dissolve", RasterBlendMode::Dissolve}};
    value = lower(trim(std::move(value)));
    if (value.empty())
        return RasterBlendMode::SourceOver;
    for (auto [name, mode] : modes)
        if (value == name)
            return mode;
    throw std::invalid_argument("Unsupported legacy layer blend mode");
}
} // namespace
std::expected<Document, std::string> migrate_legacy_canvas(const RecentCanvas& canvas)
{
    try
    {
        auto decoded = decodeIisc({reinterpret_cast<const std::uint8_t*>(canvas.document.data()),
                                   canvas.document.size()});
        if (!decoded.ok())
            return std::unexpected(decoded.error.message);
        auto doc = std::move(decoded.document);
        if (!canvas.background)
            for (auto& layer : doc.layers)
                layerProperties(layer).visible = false;
        std::set<int> identifiers;
        std::set<std::pair<int, int>> consumed;
        for (const auto& value : canvas.objects)
        {
            const auto object = congregation::object(value);
            const auto raw_id = number(field(object, "id"));
            if (!std::isfinite(raw_id) || raw_id < 1 || raw_id > INT32_MAX ||
                std::floor(raw_id) != raw_id)
                throw std::invalid_argument("Invalid legacy object identifier");
            const auto id = int(raw_id);
            if (id <= 0 || !identifiers.insert(id).second)
                throw std::invalid_argument("Invalid legacy object identifier");
            const auto type = text(field(object, "type"));
            const auto asset = uuid();
            LayerProperties p{uuid(), text(field(object, "name"))};
            if (p.name.empty())
                p.name = type;
            const auto n = [&](const char* key, double fallback = 0)
            {
                const auto& v = field(object, key);
                const auto result = v.kind() == ii::core::ValueKind::null ? fallback : number(v);
                if (!std::isfinite(result) || std::abs(result) > 1e7)
                    throw std::invalid_argument("Invalid legacy object geometry");
                return result;
            };
            p.visible = field(object, "visible").kind() == ii::core::ValueKind::null ||
                        boolean(field(object, "visible"));
            p.opacity = std::clamp(n("opacity", 1), 0., 1.);
            p.blendMode = blend(text(field(object, "blendMode")));
            p.transform.translationX = n("x");
            p.transform.translationY = n("y");
            const auto w = n("width"), h = n("height");
            if (type == "image" || type == "layer")
            {
                const int kind = type == "image" ? 1 : 2;
                auto found =
                    std::ranges::find_if(canvas.assets, [&](const auto& item)
                                         { return item.objectId == id && item.kind == kind; });
                if (found == canvas.assets.end())
                    throw std::invalid_argument("Legacy image pixels are missing");
                consumed.emplace(id, kind);
                auto bitmap = decodeBitmap(
                    {reinterpret_cast<const std::uint8_t*>(found->png.data()), found->png.size()});
                if (!bitmap.ok())
                    throw std::invalid_argument(bitmap.result.message);
                if (type == "image")
                {
                    if (w < 1 || h < 1 || w > 32768 || h > 32768)
                        throw std::invalid_argument("Invalid legacy image extent");
                    p.transform.m11 = w / bitmap.asset.pixels.width;
                    p.transform.m22 = h / bitmap.asset.pixels.height;
                }
                else
                    p.transform = {};
                bitmap.asset.id = asset;
                doc.assets.emplace_back(std::move(bitmap.asset));
                doc.layers.emplace_back(StaticBitmapLayer{std::move(p), StaticSource{asset}});
            }
            else if (type == "shape")
            {
                if (w < 1 || h < 1 || w > 32768 || h > 32768)
                    throw std::invalid_argument("Invalid legacy shape extent");
                auto kind = lower(text(field(object, "shapeKind")));
                if (kind.empty())
                    kind = "rectangle";
                if (kind == "triagle")
                    kind = "triangle";
                doc.assets.emplace_back(VectorAsset{
                    asset,
                    {int(std::ceil(w)), int(std::ceil(h))},
                    {shape_path(kind, w, h, argb(text(field(object, "color"))), 0, true)}});
                doc.layers.emplace_back(StaticVectorLayer{std::move(p), StaticSource{asset}});
            }
            else if (type == "text")
            {
                const auto content = text(field(object, "text"));
                if (w < 1 || h < 1 || w > 8192 || h > 8192)
                    throw std::invalid_argument("Invalid legacy text extent");
                const auto c = argb(text(field(object, "color")));
                const auto linear = [](unsigned v)
                {
                    const auto n = v / 255.f;
                    return n <= .04045f ? n / 12.92f : std::pow((n + .055f) / 1.055f, 2.4f);
                };
                auto raster = ii::gui::render_text(
                    {.bounds = {0, 0, float(w), float(h)},
                     .text = content,
                     .color = {linear((c >> 16) & 255), linear((c >> 8) & 255), linear(c & 255),
                               float(c >> 24) / 255},
                     .font_size = float(std::clamp(n("fontPixelSize", 16), 6., 512.)),
                     .font_family = "Pretendard"});
                if (!raster)
                    throw std::invalid_argument(raster.error().message);
                auto pixels = makeRasterLayer(raster->width, raster->height, 0);
                for (std::size_t i = 0; i < pixels.pixels.size(); ++i)
                {
                    auto* c = raster->rgba.data() + 4 * i;
                    pixels.pixels[i] = (std::uint32_t(c[3]) << 24) | (std::uint32_t(c[0]) << 16) |
                                       (std::uint32_t(c[1]) << 8) | c[2];
                }
                doc.assets.emplace_back(RasterAsset{asset, std::move(pixels)});
                doc.layers.emplace_back(StaticBitmapLayer{std::move(p), StaticSource{asset}});
            }
            else
                throw std::invalid_argument("Unsupported legacy object type");
        }
        if (consumed.size() != canvas.assets.size())
            throw std::invalid_argument("Legacy file contains unreferenced pixels");
        auto validation = validate(doc);
        if (!validation.ok())
            return std::unexpected(validation.issues.front().message);
        return doc;
    }
    catch (const std::exception& error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace congregation::native
