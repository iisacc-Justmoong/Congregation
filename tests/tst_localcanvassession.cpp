#include <QSignalSpy>
#include <QRandomGenerator>
#include <QUuid>
#include <QtTest>

#include "localcanvassession.h"
#include "nearbycongregationdiscovery.h"
#include "recentcanvascontainer.h"

namespace
{
QByteArray canvasSnapshot(const QByteArray& marker)
{
    RecentCanvasContainer container;
    container.sharedCanvasDocument = marker;
    container.backgroundLayerPresent = true;
    return encodeRecentCanvasContainer(container);
}
} // namespace

class tst_LocalCanvasSession : public QObject
{
    Q_OBJECT

  private slots:
    void hostExecutesClientCommandsAndPublishesVersionedSnapshots();
    void invitationDecisionControlsCanvasConnection();
    void invalidSnapshotsAndEndpointsAreRejected();
};

void tst_LocalCanvasSession::hostExecutesClientCommandsAndPublishesVersionedSnapshots()
{
    LocalCanvasSession host(nullptr);
    QSignalSpy hostSnapshotSpy(&host, &LocalCanvasSession::snapshotReceived);
    QSignalSpy hostEditCommandSpy(&host, &LocalCanvasSession::editCommandReceived);
    QVERIFY(host.startHosting(QStringLiteral("Host Artist")));
    QVERIFY(host.hosting());
    QVERIFY(host.listenPort() > 0);

    const QByteArray initialSnapshot = canvasSnapshot(QByteArrayLiteral("host-initial-iisc"));
    QVERIFY(!initialSnapshot.isEmpty());
    QVERIFY(host.publishSnapshot(initialSnapshot));
    QCOMPARE(host.revision(), quint64(1));

    LocalCanvasSession client(nullptr);
    QSignalSpy clientSnapshotSpy(&client, &LocalCanvasSession::snapshotReceived);
    QVERIFY(client.joinCanvasAt(host.sessionId(), QStringLiteral("127.0.0.1"), host.listenPort(),
                                QStringLiteral("Guest Artist")));
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(clientSnapshotSpy.size(), 1, 5000);
    QCOMPARE(clientSnapshotSpy.first().at(0).toByteArray(), initialSnapshot);
    QCOMPARE(client.revision(), quint64(1));

    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(client.participantCount(), 1, 5000);
    const QVariantMap hostViewOfGuest = host.participantProfiles().first().toMap();
    QCOMPARE(hostViewOfGuest.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Guest Artist"));
    QVERIFY(hostViewOfGuest.value(QStringLiteral("removable")).toBool());
    const QVariantMap clientViewOfHost = client.participantProfiles().first().toMap();
    QCOMPARE(clientViewOfHost.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Host Artist"));
    QVERIFY(clientViewOfHost.value(QStringLiteral("isHost")).toBool());
    QVERIFY(!client.currentUserIsHost());

    client.setLocalProfileName(QStringLiteral("Guest Renamed"));
    QTRY_COMPARE_WITH_TIMEOUT(
        host.participantProfiles().first().toMap().value(QStringLiteral("profileName")).toString(),
        QStringLiteral("Guest Renamed"), 5000);

    const QVariantMap fillCommand{{QStringLiteral("type"), QStringLiteral("fill")},
                                  {QStringLiteral("layerKey"), QStringLiteral("raster-canvas")},
                                  {QStringLiteral("x"), 24.0},
                                  {QStringLiteral("y"), 32.0},
                                  {QStringLiteral("color"), QStringLiteral("#336699")}};
    QVERIFY(client.submitEditCommand(fillCommand));
    QTRY_COMPARE_WITH_TIMEOUT(hostEditCommandSpy.size(), 1, 5000);
    QCOMPARE(hostEditCommandSpy.first().at(0).toMap(), fillCommand);
    QCOMPARE(hostEditCommandSpy.first().at(1).toString(), client.peerId());
    QCOMPARE(host.revision(), quint64(1));
    QCOMPARE(hostSnapshotSpy.size(), 0);

    const QByteArray hostAppliedSnapshot =
        canvasSnapshot(QByteArrayLiteral("host-applied-edit-iisc"));
    QVERIFY(host.publishSnapshot(hostAppliedSnapshot));
    QTRY_COMPARE_WITH_TIMEOUT(host.revision(), quint64(2), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(client.revision(), quint64(2), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(clientSnapshotSpy.size(), 2, 5000);
    QCOMPARE(clientSnapshotSpy.last().at(0).toByteArray(), hostAppliedSnapshot);

    const QByteArray forbiddenClientSnapshot =
        canvasSnapshot(QByteArrayLiteral("client-owned-snapshot-must-not-upload"));
    QVERIFY(!client.publishSnapshot(forbiddenClientSnapshot));
    QCOMPARE(host.revision(), quint64(2));
    QCOMPARE(hostSnapshotSpy.size(), 0);

    QVERIFY(!client.submitEditCommand(
        QVariantMap{{QStringLiteral("type"), QStringLiteral("unknown-command")}}));

    QVariantMap nonAsciiColorCommand = fillCommand;
    nonAsciiColorCommand.insert(QStringLiteral("color"), QStringLiteral("#１２３"));
    QVERIFY(!client.submitEditCommand(nonAsciiColorCommand));

    QVERIFY(host.removeParticipant(client.peerId()));
    QTRY_VERIFY_WITH_TIMEOUT(!client.active(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 0, 5000);
    QVERIFY(client.errorString().contains(QStringLiteral("removed"), Qt::CaseInsensitive));
}

void tst_LocalCanvasSession::invitationDecisionControlsCanvasConnection()
{
    NearbyCongregationDiscovery::Configuration configuration;
    configuration.port = static_cast<quint16>(49152 + QRandomGenerator::global()->bounded(12000));
    configuration.heartbeatIntervalMs = 100;
    configuration.peerTimeoutMs = 700;
    configuration.pruneIntervalMs = 50;
    configuration.ignoreLocalSenders = false;
    configuration.includeLoopbackInterfaces = true;

    NearbyCongregationDiscovery hostDiscovery(configuration);
    NearbyCongregationDiscovery guestDiscovery(configuration);
    LocalCanvasSession host(&hostDiscovery);
    LocalCanvasSession guest(&guestDiscovery);
    guest.setInvitationsAllowed(true);
    QSignalSpy pendingInvitationSpy(&guest, &LocalCanvasSession::pendingInvitationsChanged);

    hostDiscovery.start();
    guestDiscovery.start();
    QTRY_VERIFY_WITH_TIMEOUT(hostDiscovery.running(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(guestDiscovery.running(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.availableInvitees().size(), 1, 5000);

    QVERIFY(host.invitePeer(guestDiscovery.sessionId(), QStringLiteral("Host Artist")));
    QTRY_COMPARE_WITH_TIMEOUT(guest.pendingInvitationCount(), 1, 5000);
    QVERIFY(!pendingInvitationSpy.isEmpty());
    QCOMPARE(guest.pendingInvitation().value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Host Artist"));
    QCOMPARE(guest.pendingInvitation().value(QStringLiteral("sessionId")).toString(),
             hostDiscovery.sessionId());
    QVERIFY(host.hosting());

    QVERIFY(guest.respondToPendingInvitation(false, QStringLiteral("Guest Artist")));
    QCOMPARE(guest.pendingInvitationCount(), 0);
    QVERIFY(!guest.active());

    QVERIFY(host.invitePeer(guestDiscovery.sessionId(), QStringLiteral("Host Artist")));
    QTRY_COMPARE_WITH_TIMEOUT(guest.pendingInvitationCount(), 1, 5000);
    QVERIFY(guest.respondToPendingInvitation(true, QStringLiteral("Guest Artist")));
    QTRY_VERIFY_WITH_TIMEOUT(guest.connected(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 1, 5000);
    QCOMPARE(guest.pendingInvitationCount(), 0);

    hostDiscovery.stop();
    guestDiscovery.stop();
}

void tst_LocalCanvasSession::invalidSnapshotsAndEndpointsAreRejected()
{
    LocalCanvasSession session(nullptr);
    QVERIFY(!session.publishSnapshot(QByteArrayLiteral("not-a-congregation-canvas")));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.revision(), quint64(0));

    QVERIFY(!session.joinCanvasAt(QStringLiteral("not-a-session"), QStringLiteral("127.0.0.1"),
                                  12345, QStringLiteral("Guest")));
    QVERIFY(!session.joinCanvasAt(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                  QStringLiteral("239.255.1.1"), 12345, QStringLiteral("Guest")));
    QVERIFY(!session.joinCanvasAt(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                  QStringLiteral("203.0.113.1"), 12345, QStringLiteral("Guest")));
    QVERIFY(!session.active());
}

QTEST_GUILESS_MAIN(tst_LocalCanvasSession)

#include "tst_localcanvassession.moc"
