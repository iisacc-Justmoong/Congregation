#pragma once

#include <QColor>
#include <QPointer>
#include <QString>

class QObject;

struct CanvasToolState
{
    QColor color = QColor(Qt::black);
    qreal size = 8.0;
    qreal flow = 1.0;
    qreal opacity = 1.0;
    qreal hardness = 1.0;
    qreal spacing = 0.0;
    qreal spacingRatio = 0.0;
    qreal pressureCurveMinimum = 0.0;
    qreal pressureCurveCenter = 0.5;
    qreal pressureCurveMaximum = 1.0;
    qreal stabilizerStrength = 0.0;
    bool flowEnabled = true;
    bool opacityEnabled = true;
    bool hardnessEnabled = true;
    bool spacingEnabled = true;
    bool pressureToOpacityEnabled = true;
};

class CanvasViewModelBridge
{
public:
    void setDocumentViewModel(QObject *documentViewModel);
    [[nodiscard]] QObject *documentViewModel() const;
    [[nodiscard]] bool canMutateDocument() const;

    void syncToolState(CanvasToolState &toolState, QString &toolMode) const;
    void syncCanvasSize(qreal width, qreal height) const;

private:
    QPointer<QObject> m_documentViewModel;
};
