#include "qtenginehost.h"

#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <limits>

QtEngineHost::QtEngineHost(ii::runtime::EventLoop& events, QObject* parent)
    : QObject(parent), m_events(events)
{
    m_deadline.setSingleShot(true);
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_deadline, &QTimer::timeout, this, &QtEngineHost::dispatch);
    m_connection = std::make_unique<ii::runtime::HostLoop>(
        m_events, [this]() noexcept
        { QMetaObject::invokeMethod(this, [this] { dispatch(); }, Qt::QueuedConnection); },
        ii::runtime::HostLoopOptions{.monitor_io = false});
}

QtEngineHost::~QtEngineHost()
{
    // The Core barrier finishes foreign wake calls before QObject removes its
    // queued invocations. The native deadline cannot outlive this host either.
    m_connection.reset();
    m_deadline.stop();
}

void QtEngineHost::dispatch()
{
    try
    {
        m_events.poll({64, std::chrono::milliseconds(4)});
        const auto deadline = m_events.next_deadline();
        if (!deadline)
        {
            m_deadline.stop();
            return;
        }
        const auto delay =
            std::chrono::ceil<std::chrono::milliseconds>(*deadline - ii::runtime::Clock::now())
                .count();
        m_deadline.start(
            int(std::clamp<decltype(delay)>(delay, 0, std::numeric_limits<int>::max())));
    }
    catch (const std::exception& error)
    {
        m_deadline.stop();
        m_events.request_stop();
        emit dispatchFailed(QString::fromUtf8(error.what()));
    }
    catch (...)
    {
        m_deadline.stop();
        m_events.request_stop();
        emit dispatchFailed(QStringLiteral("Unknown iiSDKCore dispatch failure"));
    }
}
