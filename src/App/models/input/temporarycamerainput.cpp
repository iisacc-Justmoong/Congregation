#include "temporarycamerainput.h"

#include <QEvent>
#include <QKeyEvent>

namespace {

bool isZoomModifierKey(int key)
{
    return key == Qt::Key_Control || key == Qt::Key_Meta;
}

} // namespace

TemporaryCameraInput::TemporaryCameraInput(QObject *parent)
    : QObject(parent)
{
}

bool TemporaryCameraInput::enabled() const
{
    return m_enabled;
}

void TemporaryCameraInput::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    if (!m_enabled) {
        resetState();
    }
    emit enabledChanged();
}

QString TemporaryCameraInput::mode() const
{
    return m_mode;
}

bool TemporaryCameraInput::eventFilter(QObject *watched, QEvent *event)
{
    if (!event) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ApplicationDeactivate
        || event->type() == QEvent::WindowDeactivate) {
        resetState();
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
        return QObject::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    const bool spaceKey = keyEvent->key() == Qt::Key_Space;
    const bool zoomModifierKey = isZoomModifierKey(keyEvent->key());
    if (!spaceKey && !zoomModifierKey) {
        return QObject::eventFilter(watched, event);
    }

    if (!m_enabled) {
        return QObject::eventFilter(watched, event);
    }

    const bool pressed = event->type() == QEvent::KeyPress;
    if (zoomModifierKey) {
        if (!keyEvent->isAutoRepeat()) {
            if (keyEvent->key() == Qt::Key_Control) {
                m_controlPressed = pressed;
            } else {
                m_metaPressed = pressed;
            }
            updateMode();
        }

        if (m_spacePressed) {
            keyEvent->accept();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

    if (pressed) {
        if (!keyEvent->isAutoRepeat()) {
            m_spacePressed = true;
            m_controlPressed = keyEvent->modifiers().testFlag(Qt::ControlModifier);
            m_metaPressed = keyEvent->modifiers().testFlag(Qt::MetaModifier);
            updateMode();
        }
        keyEvent->accept();
        return true;
    }

    const bool handled = m_spacePressed || !m_mode.isEmpty();
    if (!keyEvent->isAutoRepeat()) {
        m_spacePressed = false;
        updateMode();
    }
    if (handled) {
        keyEvent->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void TemporaryCameraInput::resetState()
{
    m_spacePressed = false;
    m_controlPressed = false;
    m_metaPressed = false;
    updateMode();
}

void TemporaryCameraInput::updateMode()
{
    if (!m_enabled || !m_spacePressed) {
        setMode(QString{});
        return;
    }

    setMode(m_controlPressed || m_metaPressed ? QStringLiteral("zoom")
                                              : QStringLiteral("pan"));
}

void TemporaryCameraInput::setMode(const QString &mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    emit modeChanged();
}
