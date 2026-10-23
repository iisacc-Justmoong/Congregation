#pragma once

#include <iiSDKCore/Runtime/HostLoop.h>

#include <QObject>
#include <QTimer>
#include <memory>

// Qt supplies a wake gate and a single deadline timer for the Core EventLoop.
// Task timing, queue capacity, dispatch and cancellation belong to Core.
class QtEngineHost final : public QObject
{
    Q_OBJECT
  public:
    explicit QtEngineHost(ii::runtime::EventLoop& events, QObject* parent = nullptr);
    ~QtEngineHost() override;

  signals:
    void dispatchFailed(const QString& message);

  private:
    void dispatch();
    ii::runtime::EventLoop& m_events;
    QTimer m_deadline;
    std::unique_ptr<ii::runtime::HostLoop> m_connection;
};
