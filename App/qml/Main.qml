import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Window as QtQuickWindow
import LVRS 1.0 as LV
import "./canvas" as CanvasViews
import "./license" as LicenseViews
import "./menus" as MenuViews
import "./preferences" as PreferencesViews

LV.ApplicationWindow {
    id: window
    visible: false
    solidChrome: true
    windowDragHandleEnabled: Qt.platform.os === "osx" && visibility !== QtQuickWindow.Window.FullScreen
    navigationEnabled: false

    property var canvasPage: null
    property bool canvasIncubationRequested: false
    property string clipboardPasteFailureMessage: ""
    property bool presentationMode: false
    property int prePresentationWindowVisibility: QtQuickWindow.Window.Windowed
    property bool prePresentationPreferencesVisible: false
    readonly property url accountDashboardUrl: "https://iisacc.com/Account/Dashboard"
    readonly property bool licenseGranted: !CongregationLicenseManager.enforcementEnabled || CongregationLicenseManager.licensed
    readonly property bool canvasCommandsEnabled: canvasPage !== null && !canvasPage.dialogActive
    readonly property bool canvasEditingCommandsEnabled: canvasCommandsEnabled && !canvasPage.textEditingActive
    readonly property string currentToolMode: canvasPage && canvasPage.vm ? canvasPage.vm.toolMode : ""
    readonly property string currentShapeKind: canvasPage && canvasPage.vm ? canvasPage.vm.shapeKind : ""
    // QKeySequence's portable Ctrl token maps to the physical Command key on Apple platforms.
    readonly property string menuCommandModifier: "Ctrl"
    readonly property string shortcutNewCanvas: menuCommandModifier + "+N"
    readonly property string shortcutOpenImage: menuCommandModifier + "+O"
    readonly property string shortcutSaveImageAs: menuCommandModifier + "+S"
    readonly property string shortcutClearCanvas: menuCommandModifier + "+Shift+K"
    readonly property string shortcutQuit: menuCommandModifier + "+Q"
    readonly property string shortcutUndo: menuCommandModifier + "+Z"
    readonly property string shortcutRedo: Qt.platform.os === "windows" ? "Ctrl+Y" : "Ctrl+Shift+Z"
    readonly property string shortcutPasteImage: menuCommandModifier + "+V"
    readonly property string shortcutAddLayer: menuCommandModifier + "+Shift+N"
    readonly property string shortcutDeleteCurrentLayer: menuCommandModifier + "+Shift+Delete"
    readonly property string shortcutBrushTool: "B"
    readonly property string shortcutEraserTool: "E"
    readonly property string shortcutHandPanTool: "H"
    readonly property string shortcutMoveTool: "V"
    readonly property string shortcutZoomTool: "Z"
    readonly property string shortcutShapeTool: "U"
    readonly property string shortcutFillTool: "G"
    readonly property string shortcutTextTool: "T"
    readonly property string shortcutRectangleShape: menuCommandModifier + "+Alt+1"
    readonly property string shortcutEllipseShape: menuCommandModifier + "+Alt+2"
    readonly property string shortcutTriangleShape: menuCommandModifier + "+Alt+3"
    readonly property string shortcutDiamondShape: menuCommandModifier + "+Alt+4"
    readonly property string shortcutStarShape: menuCommandModifier + "+Alt+5"
    readonly property string shortcutRectangleBubbleShape: menuCommandModifier + "+Alt+6"
    readonly property string shortcutEllipseBubbleShape: menuCommandModifier + "+Alt+7"
    readonly property string shortcutDecreaseBrushSize: "["
    readonly property string shortcutIncreaseBrushSize: "]"
    readonly property string shortcutFitCanvasToWindow: menuCommandModifier + "+0"
    readonly property string shortcutResetCanvasView: menuCommandModifier + "+1"
    readonly property string shortcutMinimizeWindow: menuCommandModifier + "+M"
    readonly property string shortcutToggleFullScreen: Qt.platform.os === "osx" ? "Meta+Ctrl+F" : "F11"
    readonly property string shortcutPreferences: menuCommandModifier + "+,"

    component ApplicationMenuItem: Controls.MenuItem {
        readonly property color menuTextColor: enabled ? LV.Theme.bodyColor : LV.Theme.disabledColor

        font.family: LV.Theme.fontBody
        font.pixelSize: LV.Theme.textBody
        font.weight: LV.Theme.textBodyWeight
        font.styleName: LV.Theme.textBodyStyleName
        font.letterSpacing: LV.Theme.textBodyLetterSpacing
        palette.text: menuTextColor
        palette.windowText: menuTextColor
        palette.light: LV.Theme.surfaceAlt
        palette.midlight: LV.Theme.surfaceAlt
    }

    component ApplicationMenu: Controls.Menu {
        delegate: ApplicationMenuItem {}
        font.family: LV.Theme.fontBody
        font.pixelSize: LV.Theme.textBody
        font.weight: LV.Theme.textBodyWeight
        font.styleName: LV.Theme.textBodyStyleName
        font.letterSpacing: LV.Theme.textBodyLetterSpacing
        palette.text: LV.Theme.bodyColor
        palette.disabled.text: LV.Theme.disabledColor
    }

    Component.onCompleted: Qt.callLater(function () {
        window.canvasIncubationRequested = true;
    })

    onClosing: event => {
        if (window.canvasPage) {
            window.canvasPage.flushRecentCanvasSave();
        }
    }

    onVisibilityChanged: {
        if (window.presentationMode && window.visibility === QtQuickWindow.Window.FullScreen && window.canvasPage) {
            Qt.callLater(window.canvasPage.refreshPresentationMode);
        }
    }

    function requestNewCanvas() {
        if (window.canvasPage) {
            window.canvasPage.openNewCanvasDialog();
        }
    }

    function requestOpenImage() {
        if (window.canvasPage) {
            window.canvasPage.openFileDialog();
        }
    }

    function requestSaveImage() {
        if (window.canvasPage) {
            window.canvasPage.openSaveDialog();
        }
    }

    function requestClearCanvas() {
        if (window.canvasPage) {
            window.canvasPage.clearCanvas();
        }
    }

    function requestUndo() {
        if (window.canvasPage) {
            window.canvasPage.undoActiveRasterSurface();
        }
    }

    function requestRedo() {
        if (window.canvasPage) {
            window.canvasPage.redoActiveRasterSurface();
        }
    }

    function requestPasteImage() {
        if (window.canvasPage) {
            const pasted = window.canvasPage.pasteClipboardImage();
            if (pasted) {
                window.clipboardPasteFailureMessage = "";
                clipboardPasteFailureTimer.stop();
            }
            return pasted;
        }
        return false;
    }

    function clipboardPasteFailureText(errorCode) {
        switch (String(errorCode || "")) {
        case "clipboard-unavailable":
            return qsTr("The system clipboard is unavailable.");
        case "canvas-unavailable":
            return qsTr("The canvas is still loading. Try pasting again in a moment.");
        case "no-image":
            return qsTr("Copy an image or a local image file, then paste again.");
        case "decode-failed":
            return qsTr("Congregation could not read the image. Try another image or copy it and paste again.");
        case "image-too-large":
            return qsTr("The image is too large. Use an image up to 32,768 pixels per side and 64 megapixels.");
        case "cache-write-failed":
            return qsTr("Congregation could not store the image. Check available disk space and cache-folder permissions.");
        case "download-failed":
            return qsTr("Congregation could not download the dropped web image. Check the connection, or save the image locally and drop it again.");
        case "download-too-large":
            return qsTr("The dropped web image download exceeds the 64 MB safety limit. Save or resize it locally, then drop it again.");
        default:
            return qsTr("Congregation could not paste the clipboard image.");
        }
    }

    function showClipboardPasteFailure(errorCode) {
        window.clipboardPasteFailureMessage = window.clipboardPasteFailureText(errorCode);
        clipboardPasteFailureTimer.restart();
    }

    function requestAddLayer() {
        if (window.canvasPage) {
            window.canvasPage.addLayer();
        }
    }

    function requestDeleteCurrentLayer() {
        if (window.canvasPage) {
            window.canvasPage.deleteCurrentLayer();
        }
    }

    function requestToolMode(toolMode) {
        if (window.canvasPage) {
            window.canvasPage.setToolMode(toolMode);
        }
    }

    function requestShapeTool(shapeKind) {
        if (window.canvasPage) {
            window.canvasPage.selectShapeTool(shapeKind);
        }
    }

    function requestBrushSizeDelta(delta) {
        if (window.canvasPage) {
            window.canvasPage.adjustBrush(delta);
        }
    }

    function requestFitCanvasToWindow() {
        if (window.canvasPage) {
            window.canvasPage.fitCanvasToWindow();
        }
    }

    function requestResetCanvasView() {
        if (window.canvasPage) {
            window.canvasPage.resetCanvasView();
        }
    }

    function requestMinimizeWindow() {
        window.showMinimized();
    }

    function requestToggleFullScreen() {
        if (window.presentationMode) {
            window.exitPresentationMode();
            return;
        }
        if (window.visibility === QtQuickWindow.Window.FullScreen) {
            window.showNormal();
            return;
        }
        window.showFullScreen();
    }

    function restorePrePresentationWindowVisibility() {
        switch (window.prePresentationWindowVisibility) {
        case QtQuickWindow.Window.FullScreen:
            window.showFullScreen();
            break;
        case QtQuickWindow.Window.Maximized:
            window.showMaximized();
            break;
        case QtQuickWindow.Window.Minimized:
            window.showMinimized();
            break;
        default:
            window.showNormal();
            break;
        }
    }

    function enterPresentationMode() {
        if (window.presentationMode || !window.canvasPage) {
            return;
        }

        window.prePresentationWindowVisibility = window.visibility;
        window.prePresentationPreferencesVisible = preferencesWindow.visible;
        preferencesWindow.hide();
        window.clipboardPasteFailureMessage = "";
        clipboardPasteFailureTimer.stop();
        window.presentationMode = true;
        window.canvasPage.enterPresentationMode();
        window.showFullScreen();
    }

    function exitPresentationMode() {
        if (!window.presentationMode) {
            return;
        }

        if (window.canvasPage) {
            window.canvasPage.exitPresentationMode();
        }
        window.presentationMode = false;
        window.restorePrePresentationWindowVisibility();

        if (window.prePresentationPreferencesVisible) {
            Qt.callLater(function () {
                preferencesWindow.show();
                preferencesWindow.raise();
            });
        }
        window.prePresentationPreferencesVisible = false;
    }

    function requestPreferences() {
        CongregationAccountManager.refresh();
        preferencesWindow.showGeneralSection();
        preferencesWindow.applyInitialCentering();
        preferencesWindow.showNormal();
        preferencesWindow.raise();
        preferencesWindow.requestActivate();
    }

    function requestRestorePurchases() {
        return Qt.openUrlExternally(window.accountDashboardUrl);
    }

    function acceptCanvasPage(page) {
        window.canvasPage = page;
        if (!CongregationApplicationPreferences.startWithRecentCanvas) {
            return;
        }

        const recentCanvasUrl = CongregationApplicationPreferences.recentCanvasUrl;
        if (recentCanvasUrl.toString().length === 0) {
            return;
        }
        if (!page.openRecentCanvas(recentCanvasUrl)) {
            CongregationApplicationPreferences.clearRecentCanvas();
        }
    }

    function requestUpdateCheckFromPreferences() {
        preferencesWindow.hide();
        window.raise();
        window.requestActivate();
        checkForUpdatesAction.trigger();
    }

    function nativeShortcutText(shortcutText) {
        const shortcutTokens = String(shortcutText).split("+");
        if (Qt.platform.os !== "osx") {
            return shortcutTokens.join("+");
        }

        return shortcutTokens.map(function (token) {
            switch (token) {
            case "Ctrl":
                return "Command";
            case "Meta":
                return "Control";
            case "Alt":
                return "Option";
            default:
                return token;
            }
        }).join("+");
    }

    function shortcutReference(commandName, shortcutText) {
        return commandName + " - " + window.nativeShortcutText(shortcutText);
    }

    Controls.Action {
        id: newCanvasAction
        text: qsTr("New Canvas...")
        shortcut: window.shortcutNewCanvas
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestNewCanvas()
    }

    Controls.Action {
        id: openImageAction
        text: qsTr("Open Image...")
        shortcut: window.shortcutOpenImage
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestOpenImage()
    }

    Controls.Action {
        id: saveImageAsAction
        text: qsTr("Save Image As...")
        shortcut: window.shortcutSaveImageAs
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestSaveImage()
    }

    Controls.Action {
        id: clearCanvasAction
        text: qsTr("Clear Canvas")
        shortcut: window.shortcutClearCanvas
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestClearCanvas()
    }

    Controls.Action {
        id: quitAction
        text: qsTr("Quit Congregation")
        shortcut: window.shortcutQuit
        onTriggered: Qt.quit()
    }

    Controls.Action {
        id: preferencesAction
        text: qsTr("Preferences...")
        shortcut: StandardKey.Preferences
        onTriggered: window.requestPreferences()
    }

    Controls.Action {
        id: checkForUpdatesAction
        text: qsTr("Check for Updates…")
        enabled: CongregationUpdateManager.selfUpdateSupported && !CongregationUpdateManager.busy
        onTriggered: {
            updateModal.open = true;
            CongregationUpdateManager.checkForUpdates();
        }
    }

    Controls.Action {
        id: undoAction
        text: qsTr("Undo")
        shortcut: window.shortcutUndo
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestUndo()
    }

    Controls.Action {
        id: redoAction
        text: qsTr("Redo")
        shortcut: window.shortcutRedo
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestRedo()
    }

    Controls.Action {
        id: pasteImageAction
        text: qsTr("Paste Image")
        shortcut: window.shortcutPasteImage
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestPasteImage()
    }

    Controls.Action {
        id: addLayerAction
        text: qsTr("Add Layer")
        shortcut: window.shortcutAddLayer
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestAddLayer()
    }

    Controls.Action {
        id: deleteCurrentLayerAction
        text: qsTr("Delete Current Layer")
        shortcut: window.shortcutDeleteCurrentLayer
        enabled: window.canvasEditingCommandsEnabled && window.canvasPage.canDeleteCurrentLayer()
        onTriggered: window.requestDeleteCurrentLayer()
    }

    Controls.Action {
        id: brushToolAction
        text: qsTr("Brush")
        shortcut: window.shortcutBrushTool
        checkable: true
        checked: window.currentToolMode === "brush"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("brush")
    }

    Controls.Action {
        id: eraserToolAction
        text: qsTr("Eraser")
        shortcut: window.shortcutEraserTool
        checkable: true
        checked: window.currentToolMode === "eraser"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("eraser")
    }

    Controls.Action {
        id: handPanToolAction
        text: qsTr("Hand Pan")
        shortcut: window.shortcutHandPanTool
        checkable: true
        checked: window.currentToolMode === "pan"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("pan")
    }

    Controls.Action {
        id: moveToolAction
        text: qsTr("Move")
        shortcut: window.shortcutMoveTool
        checkable: true
        checked: window.currentToolMode === "move"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("move")
    }

    Controls.Action {
        id: zoomToolAction
        text: qsTr("Zoom")
        shortcut: window.shortcutZoomTool
        checkable: true
        checked: window.currentToolMode === "zoom"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("zoom")
    }

    Controls.Action {
        id: shapeToolAction
        text: qsTr("Shape")
        shortcut: window.shortcutShapeTool
        checkable: true
        checked: window.currentToolMode === "shape"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("shape")
    }

    Controls.Action {
        id: fillToolAction
        text: qsTr("Fill")
        shortcut: window.shortcutFillTool
        checkable: true
        checked: window.currentToolMode === "fill"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("fill")
    }

    Controls.Action {
        id: textToolAction
        text: qsTr("Text")
        shortcut: window.shortcutTextTool
        checkable: true
        checked: window.currentToolMode === "text"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("text")
    }

    Controls.Action {
        id: rectangleShapeAction
        text: qsTr("Rectangle")
        shortcut: window.shortcutRectangleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectangle"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("rectangle")
    }

    Controls.Action {
        id: ellipseShapeAction
        text: qsTr("Ellipse")
        shortcut: window.shortcutEllipseShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipse"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("ellipse")
    }

    Controls.Action {
        id: triangleShapeAction
        text: qsTr("Triangle")
        shortcut: window.shortcutTriangleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "triangle"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("triangle")
    }

    Controls.Action {
        id: diamondShapeAction
        text: qsTr("Diamond")
        shortcut: window.shortcutDiamondShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "diamond"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("diamond")
    }

    Controls.Action {
        id: starShapeAction
        text: qsTr("Star")
        shortcut: window.shortcutStarShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "star"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("star")
    }

    Controls.Action {
        id: rectangleBubbleShapeAction
        text: qsTr("Rectangle Bubble")
        shortcut: window.shortcutRectangleBubbleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectanglebubble"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("rectanglebubble")
    }

    Controls.Action {
        id: ellipseBubbleShapeAction
        text: qsTr("Ellipse Bubble")
        shortcut: window.shortcutEllipseBubbleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipsebubble"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("ellipsebubble")
    }

    Controls.Action {
        id: decreaseBrushSizeAction
        text: qsTr("Decrease Brush Size")
        shortcut: window.shortcutDecreaseBrushSize
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestBrushSizeDelta(-1)
    }

    Controls.Action {
        id: increaseBrushSizeAction
        text: qsTr("Increase Brush Size")
        shortcut: window.shortcutIncreaseBrushSize
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestBrushSizeDelta(1)
    }

    Controls.Action {
        id: fitCanvasToWindowAction
        text: qsTr("Fit Canvas to Window")
        shortcut: window.shortcutFitCanvasToWindow
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestFitCanvasToWindow()
    }

    Controls.Action {
        id: resetCanvasViewAction
        text: qsTr("Reset Canvas View")
        shortcut: window.shortcutResetCanvasView
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestResetCanvasView()
    }

    Controls.Action {
        id: minimizeWindowAction
        text: qsTr("Minimize")
        shortcut: window.shortcutMinimizeWindow
        onTriggered: window.requestMinimizeWindow()
    }

    Controls.Action {
        id: toggleFullScreenAction
        text: window.visibility === QtQuickWindow.Window.FullScreen ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen")
        shortcut: window.shortcutToggleFullScreen
        onTriggered: window.requestToggleFullScreen()
    }

    Controls.Action {
        id: exitPresentationModeAction
        shortcut: "Escape"
        enabled: window.presentationMode
        onTriggered: window.exitPresentationMode()
    }

    header: MenuViews.WindowsApplicationMenuBar {
        id: windowsApplicationMenuBar
        objectName: "windowsApplicationMenuBar"
        visible: Qt.platform.os === "windows" && !window.presentationMode
        height: visible ? implicitHeight : 0
        barColor: window.windowColor
        applicationVersion: Qt.application.version
        updateSupported: CongregationUpdateManager.selfUpdateSupported
        menuActions: ({
            "newCanvas": newCanvasAction,
            "openImage": openImageAction,
            "saveImageAs": saveImageAsAction,
            "clearCanvas": clearCanvasAction,
            "quit": quitAction,
            "preferences": preferencesAction,
            "checkForUpdates": checkForUpdatesAction,
            "undo": undoAction,
            "redo": redoAction,
            "pasteImage": pasteImageAction,
            "addLayer": addLayerAction,
            "deleteCurrentLayer": deleteCurrentLayerAction,
            "brushTool": brushToolAction,
            "eraserTool": eraserToolAction,
            "handPanTool": handPanToolAction,
            "moveTool": moveToolAction,
            "zoomTool": zoomToolAction,
            "shapeTool": shapeToolAction,
            "fillTool": fillToolAction,
            "textTool": textToolAction,
            "rectangleShape": rectangleShapeAction,
            "ellipseShape": ellipseShapeAction,
            "triangleShape": triangleShapeAction,
            "diamondShape": diamondShapeAction,
            "starShape": starShapeAction,
            "rectangleBubbleShape": rectangleBubbleShapeAction,
            "ellipseBubbleShape": ellipseBubbleShapeAction,
            "decreaseBrushSize": decreaseBrushSizeAction,
            "increaseBrushSize": increaseBrushSizeAction,
            "fitCanvasToWindow": fitCanvasToWindowAction,
            "resetCanvasView": resetCanvasViewAction,
            "minimizeWindow": minimizeWindowAction,
            "toggleFullScreen": toggleFullScreenAction
        })
        shortcutTexts: ({
            "newCanvas": window.shortcutNewCanvas,
            "openImage": window.shortcutOpenImage,
            "saveImageAs": window.shortcutSaveImageAs,
            "clearCanvas": window.shortcutClearCanvas,
            "quit": window.shortcutQuit,
            "preferences": window.shortcutPreferences,
            "undo": window.shortcutUndo,
            "redo": window.shortcutRedo,
            "pasteImage": window.shortcutPasteImage,
            "addLayer": window.shortcutAddLayer,
            "deleteCurrentLayer": window.shortcutDeleteCurrentLayer,
            "brushTool": window.shortcutBrushTool,
            "eraserTool": window.shortcutEraserTool,
            "handPanTool": window.shortcutHandPanTool,
            "moveTool": window.shortcutMoveTool,
            "zoomTool": window.shortcutZoomTool,
            "shapeTool": window.shortcutShapeTool,
            "fillTool": window.shortcutFillTool,
            "textTool": window.shortcutTextTool,
            "rectangleShape": window.shortcutRectangleShape,
            "ellipseShape": window.shortcutEllipseShape,
            "triangleShape": window.shortcutTriangleShape,
            "diamondShape": window.shortcutDiamondShape,
            "starShape": window.shortcutStarShape,
            "rectangleBubbleShape": window.shortcutRectangleBubbleShape,
            "ellipseBubbleShape": window.shortcutEllipseBubbleShape,
            "decreaseBrushSize": window.shortcutDecreaseBrushSize,
            "increaseBrushSize": window.shortcutIncreaseBrushSize,
            "fitCanvasToWindow": window.shortcutFitCanvasToWindow,
            "resetCanvasView": window.shortcutResetCanvasView,
            "minimizeWindow": window.shortcutMinimizeWindow,
            "toggleFullScreen": window.shortcutToggleFullScreen
        })
    }

    menuBar: Controls.MenuBar {
        id: applicationMenuBar
        objectName: "applicationMenuBar"
        visible: Qt.platform.os !== "windows" && !window.presentationMode
        implicitHeight: LV.Theme.controlHeightSm
        padding: LV.Theme.gapNone
        spacing: LV.Theme.gapNone
        palette.button: window.windowColor
        palette.buttonText: LV.Theme.bodyColor
        palette.mid: LV.Theme.surfaceAlt

        delegate: Controls.MenuBarItem {
            id: applicationMenuBarItem
            implicitHeight: applicationMenuBar.implicitHeight
            topPadding: LV.Theme.gap2
            bottomPadding: LV.Theme.gap2
            leftPadding: LV.Theme.gap8
            rightPadding: LV.Theme.gap8

            contentItem: LV.Label {
                text: applicationMenuBarItem.text
                style: body
                color: applicationMenuBarItem.enabled ? LV.Theme.bodyColor : LV.Theme.disabledColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                implicitHeight: applicationMenuBar.implicitHeight
                color: applicationMenuBarItem.down || applicationMenuBarItem.highlighted ? LV.Theme.surfaceAlt : "transparent"
            }
        }

        background: Rectangle {
            implicitHeight: LV.Theme.controlHeightSm
            color: window.windowColor
        }

        ApplicationMenu {
            title: qsTr("File")

            ApplicationMenuItem {
                action: newCanvasAction
            }

            ApplicationMenuItem {
                action: openImageAction
            }

            ApplicationMenuItem {
                action: saveImageAsAction
            }

            ApplicationMenuItem {
                action: clearCanvasAction
            }

            Controls.MenuSeparator {}

            ApplicationMenuItem {
                action: quitAction
            }
        }

        ApplicationMenu {
            title: qsTr("Edit")

            ApplicationMenuItem {
                action: preferencesAction
            }

            Controls.MenuSeparator {}

            ApplicationMenuItem {
                action: undoAction
            }

            ApplicationMenuItem {
                action: redoAction
            }

            ApplicationMenuItem {
                action: pasteImageAction
            }

            Controls.MenuSeparator {}

            ApplicationMenuItem {
                action: addLayerAction
            }

            ApplicationMenuItem {
                action: deleteCurrentLayerAction
            }

            Controls.MenuSeparator {}

            ApplicationMenu {
                title: qsTr("Tools")

                ApplicationMenuItem {
                    action: brushToolAction
                }

                ApplicationMenuItem {
                    action: eraserToolAction
                }

                ApplicationMenuItem {
                    action: handPanToolAction
                }

                ApplicationMenuItem {
                    action: moveToolAction
                }

                ApplicationMenuItem {
                    action: zoomToolAction
                }

                ApplicationMenuItem {
                    action: shapeToolAction
                }

                ApplicationMenuItem {
                    action: fillToolAction
                }

                ApplicationMenuItem {
                    action: textToolAction
                }
            }

            ApplicationMenu {
                title: qsTr("Shape Kind")

                ApplicationMenuItem {
                    action: rectangleShapeAction
                }

                ApplicationMenuItem {
                    action: ellipseShapeAction
                }

                ApplicationMenuItem {
                    action: triangleShapeAction
                }

                ApplicationMenuItem {
                    action: diamondShapeAction
                }

                ApplicationMenuItem {
                    action: starShapeAction
                }

                ApplicationMenuItem {
                    action: rectangleBubbleShapeAction
                }

                ApplicationMenuItem {
                    action: ellipseBubbleShapeAction
                }
            }

            Controls.MenuSeparator {}

            ApplicationMenuItem {
                action: decreaseBrushSizeAction
            }

            ApplicationMenuItem {
                action: increaseBrushSizeAction
            }
        }

        ApplicationMenu {
            title: qsTr("Window")

            ApplicationMenuItem {
                action: fitCanvasToWindowAction
            }

            ApplicationMenuItem {
                action: resetCanvasViewAction
            }

            Controls.MenuSeparator {}

            ApplicationMenuItem {
                action: minimizeWindowAction
            }

            ApplicationMenuItem {
                action: toggleFullScreenAction
            }
        }

        ApplicationMenu {
            title: qsTr("Help")

            ApplicationMenuItem {
                action: checkForUpdatesAction
                visible: CongregationUpdateManager.selfUpdateSupported
            }

            Controls.MenuSeparator {
                visible: CongregationUpdateManager.selfUpdateSupported
            }

            ApplicationMenu {
                title: qsTr("Keyboard Shortcuts")

                ApplicationMenuItem {
                    text: qsTr("Application")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Preferences"), window.shortcutPreferences)
                    enabled: false
                }

                Controls.MenuSeparator {}

                ApplicationMenuItem {
                    text: qsTr("File")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("New Canvas"), window.shortcutNewCanvas)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Open Image"), window.shortcutOpenImage)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Save Image As"), window.shortcutSaveImageAs)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Clear Canvas"), window.shortcutClearCanvas)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Quit Congregation"), window.shortcutQuit)
                    enabled: false
                }

                Controls.MenuSeparator {}

                ApplicationMenuItem {
                    text: qsTr("Edit")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Undo"), window.shortcutUndo)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Redo"), window.shortcutRedo)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Paste Image"), window.shortcutPasteImage)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Add Layer"), window.shortcutAddLayer)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Delete Current Layer"), window.shortcutDeleteCurrentLayer)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Decrease Brush Size"), window.shortcutDecreaseBrushSize)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Increase Brush Size"), window.shortcutIncreaseBrushSize)
                    enabled: false
                }

                Controls.MenuSeparator {}

                ApplicationMenuItem {
                    text: qsTr("Tools")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Brush"), window.shortcutBrushTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Eraser"), window.shortcutEraserTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Hand Pan"), window.shortcutHandPanTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Move"), window.shortcutMoveTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Zoom"), window.shortcutZoomTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Shape"), window.shortcutShapeTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Fill"), window.shortcutFillTool)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Text"), window.shortcutTextTool)
                    enabled: false
                }

                Controls.MenuSeparator {}

                ApplicationMenuItem {
                    text: qsTr("Shape Kind")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Rectangle"), window.shortcutRectangleShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Ellipse"), window.shortcutEllipseShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Triangle"), window.shortcutTriangleShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Diamond"), window.shortcutDiamondShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Star"), window.shortcutStarShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Rectangle Bubble"), window.shortcutRectangleBubbleShape)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Ellipse Bubble"), window.shortcutEllipseBubbleShape)
                    enabled: false
                }

                Controls.MenuSeparator {}

                ApplicationMenuItem {
                    text: qsTr("Window")
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Fit Canvas to Window"), window.shortcutFitCanvasToWindow)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Reset Canvas View"), window.shortcutResetCanvasView)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Minimize"), window.shortcutMinimizeWindow)
                    enabled: false
                }

                ApplicationMenuItem {
                    text: window.shortcutReference(qsTr("Enter Full Screen"), window.shortcutToggleFullScreen)
                    enabled: false
                }
            }

            ApplicationMenuItem {
                text: qsTr("Congregation %1").arg(Qt.application.version)
                enabled: false
            }
        }
    }

    PreferencesViews.PreferencesWindow {
        id: preferencesWindow
        transientParent: window
        accountEmail: CongregationAccountManager.accountEmail
        accountEmailLoading: CongregationAccountManager.accountEmailLoading
        startWithRecentCanvas: CongregationApplicationPreferences.startWithRecentCanvas
        discoverNearbyCongregationUsers: CongregationApplicationPreferences.discoverNearbyCongregationUsers
        currentCanvasMemberProfiles: window.canvasPage ? window.canvasPage.collaboratorProfiles : []
        currentUserIsCanvasHost: window.canvasPage ? window.canvasPage.currentUserIsCanvasHost : true
        localCanvasState: CongregationLocalCanvasSession.state
        localCanvasError: CongregationLocalCanvasSession.errorString
        localCanvasParticipantCount: CongregationLocalCanvasSession.participantCount
        availableLocalCanvases: CongregationLocalCanvasSession.availableCanvases
        availableLocalInvitees: CongregationLocalCanvasSession.availableInvitees
        updateCheckEnabled: checkForUpdatesAction.enabled
        onStartWithRecentCanvasRequested: enabled => CongregationApplicationPreferences.setStartWithRecentCanvas(enabled)
        onDiscoverNearbyCongregationUsersRequested: function (enabled) {
            CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(enabled);
            if (!enabled)
                CongregationLocalCanvasSession.stopSession();
        }
        onProfileNameChanged: CongregationLocalCanvasSession.setLocalProfileName(profileName)
        onCanInviteOtherUsersChanged: {
            if (canInviteOtherUsers) {
                CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(true);
            }
            CongregationLocalCanvasSession.setInvitationsAllowed(canInviteOtherUsers);
        }
        onHostCanvasRequested: {
            CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(true);
            CongregationLocalCanvasSession.startHosting(profileName);
        }
        onJoinCanvasRequested: function (sessionId) {
            CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(true);
            CongregationLocalCanvasSession.joinCanvas(sessionId, profileName);
        }
        onLeaveCanvasRequested: CongregationLocalCanvasSession.stopSession()
        onInviteCanvasMemberRequested: function (sessionId) {
            CongregationApplicationPreferences.setDiscoverNearbyCongregationUsers(true);
            CongregationLocalCanvasSession.invitePeer(sessionId, profileName);
        }
        onDeleteCanvasMemberRequested: function (profile, index) {
            if (profile && profile.peerId)
                CongregationLocalCanvasSession.removeParticipant(String(profile.peerId));
        }
        onRestorePurchasesRequested: window.requestRestorePurchases()
        onCheckForUpdatesRequested: window.requestUpdateCheckFromPreferences()
    }

    Loader {
        id: painterPageLoader
        anchors.fill: parent
        active: window.canvasIncubationRequested && window.licenseGranted
        asynchronous: true
        sourceComponent: CanvasViews.PainterCanvasPage {
            id: painterPage
            topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0
            localCanvasSession: CongregationLocalCanvasSession
            collaboratorProfiles: CongregationLocalCanvasSession.participantProfiles
            currentUserIsCanvasHost: CongregationLocalCanvasSession.currentUserIsHost
            pendingInvitation: CongregationLocalCanvasSession.pendingInvitation
            pendingInvitationCount: CongregationLocalCanvasSession.pendingInvitationCount
            onPageReady: window.acceptCanvasPage(painterPage)
            onPresentationModeRequested: window.enterPresentationMode()
            onInvitationResponseRequested: accepted => CongregationLocalCanvasSession.respondToPendingInvitation(accepted, preferencesWindow.profileName)
            onClipboardImagePasteFailed: errorCode => window.showClipboardPasteFailure(errorCode)
            onImageDropSucceeded: {
                window.clipboardPasteFailureMessage = "";
                clipboardPasteFailureTimer.stop();
            }
            onImageDropFailed: errorCode => window.showClipboardPasteFailure(errorCode)
        }
    }

    LicenseViews.LicenseActivationPage {
        anchors.fill: parent
        visible: CongregationLicenseManager.enforcementEnabled && !window.licenseGranted
    }

    LV.AppCard {
        objectName: "licensePersistenceWarning"
        anchors.top: parent.top
        anchors.topMargin: LV.Theme.gap24
        anchors.horizontalCenter: parent.horizontalCenter
        z: 1000
        visible: CongregationLicenseManager.enforcementEnabled && window.licenseGranted && CongregationLicenseManager.resultCode === "secure_storage_unavailable" && !window.presentationMode
        title: qsTr("Congregation could not remember this license")
        subtitle: qsTr("The canvas is unlocked for this session. Enter the license again the next time Congregation starts.")
    }

    LV.AppCard {
        id: clipboardPasteFailureCard
        objectName: "clipboardPasteFailureCard"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: LV.Theme.gap24
        z: 1100
        visible: window.clipboardPasteFailureMessage.length > 0 && !window.presentationMode
        title: qsTr("Image not pasted")
        subtitle: window.clipboardPasteFailureMessage
        Accessible.name: title + ": " + subtitle
        Accessible.role: Accessible.AlertMessage
    }

    Timer {
        id: clipboardPasteFailureTimer
        interval: 5000
        repeat: false
        onTriggered: window.clipboardPasteFailureMessage = ""
    }

    LV.Label {
        anchors.centerIn: parent
        visible: window.licenseGranted && painterPageLoader.status !== Loader.Ready && !window.presentationMode
        text: qsTr("Loading canvas…")
    }

    LV.Modal {
        id: updateModal
        objectName: "congregationUpdateModal"
        open: false
        dismissOnBackground: !CongregationUpdateManager.busy
        showIcon: false
        title: CongregationUpdateManager.title
        description: CongregationUpdateManager.message + (CongregationUpdateManager.busy && CongregationUpdateManager.progress > 0 ? qsTr("\n\nProgress: %1%").arg(Math.round(CongregationUpdateManager.progress * 100)) : "")
        buttonCount: CongregationUpdateManager.canUpdate ? 2 : 1
        primaryText: CongregationUpdateManager.canCancel ? qsTr("Cancel") : CongregationUpdateManager.canUpdate ? qsTr("Update now") : qsTr("Close")
        secondaryText: qsTr("Not now")
        primaryEnabled: CongregationUpdateManager.canUpdate || CongregationUpdateManager.canCancel || !CongregationUpdateManager.busy

        onPrimaryClicked: {
            if (CongregationUpdateManager.canUpdate) {
                CongregationUpdateManager.updateNow();
            } else if (CongregationUpdateManager.canCancel) {
                CongregationUpdateManager.cancelUpdate();
            } else {
                updateModal.open = false;
            }
        }
        onSecondaryClicked: updateModal.open = false
        onCanceled: {
            if (CongregationUpdateManager.canCancel)
                CongregationUpdateManager.cancelUpdate();
        }
    }
}
