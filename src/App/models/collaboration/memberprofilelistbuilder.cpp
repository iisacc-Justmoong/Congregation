#include "memberprofilelistbuilder.h"

namespace
{

QString normalizedProfileName(const QString& profileName)
{
    const QString trimmedName = profileName.trimmed();
    return trimmedName.isEmpty() ? MemberProfileListBuilder::tr("Unnamed member") : trimmedName;
}

QString displayNameFor(const QString& profileName, bool isHost, bool isMe)
{
    if (isHost && isMe)
    {
        return MemberProfileListBuilder::tr("%1 (host, me)").arg(profileName);
    }
    if (isHost)
    {
        return MemberProfileListBuilder::tr("%1 (host)").arg(profileName);
    }
    if (isMe)
    {
        return MemberProfileListBuilder::tr("%1 (me)").arg(profileName);
    }
    return profileName;
}

} // namespace

MemberProfileListBuilder::MemberProfileListBuilder(QObject* parent) : QObject(parent)
{
}

QVariantList MemberProfileListBuilder::build(const QVariantList& collaboratorProfiles,
                                             const QString& currentProfileName,
                                             const QUrl& currentProfileImageSource,
                                             bool currentUserIsHost) const
{
    bool resolvedCurrentUserIsHost = currentUserIsHost;
    for (const QVariant& profileValue : collaboratorProfiles)
    {
        const QVariantMap profile = profileValue.toMap();
        if (profile.value(QStringLiteral("isMe")).toBool() &&
            profile.value(QStringLiteral("isHost")).toBool())
        {
            resolvedCurrentUserIsHost = true;
            break;
        }
    }

    QVariantList hostProfiles;
    QVariantList otherProfiles;
    bool remoteHostAssigned = false;
    for (qsizetype index = 0; index < collaboratorProfiles.size(); ++index)
    {
        const QVariantMap sourceProfile = collaboratorProfiles.at(index).toMap();
        if (sourceProfile.value(QStringLiteral("isMe")).toBool())
        {
            continue;
        }

        const bool isHost = !resolvedCurrentUserIsHost && !remoteHostAssigned &&
                            sourceProfile.value(QStringLiteral("isHost")).toBool();
        if (isHost)
        {
            remoteHostAssigned = true;
        }

        QVariantMap displayProfile = sourceProfile;
        const QString profileName =
            normalizedProfileName(sourceProfile.value(QStringLiteral("profileName")).toString());
        displayProfile.insert(QStringLiteral("profileName"), profileName);
        displayProfile.insert(QStringLiteral("displayName"),
                              displayNameFor(profileName, isHost, false));
        displayProfile.insert(QStringLiteral("isHost"), isHost);
        displayProfile.insert(QStringLiteral("isMe"), false);
        displayProfile.insert(QStringLiteral("removable"),
                              !isHost &&
                                  sourceProfile.value(QStringLiteral("removable"), true).toBool());
        displayProfile.insert(QStringLiteral("sourceIndex"), index);
        displayProfile.insert(QStringLiteral("sourceProfile"), sourceProfile);

        if (isHost)
        {
            hostProfiles.append(displayProfile);
        }
        else
        {
            otherProfiles.append(displayProfile);
        }
    }

    const QString ownProfileName = normalizedProfileName(currentProfileName);
    QVariantMap currentUserProfile{
        {QStringLiteral("profileName"), ownProfileName},
        {QStringLiteral("displayName"),
         displayNameFor(ownProfileName, resolvedCurrentUserIsHost, true)},
        {QStringLiteral("profileImageSource"), currentProfileImageSource},
        {QStringLiteral("isHost"), resolvedCurrentUserIsHost},
        {QStringLiteral("isMe"), true},
        {QStringLiteral("removable"), false},
        {QStringLiteral("sourceIndex"), -1},
    };

    QVariantList result;
    result.reserve(hostProfiles.size() + otherProfiles.size() + 1);
    if (resolvedCurrentUserIsHost)
    {
        result.append(currentUserProfile);
        result.append(hostProfiles);
    }
    else
    {
        result.append(hostProfiles);
        result.append(currentUserProfile);
    }
    result.append(otherProfiles);
    return result;
}
