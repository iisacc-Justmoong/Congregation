#include "Artwork.h"
#include "Shapes.h"
#include "LegacyCanvas.h"
#include <Engine/Data.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderCompatibility.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
constexpr std::size_t HistoryBudget = 64 * 1024 * 1024;
constexpr std::size_t HistoryCount = 32;
const Layer* layer(const Document& d, std::string_view key)
{
    auto p = std::ranges::find_if(d.layers, [&](const auto& value)
                                  { return layerProperties(value).id == key; });
    return p == d.layers.end() ? nullptr : &*p;
}
std::optional<DocumentPoint> localPoint(DocumentPoint point, const AffineTransform& t)
{
    const double determinant = t.m11 * t.m22 - t.m12 * t.m21;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        !std::isfinite(determinant) || determinant == 0)
        return {};
    const auto x = point.x - t.translationX, y = point.y - t.translationY;
    const DocumentPoint result{(t.m22 * x - t.m21 * y) / determinant,
                               (-t.m12 * x + t.m11 * y) / determinant};
    if (!std::isfinite(result.x) || !std::isfinite(result.y))
        return {};
    return result;
}

} // namespace
std::filesystem::path native_path(std::string_view value)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
std::string path_text(const std::filesystem::path& p)
{
    auto value = p.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
Artwork::Artwork(std::filesystem::path directory) : directory_(std::move(directory))
{
    iiFileProvider::native::File::createDirectories(directory_);
}
bool Artwork::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
std::vector<std::uint8_t> Artwork::capture() const
{
    SerializationLimits limits;
    limits.maximumContainerBytes = HistoryBudget / 2;
    auto encoded = encodeIisc(draft_, limits);
    return encoded.ok() ? std::move(encoded.bytes) : std::vector<std::uint8_t>{};
}
bool Artwork::replace(Document document)
{
    auto next = directory_ / (uuid() + ".iisc");
    auto staged = std::make_unique<DocumentFile>();
    auto created = staged->create(path_text(next), document);
    if (!created.ok())
        return fail(created.message);
    cancel();
    staged->close();
    file_.close();
    auto opened = file_.open(path_text(next));
    if (!opened.ok())
        return fail(opened.message);
    draft_ = *file_.document();
    recovery_ = next;
    source_.clear();
    creation_.reset();
    selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
    undo_.clear();
    redo_.clear();
    history_bytes_ = 0;
    bind();
    error_.clear();
    if (changed)
        changed();
    return true;
}
bool Artwork::create(int width, int height, bool infinite)
{
    if (width < 1 || height < 1 || width > 32768 || height > 32768 ||
        std::uint64_t(width) * height > 256ULL * 1024 * 1024)
        return fail("Canvas extent exceeds the document limit");
    Document document;
    document.extent = {width, height};
    document.infiniteCanvas.chunkSize = 256;
    if (infinite)
    {
        document.canvasMode = CanvasMode::Infinite;
        document.assets.emplace_back(ChunkedRasterAsset{"paint", {}});
    }
    else
        document.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(width, height, 0)});
    document.layers.emplace_back(
        StaticBitmapLayer{{"paint-layer", "Paint layer"}, StaticSource{"paint"}});
    return replace(std::move(document));
}
bool Artwork::create(const CanvasDefinition& value)
{
    if (value.pixel_width < 1 || value.pixel_height < 1 || value.pixel_width > 32768 ||
        value.pixel_height > 32768 ||
        std::uint64_t(value.pixel_width) * value.pixel_height > 268435456 ||
        (value.background != "White" && value.background != "Transparent" &&
         value.background != "Black"))
        return fail("Enter valid canvas dimensions and background");
    Document document;
    document.extent = {value.pixel_width, value.pixel_height};
    if (value.background != "Transparent")
    {
        VectorPath path;
        const double w = value.pixel_width, h = value.pixel_height;
        path.commands = {MoveTo{{0, 0}}, LineTo{{w, 0}}, LineTo{{w, h}}, LineTo{{0, h}},
                         ClosePath{}};
        path.fill = SolidPaint{value.background == "Black" ? 0xff000000U : 0xffffffffU};
        document.assets.emplace_back(VectorAsset{"canvas.background", document.extent, {path}});
        document.layers.emplace_back(StaticVectorLayer{{"canvas.background.layer", "Background"},
                                                       StaticSource{"canvas.background"}});
    }
    document.assets.emplace_back(
        RasterAsset{"paint", makeRasterLayer(value.pixel_width, value.pixel_height, 0)});
    document.layers.emplace_back(
        StaticBitmapLayer{{"paint-layer", "Paint layer"}, StaticSource{"paint"}});
    if (!replace(std::move(document)))
        return false;
    creation_ = value;
    if (changed)
        changed();
    return true;
}
bool Artwork::open(const std::filesystem::path& path)
{
    std::string prefix;
    try
    {
        auto reader = iiFileProvider::native::File::openRead(path);
        prefix = reader->read(16);
    }
    catch (const std::exception& error)
    {
        return fail(error.what());
    }
    Document doc;
    const bool native = prefix.starts_with(std::string_view{"IISC\r\n\x1a\n", 8}) ||
                        prefix.starts_with(std::string_view{"SQLite format 3\0", 16});
    if (native)
    {
        const auto read = DocumentFile::readSnapshot(path_text(path), doc);
        if (!read.ok())
            return fail(read.message.empty() ? "Cannot read the native document" : read.message);
        if (!replace(std::move(doc)))
            return false;
        source_ = path;
        return true;
    }
    const auto extension = lower(path.extension().string());
    if ((extension == ".vrc" || extension == ".iisc") && prefix.starts_with("CONGREGATIONRC"))
    {
        try
        {
            auto bytes = iiFileProvider::native::File::read(path, maximum_canvas_bytes);
            auto legacy = decode_canvas(bytes);
            if (!legacy)
                return fail(legacy.error());
            auto migrated = migrate_legacy_canvas(*legacy);
            if (!migrated)
                return fail(migrated.error());
            return replace(std::move(*migrated));
        }
        catch (const std::exception& error)
        {
            return fail(error.what());
        }
    }
    if (extension == ".psd" || extension == ".ora")
    {
        auto decoded = importLayeredDocument(path_text(path));
        if (!decoded.ok())
            return fail(decoded.result.message);
        if (!replace(std::move(decoded.document)))
            return false;
        return true;
    }
    auto bitmap = importBitmap(path_text(path));
    if (!bitmap.ok())
        return fail(bitmap.result.message);
    doc.extent = {bitmap.asset.pixels.width, bitmap.asset.pixels.height};
    doc.layers.emplace_back(StaticBitmapLayer{{"image-layer", path_text(path.filename())},
                                              StaticSource{bitmap.asset.id}});
    doc.assets.emplace_back(std::move(bitmap.asset));
    return replace(std::move(doc));
}
bool Artwork::recover(const std::filesystem::path& path)
{
    if (!open(path))
        return false;
    source_.clear();
    return true;
}
bool Artwork::save(const std::filesystem::path& path)
{
    if (stroke_active())
        return fail("Finish the current stroke before saving");
    auto encoded = encodeIisc(draft_);
    if (!encoded.ok())
        return fail("Cannot encode the canonical document");
    try
    {
        iiFileProvider::native::writeCompatible(
            path, {reinterpret_cast<const char*>(encoded.bytes.data()), encoded.bytes.size()});
        source_ = path;
        error_.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        return fail(e.what());
    }
}
bool Artwork::export_image(const std::filesystem::path& path, std::string format)
{
    if (stroke_active())
        return fail("Finish the current stroke before export");
    MediaIoResult result;
    if (format == "psd")
        result = exportPsd(draft_, path_text(path), {.overwrite = true});
    else
    {
        BitmapExportOptions options;
        options.format = std::move(format);
        options.overwrite = true;
        result = exportBitmapFrame(draft_, 0, path_text(path), options);
    }
    if (!result.ok())
        return fail(result.message);
    error_.clear();
    return true;
}
bool Artwork::bind()
{
    raster_.unbind();
    chunks_.unbind();
    auto* current = layer(draft_, selected_);
    if (!current)
        return false;
    auto* source = staticLayerSource(*current);
    if (!source)
        return false;
    auto* asset = findAsset(draft_, source->assetId);
    if (!asset)
        return false;
    if (std::holds_alternative<RasterAsset>(*asset))
        return raster_.bind(draft_, source->assetId);
    if (std::holds_alternative<ChunkedRasterAsset>(*asset))
        return chunks_.bind(draft_, source->assetId);
    return false;
}
bool Artwork::select(std::string key)
{
    if (stroke_active())
        cancel();
    if (!layer(draft_, key))
        return fail("Layer no longer exists");
    const bool different = selected_ != key;
    selected_ = std::move(key);
    bind();
    if (different && selection_changed)
        selection_changed();
    return true;
}
bool Artwork::commit(std::vector<std::uint8_t> before, std::string before_selection, bool history)
{
    auto committed = file_.edit(
        [&](Document& doc)
        {
            doc = draft_;
            return true;
        });
    if (!committed.ok())
    {
        draft_ = *file_.document();
        selected_ = std::move(before_selection);
        bind();
        return fail(committed.message);
    }
    if (!committed.changed)
    {
        selected_ = std::move(before_selection);
        bind();
        return true;
    }
    draft_ = *file_.document();
    bind();
    if (history)
    {
        auto after = capture();
        for (const auto& entry : redo_)
            history_bytes_ -= entry.before.size() + entry.after.size();
        redo_.clear();
        if (!before.empty() && !after.empty())
        {
            history_bytes_ += before.size() + after.size();
            undo_.push_back(
                {std::move(before), std::move(after), std::move(before_selection), selected_});
            while (history_bytes_ > HistoryBudget || undo_.size() > HistoryCount)
            {
                history_bytes_ -= undo_.front().before.size() + undo_.front().after.size();
                undo_.pop_front();
            }
        }
        else
        {
            undo_.clear();
            history_bytes_ = 0;
        }
    }
    error_.clear();
    if (changed)
        changed();
    return true;
}
bool Artwork::edit(const std::function<bool(DocumentEditor&)>& action)
{
    cancel();
    auto before = capture();
    auto selection = selected_;
    DocumentEditor editor(draft_);
    if (!action(editor))
    {
        auto failure = editor.lastResult().message;
        draft_ = *file_.document();
        selected_ = std::move(selection);
        bind();
        return fail(failure.empty() ? "Document edit was rejected" : failure);
    }
    return commit(std::move(before), std::move(selection));
}
bool Artwork::add_layer(std::string name)
{
    const auto asset = uuid(), key = uuid();
    return edit(
        [&](DocumentEditor& editor)
        {
            if (draft_.canvasMode == CanvasMode::Infinite)
            {
                if (!editor.insertChunkedRasterAsset({asset, {}}).ok())
                    return false;
            }
            else if (!editor
                          .insertRasterAsset(
                              asset, makeRasterLayer(draft_.extent.width, draft_.extent.height, 0))
                          .ok())
                return false;
            if (!editor.insertLayer(StaticBitmapLayer{{key, std::move(name)}, StaticSource{asset}})
                     .ok())
                return false;
            selected_ = key;
            return true;
        });
}
bool Artwork::duplicate_layer()
{
    cancel();
    auto* original = layer(draft_, selected_);
    if (!original)
        return false;
    auto copy = *original;
    auto& properties = layerProperties(copy);
    properties.id = uuid();
    properties.name += " copy";
    auto* source = staticLayerSource(copy);
    if (!source)
        return fail("Animated layer duplication needs an explicit frame");
    auto* original_asset = findAsset(draft_, source->assetId);
    if (!original_asset)
        return false;
    auto asset = *original_asset;
    std::visit(
        [&](auto& value)
        {
            value.id = uuid();
            source->assetId = value.id;
        },
        asset);
    return edit(
        [&](DocumentEditor& editor)
        {
            auto inserted = std::visit(
                [&](auto value) -> DocumentEditResult
                {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::same_as<T, RasterAsset>)
                        return editor.insertRasterAsset(std::move(value.id),
                                                        std::move(value.pixels));
                    else if constexpr (std::same_as<T, ChunkedRasterAsset>)
                        return editor.insertChunkedRasterAsset(std::move(value));
                    else if constexpr (std::same_as<T, VectorAsset>)
                        return editor.insertVectorAsset(std::move(value.id), value.viewport,
                                                        std::move(value.paths));
                    else
                        return {DocumentEditCode::InvalidArgument,
                                false,
                                {},
                                "Duplicate requires a bitmap or vector layer"};
                },
                asset);
            if (!inserted.ok())
                return false;
            const auto key = properties.id;
            auto result = editor.insertLayer(std::move(copy));
            if (result.ok())
                selected_ = key;
            return result.ok();
        });
}
bool Artwork::remove_layer()
{
    auto key = selected_;
    return edit(
        [&](DocumentEditor& editor)
        {
            auto* current = layer(draft_, key);
            const auto* source = current ? staticLayerSource(*current) : nullptr;
            const auto asset = source ? source->assetId : std::string{};
            if (!editor.removeLayer(key).ok())
                return false;
            if (!asset.empty())
            {
                // Frame content and masks may share the asset even when no
                // remaining layer has a matching static source.
                if (assetReferences(draft_, asset).empty() && !editor.removeAsset(asset).ok())
                    return false;
            }
            selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
            return true;
        });
}
bool Artwork::rename(std::string key, std::string name)
{
    return edit([&](DocumentEditor& e) { return e.setLayerName(key, std::move(name)).ok(); });
}
bool Artwork::visible(std::string key, bool value)
{
    return edit([&](DocumentEditor& e) { return e.setLayerVisible(key, value).ok(); });
}
bool Artwork::opacity(std::string key, double value)
{
    return edit([&](DocumentEditor& e)
                { return e.setLayerOpacity(key, std::clamp(value, 0., 1.)).ok(); });
}
bool Artwork::cycle_blend(std::string key)
{
    auto found = std::ranges::find_if(draft_.layers,
                                      [&](const auto& l) { return layerProperties(l).id == key; });
    if (found == draft_.layers.end())
        return fail("Layer not found");
    const auto mode = layerProperties(*found).blendMode;
    const auto next = mode == RasterBlendMode::SourceOver ? RasterBlendMode::Multiply
                      : mode == RasterBlendMode::Multiply ? RasterBlendMode::Screen
                                                          : RasterBlendMode::SourceOver;
    return edit([&](DocumentEditor& e) { return e.setLayerBlendMode(key, next).ok(); });
}
bool Artwork::timing(int rate, int frames)
{
    if (rate < 1 || rate > 240 || frames < 1)
        return fail("Invalid document timing");
    return edit(
        [&](DocumentEditor& e)
        {
            return e.setFrameRate({std::uint32_t(rate), 1}).ok() &&
                   e.setFrameCount(std::uint32_t(frames)).ok();
        });
}
bool Artwork::move(std::string key, std::string before)
{
    return edit(
        [&](DocumentEditor& e)
        {
            // The UI exposes the compositing stack from top to bottom.
            // "before" identifies the next row in that displayed order.
            std::size_t index = 0;
            if (!before.empty())
            {
                auto p = std::ranges::find_if(draft_.layers, [&](const auto& v)
                                              { return layerProperties(v).id == before; });
                if (p == draft_.layers.end())
                    return false;
                const auto source =
                    std::ranges::find_if(draft_.layers, [&](const auto& value)
                                         { return layerProperties(value).id == key; });
                if (source == draft_.layers.end())
                    return false;
                if (source == p)
                    return true;
                index = std::size_t(p - draft_.layers.begin()) + 1 - (source < p ? 1 : 0);
            }
            return e.moveLayer(key, index).ok();
        });
}
bool Artwork::transform(AffineTransform transform)
{
    return edit([&](DocumentEditor& e) { return e.setLayerTransform(selected_, transform).ok(); });
}
bool Artwork::transform(LayerTransform command)
{
    const auto* current = layer(draft_, selected_);
    if (!current)
        return fail("Layer not found");
    const auto old = layerProperties(*current).transform;
    auto next = old;
    if (command == LayerTransform::flip_horizontal)
    {
        next.m11 = -old.m11;
        next.m21 = -old.m21;
        next.translationX = draft_.extent.width - old.translationX;
    }
    else if (command == LayerTransform::flip_vertical)
    {
        next.m12 = -old.m12;
        next.m22 = -old.m22;
        next.translationY = draft_.extent.height - old.translationY;
    }
    else
    {
        const double direction = command == LayerTransform::rotate_left ? -1 : 1;
        next.m11 = -direction * old.m12;
        next.m21 = -direction * old.m22;
        next.m12 = direction * old.m11;
        next.m22 = direction * old.m21;
        const double x = draft_.extent.width / 2., y = draft_.extent.height / 2.;
        next.translationX = x - direction * (old.translationY - y);
        next.translationY = y + direction * (old.translationX - x);
    }
    return transform(next);
}
bool Artwork::import_pixels(RasterLayer pixels, std::string name, double x, double y)
{
    const auto asset = uuid(), key = uuid();
    return edit(
        [&](DocumentEditor& e)
        {
            if (!e.insertRasterAsset(asset, std::move(pixels)).ok())
                return false;
            LayerProperties properties{key, std::move(name)};
            properties.transform.translationX = x;
            properties.transform.translationY = y;
            if (!e.insertLayer(StaticBitmapLayer{std::move(properties), StaticSource{asset}}).ok())
                return false;
            selected_ = key;
            return true;
        });
}
bool Artwork::import_image(const std::filesystem::path& path, double x, double y)
{
    auto decoded = importBitmap(path_text(path));
    if (!decoded.ok())
        return fail(decoded.result.message);
    return import_pixels(std::move(decoded.asset.pixels), path_text(path.filename()), x, y);
}
bool Artwork::grow(DocumentPoint point, double radius)
{
    if (draft_.canvasMode != CanvasMode::Infinite)
        return true;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > 1e8 ||
        std::abs(point.y) > 1e8)
        return fail("Painting coordinates exceed the canvas range");
    DocumentEditor editor(draft_);
    auto result = editor.ensureInfiniteCanvasRegion(
        {{int(std::floor(point.x - radius)), int(std::floor(point.y - radius))},
         {int(std::ceil(radius * 2 + 2)), int(std::ceil(radius * 2 + 2))}});
    return result.ok() || fail(result.message);
}
bool Artwork::begin(DocumentPoint point, double pressure, BitmapBrush brush, FrameIndex frame)
{
    if (stroke_active())
        cancel();
    if (frame >= draft_.timeline.frameCount)
        return fail("The current frame is outside the document timeline");
    if (!bind())
        return fail("Select a bitmap layer before painting");
    const auto* selected = layer(draft_, selected_);
    if (!selected)
        return fail("Select a bitmap layer before painting");
    // Keep the exact displayed transform stable for every point in the stroke.
    stroke_transform_ = sampleLayerAt(draft_, *selected, frame).transform;
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted)
        return fail("Cannot paint through a degenerate layer transform");
    point = *converted;
    stroke_before_ = capture();
    stroke_selection_ = selected_;
    const auto rollback = [&](std::string error)
    {
        raster_.cancelStroke();
        chunks_.cancelStroke();
        draft_ = *file_.document();
        stroke_before_.clear();
        bind();
        return fail(std::move(error));
    };
    if (!grow(point, std::ceil(brush.size / 2) + 2))
        return rollback(error_);
    if (raster_.isBound())
    {
        if (!raster_.setBrush(brush) || !raster_.beginStroke(point, pressure))
            return rollback(raster_.lastError());
    }
    else if (!chunks_.setBrush(brush) || !chunks_.beginStroke(point, pressure))
        return rollback(chunks_.lastError());
    return true;
}

bool Artwork::extend(DocumentPoint point, double pressure)
{
    if (!stroke_active())
        return fail("No stroke is active");
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted)
        return fail("Cannot paint through a degenerate layer transform");
    point = *converted;
    if (!grow(point,
              std::ceil(chunks_.isBound() ? chunks_.brush().size / 2 : raster_.brush().size / 2) +
                  2))
        return false;
    const auto result = raster_.isBound() ? raster_.continueStroke(point, pressure)
                                          : chunks_.continueStroke(point, pressure);
    return result || fail(raster_.isBound() ? raster_.lastError() : chunks_.lastError());
}
bool Artwork::end(DocumentPoint point, double pressure)
{
    if (!stroke_active())
        return fail("No stroke is active");
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted)
    {
        cancel();
        return fail("Cannot paint through a degenerate layer transform");
    }
    point = *converted;
    if (!grow(point,
              std::ceil(chunks_.isBound() ? chunks_.brush().size / 2 : raster_.brush().size / 2) +
                  2))
    {
        cancel();
        return false;
    }
    const auto result =
        raster_.isBound() ? raster_.endStroke(point, pressure) : chunks_.endStroke(point, pressure);
    if (!result)
        return fail(raster_.isBound() ? raster_.lastError() : chunks_.lastError());
    return commit(std::move(stroke_before_), std::move(stroke_selection_));
}
void Artwork::cancel()
{
    if (!stroke_active())
        return;
    raster_.cancelStroke();
    chunks_.cancelStroke();
    draft_ = *file_.document();
    stroke_before_.clear();
    bind();
}
bool Artwork::shape(std::string kind, Point from, Point to, std::uint32_t color, double width,
                    bool filled)
{
    if (kind == "line")
        filled = false;
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) ||
        !std::isfinite(to.y) || !std::isfinite(width) || width < 0)
        return fail("Invalid shape geometry");
    const auto left = std::min(from.x, to.x), top = std::min(from.y, to.y),
               w = std::max(1., std::abs(to.x - from.x)), h = std::max(1., std::abs(to.y - from.y));
    if (w > 32768 || h > 32768)
        return fail("Shape extent exceeds the document limit");
    const auto padding = filled ? 0. : std::ceil(width / 2);
    auto path = shape_path(kind, w, h, color, width, filled);
    if (kind == "line")
    {
        path.commands = {MoveTo{{from.x - left, from.y - top}}, LineTo{{to.x - left, to.y - top}}};
    }
    for (auto& command : path.commands)
        std::visit(
            [&](auto& item)
            {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::same_as<T, MoveTo> || std::same_as<T, LineTo>)
                {
                    item.point.x += padding;
                    item.point.y += padding;
                }
                else if constexpr (std::same_as<T, QuadraticTo>)
                {
                    item.control.x += padding;
                    item.control.y += padding;
                    item.end.x += padding;
                    item.end.y += padding;
                }
                else if constexpr (std::same_as<T, CubicTo>)
                {
                    item.control1.x += padding;
                    item.control1.y += padding;
                    item.control2.x += padding;
                    item.control2.y += padding;
                    item.end.x += padding;
                    item.end.y += padding;
                }
            },
            command);
    const auto asset = uuid(), key = uuid();
    return edit(
        [&](DocumentEditor& e)
        {
            if (!e.insertVectorAsset(
                      asset, {int(std::ceil(w + 2 * padding)), int(std::ceil(h + 2 * padding))},
                      {std::move(path)})
                     .ok())
                return false;
            LayerProperties p{key, kind};
            p.transform.translationX = left - padding;
            p.transform.translationY = top - padding;
            if (!e.insertLayer(StaticVectorLayer{std::move(p), StaticSource{asset}}).ok())
                return false;
            selected_ = key;
            return true;
        });
}

bool Artwork::fill(std::uint32_t color)
{
    return shape("rectangle", {0, 0}, {double(draft_.extent.width), double(draft_.extent.height)},
                 color, 0, true);
}
bool Artwork::fill_at(Point point, std::uint32_t color, FrameIndex frame)
{
    if (frame >= draft_.timeline.frameCount)
        return fail("The current frame is outside the document timeline");
    const auto origin = canvasOrigin(draft_);
    if (point.x < origin.x || point.y < origin.y || point.x >= origin.x + draft_.extent.width ||
        point.y >= origin.y + draft_.extent.height)
        return false;
    if (std::uint64_t(draft_.extent.width) * draft_.extent.height > 64ULL * 1024 * 1024)
        return fail("Flood fill exceeds the 64 megapixel working budget");
    auto rendered = renderFrame(draft_, frame);
    if (!rendered.ok())
        return fail(rendered.message);
    auto mask =
        floodMask(rendered.pixels, {int(point.x - origin.x), int(point.y - origin.y)}, 0, true);
    if (!mask.ok())
        return fail(mask.error);
    auto pixels = makeRasterLayer(draft_.extent.width, draft_.extent.height, 0);
    for (std::size_t i = 0; i < pixels.pixels.size(); ++i)
    {
        const auto alpha = unsigned(color >> 24) * mask.mask.alpha[i] / 255;
        pixels.pixels[i] = (color & 0xffffff) | (alpha << 24);
    }
    return import_pixels(std::move(pixels), "Fill", origin.x, origin.y);
}
bool Artwork::apply_history(bool forward)
{
    cancel();
    auto& source = forward ? redo_ : undo_;
    auto& destination = forward ? undo_ : redo_;
    if (source.empty())
        return false;
    auto& entry = source.back();
    auto decoded = decodeIisc(forward ? entry.after : entry.before);
    if (!decoded.ok())
        return fail("History snapshot is invalid");
    auto previous_selection = selected_;
    draft_ = std::move(decoded.document);
    selected_ = forward ? entry.after_selection : entry.before_selection;
    if (!commit({}, std::move(previous_selection), false))
        return false;
    destination.push_back(std::move(entry));
    source.pop_back();
    return true;
}
bool Artwork::undo()
{
    return apply_history(false);
}
bool Artwork::redo()
{
    return apply_history(true);
}
std::string Artwork::snapshot() const
{
    auto encoded = encodeIisc(draft_);
    return encoded.ok() ? std::string(reinterpret_cast<const char*>(encoded.bytes.data()),
                                      encoded.bytes.size())
                        : std::string{};
}
bool Artwork::receive(std::string_view bytes)
{
    auto decoded = decodeIisc({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    if (!decoded.ok())
        return fail("Received document is invalid");
    cancel();
    auto before = capture();
    auto previous_selection = selected_;
    draft_ = std::move(decoded.document);
    selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
    return commit(std::move(before), std::move(previous_selection));
}
} // namespace congregation::native
