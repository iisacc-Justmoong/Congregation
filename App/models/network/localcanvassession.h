#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class NearbyCongregationDiscovery;
class QTcpServer;
class QTcpSocket;
class QTimer;

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
    enum class SessionState
    {
        Idle,
        Hosting,
        Joining,
        Connected,
    };

    struct HostPeer
    {
        QByteArray receiveBuffer;
        QString peerId;
        QString profileName;
        bool accepted = false;
    };

    void setState(SessionState state);
    void setErrorString(const QString& errorString);
    void clearParticipants();
    void rebuildHostParticipantProfiles();
    void broadcastParticipantProfiles();
    void applyClientParticipantProfiles(const QByteArray& payload);
    void receiveCanvasInvitation(const QVariantMap& invitation);
    void handleHostNewConnection();
    void handleHostReadyRead(QTcpSocket* socket);
    void handleHostDisconnected(QTcpSocket* socket);
    void handleClientConnected();
    void handleClientReadyRead();
    void handleClientDisconnected();
    void handleClientError();
    void processHostFrame(QTcpSocket* socket, quint8 type, const QByteArray& payload);
    void processClientFrame(quint8 type, const QByteArray& payload);
    void acceptSnapshot(const QByteArray& snapshot, const QString& originPeerId);
    void sendLatestSnapshot(QTcpSocket* socket) const;
    void disconnectHostPeer(QTcpSocket* socket, const QString& code, const QString& message);
    void resetTransport();

    [[nodiscard]] bool validSnapshot(const QByteArray& snapshot) const;
    [[nodiscard]] QByteArray participantPayload() const;

    NearbyCongregationDiscovery* m_discovery = nullptr;
    QTcpServer* m_server = nullptr;
    QPointer<QTcpSocket> m_clientSocket;
    QTimer* m_connectTimer = nullptr;
    QHash<QTcpSocket*, HostPeer> m_hostPeers;
    QByteArray m_clientReceiveBuffer;
    QByteArray m_latestSnapshot;
    QVariantList m_participantProfiles;
    QVariantList m_pendingInvitations;
    QString m_ownSessionId;
    QString m_sessionId;
    QString m_peerId;
    QString m_localProfileName;
    QString m_hostPeerId;
    QString m_errorString;
    QString m_latestOriginPeerId;
    SessionState m_state = SessionState::Idle;
    quint64 m_revision = 0;
    bool m_stopping = false;
    bool m_invitationsAllowed = false;
};
