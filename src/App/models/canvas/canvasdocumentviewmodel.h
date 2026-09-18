#pragma once

#include <QObject>
#include <QColor>
#include <QVariantList>

class PaletteUtils;

class CanvasDocumentViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList palette READ palette NOTIFY paletteChanged)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushColorChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
    Q_PROPERTY(qreal brushFlow READ brushFlow WRITE setBrushFlow NOTIFY brushFlowChanged)
    Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY brushOpacityChanged)
    Q_PROPERTY(qreal brushHardness READ brushHardness WRITE setBrushHardness NOTIFY brushHardnessChanged)
    Q_PROPERTY(qreal brushSpacing READ brushSpacing WRITE setBrushSpacing NOTIFY brushSpacingChanged)
    Q_PROPERTY(qreal brushSpacingRatio READ brushSpacingRatio WRITE setBrushSpacingRatio NOTIFY brushSpacingRatioChanged)
    Q_PROPERTY(qreal pressureCurveMinimum READ pressureCurveMinimum WRITE setPressureCurveMinimum NOTIFY pressureCurveMinimumChanged)
    Q_PROPERTY(qreal pressureCurveCenter READ pressureCurveCenter WRITE setPressureCurveCenter NOTIFY pressureCurveCenterChanged)
    Q_PROPERTY(qreal pressureCurveMaximum READ pressureCurveMaximum WRITE setPressureCurveMaximum NOTIFY pressureCurveMaximumChanged)
    Q_PROPERTY(bool brushPressureControlsOpacity READ brushPressureControlsOpacity WRITE setBrushPressureControlsOpacity NOTIFY brushPressureControlsOpacityChanged)
    Q_PROPERTY(qreal stabilizerStrength READ stabilizerStrength WRITE setStabilizerStrength NOTIFY stabilizerStrengthChanged)
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(QString shapeKind READ shapeKind WRITE setShapeKind NOTIFY shapeKindChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth WRITE setCanvasWidth NOTIFY canvasWidthChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight WRITE setCanvasHeight NOTIFY canvasHeightChanged)

public:
    explicit CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent = nullptr);

    static constexpr qreal maximumAntialiasingBrushHardness()
    {
        return 1.0;
    }

    [[nodiscard]] QVariantList palette() const;
    [[nodiscard]] QColor brushColor() const;
    [[nodiscard]] qreal brushSize() const;
    [[nodiscard]] qreal brushFlow() const;
    [[nodiscard]] qreal brushOpacity() const;
    [[nodiscard]] qreal brushHardness() const;
    [[nodiscard]] qreal brushSpacing() const;
    [[nodiscard]] qreal brushSpacingRatio() const;
    [[nodiscard]] qreal pressureCurveMinimum() const;
    [[nodiscard]] qreal pressureCurveCenter() const;
    [[nodiscard]] qreal pressureCurveMaximum() const;
    [[nodiscard]] bool brushPressureControlsOpacity() const;
    [[nodiscard]] qreal stabilizerStrength() const;
    [[nodiscard]] QString toolMode() const;
    [[nodiscard]] QString shapeKind() const;
    [[nodiscard]] int canvasWidth() const;
    [[nodiscard]] int canvasHeight() const;

    void setBrushColor(const QColor &brushColor);
    void setBrushSize(qreal brushSize);
    void setBrushFlow(qreal brushFlow);
    void setBrushOpacity(qreal brushOpacity);
    void setBrushHardness(qreal brushHardness);
    void setBrushSpacing(qreal brushSpacing);
    void setBrushSpacingRatio(qreal brushSpacingRatio);
    void setPressureCurveMinimum(qreal pressureCurveMinimum);
    void setPressureCurveCenter(qreal pressureCurveCenter);
    void setPressureCurveMaximum(qreal pressureCurveMaximum);
    void setBrushPressureControlsOpacity(bool enabled);
    void setStabilizerStrength(qreal stabilizerStrength);
    void setToolMode(const QString &toolMode);
    void setShapeKind(const QString &shapeKind);
    void setCanvasWidth(int canvasWidth);
    void setCanvasHeight(int canvasHeight);

signals:
    void paletteChanged();
    void brushColorChanged();
    void brushSizeChanged();
    void brushFlowChanged();
    void brushOpacityChanged();
    void brushHardnessChanged();
    void brushSpacingChanged();
    void brushSpacingRatioChanged();
    void pressureCurveMinimumChanged();
    void pressureCurveCenterChanged();
    void pressureCurveMaximumChanged();
    void brushPressureControlsOpacityChanged();
    void stabilizerStrengthChanged();
    void toolModeChanged();
    void shapeKindChanged();
    void canvasWidthChanged();
    void canvasHeightChanged();

private:
    QVariantList buildDefaultPalette(PaletteUtils *paletteUtils) const;
    QVariantList m_palette;
    QColor m_brushColor;
    qreal m_brushSize = 2.0;
    qreal m_brushFlow = 1.0;
    qreal m_brushOpacity = 1.0;
    qreal m_brushHardness = maximumAntialiasingBrushHardness();
    qreal m_brushSpacing = 0.0;
    qreal m_brushSpacingRatio = 0.0;
    qreal m_pressureCurveMinimum = 0.0;
    qreal m_pressureCurveCenter = 0.5;
    qreal m_pressureCurveMaximum = 1.0;
    bool m_brushPressureControlsOpacity = true;
    qreal m_stabilizerStrength = 0.0;
    QString m_toolMode = QStringLiteral("brush");
    QString m_shapeKind = QStringLiteral("rectangle");
    int m_canvasWidth = 1;
    int m_canvasHeight = 1;
};
