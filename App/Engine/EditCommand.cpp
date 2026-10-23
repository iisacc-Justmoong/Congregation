#include <Serialization/IiscCodec.h>
#include "EditCommand.h"
#include <set>
#include <limits>
#include <charconv>
namespace congregation
{
namespace
{
constexpr std::size_t maximumEditCommandPayloadBytes = 64ULL * 1024 * 1024,
                      maximumStrokePointCount = 4096, maximumLayerOrderCount = 4096,
                      maximumTextLength = 1024 * 1024;
bool hasExactKeys(const Object& map, std::initializer_list<const char*> keys)
{
    if (map.size() != keys.size())
        return false;
    return std::all_of(keys.begin(), keys.end(), [&](auto k) { return map.contains(k); });
}
std::optional<double> boundedNumber(const Value& v, double lo, double hi)
{
    double n = number(v, std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(n) || n < lo || n > hi)
        return {};
    return n;
}
std::optional<int> boundedInteger(const Value& v, int lo, int hi)
{
    auto n = boundedNumber(v, lo, hi);
    if (!n || std::floor(*n) != *n)
        return {};
    return static_cast<int>(*n);
}
std::optional<bool> strictBoolean(const Value& v)
{
    auto b = v.get<bool>();
    if (!b)
        return {};
    return b->get();
}
std::optional<std::string> boundedString(const Value& v, std::size_t maximum,
                                         bool allowEmpty = false)
{
    auto p = v.get<std::string>();
    if (!p || (!allowEmpty && p->get().empty()) || !ii::core::valid_utf8(p->get()))
        return {};
    // Preserve the former QString limits while storing UTF-8 in the engine.
    // A four-byte UTF-8 scalar occupies two UTF-16 code units.
    std::size_t units = 0;
    for (unsigned char byte : p->get())
    {
        if (byte < 0x80 || byte >= 0xc0)
        {
            units += byte >= 0xf0 ? 2 : 1;
            if (units > maximum)
                return {};
        }
    }
    return p->get();
}
ii::core::Bytes bytes(const Value& v)
{
    auto b = v.get<ii::core::Bytes>();
    return b ? b->get() : ii::core::Bytes{};
}
std::optional<std::string> normalizedLayerKey(const Value& v)
{
    auto s = boundedString(v, 32);
    if (!s)
        return {};
    if (*s == "raster-canvas")
        return s;
    if (!s->starts_with("object-"))
        return {};
    int id = 0;
    auto [end, error] = std::from_chars(s->data() + 7, s->data() + s->size(), id);
    if (error != std::errc{} || end != s->data() + s->size() || id <= 0 ||
        *s != "object-" + std::to_string(id))
        return {};
    return s;
}
std::optional<std::string> normalizedColor(const Value& v)
{
    auto s = boundedString(v, 9);
    if (!s || (*s)[0] != '#' || (s->size() != 4 && s->size() != 7 && s->size() != 9))
        return {};
    for (std::size_t i = 1; i < s->size(); ++i)
        if (!std::isxdigit(static_cast<unsigned char>((*s)[i])))
            return {};
    return lower(*s);
}
} // namespace
std::optional<Object> normalize_command(const Object& command)
{
    const std::optional<std::string> type = boundedString(field(command, "type"), 32);
    if (!type)
    {
        return std::nullopt;
    }

    if (*type == "nativeDocument")
    {
        if(!hasExactKeys(command,{"type","document","baseRevision"}))return std::nullopt;
        const auto& version=field(command,"baseRevision");auto unsigned_version=version.get<std::uint64_t>();auto signed_version=version.get<std::int64_t>();if(!unsigned_version&&(!signed_version||signed_version->get()<0))return std::nullopt;
        auto encoded=boundedString(field(command,"document"),maximumEditCommandPayloadBytes);
        if(!encoded)return std::nullopt;
        auto decoded=unbase64(*encoded);
        if(!decoded||decoded->size()>maximumEditCommandPayloadBytes)return std::nullopt;
        if(!iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(decoded->data()),decoded->size()}).ok())return std::nullopt;
        return Object{{"type",Value(*type)},{"document",Value(*encoded)},{"baseRevision",version}};
    }

    if (*type == "fill")
    {
        const std::optional<std::string> layerKey = normalizedLayerKey(field(command, "layerKey"));
        const std::optional<double> x = boundedNumber(field(command, "x"), -10000000.0, 10000000.0);
        const std::optional<double> y = boundedNumber(field(command, "y"), -10000000.0, 10000000.0);
        const std::optional<std::string> color = normalizedColor(field(command, "color"));
        if (!hasExactKeys(command, {"type", "layerKey", "x", "y", "color"}) || !layerKey || !x ||
            !y || !color)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},
                      {"layerKey", Value(*layerKey)},
                      {"x", Value(*x)},
                      {"y", Value(*y)},
                      {"color", Value(*color)}};
    }

    if (*type == "stroke")
    {
        if (!hasExactKeys(command, {"type", "layerKey", "tool", "color", "size", "flow", "opacity",
                                    "hardness", "spacing", "spacingRatio", "pressureMinimum",
                                    "pressureCenter", "pressureMaximum", "pressureOpacity",
                                    "stabilizer", "pressureSensitive", "points"}))
        {
            return std::nullopt;
        }
        const std::optional<std::string> layerKey = normalizedLayerKey(field(command, "layerKey"));
        const std::optional<std::string> tool = boundedString(field(command, "tool"), 8);
        const std::optional<std::string> color = normalizedColor(field(command, "color"));
        const std::optional<double> size = boundedNumber(field(command, "size"), 0.01, 4096.0);
        const std::optional<double> flow = boundedNumber(field(command, "flow"), 0.0, 1.0);
        const std::optional<double> opacity = boundedNumber(field(command, "opacity"), 0.0, 1.0);
        const std::optional<double> hardness = boundedNumber(field(command, "hardness"), 0.0, 1.0);
        const std::optional<double> spacing = boundedNumber(field(command, "spacing"), 0.0, 4096.0);
        const std::optional<double> spacingRatio =
            boundedNumber(field(command, "spacingRatio"), 0.0, 1.0);
        const std::optional<double> pressureMinimum =
            boundedNumber(field(command, "pressureMinimum"), 0.0, 1.0);
        const std::optional<double> pressureCenter =
            boundedNumber(field(command, "pressureCenter"), 0.0, 1.0);
        const std::optional<double> pressureMaximum =
            boundedNumber(field(command, "pressureMaximum"), 0.0, 1.0);
        const std::optional<bool> pressureOpacity =
            strictBoolean(field(command, "pressureOpacity"));
        const std::optional<double> stabilizer =
            boundedNumber(field(command, "stabilizer"), 0.0, 1.0);
        const std::optional<bool> pressureSensitive =
            strictBoolean(field(command, "pressureSensitive"));
        const Array points = array(field(command, "points"));
        if (!layerKey || !tool || (*tool != "brush" && *tool != "eraser") || !color || !size ||
            !flow || !opacity || !hardness || !spacing || !spacingRatio || !pressureMinimum ||
            !pressureCenter || !pressureMaximum || !pressureOpacity || !stabilizer ||
            !pressureSensitive || *pressureMinimum > *pressureCenter ||
            *pressureCenter > *pressureMaximum || points.size() < 2 ||
            points.size() > maximumStrokePointCount)
        {
            return std::nullopt;
        }

        Array normalizedPoints;
        normalizedPoints.reserve(points.size());
        for (const Value& pointValue : points)
        {
            const Object point = object(pointValue);
            const std::optional<double> x =
                boundedNumber(field(point, "x"), -10000000.0, 10000000.0);
            const std::optional<double> y =
                boundedNumber(field(point, "y"), -10000000.0, 10000000.0);
            const std::optional<double> pressure =
                boundedNumber(field(point, "pressure"), 0.0, 1.0);
            if (!hasExactKeys(point, {"x", "y", "pressure"}) || !x || !y || !pressure)
            {
                return std::nullopt;
            }
            normalizedPoints.emplace_back(
                Object{{"x", Value(*x)}, {"y", Value(*y)}, {"pressure", Value(*pressure)}});
        }
        return Object{{"type", Value(*type)},
                      {"layerKey", Value(*layerKey)},
                      {"tool", Value(*tool)},
                      {"color", Value(*color)},
                      {"size", Value(*size)},
                      {"flow", Value(*flow)},
                      {"opacity", Value(*opacity)},
                      {"hardness", Value(*hardness)},
                      {"spacing", Value(*spacing)},
                      {"spacingRatio", Value(*spacingRatio)},
                      {"pressureMinimum", Value(*pressureMinimum)},
                      {"pressureCenter", Value(*pressureCenter)},
                      {"pressureMaximum", Value(*pressureMaximum)},
                      {"pressureOpacity", Value(*pressureOpacity)},
                      {"stabilizer", Value(*stabilizer)},
                      {"pressureSensitive", Value(*pressureSensitive)},
                      {"points", Value(normalizedPoints)}};
    }

    if (*type == "add-text")
    {
        const std::optional<double> x = boundedNumber(field(command, "x"), -10000000.0, 10000000.0);
        const std::optional<double> y = boundedNumber(field(command, "y"), -10000000.0, 10000000.0);
        const std::optional<double> width = boundedNumber(field(command, "width"), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(field(command, "height"), 1.0, 10000000.0);
        const std::optional<std::string> text =
            boundedString(field(command, "text"), maximumTextLength);
        const std::optional<double> fontSize =
            boundedNumber(field(command, "fontSize"), 1.0, 4096.0);
        const std::optional<std::string> color = normalizedColor(field(command, "color"));
        if (!hasExactKeys(command,
                          {"type", "x", "y", "width", "height", "text", "fontSize", "color"}) ||
            !x || !y || !width || !height || !text || !fontSize || !color)
        {
            return std::nullopt;
        }
        return Object{
            {"type", Value(*type)},         {"x", Value(*x)},           {"y", Value(*y)},
            {"width", Value(*width)},       {"height", Value(*height)}, {"text", Value(*text)},
            {"fontSize", Value(*fontSize)}, {"color", Value(*color)}};
    }

    if (*type == "add-shape")
    {
        const std::optional<double> x = boundedNumber(field(command, "x"), -10000000.0, 10000000.0);
        const std::optional<double> y = boundedNumber(field(command, "y"), -10000000.0, 10000000.0);
        const std::optional<double> width = boundedNumber(field(command, "width"), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(field(command, "height"), 1.0, 10000000.0);
        const std::optional<std::string> shape = boundedString(field(command, "shape"), 32);
        const std::optional<std::string> color = normalizedColor(field(command, "color"));
        static const std::set<std::string> validShapes{"rectangle",    "ellipse", "triangle",
                                                       "diamond",      "star",    "rectanglebubble",
                                                       "ellipsebubble"};
        if (!hasExactKeys(command, {"type", "x", "y", "width", "height", "shape", "color"}) || !x ||
            !y || !width || !height || !shape || !validShapes.contains(*shape) || !color)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},     {"x", Value(*x)},
                      {"y", Value(*y)},           {"width", Value(*width)},
                      {"height", Value(*height)}, {"shape", Value(*shape)},
                      {"color", Value(*color)}};
    }

    if (*type == "transform-object")
    {
        const std::optional<int> objectId =
            boundedInteger(field(command, "objectId"), 1, std::numeric_limits<int>::max());
        const std::optional<double> x = boundedNumber(field(command, "x"), -10000000.0, 10000000.0);
        const std::optional<double> y = boundedNumber(field(command, "y"), -10000000.0, 10000000.0);
        const std::optional<double> width = boundedNumber(field(command, "width"), 1.0, 10000000.0);
        const std::optional<double> height =
            boundedNumber(field(command, "height"), 1.0, 10000000.0);
        if (!hasExactKeys(command, {"type", "objectId", "x", "y", "width", "height"}) ||
            !objectId || !x || !y || !width || !height)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},   {"objectId", Value(*objectId)},
                      {"x", Value(*x)},         {"y", Value(*y)},
                      {"width", Value(*width)}, {"height", Value(*height)}};
    }

    if (*type == "new-canvas")
    {
        const std::optional<int> width = boundedInteger(field(command, "width"), 1, 8192);
        const std::optional<int> height = boundedInteger(field(command, "height"), 1, 8192);
        const std::optional<bool> infinite = strictBoolean(field(command, "infinite"));
        if (!hasExactKeys(command, {"type", "width", "height", "infinite"}) || !width || !height ||
            !infinite)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},
                      {"width", Value(*width)},
                      {"height", Value(*height)},
                      {"infinite", Value(*infinite)}};
    }

    if (*type == "ensure-region")
    {
        const std::optional<int> x =
            boundedInteger(field(command, "x"), std::numeric_limits<int>::min() + 1,
                           std::numeric_limits<int>::max());
        const std::optional<int> y =
            boundedInteger(field(command, "y"), std::numeric_limits<int>::min() + 1,
                           std::numeric_limits<int>::max());
        const std::optional<int> width = boundedInteger(field(command, "width"), 1, 10000000);
        const std::optional<int> height = boundedInteger(field(command, "height"), 1, 10000000);
        if (!hasExactKeys(command, {"type", "x", "y", "width", "height"}) || !x || !y || !width ||
            !height)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},
                      {"x", Value(*x)},
                      {"y", Value(*y)},
                      {"width", Value(*width)},
                      {"height", Value(*height)}};
    }

    if (*type == "clear-canvas" || *type == "add-layer")
    {
        return hasExactKeys(command, {"type"})
                   ? std::optional<Object>(Object{{"type", Value(*type)}})
                   : std::nullopt;
    }

    if (*type == "delete-layer" || *type == "undo" || *type == "redo")
    {
        const std::optional<std::string> layerKey = normalizedLayerKey(field(command, "layerKey"));
        if (!hasExactKeys(command, {"type", "layerKey"}) || !layerKey)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)}, {"layerKey", Value(*layerKey)}};
    }

    if (*type == "rename-layer")
    {
        const std::optional<std::string> layerKey = normalizedLayerKey(field(command, "layerKey"));
        const std::optional<std::string> name = boundedString(field(command, "name"), 256);
        if (!hasExactKeys(command, {"type", "layerKey", "name"}) || !layerKey || !name ||
            trim(*name).empty())
        {
            return std::nullopt;
        }
        return Object{
            {"type", Value(*type)}, {"layerKey", Value(*layerKey)}, {"name", Value(trim(*name))}};
    }

    if (*type == "reorder-layers")
    {
        const Array layerKeys = array(field(command, "layerKeys"));
        if (!hasExactKeys(command, {"type", "layerKeys"}) || layerKeys.empty() ||
            layerKeys.size() > maximumLayerOrderCount)
        {
            return std::nullopt;
        }
        Array normalizedKeys;
        std::set<std::string> seenKeys;
        for (const Value& keyValue : layerKeys)
        {
            const std::optional<std::string> key = normalizedLayerKey(keyValue);
            if (!key || seenKeys.contains(*key))
            {
                return std::nullopt;
            }
            seenKeys.insert(*key);
            normalizedKeys.emplace_back(*key);
        }
        return Object{{"type", Value(*type)}, {"layerKeys", Value(normalizedKeys)}};
    }

    if (*type == "insert-image")
    {
        const ii::core::Bytes data = bytes(field(command, "data"));
        const std::optional<double> x = boundedNumber(field(command, "x"), -10000000.0, 10000000.0);
        const std::optional<double> y = boundedNumber(field(command, "y"), -10000000.0, 10000000.0);
        const std::optional<bool> positioned = strictBoolean(field(command, "positioned"));
        const std::optional<std::string> name = boundedString(field(command, "name"), 256);
        if (!hasExactKeys(command, {"type", "data", "x", "y", "positioned", "name"}) ||
            field(command, "data").kind() != ii::core::ValueKind::bytes || data.empty() ||
            data.size() > maximumEditCommandPayloadBytes || !x || !y || !positioned || !name)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)},
                      {"data", Value(data)},
                      {"x", Value(*x)},
                      {"y", Value(*y)},
                      {"positioned", Value(*positioned)},
                      {"name", Value(*name)}};
    }

    if (*type == "open-raster")
    {
        const ii::core::Bytes data = bytes(field(command, "data"));
        if (!hasExactKeys(command, {"type", "data"}) ||
            field(command, "data").kind() != ii::core::ValueKind::bytes || data.empty() ||
            data.size() > maximumEditCommandPayloadBytes)
        {
            return std::nullopt;
        }
        return Object{{"type", Value(*type)}, {"data", Value(data)}};
    }

    return std::nullopt;
}

} // namespace congregation
