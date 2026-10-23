#include <QSignalSpy>
#include <QtTest>
#include <limits>

#include "canvasdocumentviewmodel.h"
#include "paletteutils.h"

class tst_CanvasDocumentViewModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultDocumentState();
    void updatesBrushStateAndClampsValues();
    void updatesEngineBrushSettingsAndClampsValues();
    void acceptsOnlySupportedTools();
    void acceptsOnlySupportedShapeKinds();
    void keepsCanvasDimensionsPositive();
    void adaptsTheApplicationEngineDocument();
    void rejectsNonFiniteBrushValuesWithoutNotifyingQml();
    void paletteAdapterPreservesColorsAndOrdering();
};

void tst_CanvasDocumentViewModel::exposesDefaultDocumentState()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVERIFY(!viewModel.palette().isEmpty());
    QCOMPARE(viewModel.brushColor(), QColor(QStringLiteral("#1a1a1a")));
    QCOMPARE(viewModel.brushSize(), 2.0);
    QCOMPARE(viewModel.brushFlow(), 1.0);
    QCOMPARE(viewModel.brushOpacity(), 1.0);
    QCOMPARE(CanvasDocumentViewModel::maximumAntialiasingBrushHardness(), 1.0);
    QCOMPARE(viewModel.brushHardness(), CanvasDocumentViewModel::maximumAntialiasingBrushHardness());
    QCOMPARE(viewModel.brushSpacing(), 0.0);
    QCOMPARE(viewModel.brushSpacingRatio(), 0.0);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.0);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.5);
    QCOMPARE(viewModel.pressureCurveMaximum(), 1.0);
    QCOMPARE(viewModel.brushPressureControlsOpacity(), true);
    QCOMPARE(viewModel.stabilizerStrength(), 0.0);
    QCOMPARE(viewModel.toolMode(), QStringLiteral("brush"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("rectangle"));
    QCOMPARE(viewModel.canvasWidth(), 1);
    QCOMPARE(viewModel.canvasHeight(), 1);
}

void tst_CanvasDocumentViewModel::updatesBrushStateAndClampsValues()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy colorSpy(&viewModel, &CanvasDocumentViewModel::brushColorChanged);
    QSignalSpy sizeSpy(&viewModel, &CanvasDocumentViewModel::brushSizeChanged);

    viewModel.setBrushColor(QColor(QStringLiteral("#ff7043")));
    QCOMPARE(viewModel.brushColor(), QColor(QStringLiteral("#ff7043")));
    QCOMPARE(colorSpy.count(), 1);

    viewModel.setBrushSize(0.2);
    QCOMPARE(viewModel.brushSize(), 1.0);

    viewModel.setBrushSize(80.0);
    QCOMPARE(viewModel.brushSize(), 48.0);
    QCOMPARE(sizeSpy.count(), 2);

    viewModel.setBrushSize(48.0);
    QCOMPARE(sizeSpy.count(), 2);
}

void tst_CanvasDocumentViewModel::updatesEngineBrushSettingsAndClampsValues()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy flowSpy(&viewModel, &CanvasDocumentViewModel::brushFlowChanged);
    QSignalSpy hardnessSpy(&viewModel, &CanvasDocumentViewModel::brushHardnessChanged);
    QSignalSpy pressureCenterSpy(&viewModel, &CanvasDocumentViewModel::pressureCurveCenterChanged);
    QSignalSpy pressureOpacitySpy(&viewModel, &CanvasDocumentViewModel::brushPressureControlsOpacityChanged);

    viewModel.setBrushFlow(0.35);
    QCOMPARE(viewModel.brushFlow(), 0.35);
    QCOMPARE(flowSpy.count(), 1);

    viewModel.setBrushFlow(-1.0);
    QCOMPARE(viewModel.brushFlow(), 0.0);
    QCOMPARE(flowSpy.count(), 2);

    viewModel.setBrushOpacity(2.0);
    QCOMPARE(viewModel.brushOpacity(), 1.0);

    viewModel.setBrushHardness(0.0);
    QCOMPARE(viewModel.brushHardness(), 0.01);
    QCOMPARE(hardnessSpy.count(), 1);

    viewModel.setBrushSpacing(-4.0);
    QCOMPARE(viewModel.brushSpacing(), 0.0);
    viewModel.setBrushSpacing(18.5);
    QCOMPARE(viewModel.brushSpacing(), 18.5);

    viewModel.setBrushSpacingRatio(1.4);
    QCOMPARE(viewModel.brushSpacingRatio(), 1.0);

    viewModel.setPressureCurveMinimum(0.7);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.7);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.7);
    QCOMPARE(viewModel.pressureCurveMaximum(), 1.0);
    QCOMPARE(pressureCenterSpy.count(), 1);

    viewModel.setPressureCurveMaximum(0.4);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.4);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.4);
    QCOMPARE(viewModel.pressureCurveMaximum(), 0.4);
    QCOMPARE(pressureCenterSpy.count(), 2);

    viewModel.setBrushPressureControlsOpacity(false);
    QCOMPARE(viewModel.brushPressureControlsOpacity(), false);
    QCOMPARE(pressureOpacitySpy.count(), 1);
    viewModel.setBrushPressureControlsOpacity(false);
    QCOMPARE(pressureOpacitySpy.count(), 1);
    viewModel.setBrushPressureControlsOpacity(true);
    QCOMPARE(viewModel.brushPressureControlsOpacity(), true);
    QCOMPARE(pressureOpacitySpy.count(), 2);

    viewModel.setStabilizerStrength(1.5);
    QCOMPARE(viewModel.stabilizerStrength(), 1.0);
}

void tst_CanvasDocumentViewModel::acceptsOnlySupportedTools()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy toolSpy(&viewModel, &CanvasDocumentViewModel::toolModeChanged);

    viewModel.setToolMode(QStringLiteral("eraser"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("eraser"));

    viewModel.setToolMode(QStringLiteral("text"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("text"));

    viewModel.setToolMode(QStringLiteral("shape"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("shape"));

    viewModel.setToolMode(QStringLiteral("fill"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("fill"));

    viewModel.setToolMode(QStringLiteral("pan"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("pan"));

    viewModel.setToolMode(QStringLiteral("move"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("move"));

    viewModel.setToolMode(QStringLiteral("zoom"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("zoom"));

    viewModel.setToolMode(QStringLiteral("translate"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("brush"));
    QCOMPARE(toolSpy.count(), 8);
}

void tst_CanvasDocumentViewModel::acceptsOnlySupportedShapeKinds()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy shapeSpy(&viewModel, &CanvasDocumentViewModel::shapeKindChanged);

    viewModel.setShapeKind(QStringLiteral("ellipse"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("ellipse"));

    viewModel.setShapeKind(QStringLiteral("triangle"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("triangle"));

    viewModel.setShapeKind(QStringLiteral("triagle"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("triangle"));

    viewModel.setShapeKind(QStringLiteral("diamond"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("diamond"));

    viewModel.setShapeKind(QStringLiteral("star"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("star"));

    viewModel.setShapeKind(QStringLiteral("rectanglebubble"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("rectanglebubble"));

    viewModel.setShapeKind(QStringLiteral("ellipsebubble"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("ellipsebubble"));

    viewModel.setShapeKind(QStringLiteral("unsupported"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("rectangle"));

    viewModel.setShapeKind(QStringLiteral("rectangle"));
    QCOMPARE(shapeSpy.count(), 7);
}

void tst_CanvasDocumentViewModel::keepsCanvasDimensionsPositive()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    viewModel.setCanvasWidth(640);
    viewModel.setCanvasHeight(480);
    QCOMPARE(viewModel.canvasWidth(), 640);
    QCOMPARE(viewModel.canvasHeight(), 480);

    viewModel.setCanvasWidth(0);
    viewModel.setCanvasHeight(-4);
    QCOMPARE(viewModel.canvasWidth(), 1);
    QCOMPARE(viewModel.canvasHeight(), 1);
}

void tst_CanvasDocumentViewModel::adaptsTheApplicationEngineDocument()
{
    congregation::CanvasDocument document;
    QVERIFY(document.queries().send(congregation::SetReal{congregation::RealProperty::brushSize, 12}));
    CanvasDocumentViewModel viewModel(document);
    QCOMPARE(viewModel.brushSize(), 12.0);
    QSignalSpy sizeSpy(&viewModel, &CanvasDocumentViewModel::brushSizeChanged);
    viewModel.setBrushSize(16);
    QCOMPARE(document.snapshot().brushSize, 16.0);
    QCOMPARE(sizeSpy.size(), 1);
    const auto revision = document.snapshot().revision;
    viewModel.setBrushSize(16);
    QCOMPARE(sizeSpy.size(), 1);
    QCOMPARE(document.snapshot().revision, revision);
    viewModel.setShapeKind(QString::fromUtf8("\xe2\x80\x83TRIAGLE\xe2\x80\x83"));
    QCOMPARE(viewModel.shapeKind(), QStringLiteral("triangle"));
}

void tst_CanvasDocumentViewModel::rejectsNonFiniteBrushValuesWithoutNotifyingQml()
{
    congregation::CanvasDocument document;
    CanvasDocumentViewModel viewModel(document);
    QSignalSpy flowSpy(&viewModel, &CanvasDocumentViewModel::brushFlowChanged);
    viewModel.setBrushFlow(std::numeric_limits<double>::quiet_NaN());
    viewModel.setBrushFlow(std::numeric_limits<double>::infinity());
    QCOMPARE(viewModel.brushFlow(), 1.0);
    QCOMPARE(flowSpy.size(), 0);
    QCOMPARE(document.snapshot().revision, std::uint64_t(0));
}

void tst_CanvasDocumentViewModel::paletteAdapterPreservesColorsAndOrdering()
{
    PaletteUtils adapter;
    auto entry = [](const QString &name, const QVariant &color) {
        return QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("color"), color}};
    };
    const auto ordered = adapter.buildDefaultPalette(
        {entry(QStringLiteral("white"), QStringLiteral("#ffffff")),
         entry(QStringLiteral("red"), QColor(Qt::red)),
         entry(QStringLiteral("black"), QColor(Qt::black))},
        {entry(QStringLiteral("duplicate"), QStringLiteral("#FF0000")),
         entry(QStringLiteral("invalid"), QStringLiteral("invalid"))});
    QCOMPARE(ordered.size(), 3);
    QCOMPARE(ordered[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("black"));
    QCOMPARE(ordered[1].toMap().value(QStringLiteral("color")).value<QColor>(), QColor(Qt::red));
    QCOMPARE(ordered[2].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("white"));
}

QTEST_APPLESS_MAIN(tst_CanvasDocumentViewModel)

#include "tst_canvasdocumentviewmodel.moc"
