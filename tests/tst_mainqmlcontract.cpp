#include <QFile>
#include <QString>
#include <QStringList>
#include <QtTest>

class tst_MainQmlContract : public QObject
{
    Q_OBJECT

  private slots:
    void applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome();
    void applicationWindowUsesOnlyTheMacOsFullSizeTitleBarDragRegion();
    void applicationWindowPassesOnlyTheActiveMacOsDragHeightToCanvasPage();
    void applicationWindowUsesStockLvrsGeometry();
    void applicationWindowUsesDeferredCanvasIncubation();
    void applicationWindowTemporarilyDisablesLicenseEnforcement();
    void presentationModeUsesFullScreenCanvasOnlyAndRestoresOnEscape();
    void canvasWheelZoomIsAvailableInEveryToolMode();
    void applicationWindowProvidesApplicationMenuBar();
    void applicationMenuBarUsesLvrsContextMenusOnWindows();
    void applicationMenuAssignsShortcutContracts();
    void applicationProvidesProfilePreferencesWindow();
    void applicationActionsAreTheOnlyOwnersOfPortableShortcuts();
    void clipboardPasteFailuresAreExplainedWithoutBlockingCanvas();
    void manualUpdateFlowIsExplicitLvrsModalAndCredentialOpaque();
};

void tst_MainQmlContract::presentationModeUsesFullScreenCanvasOnlyAndRestoresOnEscape()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString painterPageQmlPath = QFINDTESTDATA("../src/App/qml/canvas/PainterCanvasPage.qml");
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../src/App/qml/painting/DrawingSurface.qml");
    const QString toolbarQmlPath = QFINDTESTDATA("../src/App/qml/brush/CanvasToolBar.qml");
    const QString laserPointerQmlPath =
        QFINDTESTDATA("../src/App/qml/canvas/PresentationLaserPointer.qml");
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");
    QVERIFY2(!laserPointerQmlPath.isEmpty(),
             "PresentationLaserPointer.qml test data was not found");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString pageSource = readSource(painterPageQmlPath);
    const QString surfaceSource = readSource(drawingSurfaceQmlPath);
    const QString toolbarSource = readSource(toolbarQmlPath);
    const QString laserPointerSource = readSource(laserPointerQmlPath);
    const QString cmakeSource = readSource(cmakePath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!pageSource.isEmpty());
    QVERIFY(!surfaceSource.isEmpty());
    QVERIFY(!toolbarSource.isEmpty());
    QVERIFY(!laserPointerSource.isEmpty());
    QVERIFY(!cmakeSource.isEmpty());

    QVERIFY(toolbarSource.contains(QStringLiteral("signal presentationModeRequested")));
    QVERIFY(
        toolbarSource.contains(QStringLiteral("onClicked: toolbar.presentationModeRequested()")));

    QVERIFY(mainSource.contains(QStringLiteral("property bool presentationMode: false")));
    QVERIFY(mainSource.contains(QStringLiteral("property int prePresentationWindowVisibility: "
                                               "QtQuickWindow.Window.Windowed")));
    QVERIFY(mainSource.contains(QStringLiteral("function enterPresentationMode()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("window.prePresentationWindowVisibility = window.visibility;")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.enterPresentationMode();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.showFullScreen();")));
    QVERIFY(mainSource.contains(QStringLiteral("function exitPresentationMode()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.exitPresentationMode();")));
    QVERIFY(
        mainSource.contains(QStringLiteral("window.restorePrePresentationWindowVisibility();")));
    QVERIFY(mainSource.contains(QStringLiteral("id: exitPresentationModeAction")));
    QVERIFY(mainSource.contains(QStringLiteral("shortcut: \"Escape\"")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: window.presentationMode")));
    QVERIFY(mainSource.contains(
        QStringLiteral("visible: Qt.platform.os === \"windows\" && !window.presentationMode")));
    QVERIFY(mainSource.contains(
        QStringLiteral("visible: Qt.platform.os !== \"windows\" && !window.presentationMode")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onPresentationModeRequested: window.enterPresentationMode()")));

    QVERIFY(pageSource.contains(QStringLiteral("readonly property bool presentationMode: "
                                               "drawingSurface.presentationMode")));
    QVERIFY(pageSource.contains(QStringLiteral("signal presentationModeRequested")));
    QVERIFY(pageSource.contains(QStringLiteral("function enterPresentationMode()")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.enterPresentationMode();")));
    QVERIFY(pageSource.contains(QStringLiteral("function exitPresentationMode()")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.exitPresentationMode();")));
    QVERIFY(pageSource.contains(
        QStringLiteral("function zoomCanvasFromWheel(angleDeltaY, pixelDeltaY)")));
    QVERIFY(pageSource.contains(
        QStringLiteral("drawingSurface.zoomCanvasFromWheel(angleDeltaY, pixelDeltaY);")));
    QVERIFY(pageSource.contains(QStringLiteral("visible: !painterPage.presentationMode")));
    QVERIFY(pageSource.contains(
        QStringLiteral("anchors.left: painterPage.presentationMode ? parent.left : "
                       "layerHierarchyPanel.right")));
    QVERIFY(pageSource.contains(
        QStringLiteral("onPresentationModeRequested: painterPage.presentationModeRequested()")));
    QVERIFY(pageSource.contains(QStringLiteral("PresentationLaserPointer {")));
    QVERIFY(pageSource.contains(QStringLiteral("id: presentationLaserPointer")));
    QVERIFY(pageSource.contains(QStringLiteral("visible: painterPage.presentationMode")));
    QVERIFY(pageSource.contains(QStringLiteral("anchors.fill: parent")));
    QVERIFY(pageSource.contains(QStringLiteral("z: 20")));
    QVERIFY(pageSource.contains(
        QStringLiteral("onWheelZoomRequested: (angleDeltaY, pixelDeltaY) => "
                       "painterPage.zoomCanvasFromWheel(angleDeltaY, pixelDeltaY)")));

    QVERIFY(cmakeSource.contains(QStringLiteral("src/App/qml/canvas/PresentationLaserPointer.qml")));
    QVERIFY(
        laserPointerSource.contains(QStringLiteral("readonly property int trailLifetimeMs: 2000")));
    QVERIFY(
        laserPointerSource.contains(QStringLiteral("readonly property int repaintIntervalMs: 16")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("readonly property int maximumTrailPointCount: 256")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("readonly property int trailOpacityStepCount: 24")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("readonly property real trailGlowWidth: 16")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("property var trailPoints: []")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("property bool laserActive: false")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("signal wheelZoomRequested(real angleDeltaY, real pixelDeltaY)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("function beginLaserPoint(x, y)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("function updateLaserPoint(x, y)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("function endLaserPoint(x, y)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "function appendSmoothTrailRun(context, runPoints)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "function strokeTrailLayer(context, points, now, lineWidth, maximumOpacity)")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("startsStroke: startsStroke === true")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("if (!startsStroke && deltaX === 0 && deltaY === 0)")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("laserPointer.appendTrailPoint(x, y, true, true);")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("function pruneExpiredTrailPoints()")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("function clearLaserTrail()")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("Timer {")));
    QVERIFY(
        laserPointerSource.contains(QStringLiteral("interval: laserPointer.repaintIntervalMs")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("repeat: true")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("Canvas {")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("const ageMs = now - point.createdAt;")));
    QVERIFY(
        laserPointerSource.contains(QStringLiteral("1 - ageMs / laserPointer.trailLifetimeMs")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("context.bezierCurveTo(")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "laserPointer.appendSmoothTrailRun(context, runPoints);")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("context.lineTo(point.x, point.y);")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("context.lineCap = \"butt\";")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("context.lineJoin = \"bevel\";")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("context.lineCap = \"round\";")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("context.lineJoin = \"round\";")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "laserPointer.strokeTrailLayer(context, points, now, "
        "laserPointer.trailGlowWidth, 0.18);")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "laserPointer.strokeTrailLayer(context, points, now, "
        "laserPointer.trailLineWidth, 0.55);")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("context.arc(")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("context.fill()")));
    QCOMPARE(laserPointerSource.count(QStringLiteral("id: activeLaserPoint")), 1);
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("visible: laserPointer.laserActive")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("x: laserPointer.currentPointX - width / 2")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("y: laserPointer.currentPointY - height / 2")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("color: \"#ff2b2b\"")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("MouseArea {")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("acceptedButtons: Qt.LeftButton")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("preventStealing: true")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("cursorShape: pressed ? Qt.BlankCursor : Qt.ArrowCursor")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("mouse.accepted = true;")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("onWheel: function (wheel)")));
    QVERIFY(laserPointerSource.contains(QStringLiteral(
        "laserPointer.wheelZoomRequested(wheel.angleDelta.y, wheel.pixelDelta.y);")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("wheel.accepted = true;")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("signal panRequested")));
    QVERIFY(!laserPointerSource.contains(QStringLiteral("beginPanDrag")));
    QVERIFY(laserPointerSource.contains(QStringLiteral("onVisibleChanged: if (!visible)")));

    QVERIFY(surfaceSource.contains(QStringLiteral("property bool presentationMode: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: !surface.presentationMode")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property real presentationMinimumZoomMultiplier: 0.125")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property real presentationMaximumZoomMultiplier: 8")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("property real presentationFittedCanvasZoomScale: 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.presentationPreviousCanvasZoomScale = "
                                                  "surface.canvasZoomScale;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.presentationPreviousCanvasPanOffsetX = "
                                                  "surface.canvasPanOffsetX;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.presentationPreviousCanvasPanOffsetY = "
                                                  "surface.canvasPanOffsetY;")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function fittedPresentationCanvasZoomScale(canvasWidth, canvasHeight)")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("function fitCanvasZoomToPresentationViewport()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function zoomPresentationCanvas(zoomFactor)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("surface.presentationFittedCanvasZoomScale = fittedScale;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.canvasPanOffsetX = 0;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.canvasPanOffsetY = 0;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.presentationFittedCanvasZoomScale * "
                                                  "surface.presentationMinimumZoomMultiplier")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.presentationFittedCanvasZoomScale * "
                                                  "surface.presentationMaximumZoomMultiplier")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surface.canvasZoomScale * normalizedZoomFactor")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const fittedScale = fittedPresentationCanvasZoomScale(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.canvasZoomScale = fittedScale;")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("return Math.max(surface.minimumCanvasZoomScale, Math.min(")));
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("return boundedCanvasZoomScale(Math.min(surface.workspaceCanvasWidth")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("readonly property int workspaceCanvasHorizontalInset: "
                       "surface.presentationMode ? 0 :")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property int workspaceCanvasTopInset: "
                                                  "surface.presentationMode ? 0 :")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("readonly property int workspaceCanvasBottomInset: "
                                              "surface.presentationMode ? 0 :")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.canvasZoomScale = "
                                                  "surface.presentationPreviousCanvasZoomScale;")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surface.canvasPanOffsetX = "
                                              "surface.presentationPreviousCanvasPanOffsetX;")));
    QVERIFY(
        surfaceSource.contains(QStringLiteral("surface.canvasPanOffsetY = "
                                              "surface.presentationPreviousCanvasPanOffsetY;")));
}

void tst_MainQmlContract::canvasWheelZoomIsAvailableInEveryToolMode()
{
    const QString painterPageQmlPath = QFINDTESTDATA("../src/App/qml/canvas/PainterCanvasPage.qml");
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../src/App/qml/painting/DrawingSurface.qml");
    const QString laserPointerQmlPath =
        QFINDTESTDATA("../src/App/qml/canvas/PresentationLaserPointer.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");
    QVERIFY2(!laserPointerQmlPath.isEmpty(),
             "PresentationLaserPointer.qml test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString pageSource = readSource(painterPageQmlPath);
    const QString surfaceSource = readSource(drawingSurfaceQmlPath);
    const QString laserPointerSource = readSource(laserPointerQmlPath);
    QVERIFY(!pageSource.isEmpty());
    QVERIFY(!surfaceSource.isEmpty());
    QVERIFY(!laserPointerSource.isEmpty());

    QVERIFY(surfaceSource.contains(
        QStringLiteral("function wheelZoomFactor(angleDeltaY, pixelDeltaY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function zoomCanvas(zoomFactor)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("function zoomCanvasFromWheel(angleDeltaY, pixelDeltaY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function handleCanvasWheel(wheel)")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("return surface.zoomPresentationCanvas(normalizedZoomFactor);")));
    QVERIFY(surfaceSource.contains(QStringLiteral(
        "surface.canvasZoomScale = surface.boundedCanvasZoomScale(surface.canvasZoomScale * "
        "normalizedZoomFactor);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.ensureInfiniteCanvasForViewport();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"canvasWheelZoomHandler\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("target: null")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad")));
    QVERIFY(surfaceSource.contains(QStringLiteral("cursorShape: surface.canvasCursorShape()")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("const handled = surface.zoomCanvasFromWheel(wheel.angleDelta.y, "
                       "wheel.pixelDelta.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("wheel.accepted = handled;")));
    QCOMPARE(surfaceSource.count(QStringLiteral("surface.handleCanvasWheel(wheel);")), 3);
    QVERIFY(!surfaceSource.contains(
        QStringLiteral("surface.brushDeltaRequested(wheel.angleDelta.y > 0 ? 1 : -1);")));

    QVERIFY(pageSource.contains(
        QStringLiteral("function zoomCanvasFromWheel(angleDeltaY, pixelDeltaY)")));
    QVERIFY(pageSource.contains(
        QStringLiteral("drawingSurface.zoomCanvasFromWheel(angleDeltaY, pixelDeltaY);")));
    QVERIFY(laserPointerSource.contains(
        QStringLiteral("signal wheelZoomRequested(real angleDeltaY, real pixelDeltaY)")));
    QVERIFY(!laserPointerSource.contains(
        QStringLiteral("function wheelZoomFactor(angleDeltaY, pixelDeltaY)")));
}

void tst_MainQmlContract::clipboardPasteFailuresAreExplainedWithoutBlockingCanvas()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("function clipboardPasteFailureText(errorCode)")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"no-image\":")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"decode-failed\":")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"image-too-large\":")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"cache-write-failed\":")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"download-failed\":")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"download-too-large\":")));
    QVERIFY(mainSource.contains(QStringLiteral("function showClipboardPasteFailure(errorCode)")));
    QVERIFY(mainSource.contains(QStringLiteral("clipboardPasteFailureTimer.restart();")));
    QVERIFY(mainSource.contains(QStringLiteral("onClipboardImagePasteFailed:")));
    QVERIFY(mainSource.contains(QStringLiteral("onImageDropFailed:")));
    QVERIFY(mainSource.contains(QStringLiteral("id: clipboardPasteFailureCard")));
    QVERIFY(mainSource.contains(
        QStringLiteral("visible: window.clipboardPasteFailureMessage.length > 0")));
    QVERIFY(mainSource.contains(QStringLiteral("id: clipboardPasteFailureTimer")));
}

void tst_MainQmlContract::manualUpdateFlowIsExplicitLvrsModalAndCredentialOpaque()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString updateManagerPath =
        QFINDTESTDATA("../src/App/models/update/congregationupdatemanager.cpp");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!updateManagerPath.isEmpty(), "Congregation update manager test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());
    QFile updateManager(updateManagerPath);
    QVERIFY(updateManager.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString updateManagerSource = QString::fromUtf8(updateManager.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("Check for Updates…")));
    QVERIFY(mainSource.contains(QStringLiteral("LV.Modal")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationUpdateManager.checkForUpdates()")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationUpdateManager.updateNow()")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationUpdateManager.cancelUpdate()")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationUpdateManager.progress")));
    QVERIFY(
        mainSource.contains(QStringLiteral("enabled: CongregationUpdateManager.selfUpdateSupported")));
    QVERIFY(
        mainSource.contains(QStringLiteral("visible: CongregationUpdateManager.selfUpdateSupported")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("GetCurrentPackageFullName")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("APPMODEL_ERROR_NO_PACKAGE")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("_MASReceipt/receipt")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("IISACCDistributionChannel")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("Congregation will not quit automatically")));
    QVERIFY(updateManagerSource.contains(QStringLiteral("platform safety policy")));
    QVERIFY(!updateManagerSource.contains(QStringLiteral("platform signature")));
    QVERIFY(updateManagerSource.contains(
        QStringLiteral("case UpdateManager::UpdateError::DownloadInvalidResponse:")));
    const qsizetype canCancelStart = updateManagerSource.indexOf(
        QStringLiteral("bool CongregationUpdateManager::canCancel() const noexcept"));
    const qsizetype canCancelEnd = updateManagerSource.indexOf(
        QStringLiteral("bool CongregationUpdateManager::checkForUpdates()"), canCancelStart);
    QVERIFY(canCancelStart >= 0);
    QVERIFY(canCancelEnd > canCancelStart);
    const QString canCancelBlock =
        updateManagerSource.mid(canCancelStart, canCancelEnd - canCancelStart);
    QVERIFY(canCancelBlock.contains(QStringLiteral("State::Authorizing")));
    QVERIFY(canCancelBlock.contains(QStringLiteral("State::Downloading")));
    QVERIFY(canCancelBlock.contains(QStringLiteral("State::Verifying")));
    QVERIFY(canCancelBlock.contains(QStringLiteral("State::LaunchingInstaller")));
    QVERIFY(mainSource.contains(QStringLiteral("dismissOnBackground: !CongregationUpdateManager.busy")));

    QVERIFY(!mainSource.contains(QStringLiteral("licenseKey")));
    QVERIFY(!mainSource.contains(QStringLiteral("signedUrl")));
    QVERIFY(!mainSource.contains(QStringLiteral("artifactPath")));
    QVERIFY(!mainSource.contains(QStringLiteral("onUpdateAvailable:")));
    QVERIFY(!mainSource.contains(QStringLiteral("onInstallerLaunched: Qt.quit")));

    const qsizetype startupStart = mainSource.indexOf(QStringLiteral("Component.onCompleted:"));
    const qsizetype startupEnd =
        mainSource.indexOf(QStringLiteral("\n    function "), startupStart);
    QVERIFY(startupStart >= 0);
    QVERIFY(startupEnd > startupStart);
    const QString startupBlock = mainSource.mid(startupStart, startupEnd - startupStart);
    QVERIFY(!startupBlock.contains(QStringLiteral("CongregationUpdateManager")));
}

void tst_MainQmlContract::applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("LV.ApplicationWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("solidChrome: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("autoAttachRuntimeEvents: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("flags:")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.CustomizeWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.FramelessWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.WindowTitleHint")));
}

void tst_MainQmlContract::applicationWindowUsesOnlyTheMacOsFullSizeTitleBarDragRegion()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(
        mainSource.contains(QStringLiteral("windowDragHandleEnabled: Qt.platform.os === \"osx\"")));
    QVERIFY(
        mainSource.contains(QStringLiteral("&& visibility !== QtQuickWindow.Window.FullScreen")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("windowDragHandleEnabled: Qt.platform.os !== \"windows\"")));
    QVERIFY(!mainSource.contains(QStringLiteral("windowDragHandleEnabled: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("windowDragHandleHeight: 0")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("onTitlebarDragRequested: window.requestWindowMove()")));
    QVERIFY(!mainSource.contains(QStringLiteral("MouseArea")));
    QVERIFY(!mainSource.contains(QStringLiteral("DragHandler")));
}

void tst_MainQmlContract::applicationWindowPassesOnlyTheActiveMacOsDragHeightToCanvasPage()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(
        QStringLiteral("topChromeReservedHeight: window.windowDragHandleEnabled ? "
                       "window.windowDragHandleHeight : 0")));
}

void tst_MainQmlContract::applicationWindowUsesStockLvrsGeometry()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("LV.ApplicationWindow")));
    QVERIFY(!mainSource.contains(QStringLiteral("readonly property int initialWidth")));
    QVERIFY(!mainSource.contains(QStringLiteral("readonly property int initialHeight")));
    QVERIFY(!mainSource.contains(QStringLiteral("width: initialWidth")));
    QVERIFY(!mainSource.contains(QStringLiteral("height: initialHeight")));
    QVERIFY(!mainSource.contains(QStringLiteral("readonly property int minimumWindowWidth")));
    QVERIFY(!mainSource.contains(QStringLiteral("readonly property int minimumWindowHeight")));
    QVERIFY(!mainSource.contains(QStringLiteral("minimumWidth: minimumWindowWidth")));
    QVERIFY(!mainSource.contains(QStringLiteral("minimumHeight: minimumWindowHeight")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: false")));
    QVERIFY(!mainSource.contains(QStringLiteral("visible: true")));
}

void tst_MainQmlContract::applicationWindowUsesDeferredCanvasIncubation()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("Loader {")));
    QVERIFY(mainSource.contains(QStringLiteral("id: painterPageLoader")));
    QVERIFY(mainSource.contains(QStringLiteral("property bool canvasIncubationRequested: false")));
    QVERIFY(mainSource.contains(QStringLiteral("asynchronous: true")));
    QVERIFY(mainSource.contains(QStringLiteral("sourceComponent: CanvasViews.PainterCanvasPage")));
    QVERIFY(mainSource.contains(QStringLiteral("Qt.callLater(function ()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasIncubationRequested = true;")));
    QVERIFY(mainSource.contains(QStringLiteral("LV.Label {")));
}

void tst_MainQmlContract::applicationWindowTemporarilyDisablesLicenseEnforcement()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString activationPagePath =
        QFINDTESTDATA("../src/App/qml/license/LicenseActivationPage.qml");
    const QString appEntryPath = QFINDTESTDATA("../src/App/main.cpp");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!activationPagePath.isEmpty(), "LicenseActivationPage.qml test data was not found");
    QVERIFY2(!appEntryPath.isEmpty(), "src/App/main.cpp test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString activationSource = readSource(activationPagePath);
    const QString appEntrySource = readSource(appEntryPath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!activationSource.isEmpty());
    QVERIFY(!appEntrySource.isEmpty());

    QVERIFY(mainSource.contains(QStringLiteral(
        "!CongregationLicenseManager.enforcementEnabled || CongregationLicenseManager.licensed")));
    QVERIFY(mainSource.contains(
        QStringLiteral("active: window.canvasIncubationRequested && window.licenseGranted")));
    QVERIFY(mainSource.contains(QStringLiteral("LicenseViews.LicenseActivationPage")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "visible: CongregationLicenseManager.enforcementEnabled && !window.licenseGranted")));
    QVERIFY(mainSource.contains(QStringLiteral("objectName: \"licensePersistenceWarning\"")));
    QVERIFY(
        !mainSource.contains(QStringLiteral("width: Math.min(560, Math.max(320, parent.width")));
    QVERIFY(
        !mainSource.contains(QStringLiteral("width: Math.min(620, Math.max(320, parent.width")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationLicenseManager.enforcementEnabled && window.licenseGranted && "
                       "CongregationLicenseManager.resultCode === "
                       "\"secure_storage_unavailable\"")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "visible: window.licenseGranted && painterPageLoader.status !== Loader.Ready")));

    QVERIFY(activationSource.contains(QStringLiteral("LV.AppCard")));
    QVERIFY(!activationSource.contains(
        QStringLiteral("width: Math.min(520, Math.max(320, parent.width")));
    QCOMPARE(activationSource.count(QStringLiteral("LV.InputField")), 2);
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"licenseEmailInput\"")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"licenseKeyInput\"")));
    QVERIFY(activationSource.contains(QStringLiteral("maximumLength: 254")));
    QVERIFY(activationSource.contains(QStringLiteral("echoMode: TextInput.Password")));
    QVERIFY(
        activationSource.contains(QStringLiteral("Qt.ImhSensitiveData | Qt.ImhNoPredictiveText")));
    QVERIFY(activationSource.contains(
        QStringLiteral("Accessible.name: qsTr(\"Verified account email\")")));
    QVERIFY(activationSource.contains(
        QStringLiteral("Accessible.name: qsTr(\"Congregation license key\")")));
    QVERIFY(
        activationSource.contains(QStringLiteral("Accessible.name: qsTr(\"Activate Congregation\")")));
    QVERIFY(activationSource.contains(QStringLiteral(
        "CongregationLicenseManager.validateLicense(emailInput.text, licenseKeyInput.text)")));
    QVERIFY(activationSource.contains(
        QStringLiteral("visible: CongregationLicenseManager.hasStoredLicense")));
    QVERIFY(
        activationSource.contains(QStringLiteral("CongregationLicenseManager.retryStoredLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("CongregationLicenseManager.forgetLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"retryStoredLicenseButton\"")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"forgetStoredLicenseButton\"")));
    QVERIFY(!mainSource.contains(QStringLiteral("CongregationLicenseManager.forgetLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("text: qsTr(\"Congregation\")")));
    QVERIFY(!activationSource.contains(QStringLiteral("productIdInput")));
    QVERIFY(!activationSource.contains(QStringLiteral("productIdField")));

    QVERIFY(appEntrySource.contains(
        QStringLiteral("new LicenseManager(LicenseManager::EnforcementMode::Disabled, &engine)")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("setContextProperty(\"CongregationLicenseManager\", licenseManager)")));
}

void tst_MainQmlContract::applicationWindowProvidesApplicationMenuBar()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Controls as Controls")));
    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Window as QtQuickWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("menuBar: Controls.MenuBar")));
    const qsizetype fileMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"File\")"));
    const qsizetype editMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Edit\")"));
    const qsizetype windowMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Window\")"));
    const qsizetype helpMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Help\")"));
    QVERIFY(fileMenuIndex >= 0);
    QVERIFY(editMenuIndex > fileMenuIndex);
    QVERIFY(windowMenuIndex > editMenuIndex);
    QVERIFY(helpMenuIndex > windowMenuIndex);
    QVERIFY(mainSource.contains(QStringLiteral("function requestNewCanvas()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openNewCanvasDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openFileDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openSaveDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.undoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.redoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.pasteClipboardImage();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.addLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.deleteCurrentLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.setToolMode(toolMode);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.selectShapeTool(shapeKind);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.fitCanvasToWindow();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.resetCanvasView();")));
    QVERIFY(mainSource.contains(QStringLiteral("QtQuickWindow.Window.FullScreen")));
    QVERIFY(mainSource.contains(QStringLiteral("Qt.quit()")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"New Canvas...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Open Image...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Save Image As...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Clear Canvas\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Undo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Redo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Paste Image\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Add Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Delete Current Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Tools\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Shape Kind\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Fit Canvas to Window\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Reset Canvas View\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Keyboard Shortcuts\")")));
    QVERIFY(mainSource.contains(
        QStringLiteral("text: qsTr(\"Congregation %1\").arg(Qt.application.version)")));
}

void tst_MainQmlContract::applicationMenuBarUsesLvrsContextMenusOnWindows()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString windowsMenuQmlPath =
        QFINDTESTDATA("../src/App/qml/menus/WindowsApplicationMenuBar.qml");
    const QString rootCMakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!windowsMenuQmlPath.isEmpty(),
             "WindowsApplicationMenuBar.qml test data was not found");
    QVERIFY2(!rootCMakePath.isEmpty(), "CMakeLists.txt test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString windowsMenuSource = readSource(windowsMenuQmlPath);
    const QString cmakeSource = readSource(rootCMakePath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!windowsMenuSource.isEmpty());
    QVERIFY(!cmakeSource.isEmpty());

    QVERIFY(mainSource.contains(QStringLiteral("id: applicationMenuBar")));
    QVERIFY(mainSource.contains(
        QStringLiteral("visible: Qt.platform.os !== \"windows\" && !window.presentationMode")));
    QVERIFY(mainSource.contains(QStringLiteral("implicitHeight: LV.Theme.controlHeightSm")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.button: window.windowColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.buttonText: LV.Theme.bodyColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.text: LV.Theme.bodyColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.disabled.text: LV.Theme.disabledColor")));
    QVERIFY(mainSource.contains(QStringLiteral("component ApplicationMenu: Controls.Menu")));
    QCOMPARE(mainSource.count(QStringLiteral("ApplicationMenu {")), 7);
    QVERIFY(
        mainSource.contains(QStringLiteral("component ApplicationMenuItem: Controls.MenuItem")));
    QVERIFY(mainSource.contains(QStringLiteral("delegate: ApplicationMenuItem")));
    QCOMPARE(mainSource.count(QStringLiteral("ApplicationMenuItem {")), 73);
    QCOMPARE(mainSource.count(QStringLiteral("Controls.MenuItem {")), 1);
    QVERIFY(mainSource.contains(
        QStringLiteral("readonly property color menuTextColor: enabled ? LV.Theme.bodyColor : "
                       "LV.Theme.disabledColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.text: menuTextColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.windowText: menuTextColor")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.light: LV.Theme.surfaceAlt")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.midlight: LV.Theme.surfaceAlt")));
    QVERIFY(mainSource.contains(QStringLiteral("font.family: LV.Theme.fontBody")));
    QVERIFY(mainSource.contains(QStringLiteral("font.pixelSize: LV.Theme.textBody")));
    QVERIFY(mainSource.contains(QStringLiteral("font.weight: LV.Theme.textBodyWeight")));
    QVERIFY(mainSource.contains(QStringLiteral("font.styleName: LV.Theme.textBodyStyleName")));
    QVERIFY(
        mainSource.contains(QStringLiteral("font.letterSpacing: LV.Theme.textBodyLetterSpacing")));
    QVERIFY(mainSource.contains(QStringLiteral("delegate: Controls.MenuBarItem")));
    QVERIFY(mainSource.contains(QStringLiteral("topPadding: LV.Theme.gap2")));
    QVERIFY(mainSource.contains(QStringLiteral("bottomPadding: LV.Theme.gap2")));
    QVERIFY(mainSource.contains(QStringLiteral("contentItem: LV.Label")));
    QVERIFY(mainSource.contains(QStringLiteral("style: body")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "color: applicationMenuBarItem.enabled ? LV.Theme.bodyColor : LV.Theme.disabledColor")));
    QVERIFY(mainSource.contains(QStringLiteral("color: window.windowColor")));
    QVERIFY(mainSource.contains(QStringLiteral("import \"./menus\" as MenuViews")));
    QVERIFY(mainSource.contains(QStringLiteral("header: MenuViews.WindowsApplicationMenuBar")));
    QVERIFY(mainSource.contains(QStringLiteral("id: windowsApplicationMenuBar")));
    QVERIFY(mainSource.contains(QStringLiteral("objectName: \"windowsApplicationMenuBar\"")));
    QVERIFY(mainSource.contains(
        QStringLiteral("visible: Qt.platform.os === \"windows\" && !window.presentationMode")));
    QVERIFY(mainSource.contains(QStringLiteral("height: visible ? implicitHeight : 0")));
    QVERIFY(mainSource.contains(QStringLiteral("barColor: window.windowColor")));
    QVERIFY(mainSource.contains(
        QStringLiteral("updateSupported: CongregationUpdateManager.selfUpdateSupported")));

    const QStringList windowsActionBindings = {
        QStringLiteral("\"newCanvas\": newCanvasAction"),
        QStringLiteral("\"preferences\": preferencesAction"),
        QStringLiteral("\"brushTool\": brushToolAction"),
        QStringLiteral("\"rectangleShape\": rectangleShapeAction"),
        QStringLiteral("\"toggleFullScreen\": toggleFullScreenAction"),
        QStringLiteral("\"checkForUpdates\": checkForUpdatesAction"),
    };
    for (const QString& binding : windowsActionBindings)
    {
        QVERIFY2(mainSource.contains(binding), qPrintable(binding + QStringLiteral(" is missing")));
    }

    QVERIFY(windowsMenuSource.contains(QStringLiteral("import LVRS 1.0 as LV")));
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("component ApplicationContextMenu: LV.ContextMenu")));
    QCOMPARE(windowsMenuSource.count(QStringLiteral("ApplicationContextMenu {")), 12);
    QVERIFY(!windowsMenuSource.contains(QStringLiteral("Controls.Menu {")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("Component {")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("LV.MenuItem {")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("showIconSlot: false")));
    QVERIFY(
        windowsMenuSource.contains(QStringLiteral("itemDelegate: applicationMenuItemDelegate")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("itemWidth: LV.Theme.scaleMetric(220)")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("itemWidth: LV.Theme.scaleMetric(280)")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("Accessible.name: label")));
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("selected: action ? action.checked === true : false")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("trigger: \"globalPressed\"")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("includeUiHit: true")));
    QVERIFY(
        windowsMenuSource.contains(QStringLiteral("closePolicy: Controls.Popup.CloseOnEscape")));
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("onClicked: root.openTopLevelMenu(popupMenu, menuButton, true)")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("onHoveredChanged: {")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral("action.trigger();")));

    const QStringList windowsContextMenuNames = {
        QStringLiteral("windowsFileContextMenu"),
        QStringLiteral("windowsEditContextMenu"),
        QStringLiteral("windowsWindowContextMenu"),
        QStringLiteral("windowsHelpContextMenu"),
        QStringLiteral("windowsToolsContextMenu"),
        QStringLiteral("windowsShapeKindContextMenu"),
        QStringLiteral("windowsKeyboardShortcutsContextMenu"),
        QStringLiteral("windowsShortcutFileContextMenu"),
        QStringLiteral("windowsShortcutEditContextMenu"),
        QStringLiteral("windowsShortcutToolsContextMenu"),
        QStringLiteral("windowsShortcutShapeKindContextMenu"),
        QStringLiteral("windowsShortcutWindowContextMenu"),
    };
    for (const QString& objectName : windowsContextMenuNames)
    {
        QVERIFY2(windowsMenuSource.contains(QStringLiteral("objectName: \"") + objectName +
                                            QStringLiteral("\"")),
                 qPrintable(objectName + QStringLiteral(" is missing")));
    }

    const qsizetype fileButtonIndex =
        windowsMenuSource.indexOf(QStringLiteral("objectName: \"windowsFileMenuButton\""));
    const qsizetype editButtonIndex =
        windowsMenuSource.indexOf(QStringLiteral("objectName: \"windowsEditMenuButton\""));
    const qsizetype windowButtonIndex =
        windowsMenuSource.indexOf(QStringLiteral("objectName: \"windowsWindowMenuButton\""));
    const qsizetype helpButtonIndex =
        windowsMenuSource.indexOf(QStringLiteral("objectName: \"windowsHelpMenuButton\""));
    QVERIFY(fileButtonIndex >= 0);
    QVERIFY(editButtonIndex > fileButtonIndex);
    QVERIFY(windowButtonIndex > editButtonIndex);
    QVERIFY(helpButtonIndex > windowButtonIndex);
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("root.submenuEntry(qsTr(\"Tools\"), \"tools\", \"edit\")")));
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("root.submenuEntry(qsTr(\"Shape Kind\"), \"shapeKind\", \"edit\")")));
    QVERIFY(windowsMenuSource.contains(QStringLiteral(
        "root.submenuEntry(qsTr(\"Keyboard Shortcuts\"), \"keyboardShortcuts\", \"help\")")));
    QVERIFY(windowsMenuSource.contains(
        QStringLiteral("root.referenceEntry(qsTr(\"Preferences\"), \"preferences\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("src/App/qml/menus/WindowsApplicationMenuBar.qml")));

    const QString appEntryPath = QFINDTESTDATA("../src/App/main.cpp");
    QVERIFY2(!appEntryPath.isEmpty(), "src/App/main.cpp test data was not found");
    QFile appEntry(appEntryPath);
    QVERIFY(appEntry.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString appEntrySource = QString::fromUtf8(appEntry.readAll());
    QVERIFY(appEntrySource.contains(
        QStringLiteral("QGuiApplication::setApplicationVersion(QStringLiteral(CONGREGATION_VERSION))")));
    QVERIFY(!appEntrySource.contains(QStringLiteral("AA_DontUseNativeMenuBar")));
}

void tst_MainQmlContract::applicationMenuAssignsShortcutContracts()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(
        QStringLiteral("readonly property string menuCommandModifier: \"Ctrl\"")));
    QVERIFY(
        !mainSource.contains(QStringLiteral("Qt.platform.os === \"osx\" ? \"Meta\" : \"Ctrl\"")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "readonly property string shortcutSaveImageAs: menuCommandModifier + \"+S\"")));
    QVERIFY(
        mainSource.contains(QStringLiteral("readonly property string shortcutRedo: Qt.platform.os "
                                           "=== \"windows\" ? \"Ctrl+Y\" : \"Ctrl+Shift+Z\"")));
    QVERIFY(mainSource.contains(
        QStringLiteral("readonly property string shortcutToggleFullScreen: Qt.platform.os === "
                       "\"osx\" ? \"Meta+Ctrl+F\" : \"F11\"")));
    QVERIFY(mainSource.contains(QStringLiteral("function nativeShortcutText(shortcutText)")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"Ctrl\":")));
    QVERIFY(mainSource.contains(QStringLiteral("return \"Command\";")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"Meta\":")));
    QVERIFY(mainSource.contains(QStringLiteral("return \"Control\";")));
    QVERIFY(mainSource.contains(QStringLiteral("case \"Alt\":")));
    QVERIFY(mainSource.contains(QStringLiteral("return \"Option\";")));
    QVERIFY(mainSource.contains(
        QStringLiteral("function shortcutReference(commandName, shortcutText)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("return commandName + \" - \" + window.nativeShortcutText(shortcutText);")));

    const QStringList actionShortcutContracts = {
        QStringLiteral("shortcutNewCanvas|newCanvasAction"),
        QStringLiteral("shortcutOpenImage|openImageAction"),
        QStringLiteral("shortcutSaveImageAs|saveImageAsAction"),
        QStringLiteral("shortcutClearCanvas|clearCanvasAction"),
        QStringLiteral("shortcutQuit|quitAction"),
        QStringLiteral("shortcutUndo|undoAction"),
        QStringLiteral("shortcutRedo|redoAction"),
        QStringLiteral("shortcutPasteImage|pasteImageAction"),
        QStringLiteral("shortcutAddLayer|addLayerAction"),
        QStringLiteral("shortcutDeleteCurrentLayer|deleteCurrentLayerAction"),
        QStringLiteral("shortcutBrushTool|brushToolAction"),
        QStringLiteral("shortcutEraserTool|eraserToolAction"),
        QStringLiteral("shortcutHandPanTool|handPanToolAction"),
        QStringLiteral("shortcutMoveTool|moveToolAction"),
        QStringLiteral("shortcutZoomTool|zoomToolAction"),
        QStringLiteral("shortcutShapeTool|shapeToolAction"),
        QStringLiteral("shortcutFillTool|fillToolAction"),
        QStringLiteral("shortcutTextTool|textToolAction"),
        QStringLiteral("shortcutRectangleShape|rectangleShapeAction"),
        QStringLiteral("shortcutEllipseShape|ellipseShapeAction"),
        QStringLiteral("shortcutTriangleShape|triangleShapeAction"),
        QStringLiteral("shortcutDiamondShape|diamondShapeAction"),
        QStringLiteral("shortcutStarShape|starShapeAction"),
        QStringLiteral("shortcutRectangleBubbleShape|rectangleBubbleShapeAction"),
        QStringLiteral("shortcutEllipseBubbleShape|ellipseBubbleShapeAction"),
        QStringLiteral("shortcutDecreaseBrushSize|decreaseBrushSizeAction"),
        QStringLiteral("shortcutIncreaseBrushSize|increaseBrushSizeAction"),
        QStringLiteral("shortcutFitCanvasToWindow|fitCanvasToWindowAction"),
        QStringLiteral("shortcutResetCanvasView|resetCanvasViewAction"),
        QStringLiteral("shortcutMinimizeWindow|minimizeWindowAction"),
        QStringLiteral("shortcutToggleFullScreen|toggleFullScreenAction"),
    };

    QVERIFY(mainSource.contains(QStringLiteral("Controls.Action {")));
    QVERIFY(!mainSource.contains(QStringLiteral("Shortcut {")));

    for (const QString& actionShortcutContract : actionShortcutContracts)
    {
        const QStringList parts = actionShortcutContract.split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 2);
        const QString shortcutContract = parts.at(0);
        const QString actionId = parts.at(1);

        QVERIFY2(
            mainSource.contains(QStringLiteral("readonly property string ") + shortcutContract),
            qPrintable(shortcutContract + QStringLiteral(" property is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("id: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" action is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("shortcut: window.") + shortcutContract),
                 qPrintable(shortcutContract + QStringLiteral(" action shortcut is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("action: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" menu binding is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral(", window.") + shortcutContract +
                                     QStringLiteral(")")),
                 qPrintable(shortcutContract + QStringLiteral(" help reference is missing")));
    }
}

void tst_MainQmlContract::applicationProvidesProfilePreferencesWindow()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString preferencesQmlPath =
        QFINDTESTDATA("../src/App/qml/preferences/PreferencesWindow.qml");
    const QString painterPageQmlPath = QFINDTESTDATA("../src/App/qml/canvas/PainterCanvasPage.qml");
    const QString rootCMakePath = QFINDTESTDATA("../CMakeLists.txt");
    const QString appEntryPath = QFINDTESTDATA("../src/App/main.cpp");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!preferencesQmlPath.isEmpty(), "PreferencesWindow.qml test data was not found");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!rootCMakePath.isEmpty(), "CMakeLists.txt test data was not found");
    QVERIFY2(!appEntryPath.isEmpty(), "src/App/main.cpp test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString preferencesSource = readSource(preferencesQmlPath);
    const QString painterPageSource = readSource(painterPageQmlPath);
    const QString cmakeSource = readSource(rootCMakePath);
    const QString appEntrySource = readSource(appEntryPath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!preferencesSource.isEmpty());
    QVERIFY(!painterPageSource.isEmpty());
    QVERIFY(!cmakeSource.isEmpty());
    QVERIFY(!appEntrySource.isEmpty());

    QVERIFY(mainSource.contains(QStringLiteral("import \"./preferences\" as PreferencesViews")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "readonly property string shortcutPreferences: menuCommandModifier + \"+,\"")));
    QVERIFY(mainSource.contains(QStringLiteral("function requestPreferences()")));
    const qsizetype refreshAccountEmailIndex =
        mainSource.indexOf(QStringLiteral("CongregationAccountManager.refresh();"));
    const qsizetype centerPreferencesIndex =
        mainSource.indexOf(QStringLiteral("preferencesWindow.applyInitialCentering();"));
    const qsizetype showGeneralSectionIndex =
        mainSource.indexOf(QStringLiteral("preferencesWindow.showGeneralSection();"));
    const qsizetype showPreferencesIndex =
        mainSource.indexOf(QStringLiteral("preferencesWindow.showNormal();"));
    QVERIFY(centerPreferencesIndex >= 0);
    QVERIFY(refreshAccountEmailIndex >= 0);
    QVERIFY(showGeneralSectionIndex >= 0);
    QVERIFY(showPreferencesIndex >= 0);
    QVERIFY(refreshAccountEmailIndex < showPreferencesIndex);
    QVERIFY(centerPreferencesIndex < showPreferencesIndex);
    QVERIFY(showGeneralSectionIndex < showPreferencesIndex);
    QVERIFY(mainSource.contains(QStringLiteral("preferencesWindow.showNormal();")));
    QVERIFY(mainSource.contains(QStringLiteral("preferencesWindow.raise();")));
    QVERIFY(mainSource.contains(QStringLiteral("preferencesWindow.requestActivate();")));
    QVERIFY(mainSource.contains(QStringLiteral("id: preferencesAction")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Preferences...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("shortcut: StandardKey.Preferences")));
    QVERIFY(mainSource.contains(QStringLiteral("onTriggered: window.requestPreferences()")));
    QVERIFY(mainSource.contains(QStringLiteral("action: preferencesAction")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "window.shortcutReference(qsTr(\"Preferences\"), window.shortcutPreferences)")));
    QVERIFY(mainSource.contains(QStringLiteral("PreferencesViews.PreferencesWindow {")));
    QVERIFY(mainSource.contains(QStringLiteral("id: preferencesWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("transientParent: window")));
    QVERIFY(
        mainSource.contains(QStringLiteral("updateCheckEnabled: checkForUpdatesAction.enabled")));
    QVERIFY(
        mainSource.contains(QStringLiteral("accountEmail: CongregationAccountManager.accountEmail")));
    QVERIFY(mainSource.contains(
        QStringLiteral("accountEmailLoading: CongregationAccountManager.accountEmailLoading")));
    QVERIFY(!mainSource.contains(QStringLiteral("CongregationLicenseManager.accountEmail")));
    QVERIFY(appEntrySource.contains(QStringLiteral("new AccountManager(licenseManager, &engine)")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("setContextProperty(\"CongregationAccountManager\", accountManager)")));
    QVERIFY(
        cmakeSource.contains(QStringLiteral("find_package(iiLicenseManager 0.2 CONFIG REQUIRED)")));
    QVERIFY(cmakeSource.contains(QStringLiteral("iiLicenseManager::iiLicenseManager")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "startWithRecentCanvas: CongregationApplicationPreferences.startWithRecentCanvas")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "discoverNearbyCongregationUsers: CongregationApplicationPreferences.discoverNearbyCongregationUsers")));
    QVERIFY(mainSource.contains(QStringLiteral("currentCanvasMemberProfiles: window.canvasPage ? "
                                               "window.canvasPage.collaboratorProfiles : []")));
    QVERIFY(
        mainSource.contains(QStringLiteral("currentUserIsCanvasHost: window.canvasPage ? "
                                           "window.canvasPage.currentUserIsCanvasHost : true")));
    QVERIFY(
        mainSource.contains(QStringLiteral("localCanvasState: CongregationLocalCanvasSession.state")));
    QVERIFY(mainSource.contains(
        QStringLiteral("localCanvasError: CongregationLocalCanvasSession.errorString")));
    QVERIFY(mainSource.contains(
        QStringLiteral("localCanvasParticipantCount: CongregationLocalCanvasSession.participantCount")));
    QVERIFY(mainSource.contains(
        QStringLiteral("availableLocalCanvases: CongregationLocalCanvasSession.availableCanvases")));
    QVERIFY(mainSource.contains(
        QStringLiteral("availableLocalInvitees: CongregationLocalCanvasSession.availableInvitees")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("setContextProperty(\"CongregationMemberProfileListBuilder\",")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("setContextProperty(\"CongregationLocalCanvasSession\", localCanvasSession)")));
    QVERIFY(mainSource.contains(QStringLiteral("onCanInviteOtherUsersChanged: {")));
    QVERIFY(mainSource.contains(QStringLiteral("if (canInviteOtherUsers) {")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(true);")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationLocalCanvasSession.setInvitationsAllowed(canInviteOtherUsers);")));
    QVERIFY(
        mainSource.contains(QStringLiteral("onInviteCanvasMemberRequested: function (sessionId)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationLocalCanvasSession.invitePeer(sessionId, profileName);")));
    QVERIFY(mainSource.contains(QStringLiteral("onDeleteCanvasMemberRequested:")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationLocalCanvasSession.removeParticipant(String(profile.peerId));")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onStartWithRecentCanvasRequested: enabled => "
                       "CongregationApplicationPreferences.setStartWithRecentCanvas(enabled)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onDiscoverNearbyCongregationUsersRequested: function (enabled)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(enabled);")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationLocalCanvasSession.stopSession();")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "onProfileNameChanged: CongregationLocalCanvasSession.setLocalProfileName(profileName)")));
    QVERIFY(mainSource.contains(QStringLiteral("onHostCanvasRequested:")));
    QVERIFY(mainSource.contains(QStringLiteral("onJoinCanvasRequested: function (sessionId)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("CongregationLocalCanvasSession.joinCanvas(sessionId, profileName);")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onLeaveCanvasRequested: CongregationLocalCanvasSession.stopSession()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onRestorePurchasesRequested: window.requestRestorePurchases()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onCheckForUpdatesRequested: window.requestUpdateCheckFromPreferences()")));
    QVERIFY(mainSource.contains(QStringLiteral("function requestUpdateCheckFromPreferences()")));
    const qsizetype hidePreferencesForUpdateIndex =
        mainSource.indexOf(QStringLiteral("preferencesWindow.hide();"));
    const qsizetype triggerUpdateFromPreferencesIndex =
        mainSource.indexOf(QStringLiteral("checkForUpdatesAction.trigger();"));
    QVERIFY(hidePreferencesForUpdateIndex >= 0);
    QVERIFY(triggerUpdateFromPreferencesIndex > hidePreferencesForUpdateIndex);
    QVERIFY(mainSource.contains(QStringLiteral(
        "readonly property url accountDashboardUrl: \"https://iisacc.com/Account/Dashboard\"")));
    QVERIFY(mainSource.contains(QStringLiteral("function requestRestorePurchases()")));
    QVERIFY(
        mainSource.contains(QStringLiteral("Qt.openUrlExternally(window.accountDashboardUrl)")));
    QVERIFY(mainSource.contains(QStringLiteral("function acceptCanvasPage(page)")));
    QVERIFY(
        mainSource.contains(QStringLiteral("CongregationApplicationPreferences.startWithRecentCanvas")));
    QVERIFY(mainSource.contains(QStringLiteral("CongregationApplicationPreferences.recentCanvasUrl")));
    QVERIFY(
        mainSource.contains(QStringLiteral("CongregationApplicationPreferences.clearRecentCanvas();")));
    QVERIFY(
        mainSource.contains(QStringLiteral("onPageReady: window.acceptCanvasPage(painterPage)")));
    QVERIFY(mainSource.contains(QStringLiteral("page.openRecentCanvas(recentCanvasUrl)")));
    QVERIFY(!mainSource.contains(QStringLiteral("onCanvasFileActivated:")));
    QVERIFY(mainSource.contains(QStringLiteral("onClosing: event =>")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.flushRecentCanvasSave();")));

    QVERIFY(appEntrySource.contains(
        QStringLiteral("QGuiApplication::setOrganizationName(QStringLiteral(\"iisacc\"))")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("QGuiApplication::setOrganizationDomain(QStringLiteral(\"iisacc.com\"))")));
    QVERIFY(appEntrySource.contains(QStringLiteral("new ApplicationPreferences(&engine)")));
    QVERIFY(appEntrySource.contains(
        QStringLiteral("setContextProperty(\"CongregationApplicationPreferences\",")));

    QVERIFY(preferencesSource.contains(QStringLiteral("LV.Window {")));
    QVERIFY(preferencesSource.contains(QStringLiteral("objectName: \"preferencesWindow\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("visible: false")));
    QVERIFY(preferencesSource.contains(QStringLiteral("title: qsTr(\"Preferences\")")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property bool initialCenteringApplied: false")));
    QVERIFY(preferencesSource.contains(QStringLiteral("function applyInitialCentering()")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("if (initialCenteringApplied || !transientParent)")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("const parentCenterX = transientParent.x + transientParent.width / 2;")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("const parentCenterY = transientParent.y + transientParent.height / 2;")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("x = Math.round(parentCenterX - width / 2);")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("y = Math.round(parentCenterY - height / 2);")));
    QVERIFY(preferencesSource.contains(QStringLiteral("initialCenteringApplied = true;")));
    QVERIFY(preferencesSource.contains(QStringLiteral("function showGeneralSection()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("generalSectionButton.checked = true;")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "readonly property url profileImageSource: CongregationProfileImageProcessor.imageSource")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("property alias profileName: profileNameField.text")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("property alias canInviteOtherUsers: inviteOtherUsersCheckBox.checked")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property string accountEmail: \"\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property bool accountEmailLoading: false")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property bool startWithRecentCanvas: false")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("property bool discoverNearbyCongregationUsers: true")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property bool restorePurchasesEnabled: true")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property bool updateCheckEnabled: false")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property var currentCanvasMemberProfiles: []")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property bool currentUserIsCanvasHost: true")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property string localCanvasState: \"idle\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property string localCanvasError: \"\"")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("property int localCanvasParticipantCount: 0")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property var availableLocalCanvases: []")));
    QVERIFY(preferencesSource.contains(QStringLiteral("property var availableLocalInvitees: []")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("readonly property bool localCanvasActive: localCanvasState !== \"idle\"")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("readonly property var displayedCanvasMemberProfiles: "
                       "CongregationMemberProfileListBuilder.build(")));
    QVERIFY(preferencesSource.contains(QStringLiteral("signal restorePurchasesRequested")));
    QVERIFY(preferencesSource.contains(QStringLiteral("signal checkForUpdatesRequested")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("signal startWithRecentCanvasRequested(bool enabled)")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("signal discoverNearbyCongregationUsersRequested(bool enabled)")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("signal inviteCanvasMemberRequested(string sessionId)")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("signal deleteCanvasMemberRequested(var profile, int index)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("signal hostCanvasRequested")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("signal joinCanvasRequested(string sessionId)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("signal leaveCanvasRequested")));

    QVERIFY(preferencesSource.contains(QStringLiteral("Dialogs.FileDialog {")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileImageDialog")));
    QVERIFY(preferencesSource.contains(QStringLiteral("title: qsTr(\"Choose profile image\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "onAccepted: CongregationProfileImageProcessor.processProfileImage(selectedFile)")));
    QVERIFY(!preferencesSource.contains(
        QStringLiteral("preferencesWindow.profileImageSource = selectedFile")));

    QCOMPARE(preferencesSource.count(QStringLiteral("LV.LabelSegmentedControl {")), 1);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: preferencesSectionHeader")));
    QVERIFY(preferencesSource.contains(QStringLiteral("objectName: \"preferencesSectionHeader\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("anchors.top: parent.top")));
    QVERIFY(preferencesSource.contains(QStringLiteral("anchors.topMargin: LV.Theme.gap24")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("anchors.horizontalCenter: parent.horizontalCenter")));
    QCOMPARE(preferencesSource.count(QStringLiteral("LV.LabelButton {")), 8);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: generalSectionButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"General\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileSectionButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Profile\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: membersSectionButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Members\")")));
    const qsizetype generalSectionIndex =
        preferencesSource.indexOf(QStringLiteral("id: generalSectionButton"));
    const qsizetype profileSectionIndex =
        preferencesSource.indexOf(QStringLiteral("id: profileSectionButton"));
    const qsizetype defaultCheckedIndex =
        preferencesSource.indexOf(QStringLiteral("checked: true"), generalSectionIndex);
    QVERIFY(generalSectionIndex >= 0);
    QVERIFY(profileSectionIndex >= 0);
    QVERIFY(defaultCheckedIndex > generalSectionIndex);
    QVERIFY(defaultCheckedIndex < profileSectionIndex);
    QCOMPARE(preferencesSource.count(QStringLiteral("checked: true")), 1);
    QCOMPARE(preferencesSource.count(QStringLiteral("checkable: true")), 3);
    QCOMPARE(preferencesSource.count(QStringLiteral("autoExclusive: true")), 5);
    QVERIFY(
        preferencesSource.contains(QStringLiteral("anchors.top: preferencesSectionHeader.bottom")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("id: profileSectionHeader")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("id: profileSectionIcon")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileSettings")));
    QVERIFY(preferencesSource.contains(QStringLiteral("visible: profileSectionButton.checked")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("selectedSection")));

    QVERIFY(preferencesSource.contains(QStringLiteral("id: generalSettings")));
    QVERIFY(preferencesSource.contains(QStringLiteral("visible: generalSectionButton.checked")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: accountEmailLabel")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Account\")")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("iisacc account email")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: accountEmailValueLabel")));
    QVERIFY(preferencesSource.contains(QStringLiteral("preferencesWindow.accountEmailLoading")));
    QVERIFY(preferencesSource.contains(QStringLiteral("preferencesWindow.accountEmail.length")));
    QVERIFY(preferencesSource.contains(QStringLiteral("qsTr(\"Not connected\")")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("licenseKey")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Startup with\")")));
    QCOMPARE(preferencesSource.count(QStringLiteral("LV.RadioButton {")), 2);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: newCanvasRadioButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"New canvas\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("checked: !preferencesWindow.startWithRecentCanvas")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("preferencesWindow.startWithRecentCanvasRequested(false)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: recentCanvasRadioButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Recent canvas\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("checked: preferencesWindow.startWithRecentCanvas")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("preferencesWindow.startWithRecentCanvasRequested(true)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: discoverNearbyCongregationUsersCheckBox")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("text: qsTr(\"Discover nearby Congregation users\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("checked: preferencesWindow.discoverNearbyCongregationUsers")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "onToggled: preferencesWindow.discoverNearbyCongregationUsersRequested(checked)")));
    QCOMPARE(preferencesSource.count(QStringLiteral("onToggled:")), 3);
    QVERIFY(!preferencesSource.contains(QStringLiteral("CongregationNearbyDiscovery.start();")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("CongregationNearbyDiscovery.stop();")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: generalActions")));
    QVERIFY(preferencesSource.contains(QStringLiteral("anchors.bottom: parent.bottom")));
    QCOMPARE(preferencesSource.count(QStringLiteral("LV.Spacer {")), 1);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: restorePurchasesButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Restore Purchases\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("enabled: preferencesWindow.restorePurchasesEnabled")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("onClicked: preferencesWindow.restorePurchasesRequested()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: checkForUpdatesButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Check for Updates…\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("enabled: preferencesWindow.updateCheckEnabled")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("onClicked: preferencesWindow.checkForUpdatesRequested()")));
    QCOMPARE(preferencesSource.count(QStringLiteral("LV.List {")), 1);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: membersSettings")));
    QVERIFY(preferencesSource.contains(QStringLiteral("visible: membersSectionButton.checked")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: localCanvasJoinMenu")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("items: preferencesWindow.localCanvasJoinItems()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: localCanvasActions")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: localCanvasStatusLabel")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: shareCanvasButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Share canvas\")")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("onClicked: preferencesWindow.hostCanvasRequested()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: joinCanvasButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("text: qsTr(\"Join nearby…\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: leaveCanvasButton")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("onClicked: preferencesWindow.leaveCanvasRequested()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: memberProfileDelegate")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: memberList")));
    QVERIFY(preferencesSource.contains(QStringLiteral("objectName: \"memberList\"")));
    const qsizetype membersSettingsIndex =
        preferencesSource.indexOf(QStringLiteral("id: membersSettings"));
    const qsizetype memberListIndex =
        preferencesSource.indexOf(QStringLiteral("id: memberList"), membersSettingsIndex);
    QVERIFY(membersSettingsIndex >= 0);
    QVERIFY(memberListIndex > membersSettingsIndex);
    const QString membersSettingsSource =
        preferencesSource.mid(membersSettingsIndex, memberListIndex - membersSettingsIndex);
    const QString memberListSource = preferencesSource.mid(memberListIndex);
    QVERIFY(membersSettingsSource.contains(QStringLiteral("anchors.topMargin: LV.Theme.gap20")));
    QVERIFY(memberListSource.contains(QStringLiteral("anchors.top: localCanvasActions.bottom")));
    QVERIFY(memberListSource.contains(QStringLiteral("anchors.topMargin: LV.Theme.gap12")));
    QVERIFY(memberListSource.contains(QStringLiteral("anchors.bottom: parent.bottom")));
    QVERIFY(memberListSource.contains(QStringLiteral("anchors.bottomMargin: LV.Theme.gap24")));
    QVERIFY(memberListSource.contains(
        QStringLiteral("anchors.horizontalCenter: parent.horizontalCenter")));
    QVERIFY(!memberListSource.contains(QStringLiteral("height: implicitHeight")));
    QVERIFY(preferencesSource.contains(QStringLiteral("listWidth: LV.Theme.scaleMetric(237)")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("minimumListHeight: LV.Theme.scaleMetric(231)")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("model: preferencesWindow.displayedCanvasMemberProfiles")));
    QVERIFY(preferencesSource.contains(QStringLiteral("labelRole: \"displayName\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("defaultItemIconName: \"user\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("itemDelegate: memberProfileDelegate")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("iconSource: memberList.memberProfileImageSource(entry)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("footerVisible: true")));
    QVERIFY(preferencesSource.contains(QStringLiteral("iconName: \"add\"")));
    QVERIFY(!memberListSource.contains(QStringLiteral("iconName: \"addFile\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("iconName: \"generaldelete\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("iconName: \"\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("iconGlyph: \" \"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("enabled: false")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: localCanvasInviteMenu")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("items: preferencesWindow.localCanvasInviteItems()")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("preferencesWindow.inviteCanvasMemberRequested(String(item.sessionId));")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("localCanvasInviteMenu.openFor(memberList, 0, memberList.height)")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "const sourceProfile = memberList.roleValue(selectedProfile, \"sourceProfile\",")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "const sourceIndex = memberList.roleValue(selectedProfile, \"sourceIndex\", -1);")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "preferencesWindow.deleteCanvasMemberRequested(sourceProfile, sourceIndex);")));
    QVERIFY(painterPageSource.contains(QStringLiteral("property var collaboratorProfiles: []")));
    QVERIFY(
        painterPageSource.contains(QStringLiteral("property bool currentUserIsCanvasHost: true")));
    QVERIFY(painterPageSource.contains(QStringLiteral("property var localCanvasSession: null")));
    QVERIFY(mainSource.contains(QStringLiteral("localCanvasSession: CongregationLocalCanvasSession")));
    QVERIFY(mainSource.contains(
        QStringLiteral("collaboratorProfiles: CongregationLocalCanvasSession.participantProfiles")));
    QVERIFY(mainSource.contains(
        QStringLiteral("currentUserIsCanvasHost: CongregationLocalCanvasSession.currentUserIsHost")));
    QVERIFY(mainSource.contains(
        QStringLiteral("pendingInvitation: CongregationLocalCanvasSession.pendingInvitation")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "pendingInvitationCount: CongregationLocalCanvasSession.pendingInvitationCount")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onInvitationResponseRequested: accepted => "
                       "CongregationLocalCanvasSession.respondToPendingInvitation(accepted, "
                       "preferencesWindow.profileName)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("signal collaboratorInvitationRequested")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("signal invitationResponseRequested(bool accepted)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("property var pendingInvitation: ({})")));
    QVERIFY(painterPageSource.contains(QStringLiteral("property int pendingInvitationCount: 0")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("pendingInvitation: painterPage.pendingInvitation")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("pendingInvitationCount: painterPage.pendingInvitationCount")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("onInvitationResponseRequested: accepted => "
                       "painterPage.invitationResponseRequested(accepted)")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("signal collaboratorRemovalRequested(var profile, int index)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function requestAddCollaborator()")));
    QVERIFY(painterPageSource.contains(
        QStringLiteral("function requestRemoveCollaborator(profile, index)")));

    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileImageButton")));
    QVERIFY(preferencesSource.contains(QStringLiteral("tone: LV.AbstractButton.Borderless")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("shapeStyle: profileImageButton.shapeCylinder")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("readonly property int avatarInset: Math.max(0, "
                                                  "Math.round((avatarSize - iconSize) / 2))")));
    QVERIFY(preferencesSource.contains(QStringLiteral("horizontalPadding: avatarInset")));
    QVERIFY(preferencesSource.contains(QStringLiteral("verticalPadding: avatarInset")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("iconSource: preferencesWindow.profileImageSource")));
    QVERIFY(preferencesSource.contains(QStringLiteral("LV.ContextMenu {")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileImageMenuItemDelegate")));
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileImageMenu")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("itemDelegate: profileImageMenuItemDelegate")));
    QVERIFY(preferencesSource.contains(QStringLiteral("showIconSlot: false")));
    QVERIFY(preferencesSource.contains(QStringLiteral("Accessible.name: label")));
    QVERIFY(preferencesSource.contains(QStringLiteral("label: qsTr(\"Select profile image\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral("action: \"select\"")));
    QVERIFY(preferencesSource.contains(QStringLiteral("label: qsTr(\"Delete profile image\")")));
    QVERIFY(preferencesSource.contains(QStringLiteral("action: \"delete\"")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("enabled: preferencesWindow.profileImageSource.toString().length > 0")));
    QVERIFY(preferencesSource.contains(QStringLiteral("onItemTriggered: function (index, item)")));
    QVERIFY(preferencesSource.contains(QStringLiteral("Qt.callLater(function ()")));
    QVERIFY(preferencesSource.contains(QStringLiteral("profileImageDialog.open();")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("CongregationProfileImageProcessor.clearProfileImage();")));
    QVERIFY(preferencesSource.contains(QStringLiteral(
        "onClicked: profileImageMenu.openFor(profileImageButton, 0, profileImageButton.height)")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("onClicked: profileImageDialog.open()")));
    QVERIFY(preferencesSource.contains(
        QStringLiteral("Accessible.name: qsTr(\"Profile image options\")")));

    QCOMPARE(preferencesSource.count(QStringLiteral("LV.InputField {")), 1);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: profileNameField")));
    QVERIFY(preferencesSource.contains(QStringLiteral("placeholder: qsTr(\"Profile name\")")));
    QCOMPARE(preferencesSource.count(QStringLiteral("LV.CheckBox {")), 2);
    QVERIFY(preferencesSource.contains(QStringLiteral("id: inviteOtherUsersCheckBox")));
    QVERIFY(
        preferencesSource.contains(QStringLiteral("text: qsTr(\"Allow inviting other users\")")));
    QVERIFY(!preferencesSource.contains(QStringLiteral("preferencesPlaceholderButton")));
    QVERIFY(cmakeSource.contains(QStringLiteral("src/App/qml/preferences/PreferencesWindow.qml")));
}

void tst_MainQmlContract::applicationActionsAreTheOnlyOwnersOfPortableShortcuts()
{
    const QString mainQmlPath = QFINDTESTDATA("../src/App/qml/Main.qml");
    const QString pageQmlPath = QFINDTESTDATA("../src/App/qml/canvas/PainterCanvasPage.qml");
    const QString toolbarQmlPath = QFINDTESTDATA("../src/App/qml/brush/CanvasToolBar.qml");
    const QString surfaceQmlPath = QFINDTESTDATA("../src/App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");
    QVERIFY2(!surfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    auto readSource = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString pageSource = readSource(pageQmlPath);
    const QString toolbarSource = readSource(toolbarQmlPath);
    const QString surfaceSource = readSource(surfaceQmlPath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!pageSource.isEmpty());
    QVERIFY(!toolbarSource.isEmpty());
    QVERIFY(!surfaceSource.isEmpty());

    QVERIFY(pageSource.contains(
        QStringLiteral("readonly property bool dialogActive: canvasToolBar.dialogActive")));
    QVERIFY(pageSource.contains(
        QStringLiteral("readonly property bool textEditingActive: drawingSurface.textEditingActive "
                       "|| painterPage.layerRenameActive")));
    QVERIFY(pageSource.contains(QStringLiteral(
        "toolShortcutsEnabled: !painterPage.layerRenameActive && !canvasToolBar.dialogActive")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property bool canvasCommandsEnabled")));
    QVERIFY(
        mainSource.contains(QStringLiteral("readonly property bool canvasEditingCommandsEnabled")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: window.canvasCommandsEnabled")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: window.canvasEditingCommandsEnabled")));

    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.New")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.Open")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.Save")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Meta+Shift+K")));

    const QStringList latinToolSequences = {
        QStringLiteral("sequences: [\"B\", \"ㅠ\"]"), QStringLiteral("sequences: [\"E\", \"ㄷ\"]"),
        QStringLiteral("sequences: [\"H\", \"ㅗ\"]"), QStringLiteral("sequences: [\"V\", \"ㅍ\"]"),
        QStringLiteral("sequences: [\"Z\", \"ㅋ\"]"), QStringLiteral("sequences: [\"U\", \"ㅕ\"]"),
        QStringLiteral("sequences: [\"G\", \"ㅎ\"]"), QStringLiteral("sequences: [\"T\", \"ㅅ\"]"),
    };
    for (const QString& sequence : latinToolSequences)
    {
        QVERIFY2(!surfaceSource.contains(sequence), qPrintable(sequence));
    }
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅠ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㄷ\"]")));
    QVERIFY(surfaceSource.contains(
        QStringLiteral("enabled: surface.toolShortcutsEnabled && !surface.textEditingActive")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [\"[\"]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [\"]\"]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Undo]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Redo]")));
}

QTEST_APPLESS_MAIN(tst_MainQmlContract)

#include "tst_mainqmlcontract.moc"
