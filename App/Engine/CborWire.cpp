#include "EditCommand.h"
#include <iiSDKCore/Core/Serialization/Serialization.h>
namespace congregation
{
namespace
{
namespace cbor = ii::serialization;
constexpr std::size_t maximum = 64ULL * 1024 * 1024;
cbor::Limits limits()
{
    cbor::Limits value;
    value.max_bytes = maximum;
    value.max_blob_bytes = maximum;
    value.max_items = 100000;
    value.max_container_items = 100000;
    value.max_allocation_bytes = 128ULL * 1024 * 1024;
    return value;
}
cbor::Result<void> write_value(cbor::Encoder& out, const Value& value, unsigned depth)
{
    if (depth > 64)
        return out.reject<void>(cbor::ErrorCode::limit_exceeded, "Command nesting limit");
    return std::visit(
        [&](const auto& v) -> cbor::Result<void>
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Array>)
            {
                if (auto r = out.array(v.size()); !r)
                    return r;
                for (const auto& child : v)
                    if (auto r = write_value(out, child, depth + 1); !r)
                        return r;
                return {};
            }
            else if constexpr (std::is_same_v<T, Object>)
            {
                if (auto r = out.map(v.size()); !r)
                    return r;
                std::vector<const Object::value_type*> sorted;
                for (const auto& entry : v)
                    sorted.push_back(&entry);
                std::sort(sorted.begin(), sorted.end(),
                          [](auto a, auto b)
                          {
                              return a->first.size() != b->first.size()
                                         ? a->first.size() < b->first.size()
                                         : a->first < b->first;
                          });
                for (const auto* entry : sorted)
                {
                    if (auto r = out.text(entry->first); !r)
                        return r;
                    if (auto r = write_value(out, entry->second, depth + 1); !r)
                        return r;
                }
                return {};
            }
            else
                return out.write(v);
        },
        value.storage());
}
std::optional<Value> read_value(cbor::Decoder& in, std::span<const std::byte> bytes, unsigned depth)
{
    if (depth > 64 || !in.remaining())
        return {};
    const auto lead = std::to_integer<unsigned>(bytes[in.position()]);
    const auto major = lead >> 5, info = lead & 31;
    if (major == 0)
    {
        auto n = in.integer<std::uint64_t>();
        if (!n)
            return {};
        return *n <= INT64_MAX ? Value(static_cast<std::int64_t>(*n)) : Value(*n);
    }
    if (major == 1)
    {
        auto n = in.integer<std::int64_t>();
        if (!n)
            return {};
        return Value(*n);
    }
    if (major == 2)
    {
        auto b = in.binary();
        if (!b)
            return {};
        return Value(std::move(*b));
    }
    if (major == 3)
    {
        auto s = in.text();
        if (!s)
            return {};
        return Value(std::move(*s));
    }
    if (major == 4)
    {
        auto size = in.array();
        if (!size || !in.allocate(*size, sizeof(Value)))
            return {};
        Array result;
        result.reserve(*size);
        for (std::size_t i = 0; i < *size; ++i)
        {
            auto child = read_value(in, bytes, depth + 1);
            if (!child)
                return {};
            result.push_back(std::move(*child));
        }
        return Value(std::move(result));
    }
    if (major == 5)
    {
        auto size = in.map();
        if (!size || !in.allocate(*size, sizeof(Object::value_type) + 3 * sizeof(void*)))
            return {};
        Object result;
        for (std::size_t i = 0; i < *size; ++i)
        {
            auto key = in.text();
            if (!key)
                return {};
            auto child = read_value(in, bytes, depth + 1);
            if (!child || !result.emplace(std::move(*key), std::move(*child)).second)
                return {};
        }
        return Value(std::move(result));
    }
    if (major == 7 && (info == 20 || info == 21))
    {
        auto b = in.boolean();
        if (!b)
            return {};
        return Value(*b);
    }
    if (major == 7 && info == 22)
    {
        if (!in.null())
            return {};
        return Value{};
    }
    if (major == 7 && info >= 25 && info <= 27)
    {
        auto d = in.floating();
        if (!d || !std::isfinite(*d))
            return {};
        return Value(*d);
    }
    return {};
}
} // namespace
std::string encode_command(const Object& command)
{
    auto normalized = normalize_command(command);
    if (!normalized)
        return {};
    cbor::Encoder out(limits());
    if (!write_value(out, Value(std::move(*normalized)), 0))
        return {};
    auto encoded = out.finish();
    return encoded ? std::string(reinterpret_cast<const char*>(encoded->data()), encoded->size())
                   : std::string{};
}
std::optional<Object> decode_command(std::string_view bytes)
{
    if (bytes.empty() || bytes.size() > maximum)
        return {};
    auto input = std::span(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
    if (!cbor::validate(input, limits()))
        return {};
    cbor::Decoder in(input, limits());
    auto value = read_value(in, input, 0);
    if (!value || !in.finish() || value->kind() != ii::core::ValueKind::object)
        return {};
    return normalize_command(object(*value));
}
} // namespace congregation
