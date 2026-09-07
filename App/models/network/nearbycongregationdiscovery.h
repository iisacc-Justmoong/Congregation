#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

struct NearbyCongregationPresence
{
    QString sessionId;
    bool online = false;
    quint16 canvasPort = 0;
    bool invitationsAllowed = false;
};

struct NearbyCongregationInvitation
{
    QString invitationId;
    QString senderSessionId;
    QString targetSessionId;
    quint16 canvasPort = 0;
    QString inviterProfileName;
};

class NearbyCongregationProtocol final
{
  public:
    [[nodiscard]] static QString serviceName();
    [[nodiscard]] static QByteArray encodePresence(const QString& sessionId, bool online,
                                                   quint16 canvasPort = 0,
                                                   bool invitationsAllowed = false);
    [[nodiscard]] static std::optional<NearbyCongregationPresence>
    decodePresence(const QByteArray& payload);
    [[nodiscard]] static QByteArray encodeInvitation(const QString& invitationId,
                                                     const QString& senderSessionId,
                                                     const QString& targetSessionId,
                                                     quint16 canvasPort,
                                                     const QString& inviterProfileName);
    [[nodiscard]] static std::optional<NearbyCongregationInvitation>
    decodeInvitation(const QByteArray& payload);
    [[nodiscard]] static constexpr qsizetype maximumDatagramSize() noexcept { return 512; }
};

class NearbyCongregationDiscoveryWorker;

class NearbyCongregationDiscovery final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool anotherCongregationUserDetected READ anotherCongregationUserDetected NOTIFY
                   nearbyPresenceChanged)
    Q_PROPERTY(int nearbyDeviceCount READ nearbyDeviceCount NOTIFY nearbyPresenceChanged)
    Q_PROPERTY(QString sessionId READ sessionId CONSTANT)
    Q_PROPERTY(QVariantList availableCanvasSessions READ availableCanvasSessions NOTIFY
                   availableCanvasSessionsChanged)
    Q_PROPERTY(QVariantList availableInvitationTargets READ availableInvitationTargets NOTIFY
                   availableInvitationTargetsChanged)
    Q_PROPERTY(quint16 hostedCanvasPort READ hostedCanvasPort NOTIFY hostedCanvasPortChanged)
    Q_PROPERTY(bool invitationsAllowed READ invitationsAllowed NOTIFY invitationsAllowedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

  public:
    struct Configuration
    {
        QHostAddress multicastGroup{QStringLiteral("239.255.86.67")};
        quint16 port = 52683;
        int heartbeatIntervalMs = 4000;
        int peerTimeoutMs = 14000;
        int pruneIntervalMs = 1000;
        bool ignoreLocalSenders = true;
        bool includeLoopbackInterfaces = false;
    };

    explicit NearbyCongregationDiscovery(QObject* parent = nullptr);
    explicit NearbyCongregationDiscovery(Configuration configuration, QObject* parent = nullptr);
    ~NearbyCongregationDiscovery() override;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool anotherCongregationUserDetected() const noexcept;
    [[nodiscard]] int nearbyDeviceCount() const noexcept;
    [[nodiscard]] QString sessionId() const;
    [[nodiscard]] QVariantList availableCanvasSessions() const;
    [[nodiscard]] QVariantList availableInvitationTargets() const;
    [[nodiscard]] quint16 hostedCanvasPort() const noexcept;
    [[nodiscard]] bool invitationsAllowed() const noexcept;
    [[nodiscard]] QString errorString() const;

    Q_INVOKABLE bool sendCanvasInvitation(const QString& invitationId,
                                          const QString& targetSessionId, int canvasPort,
                                          const QString& inviterProfileName);

  public slots:
    void start();
    void stop();
    void setHostedCanvasPort(int port);
    void setInvitationsAllowed(bool allowed);

  signals:
    void runningChanged();
    void nearbyPresenceChanged();
    void availableCanvasSessionsChanged();
    void availableInvitationTargetsChanged();
    void hostedCanvasPortChanged();
    void invitationsAllowedChanged();
    void errorStringChanged();
    void canvasInvitationReceived(const QVariantMap& invitation);

  private:
    void applyRunning(bool running);
    void applyNearbyDeviceCount(int nearbyDeviceCount);
    void applyAvailableCanvasSessions(const QVariantList& sessions);
    void applyAvailableInvitationTargets(const QVariantList& targets);
    void applyErrorString(const QString& errorString);

    QThread m_workerThread;
    QString m_sessionId;
    NearbyCongregationDiscoveryWorker* m_worker = nullptr;
    bool m_running = false;
    int m_nearbyDeviceCount = 0;
    quint16 m_hostedCanvasPort = 0;
    bool m_invitationsAllowed = false;
    QVariantList m_availableCanvasSessions;
    QVariantList m_availableInvitationTargets;
    QString m_errorString;
};
