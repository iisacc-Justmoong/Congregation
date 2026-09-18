#include "nearbycongregationdiscovery.h"

#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QUdpSocket>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace
{
constexpr int protocolVersion = 1;
constexpr qsizetype maximumTrackedSessions = 256;
constexpr qsizetype maximumInvitationProfileNameLength = 80;

NearbyCongregationDiscovery::Configuration
normalizedConfiguration(NearbyCongregationDiscovery::Configuration configuration)
{
    const NearbyCongregationDiscovery::Configuration defaults;
    if (configuration.multicastGroup.protocol() != QAbstractSocket::IPv4Protocol ||
        !configuration.multicastGroup.isMulticast())
    {
        configuration.multicastGroup = defaults.multicastGroup;
    }
    if (configuration.port == 0)
    {
        configuration.port = defaults.port;
    }
    configuration.heartbeatIntervalMs = qMax(50, configuration.heartbeatIntervalMs);
    configuration.peerTimeoutMs =
        qMax(configuration.heartbeatIntervalMs * 2, configuration.peerTimeoutMs);
    configuration.pruneIntervalMs =
        qBound(25, configuration.pruneIntervalMs, configuration.peerTimeoutMs);
    return configuration;
}

bool isCanonicalSessionId(const QString& sessionId)
{
    const QUuid uuid(sessionId);
    return !uuid.isNull() &&
           uuid.toString(QUuid::WithoutBraces).compare(sessionId, Qt::CaseInsensitive) == 0;
}

bool isUsableMulticastInterface(const QNetworkInterface& networkInterface,
                                bool includeLoopbackInterfaces)
{
    const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
    if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
        !flags.testFlag(QNetworkInterface::CanMulticast) ||
        (!includeLoopbackInterfaces && flags.testFlag(QNetworkInterface::IsLoopBack)))
    {
        return false;
    }

    for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
    {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
        {
            return true;
        }
    }
    return false;
}
} // namespace

QString NearbyCongregationProtocol::serviceName()
{
    return QStringLiteral("com.iisacc.congregation.nearby");
}

QByteArray NearbyCongregationProtocol::encodePresence(const QString& sessionId, bool online,
                                                 quint16 canvasPort, bool invitationsAllowed)
{
    if (!isCanonicalSessionId(sessionId))
    {
        return {};
    }

    QJsonObject object{
        {QStringLiteral("service"), serviceName()},
        {QStringLiteral("version"), protocolVersion},
        {QStringLiteral("session"), sessionId.toLower()},
        {QStringLiteral("state"), online ? QStringLiteral("online") : QStringLiteral("offline")}};
    if (online && canvasPort > 0)
    {
        object.insert(QStringLiteral("canvasPort"), canvasPort);
    }
    if (online && invitationsAllowed)
    {
        object.insert(QStringLiteral("invitationsAllowed"), true);
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<NearbyCongregationPresence>
NearbyCongregationProtocol::decodePresence(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumDatagramSize())
    {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    const QJsonValue versionValue = object.value(QStringLiteral("version"));
    if (object.value(QStringLiteral("service")).toString() != serviceName() ||
        !versionValue.isDouble() || versionValue.toDouble() != protocolVersion)
    {
        return std::nullopt;
    }

    const QString sessionId = object.value(QStringLiteral("session")).toString();
    if (!isCanonicalSessionId(sessionId))
    {
        return std::nullopt;
    }

    const QString state = object.value(QStringLiteral("state")).toString();
    if (state != QStringLiteral("online") && state != QStringLiteral("offline"))
    {
        return std::nullopt;
    }

    quint16 canvasPort = 0;
    const QJsonValue canvasPortValue = object.value(QStringLiteral("canvasPort"));
    if (!canvasPortValue.isUndefined())
    {
        const double rawCanvasPort = canvasPortValue.toDouble(-1);
        if (!canvasPortValue.isDouble() || rawCanvasPort < 1 || rawCanvasPort > 65535 ||
            rawCanvasPort != static_cast<int>(rawCanvasPort))
        {
            return std::nullopt;
        }
        canvasPort = static_cast<quint16>(rawCanvasPort);
    }

    bool invitationsAllowed = false;
    const QJsonValue invitationsAllowedValue = object.value(QStringLiteral("invitationsAllowed"));
    if (!invitationsAllowedValue.isUndefined())
    {
        if (!invitationsAllowedValue.isBool())
        {
            return std::nullopt;
        }
        invitationsAllowed = invitationsAllowedValue.toBool();
    }

    const bool online = state == QStringLiteral("online");
    return NearbyCongregationPresence{sessionId.toLower(), online,
                                 online ? canvasPort : static_cast<quint16>(0),
                                 online && invitationsAllowed};
}

QByteArray NearbyCongregationProtocol::encodeInvitation(const QString& invitationId,
                                                   const QString& senderSessionId,
                                                   const QString& targetSessionId,
                                                   quint16 canvasPort,
                                                   const QString& inviterProfileName)
{
    const QString normalizedProfileName =
        inviterProfileName.simplified().left(maximumInvitationProfileNameLength);
    if (!isCanonicalSessionId(invitationId) || !isCanonicalSessionId(senderSessionId) ||
        !isCanonicalSessionId(targetSessionId) ||
        senderSessionId.compare(targetSessionId, Qt::CaseInsensitive) == 0 || canvasPort == 0 ||
        normalizedProfileName.isEmpty())
    {
        return {};
    }

    const QJsonObject object{{QStringLiteral("service"), serviceName()},
                             {QStringLiteral("version"), protocolVersion},
                             {QStringLiteral("type"), QStringLiteral("canvas-invitation")},
                             {QStringLiteral("invitation"), invitationId.toLower()},
                             {QStringLiteral("sender"), senderSessionId.toLower()},
                             {QStringLiteral("target"), targetSessionId.toLower()},
                             {QStringLiteral("canvasPort"), canvasPort},
                             {QStringLiteral("profileName"), normalizedProfileName}};
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return payload.size() <= maximumDatagramSize() ? payload : QByteArray{};
}

std::optional<NearbyCongregationInvitation>
NearbyCongregationProtocol::decodeInvitation(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumDatagramSize())
    {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    const QJsonValue versionValue = object.value(QStringLiteral("version"));
    if (object.value(QStringLiteral("service")).toString() != serviceName() ||
        !versionValue.isDouble() || versionValue.toDouble() != protocolVersion ||
        object.value(QStringLiteral("type")).toString() != QStringLiteral("canvas-invitation"))
    {
        return std::nullopt;
    }

    const QString invitationId = object.value(QStringLiteral("invitation")).toString();
    const QString senderSessionId = object.value(QStringLiteral("sender")).toString();
    const QString targetSessionId = object.value(QStringLiteral("target")).toString();
    const QString inviterProfileName =
        object.value(QStringLiteral("profileName")).toString().simplified();
    const QJsonValue canvasPortValue = object.value(QStringLiteral("canvasPort"));
    const double rawCanvasPort = canvasPortValue.toDouble(-1);
    if (!isCanonicalSessionId(invitationId) || !isCanonicalSessionId(senderSessionId) ||
        !isCanonicalSessionId(targetSessionId) ||
        senderSessionId.compare(targetSessionId, Qt::CaseInsensitive) == 0 ||
        !canvasPortValue.isDouble() || rawCanvasPort < 1 || rawCanvasPort > 65535 ||
        rawCanvasPort != static_cast<int>(rawCanvasPort) || inviterProfileName.isEmpty() ||
        inviterProfileName.size() > maximumInvitationProfileNameLength)
    {
        return std::nullopt;
    }

    return NearbyCongregationInvitation{invitationId.toLower(), senderSessionId.toLower(),
                                   targetSessionId.toLower(), static_cast<quint16>(rawCanvasPort),
                                   inviterProfileName};
}

class NearbyCongregationDiscoveryWorker final : public QObject
{
    Q_OBJECT

  public:
    NearbyCongregationDiscoveryWorker(NearbyCongregationDiscovery::Configuration configuration,
                                 QString sessionId)
        : m_configuration(std::move(configuration)), m_sessionId(std::move(sessionId))
    {
    }

  public slots:
    void start()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_socket)
        {
            return;
        }

        m_clock.start();
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead, this,
                &NearbyCongregationDiscoveryWorker::receivePendingDatagrams);

        const auto bindMode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
        if (!m_socket->bind(QHostAddress::AnyIPv4, m_configuration.port, bindMode))
        {
            setError(m_socket->errorString());
            delete m_socket;
            m_socket = nullptr;
            emit runningChanged(false);
            return;
        }

        m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
        m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(m_configuration.heartbeatIntervalMs);
        connect(m_heartbeatTimer, &QTimer::timeout, this,
                &NearbyCongregationDiscoveryWorker::sendHeartbeat);

        m_pruneTimer = new QTimer(this);
        m_pruneTimer->setInterval(m_configuration.pruneIntervalMs);
        connect(m_pruneTimer, &QTimer::timeout, this,
                &NearbyCongregationDiscoveryWorker::pruneExpiredPeers);

        refreshNetworkInterfaces();
        sendPresence(true);
        m_heartbeatTimer->start();
        m_pruneTimer->start();
        emit runningChanged(true);
    }

    void stop()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!m_socket)
        {
            if (!m_peers.isEmpty())
            {
                m_peers.clear();
                emit nearbyDeviceCountChanged(0);
                emit availableCanvasSessionsChanged({});
                emit availableInvitationTargetsChanged({});
            }
            m_seenInvitationIds.clear();
            setError({});
            emit runningChanged(false);
            return;
        }

        m_heartbeatTimer->stop();
        m_pruneTimer->stop();
        sendPresence(false);

        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
        delete m_pruneTimer;
        m_pruneTimer = nullptr;
        delete m_socket;
        m_socket = nullptr;
        m_joinedInterfaces.clear();
        m_joinedDefaultInterface = false;
        m_localAddresses.clear();

        if (!m_peers.isEmpty())
        {
            m_peers.clear();
            emit nearbyDeviceCountChanged(0);
            emit availableCanvasSessionsChanged({});
            emit availableInvitationTargetsChanged({});
        }
        m_seenInvitationIds.clear();
        setError({});
        emit runningChanged(false);
    }

    void setHostedCanvasPort(quint16 port)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_hostedCanvasPort == port)
        {
            return;
        }
        m_hostedCanvasPort = port;
        if (m_socket)
        {
            sendPresence(true);
        }
    }

    void setInvitationsAllowed(bool allowed)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_invitationsAllowed == allowed)
        {
            return;
        }
        m_invitationsAllowed = allowed;
        if (m_socket)
        {
            sendPresence(true);
        }
    }

    void sendCanvasInvitation(const QString& invitationId, const QString& targetSessionId,
                              quint16 canvasPort, const QString& inviterProfileName)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        const auto target = m_peers.constFind(targetSessionId);
        if (!m_socket || target == m_peers.cend() || !target->invitationsAllowed)
        {
            return;
        }
        const QByteArray payload = NearbyCongregationProtocol::encodeInvitation(
            invitationId, m_sessionId, targetSessionId, canvasPort, inviterProfileName);
        if (!payload.isEmpty())
        {
            sendDatagram(payload);
        }
    }

  signals:
    void runningChanged(bool running);
    void nearbyDeviceCountChanged(int nearbyDeviceCount);
    void availableCanvasSessionsChanged(const QVariantList& sessions);
    void availableInvitationTargetsChanged(const QVariantList& targets);
    void errorStringChanged(const QString& errorString);
    void canvasInvitationReceived(const QVariantMap& invitation);

  private:
    struct Peer
    {
        QHostAddress senderAddress;
        qint64 lastSeenMs = 0;
        quint16 canvasPort = 0;
        bool invitationsAllowed = false;
    };

    void sendHeartbeat()
    {
        refreshNetworkInterfaces();
        sendPresence(true);
    }

    void refreshNetworkInterfaces()
    {
        if (!m_socket)
        {
            return;
        }

        QSet<QHostAddress> localAddresses{QHostAddress::LocalHost};
        QHash<int, QNetworkInterface> currentInterfaces;
        const QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& networkInterface : allInterfaces)
        {
            const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
            if (flags.testFlag(QNetworkInterface::IsUp) &&
                flags.testFlag(QNetworkInterface::IsRunning))
            {
                for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
                {
                    if (!entry.ip().isNull())
                    {
                        localAddresses.insert(entry.ip());
                    }
                }
            }
            if (isUsableMulticastInterface(networkInterface,
                                           m_configuration.includeLoopbackInterfaces))
            {
                currentInterfaces.insert(networkInterface.index(), networkInterface);
            }
        }
        m_localAddresses = std::move(localAddresses);

        for (auto iterator = m_joinedInterfaces.begin(); iterator != m_joinedInterfaces.end();)
        {
            if (!currentInterfaces.contains(iterator.key()))
            {
                m_socket->leaveMulticastGroup(m_configuration.multicastGroup, iterator.value());
                iterator = m_joinedInterfaces.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }

        for (auto iterator = currentInterfaces.cbegin(); iterator != currentInterfaces.cend();
             ++iterator)
        {
            if (m_joinedInterfaces.contains(iterator.key()))
            {
                continue;
            }
            if (m_socket->joinMulticastGroup(m_configuration.multicastGroup, iterator.value()))
            {
                m_joinedInterfaces.insert(iterator.key(), iterator.value());
            }
        }

        if (!m_joinedInterfaces.isEmpty() && m_joinedDefaultInterface)
        {
            m_socket->leaveMulticastGroup(m_configuration.multicastGroup);
            m_joinedDefaultInterface = false;
        }
        else if (m_joinedInterfaces.isEmpty() && !m_joinedDefaultInterface)
        {
            m_joinedDefaultInterface = m_socket->joinMulticastGroup(m_configuration.multicastGroup);
        }

        if (m_joinedInterfaces.isEmpty() && !m_joinedDefaultInterface)
        {
            setError(m_socket->errorString());
        }
    }

    void sendPresence(bool online)
    {
        if (!m_socket)
        {
            return;
        }

        const QByteArray payload = NearbyCongregationProtocol::encodePresence(
            m_sessionId, online, m_hostedCanvasPort, m_invitationsAllowed);
        sendDatagram(payload);
    }

    void sendDatagram(const QByteArray& payload)
    {
        if (!m_socket || payload.isEmpty())
        {
            return;
        }
        bool sent = false;
        for (const QNetworkInterface& networkInterface : std::as_const(m_joinedInterfaces))
        {
            m_socket->setMulticastInterface(networkInterface);
            sent = m_socket->writeDatagram(payload, m_configuration.multicastGroup,
                                           m_configuration.port) == payload.size() ||
                   sent;
        }

        if (m_joinedInterfaces.isEmpty())
        {
            m_socket->setMulticastInterface(QNetworkInterface{});
            sent = m_socket->writeDatagram(payload, m_configuration.multicastGroup,
                                           m_configuration.port) == payload.size();
        }

        const bool listening = !m_joinedInterfaces.isEmpty() || m_joinedDefaultInterface;
        if (sent && listening)
        {
            setError({});
        }
        else if (!listening)
        {
            setError(QStringLiteral("Local network multicast listening is unavailable."));
        }
        else
        {
            setError(m_socket->errorString());
        }
    }

    void receivePendingDatagrams()
    {
        while (m_socket && m_socket->hasPendingDatagrams())
        {
            const QNetworkDatagram datagram =
                m_socket->receiveDatagram(NearbyCongregationProtocol::maximumDatagramSize() + 1);
            if (!datagram.isValid() ||
                datagram.data().size() > NearbyCongregationProtocol::maximumDatagramSize())
            {
                continue;
            }

            if (m_configuration.ignoreLocalSenders &&
                (datagram.senderAddress().isLoopback() ||
                 m_localAddresses.contains(datagram.senderAddress())))
            {
                continue;
            }

            const std::optional<NearbyCongregationInvitation> invitation =
                NearbyCongregationProtocol::decodeInvitation(datagram.data());
            if (invitation)
            {
                const auto sender = m_peers.constFind(invitation->senderSessionId);
                if (m_invitationsAllowed && invitation->targetSessionId == m_sessionId &&
                    invitation->senderSessionId != m_sessionId && sender != m_peers.cend() &&
                    sender->senderAddress == datagram.senderAddress() &&
                    !m_seenInvitationIds.contains(invitation->invitationId))
                {
                    if (m_seenInvitationIds.size() >= maximumTrackedSessions)
                    {
                        const auto oldestInvitation = std::min_element(m_seenInvitationIds.cbegin(),
                                                                       m_seenInvitationIds.cend());
                        if (oldestInvitation != m_seenInvitationIds.cend())
                        {
                            m_seenInvitationIds.erase(oldestInvitation);
                        }
                    }
                    m_seenInvitationIds.insert(invitation->invitationId, m_clock.elapsed());
                    emit canvasInvitationReceived(QVariantMap{
                        {QStringLiteral("invitationId"), invitation->invitationId},
                        {QStringLiteral("sessionId"), invitation->senderSessionId},
                        {QStringLiteral("address"), datagram.senderAddress().toString()},
                        {QStringLiteral("port"), invitation->canvasPort},
                        {QStringLiteral("profileName"), invitation->inviterProfileName}});
                }
                continue;
            }

            const std::optional<NearbyCongregationPresence> presence =
                NearbyCongregationProtocol::decodePresence(datagram.data());
            if (!presence || presence->sessionId == m_sessionId)
            {
                continue;
            }
            const int previousCount = peerDeviceCount();
            const QVariantList previousCanvasSessions = availableCanvasSessions();
            const QVariantList previousInvitationTargets = availableInvitationTargets();
            if (presence->online)
            {
                if (!m_peers.contains(presence->sessionId) &&
                    m_peers.size() >= maximumTrackedSessions)
                {
                    const auto oldestPeer = std::min_element(
                        m_peers.cbegin(), m_peers.cend(), [](const Peer& left, const Peer& right)
                        { return left.lastSeenMs < right.lastSeenMs; });
                    if (oldestPeer != m_peers.cend())
                    {
                        m_peers.erase(oldestPeer);
                    }
                }
                m_peers.insert(presence->sessionId,
                               Peer{datagram.senderAddress(), m_clock.elapsed(),
                                    presence->canvasPort, presence->invitationsAllowed});
            }
            else
            {
                m_peers.remove(presence->sessionId);
            }
            emitPresenceChanges(previousCount, previousCanvasSessions, previousInvitationTargets);
        }
    }

    void pruneExpiredPeers()
    {
        const int previousCount = peerDeviceCount();
        const QVariantList previousCanvasSessions = availableCanvasSessions();
        const QVariantList previousInvitationTargets = availableInvitationTargets();
        const qint64 oldestAllowed = m_clock.elapsed() - m_configuration.peerTimeoutMs;
        for (auto iterator = m_peers.begin(); iterator != m_peers.end();)
        {
            if (iterator->lastSeenMs < oldestAllowed)
            {
                iterator = m_peers.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
        for (auto iterator = m_seenInvitationIds.begin(); iterator != m_seenInvitationIds.end();)
        {
            if (iterator.value() < oldestAllowed)
            {
                iterator = m_seenInvitationIds.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
        emitPresenceChanges(previousCount, previousCanvasSessions, previousInvitationTargets);
    }

    [[nodiscard]] int peerDeviceCount() const
    {
        QSet<QHostAddress> addresses;
        for (const Peer& peer : m_peers)
        {
            addresses.insert(peer.senderAddress);
        }
        return addresses.size();
    }

    [[nodiscard]] QVariantList availableCanvasSessions() const
    {
        QVariantList sessions;
        QStringList sessionIds = m_peers.keys();
        sessionIds.sort(Qt::CaseInsensitive);
        for (const QString& peerSessionId : sessionIds)
        {
            const Peer peer = m_peers.value(peerSessionId);
            if (peer.canvasPort == 0)
            {
                continue;
            }
            sessions.append(QVariantMap{{QStringLiteral("sessionId"), peerSessionId},
                                        {QStringLiteral("address"), peer.senderAddress.toString()},
                                        {QStringLiteral("port"), peer.canvasPort}});
        }
        return sessions;
    }

    [[nodiscard]] QVariantList availableInvitationTargets() const
    {
        QVariantList targets;
        QStringList sessionIds = m_peers.keys();
        sessionIds.sort(Qt::CaseInsensitive);
        for (const QString& peerSessionId : sessionIds)
        {
            const Peer peer = m_peers.value(peerSessionId);
            if (!peer.invitationsAllowed)
            {
                continue;
            }
            targets.append(QVariantMap{{QStringLiteral("sessionId"), peerSessionId},
                                       {QStringLiteral("address"), peer.senderAddress.toString()}});
        }
        return targets;
    }

    void emitPresenceChanges(int previousCount, const QVariantList& previousCanvasSessions,
                             const QVariantList& previousInvitationTargets)
    {
        const int currentCount = peerDeviceCount();
        if (currentCount != previousCount)
        {
            emit nearbyDeviceCountChanged(currentCount);
        }
        const QVariantList currentCanvasSessions = availableCanvasSessions();
        if (currentCanvasSessions != previousCanvasSessions)
        {
            emit availableCanvasSessionsChanged(currentCanvasSessions);
        }
        const QVariantList currentInvitationTargets = availableInvitationTargets();
        if (currentInvitationTargets != previousInvitationTargets)
        {
            emit availableInvitationTargetsChanged(currentInvitationTargets);
        }
    }

    void setError(const QString& errorString)
    {
        if (m_errorString == errorString)
        {
            return;
        }
        m_errorString = errorString;
        emit errorStringChanged(m_errorString);
    }

    NearbyCongregationDiscovery::Configuration m_configuration;
    QString m_sessionId;
    QUdpSocket* m_socket = nullptr;
    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_pruneTimer = nullptr;
    QElapsedTimer m_clock;
    QHash<int, QNetworkInterface> m_joinedInterfaces;
    bool m_joinedDefaultInterface = false;
    QSet<QHostAddress> m_localAddresses;
    QHash<QString, Peer> m_peers;
    QHash<QString, qint64> m_seenInvitationIds;
    quint16 m_hostedCanvasPort = 0;
    bool m_invitationsAllowed = false;
    QString m_errorString;
};

NearbyCongregationDiscovery::NearbyCongregationDiscovery(QObject* parent)
    : NearbyCongregationDiscovery(Configuration{}, parent)
{
}

NearbyCongregationDiscovery::NearbyCongregationDiscovery(Configuration configuration, QObject* parent)
    : QObject(parent), m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_worker(new NearbyCongregationDiscoveryWorker(normalizedConfiguration(std::move(configuration)),
                                                m_sessionId))
{
    m_workerThread.setObjectName(QStringLiteral("Congregation nearby discovery"));
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::runningChanged, this,
            &NearbyCongregationDiscovery::applyRunning);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::nearbyDeviceCountChanged, this,
            &NearbyCongregationDiscovery::applyNearbyDeviceCount);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::availableCanvasSessionsChanged, this,
            &NearbyCongregationDiscovery::applyAvailableCanvasSessions);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::availableInvitationTargetsChanged, this,
            &NearbyCongregationDiscovery::applyAvailableInvitationTargets);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::canvasInvitationReceived, this,
            &NearbyCongregationDiscovery::canvasInvitationReceived);
    connect(m_worker, &NearbyCongregationDiscoveryWorker::errorStringChanged, this,
            &NearbyCongregationDiscovery::applyErrorString);
    m_workerThread.start();
}

NearbyCongregationDiscovery::~NearbyCongregationDiscovery()
{
    if (!m_workerThread.isRunning())
    {
        return;
    }

    QMetaObject::invokeMethod(m_worker, &NearbyCongregationDiscoveryWorker::stop,
                              Qt::BlockingQueuedConnection);
    m_workerThread.quit();
    m_workerThread.wait();
    m_worker = nullptr;
}

bool NearbyCongregationDiscovery::running() const noexcept
{
    return m_running;
}

bool NearbyCongregationDiscovery::anotherCongregationUserDetected() const noexcept
{
    return m_nearbyDeviceCount > 0;
}

int NearbyCongregationDiscovery::nearbyDeviceCount() const noexcept
{
    return m_nearbyDeviceCount;
}

QString NearbyCongregationDiscovery::sessionId() const
{
    return m_sessionId;
}

QVariantList NearbyCongregationDiscovery::availableCanvasSessions() const
{
    return m_availableCanvasSessions;
}

QVariantList NearbyCongregationDiscovery::availableInvitationTargets() const
{
    return m_availableInvitationTargets;
}

quint16 NearbyCongregationDiscovery::hostedCanvasPort() const noexcept
{
    return m_hostedCanvasPort;
}

bool NearbyCongregationDiscovery::invitationsAllowed() const noexcept
{
    return m_invitationsAllowed;
}

QString NearbyCongregationDiscovery::errorString() const
{
    return m_errorString;
}

bool NearbyCongregationDiscovery::sendCanvasInvitation(const QString& invitationId,
                                                  const QString& targetSessionId, int canvasPort,
                                                  const QString& inviterProfileName)
{
    const quint16 normalizedPort =
        canvasPort > 0 && canvasPort <= 65535 ? static_cast<quint16>(canvasPort) : 0;
    const QString normalizedTargetSessionId = targetSessionId.toLower();
    const QString normalizedProfileName =
        inviterProfileName.simplified().left(maximumInvitationProfileNameLength);
    const bool targetAvailable =
        std::any_of(m_availableInvitationTargets.cbegin(), m_availableInvitationTargets.cend(),
                    [&normalizedTargetSessionId](const QVariant& value)
                    {
                        return value.toMap().value(QStringLiteral("sessionId")).toString() ==
                               normalizedTargetSessionId;
                    });
    const QByteArray payload = NearbyCongregationProtocol::encodeInvitation(
        invitationId, m_sessionId, normalizedTargetSessionId, normalizedPort,
        normalizedProfileName);
    if (!m_workerThread.isRunning() || !isCanonicalSessionId(invitationId) ||
        !isCanonicalSessionId(normalizedTargetSessionId) || normalizedPort == 0 ||
        normalizedProfileName.isEmpty() || payload.isEmpty() || !targetAvailable)
    {
        return false;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, normalizedInvitationId = invitationId.toLower(),
         normalizedTargetSessionId, normalizedPort, normalizedProfileName]()
        {
            worker->sendCanvasInvitation(normalizedInvitationId, normalizedTargetSessionId,
                                         normalizedPort, normalizedProfileName);
        },
        Qt::QueuedConnection);
    return true;
}

void NearbyCongregationDiscovery::start()
{
    if (!m_workerThread.isRunning())
    {
        return;
    }
    QMetaObject::invokeMethod(m_worker, &NearbyCongregationDiscoveryWorker::start, Qt::QueuedConnection);
}

void NearbyCongregationDiscovery::stop()
{
    if (!m_workerThread.isRunning())
    {
        applyRunning(false);
        applyNearbyDeviceCount(0);
        applyAvailableCanvasSessions({});
        applyAvailableInvitationTargets({});
        return;
    }
    QMetaObject::invokeMethod(m_worker, &NearbyCongregationDiscoveryWorker::stop,
                              Qt::BlockingQueuedConnection);
    applyRunning(false);
    applyNearbyDeviceCount(0);
    applyAvailableCanvasSessions({});
    applyAvailableInvitationTargets({});
    applyErrorString({});
}

void NearbyCongregationDiscovery::setHostedCanvasPort(int port)
{
    const quint16 normalizedPort = port > 0 && port <= 65535 ? static_cast<quint16>(port) : 0;
    if (m_hostedCanvasPort == normalizedPort)
    {
        return;
    }
    m_hostedCanvasPort = normalizedPort;
    emit hostedCanvasPortChanged();
    if (!m_workerThread.isRunning())
    {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, normalizedPort]()
        { worker->setHostedCanvasPort(normalizedPort); }, Qt::QueuedConnection);
}

void NearbyCongregationDiscovery::setInvitationsAllowed(bool allowed)
{
    if (m_invitationsAllowed == allowed)
    {
        return;
    }
    m_invitationsAllowed = allowed;
    emit invitationsAllowedChanged();
    if (!m_workerThread.isRunning())
    {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, allowed]() { worker->setInvitationsAllowed(allowed); },
        Qt::QueuedConnection);
}

void NearbyCongregationDiscovery::applyRunning(bool running)
{
    if (m_running == running)
    {
        return;
    }
    m_running = running;
    emit runningChanged();
}

void NearbyCongregationDiscovery::applyNearbyDeviceCount(int nearbyDeviceCount)
{
    if (m_nearbyDeviceCount == nearbyDeviceCount)
    {
        return;
    }
    m_nearbyDeviceCount = nearbyDeviceCount;
    emit nearbyPresenceChanged();
}

void NearbyCongregationDiscovery::applyAvailableCanvasSessions(const QVariantList& sessions)
{
    if (m_availableCanvasSessions == sessions)
    {
        return;
    }
    m_availableCanvasSessions = sessions;
    emit availableCanvasSessionsChanged();
}

void NearbyCongregationDiscovery::applyAvailableInvitationTargets(const QVariantList& targets)
{
    if (m_availableInvitationTargets == targets)
    {
        return;
    }
    m_availableInvitationTargets = targets;
    emit availableInvitationTargetsChanged();
}

void NearbyCongregationDiscovery::applyErrorString(const QString& errorString)
{
    if (m_errorString == errorString)
    {
        return;
    }
    m_errorString = errorString;
    emit errorStringChanged();
}

#include "nearbycongregationdiscovery.moc"
