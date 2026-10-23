#include "localcanvassession.h"
#include "nearbycongregationdiscovery.h"
#include "../../host/qtenginehost.h"
#include "../../host/structuredvalues.h"
#include <Engine/CanvasSession.h>
using namespace congregation;
namespace
{
QString qtext(const std::string& s)
{
    return QString::fromStdString(s);
}
QVariantList list(const Array& a)
{
    return presentation::toVariant(Value(a)).toList();
}
} // namespace
LocalCanvasSession::LocalCanvasSession(NearbyCongregationDiscovery* discovery, QObject* parent)
    : QObject(parent), m_discovery(discovery), m_events(std::make_unique<ii::runtime::EventLoop>()),
      m_host(std::make_unique<QtEngineHost>(*m_events)),
      m_backend(std::make_unique<CanvasSession>(
          *m_events, discovery ? discovery->sessionId().toStdString() : uuid()))
{
    m_backend->changed = [this](CanvasSession::Notice n)
    {
        switch (n)
        {
        case CanvasSession::Notice::state:
            emit stateChanged();
            emit availableCanvasesChanged();
            break;
        case CanvasSession::Notice::error:
            emit errorStringChanged();
            break;
        case CanvasSession::Notice::profiles:
            emit participantProfilesChanged();
            break;
        case CanvasSession::Notice::pending:
            emit pendingInvitationsChanged();
            break;
        case CanvasSession::Notice::revision:
            emit revisionChanged();
            break;
        case CanvasSession::Notice::invitations:
            emit invitationsAllowedChanged();
            break;
        }
    };
    m_backend->snapshot_requested = [this] { emit snapshotRequested(); };
    m_backend->snapshot_received = [this](auto bytes, auto revision, auto origin)
    { emit snapshotReceived(QByteArray::fromStdString(bytes), revision, qtext(origin)); };
    m_backend->edit_received = [this](auto command, auto peer)
    {
        emit editCommandReceived(presentation::toVariant(Value(std::move(command))).toMap(),
                                 qtext(peer));
    };
    m_backend->advertise = [this](auto port)
    {
        if (m_discovery)
            m_discovery->setHostedCanvasPort(port);
    };
    if (discovery)
    {
        connect(discovery, &NearbyCongregationDiscovery::availableCanvasSessionsChanged, this,
                &LocalCanvasSession::availableCanvasesChanged);
        connect(discovery, &NearbyCongregationDiscovery::availableInvitationTargetsChanged, this,
                &LocalCanvasSession::availableInviteesChanged);
        connect(discovery, &NearbyCongregationDiscovery::canvasInvitationReceived, this,
                [this](auto invitation)
                { m_backend->receive_invitation(object(presentation::fromVariant(invitation))); });
    }
}
LocalCanvasSession::~LocalCanvasSession()
{
    m_backend.reset();
    m_host.reset();
}
bool LocalCanvasSession::active() const noexcept
{
    return m_backend->state != CanvasSession::State::idle;
}
bool LocalCanvasSession::hosting() const noexcept
{
    return m_backend->state == CanvasSession::State::hosting;
}
bool LocalCanvasSession::connected() const noexcept
{
    return m_backend->state == CanvasSession::State::connected;
}
bool LocalCanvasSession::currentUserIsHost() const noexcept
{
    return m_backend->state != CanvasSession::State::connected &&
           m_backend->state != CanvasSession::State::joining;
}
QString LocalCanvasSession::state() const
{
    switch (m_backend->state)
    {
    case CanvasSession::State::hosting:
        return QStringLiteral("hosting");
    case CanvasSession::State::joining:
        return QStringLiteral("joining");
    case CanvasSession::State::connected:
        return QStringLiteral("connected");
    default:
        return QStringLiteral("idle");
    }
}
QString LocalCanvasSession::sessionId() const
{
    return qtext(m_backend->session);
}
QString LocalCanvasSession::peerId() const
{
    return qtext(m_backend->peer_id);
}
QString LocalCanvasSession::errorString() const
{
    return qtext(m_backend->error);
}
QVariantList LocalCanvasSession::participantProfiles() const
{
    return list(m_backend->profiles);
}
int LocalCanvasSession::participantCount() const noexcept
{
    return static_cast<int>(m_backend->profiles.size());
}
QVariantList LocalCanvasSession::availableCanvases() const
{
    QVariantList out;
    if (!m_discovery)
        return out;
    for (auto v : m_discovery->availableCanvasSessions())
    {
        auto c = v.toMap();
        if (hosting() && c.value(QStringLiteral("sessionId")).toString() == sessionId())
            continue;
        c.insert(QStringLiteral("displayName"),
                 tr("Canvas at %1").arg(c.value(QStringLiteral("address")).toString()));
        out.append(c);
    }
    return out;
}
QVariantList LocalCanvasSession::availableInvitees() const
{
    QVariantList out;
    if (!m_discovery)
        return out;
    for (auto v : m_discovery->availableInvitationTargets())
    {
        auto c = v.toMap();
        c.insert(QStringLiteral("displayName"),
                 tr("Congregation at %1").arg(c.value(QStringLiteral("address")).toString()));
        out.append(c);
    }
    return out;
}
QVariantMap LocalCanvasSession::pendingInvitation() const
{
    return m_backend->pending.empty() ? QVariantMap{}
                                      : presentation::toVariant(m_backend->pending.front()).toMap();
}
int LocalCanvasSession::pendingInvitationCount() const noexcept
{
    return static_cast<int>(m_backend->pending.size());
}
bool LocalCanvasSession::invitationsAllowed() const noexcept
{
    return m_backend->invitations;
}
quint16 LocalCanvasSession::listenPort() const noexcept
{
    return m_backend->port();
}
quint64 LocalCanvasSession::revision() const noexcept
{
    return m_backend->revision;
}
bool LocalCanvasSession::startHosting(const QString& name)
{
    return m_backend->host(name.toStdString());
}
bool LocalCanvasSession::joinCanvasAt(const QString& session, const QString& address, int port,
                                      const QString& name)
{
    return m_backend->join(session.toStdString(), address.toStdString(), port, name.toStdString());
}
bool LocalCanvasSession::joinCanvas(const QString& session, const QString& name)
{
    for (const auto& v : availableCanvases())
    {
        auto c = v.toMap();
        if (c.value(QStringLiteral("sessionId")).toString() == session)
            return joinCanvasAt(session, c.value(QStringLiteral("address")).toString(),
                                c.value(QStringLiteral("port")).toInt(), name);
    }
    return false;
}
void LocalCanvasSession::stopSession()
{
    m_backend->stop();
}
void LocalCanvasSession::setLocalProfileName(const QString& name)
{
    m_backend->set_profile(name.toStdString());
}
bool LocalCanvasSession::publishSnapshot(const QByteArray& bytes)
{
    return m_backend->publish(std::string_view(bytes.constData(), bytes.size()));
}
bool LocalCanvasSession::submitEditCommand(const QVariantMap& command)
{
    return m_backend->submit(object(presentation::fromVariant(command)));
}
bool LocalCanvasSession::removeParticipant(const QString& peer)
{
    return m_backend->remove(peer.toStdString());
}
void LocalCanvasSession::setInvitationsAllowed(bool allowed)
{
    m_backend->set_invitations(allowed);
    if (m_discovery)
        m_discovery->setInvitationsAllowed(allowed);
}
bool LocalCanvasSession::invitePeer(const QString& target, const QString& name)
{
    if (!m_discovery || !currentUserIsHost())
        return false;
    bool found = false;
    for (const auto& v : availableInvitees())
        if (v.toMap().value(QStringLiteral("sessionId")).toString() == target)
            found = true;
    if (!found || (!hosting() && !startHosting(name)))
        return false;
    setLocalProfileName(name);
    return m_discovery->sendCanvasInvitation(qtext(uuid()), target, listenPort(), name);
}
bool LocalCanvasSession::respondToPendingInvitation(bool accepted, const QString& name)
{
    return m_backend->respond(accepted, name.toStdString());
}
