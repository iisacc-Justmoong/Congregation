#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

class MemberProfileListBuilder final : public QObject
{
    Q_OBJECT

  public:
    explicit MemberProfileListBuilder(QObject* parent = nullptr);

    Q_INVOKABLE QVariantList build(const QVariantList& collaboratorProfiles,
                                   const QString& currentProfileName,
                                   const QUrl& currentProfileImageSource,
                                   bool currentUserIsHost) const;
};
