#include "canvasdocumentviewmodel.h"

#include "../brush/paletteutils.h"

#include <QStringList>
#include <QVariantMap>
#include <QtGlobal>

namespace {

QVariantMap paletteEntry(const QString &name, const QString &color)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("color"), color}
    };
}

bool setBoundedValue(qreal &storage, qreal value, qreal minimum, qreal maximum)
{
    const qreal boundedValue = qBound(minimum, value, maximum);
    if (qFuzzyCompare(storage, boundedValue)) {
        return false;
    }

    storage = boundedValue;
    return true;
}

bool setMinimumValue(qreal &storage, qreal value, qreal minimum)
{
    const qreal boundedValue = qMax(minimum, value);
    if (qFuzzyCompare(storage, boundedValue)) {
        return false;
    }

    storage = boundedValue;
    return true;
}

QString normalizedShapeKind(const QString &shapeKind)
{
    const QString normalized = shapeKind.trimmed().toLower();
    if (normalized == QStringLiteral("triagle")) {
        return QStringLiteral("triangle");
    }

    static const QStringList allowedShapes{
        QStringLiteral("rectangle"),
        QStringLiteral("ellipse"),
        QStringLiteral("triangle"),
        QStringLiteral("diamond"),
        QStringLiteral("star"),
        QStringLiteral("rectanglebubble"),
        QStringLiteral("ellipsebubble")
    };

    return allowedShapes.contains(normalized)
        ? normalized
        : QStringLiteral("rectangle");
}

} // namespace

CanvasDocumentViewModel::CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent)
    : QObject(parent)
    , m_palette(buildDefaultPalette(paletteUtils))
    , m_brushColor(QStringLiteral("#1a1a1a"))
{
}

QVariantList CanvasDocumentViewModel::palette() const
{
    return m_palette;
}

QColor CanvasDocumentViewModel::brushColor() const
{
    return m_brushColor;
}

qreal CanvasDocumentViewModel::brushSize() const
{
    return m_brushSize;
}

qreal CanvasDocumentViewModel::brushFlow() const
{
    return m_brushFlow;
}

qreal CanvasDocumentViewModel::brushOpacity() const
{
    return m_brushOpacity;
}

qreal CanvasDocumentViewModel::brushHardness() const
{
    return m_brushHardness;
}

qreal CanvasDocumentViewModel::brushSpacing() const
{
    return m_brushSpacing;
}

qreal CanvasDocumentViewModel::brushSpacingRatio() const
{
    return m_brushSpacingRatio;
}

qreal CanvasDocumentViewModel::pressureCurveMinimum() const
{
    return m_pressureCurveMinimum;
}

qreal CanvasDocumentViewModel::pressureCurveCenter() const
{
    return m_pressureCurveCenter;
}

qreal CanvasDocumentViewModel::pressureCurveMaximum() const
{
    return m_pressureCurveMaximum;
}

bool CanvasDocumentViewModel::brushPressureControlsOpacity() const
{
    return m_brushPressureControlsOpacity;
}

qreal CanvasDocumentViewModel::stabilizerStrength() const
{
    return m_stabilizerStrength;
}

QString CanvasDocumentViewModel::toolMode() const
{
    return m_toolMode;
}

QString CanvasDocumentViewModel::shapeKind() const
{
    return m_shapeKind;
}

int CanvasDocumentViewModel::canvasWidth() const
{
    return m_canvasWidth;
}

int CanvasDocumentViewModel::canvasHeight() const
{
    return m_canvasHeight;
}

void CanvasDocumentViewModel::setBrushColor(const QColor &brushColor)
{
    if (m_brushColor == brushColor) {
        return;
    }

    m_brushColor = brushColor;
    emit brushColorChanged();
}

void CanvasDocumentViewModel::setBrushSize(qreal brushSize)
{
    const qreal boundedSize = qBound<qreal>(1.0, brushSize, 48.0);
    if (qFuzzyCompare(m_brushSize, boundedSize)) {
        return;
    }

    m_brushSize = boundedSize;
    emit brushSizeChanged();
}

void CanvasDocumentViewModel::setBrushFlow(qreal brushFlow)
{
    if (setBoundedValue(m_brushFlow, brushFlow, 0.0, 1.0)) {
        emit brushFlowChanged();
    }
}

void CanvasDocumentViewModel::setBrushOpacity(qreal brushOpacity)
{
    if (setBoundedValue(m_brushOpacity, brushOpacity, 0.0, 1.0)) {
        emit brushOpacityChanged();
    }
}

void CanvasDocumentViewModel::setBrushHardness(qreal brushHardness)
{
    if (setBoundedValue(m_brushHardness, brushHardness, 0.01, 1.0)) {
        emit brushHardnessChanged();
    }
}

void CanvasDocumentViewModel::setBrushSpacing(qreal brushSpacing)
{
    if (setMinimumValue(m_brushSpacing, brushSpacing, 0.0)) {
        emit brushSpacingChanged();
    }
}

void CanvasDocumentViewModel::setBrushSpacingRatio(qreal brushSpacingRatio)
{
    if (setBoundedValue(m_brushSpacingRatio, brushSpacingRatio, 0.0, 1.0)) {
        emit brushSpacingRatioChanged();
    }
}

void CanvasDocumentViewModel::setPressureCurveMinimum(qreal pressureCurveMinimum)
{
    const qreal nextMinimum = qBound<qreal>(0.0, pressureCurveMinimum, 1.0);
    const qreal nextCenter = qMax(m_pressureCurveCenter, nextMinimum);
    const qreal nextMaximum = qMax(m_pressureCurveMaximum, nextMinimum);
    const bool minimumChanged = !qFuzzyCompare(m_pressureCurveMinimum, nextMinimum);
    const bool centerChanged = !qFuzzyCompare(m_pressureCurveCenter, nextCenter);
    const bool maximumChanged = !qFuzzyCompare(m_pressureCurveMaximum, nextMaximum);

    if (!minimumChanged && !centerChanged && !maximumChanged) {
        return;
    }

    m_pressureCurveMinimum = nextMinimum;
    m_pressureCurveCenter = nextCenter;
    m_pressureCurveMaximum = nextMaximum;

    if (minimumChanged) {
        emit pressureCurveMinimumChanged();
    }
    if (centerChanged) {
        emit pressureCurveCenterChanged();
    }
    if (maximumChanged) {
        emit pressureCurveMaximumChanged();
    }
}

void CanvasDocumentViewModel::setPressureCurveCenter(qreal pressureCurveCenter)
{
    const qreal nextCenter = qBound(m_pressureCurveMinimum, pressureCurveCenter, m_pressureCurveMaximum);
    if (qFuzzyCompare(m_pressureCurveCenter, nextCenter)) {
        return;
    }

    m_pressureCurveCenter = nextCenter;
    emit pressureCurveCenterChanged();
}

void CanvasDocumentViewModel::setPressureCurveMaximum(qreal pressureCurveMaximum)
{
    const qreal nextMaximum = qBound<qreal>(0.0, pressureCurveMaximum, 1.0);
    const qreal nextMinimum = qMin(m_pressureCurveMinimum, nextMaximum);
    const qreal nextCenter = qBound(nextMinimum, m_pressureCurveCenter, nextMaximum);
    const bool minimumChanged = !qFuzzyCompare(m_pressureCurveMinimum, nextMinimum);
    const bool centerChanged = !qFuzzyCompare(m_pressureCurveCenter, nextCenter);
    const bool maximumChanged = !qFuzzyCompare(m_pressureCurveMaximum, nextMaximum);

    if (!minimumChanged && !centerChanged && !maximumChanged) {
        return;
    }

    m_pressureCurveMinimum = nextMinimum;
    m_pressureCurveCenter = nextCenter;
    m_pressureCurveMaximum = nextMaximum;

    if (minimumChanged) {
        emit pressureCurveMinimumChanged();
    }
    if (centerChanged) {
        emit pressureCurveCenterChanged();
    }
    if (maximumChanged) {
        emit pressureCurveMaximumChanged();
    }
}

void CanvasDocumentViewModel::setBrushPressureControlsOpacity(bool enabled)
{
    if (m_brushPressureControlsOpacity == enabled) {
        return;
    }

    m_brushPressureControlsOpacity = enabled;
    emit brushPressureControlsOpacityChanged();
}

void CanvasDocumentViewModel::setStabilizerStrength(qreal stabilizerStrength)
{
    if (setBoundedValue(m_stabilizerStrength, stabilizerStrength, 0.0, 1.0)) {
        emit stabilizerStrengthChanged();
    }
}

void CanvasDocumentViewModel::setToolMode(const QString &toolMode)
{
    static const QStringList allowedTools{
        QStringLiteral("brush"),
        QStringLiteral("eraser"),
        QStringLiteral("pan"),
        QStringLiteral("move"),
        QStringLiteral("zoom"),
        QStringLiteral("shape"),
        QStringLiteral("fill"),
        QStringLiteral("text"),
    };

    const QString normalizedTool = allowedTools.contains(toolMode)
        ? toolMode
        : QStringLiteral("brush");
    if (m_toolMode == normalizedTool) {
        return;
    }

    m_toolMode = normalizedTool;
    emit toolModeChanged();
}

void CanvasDocumentViewModel::setShapeKind(const QString &shapeKind)
{
    const QString normalizedKind = normalizedShapeKind(shapeKind);
    if (m_shapeKind == normalizedKind) {
        return;
    }

    m_shapeKind = normalizedKind;
    emit shapeKindChanged();
}

void CanvasDocumentViewModel::setCanvasWidth(int canvasWidth)
{
    const int boundedWidth = qMax(1, canvasWidth);
    if (m_canvasWidth == boundedWidth) {
        return;
    }

    m_canvasWidth = boundedWidth;
    emit canvasWidthChanged();
}

void CanvasDocumentViewModel::setCanvasHeight(int canvasHeight)
{
    const int boundedHeight = qMax(1, canvasHeight);
    if (m_canvasHeight == boundedHeight) {
        return;
    }

    m_canvasHeight = boundedHeight;
    emit canvasHeightChanged();
}

QVariantList CanvasDocumentViewModel::buildDefaultPalette(PaletteUtils *paletteUtils) const
{
    const QVariantList primary = {
        paletteEntry(QStringLiteral("Ink Black"), QStringLiteral("#1a1a1a")),
        paletteEntry(QStringLiteral("Signal Red"), QStringLiteral("#e53935")),
        paletteEntry(QStringLiteral("Amber"), QStringLiteral("#fb8c00")),
        paletteEntry(QStringLiteral("Sun Yellow"), QStringLiteral("#fdd835")),
        paletteEntry(QStringLiteral("Leaf Green"), QStringLiteral("#43a047")),
        paletteEntry(QStringLiteral("Sky Blue"), QStringLiteral("#1e88e5")),
        paletteEntry(QStringLiteral("Violet"), QStringLiteral("#5e35b1")),
        paletteEntry(QStringLiteral("Clay"), QStringLiteral("#8d6e63")),
        paletteEntry(QStringLiteral("Pure White"), QStringLiteral("#ffffff")),
        paletteEntry(QStringLiteral("Pitch Black"), QStringLiteral("#000000"))
    };

    const QVariantList extended = {
        paletteEntry(QStringLiteral("Coral"), QStringLiteral("#ff7043")),
        paletteEntry(QStringLiteral("Rose"), QStringLiteral("#f06292")),
        paletteEntry(QStringLiteral("Lilac"), QStringLiteral("#ba68c8")),
        paletteEntry(QStringLiteral("Cerulean"), QStringLiteral("#0091ea")),
        paletteEntry(QStringLiteral("Seafoam"), QStringLiteral("#26c6da")),
        paletteEntry(QStringLiteral("Forest"), QStringLiteral("#2e7d32")),
        paletteEntry(QStringLiteral("Olive"), QStringLiteral("#827717")),
        paletteEntry(QStringLiteral("Burnt Sienna"), QStringLiteral("#d84315")),
        paletteEntry(QStringLiteral("Slate"), QStringLiteral("#546e7a"))
    };

    if (paletteUtils) {
        return paletteUtils->buildDefaultPalette(primary, extended);
    }

    QVariantList fallback = primary;
    fallback.append(extended);
    return fallback;
}
