pragma ComponentBehavior: Bound

import QtQuick
import LVRS 1.0 as LV
import "../brush" as BrushUi
import "../painting" as Painting

Item {
    id: painterPage

    readonly property int spacingSmall: LV.Theme.gap8
    readonly property int spacingMedium: LV.Theme.gap12
    readonly property int spacingLarge: LV.Theme.gap16
    readonly property string viewId: "PainterCanvasPage"
    readonly property real maximumAntialiasingBrushHardness: 1
    readonly property color toolbarBackgroundColor: LV.Theme.panelBackground03
    readonly property int toolbarTopMargin: topChromeReservedHeight
    readonly property int fallbackNewCanvasWidth: 1024
    readonly property int fallbackNewCanvasHeight: 768
    readonly property int layerPanelTopMargin: toolbarTopMargin + canvasToolBar.height
    readonly property int layerRenameActivationWindowMs: 500
    readonly property int layerRenameRepeatActivationThreshold: 3
    readonly property bool dialogActive: canvasToolBar.dialogActive
    readonly property bool textEditingActive: drawingSurface.textEditingActive || painterPage.layerRenameActive
    readonly property bool presentationMode: drawingSurface.presentationMode
    readonly property bool participantUsesHostCanvas: Boolean(localCanvasSession && localCanvasSession.connected)
    readonly property bool ownsAuthoritativeCanvas: !participantUsesHostCanvas

    property int topChromeReservedHeight: 0
    property var vm: null
    property var localCanvasSession: null
    property var collaboratorProfiles: []
    property bool currentUserIsCanvasHost: true
    property var pendingInvitation: ({})
    property int pendingInvitationCount: 0
    property bool layerRenameActive: false
    property string layerRenameKey: ""
    property string layerRenameActivationKey: ""
    property int layerRenameActivationCount: 0
    property real layerRenameLastActivationMs: 0
    property real layerRenameEditorX: 0
    property real layerRenameEditorY: 0
    property real layerRenameEditorHeight: 20
    property bool recentCanvasTrackingEnabled: false
    property bool recentCanvasSaveInProgress: false
    property bool applyingRemoteCanvasSnapshot: false
    property bool applyingParticipantEditCommand: false
    property bool participantCanvasReady: false

    signal pageReady
    signal clipboardImagePasteFailed(string errorCode)
    signal imageDropSucceeded
    signal imageDropFailed(string errorCode)
    signal presentationModeRequested
    signal invitationResponseRequested(bool accepted)
    signal collaboratorInvitationRequested
    signal collaboratorRemovalRequested(var profile, int index)

    function refreshViewModel() {
        painterPage.vm = LV.ViewModels.getForView(painterPage.viewId);
    }

    function requestAddCollaborator() {
        painterPage.collaboratorInvitationRequested();
    }

    function requestRemoveCollaborator(profile, index) {
        painterPage.collaboratorRemovalRequested(profile, index);
    }

    function bindViewModel() {
        if (!LV.ViewModels.bindView(painterPage.viewId, "CanvasDocument", true)) {
            console.warn(LV.ViewModels.lastError);
        }
        painterPage.refreshViewModel();
    }

    function updateDocumentProperty(propertyName, value) {
        if (!LV.ViewModels.updateProperty(painterPage.viewId, propertyName, value)) {
            console.warn(LV.ViewModels.lastError);
        }
    }

    Component.onCompleted: {
        painterPage.bindViewModel();
        Qt.callLater(function () {
            pageReady();
            Qt.callLater(function () {
                painterPage.recentCanvasTrackingEnabled = true;
            });
        });
    }

    Component.onDestruction: {
        LV.ViewModels.unbindView(painterPage.viewId);
    }

    Connections {
        target: LV.ViewModels

        function onViewsChanged() {
            painterPage.refreshViewModel();
        }

        function onBindingsChanged() {
            painterPage.refreshViewModel();
        }
    }

    Timer {
        id: recentCanvasSaveTimer

        interval: 1200
        repeat: false
        onTriggered: painterPage.saveRecentCanvasNow()
    }

    Timer {
        id: localCanvasPublishTimer

        interval: 350
        repeat: false
        onTriggered: painterPage.publishLocalCanvasSnapshotNow()
    }

    Timer {
        id: participantCanvasPublishTimer

        interval: 0
        repeat: false
        onTriggered: painterPage.publishParticipantCanvasSnapshotNow()
    }

    function handleCanvasSessionChanged() {
        painterPage.scheduleRecentCanvasSave();
        painterPage.scheduleLocalCanvasPublish();
    }

    function scheduleLocalCanvasPublish() {
        if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.hosting || painterPage.applyingRemoteCanvasSnapshot || painterPage.applyingParticipantEditCommand) {
            return;
        }
        localCanvasPublishTimer.restart();
    }

    function publishLocalCanvasSnapshotNow() {
        if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.hosting || painterPage.applyingRemoteCanvasSnapshot || painterPage.applyingParticipantEditCommand || !drawingSurface.canvasItemReady) {
            return false;
        }
        if (drawingSurface.textEditingActive || drawingSurface.shapeDraggingActive || drawingSurface.drawableObjectTransformActive) {
            localCanvasPublishTimer.restart();
            return false;
        }
        participantCanvasPublishTimer.stop();
        return painterPage.localCanvasSession.publishSnapshot(drawingSurface.canvasSessionSnapshot());
    }

    function scheduleParticipantCanvasPublish() {
        if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.hosting || painterPage.applyingRemoteCanvasSnapshot) {
            return false;
        }
        participantCanvasPublishTimer.restart();
        return true;
    }

    function publishParticipantCanvasSnapshotNow() {
        if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.hosting || painterPage.applyingRemoteCanvasSnapshot || painterPage.applyingParticipantEditCommand || !drawingSurface.canvasItemReady) {
            return false;
        }
        const snapshot = drawingSurface.canvasSessionSnapshot();
        if (!snapshot || snapshot.length === 0) {
            return false;
        }
        return painterPage.localCanvasSession.publishSnapshot(snapshot);
    }

    function applyRemoteCanvasSnapshot(snapshot) {
        localCanvasPublishTimer.stop();
        painterPage.participantCanvasReady = false;
        painterPage.applyingRemoteCanvasSnapshot = true;
        const applied = drawingSurface.applyCanvasSessionSnapshot(snapshot);
        if (!applied) {
            painterPage.applyingRemoteCanvasSnapshot = false;
        }
        return applied;
    }

    function finishRemoteCanvasRestore(success) {
        if (!painterPage.applyingRemoteCanvasSnapshot) {
            return;
        }
        painterPage.applyingRemoteCanvasSnapshot = false;
        painterPage.participantCanvasReady = Boolean(success && painterPage.participantUsesHostCanvas);
    }

    function submitParticipantEditCommand(command) {
        if (!painterPage.participantUsesHostCanvas || !painterPage.participantCanvasReady || painterPage.applyingRemoteCanvasSnapshot || !painterPage.localCanvasSession) {
            return false;
        }
        return painterPage.localCanvasSession.submitEditCommand(command);
    }

    Connections {
        target: painterPage.localCanvasSession

        function onSnapshotRequested() {
            painterPage.publishLocalCanvasSnapshotNow();
        }

        function onStateChanged() {
            if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.connected) {
                painterPage.participantCanvasReady = false;
            }
        }

        function onSnapshotReceived(snapshot, revision, originPeerId) {
            painterPage.applyRemoteCanvasSnapshot(snapshot);
        }

        function onEditCommandReceived(command, originPeerId) {
            if (!painterPage.localCanvasSession || !painterPage.localCanvasSession.hosting || !drawingSurface.canvasItemReady) {
                return;
            }
            localCanvasPublishTimer.stop();
            painterPage.applyingParticipantEditCommand = true;
            const applied = drawingSurface.applyHostEditCommand(command);
            painterPage.applyingParticipantEditCommand = false;
            if (applied) {
                painterPage.scheduleRecentCanvasSave();
                painterPage.scheduleParticipantCanvasPublish();
            }
        }
    }

    function scheduleRecentCanvasSave() {
        if (!painterPage.ownsAuthoritativeCanvas || !painterPage.recentCanvasTrackingEnabled || painterPage.recentCanvasSaveInProgress) {
            return;
        }
        recentCanvasSaveTimer.restart();
    }

    function saveRecentCanvasNow() {
        if (!painterPage.ownsAuthoritativeCanvas || !painterPage.recentCanvasTrackingEnabled || painterPage.recentCanvasSaveInProgress || !drawingSurface.canvasItemReady) {
            return false;
        }
        if (drawingSurface.textEditingActive || drawingSurface.shapeDraggingActive || drawingSurface.drawableObjectTransformActive) {
            recentCanvasSaveTimer.restart();
            return false;
        }

        const storageUrl = CongregationApplicationPreferences.recentCanvasStorageUrl;
        if (!storageUrl || storageUrl.toString().length === 0) {
            return false;
        }

        painterPage.recentCanvasSaveInProgress = true;
        const saved = drawingSurface.saveRecentCanvas(storageUrl);
        painterPage.recentCanvasSaveInProgress = false;
        if (saved) {
            CongregationApplicationPreferences.recordRecentCanvas(storageUrl);
        } else {
            console.warn("Congregation could not update the internal recent canvas container.");
        }
        return saved;
    }

    function flushRecentCanvasSave() {
        if (!painterPage.ownsAuthoritativeCanvas) {
            recentCanvasSaveTimer.stop();
            return true;
        }
        const activeEdit = drawingSurface.textEditingActive || drawingSurface.shapeDraggingActive || drawingSurface.drawableObjectTransformActive;
        if (!recentCanvasSaveTimer.running && !activeEdit) {
            return true;
        }

        recentCanvasSaveTimer.stop();
        drawingSurface.commitActiveText();
        drawingSurface.commitActiveShape();
        drawingSurface.commitDrawableObjectTransform();
        return painterPage.saveRecentCanvasNow();
    }

    Binding {
        target: CongregationTemporaryCameraInput
        property: "enabled"
        value: drawingSurface.toolShortcutsEnabled && !drawingSurface.textEditingActive
        restoreMode: Binding.RestoreBindingOrValue
    }

    Connections {
        target: CongregationTemporaryCameraInput

        function onModeChanged() {
            drawingSurface.setTemporaryCameraMode(CongregationTemporaryCameraInput.mode);
        }
    }

    function newCanvas(canvasWidth, canvasHeight, infiniteCanvas) {
        drawingSurface.newCanvas(canvasWidth, canvasHeight, infiniteCanvas);
        painterPage.scheduleRecentCanvasSave();
    }

    function clearCanvas() {
        drawingSurface.clearCanvas();
        painterPage.scheduleRecentCanvasSave();
    }

    function setBrushColor(colorValue) {
        painterPage.updateDocumentProperty("brushColor", colorValue);
    }

    function adjustBrush(delta) {
        const baseSize = painterPage.vm ? painterPage.vm.brushSize : 2;
        painterPage.updateDocumentProperty("brushSize", Math.max(1, Math.min(48, baseSize + delta)));
    }

    function setBrushSize(size) {
        painterPage.updateDocumentProperty("brushSize", size);
    }

    function setBrushProperty(propertyName, value) {
        painterPage.updateDocumentProperty(propertyName, value);
    }

    function setToolMode(tool) {
        painterPage.updateDocumentProperty("toolMode", tool);
    }

    function setShapeKind(shapeKind) {
        painterPage.updateDocumentProperty("shapeKind", shapeKind);
    }

    function saveCanvasAs(fileUrl) {
        const saved = drawingSurface.saveToFile(fileUrl);
        if (saved) {
            painterPage.scheduleRecentCanvasSave();
        }
        return saved;
    }

    function openRaster(fileUrl) {
        const opened = drawingSurface.openRaster(fileUrl);
        if (opened) {
            painterPage.scheduleRecentCanvasSave();
        }
        return opened;
    }

    function openRecentCanvas(fileUrl) {
        if (painterPage.participantUsesHostCanvas) {
            return false;
        }
        const trackingWasEnabled = painterPage.recentCanvasTrackingEnabled;
        painterPage.recentCanvasTrackingEnabled = false;
        recentCanvasSaveTimer.stop();
        const opened = drawingSurface.openRecentCanvas(fileUrl);
        painterPage.recentCanvasTrackingEnabled = trackingWasEnabled;
        return opened;
    }

    function pasteClipboardImage() {
        const pasted = drawingSurface.pasteClipboardImage();
        if (pasted) {
            painterPage.setToolMode("move");
            Qt.callLater(painterPage.syncLayerHierarchySelection);
        } else {
            clipboardImagePasteFailed(drawingSurface.clipboardImagePasteErrorCode || "decode-failed");
        }
        return pasted;
    }

    function openNewCanvasDialog() {
        canvasToolBar.openNewCanvasDialog();
    }

    function openFileDialog() {
        canvasToolBar.openFileDialog();
    }

    function openSaveDialog() {
        canvasToolBar.openSaveDialog();
    }

    function undoActiveRasterSurface() {
        if (painterPage.participantUsesHostCanvas) {
            return painterPage.submitParticipantEditCommand({
                type: "undo",
                layerKey: drawingSurface.currentLayerKey()
            });
        }
        const rasterSurface = drawingSurface.activeRasterSurface();
        if (rasterSurface) {
            rasterSurface.undo();
            painterPage.scheduleRecentCanvasSave();
            return true;
        }
        return false;
    }

    function redoActiveRasterSurface() {
        if (painterPage.participantUsesHostCanvas) {
            return painterPage.submitParticipantEditCommand({
                type: "redo",
                layerKey: drawingSurface.currentLayerKey()
            });
        }
        const rasterSurface = drawingSurface.activeRasterSurface();
        if (rasterSurface) {
            rasterSurface.redo();
            painterPage.scheduleRecentCanvasSave();
            return true;
        }
        return false;
    }

    function addLayer() {
        drawingSurface.addEmptyLayer();
        Qt.callLater(painterPage.syncLayerHierarchySelection);
    }

    function deleteCurrentLayer() {
        drawingSurface.deleteCurrentLayer();
        Qt.callLater(painterPage.syncLayerHierarchySelection);
    }

    function canDeleteCurrentLayer() {
        return drawingSurface.canDeleteCurrentLayer();
    }

    function selectShapeTool(shapeKind) {
        painterPage.setShapeKind(shapeKind);
        painterPage.setToolMode("shape");
    }

    function fitCanvasToWindow() {
        drawingSurface.fitCanvasZoomToCurrentCanvas();
    }

    function resetCanvasView() {
        drawingSurface.resetCanvasPan();
        drawingSurface.fitCanvasZoomToCurrentCanvas();
    }

    function enterPresentationMode() {
        if (painterPage.layerRenameActive) {
            painterPage.commitLayerRename();
        }
        drawingSurface.enterPresentationMode();
    }

    function refreshPresentationMode() {
        drawingSurface.refreshPresentationMode();
    }

    function exitPresentationMode() {
        drawingSurface.exitPresentationMode();
    }

    function zoomCanvasFromWheel(angleDeltaY, pixelDeltaY) {
        drawingSurface.zoomCanvasFromWheel(angleDeltaY, pixelDeltaY);
    }

    function syncLayerHierarchySelection() {
        if (!layerHierarchyPanel.visible) {
            return;
        }
        layerHierarchyPanel.activateListItemByKey(drawingSurface.currentLayerKey());
    }

    function resetLayerRenameActivation() {
        painterPage.layerRenameActivationKey = "";
        painterPage.layerRenameActivationCount = 0;
        painterPage.layerRenameLastActivationMs = 0;
    }

    function handleLayerHierarchyItemActivated(item) {
        if (!item || item.itemKey === undefined) {
            return;
        }

        const itemKey = String(item.itemKey);
        drawingSurface.activateLayerByKey(itemKey);
        if (!drawingSurface.canRenameLayerByKey(itemKey)) {
            painterPage.resetLayerRenameActivation();
            return;
        }

        const now = Date.now();
        const sameActivationRun = painterPage.layerRenameActivationKey === itemKey && now - painterPage.layerRenameLastActivationMs <= painterPage.layerRenameActivationWindowMs;
        painterPage.layerRenameActivationKey = itemKey;
        painterPage.layerRenameActivationCount = sameActivationRun ? painterPage.layerRenameActivationCount + 1 : 1;
        painterPage.layerRenameLastActivationMs = now;

        if (painterPage.layerRenameActivationCount >= painterPage.layerRenameRepeatActivationThreshold) {
            painterPage.resetLayerRenameActivation();
            painterPage.beginLayerRename(item);
        }
    }

    function beginLayerRename(item) {
        if (!item || item.itemKey === undefined) {
            return false;
        }

        const itemKey = String(item.itemKey);
        if (!drawingSurface.canRenameLayerByKey(itemKey)) {
            return false;
        }

        const panelPoint = item.mapToItem(layerHierarchyPanel, 0, 0);
        const labelX = Number(item.leftPadding || 0) + Number(item.iconSize || 16) + Number(item.leadingSpacing || 4);
        painterPage.layerRenameKey = itemKey;
        painterPage.layerRenameEditorX = Math.max(painterPage.spacingSmall, labelX);
        painterPage.layerRenameEditorY = Math.max(0, panelPoint.y);
        painterPage.layerRenameEditorHeight = Math.max(20, item.height || 20);
        layerRenameField.text = drawingSurface.layerNameByKey(itemKey);
        painterPage.layerRenameActive = true;
        Qt.callLater(function () {
            layerRenameField.forceActiveFocus();
            layerRenameField.selectAll();
        });
        return true;
    }

    function commitLayerRename() {
        if (!painterPage.layerRenameActive) {
            return false;
        }

        const itemKey = painterPage.layerRenameKey;
        const nextName = layerRenameField.text;
        painterPage.layerRenameActive = false;
        painterPage.layerRenameKey = "";
        const renamed = drawingSurface.renameLayerByKey(itemKey, nextName);
        Qt.callLater(painterPage.syncLayerHierarchySelection);
        return renamed;
    }

    function cancelLayerRename() {
        painterPage.layerRenameActive = false;
        painterPage.layerRenameKey = "";
        painterPage.resetLayerRenameActivation();
    }

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: LV.Theme.window

            LV.Hierarchy {
                id: layerHierarchyPanel
                objectName: "layerHierarchyPanel"
                visible: !painterPage.presentationMode
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: painterPage.layerPanelTopMargin
                anchors.leftMargin: 0
                anchors.bottomMargin: 0
                z: 4
                editable: true
                model: drawingSurface.layerHierarchyRows
                footerVisible: true
                footerButton1: ({
                        type: "icon",
                        iconName: "add",
                        enabled: true
                    })
                footerButton2: ({
                        type: "icon",
                        iconName: "remove",
                        enabled: drawingSurface.canDeleteCurrentLayer()
                    })
                footerButton3: ({
                        type: "icon",
                        visible: false,
                        enabled: false
                    })

                onListItemActivated: function (item) {
                    painterPage.handleLayerHierarchyItemActivated(item);
                }

                onListItemMoved: function () {
                    drawingSurface.applyLayerHierarchyOrder(layerHierarchyPanel.model);
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }

                onFooterButtonTriggered: function (index) {
                    if (index === 0) {
                        drawingSurface.addEmptyLayer();
                        Qt.callLater(painterPage.syncLayerHierarchySelection);
                    } else if (index === 1) {
                        drawingSurface.deleteCurrentLayer();
                        Qt.callLater(painterPage.syncLayerHierarchySelection);
                    }
                }
            }

            Painting.DrawingSurface {
                id: drawingSurface
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.left: painterPage.presentationMode ? parent.left : layerHierarchyPanel.right
                anchors.leftMargin: 0
                workspaceColor: LV.Theme.window
                documentViewModel: painterPage.vm
                viewId: painterPage.viewId
                brushColor: painterPage.vm ? painterPage.vm.brushColor : "#1a1a1a"
                brushSize: painterPage.vm ? painterPage.vm.brushSize : 2
                brushFlow: painterPage.vm ? painterPage.vm.brushFlow : 1
                brushOpacity: painterPage.vm ? painterPage.vm.brushOpacity : 1
                brushHardness: painterPage.vm ? painterPage.vm.brushHardness : painterPage.maximumAntialiasingBrushHardness
                brushSpacing: painterPage.vm ? painterPage.vm.brushSpacing : 0
                brushSpacingRatio: painterPage.vm ? painterPage.vm.brushSpacingRatio : 0
                pressureCurveMinimum: painterPage.vm ? painterPage.vm.pressureCurveMinimum : 0
                pressureCurveCenter: painterPage.vm ? painterPage.vm.pressureCurveCenter : 0.5
                pressureCurveMaximum: painterPage.vm ? painterPage.vm.pressureCurveMaximum : 1
                brushPressureControlsOpacity: painterPage.vm ? painterPage.vm.brushPressureControlsOpacity : true
                stabilizerStrength: painterPage.vm ? painterPage.vm.stabilizerStrength : 0
                toolMode: painterPage.vm ? painterPage.vm.toolMode : "brush"
                hostCanvasInputClient: painterPage.participantUsesHostCanvas
                toolShortcutsEnabled: !painterPage.layerRenameActive && !canvasToolBar.dialogActive && !painterPage.presentationMode
                shapeKind: painterPage.vm ? painterPage.vm.shapeKind : "rectangle"
                textToolAccentColor: LV.Theme.primary
                textToolFramePadding: painterPage.spacingSmall
                canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : 1
                canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : 1
                onToolShortcutRequested: tool => painterPage.setToolMode(tool)
                onSessionChanged: painterPage.handleCanvasSessionChanged()
                onHostEditCommandRequested: command => painterPage.submitParticipantEditCommand(command)
                onSessionRestoreFinished: success => painterPage.finishRemoteCanvasRestore(success)
                onImageDropSucceeded: {
                    painterPage.setToolMode("move");
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                    painterPage.imageDropSucceeded();
                }
                onImageDropFailed: errorCode => painterPage.imageDropFailed(errorCode)
            }

            Connections {
                target: drawingSurface

                function onLayerHierarchyRowsChanged() {
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }

                function onSelectedDrawableObjectIdChanged() {
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }
            }

            Rectangle {
                id: layerRenameEditorFrame
                parent: layerHierarchyPanel
                visible: painterPage.layerRenameActive
                x: painterPage.layerRenameEditorX
                y: painterPage.layerRenameEditorY
                width: Math.max(96, layerHierarchyPanel.width - x - painterPage.spacingSmall)
                height: painterPage.layerRenameEditorHeight
                z: 40
                radius: 3
                color: LV.Theme.panelBackground12
                border.width: 1
                border.color: LV.Theme.primary

                TextInput {
                    id: layerRenameField
                    anchors.fill: parent
                    anchors.leftMargin: painterPage.spacingSmall
                    anchors.rightMargin: painterPage.spacingSmall
                    color: LV.Theme.bodyColor
                    selectionColor: LV.Theme.primary
                    selectedTextColor: "#ffffff"
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                    clip: true

                    onAccepted: painterPage.commitLayerRename()
                    onActiveFocusChanged: {
                        if (!activeFocus && painterPage.layerRenameActive) {
                            painterPage.commitLayerRename();
                        }
                    }

                    Keys.onEscapePressed: function (event) {
                        event.accepted = true;
                        painterPage.cancelLayerRename();
                    }
                }
            }
        }

        Rectangle {
            id: toolbarChromeBackground
            visible: !painterPage.presentationMode
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: painterPage.toolbarTopMargin + canvasToolBar.height
            color: painterPage.toolbarBackgroundColor
            z: 9
        }

        BrushUi.CanvasToolBar {
            id: canvasToolBar
            visible: !painterPage.presentationMode
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: painterPage.toolbarTopMargin
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            z: 10
            backgroundColor: painterPage.toolbarBackgroundColor
            brushSize: painterPage.vm ? painterPage.vm.brushSize : 2
            brushFlow: painterPage.vm ? painterPage.vm.brushFlow : 1
            brushOpacity: painterPage.vm ? painterPage.vm.brushOpacity : 1
            brushHardness: painterPage.vm ? painterPage.vm.brushHardness : painterPage.maximumAntialiasingBrushHardness
            brushSpacing: painterPage.vm ? painterPage.vm.brushSpacing : 0
            brushSpacingRatio: painterPage.vm ? painterPage.vm.brushSpacingRatio : 0
            pressureCurveMinimum: painterPage.vm ? painterPage.vm.pressureCurveMinimum : 0
            pressureCurveCenter: painterPage.vm ? painterPage.vm.pressureCurveCenter : 0.5
            pressureCurveMaximum: painterPage.vm ? painterPage.vm.pressureCurveMaximum : 1
            brushPressureControlsOpacity: painterPage.vm ? painterPage.vm.brushPressureControlsOpacity : true
            stabilizerStrength: painterPage.vm ? painterPage.vm.stabilizerStrength : 0
            currentColor: painterPage.vm ? painterPage.vm.brushColor : "#1a1a1a"
            currentTool: painterPage.vm ? painterPage.vm.toolMode : "brush"
            currentShape: painterPage.vm ? painterPage.vm.shapeKind : "rectangle"
            canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : painterPage.fallbackNewCanvasWidth
            canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : painterPage.fallbackNewCanvasHeight
            pendingInvitation: painterPage.pendingInvitation
            pendingInvitationCount: painterPage.pendingInvitationCount
            onNewCanvasRequested: (canvasWidth, canvasHeight, infiniteCanvas) => painterPage.newCanvas(canvasWidth, canvasHeight, infiniteCanvas)
            onClearCanvasRequested: painterPage.clearCanvas()
            onBrushSizeChangeRequested: size => painterPage.setBrushSize(size)
            onBrushPropertyChangeRequested: (propertyName, value) => painterPage.setBrushProperty(propertyName, value)
            onBrushPressureControlsOpacityChangeRequested: enabled => painterPage.setBrushProperty("brushPressureControlsOpacity", enabled)
            onColorPicked: swatchColor => painterPage.setBrushColor(swatchColor)
            onToolSelected: tool => painterPage.setToolMode(tool)
            onShapeSelected: shapeKind => painterPage.setShapeKind(shapeKind)
            onPresentationModeRequested: painterPage.presentationModeRequested()
            onInvitationResponseRequested: accepted => painterPage.invitationResponseRequested(accepted)
            onSaveRequested: fileUrl => painterPage.saveCanvasAs(fileUrl)
            onOpenRequested: fileUrl => painterPage.openRaster(fileUrl)
        }

        PresentationLaserPointer {
            id: presentationLaserPointer
            anchors.fill: parent
            visible: painterPage.presentationMode
            z: 20
            onWheelZoomRequested: (angleDeltaY, pixelDeltaY) => painterPage.zoomCanvasFromWheel(angleDeltaY, pixelDeltaY)
        }
    }
}
