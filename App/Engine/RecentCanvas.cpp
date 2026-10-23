#include "RecentCanvas.h"
#include <iiFileProviderNative.h>
#include <set>
namespace congregation
{
namespace
{
const std::string magic = "CONGREGATIONRC\r\n\x1a\n";
constexpr std::size_t max_document = 256ULL * 1024 * 1024, max_manifest = 1024 * 1024;
std::string checksum(std::string_view data)
{
    auto hex = iiFileProvider::native::sha256(data);
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        out += static_cast<char>(digit(hex[i]) * 16 + digit(hex[i + 1]));
    }
    return out;
}
void block(std::string& out, std::string_view data)
{
    append_be(out, data.size(), 4);
    out += data;
}
bool read_block(std::string_view& in, std::string& out, std::size_t maximum)
{
    std::uint64_t size = 0;
    if (!take_be(in, size, 4) || size > maximum || size > in.size())
        return false;
    out = in.substr(0, size);
    in.remove_prefix(size);
    return true;
}
bool valid(const EmbeddedAsset& a)
{
    return a.objectId > 0 && (a.kind == 1 || a.kind == 2) && !a.png.empty() &&
           a.png.size() <= max_document;
}
} // namespace
std::expected<std::string, std::string> encode_canvas(const RecentCanvas& canvas)
{
    if (canvas.document.empty() || canvas.document.size() > max_document ||
        canvas.objects.size() > 256 || canvas.assets.size() > 256)
        return std::unexpected("invalid shared canvas document or object limit");
    auto manifest = json(Value(Object{{"schema", Value(1)},
                                      {"backgroundLayerPresent", Value(canvas.background)},
                                      {"drawableObjects", Value(canvas.objects)}}),
                         max_manifest);
    if (manifest.empty())
        return std::unexpected("invalid recent canvas manifest");
    std::string payload;
    block(payload, manifest);
    block(payload, canvas.document);
    append_be(payload, canvas.assets.size(), 4);
    std::set<std::pair<int, int>> seen;
    for (const auto& a : canvas.assets)
    {
        if (!valid(a) || !seen.emplace(a.objectId, a.kind).second)
            return std::unexpected("invalid embedded asset");
        if (payload.size() > maximum_canvas_bytes - magic.size() - 44 - 9 ||
            a.png.size() > maximum_canvas_bytes - magic.size() - 44 - 9 - payload.size())
            return std::unexpected("recent canvas payload is too large");
        append_be(payload, a.objectId, 4);
        append_be(payload, a.kind, 1);
        block(payload, a.png);
    }
    if (payload.size() > maximum_canvas_bytes - magic.size() - 44)
        return std::unexpected("recent canvas payload is too large");
    std::string result = magic;
    append_be(result, 1, 4);
    append_be(result, payload.size(), 8);
    result += checksum(payload);
    result += payload;
    return result;
}
std::expected<RecentCanvas, std::string> decode_canvas(std::string_view bytes)
{
    if (bytes.size() < magic.size() + 44 || bytes.size() > maximum_canvas_bytes ||
        !bytes.starts_with(magic))
        return std::unexpected("invalid recent canvas header");
    bytes.remove_prefix(magic.size());
    std::uint64_t version = 0, size = 0;
    if (!take_be(bytes, version, 4) || !take_be(bytes, size, 8) || version != 1 ||
        size != bytes.size() - 32)
        return std::unexpected("unsupported recent canvas version or size");
    const auto expected = bytes.substr(0, 32);
    bytes.remove_prefix(32);
    if (checksum(bytes) != expected)
        return std::unexpected("recent canvas checksum mismatch");
    RecentCanvas result;
    std::string manifest;
    std::uint64_t count = 0;
    if (!read_block(bytes, manifest, max_manifest) ||
        !read_block(bytes, result.document, max_document) || result.document.empty() ||
        !take_be(bytes, count, 4) || count > 256)
        return std::unexpected("invalid recent canvas payload");
    std::set<std::pair<int, int>> seen;
    for (std::uint64_t i = 0; i < count; ++i)
    {
        std::uint64_t id = 0, kind = 0;
        EmbeddedAsset a;
        if (!take_be(bytes, id, 4) || id > INT32_MAX || !take_be(bytes, kind, 1) ||
            !read_block(bytes, a.png, max_document))
            return std::unexpected("invalid embedded asset");
        a.objectId = static_cast<int>(id);
        a.kind = static_cast<std::uint8_t>(kind);
        if (!valid(a) || !seen.emplace(a.objectId, a.kind).second)
            return std::unexpected("invalid embedded asset");
        result.assets.push_back(std::move(a));
    }
    if (!bytes.empty())
        return std::unexpected("recent canvas payload has trailing data");
    auto tree = parse_json(manifest, max_manifest);
    if (!tree || number(field(*tree, "schema")) != 1 ||
        field(*tree, "backgroundLayerPresent").kind() != ii::core::ValueKind::boolean ||
        field(*tree, "drawableObjects").kind() != ii::core::ValueKind::array)
        return std::unexpected("unsupported recent canvas manifest");
    result.objects = array(field(*tree, "drawableObjects"));
    if (result.objects.size() > 256)
        return std::unexpected("recent canvas object limit exceeded");
    result.background = boolean(field(*tree, "backgroundLayerPresent"));
    return result;
}
} // namespace congregation
