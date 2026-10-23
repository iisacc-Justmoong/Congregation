#include "applicationpreferences.h"
#include <Engine/Preferences.h>
#include "../../host/nativepaths.h"
using congregation::presentation::pathText;
ApplicationPreferences::ApplicationPreferences(QObject* parent)
    : QObject(parent), m_backend(std::make_unique<congregation::Preferences>())
{
    const auto path = m_backend->existing_canvas();
    m_recentCanvasUrl = path.empty() ? QUrl{} : QUrl::fromLocalFile(pathText(path));
}
ApplicationPreferences::ApplicationPreferences(const QString& path, QObject* parent)
    : ApplicationPreferences(path, QString{}, parent)
{
}
ApplicationPreferences::ApplicationPreferences(const QString& path, const QString& storage,
                                               QObject* parent)
    : QObject(parent), m_backend(std::make_unique<congregation::Preferences>(
                           congregation::presentation::nativePath(path),
                           congregation::presentation::nativePath(storage)))
{
    const auto canvas = m_backend->existing_canvas();
    m_recentCanvasUrl = canvas.empty() ? QUrl{} : QUrl::fromLocalFile(pathText(canvas));
}
ApplicationPreferences::~ApplicationPreferences() = default;
bool ApplicationPreferences::startWithRecentCanvas() const noexcept
{
    return m_backend->start_recent;
}
bool ApplicationPreferences::discoverNearbyCongregationUsers() const noexcept
{
    return m_backend->discover_nearby;
}
QUrl ApplicationPreferences::recentCanvasUrl() const
{
    return m_recentCanvasUrl;
}
QUrl ApplicationPreferences::recentCanvasStorageUrl() const
{
    auto p = m_backend->storage_path();
    return p.empty() ? QUrl{} : QUrl::fromLocalFile(pathText(p));
}
void ApplicationPreferences::setStartWithRecentCanvas(bool value)
{
    if (value == startWithRecentCanvas())
        return;
    try
    {
        m_backend->set_start_recent(value);
    }
    catch (const std::exception& error)
    {
        qWarning("Cannot save Congregation preferences: %s", error.what());
        emit startWithRecentCanvasChanged();
        return;
    }
    emit startWithRecentCanvasChanged();
}
void ApplicationPreferences::setDiscoverNearbyCongregationUsers(bool value)
{
    if (value == discoverNearbyCongregationUsers())
        return;
    try
    {
        m_backend->set_discover_nearby(value);
    }
    catch (const std::exception& error)
    {
        qWarning("Cannot save Congregation preferences: %s", error.what());
        emit discoverNearbyCongregationUsersChanged();
        return;
    }
    emit discoverNearbyCongregationUsersChanged();
}
bool ApplicationPreferences::recordRecentCanvas(const QUrl& url)
{
    if (!url.isLocalFile() ||
        !m_backend->record_canvas(congregation::presentation::nativePath(url.toLocalFile())))
        return false;
    auto next = QUrl::fromLocalFile(pathText(m_backend->existing_canvas()));
    if (next != m_recentCanvasUrl)
    {
        m_recentCanvasUrl = std::move(next);
        emit recentCanvasUrlChanged();
    }
    return true;
}
bool ApplicationPreferences::clearRecentCanvas()
{
    if (!m_backend->clear_canvas())
        return false;
    if (!m_recentCanvasUrl.isEmpty())
    {
        m_recentCanvasUrl = QUrl{};
        emit recentCanvasUrlChanged();
    }
    return true;
}
