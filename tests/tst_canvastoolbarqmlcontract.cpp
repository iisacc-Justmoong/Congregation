#include <QFile>
#include <QString>
#include <QtTest>

class tst_CanvasToolBarQmlContract : public QObject
{
    Q_OBJECT

  private slots:
    void brushReselectionOpensBrushSettingsMenu();
    void colorSelectionUsesHslTrianglePicker();
    void colorPickerMovesIntoLeftClusterAndProfileStaysTrailing();
    void profileInvitationBadgeUsesDecisionContextMenu();
    void toolbarUsesStockLvrsControlGeometry();
    void drawingSurfaceProvidesPaintStyleTextToolEditor();
    void clipboardPasteCreatesCenteredTransformableImageObject();
    void shapeToolUsesStockIconMenuAndDragInsertion();
    void brushSizeControlsFlowFromDecreaseToSliderToIncrease();
    void toolbarUsesFullWidthRectangularBackground();
    void toolbarIsOffsetBelowApplicationWindowTopChrome();
    void modifierSpaceUsesApplicationWideCameraModes();
    void painterPageUsesLvHierarchyLayerPanel();
    void pressureCurveControlsUseThreePointGraphAtBottom();
    void brushSettingsExposePressureOpacityToggle();
    void recentCanvasUsesDebouncedInternalContainer();
    void remoteParticipantsMutateOnlyTheHostCanvas();
};

void tst_CanvasToolBarQmlContract::recentCanvasUsesDebouncedInternalContainer()
{
    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(painterPageQml.readAll());
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(!pageSource.contains(QStringLiteral("signal canvasFileActivated(url fileUrl)")));
    QVERIFY(
        pageSource.contains(QStringLiteral("const saved = drawingSurface.saveToFile(fileUrl);")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.scheduleRecentCanvasSave();")));
    QVERIFY(pageSource.contains(QStringLiteral("return saved;")));
    QVERIFY(
        pageSource.contains(QStringLiteral("const opened = drawingSurface.openRaster(fileUrl);")));
    QVERIFY(pageSource.contains(QStringLiteral("return opened;")));
    QVERIFY(pageSource.contains(QStringLiteral("id: recentCanvasSaveTimer")));
    QVERIFY(pageSource.contains(QStringLiteral("interval: 1200")));
    QVERIFY(pageSource.contains(QStringLiteral("function saveRecentCanvasNow()")));
    QVERIFY(pageSource.contains(QStringLiteral("function flushRecentCanvasSave()")));
    QVERIFY(pageSource.contains(QStringLiteral("Qt.callLater(function ()")));
    QVERIFY(pageSource.contains(
        QStringLiteral("CongregationApplicationPreferences.recentCanvasStorageUrl")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.saveRecentCanvas(")));
    QVERIFY(pageSource.contains(
        QStringLiteral("CongregationApplicationPreferences.recordRecentCanvas(storageUrl)")));
    QVERIFY(pageSource.contains(QStringLiteral("function openRecentCanvas(fileUrl)")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.openRecentCanvas(fileUrl)")));
    QVERIFY(pageSource.contains(QStringLiteral("onSessionChanged:")));
    QVERIFY(surfaceSource.contains(QStringLiteral("signal sessionChanged")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function saveRecentCanvas(fileUrl)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.saveRecentCanvas(")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property bool recentCanvasRestoreInProgress: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function cancelPendingRecentCanvasRestore()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const restoreGeneration = ++surface.recentCanvasRestoreGeneration")));
    QVERIFY(surfaceSource.contains(QStringLiteral("if (!surface.recentCanvasRestoreInProgress)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.sessionChanged();")));
    QVERIFY(pageSource.contains(QStringLiteral("id: localCanvasPublishTimer")));
    QVERIFY(pageSource.contains(QStringLiteral("interval: 350")));
    QVERIFY(pageSource.contains(QStringLiteral("function scheduleLocalCanvasPublish()")));
    QVERIFY(pageSource.contains(QStringLiteral("function publishLocalCanvasSnapshotNow()")));
    QVERIFY(pageSource.contains(QStringLiteral("function applyRemoteCanvasSnapshot(snapshot)")));
    QVERIFY(pageSource.contains(QStringLiteral("target: painterPage.localCanvasSession")));
    QVERIFY(pageSource.contains(QStringLiteral("function onSnapshotRequested()")));
    QVERIFY(pageSource.contains(
        QStringLiteral("function onSnapshotReceived(snapshot, revision, originPeerId)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("signal sessionRestoreFinished(bool success)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function applyCanvasSessionSnapshot(snapshot)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.importCanvasSession(snapshot)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canvasSessionSnapshot()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.exportCanvasSession(")));
}

void tst_CanvasToolBarQmlContract::remoteParticipantsMutateOnlyTheHostCanvas()
{
    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(painterPageQml.readAll());
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(
        pageSource.contains(QStringLiteral("readonly property bool participantUsesHostCanvas:")));
    QVERIFY(pageSource.contains(QStringLiteral("localCanvasSession.connected")));
    QVERIFY(pageSource.contains(QStringLiteral("localCanvasSession.hosting")));
    QVERIFY(pageSource.contains(QStringLiteral("localCanvasSession.submitEditCommand(command)")));
    QVERIFY(pageSource.contains(QStringLiteral("property bool participantCanvasReady: false")));
    QVERIFY(pageSource.contains(QStringLiteral("function submitParticipantEditCommand(command)")));
    QVERIFY(pageSource.contains(QStringLiteral("!painterPage.participantCanvasReady")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "participantCanvasReady = Boolean(success && painterPage.participantUsesHostCanvas)")));
    QVERIFY(pageSource.contains(QStringLiteral("function onStateChanged()")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "onHostEditCommandRequested: command => painterPage.submitParticipantEditCommand(command)")));
    QVERIFY(pageSource.contains(
        QStringLiteral("function onEditCommandReceived(command, originPeerId)")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.applyHostEditCommand(command)")));
    QVERIFY(pageSource.contains(QStringLiteral("id: participantCanvasPublishTimer")));
    QVERIFY(pageSource.contains(QStringLiteral("interval: 0")));
    QVERIFY(pageSource.contains(QStringLiteral("function scheduleParticipantCanvasPublish()")));
    QVERIFY(pageSource.contains(QStringLiteral("function publishParticipantCanvasSnapshotNow()")));
    QVERIFY(pageSource.contains(QStringLiteral("const snapshot = drawingSurface.canvasSessionSnapshot();")));
    QVERIFY(pageSource.contains(QStringLiteral("localCanvasSession.publishSnapshot(snapshot)")));

    QVERIFY(surfaceSource.contains(QStringLiteral("property bool hostCanvasInputClient: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("signal hostEditCommandRequested(var command)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function requestHostEditCommand(command)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function applyHostEditCommand(command)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("remoteInputMode: surface.hostCanvasInputClient")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onRemoteStrokeRequested:")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"stroke\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"fill\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"add-text\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"add-shape\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"transform-object\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"new-canvas\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"ensure-region\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"clear-canvas\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"add-layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"delete-layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"rename-layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"reorder-layers\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"insert-image\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"open-raster\"")));
    QVERIFY(pageSource.contains(QStringLiteral("type: \"undo\"")));
    QVERIFY(pageSource.contains(QStringLiteral("type: \"redo\"")));
}

void tst_CanvasToolBarQmlContract::modifierSpaceUsesApplicationWideCameraModes()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    const QString mainCppPath = QFINDTESTDATA("../App/main.cpp");
    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!mainCppPath.isEmpty(), "main.cpp test data was not found");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QFile mainCpp(mainCppPath);
    QFile painterPageQml(painterPageQmlPath);
    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(mainCpp.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());
    const QString mainCppSource = QString::fromUtf8(mainCpp.readAll());
    const QString pageSource = QString::fromUtf8(painterPageQml.readAll());
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(!mainSource.contains(QStringLiteral("autoAttachRuntimeEvents: true")));
    QVERIFY(mainCppSource.contains(QStringLiteral("TemporaryCameraInput")));
    QVERIFY(
        mainCppSource.contains(QStringLiteral("app.installEventFilter(temporaryCameraInput);")));
    QVERIFY(mainCppSource.contains(
        QStringLiteral("engine.rootContext()->setContextProperty(\"CongregationTemporaryCameraInput\", "
                       "temporaryCameraInput);")));
    QCOMPARE(pageSource.count(QStringLiteral("target: CongregationTemporaryCameraInput")), 2);
    QVERIFY(pageSource.contains(QStringLiteral("property: \"enabled\"")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "value: drawingSurface.toolShortcutsEnabled && !drawingSurface.textEditingActive")));
    QVERIFY(pageSource.contains(QStringLiteral("function onModeChanged()")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "drawingSurface.setTemporaryCameraMode(CongregationTemporaryCameraInput.mode);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property string temporaryCameraMode: \"\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function setTemporaryCameraMode(mode)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginSpaceZoomMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("Qt.ControlModifier | Qt.MetaModifier")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("if (surface.effectiveToolMode() !== \"zoom\")")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("if (surface.textEditingActive || !surface.toolShortcutsEnabled)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "return surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor;")));
}

void tst_CanvasToolBarQmlContract::clipboardPasteCreatesCenteredTransformableImageObject()
{
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());
    const QString pageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property real imageInsertionMaximumCanvasRatio: 0.8")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int imageInsertionDuplicateOffset: 16")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function clipboardImagePlacement(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function pasteClipboardImage()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.clipboardImageObject(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function insertDroppedImageObject(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: canvasImageDropArea")));
    QVERIFY(surfaceSource.contains(QStringLiteral("parent: canvasSurface")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.canImportDroppedImage(drag)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.importDroppedImage(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("drop.acceptProposedAction();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onDroppedImageReady:")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onDroppedImageFailed:")));
    QVERIFY(surfaceSource.contains(QStringLiteral("signal imageDropSucceeded")));
    QVERIFY(surfaceSource.contains(QStringLiteral("signal imageDropFailed(string errorCode)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property string clipboardImagePasteErrorCode: \"\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("clipboardObject.status")));
    QVERIFY(surfaceSource.contains(QStringLiteral("status !== \"ready\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("isFinite(objectWidth)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"image\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("name: String(imageObject.suggestedName || fallbackName)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("qsTr(\"Pasted Image\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("x: placement.x")));
    QVERIFY(surfaceSource.contains(QStringLiteral("y: placement.y")));
    QVERIFY(surfaceSource.contains(QStringLiteral("appendDrawableObject(insertedObject);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("drawableObject.type === \"image\" || drawableObject.type === \"text\" || "
                       "drawableObject.type === \"shape\"")));

    QVERIFY(pageSource.contains(QStringLiteral("function pasteClipboardImage()")));
    QVERIFY(
        pageSource.contains(QStringLiteral("signal clipboardImagePasteFailed(string errorCode)")));
    QVERIFY(pageSource.contains(
        QStringLiteral("const pasted = drawingSurface.pasteClipboardImage();")));
    QVERIFY(pageSource.contains(QStringLiteral("clipboardImagePasteFailed(")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.setToolMode(\"move\");")));
    QVERIFY(pageSource.contains(QStringLiteral("onImageDropSucceeded:")));
    QVERIFY(pageSource.contains(QStringLiteral("onImageDropFailed:")));
}

void tst_CanvasToolBarQmlContract::brushReselectionOpensBrushSettingsMenu()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(source.contains(QStringLiteral("function activateBrushTool(triggerItem)")));
    QVERIFY(source.contains(QStringLiteral("if (toolbar.currentTool === \"brush\")")));
    QVERIFY(source.contains(QStringLiteral("toolbar.openBrushSettingsMenu(triggerItem);")));
    QVERIFY(source.contains(QStringLiteral("toolbar.toolSelected(\"brush\");")));
    QVERIFY(
        source.contains(QStringLiteral("onClicked: toolbar.activateBrushTool(brushToolButton)")));
    QVERIFY(!source.contains(QStringLiteral("acceptedButtons: Qt.RightButton")));
}

void tst_CanvasToolBarQmlContract::colorSelectionUsesHslTrianglePicker()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());
    const qsizetype colorPickerIndex = toolbarSource.indexOf(QStringLiteral("id: colorPickerMenu"));
    const qsizetype shapeMenuIndex = toolbarSource.indexOf(QStringLiteral("id: shapeMenu"));
    const qsizetype brushSettingsIndex =
        toolbarSource.indexOf(QStringLiteral("id: brushSettingsMenu"));
    const qsizetype currentColorButtonIndex =
        toolbarSource.indexOf(QStringLiteral("id: currentColorButton"));
    const qsizetype currentColorComponentIndex =
        toolbarSource.lastIndexOf(QStringLiteral("ToolbarCircleButton {"), currentColorButtonIndex);
    const qsizetype circleButtonDefinitionIndex =
        toolbarSource.indexOf(QStringLiteral("component ToolbarCircleButton: LV.IconButton"));
    const qsizetype brushPropertySliderIndex =
        toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider: Item"));
    const qsizetype spacerAfterColorIndex =
        toolbarSource.indexOf(QStringLiteral("LV.Spacer {}"), currentColorButtonIndex);
    QVERIFY(colorPickerIndex >= 0);
    QVERIFY(shapeMenuIndex > colorPickerIndex);
    QVERIFY(brushSettingsIndex > colorPickerIndex);
    QVERIFY(currentColorButtonIndex > brushSettingsIndex);
    QVERIFY(currentColorComponentIndex >= 0);
    QVERIFY(currentColorComponentIndex < currentColorButtonIndex);
    QVERIFY(circleButtonDefinitionIndex >= 0);
    QVERIFY(brushPropertySliderIndex > circleButtonDefinitionIndex);
    QVERIFY(spacerAfterColorIndex > currentColorButtonIndex);
    const QString colorPickerSource =
        toolbarSource.mid(colorPickerIndex, shapeMenuIndex - colorPickerIndex);
    const QString circleButtonDefinition = toolbarSource.mid(
        circleButtonDefinitionIndex, brushPropertySliderIndex - circleButtonDefinitionIndex);
    const QString currentColorButtonSource = toolbarSource.mid(
        currentColorComponentIndex, spacerAfterColorIndex - currentColorComponentIndex);

    const QString pickerQmlPath = QFINDTESTDATA("../App/qml/brush/HslTriangleColorPicker.qml");
    QVERIFY2(!pickerQmlPath.isEmpty(), "HslTriangleColorPicker.qml test data was not found");

    QFile pickerQml(pickerQmlPath);
    QVERIFY(pickerQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pickerSource = QString::fromUtf8(pickerQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("Controls.Popup")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: colorPickerMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openColorPickerMenu(triggerItem)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("HslTriangleColorPicker")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("onColorSelected: selectedColor => toolbar.colorPicked(selectedColor)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Brush color\")")));
    QVERIFY(circleButtonDefinition.contains(QStringLiteral("tone: LV.AbstractButton.Borderless")));
    QVERIFY(circleButtonDefinition.contains(QStringLiteral("contentItem: Rectangle {")));
    QCOMPARE(circleButtonDefinition.count(QStringLiteral("Rectangle {")), 1);
    QVERIFY(circleButtonDefinition.contains(QStringLiteral("radius: Math.min(width, height) / 2")));
    QVERIFY(currentColorButtonSource.contains(QStringLiteral("circleColor: toolbar.currentColor")));
    QVERIFY(currentColorButtonSource.contains(
        QStringLiteral("onClicked: toolbar.openColorPickerMenu(currentColorButton)")));
    QVERIFY(circleButtonDefinition.contains(
        QStringLiteral("property real circleBorderWidth: LV.Theme.scaleMetric(2)")));
    QVERIFY(circleButtonDefinition.contains(
        QStringLiteral("border.width: circleButton.circleBorderWidth")));
    QVERIFY(circleButtonDefinition.contains(QStringLiteral("border.color: \"#ffffff\"")));
    QVERIFY(!currentColorButtonSource.contains(QStringLiteral("MouseArea")));
    QVERIFY(!currentColorButtonSource.contains(QStringLiteral("Canvas")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: colorPickerBall")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: colorButtonMouseArea")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarColorSwatchRingSize")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("function paintRgbRainbowBall()")));
    QVERIFY(
        !toolbarSource.contains(QStringLiteral("onPaint: colorPickerBall.paintRgbRainbowBall()")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("Repeater")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("model: toolbar.palette")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("component ColorSwatch")));

    QVERIFY(pickerSource.contains(QStringLiteral("signal colorSelected(color selectedColor)")));
    QVERIFY(pickerSource.contains(QStringLiteral("function colorFromHue(hueValue)")));
    QVERIFY(pickerSource.contains(QStringLiteral("function colorFromTrianglePoint(point)")));
    QVERIFY(pickerSource.contains(QStringLiteral("Qt.hsla")));
    QVERIFY(pickerSource.contains(QStringLiteral("Canvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("hueWheelCanvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("triangleCanvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("context.fillRect(x, y, 1, 1)")));
    QVERIFY(pickerSource.contains(QStringLiteral("picker.colorFromWeights(picker.selectedHue")));
    const qsizetype pointFromWeightsIndex =
        pickerSource.indexOf(QStringLiteral("function pointFromWeights()"));
    const qsizetype selectionModeIndex =
        pickerSource.indexOf(QStringLiteral("function selectionModeAt(point)"));
    QVERIFY(pointFromWeightsIndex >= 0);
    QVERIFY(selectionModeIndex > pointFromWeightsIndex);
    const QString pointFromWeightsSource =
        pickerSource.mid(pointFromWeightsIndex, selectionModeIndex - pointFromWeightsIndex);
    QVERIFY(!pointFromWeightsSource.contains(QStringLiteral("normalizeWeights();")));
    QVERIFY(
        pointFromWeightsSource.contains(QStringLiteral("const total = pureHue + white + black;")));
    QVERIFY(!pickerSource.contains(QStringLiteral("createImageData")));
    QVERIFY(!pickerSource.contains(QStringLiteral("putImageData")));
}

void tst_CanvasToolBarQmlContract::colorPickerMovesIntoLeftClusterAndProfileStaysTrailing()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    const qsizetype circleButtonComponentIndex =
        toolbarSource.indexOf(QStringLiteral("component ToolbarCircleButton: LV.IconButton"));
    const qsizetype brushPropertySliderIndex =
        toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider: Item"));
    QVERIFY(circleButtonComponentIndex >= 0);
    QVERIFY(brushPropertySliderIndex > circleButtonComponentIndex);
    const QString circleButtonComponent = toolbarSource.mid(
        circleButtonComponentIndex, brushPropertySliderIndex - circleButtonComponentIndex);
    QVERIFY(circleButtonComponent.contains(QStringLiteral("contentItem: Rectangle {")));
    QVERIFY(circleButtonComponent.contains(QStringLiteral("radius: Math.min(width, height) / 2")));
    QVERIFY(circleButtonComponent.contains(
        QStringLiteral("property real circleBorderWidth: LV.Theme.scaleMetric(2)")));
    QVERIFY(circleButtonComponent.contains(
        QStringLiteral("border.width: circleButton.circleBorderWidth")));
    QVERIFY(circleButtonComponent.contains(QStringLiteral("border.color: \"#ffffff\"")));
    QVERIFY(circleButtonComponent.contains(QStringLiteral("antialiasing: true")));

    const qsizetype toolbarLayoutIndex = toolbarSource.indexOf(QStringLiteral("id: toolbarLayout"));
    const qsizetype brushControlsIndex =
        toolbarSource.indexOf(QStringLiteral("id: brushControlsRow"), toolbarLayoutIndex);
    const qsizetype currentColorButtonIndex =
        toolbarSource.indexOf(QStringLiteral("id: currentColorButton"), brushControlsIndex);
    const qsizetype spacerIndex =
        toolbarSource.indexOf(QStringLiteral("LV.Spacer {}"), currentColorButtonIndex);
    const qsizetype presentationModeButtonIndex =
        toolbarSource.indexOf(QStringLiteral("id: presentationModeButton"), spacerIndex);
    const qsizetype profileButtonIndex =
        toolbarSource.indexOf(QStringLiteral("id: profileButton"), presentationModeButtonIndex);
    const qsizetype presentationModeButtonTypeIndex =
        toolbarSource.lastIndexOf(QStringLiteral("LV.IconButton {"), presentationModeButtonIndex);
    const qsizetype profileButtonTypeIndex =
        toolbarSource.lastIndexOf(QStringLiteral("LV.IconButton {"), profileButtonIndex);
    QVERIFY(toolbarLayoutIndex >= 0);
    QVERIFY(brushControlsIndex > toolbarLayoutIndex);
    QVERIFY(currentColorButtonIndex > brushControlsIndex);
    QVERIFY(spacerIndex > currentColorButtonIndex);
    QVERIFY(presentationModeButtonTypeIndex > spacerIndex);
    QVERIFY(presentationModeButtonIndex > presentationModeButtonTypeIndex);
    QVERIFY(presentationModeButtonIndex > spacerIndex);
    QVERIFY(profileButtonTypeIndex > presentationModeButtonIndex);
    QVERIFY(profileButtonIndex > profileButtonTypeIndex);
    QVERIFY(profileButtonIndex > presentationModeButtonIndex);

    const QString currentColorButtonSource =
        toolbarSource.mid(currentColorButtonIndex, spacerIndex - currentColorButtonIndex);
    QVERIFY(currentColorButtonSource.contains(QStringLiteral("circleColor: toolbar.currentColor")));
    QVERIFY(currentColorButtonSource.contains(
        QStringLiteral("onClicked: toolbar.openColorPickerMenu(currentColorButton)")));

    const QString presentationModeButtonSource = toolbarSource.mid(
        presentationModeButtonTypeIndex, profileButtonTypeIndex - presentationModeButtonTypeIndex);
    QVERIFY(presentationModeButtonSource.trimmed().startsWith(QStringLiteral("LV.IconButton {")));
    QVERIFY(presentationModeButtonSource.contains(
        QStringLiteral("tone: LV.AbstractButton.Borderless")));
    QVERIFY(presentationModeButtonSource.contains(QStringLiteral("iconName: \"screens\"")));
    QVERIFY(!presentationModeButtonSource.contains(QStringLiteral("ToolbarCircleButton")));
    QVERIFY(!presentationModeButtonSource.contains(QStringLiteral("circleBorderWidth")));
    QVERIFY(!presentationModeButtonSource.contains(QStringLiteral("circleIconName")));
    QVERIFY(presentationModeButtonSource.contains(
        QStringLiteral("Accessible.name: qsTr(\"Presentation mode\")")));
    QVERIFY(presentationModeButtonSource.contains(
        QStringLiteral("Controls.ToolTip.text: qsTr(\"Presentation mode\")")));
    QVERIFY(presentationModeButtonSource.contains(
        QStringLiteral("onClicked: toolbar.presentationModeRequested()")));

    const QString profileButtonSource = toolbarSource.mid(profileButtonTypeIndex);
    QVERIFY(profileButtonSource.trimmed().startsWith(QStringLiteral("LV.IconButton {")));
    QVERIFY(profileButtonSource.contains(QStringLiteral("tone: LV.AbstractButton.Borderless")));
    QVERIFY(profileButtonSource.contains(QStringLiteral("iconName: \"user\"")));
    QVERIFY(!profileButtonSource.contains(QStringLiteral("ToolbarCircleButton")));
    QVERIFY(!profileButtonSource.contains(QStringLiteral("circleBorderWidth")));
    QVERIFY(!profileButtonSource.contains(QStringLiteral("circleIconName")));
    QVERIFY(profileButtonSource.contains(QStringLiteral("Accessible.name: qsTr(\"Profile\")")));
    QVERIFY(
        profileButtonSource.contains(QStringLiteral("Controls.ToolTip.text: qsTr(\"Profile\")")));
    QVERIFY(profileButtonSource.contains(QStringLiteral("onClicked: {")));
    QVERIFY(profileButtonSource.contains(
        QStringLiteral("profileInvitationMenu.openFor(profileButton")));
}

void tst_CanvasToolBarQmlContract::profileInvitationBadgeUsesDecisionContextMenu()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("property var pendingInvitation: ({})")));
    QVERIFY(toolbarSource.contains(QStringLiteral("property int pendingInvitationCount: 0")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("readonly property bool hasPendingInvitation: pendingInvitationCount > 0")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("signal invitationResponseRequested(bool accepted)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: profileInvitationMenuItemDelegate")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: profileInvitationMenu")));
    QVERIFY(
        toolbarSource.contains(QStringLiteral("itemDelegate: profileInvitationMenuItemDelegate")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"user\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("text: qsTr(\"invited you to a canvas\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("text: qsTr(\"Decline\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("text: qsTr(\"Accept\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.invitationResponseRequested(false);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.invitationResponseRequested(true);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: profileInvitationBadge")));
    QVERIFY(toolbarSource.contains(QStringLiteral("visible: toolbar.hasPendingInvitation")));
    QVERIFY(toolbarSource.contains(QStringLiteral("color: LV.Theme.danger")));
}

void tst_CanvasToolBarQmlContract::toolbarUsesStockLvrsControlGeometry()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarButtonVerticalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarButtonHorizontalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("figmaToolbarButtonSize")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("figmaToolbarMenuButtonWidth")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("figmaToolbarIconSize")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("figmaFileActionsRow")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("figmaToolActionsRow")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarControlButtonSize")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarControlIconSize")));
    QVERIFY(
        toolbarSource.contains(QStringLiteral("readonly property int toolbarAuxiliaryControlSize: "
                                              "LV.Theme.controlHeightMd")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarColorSwatchSize")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarColorSwatchRingSize")));
    QVERIFY(toolbarSource.contains(QStringLiteral(
        "readonly property url panHandIconSource: \"qrc:/Congregation/resources/icons/panHand.svg\"")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("readonly property url translateObjectIconSource: "
                       "\"qrc:/Congregation/resources/icons/translateObject.svg\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("typeAliasIconSource")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("property color backgroundColor: LV.Theme.panelBackground03")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarVerticalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbarHorizontalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("colorPickerRightPadding")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: leftToolbarActions")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: toolbarBackground")));
    QVERIFY(toolbarSource.contains(QStringLiteral("anchors.fill: toolbarBackground")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("component FigmaToolbarButton")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("component FigmaToolbarMenuButton")));
    QCOMPARE(toolbarSource.count(QStringLiteral("LV.IconButton {")), 16);
    QCOMPARE(toolbarSource.count(QStringLiteral("LV.IconMenuButton {")), 1);
    const qsizetype toolbarLayoutIndex = toolbarSource.indexOf(QStringLiteral("id: toolbarLayout"));
    QVERIFY(toolbarLayoutIndex >= 0);
    const QString toolbarLayoutSource = toolbarSource.mid(toolbarLayoutIndex);
    QCOMPARE(toolbarLayoutSource.count(QStringLiteral("LV.IconButton {")), 14);
    QCOMPARE(toolbarLayoutSource.count(QStringLiteral("tone: LV.AbstractButton.Borderless")), 15);
    QVERIFY(!toolbarLayoutSource.contains(QStringLiteral("LV.AbstractButton.Primary")));
    QVERIFY(!toolbarLayoutSource.contains(QStringLiteral("LV.AbstractButton.Default")));
    QVERIFY(!toolbarLayoutSource.contains(QStringLiteral("tone: toolbar.currentTool")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconSize:")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("horizontalPadding:")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("verticalPadding:")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("itemWidth: 196")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Layout.rightMargin:")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("titlebarReservedHeight")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("titlebarDragRequested")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property int figmaLeftToolbarWidth")));
    QVERIFY(
        !toolbarSource.contains(QStringLiteral("readonly property int figmaLeftToolbarHeight")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("readonly property int figmaToolbarOuterHorizontalPadding")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("readonly property int figmaToolbarOuterVerticalPadding")));
    QVERIFY(
        !toolbarSource.contains(QStringLiteral("readonly property color figmaToolbarBackground")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: figmaLeftToolbar")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("Layout.preferredWidth: toolbar.figmaLeftToolbarWidth")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("Layout.preferredHeight: toolbar.figmaLeftToolbarHeight")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("color: toolbar.figmaToolbarBackground")));

    const qsizetype addIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"addFile\""));
    const qsizetype openIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generalopen\""));
    const qsizetype saveIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generalsave\""));
    const qsizetype panIndex =
        toolbarSource.indexOf(QStringLiteral("iconSource: toolbar.panHandIconSource"));
    const qsizetype translateIndex =
        toolbarSource.indexOf(QStringLiteral("iconName: \"translateObject\""));
    const qsizetype zoomIndex =
        toolbarSource.indexOf(QStringLiteral("iconName: \"generalsearch\""));
    const qsizetype brushIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"showCode\""));
    const qsizetype eraserIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"eraser\""));
    const qsizetype shapeIndex = toolbarSource.indexOf(QStringLiteral("id: shapeToolButton"));
    const qsizetype fillIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"fillbucket\""));
    const qsizetype typeIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"typeAlias\""));

    QVERIFY(addIndex >= 0);
    QVERIFY(openIndex > addIndex);
    QVERIFY(saveIndex > openIndex);
    QVERIFY(panIndex > saveIndex);
    QVERIFY(translateIndex > panIndex);
    QVERIFY(zoomIndex > translateIndex);
    QVERIFY(brushIndex > zoomIndex);
    QVERIFY(eraserIndex > brushIndex);
    QVERIFY(shapeIndex > eraserIndex);
    QVERIFY(fillIndex > shapeIndex);
    QVERIFY(typeIndex > fillIndex);

    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"New canvas\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral(
        "signal newCanvasRequested(int canvasWidth, int canvasHeight, bool infiniteCanvas)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openNewCanvasDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasDialog")));
    QVERIFY(toolbarSource.contains(QStringLiteral("modal: true")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasWidthField")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasHeightField")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasInfiniteCheckBox")));
    QVERIFY(toolbarSource.contains(QStringLiteral("text: qsTr(\"Infinite canvas\")")));
    QCOMPARE(toolbarSource.count(QStringLiteral("enabled: !newCanvasInfiniteCheckBox.checked")), 2);
    QVERIFY(
        toolbarSource.contains(QStringLiteral("enabled: newCanvasInfiniteCheckBox.checked ||")));
    QVERIFY(toolbarSource.contains(QStringLiteral(
        "toolbar.newCanvasRequested(nextWidth, nextHeight, newCanvasInfiniteCheckBox.checked);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openNewCanvasDialog()")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onActivated: toolbar.openNewCanvasDialog()")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.newCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Open image\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openFileDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Open Canvas or Image")));
    QVERIFY(toolbarSource.contains(QStringLiteral(
        "Canvas and Images (*.iisc *.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff *.psd)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Save image\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openSaveDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function defaultSaveExtension(nameFilter)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"PNG Image (*.png)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"JPEG Image (*.jpg *.jpeg)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"Bitmap Image (*.bmp)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"WebP Image (*.webp)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"TIFF Image (*.tif *.tiff)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"Photoshop Document (*.psd)\")")));
    const qsizetype psdSaveFilterIndex =
        toolbarSource.indexOf(QStringLiteral("qsTr(\"Photoshop Document (*.psd)\")"));
    const qsizetype pngSaveFilterIndex =
        toolbarSource.indexOf(QStringLiteral("qsTr(\"PNG Image (*.png)\")"));
    QVERIFY(psdSaveFilterIndex >= 0);
    QVERIFY(pngSaveFilterIndex >= 0);
    QVERIFY(psdSaveFilterIndex < pngSaveFilterIndex);
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".webp\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".tif\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".psd\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("if (suffix.indexOf(\"png\") !== -1)")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generaldelete\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Clear canvas\")")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.clearCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.panHandIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Pan tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"pan\")")));
    QVERIFY(
        toolbarSource.contains(QStringLiteral("iconSource: toolbar.translateObjectIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"move\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Zoom tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"zoom\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Brush tool\")")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("onClicked: toolbar.activateBrushTool(brushToolButton)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Eraser tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"eraser\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Shape tool\")")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("iconName: toolbar.selectedShapeIconName(toolbar.currentShape)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.openShapeMenu(shapeToolButton);")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("onClicked: toolbar.openColorPickerMenu(shapeToolButton)")));
    QVERIFY(!toolbarSource.contains(
        QStringLiteral("onClicked: toolbar.openColorPickerMenu(colorMenuButton)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Fill tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"fill\")")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("typeAliasIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"text\")")));

    const QString qrcPath = QFINDTESTDATA("../App/resources.qrc");
    QVERIFY2(!qrcPath.isEmpty(), "resources.qrc test data was not found");
    QFile qrcFile(qrcPath);
    QVERIFY(qrcFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString qrcSource = QString::fromUtf8(qrcFile.readAll());
    QVERIFY(qrcSource.contains(QStringLiteral("resources/icons/panHand.svg")));
    QVERIFY(qrcSource.contains(QStringLiteral("resources/icons/typeAlias.svg")));

    const QString panHandSvgPath = QFINDTESTDATA("../App/resources/icons/panHand.svg");
    QVERIFY2(!panHandSvgPath.isEmpty(), "panHand.svg test data was not found");
    QFile panHandSvg(panHandSvgPath);
    QVERIFY(panHandSvg.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString panHandSvgSource = QString::fromUtf8(panHandSvg.readAll());
    QVERIFY(panHandSvgSource.contains(QStringLiteral("stroke=\"#CED0D6\"")));
    QVERIFY(panHandSvgSource.contains(QStringLiteral("fill=\"#2F2936\"")));

    const QString typeAliasSvgPath = QFINDTESTDATA("../App/resources/icons/typeAlias.svg");
    QVERIFY2(!typeAliasSvgPath.isEmpty(), "typeAlias.svg test data was not found");
    QFile typeAliasSvg(typeAliasSvgPath);
    QVERIFY(typeAliasSvg.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString typeAliasSvgSource = QString::fromUtf8(typeAliasSvg.readAll());
    QVERIFY(typeAliasSvgSource.contains(
        QStringLiteral("rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"1.5\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("fill=\"#2F2936\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("stroke=\"#A571E6\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("M7.5 5.5H5V4.5H11V5.5H8.5V11.5H7.5V5.5Z")));

    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"virtualFolder\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"imageClassification\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"rendererKit\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"clearOutputs\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generaledit\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbar.toolSelected(\"translate\")")));
}

void tst_CanvasToolBarQmlContract::drawingSurfaceProvidesPaintStyleTextToolEditor()
{
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property color textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property int textToolFramePadding")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int minimumTextToolFontPixelSize")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "readonly property int textToolFontPixelSize: "
        "Math.max(surface.minimumTextToolFontPixelSize, Math.round(surface.brushSize))")));
    QVERIFY(
        !surfaceSource.contains(QStringLiteral("readonly property int textToolFontPixelSize: 28")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function longestTextLine(textValue)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolAvailableWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolMeasuredWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolResponsiveWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginTextPlacement(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function commitActiveText()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.openRaster(sourceUrl)")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("property bool flatRasterDocumentOpened")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("appendRasterImageObject")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("return appendRasterImageObject(sourceUrl);")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("canvasSurface.imageObjectForFile(fileUrl)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("addDefaultDrawingLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("appendDrawableObject({")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"text\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("canvasSurface.saveToFileWithObjectsAndRasterLayers(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function rasterLayerDescriptors()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: textToolEditor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"textToolEditor\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("visible: surface.textEditingActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("color: surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canvasMouseAcceptedButtons()")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "mode === \"shape\" || mode === \"move\" || mode === \"fill\" || mode === \"text\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("acceptedButtons: surface.canvasMouseAcceptedButtons()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canvasCursorShape(pointX, pointY)")));
    const qsizetype cursorFunctionStart =
        surfaceSource.indexOf(QStringLiteral("function canvasCursorShape(pointX, pointY)"));
    const qsizetype cursorFunctionEnd =
        surfaceSource.indexOf(QStringLiteral("function cancelActiveShape()"), cursorFunctionStart);
    QVERIFY(cursorFunctionStart >= 0);
    QVERIFY(cursorFunctionEnd > cursorFunctionStart);
    const QString cursorFunctionSource =
        surfaceSource.mid(cursorFunctionStart, cursorFunctionEnd - cursorFunctionStart);
    QVERIFY(surfaceSource.contains(QStringLiteral("if (mode === \"pan\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "return surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("if (mode === \"move\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeAllCursor;")));
    QVERIFY(cursorFunctionSource.contains(
        QStringLiteral("if (mode === \"brush\" || mode === \"eraser\")")));
    QVERIFY(cursorFunctionSource.contains(QStringLiteral(
        "return surface.brushCursorUsesSystemFallback ? Qt.CrossCursor : Qt.BlankCursor;")));
    QVERIFY(cursorFunctionSource.contains(
        QStringLiteral("if (mode === \"zoom\" || mode === \"shape\")")));
    QVERIFY(cursorFunctionSource.contains(QStringLiteral("if (mode === \"fill\")")));
    QVERIFY(cursorFunctionSource.contains(QStringLiteral("return Qt.ArrowCursor;")));
    QVERIFY(!cursorFunctionSource.contains(QStringLiteral("Qt.SizeHorCursor")));
    QCOMPARE(surfaceSource.count(QStringLiteral("return Qt.SizeHorCursor;")), 1);
    QVERIFY(
        surfaceSource.contains(QStringLiteral("readonly property real brushCursorScreenDiameter: "
                                              "surface.brushSize * surface.canvasZoomScale")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "readonly property real brushCursorEffectiveOuterStrokeWidth: "
        "Math.min(surface.brushCursorOuterStrokeWidth, surface.brushCursorScreenDiameter / 2)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "readonly property real brushCursorEffectiveInnerStrokeWidth: "
        "Math.min(surface.brushCursorInnerStrokeWidth, surface.brushCursorScreenDiameter / 6)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property bool brushCursorUsesSystemFallback: "
                       "surface.brushCursorToolActive && surface.brushCursorScreenDiameter < 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property bool brushCursorToolActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: brushCursorGlassPane")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorGlassPane\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: brushCursorHoverHandler")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorHoverHandler\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("HoverHandler")));
    QVERIFY(surfaceSource.contains(QStringLiteral("blocking: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: brushCursorPointHandler")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorPointHandler\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("PointHandler")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: surface.brushCursorToolActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("acceptedButtons: Qt.NoButton")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "cursorShape: surface.brushCursorUsesSystemFallback ? Qt.CrossCursor : Qt.BlankCursor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: brushCursorOutline")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorOutline\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("parent: canvasViewport")));
    QVERIFY(surfaceSource.contains(QStringLiteral("z: 8")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("cursorDiameter: surface.brushCursorScreenDiameter")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("brushCursorGlassPane.mapToItem(canvasViewport, cursorPosition)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: brushCursorRingShape")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorRingShape\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("Shapes.Shape")));
    QCOMPARE(surfaceSource.count(QStringLiteral("Shapes.ShapePath")), 2);
    QCOMPARE(surfaceSource.count(QStringLiteral("PathAngleArc")), 2);
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorOuterRing\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"brushCursorInnerRing\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("strokeWidth: surface.brushCursorEffectiveOuterStrokeWidth")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("strokeWidth: surface.brushCursorEffectiveInnerStrokeWidth")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("border.width: surface.brushCursorEffectiveOuterStrokeWidth")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("border.width: surface.brushCursorEffectiveInnerStrokeWidth")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: canvasPointerMouseArea")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("cursorShape: surface.canvasCursorShape(mouseX, mouseY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("border.color: surface.textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("selectionColor: surface.textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("TextMetrics")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: textToolLineMetrics")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("text: surface.longestTextLine(textToolEditor.text)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("width: surface.textToolResponsiveWidth()")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("width: surface.textToolDefaultWidth")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: !surface.textEditingActive")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"text\")")));

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString painterPageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(painterPageSource.contains(QStringLiteral("textToolAccentColor: LV.Theme.primary")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("textToolFramePadding: painterPage.spacingSmall")));
}

void tst_CanvasToolBarQmlContract::shapeToolUsesStockIconMenuAndDragInsertion()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("property string currentShape: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("signal shapeSelected(string shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openShapeMenu(triggerItem)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function selectedShapeIconName(shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function selectShape(shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("LV.ContextMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("LV.IconMenuButton")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: shapeMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("items: toolbar.shapeMenuEntries")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("showIconSlot: true")));
    const qsizetype shapeMenuIndex = toolbarSource.indexOf(QStringLiteral("id: shapeMenu"));
    const qsizetype invitationDelegateIndex =
        toolbarSource.indexOf(QStringLiteral("id: profileInvitationMenuItemDelegate"));
    QVERIFY(shapeMenuIndex >= 0);
    QVERIFY(invitationDelegateIndex > shapeMenuIndex);
    const QString shapeMenuSource =
        toolbarSource.mid(shapeMenuIndex, invitationDelegateIndex - shapeMenuIndex);
    QVERIFY(!shapeMenuSource.contains(QStringLiteral("itemWidth:")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("selectedIndex: toolbar.shapeMenuEntries.findIndex(entry => entry.shape === "
                       "toolbar.currentShape)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onItemTriggered: function (index, item)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.selectShape(item.shape);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"ellipse\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"ellipse\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"triangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"triangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"diamond\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"diamond\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"star\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"star\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"rectanglebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"ellipsebubble\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"rectanglebubble\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"ellipsebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shapeSelected(shapeKind);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolSelected(\"shape\");")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("return entry && entry.iconName ? entry.iconName : \"rectangle\";")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: shapeMenuRepeater")));

    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property string shapeKind: \"rectangle\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool shapeDraggingActive: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool shapeAspectLocked: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function shiftModifierActiveFromMouse(mouse)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function shapeAspectLockedFromMouse(mouse)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function drawableObjectTransformConstrainedFromMouse(mouse)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("return (mouse.modifiers & Qt.ShiftModifier) !== 0;")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function constrainedShapeDragPoint(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const side = Math.min(Math.max(Math.abs(deltaX), Math.abs(deltaY)), "
                       "horizontalLimit, verticalLimit);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function shapeDragPoint(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function applyShapeDragPoint(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function beginShapeDrag(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function updateShapeDrag(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function commitActiveShape()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function paintShapePreview(context, previewWidth, previewHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "function traceRectangleBubblePath(context, x, y, widthValue, heightValue)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function traceEllipseBubblePath(context, x, y, widthValue, heightValue)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function traceEllipseArcPath(context, x, y, "
                                              "widthValue, heightValue, startAngle, endAngle)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: shapePreviewCanvas")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"shape\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.shapeKind")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("context.fillStyle = surface.brushColor.toString();")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("context.fillStyle = drawableObjectDelegate.objectColor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("context.fill();")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("context.strokeWidth")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("context.stroke();")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("traceEllipsePath(context, x, y, widthValue, bodyHeight);")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "mode === \"shape\" || mode === \"move\" || mode === \"fill\" || mode === \"text\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("if (mode === \"shape\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.CrossCursor;")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.beginShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitActiveShape();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function fillAt(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode !== \"fill\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const rasterSurface = activeRasterSurface();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("if (!rasterSurface)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("if (rasterSurface.fillAt(pointX, pointY, surface.brushColor))")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.fillAt(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property real canvasZoomScale: 1")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("readonly property real defaultCanvasZoomScale: 1")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property real minimumCanvasZoomScale: 0.01")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function fittedCanvasZoomScale(canvasWidth, canvasHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function fitCanvasZoomToCurrentCanvas()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("Math.min(surface.defaultCanvasZoomScale, surface.workspaceCanvasWidth / "
                       "normalizedWidth, surface.workspaceCanvasHeight / normalizedHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginZoomDrag(pointX)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updateZoomDrag(pointX)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function cancelZoomDrag()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("scale: surface.canvasZoomScale")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: canvasZoomMouseArea")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"canvasZoomMouseArea\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("enabled: surface.effectiveToolMode() === \"zoom\"")));
    const qsizetype zoomMouseAreaStart =
        surfaceSource.indexOf(QStringLiteral("id: canvasZoomMouseArea"));
    const qsizetype zoomMouseAreaEnd =
        surfaceSource.indexOf(QStringLiteral("onPressed: function (mouse)"), zoomMouseAreaStart);
    QVERIFY(zoomMouseAreaStart >= 0);
    QVERIFY(zoomMouseAreaEnd > zoomMouseAreaStart);
    const QString zoomMouseAreaSource =
        surfaceSource.mid(zoomMouseAreaStart, zoomMouseAreaEnd - zoomMouseAreaStart);
    QVERIFY(zoomMouseAreaSource.contains(
        QStringLiteral("cursorShape: surface.canvasCursorShape(mouseX, mouseY)")));
    QVERIFY(!zoomMouseAreaSource.contains(QStringLiteral("Qt.SizeHorCursor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginZoomDrag(mouse.x);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updateZoomDrag(mouse.x);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitZoomDrag();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property real canvasPanOffsetX: 0")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginPanDrag(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updatePanDrag(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function ensureInfiniteCanvasForViewport()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.ensureInfiniteCanvasRegion(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.applyInfiniteCanvasGrowth(growth);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function resetCanvasPan()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: canvasPanMouseArea")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("enabled: surface.effectiveToolMode() === \"pan\"")));
    const qsizetype panMouseAreaStart =
        surfaceSource.indexOf(QStringLiteral("id: canvasPanMouseArea"));
    const qsizetype panMouseAreaEnd =
        surfaceSource.indexOf(QStringLiteral("onPressed: function (mouse)"), panMouseAreaStart);
    QVERIFY(panMouseAreaStart >= 0);
    QVERIFY(panMouseAreaEnd > panMouseAreaStart);
    const QString panMouseAreaSource =
        surfaceSource.mid(panMouseAreaStart, panMouseAreaEnd - panMouseAreaStart);
    QVERIFY(panMouseAreaSource.contains(
        QStringLiteral("cursorShape: surface.canvasCursorShape(mouseX, mouseY)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("anchors.horizontalCenterOffset: surface.canvasPanOffsetX")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("anchors.verticalCenterOffset: surface.canvasPanOffsetY")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginPanDrag(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updatePanDrag(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitPanDrag();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅠ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"brush\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㄷ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"eraser\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅗ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"pan\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property string temporaryCameraMode: \"\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function effectiveToolMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function setTemporaryCameraMode(mode)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginSpacePanMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginSpaceZoomMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function endTemporaryCameraMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function endSpacePanMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("event.key === Qt.Key_Space")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.temporaryCameraModeForModifiers(event.modifiers)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.endTemporaryCameraMode()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅍ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"move\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅋ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"zoom\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅕ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"shape\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅎ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"fill\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅅ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("onActivated: surface.toolShortcutRequested(\"text\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool toolShortcutsEnabled: true")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("enabled: surface.toolShortcutsEnabled && !surface.textEditingActive")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Undo]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Redo]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var drawableObjects: []")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function psdCompatibilityManifest()")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("canvasSurface.psdCompatibilityManifest(surface."
                                              "drawableObjects, surface.backgroundLayerPresent)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function openLayeredPsd(fileUrl)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.psdImportDocument(fileUrl)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("sourceUrl.toLowerCase().endsWith(\".psd\") && openLayeredPsd(sourceUrl)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.rasterLayerSnapshotSources[String(layerId)] = layer.source;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("blendMode: layer.blendModeKey || \"norm\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("source: drawableObjectDelegate.objectType === \"image\" ? "
                       "drawableObjectDelegate.objectSource : \"\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function beginDrawableObjectTransform(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function updateDrawableObjectTransform(pointX, pointY, constrained)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function constrainedDrawableObjectDelta(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function drawableObjectDelta(pointX, pointY, constrained)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "function movedDrawableObject(originalObject, pointX, pointY, constrained)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function resizedDrawableObjectBounds(originalObject, pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "function aspectLockedDrawableObjectResizeBounds(originalObject, pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "function resizedDrawableObject(originalObject, pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("Math.abs(deltaX) >= Math.abs(deltaY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const lockedWidth = originalWidth * scale;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const lockedHeight = originalHeight * scale;")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "const constrained = surface.drawableObjectTransformConstrainedFromMouse(mouse);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.updateDrawableObjectTransform(mouse.x, mouse.y, constrained);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const resizeDirections = "
                       "surface.drawableObjectTransformMode.slice(\"resize-\".length);")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int drawableObjectHandleSize: 10")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int drawableObjectHandleHitSize: 32")));
    QVERIFY(surfaceSource.contains(QStringLiteral("mode: \"resize-n\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("mode: \"resize-e\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("mode: \"resize-s\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("mode: \"resize-w\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function updateDrawableObjectHoverHandle(pointX, pointY)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function drawableObjectResizeCursor(handleMode)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeFDiagCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeBDiagCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeVerCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeHorCursor;")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("canvasSurface.width - movedObject.width")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("right = Math.min(canvasSurface.width")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("bottom = Math.min(canvasSurface.height")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("left = Math.max(0, Math.min(right")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("top = Math.max(0, Math.min(bottom")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteSelectedDrawableObject()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"Delete\", \"Backspace\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.deleteCurrentLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"canvasSurface\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("clip: true")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: drawableObjectSelectionFrame")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.updateDrawableObjectHoverHandle(mouse.x, mouse.y);")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surface.drawableObjectHoverHandleMode = \"\";")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.beginDrawableObjectTransform(mouse.x, mouse.y);")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("enabled: surface.effectiveToolMode() === \"pan\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const mode = surface.effectiveToolMode();")));

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString painterPageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(painterPageSource.contains(QStringLiteral("function setShapeKind(shapeKind)")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("function newCanvas(canvasWidth, canvasHeight, infiniteCanvas)")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("drawingSurface.newCanvas(canvasWidth, canvasHeight, infiniteCanvas);")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function openNewCanvasDialog()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("canvasToolBar.openNewCanvasDialog();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function openFileDialog()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("canvasToolBar.openFileDialog();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function openSaveDialog()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("canvasToolBar.openSaveDialog();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function undoActiveRasterSurface()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("rasterSurface.undo();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function redoActiveRasterSurface()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("rasterSurface.redo();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function addLayer()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("drawingSurface.addEmptyLayer();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function deleteCurrentLayer()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("drawingSurface.deleteCurrentLayer();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function selectShapeTool(shapeKind)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("painterPage.setToolMode(\"shape\");")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function fitCanvasToWindow()")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("drawingSurface.fitCanvasZoomToCurrentCanvas();")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function resetCanvasView()")));
    QVERIFY(painterPageSource.contains(QStringLiteral("drawingSurface.resetCanvasPan();")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : "
                       "painterPage.fallbackNewCanvasWidth")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : "
                       "painterPage.fallbackNewCanvasHeight")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("onNewCanvasRequested: (canvasWidth, canvasHeight, infiniteCanvas) => "
                       "painterPage.newCanvas(canvasWidth, canvasHeight, infiniteCanvas)")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("painterPage.updateDocumentProperty(\"shapeKind\", shapeKind);")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("shapeKind: painterPage.vm ? painterPage.vm.shapeKind : \"rectangle\"")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("currentShape: painterPage.vm ? painterPage.vm.shapeKind : \"rectangle\"")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("onShapeSelected: shapeKind => painterPage.setShapeKind(shapeKind)")));
}

void tst_CanvasToolBarQmlContract::brushSizeControlsFlowFromDecreaseToSliderToIncrease()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    const qsizetype decreaseIndex =
        toolbarSource.indexOf(QStringLiteral("Accessible.name: qsTr(\"Decrease brush size\")"));
    const qsizetype sliderIndex = toolbarSource.indexOf(QStringLiteral("id: sizeSlider"));
    const qsizetype increaseIndex =
        toolbarSource.indexOf(QStringLiteral("Accessible.name: qsTr(\"Increase brush size\")"));

    QVERIFY(decreaseIndex >= 0);
    QVERIFY(sliderIndex >= 0);
    QVERIFY(increaseIndex >= 0);
    QVERIFY(decreaseIndex < sliderIndex);
    QVERIFY(sliderIndex < increaseIndex);
}

void tst_CanvasToolBarQmlContract::toolbarUsesFullWidthRectangularBackground()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    const qsizetype backgroundIndex =
        toolbarSource.indexOf(QStringLiteral("id: toolbarBackground"));
    const qsizetype separatorIndex =
        toolbarSource.indexOf(QStringLiteral("id: toolbarBottomSeparator"));
    const qsizetype layoutIndex = toolbarSource.indexOf(QStringLiteral("id: toolbarLayout"));
    QVERIFY(backgroundIndex >= 0);
    QVERIFY(separatorIndex > backgroundIndex);
    QVERIFY(layoutIndex > separatorIndex);

    const QString backgroundBlock =
        toolbarSource.mid(backgroundIndex, separatorIndex - backgroundIndex);
    const QString layoutBlock = toolbarSource.mid(
        layoutIndex,
        toolbarSource.indexOf(QStringLiteral("LV.HStack {"), layoutIndex + 1) - layoutIndex);
    QVERIFY(backgroundBlock.contains(QStringLiteral("anchors.fill: parent")));
    QVERIFY(backgroundBlock.contains(QStringLiteral("radius: 0")));
    QVERIFY(backgroundBlock.contains(QStringLiteral("color: toolbar.backgroundColor")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: toolbarBottomSeparator")));
    QVERIFY(toolbarSource.contains(QStringLiteral("anchors.bottom: parent.bottom")));
    QVERIFY(toolbarSource.contains(QStringLiteral("height: LV.Theme.scaleMetric(1)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: toolbarEventBlocker")));
    QVERIFY(toolbarSource.contains(QStringLiteral("implicitHeight: toolbarLayout.implicitHeight")));
    QVERIFY(layoutBlock.contains(QStringLiteral("anchors.fill: toolbarBackground")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("readonly property int spacingLarge: LV.Theme.gap16")));
    QVERIFY(layoutBlock.contains(QStringLiteral("anchors.leftMargin: toolbar.spacingLarge")));
    QVERIFY(layoutBlock.contains(QStringLiteral("anchors.rightMargin: toolbar.spacingLarge")));
    QVERIFY(!layoutBlock.contains(QStringLiteral("anchors.topMargin:")));
    QVERIFY(!layoutBlock.contains(QStringLiteral("anchors.bottomMargin:")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("gradient: Gradient")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("GradientStop")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("border.width: 1")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: floatingBackground")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("radius: height / 2")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: titlebarDragArea")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("titlebarWindowControlInset")));
}

void tst_CanvasToolBarQmlContract::toolbarIsOffsetBelowApplicationWindowTopChrome()
{
    const QString pageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile pageQml(pageQmlPath);
    QVERIFY(pageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(pageQml.readAll());

    const qsizetype toolbarIndex = pageSource.indexOf(QStringLiteral("BrushUi.CanvasToolBar"));
    const qsizetype chromeBackgroundIndex =
        pageSource.indexOf(QStringLiteral("id: toolbarChromeBackground"));
    QVERIFY(toolbarIndex >= 0);
    QVERIFY(chromeBackgroundIndex >= 0);
    QVERIFY(chromeBackgroundIndex < toolbarIndex);
    const QString toolbarBlock = pageSource.mid(toolbarIndex);

    QVERIFY(pageSource.contains(QStringLiteral("property int topChromeReservedHeight: 0")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "readonly property color toolbarBackgroundColor: LV.Theme.panelBackground03")));
    QVERIFY(pageSource.contains(
        QStringLiteral("readonly property int toolbarTopMargin: topChromeReservedHeight")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "readonly property int layerPanelTopMargin: toolbarTopMargin + canvasToolBar.height")));
    QVERIFY(pageSource.contains(
        QStringLiteral("height: painterPage.toolbarTopMargin + canvasToolBar.height")));
    QVERIFY(pageSource.contains(QStringLiteral("color: painterPage.toolbarBackgroundColor")));
    QVERIFY(
        toolbarBlock.contains(QStringLiteral("anchors.topMargin: painterPage.toolbarTopMargin")));
    QVERIFY(toolbarBlock.contains(QStringLiteral("anchors.leftMargin: 0")));
    QVERIFY(toolbarBlock.contains(QStringLiteral("anchors.rightMargin: 0")));
    QVERIFY(toolbarBlock.contains(
        QStringLiteral("backgroundColor: painterPage.toolbarBackgroundColor")));
    QVERIFY(!pageSource.contains(QStringLiteral("readonly property int toolbarTopMargin: 0")));
    QVERIFY(!toolbarBlock.contains(
        QStringLiteral("titlebarReservedHeight: painterPage.topChromeReservedHeight")));
    QVERIFY(!toolbarBlock.contains(
        QStringLiteral("onTitlebarDragRequested: painterPage.titlebarDragRequested()")));
    QVERIFY(!toolbarBlock.contains(QStringLiteral("anchors.leftMargin: painterPage.spacingSmall")));
    QVERIFY(
        !toolbarBlock.contains(QStringLiteral("anchors.rightMargin: painterPage.spacingSmall")));
}

void tst_CanvasToolBarQmlContract::painterPageUsesLvHierarchyLayerPanel()
{
    const QString pageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile pageQml(pageQmlPath);
    QVERIFY(pageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(pageQml.readAll());
    const qsizetype panelIndex = pageSource.indexOf(QStringLiteral("id: layerHierarchyPanel"));
    const qsizetype surfaceIndex = pageSource.indexOf(QStringLiteral("Painting.DrawingSurface"));
    const qsizetype surfaceEndIndex =
        pageSource.indexOf(QStringLiteral("Connections {"), surfaceIndex);
    QVERIFY(panelIndex >= 0);
    QVERIFY(surfaceIndex > panelIndex);
    QVERIFY(surfaceEndIndex > surfaceIndex);
    const QString panelBlock = pageSource.mid(panelIndex, surfaceIndex - panelIndex);
    const QString surfaceBlock = pageSource.mid(surfaceIndex, surfaceEndIndex - surfaceIndex);

    QVERIFY(!pageSource.contains(QStringLiteral("readonly property int layerPanelWidth")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "readonly property int layerPanelTopMargin: toolbarTopMargin + canvasToolBar.height")));
    QVERIFY(
        pageSource.contains(QStringLiteral("readonly property int layerRenameActivationWindowMs")));
    QVERIFY(pageSource.contains(
        QStringLiteral("readonly property int layerRenameRepeatActivationThreshold")));
    QVERIFY(pageSource.contains(QStringLiteral("property bool layerRenameActive: false")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerHierarchyPanel")));
    QVERIFY(pageSource.contains(QStringLiteral("objectName: \"layerHierarchyPanel\"")));
    QVERIFY(pageSource.contains(QStringLiteral("LV.Hierarchy")));
    QVERIFY(panelBlock.contains(QStringLiteral("anchors.left: parent.left")));
    QVERIFY(
        panelBlock.contains(QStringLiteral("anchors.topMargin: painterPage.layerPanelTopMargin")));
    QVERIFY(panelBlock.contains(QStringLiteral("anchors.leftMargin: 0")));
    QVERIFY(panelBlock.contains(QStringLiteral("anchors.bottomMargin: 0")));
    QVERIFY(!panelBlock.contains(QStringLiteral("width: painterPage.layerPanelWidth")));
    QVERIFY(!panelBlock.contains(QStringLiteral("minimumPanelWidth:")));
    QVERIFY(!panelBlock.contains(QStringLiteral("minimumPanelHeight:")));
    QVERIFY(!panelBlock.contains(QStringLiteral("panelColor:")));
    QVERIFY(pageSource.contains(QStringLiteral("model: drawingSurface.layerHierarchyRows")));
    QVERIFY(pageSource.contains(QStringLiteral("editable: true")));
    QVERIFY(pageSource.contains(QStringLiteral("footerVisible: true")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton1")));
    QVERIFY(pageSource.contains(QStringLiteral("iconName: \"add\"")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton2")));
    QVERIFY(pageSource.contains(QStringLiteral("iconName: \"remove\"")));
    QVERIFY(!pageSource.contains(QStringLiteral("iconName: \"delete\"")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton3")));
    QVERIFY(pageSource.contains(QStringLiteral("visible: false")));
    QVERIFY(
        pageSource.contains(QStringLiteral("function handleLayerHierarchyItemActivated(item)")));
    QVERIFY(
        pageSource.contains(QStringLiteral("painterPage.layerRenameActivationCount >= "
                                           "painterPage.layerRenameRepeatActivationThreshold")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.beginLayerRename(item);")));
    QVERIFY(pageSource.contains(QStringLiteral("function beginLayerRename(item)")));
    QVERIFY(pageSource.contains(QStringLiteral("function commitLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("function cancelLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.activateLayerByKey(itemKey);")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.canRenameLayerByKey(itemKey)")));
    QVERIFY(pageSource.contains(
        QStringLiteral("layerRenameField.text = drawingSurface.layerNameByKey(itemKey);")));
    QVERIFY(
        pageSource.contains(QStringLiteral("drawingSurface.renameLayerByKey(itemKey, nextName);")));
    QVERIFY(pageSource.contains(
        QStringLiteral("painterPage.handleLayerHierarchyItemActivated(item);")));
    QVERIFY(pageSource.contains(
        QStringLiteral("drawingSurface.applyLayerHierarchyOrder(layerHierarchyPanel.model);")));
    QVERIFY(pageSource.contains(QStringLiteral("onFooterButtonTriggered: function (index)")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.addEmptyLayer();")));
    QVERIFY(pageSource.contains(QStringLiteral("enabled: drawingSurface.canDeleteCurrentLayer()")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.deleteCurrentLayer();")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "layerHierarchyPanel.activateListItemByKey(drawingSurface.currentLayerKey())")));
    QVERIFY(!panelBlock.contains(QStringLiteral("LV.ToolbarButton")));
    QVERIFY(!panelBlock.contains(QStringLiteral("buttonId: \"layers\"")));
    QVERIFY(!pageSource.contains(QStringLiteral("buttonId: \"delete\"")));
    QVERIFY(surfaceBlock.contains(QStringLiteral(
        "anchors.left: painterPage.presentationMode ? parent.left : layerHierarchyPanel.right")));
    QVERIFY(surfaceBlock.contains(QStringLiteral("anchors.leftMargin: 0")));
    QVERIFY(!panelBlock.contains(QStringLiteral("anchors.leftMargin: painterPage.spacingSmall")));
    QVERIFY(!panelBlock.contains(QStringLiteral("anchors.bottomMargin: painterPage.spacingSmall")));
    QVERIFY(!surfaceBlock.contains(QStringLiteral("anchors.leftMargin: painterPage.spacingSmall")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerRenameEditorFrame")));
    QVERIFY(pageSource.contains(QStringLiteral("parent: layerHierarchyPanel")));
    QVERIFY(pageSource.contains(QStringLiteral("TextInput")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerRenameField")));
    QVERIFY(pageSource.contains(QStringLiteral("onAccepted: painterPage.commitLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("Keys.onEscapePressed: function (event)")));

    const QString surfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!surfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile surfaceQml(surfaceQmlPath);
    QVERIFY(surfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(surfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property var layerHierarchyRows: []")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int layerHierarchyThumbnailRefreshDelayMs: 1000")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int brushLivePreviewFrameIntervalMs: 16")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "readonly property string transparencyGridTileSource: \"data:image/png;base64,")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property bool backgroundLayerThumbnailRefreshPending: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool backgroundLayerPresent: true")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property var pendingRasterLayerThumbnailRefreshes: ({})")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property string backgroundLayerThumbnailSource: \"\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property var drawableObjectThumbnailSources: ({})")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("property var rasterLayerThumbnailSources: ({})")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function rebuildLayerHierarchyRows()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function requestBackgroundLayerThumbnailRefresh()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function requestRasterLayerThumbnailRefresh(objectId)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function flushPendingLayerThumbnailRefreshes()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function clearRasterLayerThumbnailState(objectId)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function fallbackRasterThumbnailSource(surfaceItem)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function refreshRasterSurfaceThumbnailSource(surfaceItem, applySource)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surfaceItem.cacheGrabbedThumbnailSource(result)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("if (rasterLayerItemById(objectId) !== item)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surfaceItem.grabToImage")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: layerThumbnailRefreshTimer")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("interval: surface.layerHierarchyThumbnailRefreshDelayMs")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function layerIconSourceForDrawableObject(drawableObject)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("iconSource: layerIconSourceForDrawableObject(drawableObject)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("iconSource: surface.backgroundLayerThumbnailSource")));
    QVERIFY(surfaceSource.contains(QStringLiteral("iconGlyph: \"\"")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("function layerIconGlyphForDrawableObject(drawableObject)")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("iconGlyph: \"R\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function addEmptyLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function addDefaultDrawingLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.addDefaultDrawingLayer();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property int nextEmptyLayerNumber: 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: drawableObjectVisualModel")));
    QVERIFY(surfaceSource.contains(QStringLiteral("model: drawableObjectVisualModel")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("model: surface.drawableObjects")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function drawableObjectVisualModelEntry(drawableObject)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function reorderDrawableObjectVisualModel(orderedObjects)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function drawableObjectVisualModelCount()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("layerKind: \"layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("contentKind: drawableObject.type || \"\"")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("row.layerKind !== \"layer\" && row.layerKind !== \"object\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("parent: canvasSurface")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var rasterLayerItems: ({})")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function registerRasterLayerItem(objectId, surfaceItem)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function unregisterRasterLayerItem(objectId, surfaceItem)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("cacheRasterThumbnailSource(surface.layerHierarchyThumbnailSize, "
                       "surface.layerHierarchyThumbnailSize)")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "cacheDrawableObjectThumbnailSource(drawableObject, surface.layerHierarchyThumbnailSize, "
        "surface.layerHierarchyThumbnailSize)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("livePreviewFrameIntervalMs: surface.brushLivePreviewFrameIntervalMs")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.requestBackgroundLayerThumbnailRefresh();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.requestRasterLayerThumbnailRefresh("
                                                  "drawableObjectDelegate.rasterLayerObjectId);")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("onRasterContentChanged: surface.refreshBackgroundLayerThumbnailSource()")));
    QVERIFY(!surfaceSource.contains(QStringLiteral(
        "onRasterContentChanged: "
        "surface.refreshRasterLayerThumbnailSource(drawableObjectDelegate.rasterLayerObjectId)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function activeRasterSurface()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function hasActiveRasterSurface()")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "toolMode: surface.backgroundLayerPresent && !surface.rasterLayerObjectSelected() ? "
        "surface.effectiveToolMode() : \"move\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"transparencyGridBackground\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("visible: !surface.backgroundLayerPresent")));
    QVERIFY(surfaceSource.contains(QStringLiteral("fillMode: Image.Tile")));
    QVERIFY(surfaceSource.contains(QStringLiteral("smooth: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function nextEmptyLayerName()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function drawableObjectForLayerKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function activateLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteBackgroundLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canDeleteCurrentLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteCurrentLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canRenameLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function layerNameByKey(layerKey)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function renameLayerByKey(layerKey, layerName)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("renamedObject.name = normalizedName;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function applyLayerHierarchyOrder(layerRows)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function hasTransformableSelectedDrawableObject()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "objectName: \"rasterLayerSurface-\" + drawableObjectDelegate.rasterLayerObjectId")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.selectedDrawableObjectId === "
                       "drawableObjectDelegate.rasterLayerObjectId && (surface.effectiveToolMode() "
                       "=== \"brush\" || surface.effectiveToolMode() === \"eraser\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("label: qsTr(\"Background\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("draggable: false")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surface.effectiveToolMode() === \"move\" && "
                                              "surface.hasTransformableSelectedDrawableObject()")));
}

void tst_CanvasToolBarQmlContract::pressureCurveControlsUseThreePointGraphAtBottom()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("component PressureCurveGraph: Item")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressureCurveGraph")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressureCurveCanvas")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressurePointRepeater")));
    QVERIFY(toolbarSource.contains(QStringLiteral("model: pressureCurveGraph.pointModel")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveMinimum\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveCenter\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveMaximum\"")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("toolbar.requestBrushPropertyChange(point.propertyName, nextValue);")));

    const qsizetype stabilizerIndex =
        toolbarSource.indexOf(QStringLiteral("propertyName: \"stabilizerStrength\""));
    const qsizetype graphIndex =
        toolbarSource.lastIndexOf(QStringLiteral("id: pressureCurveGraph"));
    QVERIFY(stabilizerIndex >= 0);
    QVERIFY(graphIndex > stabilizerIndex);

    const QString sliderComponent = toolbarSource.mid(
        toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider")),
        toolbarSource.indexOf(QStringLiteral("component PressureCurveGraph")) -
            toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveMinimum")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveCenter")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveMaximum")));
}

void tst_CanvasToolBarQmlContract::brushSettingsExposePressureOpacityToggle()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");
    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(
        toolbarSource.contains(QStringLiteral("property bool brushPressureControlsOpacity: true")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("signal brushPressureControlsOpacityChangeRequested(bool enabled)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("component BrushPropertyToggle: Item")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("objectName: \"brushPressureControlsOpacityToggle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("label: qsTr(\"Pressure Opacity\")")));
    QVERIFY(
        toolbarSource.contains(QStringLiteral("checked: toolbar.brushPressureControlsOpacity")));
    QVERIFY(toolbarSource.contains(
        QStringLiteral("toolbar.brushPressureControlsOpacityChangeRequested(checked)")));

    const qsizetype opacityIndex =
        toolbarSource.indexOf(QStringLiteral("propertyName: \"brushOpacity\""));
    const qsizetype pressureOpacityIndex =
        toolbarSource.indexOf(QStringLiteral("label: qsTr(\"Pressure Opacity\")"));
    const qsizetype hardnessIndex =
        toolbarSource.indexOf(QStringLiteral("propertyName: \"brushHardness\""));
    QVERIFY(opacityIndex >= 0);
    QVERIFY(pressureOpacityIndex > opacityIndex);
    QVERIFY(hardnessIndex > pressureOpacityIndex);

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(painterPageQml.readAll());
    QVERIFY(
        pageSource.contains(QStringLiteral("brushPressureControlsOpacity: painterPage.vm ? "
                                           "painterPage.vm.brushPressureControlsOpacity : true")));
    QVERIFY(pageSource.contains(
        QStringLiteral("onBrushPressureControlsOpacityChangeRequested: enabled => "
                       "painterPage.setBrushProperty(\"brushPressureControlsOpacity\", enabled)")));

    const QString surfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!surfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");
    QFile surfaceQml(surfaceQmlPath);
    QVERIFY(surfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(surfaceQml.readAll());
    QVERIFY(
        surfaceSource.contains(QStringLiteral("property bool brushPressureControlsOpacity: true")));
    QCOMPARE(surfaceSource.count(
                 QStringLiteral("brushOpacityEnabled: surface.brushPressureControlsOpacity")),
             2);
}

QTEST_APPLESS_MAIN(tst_CanvasToolBarQmlContract)

#include "tst_canvastoolbarqmlcontract.moc"
