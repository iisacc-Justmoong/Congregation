#include "DocumentMetadata.h"
#include <filesystem>
#include <set>
#include <libxml/parser.h>
namespace congregation
{
namespace
{
std::string first_line(std::string s)
{
    s = trim(std::move(s));
    s = s.substr(0, s.find('\n'));
    s = trim(std::move(s));
    std::size_t count = 0, offset = 0;
    while (offset < s.size() && count < 32)
    {
        auto c = static_cast<unsigned char>(s[offset]);
        auto n = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
        offset += n;
        count += c >= 240 ? 2 : 1;
    }
    if (offset < s.size())
        s.resize(offset);
    return s.empty() ? "Text" : s;
}
std::string name_for(const Object& o, int index)
{
    auto name = trim(text(field(o, "name")));
    if (!name.empty())
        return name;
    auto type = lower(trim(text(field(o, "type"))));
    if (type == "text")
        return first_line(text(field(o, "text")));
    if (type == "shape")
    {
        auto shape = trim(text(field(o, "shapeKind")));
        if (shape.empty())
            return "Shape";
        if (shape[0] >= 'a' && shape[0] <= 'z')
            shape[0] -= 32;
        return shape;
    }
    if (type == "image")
    {
        auto source = text(field(o, "source"));
        if (source.starts_with("file://"))
        {
            auto decoded = percent_decode(source);
            if (decoded && ii::core::valid_utf8(*decoded))
                source = std::move(*decoded);
        }
        const auto slash = source.find_last_of("/\\");
        auto name = slash == std::string::npos ? source : source.substr(slash + 1);
        return name.empty() ? "Image" : name;
    }
    return "Layer " + std::to_string(index + 1);
}
std::string blend(const Object& o)
{
    auto original = trim(text(field(o, "blendMode"))), s = lower(original);
    if (s == "norm" || s == "normal")
        return "norm";
    if (s == "mul" || s == "multiply")
        return "mul ";
    if (s == "scrn" || s == "screen")
        return "scrn";
    if (s == "over" || s == "overlay")
        return "over";
    if (s == "dark" || s == "darken")
        return "dark";
    if (s == "lite" || s == "lighten")
        return "lite";
    if (s == "diff" || s == "difference")
        return "diff";
    static const std::set<std::string> keys{"diss", "idiv", "lbrn", "dkCl", "div ", "lddg", "lgCl",
                                            "sLit", "hLit", "vLit", "lLit", "pLit", "hMix", "smud",
                                            "fsub", "fdiv", "hue ", "sat ", "colr", "lum "};
    return keys.contains(original) ? original : "norm";
}
int bounded_floor(double value, int lo, int hi)
{
    return static_cast<int>(
        std::clamp(std::floor(value), static_cast<double>(lo), static_cast<double>(hi)));
}
std::string display(std::string name, bool host, bool me)
{
    return name + (host ? (me ? " (host, me)" : " (host)") : (me ? " (me)" : ""));
}
} // namespace
Object psd_layer(const Object& o, int width, int height, int index, bool background)
{
    width = std::max(1, width);
    height = std::max(1, height);
    auto type = lower(trim(text(field(o, "type"))));
    if (type != "image" && type != "text" && type != "shape")
        type = "raster";
    double x = number(field(o, "x")), y = number(field(o, "y"));
    int left = background ? 0 : bounded_floor(x, 0, width - 1),
        top = background ? 0 : bounded_floor(y, 0, height - 1);
    int right = background ? width
                           : static_cast<int>(std::clamp(
                                 std::ceil(x + std::max(1.0, number(field(o, "width")))),
                                 static_cast<double>(left + 1), static_cast<double>(width)));
    int bottom = background ? height
                            : static_cast<int>(std::clamp(
                                  std::ceil(y + std::max(1.0, number(field(o, "height")))),
                                  static_cast<double>(top + 1), static_cast<double>(height)));
    auto opacity = o.contains("opacity") ? number(field(o, "opacity")) : 255;
    auto alpha = std::clamp(std::round(opacity <= 1 ? opacity * 255 : opacity), 0.0, 255.0);
    Object payload;
    if (o.contains("id"))
        payload.emplace("sourceObjectId", field(o, "id"));
    if (type == "image")
    {
        payload.emplace("source", Value(text(field(o, "source"))));
        payload.emplace("originalWidth",
                        Value(static_cast<int>(number(field(o, "originalWidth")))));
        payload.emplace("originalHeight",
                        Value(static_cast<int>(number(field(o, "originalHeight")))));
    }
    else if (type == "text")
    {
        payload.emplace("text", Value(text(field(o, "text"))));
        payload.emplace("fontPixelSize", Value(number(field(o, "fontPixelSize"))));
        payload.emplace("color", Value(text(field(o, "color"))));
    }
    else if (type == "shape")
    {
        payload.emplace("shapeKind", Value(text(field(o, "shapeKind"))));
        payload.emplace("color", Value(text(field(o, "color"))));
    }
    return {{"name", Value(background ? "Background" : name_for(o, index))},
            {"kind", Value(background ? "raster" : type)},
            {"top", Value(top)},
            {"left", Value(left)},
            {"bottom", Value(bottom)},
            {"right", Value(right)},
            {"width", Value(right - left)},
            {"height", Value(bottom - top)},
            {"blendModeKey", Value(background ? "norm" : blend(o))},
            {"opacity", Value(background ? 255 : static_cast<int>(alpha))},
            {"visible", Value(background || boolean(field(o, "visible"), true))},
            {"payload", Value(std::move(payload))}};
}
Object psd_manifest(int width, int height, Array layers)
{
    return {{"formatFamily", Value("psd")},
            {"compatibilityVersion", Value(1)},
            {"canvasWidth", Value(width)},
            {"canvasHeight", Value(height)},
            {"colorMode", Value(3)},
            {"bitsPerChannel", Value(8)},
            {"coordinateSystem", Value("psd-top-left-bottom-right-exclusive")},
            {"psdCanvasSizeCompatible", Value(width <= 30000 && height <= 30000)},
            {"layers", Value(std::move(layers))}};
}
std::string xmp_text(std::string_view bytes, std::string_view name)
{
    if (bytes.empty() || bytes.size() > 8 * 1024 * 1024)
        return {};
    auto* doc = xmlReadMemory(bytes.data(), static_cast<int>(bytes.size()), nullptr, nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
        return {};
    std::string result;
    std::function<void(xmlNode*)> visit = [&](xmlNode* node)
    {
        for (auto* n = node; n && result.empty(); n = n->next)
        {
            if (n->type == XML_ELEMENT_NODE &&
                std::string_view(reinterpret_cast<const char*>(n->name)) == name)
            {
                auto* value = xmlNodeGetContent(n);
                if (value)
                {
                    result = trim(reinterpret_cast<const char*>(value));
                    xmlFree(value);
                }
                return;
            }
            visit(n->children);
        }
    };
    visit(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);
    return result;
}
Array member_profiles(const Array& collaborators, std::string name, std::string image, bool host)
{
    for (const auto& p : collaborators)
        if (boolean(field(p, "isMe")) && boolean(field(p, "isHost")))
            host = true;
    Array hosts, others;
    bool remote = false;
    for (std::size_t i = 0; i < collaborators.size(); ++i)
    {
        auto p = object(collaborators[i]);
        if (boolean(field(p, "isMe")))
            continue;
        bool is_host = !host && !remote && boolean(field(p, "isHost"));
        remote |= is_host;
        auto n = trim(text(field(p, "profileName")));
        if (n.empty())
            n = "Unnamed member";
        auto out = p;
        out.insert_or_assign("profileName", Value(n));
        out.insert_or_assign("displayName", Value(display(n, is_host, false)));
        out.insert_or_assign("isHost", Value(is_host));
        out.insert_or_assign("isMe", Value(false));
        out.insert_or_assign("removable", Value(!is_host && boolean(field(p, "removable"), true)));
        out.insert_or_assign("sourceIndex", Value(static_cast<int>(i)));
        out.insert_or_assign("sourceProfile", Value(std::move(p)));
        (is_host ? hosts : others).emplace_back(std::move(out));
    }
    name = trim(std::move(name));
    if (name.empty())
        name = "Unnamed member";
    Value me(Object{{"profileName", Value(name)},
                    {"displayName", Value(display(name, host, true))},
                    {"profileImageSource", Value(image)},
                    {"isHost", Value(host)},
                    {"isMe", Value(true)},
                    {"removable", Value(false)},
                    {"sourceIndex", Value(-1)}});
    Array out;
    if (host)
        out.push_back(me);
    out.insert(out.end(), hosts.begin(), hosts.end());
    if (!host)
        out.push_back(std::move(me));
    out.insert(out.end(), others.begin(), others.end());
    return out;
}
} // namespace congregation
