#pragma once
#include <Engine/Data.h>
#include <QVariant>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
namespace congregation::presentation
{
inline Value fromVariant(const QVariant& v)
{
    switch (v.typeId())
    {
    case QMetaType::Bool:
        return Value(v.toBool());
    case QMetaType::Int:
    case QMetaType::LongLong:
        return Value(v.toLongLong());
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return Value(v.toULongLong());
    case QMetaType::Double:
    case QMetaType::Float:
        return std::isfinite(v.toDouble()) ? Value(v.toDouble()) : Value{};
    case QMetaType::QUrl:
        return Value(v.toUrl().toString().toStdString());
    case QMetaType::QString:
        return Value(v.toString().toStdString());
    case QMetaType::QByteArray:
    {
        auto b = v.toByteArray();
        auto p = reinterpret_cast<const std::byte*>(b.constData());
        return Value(ii::core::Bytes(p, p + b.size()));
    }
    case QMetaType::QVariantList:
    {
        Array a;
        for (const auto& x : v.toList())
            a.push_back(fromVariant(x));
        return Value(std::move(a));
    }
    case QMetaType::QVariantMap:
    {
        Object o;
        const auto map = v.toMap();
        for (auto i = map.cbegin(); i != map.cend(); ++i)
            o.emplace(i.key().toStdString(), fromVariant(i.value()));
        return Value(std::move(o));
    }
    default:
        return {};
    }
}
inline QVariant toVariant(const Value& v)
{
    return std::visit(
        [](const auto& x) -> QVariant
        {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<T, std::string>)
                return QString::fromStdString(x);
            else if constexpr (std::is_same_v<T, ii::core::Bytes>)
                return QByteArray(reinterpret_cast<const char*>(x.data()), x.size());
            else if constexpr (std::is_same_v<T, Array>)
            {
                QVariantList out;
                for (const auto& v : x)
                    out.append(toVariant(v));
                return out;
            }
            else if constexpr (std::is_same_v<T, Object>)
            {
                QVariantMap out;
                for (const auto& [k, v] : x)
                    out.insert(QString::fromStdString(k), toVariant(v));
                return out;
            }
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return QVariant::fromValue<qlonglong>(x);
            else if constexpr (std::is_same_v<T, std::uint64_t>)
                return QVariant::fromValue<qulonglong>(x);
            else
                return QVariant::fromValue(x);
        },
        v.storage());
}
} // namespace congregation::presentation
