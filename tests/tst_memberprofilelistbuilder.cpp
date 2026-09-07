#include "memberprofilelistbuilder.h"

#include <QtTest>

class tst_MemberProfileListBuilder : public QObject
{
    Q_OBJECT

  private slots:
    void localHostIsFirstAndMarkedAsHostAndMe();
    void remoteHostPrecedesCurrentUserAndOtherCollaborators();
    void sourceSelfEntryIsMergedWithoutDuplication();
    void unnamedCurrentUserStillHasAVisibleRow();
};

void tst_MemberProfileListBuilder::localHostIsFirstAndMarkedAsHostAndMe()
{
    MemberProfileListBuilder builder;
    const QVariantList members = builder.build({}, QStringLiteral("Congregation"),
                                               QUrl(QStringLiteral("file:///congregation.png")), true);

    QCOMPARE(members.size(), 1);
    const QVariantMap currentUser = members.at(0).toMap();
    QCOMPARE(currentUser.value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Congregation (host, me)"));
    QCOMPARE(currentUser.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Congregation"));
    QCOMPARE(currentUser.value(QStringLiteral("profileImageSource")).toUrl(),
             QUrl(QStringLiteral("file:///congregation.png")));
    QVERIFY(currentUser.value(QStringLiteral("isHost")).toBool());
    QVERIFY(currentUser.value(QStringLiteral("isMe")).toBool());
    QVERIFY(!currentUser.value(QStringLiteral("removable")).toBool());
    QCOMPARE(currentUser.value(QStringLiteral("sourceIndex")).toInt(), -1);
}

void tst_MemberProfileListBuilder::remoteHostPrecedesCurrentUserAndOtherCollaborators()
{
    MemberProfileListBuilder builder;
    const QVariantMap peer{
        {QStringLiteral("profileId"), QStringLiteral("peer")},
        {QStringLiteral("profileName"), QStringLiteral("Peer")},
    };
    const QVariantMap host{
        {QStringLiteral("profileId"), QStringLiteral("host")},
        {QStringLiteral("profileName"), QStringLiteral("Canvas owner")},
        {QStringLiteral("isHost"), true},
        {QStringLiteral("removable"), true},
    };

    const QVariantList members =
        builder.build({peer, host}, QStringLiteral("Congregation"), QUrl{}, false);

    QCOMPARE(members.size(), 3);
    const QVariantMap first = members.at(0).toMap();
    const QVariantMap second = members.at(1).toMap();
    const QVariantMap third = members.at(2).toMap();
    QCOMPARE(first.value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Canvas owner (host)"));
    QVERIFY(first.value(QStringLiteral("isHost")).toBool());
    QVERIFY(!first.value(QStringLiteral("isMe")).toBool());
    QVERIFY(!first.value(QStringLiteral("removable")).toBool());
    QCOMPARE(first.value(QStringLiteral("sourceIndex")).toInt(), 1);
    QCOMPARE(second.value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Congregation (me)"));
    QVERIFY(!second.value(QStringLiteral("isHost")).toBool());
    QVERIFY(second.value(QStringLiteral("isMe")).toBool());
    QCOMPARE(third.value(QStringLiteral("displayName")).toString(), QStringLiteral("Peer"));
    QCOMPARE(third.value(QStringLiteral("sourceIndex")).toInt(), 0);
    QCOMPARE(third.value(QStringLiteral("sourceProfile")).toMap(), peer);
}

void tst_MemberProfileListBuilder::sourceSelfEntryIsMergedWithoutDuplication()
{
    MemberProfileListBuilder builder;
    const QVariantMap staleSelf{
        {QStringLiteral("profileName"), QStringLiteral("Stale name")},
        {QStringLiteral("isMe"), true},
        {QStringLiteral("isHost"), true},
    };
    const QVariantMap peer{
        {QStringLiteral("profileName"), QStringLiteral("Peer")},
    };

    const QVariantList members =
        builder.build({staleSelf, peer}, QStringLiteral("Current name"), QUrl{}, false);

    QCOMPARE(members.size(), 2);
    QCOMPARE(members.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Current name (host, me)"));
    QCOMPARE(members.at(1).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Peer"));
}

void tst_MemberProfileListBuilder::unnamedCurrentUserStillHasAVisibleRow()
{
    MemberProfileListBuilder builder;

    const QVariantList members = builder.build({}, QStringLiteral("   "), QUrl{}, true);

    QCOMPARE(members.size(), 1);
    QCOMPARE(members.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Unnamed member (host, me)"));
}

QTEST_GUILESS_MAIN(tst_MemberProfileListBuilder)

#include "tst_memberprofilelistbuilder.moc"
