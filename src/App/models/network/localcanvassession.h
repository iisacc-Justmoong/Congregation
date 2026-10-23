#pragma once

#include <QByteArray>
#include <memory>
#include <QObject>

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class NearbyCongregationDiscovery;
class QtEngineHost;
namespace ii::runtime
{
class EventLoop;
}
namespace congregation
{
class CanvasSession;
}

class LocalCanvasSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool hosting READ hosting NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool currentUserIsHost READ currentUserIsHost NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY stateChanged)
    Q_PROPERTY(QString peerId READ peerId CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(
        QVariantList participantProfiles READ participantProfiles NOTIFY participantProfilesChanged)
    Q_PROPERTY(int participantCount READ participantCount NOTIFY participantProfilesChanged)
    Q_PROPERTY(
        QVariantList availableCanvases READ availableCanvases NOTIFY availableCanvasesChanged)
    Q_PROPERTY(
        QVariantList availableInvitees READ availableInvitees NOTIFY availableInviteesChanged)
    Q_PROPERTY(
        QVariantMap pendingInvitation READ pendingInvitation NOTIFY pendingInvitationsChanged)
    Q_PROPERTY(
        int pendingInvitationCount READ pendingInvitationCount NOTIFY pendingInvitationsChanged)
    Q_PROPERTY(bool invitationsAllowed READ invitationsAllowed NOTIFY invitationsAllowedChanged)
    Q_PROPERTY(quint16 listenPort READ listenPort NOTIFY stateChanged)
    Q_PROPERTY(quint64 revision READ revision NOTIFY revisionChanged)

  public:
    explicit LocalCanvasSession(NearbyCongregationDiscovery* discovery, QObject* parent = nullptr);
    ~LocalCanvasSession() override;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool hosting() const noexcept;
    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] bool currentUserIsHost() const noexcept;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString sessionId() const;
    [[nodiscard]] QString peerId() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QVariantList participantProfiles() const;
    [[nodiscard]] int participantCount() const noexcept;
    [[nodiscard]] QVariantList availableCanvases() const;
    [[nodiscard]] QVariantList availableInvitees() const;
    [[nodiscard]] QVariantMap pendingInvitation() const;
    [[nodiscard]] int pendingInvitationCount() const noexcept;
    [[nodiscard]] bool invitationsAllowed() const noexcept;
    [[nodiscard]] quint16 listenPort() const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;

    Q_INVOKABLE bool startHosting(const QString& profileName);
    Q_INVOKABLE bool joinCanvas(const QString& sessionId, const QString& profileName);
    Q_INVOKABLE bool joinCanvasAt(const QString& sessionId, const QString& address, int port,
                                  const QString& profileName);
    Q_INVOKABLE void stopSession();
    Q_INVOKABLE void setLocalProfileName(const QString& profileName);
    Q_INVOKABLE bool publishSnapshot(const QByteArray& snapshot);
    Q_INVOKABLE bool submitEditCommand(const QVariantMap& command);
    Q_INVOKABLE bool removeParticipant(const QString& participantPeerId);
    Q_INVOKABLE void setInvitationsAllowed(bool allowed);
    Q_INVOKABLE bool invitePeer(const QString& targetSessionId, const QString& profileName);
    Q_INVOKABLE bool respondToPendingInvitation(bool accepted, const QString& profileName);

  signals:
    void stateChanged();
    void errorStringChanged();
    void participantProfilesChanged();
    void availableCanvasesChanged();
    void availableInviteesChanged();
    void pendingInvitationsChanged();
    void invitationsAllowedChanged();
    void revisionChanged();
    void snapshotRequested();
    void snapshotReceived(const QByteArray& snapshot, quint64 revision,
                          const QString& originPeerId);
    void editCommandReceived(const QVariantMap& command, const QString& originPeerId);

  private:
    NearbyCongregationDiscovery* m_discovery = nullptr;
    std::unique_ptr<ii::runtime::EventLoop> m_events;
    std::unique_ptr<QtEngineHost> m_host;
    std::unique_ptr<congregation::CanvasSession> m_backend;
};
