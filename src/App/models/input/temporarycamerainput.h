#pragma once

#include <QObject>
#include <QString>

class QEvent;

class TemporaryCameraInput final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)

public:
    explicit TemporaryCameraInput(QObject *parent = nullptr);

    [[nodiscard]] bool enabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] QString mode() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void enabledChanged();
    void modeChanged();

private:
    void resetState();
    void updateMode();
    void setMode(const QString &mode);

    bool m_enabled = false;
    bool m_spacePressed = false;
    bool m_controlPressed = false;
    bool m_metaPressed = false;
    QString m_mode;
};
