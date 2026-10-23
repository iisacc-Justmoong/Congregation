#include "memberprofilelistbuilder.h"
#include "../../host/structuredvalues.h"
#include <Engine/DocumentMetadata.h>
MemberProfileListBuilder::MemberProfileListBuilder(QObject* parent) : QObject(parent)
{
}
QVariantList MemberProfileListBuilder::build(const QVariantList& collaborators, const QString& name,
                                             const QUrl& image, bool host) const
{
    auto native = congregation::member_profiles(
        congregation::array(congregation::presentation::fromVariant(collaborators)),
        name.toStdString(), image.toString().toStdString(), host);
    auto result =
        congregation::presentation::toVariant(congregation::Value(std::move(native))).toList();
    // QML's own image source retains its URL metatype at the presentation boundary.
    for (auto& value : result)
    {
        auto record = value.toMap();
        if (record.value(QStringLiteral("isMe")).toBool())
            record.insert(QStringLiteral("profileImageSource"), image);
        if (!record.value(QStringLiteral("isMe")).toBool())
        {
            const auto index = record.value(QStringLiteral("sourceIndex")).toInt();
            if (index >= 0 && index < collaborators.size())
            {
                auto original = collaborators[index].toMap();
                const auto source = original;
                for (const auto* key :
                     {"profileName", "displayName", "isHost", "isMe", "removable", "sourceIndex"})
                    original.insert(QLatin1String(key), record.value(QLatin1String(key)));
                original.insert(QStringLiteral("sourceProfile"), source);
                record = std::move(original);
            }
        }
        value = record;
    }
    return result;
}
