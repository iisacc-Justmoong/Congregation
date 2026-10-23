#include "nearbycongregationdiscovery.h"
#include "../../host/qtenginehost.h"
#include "../../host/structuredvalues.h"
#include <Engine/NearbyDiscovery.h>
using namespace congregation;
QString NearbyCongregationProtocol::serviceName()
{
    return QStringLiteral("com.iisacc.congregation.nearby");
}
QByteArray NearbyCongregationProtocol::encodePresence(const QString& id, bool online, quint16 port,
                                                      bool allowed)
{
    return QByteArray::fromStdString(encode_presence({id.toStdString(), online, port, allowed}));
}
std::optional<NearbyCongregationPresence>
NearbyCongregationProtocol::decodePresence(const QByteArray& bytes)
{
    auto p = decode_presence(std::string_view(bytes.constData(), bytes.size()));
    if (!p)
        return {};
    return NearbyCongregationPresence{QString::fromStdString(p->session), p->online, p->port,
                                      p->invitations};
}
QByteArray NearbyCongregationProtocol::encodeInvitation(const QString& id, const QString& sender,
                                                        const QString& target, quint16 port,
                                                        const QString& name)
{
    return QByteArray::fromStdString(encode_invitation(
        {id.toStdString(), sender.toStdString(), target.toStdString(), port, name.toStdString()}));
}
std::optional<NearbyCongregationInvitation>
NearbyCongregationProtocol::decodeInvitation(const QByteArray& bytes)
{
    auto i = decode_invitation(std::string_view(bytes.constData(), bytes.size()));
    if (!i)
        return {};
    return NearbyCongregationInvitation{
        QString::fromStdString(i->id), QString::fromStdString(i->sender),
        QString::fromStdString(i->target), i->port, QString::fromStdString(i->name)};
}
NearbyCongregationDiscovery::NearbyCongregationDiscovery(QObject* parent)
    : NearbyCongregationDiscovery(Configuration{}, parent)
{
}
NearbyCongregationDiscovery::NearbyCongregationDiscovery(Configuration c, QObject* parent)
    : QObject(parent), m_events(std::make_unique<ii::runtime::EventLoop>()),
      m_host(std::make_unique<QtEngineHost>(*m_events)),
      m_backend(std::make_unique<NearbyDiscovery>(
          *m_events,
          NearbyDiscovery::Configuration{c.multicastGroup.toStdString(), c.port,
                                         c.heartbeatIntervalMs, c.peerTimeoutMs, c.pruneIntervalMs,
                                         c.ignoreLocalSenders, c.includeLoopbackInterfaces}))
{
    m_sessionId = QString::fromStdString(m_backend->id);
    m_backend->changed = [this] { refresh(); };
    m_backend->invited = [this](Object invitation)
    {
        emit canvasInvitationReceived(
            presentation::toVariant(Value(std::move(invitation))).toMap());
    };
}
NearbyCongregationDiscovery::~NearbyCongregationDiscovery()
{
    m_backend.reset();
    m_host.reset();
}
void NearbyCongregationDiscovery::refresh()
{
    applyRunning(m_backend->running);
    applyNearbyDeviceCount(m_backend->count());
    applyAvailableCanvasSessions(presentation::toVariant(Value(m_backend->canvases())).toList());
    applyAvailableInvitationTargets(presentation::toVariant(Value(m_backend->targets())).toList());
    applyErrorString(QString::fromStdString(m_backend->error));
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
void NearbyCongregationDiscovery::start()
{
    m_backend->start();
}
void NearbyCongregationDiscovery::stop()
{
    m_backend->stop();
}
void NearbyCongregationDiscovery::setHostedCanvasPort(int port)
{
    auto value = static_cast<quint16>(std::clamp(port, 0, 65535));
    if (value == m_hostedCanvasPort)
        return;
    m_hostedCanvasPort = value;
    m_backend->set_port(value);
    emit hostedCanvasPortChanged();
}
void NearbyCongregationDiscovery::setInvitationsAllowed(bool allowed)
{
    if (allowed == m_invitationsAllowed)
        return;
    m_invitationsAllowed = allowed;
    m_backend->set_invitations(allowed);
    emit invitationsAllowedChanged();
}
bool NearbyCongregationDiscovery::sendCanvasInvitation(const QString& id, const QString& target,
                                                       int port, const QString& name)
{
    return port > 0 && port <= 65535 &&
           m_backend->invite(id.toStdString(), target.toStdString(), static_cast<quint16>(port),
                             name.toStdString());
}
void NearbyCongregationDiscovery::applyRunning(bool value)
{
    if (m_running == value)
        return;
    m_running = value;
    emit runningChanged();
}
void NearbyCongregationDiscovery::applyNearbyDeviceCount(int value)
{
    if (m_nearbyDeviceCount == value)
        return;
    m_nearbyDeviceCount = value;
    emit nearbyPresenceChanged();
}
void NearbyCongregationDiscovery::applyAvailableCanvasSessions(const QVariantList& value)
{
    if (m_availableCanvasSessions == value)
        return;
    m_availableCanvasSessions = value;
    emit availableCanvasSessionsChanged();
}
void NearbyCongregationDiscovery::applyAvailableInvitationTargets(const QVariantList& value)
{
    if (m_availableInvitationTargets == value)
        return;
    m_availableInvitationTargets = value;
    emit availableInvitationTargetsChanged();
}
void NearbyCongregationDiscovery::applyErrorString(const QString& value)
{
    if (m_errorString == value)
        return;
    m_errorString = value;
    emit errorStringChanged();
}
