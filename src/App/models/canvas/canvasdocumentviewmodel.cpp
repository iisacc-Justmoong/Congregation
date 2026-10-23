#include "canvasdocumentviewmodel.h"
#include "../../host/enginevalues.h"

using namespace congregation;

CanvasDocumentViewModel::CanvasDocumentViewModel(PaletteUtils *, QObject *parent)
    : QObject(parent)
    , m_ownedDocument(std::make_unique<CanvasDocument>())
    , m_document(m_ownedDocument.get())
    , m_palette(presentation::toPalette(defaultPalette()))
{
}

CanvasDocumentViewModel::CanvasDocumentViewModel(CanvasDocument &document, QObject *parent)
    : QObject(parent)
    , m_document(&document)
    , m_palette(presentation::toPalette(defaultPalette()))
{
}

QVariantList CanvasDocumentViewModel::palette() const
{
    return m_palette;
}

QColor CanvasDocumentViewModel::brushColor() const
{
    return presentation::toColor(m_document->snapshot().brushColor);
}

void CanvasDocumentViewModel::setBrushColor(const QColor &color)
{
    if (brushColor() != color)
    {
        apply(SetColor{presentation::fromColor(color)});
    }
}

qreal CanvasDocumentViewModel::brushSize() const
{
    return m_document->snapshot().brushSize;
}

void CanvasDocumentViewModel::setBrushSize(qreal value)
{
    apply(SetReal{RealProperty::brushSize, value});
}

qreal CanvasDocumentViewModel::brushFlow() const
{
    return m_document->snapshot().brushFlow;
}

void CanvasDocumentViewModel::setBrushFlow(qreal value)
{
    apply(SetReal{RealProperty::brushFlow, value});
}

qreal CanvasDocumentViewModel::brushOpacity() const
{
    return m_document->snapshot().brushOpacity;
}

void CanvasDocumentViewModel::setBrushOpacity(qreal value)
{
    apply(SetReal{RealProperty::brushOpacity, value});
}

qreal CanvasDocumentViewModel::brushHardness() const
{
    return m_document->snapshot().brushHardness;
}

void CanvasDocumentViewModel::setBrushHardness(qreal value)
{
    apply(SetReal{RealProperty::brushHardness, value});
}

qreal CanvasDocumentViewModel::brushSpacing() const
{
    return m_document->snapshot().brushSpacing;
}

void CanvasDocumentViewModel::setBrushSpacing(qreal value)
{
    apply(SetReal{RealProperty::brushSpacing, value});
}

qreal CanvasDocumentViewModel::brushSpacingRatio() const
{
    return m_document->snapshot().brushSpacingRatio;
}

void CanvasDocumentViewModel::setBrushSpacingRatio(qreal value)
{
    apply(SetReal{RealProperty::brushSpacingRatio, value});
}

qreal CanvasDocumentViewModel::pressureCurveMinimum() const
{
    return m_document->snapshot().pressureMinimum;
}

void CanvasDocumentViewModel::setPressureCurveMinimum(qreal value)
{
    apply(SetReal{RealProperty::pressureMinimum, value});
}

qreal CanvasDocumentViewModel::pressureCurveCenter() const
{
    return m_document->snapshot().pressureCenter;
}

void CanvasDocumentViewModel::setPressureCurveCenter(qreal value)
{
    apply(SetReal{RealProperty::pressureCenter, value});
}

qreal CanvasDocumentViewModel::pressureCurveMaximum() const
{
    return m_document->snapshot().pressureMaximum;
}

void CanvasDocumentViewModel::setPressureCurveMaximum(qreal value)
{
    apply(SetReal{RealProperty::pressureMaximum, value});
}

qreal CanvasDocumentViewModel::stabilizerStrength() const
{
    return m_document->snapshot().stabilizerStrength;
}

void CanvasDocumentViewModel::setStabilizerStrength(qreal value)
{
    apply(SetReal{RealProperty::stabilizerStrength, value});
}

bool CanvasDocumentViewModel::brushPressureControlsOpacity() const
{
    return m_document->snapshot().pressureControlsOpacity;
}

void CanvasDocumentViewModel::setBrushPressureControlsOpacity(bool value)
{
    apply(SetPressureOpacity{value});
}

QString CanvasDocumentViewModel::toolMode() const
{
    return QString::fromStdString(m_document->snapshot().tool);
}

void CanvasDocumentViewModel::setToolMode(const QString &value)
{
    apply(SetTool{value.toStdString()});
}

QString CanvasDocumentViewModel::shapeKind() const
{
    return QString::fromStdString(m_document->snapshot().shape);
}

void CanvasDocumentViewModel::setShapeKind(const QString &value)
{
    apply(SetShape{value.trimmed().toLower().toStdString()});
}

int CanvasDocumentViewModel::canvasWidth() const
{
    return m_document->snapshot().canvasWidth;
}

void CanvasDocumentViewModel::setCanvasWidth(int value)
{
    apply(SetDimension{Dimension::width, value});
}

int CanvasDocumentViewModel::canvasHeight() const
{
    return m_document->snapshot().canvasHeight;
}

void CanvasDocumentViewModel::setCanvasHeight(int value)
{
    apply(SetDimension{Dimension::height, value});
}

void CanvasDocumentViewModel::apply(CanvasQuery query)
{
    const auto previous = m_document->snapshot();
    const auto reply = m_document->queries().send(query);
    if (!reply)
    {
        qFatal("Congregation canvas query failed on its presentation thread");
    }
    const auto &next = *reply;
    if (previous.brushColor != next.brushColor) emit brushColorChanged();
    if (previous.brushSize != next.brushSize) emit brushSizeChanged();
    if (previous.brushFlow != next.brushFlow) emit brushFlowChanged();
    if (previous.brushOpacity != next.brushOpacity) emit brushOpacityChanged();
    if (previous.brushHardness != next.brushHardness) emit brushHardnessChanged();
    if (previous.brushSpacing != next.brushSpacing) emit brushSpacingChanged();
    if (previous.brushSpacingRatio != next.brushSpacingRatio) emit brushSpacingRatioChanged();
    if (previous.pressureMinimum != next.pressureMinimum) emit pressureCurveMinimumChanged();
    if (previous.pressureCenter != next.pressureCenter) emit pressureCurveCenterChanged();
    if (previous.pressureMaximum != next.pressureMaximum) emit pressureCurveMaximumChanged();
    if (previous.stabilizerStrength != next.stabilizerStrength) emit stabilizerStrengthChanged();
    if (previous.pressureControlsOpacity != next.pressureControlsOpacity) emit brushPressureControlsOpacityChanged();
    if (previous.tool != next.tool) emit toolModeChanged();
    if (previous.shape != next.shape) emit shapeKindChanged();
    if (previous.canvasWidth != next.canvasWidth) emit canvasWidthChanged();
    if (previous.canvasHeight != next.canvasHeight) emit canvasHeightChanged();
}
