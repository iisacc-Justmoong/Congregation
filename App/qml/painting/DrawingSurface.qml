pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes as Shapes
import Congregation 1.0

Rectangle {
    id: surface
    color: workspaceColor
    focus: true
    clip: true
    enabled: !surface.presentationMode

    property color workspaceColor: "#1f1f20"
    property color canvasColor: "white"
    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property real brushFlow: 1
    property real brushOpacity: 1
    readonly property real maximumAntialiasingBrushHardness: 1
    property real brushHardness: maximumAntialiasingBrushHardness
    property real brushSpacing: 0
    property real brushSpacingRatio: 0
    property real pressureCurveMinimum: 0
    property real pressureCurveCenter: 0.5
    property real pressureCurveMaximum: 1
    property bool brushPressureControlsOpacity: true
    property real stabilizerStrength: 0
    property var documentViewModel: null
    property string viewId: ""
    property int canvasWidth: 1
    property int canvasHeight: 1
    property string toolMode: "brush"
    property bool canvasItemReady: false
    property bool canvasSizeCreated: false
    property string clipboardImagePasteErrorCode: ""
    property string temporaryCameraMode: ""
    property bool textEditingActive: false
    property bool toolShortcutsEnabled: true
    property bool presentationMode: false
    property real presentationPreviousCanvasZoomScale: 1
    property real presentationPreviousCanvasPanOffsetX: 0
    property real presentationPreviousCanvasPanOffsetY: 0
    property real presentationFittedCanvasZoomScale: 1
    property string shapeKind: "rectangle"
    property bool shapeDraggingActive: false
    property bool shapeAspectLocked: false
    property real shapeStartX: 0
    property real shapeStartY: 0
    property real shapeCurrentX: 0
    property real shapeCurrentY: 0
    property real canvasZoomScale: 1
    property bool zoomDraggingActive: false
    property real zoomDragStartX: 0
    property real zoomDragStartScale: 1
    property real canvasPanOffsetX: 0
    property real canvasPanOffsetY: 0
    property bool infiniteCanvasExpansionActive: false
    property bool panDraggingActive: false
    property real panDragStartX: 0
    property real panDragStartY: 0
    property real panDragStartOffsetX: 0
    property real panDragStartOffsetY: 0
    property color textToolAccentColor: "#A571E6"
    property int textToolFramePadding: 8
    readonly property real workspaceCanvasHorizontalInsetRatio: 0.09
    readonly property real workspaceCanvasTopInsetRatio: 0.12
    readonly property real workspaceCanvasBottomInsetRatio: 0.10
    readonly property int workspaceCanvasMinimumInset: 24
    readonly property int workspaceCanvasHorizontalInset: surface.presentationMode ? 0 : Math.max(workspaceCanvasMinimumInset, Math.round(width * workspaceCanvasHorizontalInsetRatio))
    readonly property int workspaceCanvasTopInset: surface.presentationMode ? 0 : Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasTopInsetRatio))
    readonly property int workspaceCanvasBottomInset: surface.presentationMode ? 0 : Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasBottomInsetRatio))
    readonly property int workspaceCanvasWidth: Math.max(1, Math.round(width) - workspaceCanvasHorizontalInset * 2)
    readonly property int workspaceCanvasHeight: Math.max(1, Math.round(height) - workspaceCanvasTopInset - workspaceCanvasBottomInset)
    readonly property int minimumCanvasDimension: 1
    readonly property int maximumCanvasDimension: 8192
    readonly property int defaultInfiniteCanvasChunkSize: 256
    readonly property int minimumTextToolFontPixelSize: 8
    readonly property int textToolFontPixelSize: Math.max(surface.minimumTextToolFontPixelSize, Math.round(surface.brushSize))
    readonly property int textToolMinimumWidth: 96
    readonly property int shapeToolMinimumDragDistance: 2
    readonly property int speechBubbleTailMinimumHeight: 4
    readonly property real speechBubbleTailHeightRatio: 0.24
    readonly property real speechBubbleTailMaximumHeightRatio: 0.35
    readonly property real speechBubbleTailLeftBaseXRatio: 0.26
    readonly property real speechBubbleTailTipXRatio: 0.18
    readonly property real speechBubbleTailRightBaseXRatio: 0.44
    readonly property real ellipseBubbleTailLeftAngle: 2.15
    readonly property real ellipseBubbleTailRightAngle: 1.70
    readonly property int ellipseBubbleArcSegmentCount: 32
    readonly property int drawableObjectMinimumDimension: 8
    readonly property int drawableObjectHandleSize: 10
    readonly property int drawableObjectHandleHitSize: 32
    readonly property real imageInsertionMaximumCanvasRatio: 0.8
    readonly property int imageInsertionDuplicateOffset: 16
    readonly property bool imageDropActive: canvasImageDropArea.containsDrag
    readonly property real defaultCanvasZoomScale: 1
    readonly property real minimumCanvasZoomScale: 0.01
    readonly property real maximumCanvasZoomScale: 8
    readonly property real presentationMinimumZoomMultiplier: 0.125
    readonly property real presentationMaximumZoomMultiplier: 8
    readonly property real zoomDragPixelsPerDoubling: 180
    readonly property bool brushCursorToolActive: {
        const mode = surface.effectiveToolMode();
        return (mode === "brush" || mode === "eraser") && surface.hasActiveRasterSurface();
    }
    readonly property real brushCursorScreenDiameter: surface.brushSize * surface.canvasZoomScale
    readonly property int brushCursorOuterStrokeWidth: 3
    readonly property int brushCursorInnerStrokeWidth: 1
    readonly property real brushCursorEffectiveOuterStrokeWidth: Math.min(surface.brushCursorOuterStrokeWidth, surface.brushCursorScreenDiameter / 2)
    readonly property real brushCursorEffectiveInnerStrokeWidth: Math.min(surface.brushCursorInnerStrokeWidth, surface.brushCursorScreenDiameter / 6)
    readonly property bool brushCursorUsesSystemFallback: surface.brushCursorToolActive && surface.brushCursorScreenDiameter < 1
    readonly property var drawableObjectHandles: [
        {
            mode: "resize-nw",
            xRatio: 0,
            yRatio: 0
        },
        {
            mode: "resize-ne",
            xRatio: 1,
            yRatio: 0
        },
        {
            mode: "resize-se",
            xRatio: 1,
            yRatio: 1
        },
        {
            mode: "resize-sw",
            xRatio: 0,
            yRatio: 1
        },
        {
            mode: "resize-n",
            xRatio: 0.5,
            yRatio: 0
        },
        {
            mode: "resize-e",
            xRatio: 1,
            yRatio: 0.5
        },
        {
            mode: "resize-s",
            xRatio: 0.5,
            yRatio: 1
        },
        {
            mode: "resize-w",
            xRatio: 0,
            yRatio: 0.5
        }
    ]
    property var drawableObjects: []
    property int nextDrawableObjectId: 1
    property int nextEmptyLayerNumber: 1
    property int selectedDrawableObjectId: -1
    property var layerHierarchyRows: []
    readonly property int layerHierarchyThumbnailSize: 32
    readonly property int layerHierarchyThumbnailRefreshDelayMs: 1000
    readonly property int brushLivePreviewFrameIntervalMs: 16
    readonly property string transparencyGridTileSource: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAJElEQVR42mP49OnTf3z4ypUreDHDqAHDwgBCCghZMGrAsDAAAIlzqC6skcNAAAAAAElFTkSuQmCC"
    property bool layerHierarchyRowsRebuildScheduled: false
    property bool backgroundLayerThumbnailRefreshPending: false
    property bool backgroundLayerPresent: true
    property var pendingRasterLayerThumbnailRefreshes: ({})
    property string backgroundLayerThumbnailSource: ""
    property var drawableObjectThumbnailSources: ({})
    property var rasterLayerItems: ({})
    property var rasterLayerSnapshotSources: ({})
    property var rasterLayerThumbnailSources: ({})
    property bool persistRasterLayerSnapshots: true
    property bool recentCanvasRestoreInProgress: false
    property int recentCanvasRestoreGeneration: 0
    property bool hostCanvasInputClient: false
    property bool applyingHostEditCommand: false
    property bool drawableObjectTransformActive: false
    property string drawableObjectTransformMode: ""
    property string drawableObjectHoverHandleMode: ""
    property real drawableObjectTransformStartX: 0
    property real drawableObjectTransformStartY: 0
    property var drawableObjectTransformOriginal: null

    signal toolShortcutRequested(string tool)
    signal sessionChanged
    signal sessionRestoreFinished(bool success)
    signal imageDropSucceeded
    signal imageDropFailed(string errorCode)
    signal hostEditCommandRequested(var command)

    onDrawableObjectsChanged: {
        if (!surface.recentCanvasRestoreInProgress) {
            sessionChanged();
        }
    }
    onBackgroundLayerPresentChanged: {
        if (!surface.recentCanvasRestoreInProgress) {
            sessionChanged();
        }
    }

    onSelectedDrawableObjectIdChanged: {
        surface.drawableObjectHoverHandleMode = "";
        rebuildLayerHierarchyRows();
    }

    function effectiveToolMode() {
        return surface.temporaryCameraMode !== "" && !surface.textEditingActive ? surface.temporaryCameraMode : surface.toolMode;
    }

    function requestHostEditCommand(command) {
        if (!surface.hostCanvasInputClient || surface.applyingHostEditCommand || !command) {
            return false;
        }
        surface.hostEditCommandRequested(command);
        return true;
    }

    function rasterSurfaceForLayerKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        if (normalizedKey === "raster-canvas") {
            return surface.backgroundLayerPresent ? canvasSurface : null;
        }
        const drawableObject = drawableObjectForLayerKey(normalizedKey);
        if (!drawableObject || drawableObject.type !== "layer") {
            return null;
        }
        return rasterLayerItemById(drawableObject.id);
    }

    function remoteStrokeStyle() {
        return {
            tool: surface.effectiveToolMode(),
            color: surface.brushColor.toString(),
            size: surface.brushSize,
            flow: surface.brushFlow,
            opacity: surface.brushOpacity,
            hardness: surface.brushHardness,
            spacing: surface.brushSpacing,
            spacingRatio: surface.brushSpacingRatio,
            pressureMinimum: surface.pressureCurveMinimum,
            pressureCenter: surface.pressureCurveCenter,
            pressureMaximum: surface.pressureCurveMaximum,
            pressureOpacity: surface.brushPressureControlsOpacity,
            stabilizer: surface.stabilizerStrength
        };
    }

    function requestRemoteStroke(layerKey, points, pressureSensitive) {
        const style = remoteStrokeStyle();
        return requestHostEditCommand({
            type: "stroke",
            layerKey: layerKey,
            tool: style.tool,
            color: style.color,
            size: style.size,
            flow: style.flow,
            opacity: style.opacity,
            hardness: style.hardness,
            spacing: style.spacing,
            spacingRatio: style.spacingRatio,
            pressureMinimum: style.pressureMinimum,
            pressureCenter: style.pressureCenter,
            pressureMaximum: style.pressureMaximum,
            pressureOpacity: style.pressureOpacity,
            stabilizer: style.stabilizer,
            pressureSensitive: Boolean(pressureSensitive),
            points: points
        });
    }

    function applyLayerKeyOrder(layerKeys) {
        const keys = Array.isArray(layerKeys) ? layerKeys : [];
        const rowByKey = {};
        for (let index = 0; index < surface.layerHierarchyRows.length; ++index) {
            const row = surface.layerHierarchyRows[index];
            rowByKey[String(row.key || "")] = row;
        }
        const rows = [];
        for (let index = 0; index < keys.length; ++index) {
            const row = rowByKey[String(keys[index])];
            if (!row) {
                return false;
            }
            rows.push(row);
        }
        return applyLayerHierarchyOrder(rows);
    }

    function applyHostEditCommand(command) {
        if (!command || surface.hostCanvasInputClient || surface.applyingHostEditCommand) {
            return false;
        }

        surface.applyingHostEditCommand = true;
        let applied = false;
        try {
            const type = String(command.type || "");
            if (type === "stroke") {
                const rasterSurface = rasterSurfaceForLayerKey(command.layerKey);
                if (rasterSurface) {
                    applied = rasterSurface.applyRemoteStroke(command, command.points || [], Boolean(command.pressureSensitive));
                }
            } else if (type === "fill") {
                const rasterSurface = rasterSurfaceForLayerKey(command.layerKey);
                applied = Boolean(rasterSurface && rasterSurface.fillAt(Number(command.x), Number(command.y), String(command.color)));
            } else if (type === "add-text") {
                appendDrawableObject({
                    id: surface.nextDrawableObjectId++,
                    type: "text",
                    x: Number(command.x),
                    y: Number(command.y),
                    width: Number(command.width),
                    height: Number(command.height),
                    text: String(command.text),
                    fontPixelSize: Number(command.fontSize),
                    color: String(command.color)
                });
                applied = true;
            } else if (type === "add-shape") {
                appendDrawableObject({
                    id: surface.nextDrawableObjectId++,
                    type: "shape",
                    x: Number(command.x),
                    y: Number(command.y),
                    width: Number(command.width),
                    height: Number(command.height),
                    shapeKind: String(command.shape),
                    color: String(command.color)
                });
                applied = true;
            } else if (type === "transform-object") {
                const objectId = Number(command.objectId);
                const currentObject = drawableObjectForLayerKey(layerKeyForDrawableObjectId(objectId));
                if (drawableObjectIsTransformable(currentObject)) {
                    const transformedObject = cloneDrawableObject(currentObject);
                    transformedObject.x = Number(command.x);
                    transformedObject.y = Number(command.y);
                    transformedObject.width = Number(command.width);
                    transformedObject.height = Number(command.height);
                    applied = replaceDrawableObjectById(objectId, transformedObject);
                    if (applied) {
                        refreshDrawableObjectThumbnailSource(transformedObject);
                        rebuildLayerHierarchyRows();
                    }
                }
            } else if (type === "new-canvas") {
                newCanvas(Number(command.width), Number(command.height), Boolean(command.infinite));
                applied = true;
            } else if (type === "clear-canvas") {
                clearCanvas();
                applied = true;
            } else if (type === "ensure-region") {
                if (canvasSurface.infiniteCanvas) {
                    const growth = canvasSurface.ensureInfiniteCanvasRegion(Number(command.x), Number(command.y), Number(command.width), Number(command.height));
                    if (growth && growth.changed) {
                        surface.applyInfiniteCanvasGrowth(growth);
                        applied = true;
                    }
                }
            } else if (type === "add-layer") {
                applied = addEmptyLayer() >= 0;
            } else if (type === "delete-layer") {
                applied = deleteLayerByKey(command.layerKey);
            } else if (type === "rename-layer") {
                applied = renameLayerByKey(command.layerKey, command.name);
            } else if (type === "reorder-layers") {
                applied = applyLayerKeyOrder(command.layerKeys);
            } else if (type === "undo" || type === "redo") {
                const rasterSurface = rasterSurfaceForLayerKey(command.layerKey);
                if (rasterSurface) {
                    if (type === "undo") {
                        rasterSurface.undo();
                    } else {
                        rasterSurface.redo();
                    }
                    applied = true;
                }
            } else if (type === "insert-image") {
                const maximumWidth = Math.max(1, canvasSurface.width * surface.imageInsertionMaximumCanvasRatio);
                const maximumHeight = Math.max(1, canvasSurface.height * surface.imageInsertionMaximumCanvasRatio);
                const imageObject = canvasSurface.remoteImageObject(command.data, maximumWidth, maximumHeight);
                applied = insertImageObject(imageObject, Boolean(command.positioned) ? Number(command.x) : NaN, Boolean(command.positioned) ? Number(command.y) : NaN, String(command.name));
            } else if (type === "open-raster") {
                const imageObject = canvasSurface.remoteImageObject(command.data, 0, 0);
                if (imageObject && String(imageObject.status || "") === "ready") {
                    applied = openRaster(String(imageObject.source || ""));
                }
            }
        } finally {
            surface.applyingHostEditCommand = false;
        }
        if (applied) {
            surface.sessionChanged();
        }
        return applied;
    }

    function setTemporaryCameraMode(mode) {
        let nextMode = mode === "pan" || mode === "zoom" ? mode : "";
        if (surface.textEditingActive || !surface.toolShortcutsEnabled) {
            nextMode = "";
        }
        if (surface.temporaryCameraMode === nextMode) {
            return nextMode !== "";
        }

        surface.drawableObjectHoverHandleMode = "";
        surface.commitPanDrag();
        surface.commitZoomDrag();
        surface.temporaryCameraMode = nextMode;
        return nextMode !== "";
    }

    function beginSpacePanMode() {
        return surface.setTemporaryCameraMode("pan");
    }

    function beginSpaceZoomMode() {
        return surface.setTemporaryCameraMode("zoom");
    }

    function endTemporaryCameraMode() {
        if (surface.temporaryCameraMode === "") {
            return false;
        }
        surface.setTemporaryCameraMode("");
        return true;
    }

    function endSpacePanMode() {
        return surface.endTemporaryCameraMode();
    }

    function temporaryCameraModeForModifiers(modifiers) {
        return (modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0 ? "zoom" : "pan";
    }

    function syncCanvasItemSizeToWorkspace() {
        if (!canvasItemReady || surface.width <= 0 || surface.height <= 0) {
            return;
        }
        canvasSurface.resizeCanvasSurface(workspaceCanvasWidth, workspaceCanvasHeight);
        resizeRasterLayerItems(workspaceCanvasWidth, workspaceCanvasHeight);
        canvasSizeCreated = true;
        fitCanvasZoomToCurrentCanvas();
    }

    function normalizedCanvasDimension(value, fallbackValue) {
        const parsedValue = Math.round(Number(value));
        if (!isFinite(parsedValue)) {
            return fallbackValue;
        }
        return Math.max(surface.minimumCanvasDimension, Math.min(surface.maximumCanvasDimension, parsedValue));
    }

    function resizeCanvasItemToDimensions(canvasWidth, canvasHeight) {
        if (!canvasItemReady) {
            return;
        }
        const nextCanvasWidth = normalizedCanvasDimension(canvasWidth, workspaceCanvasWidth);
        const nextCanvasHeight = normalizedCanvasDimension(canvasHeight, workspaceCanvasHeight);
        canvasSurface.resizeCanvasSurface(nextCanvasWidth, nextCanvasHeight);
        resizeRasterLayerItems(nextCanvasWidth, nextCanvasHeight);
        canvasSizeCreated = true;
        fitCanvasZoomToCurrentCanvas();
    }

    function applyInfiniteCanvasGrowth(growth) {
        const leftGrowth = Math.max(0, Number(growth.left || 0));
        const topGrowth = Math.max(0, Number(growth.top || 0));
        const rightGrowth = Math.max(0, Number(growth.right || 0));
        const bottomGrowth = Math.max(0, Number(growth.bottom || 0));
        const nextWidth = canvasSurface.canvasWidth;
        const nextHeight = canvasSurface.canvasHeight;
        const nextOriginX = canvasSurface.canvasOriginX;
        const nextOriginY = canvasSurface.canvasOriginY;

        for (const key in surface.rasterLayerItems) {
            const item = surface.rasterLayerItems[key];
            if (!item) {
                continue;
            }
            const layerGrowth = item.ensureInfiniteCanvasRegion(nextOriginX, nextOriginY, nextWidth, nextHeight);
            if (layerGrowth && layerGrowth.error) {
                console.warn("Congregation could not expand raster layer " + key + ": " + layerGrowth.error);
            }
            item.resizeCanvasSurface(nextWidth, nextHeight);
        }

        const shiftedObjects = [];
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const shiftedObject = cloneDrawableObject(surface.drawableObjects[index]);
            if (shiftedObject.type === "layer") {
                shiftedObject.x = 0;
                shiftedObject.y = 0;
                shiftedObject.width = nextWidth;
                shiftedObject.height = nextHeight;
            } else {
                shiftedObject.x = Number(shiftedObject.x || 0) + leftGrowth;
                shiftedObject.y = Number(shiftedObject.y || 0) + topGrowth;
            }
            shiftedObjects.push(shiftedObject);
        }
        surface.drawableObjects = shiftedObjects;
        for (let index = 0; index < shiftedObjects.length; ++index) {
            const shiftedObject = shiftedObjects[index];
            const visualIndex = drawableObjectVisualModelIndexForObjectId(shiftedObject.id);
            if (visualIndex < 0) {
                continue;
            }
            drawableObjectVisualModel.setProperty(visualIndex, "objectX", shiftedObject.x);
            drawableObjectVisualModel.setProperty(visualIndex, "objectY", shiftedObject.y);
            drawableObjectVisualModel.setProperty(visualIndex, "objectWidth", shiftedObject.width);
            drawableObjectVisualModel.setProperty(visualIndex, "objectHeight", shiftedObject.height);
        }

        canvasSurface.resizeCanvasSurface(nextWidth, nextHeight);
        surface.canvasPanOffsetX += (rightGrowth - leftGrowth) * surface.canvasZoomScale / 2;
        surface.canvasPanOffsetY += (bottomGrowth - topGrowth) * surface.canvasZoomScale / 2;
        surface.canvasSizeCreated = true;
    }

    function ensureInfiniteCanvasForViewport() {
        if (!surface.canvasItemReady || !canvasSurface.infiniteCanvas || surface.infiniteCanvasExpansionActive || canvasViewport.width <= 0 || canvasViewport.height <= 0) {
            return false;
        }

        const firstCorner = canvasSurface.mapFromItem(canvasViewport, 0, 0);
        const secondCorner = canvasSurface.mapFromItem(canvasViewport, canvasViewport.width, canvasViewport.height);
        const localLeft = Math.floor(Math.min(firstCorner.x, secondCorner.x));
        const localTop = Math.floor(Math.min(firstCorner.y, secondCorner.y));
        const localRight = Math.ceil(Math.max(firstCorner.x, secondCorner.x));
        const localBottom = Math.ceil(Math.max(firstCorner.y, secondCorner.y));
        if (![localLeft, localTop, localRight, localBottom].every(isFinite) || localRight <= localLeft || localBottom <= localTop) {
            return false;
        }

        if (surface.requestHostEditCommand({
            type: "ensure-region",
            x: Math.round(canvasSurface.canvasOriginX + localLeft),
            y: Math.round(canvasSurface.canvasOriginY + localTop),
            width: Math.round(localRight - localLeft),
            height: Math.round(localBottom - localTop)
        })) {
            return true;
        }

        surface.infiniteCanvasExpansionActive = true;
        const growth = canvasSurface.ensureInfiniteCanvasRegion(canvasSurface.canvasOriginX + localLeft, canvasSurface.canvasOriginY + localTop, localRight - localLeft, localBottom - localTop);
        if (growth && growth.changed) {
            surface.applyInfiniteCanvasGrowth(growth);
        }
        surface.infiniteCanvasExpansionActive = false;
        return Boolean(growth && growth.changed);
    }

    function newCanvas(canvasWidth, canvasHeight, infiniteCanvas) {
        if (surface.requestHostEditCommand({
            type: "new-canvas",
            width: Math.round(Number(canvasWidth)),
            height: Math.round(Number(canvasHeight)),
            infinite: infiniteCanvas === true
        })) {
            return true;
        }
        cancelPendingRecentCanvasRestore();
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        surface.backgroundLayerPresent = true;
        clearDrawableObjects();
        if (arguments.length >= 2) {
            resizeCanvasItemToDimensions(canvasWidth, canvasHeight);
        } else {
            syncCanvasItemSizeToWorkspace();
        }
        canvasSurface.newCanvas(infiniteCanvas === true, 0, 0, surface.defaultInfiniteCanvasChunkSize);
        addDefaultDrawingLayer();
        return true;
    }

    function clearCanvas() {
        if (surface.requestHostEditCommand({
            type: "clear-canvas"
        })) {
            return true;
        }
        cancelPendingRecentCanvasRestore();
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        surface.backgroundLayerPresent = true;
        clearDrawableObjects();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.clearCanvas();
        addDefaultDrawingLayer();
        return true;
    }

    function openRaster(fileUrl) {
        const sourceUrl = fileUrl ? fileUrl.toString() : "";
        if (surface.hostCanvasInputClient && !surface.applyingHostEditCommand) {
            const encodedImage = canvasSurface.portableImageData(sourceUrl);
            if (!encodedImage || Number(encodedImage.length || encodedImage.byteLength || 0) <= 0) {
                return false;
            }
            return surface.requestHostEditCommand({
                type: "open-raster",
                data: encodedImage
            });
        }
        cancelPendingRecentCanvasRestore();
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        if (sourceUrl.toLowerCase().endsWith(".psd") && openLayeredPsd(sourceUrl)) {
            return true;
        }

        if (!canvasSurface.openRaster(sourceUrl)) {
            return false;
        }

        surface.backgroundLayerPresent = true;
        clearDrawableObjects();
        canvasSizeCreated = true;
        resizeRasterLayerItems(canvasSurface.width, canvasSurface.height);
        fitCanvasZoomToCurrentCanvas();
        addDefaultDrawingLayer();
        return true;
    }

    function openRecentCanvas(fileUrl) {
        cancelPendingRecentCanvasRestore();
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        const session = canvasSurface.openRecentCanvas(fileUrl ? fileUrl.toString() : "");
        return restoreCanvasSession(session);
    }

    function applyCanvasSessionSnapshot(snapshot) {
        cancelPendingRecentCanvasRestore();
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        const session = canvasSurface.importCanvasSession(snapshot);
        return restoreCanvasSession(session);
    }

    function restoreCanvasSession(session) {
        if (!session || !session.valid) {
            sessionRestoreFinished(false);
            return false;
        }

        const restoredObjectSequence = session.drawableObjects || [];
        const restoredObjects = [];
        for (let index = 0; index < Number(restoredObjectSequence.length || 0); ++index) {
            restoredObjects.push(restoredObjectSequence[index]);
        }

        const seenObjectIds = {};
        var maximumObjectId = 0;
        for (let index = 0; index < restoredObjects.length; ++index) {
            const restoredObject = restoredObjects[index];
            const objectId = restoredObject ? Number(restoredObject.id) : 0;
            if (!restoredObject || objectId <= 0 || seenObjectIds[String(objectId)]) {
                sessionRestoreFinished(false);
                return false;
            }
            seenObjectIds[String(objectId)] = true;
            maximumObjectId = Math.max(maximumObjectId, objectId);
        }

        const restoreGeneration = ++surface.recentCanvasRestoreGeneration;
        surface.recentCanvasRestoreInProgress = true;
        clearDrawableObjects();
        surface.backgroundLayerPresent = Boolean(session.backgroundLayerPresent);
        surface.nextDrawableObjectId = 1;
        Qt.callLater(function () {
            if (restoreGeneration !== surface.recentCanvasRestoreGeneration) {
                return;
            }

            for (let index = 0; index < restoredObjects.length; ++index) {
                const restoredObject = restoredObjects[index];
                const objectId = Number(restoredObject.id);
                if (String(restoredObject.type) === "layer") {
                    const snapshotSources = copyStringMap(surface.rasterLayerSnapshotSources);
                    snapshotSources[String(objectId)] = String(restoredObject.snapshotSource || "");
                    surface.rasterLayerSnapshotSources = snapshotSources;
                }
                appendDrawableObject(restoredObject);
            }
            surface.nextDrawableObjectId = maximumObjectId + 1;
            surface.canvasSizeCreated = true;
            resizeRasterLayerItems(canvasSurface.width, canvasSurface.height);
            fitCanvasZoomToCurrentCanvas();
            rebuildLayerHierarchyRows();
            Qt.callLater(function () {
                if (restoreGeneration === surface.recentCanvasRestoreGeneration) {
                    surface.recentCanvasRestoreInProgress = false;
                    surface.sessionRestoreFinished(true);
                }
            });
        });
        return true;
    }

    function cancelPendingRecentCanvasRestore() {
        ++surface.recentCanvasRestoreGeneration;
        surface.recentCanvasRestoreInProgress = false;
    }

    function imageInsertionPlacement(sourceValue, objectWidth, objectHeight, requestedCenterX, requestedCenterY) {
        let duplicateCount = 0;
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (drawableObject && drawableObject.type === "image" && String(drawableObject.source || "") === sourceValue) {
                duplicateCount++;
            }
        }

        const offsetRing = Math.ceil(duplicateCount / 2);
        const offsetDirection = duplicateCount % 2 === 0 ? -1 : 1;
        const offset = duplicateCount === 0 ? 0 : offsetRing * surface.imageInsertionDuplicateOffset * offsetDirection;
        const hasRequestedCenter = isFinite(requestedCenterX) && isFinite(requestedCenterY);
        const centerX = hasRequestedCenter ? Number(requestedCenterX) : canvasSurface.width / 2;
        const centerY = hasRequestedCenter ? Number(requestedCenterY) : canvasSurface.height / 2;
        const centeredX = centerX - objectWidth / 2;
        const centeredY = centerY - objectHeight / 2;
        return {
            x: Math.max(0, Math.min(canvasSurface.width - objectWidth, centeredX + offset)),
            y: Math.max(0, Math.min(canvasSurface.height - objectHeight, centeredY + offset))
        };
    }

    function clipboardImagePlacement(sourceValue, objectWidth, objectHeight) {
        return imageInsertionPlacement(sourceValue, objectWidth, objectHeight, NaN, NaN);
    }

    function insertImageObject(imageObject, requestedCenterX, requestedCenterY, fallbackName) {
        const status = imageObject ? String(imageObject.status || "decode-failed") : "decode-failed";
        if (status !== "ready") {
            return false;
        }

        const source = String(imageObject.source || "");
        const objectWidth = Number(imageObject.width);
        const objectHeight = Number(imageObject.height);
        const originalWidth = Number(imageObject.originalWidth || objectWidth);
        const originalHeight = Number(imageObject.originalHeight || objectHeight);
        if (source.length === 0 || !isFinite(objectWidth) || objectWidth <= 0 || !isFinite(objectHeight) || objectHeight <= 0 || !isFinite(originalWidth) || originalWidth <= 0 || !isFinite(originalHeight) || originalHeight <= 0) {
            return false;
        }

        if (surface.hostCanvasInputClient && !surface.applyingHostEditCommand) {
            const encodedImage = canvasSurface.portableImageData(source);
            if (!encodedImage || Number(encodedImage.length || encodedImage.byteLength || 0) <= 0) {
                return false;
            }
            const positioned = isFinite(requestedCenterX) && isFinite(requestedCenterY);
            return surface.requestHostEditCommand({
                type: "insert-image",
                data: encodedImage,
                x: positioned ? Number(requestedCenterX) : 0,
                y: positioned ? Number(requestedCenterY) : 0,
                positioned: positioned,
                name: String(imageObject.suggestedName || fallbackName)
            });
        }

        const placement = imageInsertionPlacement(source, objectWidth, objectHeight, requestedCenterX, requestedCenterY);
        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();

        const insertedObject = {
            id: surface.nextDrawableObjectId++,
            type: "image",
            name: String(imageObject.suggestedName || fallbackName),
            source: source,
            originalSource: String(imageObject.originalSource || ""),
            x: placement.x,
            y: placement.y,
            width: objectWidth,
            height: objectHeight,
            originalWidth: originalWidth,
            originalHeight: originalHeight,
            opacity: 1,
            visible: true
        };
        appendDrawableObject(insertedObject);
        return true;
    }

    function pasteClipboardImage() {
        surface.clipboardImagePasteErrorCode = "";
        if (!canvasItemReady) {
            surface.clipboardImagePasteErrorCode = "canvas-unavailable";
            return false;
        }

        const maximumObjectWidth = Math.max(1, canvasSurface.width * surface.imageInsertionMaximumCanvasRatio);
        const maximumObjectHeight = Math.max(1, canvasSurface.height * surface.imageInsertionMaximumCanvasRatio);
        const clipboardObject = canvasSurface.clipboardImageObject(maximumObjectWidth, maximumObjectHeight);
        const clipboardStatus = clipboardObject ? String(clipboardObject.status || "decode-failed") : "decode-failed";
        if (clipboardStatus !== "ready") {
            surface.clipboardImagePasteErrorCode = clipboardStatus;
            return false;
        }

        if (!insertImageObject(clipboardObject, NaN, NaN, qsTr("Pasted Image"))) {
            surface.clipboardImagePasteErrorCode = "decode-failed";
            return false;
        }
        return true;
    }

    function insertDroppedImageObject(imageObject, dropX, dropY) {
        if (!insertImageObject(imageObject, dropX, dropY, qsTr("Dropped Image"))) {
            surface.imageDropFailed("decode-failed");
            return false;
        }
        surface.imageDropSucceeded();
        return true;
    }

    function shouldRestorePsdBackgroundLayer(psdDocument) {
        const manifest = psdDocument && psdDocument.congregationManifest ? psdDocument.congregationManifest : null;
        const manifestLayers = manifest && Array.isArray(manifest.layers) ? manifest.layers : [];
        const importedLayers = psdDocument && Array.isArray(psdDocument.layers) ? psdDocument.layers : [];
        if (manifestLayers.length === 0 || importedLayers.length === 0) {
            return false;
        }
        return String(manifestLayers[0].name || "") === "Background" && String(importedLayers[0].name || "") === "Background";
    }

    function appendImportedPsdRasterLayer(layer) {
        if (!layer || !layer.source) {
            return -1;
        }

        const layerId = surface.nextDrawableObjectId++;
        appendDrawableObject({
            id: layerId,
            type: "layer",
            name: layer.name && layer.name.length ? layer.name : nextEmptyLayerName(),
            x: 0,
            y: 0,
            width: Math.max(1, canvasSurface.width),
            height: Math.max(1, canvasSurface.height),
            opacity: Number(layer.opacityRatio === undefined ? 1 : layer.opacityRatio),
            visible: layer.visible === undefined ? true : Boolean(layer.visible),
            blendMode: layer.blendModeKey || "norm",
            psdBounds: {
                left: Number(layer.left || 0),
                top: Number(layer.top || 0),
                right: Number(layer.right || 0),
                bottom: Number(layer.bottom || 0)
            }
        });
        surface.rasterLayerSnapshotSources[String(layerId)] = layer.source;
        if (layer.thumbnailSource && layer.thumbnailSource.length > 0) {
            setRasterLayerThumbnailSource(layerId, layer.thumbnailSource);
        }
        return layerId;
    }

    function openLayeredPsd(fileUrl) {
        const psdDocument = canvasSurface.psdImportDocument(fileUrl);
        const importedLayers = psdDocument && Array.isArray(psdDocument.layers) ? psdDocument.layers : [];
        if (!psdDocument || !psdDocument.valid || importedLayers.length === 0) {
            return false;
        }

        surface.backgroundLayerPresent = false;
        clearDrawableObjects();
        resizeCanvasItemToDimensions(psdDocument.canvasWidth, psdDocument.canvasHeight);
        canvasSurface.newCanvas(false);

        var firstObjectId = -1;
        var startIndex = 0;
        if (shouldRestorePsdBackgroundLayer(psdDocument) && importedLayers[0].source) {
            surface.backgroundLayerPresent = true;
            canvasSurface.restoreRasterSnapshot(importedLayers[0].source);
            startIndex = 1;
        }

        for (let index = startIndex; index < importedLayers.length; ++index) {
            const importedObjectId = appendImportedPsdRasterLayer(importedLayers[index]);
            if (firstObjectId < 0 && importedObjectId >= 0) {
                firstObjectId = importedObjectId;
            }
        }
        surface.selectedDrawableObjectId = firstObjectId;
        rebuildLayerHierarchyRows();
        return true;
    }

    function saveToFile(fileUrl) {
        commitActiveText();
        commitActiveShape();
        return canvasSurface.saveToFileWithObjectsAndRasterLayers(fileUrl ? fileUrl.toString() : "", surface.drawableObjects, rasterLayerDescriptors(), surface.backgroundLayerPresent);
    }

    function saveRecentCanvas(fileUrl) {
        return canvasSurface.saveRecentCanvas(fileUrl ? fileUrl.toString() : "", surface.drawableObjects, rasterLayerDescriptors(), surface.backgroundLayerPresent);
    }

    function canvasSessionSnapshot() {
        commitActiveText();
        commitActiveShape();
        commitDrawableObjectTransform();
        return canvasSurface.exportCanvasSession(surface.drawableObjects, rasterLayerDescriptors(), surface.backgroundLayerPresent);
    }

    function psdCompatibilityManifest() {
        commitActiveText();
        commitActiveShape();
        return canvasSurface.psdCompatibilityManifest(surface.drawableObjects, surface.backgroundLayerPresent);
    }

    function longestTextLine(textValue) {
        const lines = (textValue && textValue.length ? textValue : " ").split(/\r?\n/);
        var longestLine = " ";
        for (let index = 0; index < lines.length; ++index) {
            if (lines[index].length > longestLine.length) {
                longestLine = lines[index];
            }
        }
        return longestLine.length ? longestLine : " ";
    }

    function textToolAvailableWidth() {
        return Math.max(surface.textToolMinimumWidth, canvasSurface.width - textToolEditorFrame.x);
    }

    function textToolMeasuredWidth() {
        return Math.ceil(textToolLineMetrics.advanceWidth) + surface.textToolFramePadding * 2;
    }

    function textToolResponsiveWidth() {
        return Math.min(surface.textToolAvailableWidth(), Math.max(surface.textToolMinimumWidth, surface.textToolMeasuredWidth()));
    }

    function beginTextPlacement(pointX, pointY) {
        if (surface.toolMode !== "text") {
            return;
        }

        commitActiveText();
        const maxX = Math.max(0, canvasSurface.width - surface.textToolMinimumWidth);
        const maxY = Math.max(0, canvasSurface.height - surface.textToolFontPixelSize - surface.textToolFramePadding * 2);
        textToolEditorFrame.x = Math.max(0, Math.min(maxX, pointX));
        textToolEditorFrame.y = Math.max(0, Math.min(maxY, pointY));
        textToolEditor.text = "";
        surface.textEditingActive = true;
        textToolEditor.forceActiveFocus();
    }

    function commitActiveText() {
        if (!surface.textEditingActive) {
            return;
        }

        const committedText = textToolEditor.text;
        const shouldCommit = committedText.trim().length > 0;
        surface.textEditingActive = false;
        if (shouldCommit) {
            if (!surface.requestHostEditCommand({
                type: "add-text",
                x: textToolEditorFrame.x,
                y: textToolEditorFrame.y,
                width: textToolEditorFrame.width,
                height: textToolEditorFrame.height,
                text: committedText,
                fontSize: surface.textToolFontPixelSize,
                color: surface.brushColor.toString()
            })) {
                appendDrawableObject({
                    id: surface.nextDrawableObjectId++,
                    type: "text",
                    x: textToolEditorFrame.x,
                    y: textToolEditorFrame.y,
                    width: textToolEditorFrame.width,
                    height: textToolEditorFrame.height,
                    text: committedText,
                    fontPixelSize: surface.textToolFontPixelSize,
                    color: surface.brushColor.toString()
                });
            }
        }
        textToolEditor.text = "";
        textToolEditor.focus = false;
    }

    function cancelActiveText() {
        if (!surface.textEditingActive) {
            return;
        }

        surface.textEditingActive = false;
        textToolEditor.text = "";
        textToolEditor.focus = false;
    }

    function clampedShapePointX(pointX) {
        return Math.max(0, Math.min(canvasSurface.width, pointX));
    }

    function clampedShapePointY(pointY) {
        return Math.max(0, Math.min(canvasSurface.height, pointY));
    }

    function shapePreviewRect() {
        const left = Math.min(surface.shapeStartX, surface.shapeCurrentX);
        const top = Math.min(surface.shapeStartY, surface.shapeCurrentY);
        return {
            x: left,
            y: top,
            width: Math.abs(surface.shapeCurrentX - surface.shapeStartX),
            height: Math.abs(surface.shapeCurrentY - surface.shapeStartY)
        };
    }

    function shiftModifierActiveFromMouse(mouse) {
        return (mouse.modifiers & Qt.ShiftModifier) !== 0;
    }

    function shapeAspectLockedFromMouse(mouse) {
        return shiftModifierActiveFromMouse(mouse);
    }

    function drawableObjectTransformConstrainedFromMouse(mouse) {
        return shiftModifierActiveFromMouse(mouse);
    }

    function shapeDragAxisDirection(delta, negativeLimit, positiveLimit) {
        if (delta < 0) {
            return -1;
        }
        if (delta > 0) {
            return 1;
        }
        return positiveLimit >= negativeLimit ? 1 : -1;
    }

    function constrainedShapeDragPoint(pointX, pointY) {
        const clampedX = clampedShapePointX(pointX);
        const clampedY = clampedShapePointY(pointY);
        const deltaX = clampedX - surface.shapeStartX;
        const deltaY = clampedY - surface.shapeStartY;
        const leftLimit = surface.shapeStartX;
        const rightLimit = canvasSurface.width - surface.shapeStartX;
        const topLimit = surface.shapeStartY;
        const bottomLimit = canvasSurface.height - surface.shapeStartY;
        const horizontalDirection = shapeDragAxisDirection(deltaX, leftLimit, rightLimit);
        const verticalDirection = shapeDragAxisDirection(deltaY, topLimit, bottomLimit);
        const horizontalLimit = horizontalDirection < 0 ? leftLimit : rightLimit;
        const verticalLimit = verticalDirection < 0 ? topLimit : bottomLimit;
        const side = Math.min(Math.max(Math.abs(deltaX), Math.abs(deltaY)), horizontalLimit, verticalLimit);
        return {
            x: surface.shapeStartX + horizontalDirection * side,
            y: surface.shapeStartY + verticalDirection * side
        };
    }

    function shapeDragPoint(pointX, pointY, aspectLocked) {
        if (aspectLocked) {
            return constrainedShapeDragPoint(pointX, pointY);
        }
        return {
            x: clampedShapePointX(pointX),
            y: clampedShapePointY(pointY)
        };
    }

    function applyShapeDragPoint(pointX, pointY, aspectLocked) {
        surface.shapeAspectLocked = aspectLocked === true;
        const dragPoint = shapeDragPoint(pointX, pointY, surface.shapeAspectLocked);
        surface.shapeCurrentX = dragPoint.x;
        surface.shapeCurrentY = dragPoint.y;
        requestShapePreviewPaint();
    }

    function requestShapePreviewPaint() {
        if (shapePreviewCanvas.visible) {
            shapePreviewCanvas.requestPaint();
        }
    }

    function beginShapeDrag(pointX, pointY, aspectLocked) {
        if (surface.toolMode !== "shape") {
            return;
        }

        commitActiveText();
        surface.shapeStartX = clampedShapePointX(pointX);
        surface.shapeStartY = clampedShapePointY(pointY);
        surface.shapeCurrentX = surface.shapeStartX;
        surface.shapeCurrentY = surface.shapeStartY;
        surface.shapeAspectLocked = aspectLocked === true;
        surface.shapeDraggingActive = true;
        requestShapePreviewPaint();
    }

    function updateShapeDrag(pointX, pointY, aspectLocked) {
        if (!surface.shapeDraggingActive) {
            return;
        }

        applyShapeDragPoint(pointX, pointY, aspectLocked);
    }

    function commitActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        const bounds = shapePreviewRect();
        surface.shapeDraggingActive = false;
        surface.shapeAspectLocked = false;
        if (bounds.width >= surface.shapeToolMinimumDragDistance && bounds.height >= surface.shapeToolMinimumDragDistance) {
            if (!surface.requestHostEditCommand({
                type: "add-shape",
                x: bounds.x,
                y: bounds.y,
                width: bounds.width,
                height: bounds.height,
                shape: surface.shapeKind,
                color: surface.brushColor.toString()
            })) {
                appendDrawableObject({
                    id: surface.nextDrawableObjectId++,
                    type: "shape",
                    x: bounds.x,
                    y: bounds.y,
                    width: bounds.width,
                    height: bounds.height,
                    shapeKind: surface.shapeKind,
                    color: surface.brushColor.toString()
                });
            }
        }
        requestShapePreviewPaint();
    }

    function clearDrawableObjects() {
        surface.persistRasterLayerSnapshots = false;
        drawableObjectVisualModel.clear();
        surface.drawableObjects = [];
        surface.rasterLayerItems = {};
        surface.rasterLayerSnapshotSources = {};
        surface.rasterLayerThumbnailSources = {};
        surface.drawableObjectThumbnailSources = {};
        surface.pendingRasterLayerThumbnailRefreshes = {};
        surface.backgroundLayerThumbnailRefreshPending = false;
        layerThumbnailRefreshTimer.stop();
        surface.persistRasterLayerSnapshots = true;
        surface.nextEmptyLayerNumber = 1;
        surface.selectedDrawableObjectId = -1;
        resetDrawableObjectTransform();
        rebuildLayerHierarchyRows();
    }

    function rasterLayerObjectSelected() {
        const drawableObject = selectedDrawableObject();
        return drawableObject !== null && drawableObject.type === "layer";
    }

    function rasterLayerItemById(objectId) {
        return surface.rasterLayerItems[String(objectId)] || null;
    }

    function rasterLayerSnapshotSource(objectId) {
        return surface.rasterLayerSnapshotSources[String(objectId)] || "";
    }

    function copyStringMap(mapValue) {
        const copied = {};
        if (!mapValue) {
            return copied;
        }
        for (const key in mapValue) {
            copied[key] = mapValue[key];
        }
        return copied;
    }

    function scheduleLayerHierarchyRowsRebuild() {
        if (surface.layerHierarchyRowsRebuildScheduled) {
            return;
        }
        surface.layerHierarchyRowsRebuildScheduled = true;
        Qt.callLater(function () {
            surface.layerHierarchyRowsRebuildScheduled = false;
            surface.rebuildLayerHierarchyRows();
        });
    }

    function scheduleLayerThumbnailRefreshFlush() {
        layerThumbnailRefreshTimer.restart();
    }

    function requestBackgroundLayerThumbnailRefresh() {
        surface.backgroundLayerThumbnailRefreshPending = true;
        scheduleLayerThumbnailRefreshFlush();
    }

    function requestRasterLayerThumbnailRefresh(objectId) {
        const nextPending = copyStringMap(surface.pendingRasterLayerThumbnailRefreshes);
        nextPending[String(objectId)] = true;
        surface.pendingRasterLayerThumbnailRefreshes = nextPending;
        scheduleLayerThumbnailRefreshFlush();
    }

    function clearPendingRasterLayerThumbnailRefresh(objectId) {
        const key = String(objectId);
        if (surface.pendingRasterLayerThumbnailRefreshes[key] === undefined) {
            return;
        }
        const nextPending = copyStringMap(surface.pendingRasterLayerThumbnailRefreshes);
        delete nextPending[key];
        surface.pendingRasterLayerThumbnailRefreshes = nextPending;
    }

    function flushPendingLayerThumbnailRefreshes() {
        layerThumbnailRefreshTimer.stop();
        if (surface.backgroundLayerThumbnailRefreshPending) {
            surface.backgroundLayerThumbnailRefreshPending = false;
            refreshBackgroundLayerThumbnailSource();
        }

        const pendingLayerIds = Object.keys(surface.pendingRasterLayerThumbnailRefreshes);
        if (pendingLayerIds.length === 0) {
            return;
        }
        surface.pendingRasterLayerThumbnailRefreshes = {};
        for (let index = 0; index < pendingLayerIds.length; ++index) {
            refreshRasterLayerThumbnailSource(Number(pendingLayerIds[index]));
        }
    }

    function setDrawableObjectThumbnailSource(objectId, source) {
        const key = String(objectId);
        if ((surface.drawableObjectThumbnailSources[key] || "") === source) {
            return;
        }
        const nextSources = copyStringMap(surface.drawableObjectThumbnailSources);
        if (source.length > 0) {
            nextSources[key] = source;
        } else {
            delete nextSources[key];
        }
        surface.drawableObjectThumbnailSources = nextSources;
    }

    function setRasterLayerThumbnailSource(objectId, source) {
        const key = String(objectId);
        if ((surface.rasterLayerThumbnailSources[key] || "") === source) {
            return;
        }
        const nextSources = copyStringMap(surface.rasterLayerThumbnailSources);
        if (source.length > 0) {
            nextSources[key] = source;
        } else {
            delete nextSources[key];
        }
        surface.rasterLayerThumbnailSources = nextSources;
    }

    function clearRasterLayerThumbnailState(objectId) {
        clearPendingRasterLayerThumbnailRefresh(objectId);
        setRasterLayerThumbnailSource(objectId, "");
    }

    function fallbackRasterThumbnailSource(surfaceItem) {
        if (!surfaceItem || !surfaceItem.cacheRasterThumbnailSource) {
            return "";
        }
        return surfaceItem.cacheRasterThumbnailSource(surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize) || "";
    }

    function refreshRasterSurfaceThumbnailSource(surfaceItem, applySource) {
        if (!surfaceItem) {
            return "";
        }

        if (surfaceItem.grabToImage) {
            const accepted = surfaceItem.grabToImage(function (result) {
                const grabbedSource = result && surfaceItem.cacheGrabbedThumbnailSource ? surfaceItem.cacheGrabbedThumbnailSource(result) : "";
                if (grabbedSource.length > 0) {
                    applySource(grabbedSource);
                    scheduleLayerHierarchyRowsRebuild();
                    return;
                }

                applySource(fallbackRasterThumbnailSource(surfaceItem));
                scheduleLayerHierarchyRowsRebuild();
            }, Qt.size(surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize));
            if (accepted) {
                return "";
            }
        }

        const source = fallbackRasterThumbnailSource(surfaceItem);
        applySource(source);
        scheduleLayerHierarchyRowsRebuild();
        return source;
    }

    function invalidateDrawableObjectThumbnailSource(objectId) {
        setDrawableObjectThumbnailSource(objectId, "");
    }

    function refreshDrawableObjectThumbnailSource(drawableObject) {
        if (!drawableObject || drawableObject.type === "layer") {
            return "";
        }
        const source = canvasSurface.cacheDrawableObjectThumbnailSource(drawableObject, surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize);
        setDrawableObjectThumbnailSource(drawableObject.id, source || "");
        return source || "";
    }

    function refreshRasterLayerThumbnailSource(objectId) {
        const item = rasterLayerItemById(objectId);
        if (!item) {
            return "";
        }
        clearPendingRasterLayerThumbnailRefresh(objectId);
        return refreshRasterSurfaceThumbnailSource(item, function (source) {
            if (rasterLayerItemById(objectId) !== item) {
                return;
            }
            setRasterLayerThumbnailSource(objectId, source || "");
        });
    }

    function refreshBackgroundLayerThumbnailSource() {
        surface.backgroundLayerThumbnailRefreshPending = false;
        return refreshRasterSurfaceThumbnailSource(canvasSurface, function (source) {
            surface.backgroundLayerThumbnailSource = source || "";
        });
    }

    function layerIconSourceForDrawableObject(drawableObject) {
        if (!drawableObject) {
            return "";
        }
        if (drawableObject.type === "layer") {
            return surface.rasterLayerThumbnailSources[String(drawableObject.id)] || "";
        }

        const key = String(drawableObject.id);
        const cachedSource = surface.drawableObjectThumbnailSources[key] || "";
        return cachedSource.length > 0 ? cachedSource : refreshDrawableObjectThumbnailSource(drawableObject);
    }

    function registerRasterLayerItem(objectId, surfaceItem) {
        if (!surfaceItem) {
            return;
        }

        surface.rasterLayerItems[String(objectId)] = surfaceItem;

        surfaceItem.resizeCanvasSurface(canvasSurface.width, canvasSurface.height);
        surfaceItem.newCanvas(canvasSurface.infiniteCanvas, canvasSurface.canvasOriginX, canvasSurface.canvasOriginY, canvasSurface.infiniteCanvas ? canvasSurface.canvasChunkSize : surface.defaultInfiniteCanvasChunkSize);
        const snapshotSource = rasterLayerSnapshotSource(objectId);
        if (snapshotSource.length > 0) {
            surfaceItem.restoreRasterSnapshot(snapshotSource);
        }
        refreshRasterLayerThumbnailSource(objectId);
    }

    function unregisterRasterLayerItem(objectId, surfaceItem) {
        const objectKey = String(objectId);
        if (surface.rasterLayerItems[objectKey] !== surfaceItem) {
            return;
        }

        if (surface.persistRasterLayerSnapshots && surfaceItem) {
            const snapshotSource = surfaceItem.cacheRasterSnapshotSource();
            if (snapshotSource.length > 0) {
                surface.rasterLayerSnapshotSources[objectKey] = snapshotSource;
            }
        }

        delete surface.rasterLayerItems[objectKey];
        clearRasterLayerThumbnailState(objectId);
    }

    function resizeRasterLayerItems(canvasWidth, canvasHeight) {
        for (const key in surface.rasterLayerItems) {
            const item = surface.rasterLayerItems[key];
            if (item) {
                item.resizeCanvasSurface(canvasWidth, canvasHeight);
            }
        }
    }

    function activeRasterSurface() {
        if (rasterLayerObjectSelected()) {
            const item = rasterLayerItemById(surface.selectedDrawableObjectId);
            if (item) {
                return item;
            }
        }
        return surface.backgroundLayerPresent ? canvasSurface : null;
    }

    function hasActiveRasterSurface() {
        return activeRasterSurface() !== null;
    }

    function rasterLayerDescriptors() {
        const layers = [];
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (!drawableObject || drawableObject.type !== "layer") {
                continue;
            }
            layers.push({
                objectId: drawableObject.id,
                item: rasterLayerItemById(drawableObject.id),
                snapshotSource: rasterLayerSnapshotSource(drawableObject.id)
            });
        }
        return layers;
    }

    function drawableObjectVisualModelEntry(drawableObject) {
        return {
            objectId: drawableObject.id,
            objectType: drawableObject.type || "",
            objectX: Number(drawableObject.x || 0),
            objectY: Number(drawableObject.y || 0),
            objectWidth: Math.max(1, Number(drawableObject.width || 1)),
            objectHeight: Math.max(1, Number(drawableObject.height || 1)),
            objectSource: drawableObject.source || "",
            objectShapeKind: drawableObject.shapeKind || "",
            objectColor: drawableObject.color || "",
            objectText: drawableObject.text || "",
            objectFontPixelSize: Math.max(1, Number(drawableObject.fontPixelSize || 1)),
            objectOpacity: Math.max(0, Math.min(1, Number(drawableObject.opacity === undefined ? 1 : drawableObject.opacity))),
            objectVisible: drawableObject.visible === undefined ? true : Boolean(drawableObject.visible),
            objectBlendMode: drawableObject.blendMode || "norm"
        };
    }

    function drawableObjectVisualModelIndexForObjectId(objectId) {
        for (let index = 0; index < drawableObjectVisualModel.count; ++index) {
            if (drawableObjectVisualModel.get(index).objectId === objectId) {
                return index;
            }
        }
        return -1;
    }

    function drawableObjectVisualModelCount() {
        return drawableObjectVisualModel.count;
    }

    function setDrawableObjectVisualModelEntry(drawableObject) {
        const index = drawableObjectVisualModelIndexForObjectId(drawableObject.id);
        if (index >= 0) {
            drawableObjectVisualModel.set(index, drawableObjectVisualModelEntry(drawableObject));
        }
    }

    function reorderDrawableObjectVisualModel(orderedObjects) {
        for (let targetIndex = 0; targetIndex < orderedObjects.length; ++targetIndex) {
            const currentIndex = drawableObjectVisualModelIndexForObjectId(orderedObjects[targetIndex].id);
            if (currentIndex >= 0 && currentIndex !== targetIndex) {
                drawableObjectVisualModel.move(currentIndex, targetIndex, 1);
            }
        }
    }

    function cloneDrawableObject(drawableObject) {
        const copy = {};
        for (const key in drawableObject) {
            copy[key] = drawableObject[key];
        }
        return copy;
    }

    function explicitLayerName(drawableObject) {
        if (!drawableObject) {
            return "";
        }
        const name = drawableObject.name === undefined || drawableObject.name === null ? "" : String(drawableObject.name).trim();
        return name;
    }

    function titleCaseLayerLabel(value, fallbackValue) {
        const textValue = value === undefined || value === null ? "" : String(value).trim();
        if (textValue.length === 0) {
            return fallbackValue;
        }
        return textValue.charAt(0).toUpperCase() + textValue.slice(1);
    }

    function fileNameFromLayerSource(sourceValue) {
        const sourceText = sourceValue === undefined || sourceValue === null ? "" : String(sourceValue);
        const pathText = sourceText.split("?")[0].split("#")[0];
        const lastSlashIndex = pathText.lastIndexOf("/");
        const fileName = lastSlashIndex >= 0 ? pathText.substring(lastSlashIndex + 1) : pathText;
        try {
            return decodeURIComponent(fileName);
        } catch (error) {
            return fileName;
        }
    }

    function firstLayerTextLine(textValue) {
        const text = textValue === undefined || textValue === null ? "" : String(textValue).trim();
        if (text.length === 0) {
            return qsTr("Text");
        }
        const firstLine = text.split(/\r?\n/)[0].trim();
        return (firstLine.length > 0 ? firstLine : qsTr("Text")).slice(0, 32);
    }

    function layerLabelForDrawableObject(drawableObject) {
        if (!drawableObject) {
            return qsTr("Layer");
        }

        const explicitName = explicitLayerName(drawableObject);
        if (explicitName.length > 0) {
            return explicitName;
        }
        if (drawableObject.type === "image") {
            const fileName = fileNameFromLayerSource(drawableObject.originalSource || drawableObject.source);
            return fileName.length > 0 ? fileName : qsTr("Image");
        }
        if (drawableObject.type === "text") {
            return firstLayerTextLine(drawableObject.text);
        }
        if (drawableObject.type === "shape") {
            return titleCaseLayerLabel(drawableObject.shapeKind, qsTr("Shape"));
        }
        if (drawableObject.type === "layer") {
            return qsTr("Layer %1").arg(drawableObject.id);
        }
        return qsTr("Layer %1").arg(drawableObject.id);
    }

    function layerKeyForDrawableObjectId(objectId) {
        return "object-" + objectId;
    }

    function drawableObjectForLayerKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                return drawableObject;
            }
        }
        return null;
    }

    function currentLayerKey() {
        if (surface.selectedDrawableObjectId >= 0) {
            return layerKeyForDrawableObjectId(surface.selectedDrawableObjectId);
        }
        return surface.backgroundLayerPresent ? "raster-canvas" : "";
    }

    function rebuildLayerHierarchyRows() {
        const rows = [];
        for (let index = surface.drawableObjects.length - 1; index >= 0; --index) {
            const drawableObject = surface.drawableObjects[index];
            rows.push({
                key: layerKeyForDrawableObjectId(drawableObject.id),
                itemId: drawableObject.id,
                objectId: drawableObject.id,
                layerKind: "layer",
                contentKind: drawableObject.type || "",
                depth: 0,
                parentKey: "",
                parentItemKey: "",
                label: layerLabelForDrawableObject(drawableObject),
                iconSource: layerIconSourceForDrawableObject(drawableObject),
                iconGlyph: "",
                selected: drawableObject.id === surface.selectedDrawableObjectId,
                enabled: true,
                activatable: true,
                draggable: true,
                showChevron: false
            });
        }
        if (surface.backgroundLayerPresent) {
            rows.push({
                key: "raster-canvas",
                itemId: 0,
                objectId: -1,
                layerKind: "raster",
                depth: 0,
                parentKey: "",
                parentItemKey: "",
                label: qsTr("Background"),
                iconSource: surface.backgroundLayerThumbnailSource,
                iconGlyph: "",
                selected: surface.selectedDrawableObjectId < 0,
                enabled: true,
                activatable: true,
                draggable: false,
                showChevron: false
            });
        }
        surface.layerHierarchyRows = rows;
    }

    function appendDrawableObject(drawableObject) {
        const nextObjects = surface.drawableObjects.slice();
        nextObjects.push(drawableObject);
        surface.drawableObjects = nextObjects;
        refreshDrawableObjectThumbnailSource(drawableObject);
        drawableObjectVisualModel.append(drawableObjectVisualModelEntry(drawableObject));
        surface.selectedDrawableObjectId = drawableObject.id;
    }

    function nextEmptyLayerName() {
        return qsTr("Layer %1").arg(surface.nextEmptyLayerNumber++);
    }

    function addEmptyLayer() {
        if (surface.requestHostEditCommand({
            type: "add-layer"
        })) {
            return -1;
        }
        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();

        const layerId = surface.nextDrawableObjectId++;
        appendDrawableObject({
            id: layerId,
            type: "layer",
            name: nextEmptyLayerName(),
            x: 0,
            y: 0,
            width: Math.max(1, canvasSurface.width),
            height: Math.max(1, canvasSurface.height),
            opacity: 1,
            visible: true
        });
        return layerId;
    }

    function addDefaultDrawingLayer() {
        if (surface.drawableObjects.length > 0) {
            return -1;
        }
        return addEmptyLayer();
    }

    function replaceDrawableObjectById(objectId, drawableObject) {
        const nextObjects = surface.drawableObjects.slice();
        for (let index = 0; index < nextObjects.length; ++index) {
            if (nextObjects[index].id === objectId) {
                nextObjects[index] = drawableObject;
                surface.drawableObjects = nextObjects;
                invalidateDrawableObjectThumbnailSource(objectId);
                setDrawableObjectVisualModelEntry(drawableObject);
                return true;
            }
        }
        return false;
    }

    function activateLayerByKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        if (normalizedKey === "raster-canvas") {
            if (!surface.backgroundLayerPresent) {
                return false;
            }
            surface.selectedDrawableObjectId = -1;
            resetDrawableObjectTransform();
            return true;
        }

        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                surface.selectedDrawableObjectId = drawableObject.id;
                resetDrawableObjectTransform();
                return true;
            }
        }
        return false;
    }

    function deleteBackgroundLayer() {
        if (!surface.backgroundLayerPresent) {
            return false;
        }

        surface.backgroundLayerPresent = false;
        surface.backgroundLayerThumbnailSource = "";
        surface.backgroundLayerThumbnailRefreshPending = false;
        surface.selectedDrawableObjectId = surface.drawableObjects.length > 0 ? surface.drawableObjects[surface.drawableObjects.length - 1].id : -1;
        resetDrawableObjectTransform();
        canvasSurface.newCanvas(canvasSurface.infiniteCanvas, canvasSurface.canvasOriginX, canvasSurface.canvasOriginY, canvasSurface.infiniteCanvas ? canvasSurface.canvasChunkSize : surface.defaultInfiniteCanvasChunkSize);
        rebuildLayerHierarchyRows();
        return true;
    }

    function deleteLayerByKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        if (surface.requestHostEditCommand({
            type: "delete-layer",
            layerKey: normalizedKey
        })) {
            return true;
        }
        if (normalizedKey === "raster-canvas") {
            return deleteBackgroundLayer();
        }

        if (!activateLayerByKey(normalizedKey) || surface.selectedDrawableObjectId < 0) {
            return false;
        }
        return deleteSelectedDrawableObject();
    }

    function canDeleteCurrentLayer() {
        return surface.selectedDrawableObjectId >= 0 || (surface.selectedDrawableObjectId < 0 && surface.backgroundLayerPresent);
    }

    function deleteCurrentLayer() {
        const key = currentLayerKey();
        return key.length > 0 && deleteLayerByKey(key);
    }

    function canRenameLayerByKey(layerKey) {
        return drawableObjectForLayerKey(layerKey) !== null;
    }

    function layerNameByKey(layerKey) {
        const drawableObject = drawableObjectForLayerKey(layerKey);
        return drawableObject ? layerLabelForDrawableObject(drawableObject) : "";
    }

    function renameLayerByKey(layerKey, layerName) {
        const normalizedName = layerName === undefined || layerName === null ? "" : String(layerName).trim();
        if (normalizedName.length === 0) {
            return false;
        }

        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        if (surface.requestHostEditCommand({
            type: "rename-layer",
            layerKey: normalizedKey,
            name: normalizedName
        })) {
            return true;
        }
        const nextObjects = surface.drawableObjects.slice();
        for (let index = 0; index < nextObjects.length; ++index) {
            const drawableObject = nextObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                const renamedObject = cloneDrawableObject(drawableObject);
                renamedObject.name = normalizedName;
                nextObjects[index] = renamedObject;
                surface.drawableObjects = nextObjects;
                setDrawableObjectVisualModelEntry(renamedObject);
                rebuildLayerHierarchyRows();
                return true;
            }
        }
        return false;
    }

    function applyLayerHierarchyOrder(layerRows) {
        const rows = Array.isArray(layerRows) ? layerRows : surface.layerHierarchyRows;
        if (surface.hostCanvasInputClient && !surface.applyingHostEditCommand) {
            const layerKeys = [];
            for (let index = 0; index < rows.length; ++index) {
                const row = rows[index];
                if (row && (row.layerKind === "layer" || row.layerKind === "object" || row.layerKind === "raster")) {
                    layerKeys.push(String(row.key || ""));
                }
            }
            return surface.requestHostEditCommand({
                type: "reorder-layers",
                layerKeys: layerKeys
            });
        }
        const objectById = {};
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            objectById[String(drawableObject.id)] = drawableObject;
        }

        const topToBottomObjects = [];
        for (let rowIndex = 0; rowIndex < rows.length; ++rowIndex) {
            const row = rows[rowIndex];
            if (!row || (row.layerKind !== "layer" && row.layerKind !== "object")) {
                continue;
            }
            const objectId = String(row.objectId);
            if (objectById[objectId] !== undefined) {
                topToBottomObjects.push(objectById[objectId]);
            }
        }
        if (topToBottomObjects.length !== surface.drawableObjects.length) {
            rebuildLayerHierarchyRows();
            return false;
        }

        const nextObjects = [];
        for (let index = topToBottomObjects.length - 1; index >= 0; --index) {
            nextObjects.push(topToBottomObjects[index]);
        }
        surface.drawableObjects = nextObjects;
        reorderDrawableObjectVisualModel(nextObjects);
        if (surface.selectedDrawableObjectId >= 0 && objectById[String(surface.selectedDrawableObjectId)] === undefined) {
            surface.selectedDrawableObjectId = -1;
        }
        rebuildLayerHierarchyRows();
        return true;
    }

    function selectedDrawableObject() {
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            if (surface.drawableObjects[index].id === surface.selectedDrawableObjectId) {
                return surface.drawableObjects[index];
            }
        }
        return null;
    }

    function hasSelectedDrawableObject() {
        return selectedDrawableObject() !== null;
    }

    function drawableObjectIsTransformable(drawableObject) {
        if (!drawableObject) {
            return false;
        }
        return drawableObject.type === "image" || drawableObject.type === "text" || drawableObject.type === "shape";
    }

    function hasTransformableSelectedDrawableObject() {
        return drawableObjectIsTransformable(selectedDrawableObject());
    }

    function deleteSelectedDrawableObject() {
        const selectedObjectId = surface.selectedDrawableObjectId;
        if (selectedObjectId < 0) {
            return false;
        }

        const nextObjects = [];
        var removed = false;
        var removedIndex = -1;
        var removedObject = null;
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (drawableObject.id === selectedObjectId) {
                removed = true;
                removedIndex = index;
                removedObject = drawableObject;
                continue;
            }
            nextObjects.push(drawableObject);
        }

        if (!removed) {
            surface.selectedDrawableObjectId = -1;
            resetDrawableObjectTransform();
            return false;
        }

        surface.drawableObjects = nextObjects;
        if (removedIndex >= 0) {
            const previousPersistRasterLayerSnapshots = surface.persistRasterLayerSnapshots;
            if (removedObject && removedObject.type === "layer") {
                surface.persistRasterLayerSnapshots = false;
                delete surface.rasterLayerSnapshotSources[String(selectedObjectId)];
                clearRasterLayerThumbnailState(selectedObjectId);
            }
            invalidateDrawableObjectThumbnailSource(selectedObjectId);
            drawableObjectVisualModel.remove(removedIndex);
            surface.persistRasterLayerSnapshots = previousPersistRasterLayerSnapshots;
        }
        surface.selectedDrawableObjectId = -1;
        resetDrawableObjectTransform();
        return true;
    }

    function selectedDrawableObjectProperty(propertyName, fallbackValue) {
        const drawableObject = selectedDrawableObject();
        return drawableObject ? drawableObject[propertyName] : fallbackValue;
    }

    function drawableObjectIndexAt(pointX, pointY) {
        for (let index = surface.drawableObjects.length - 1; index >= 0; --index) {
            const drawableObject = surface.drawableObjects[index];
            if (!drawableObjectIsTransformable(drawableObject)) {
                continue;
            }
            if (pointX >= drawableObject.x && pointX <= drawableObject.x + drawableObject.width && pointY >= drawableObject.y && pointY <= drawableObject.y + drawableObject.height) {
                return index;
            }
        }
        return -1;
    }

    function drawableObjectHandleAt(pointX, pointY) {
        const drawableObject = selectedDrawableObject();
        if (!drawableObjectIsTransformable(drawableObject)) {
            return "";
        }

        const halfHitSize = surface.drawableObjectHandleHitSize / 2;
        const halfVisibleSize = surface.drawableObjectHandleSize / 2;
        const insideObject = pointX > drawableObject.x && pointX < drawableObject.x + drawableObject.width && pointY > drawableObject.y && pointY < drawableObject.y + drawableObject.height;
        for (let index = 0; index < surface.drawableObjectHandles.length; ++index) {
            const handle = surface.drawableObjectHandles[index];
            const handleX = drawableObject.x + drawableObject.width * handle.xRatio;
            const handleY = drawableObject.y + drawableObject.height * handle.yRatio;
            const insideHandleHitTarget = pointX >= handleX - halfHitSize && pointX <= handleX + halfHitSize && pointY >= handleY - halfHitSize && pointY <= handleY + halfHitSize;
            const insideVisibleHandle = pointX >= handleX - halfVisibleSize && pointX <= handleX + halfVisibleSize && pointY >= handleY - halfVisibleSize && pointY <= handleY + halfVisibleSize;
            if (insideHandleHitTarget && (!insideObject || insideVisibleHandle)) {
                return handle.mode;
            }
        }
        return "";
    }

    function updateDrawableObjectHoverHandle(pointX, pointY) {
        surface.drawableObjectHoverHandleMode = drawableObjectHandleAt(pointX, pointY);
    }

    function drawableObjectResizeCursor(handleMode) {
        if (handleMode === "resize-nw" || handleMode === "resize-se") {
            return Qt.SizeFDiagCursor;
        }
        if (handleMode === "resize-ne" || handleMode === "resize-sw") {
            return Qt.SizeBDiagCursor;
        }
        if (handleMode === "resize-n" || handleMode === "resize-s") {
            return Qt.SizeVerCursor;
        }
        if (handleMode === "resize-e" || handleMode === "resize-w") {
            return Qt.SizeHorCursor;
        }
        return Qt.SizeAllCursor;
    }

    function resetDrawableObjectTransform() {
        surface.drawableObjectTransformActive = false;
        surface.drawableObjectTransformMode = "";
        surface.drawableObjectTransformOriginal = null;
        surface.drawableObjectHoverHandleMode = "";
    }

    function cancelActiveDrawableObjectTransform() {
        if (surface.drawableObjectTransformActive && surface.drawableObjectTransformOriginal) {
            replaceDrawableObjectById(surface.drawableObjectTransformOriginal.id, surface.drawableObjectTransformOriginal);
        }
        resetDrawableObjectTransform();
    }

    function beginDrawableObjectTransform(pointX, pointY) {
        if (surface.toolMode !== "move") {
            return false;
        }

        commitActiveText();
        cancelActiveShape();
        const handleMode = drawableObjectHandleAt(pointX, pointY);
        if (handleMode.length > 0) {
            surface.drawableObjectTransformMode = handleMode;
        } else {
            const objectIndex = drawableObjectIndexAt(pointX, pointY);
            if (objectIndex < 0) {
                surface.selectedDrawableObjectId = -1;
                resetDrawableObjectTransform();
                return false;
            }
            surface.selectedDrawableObjectId = surface.drawableObjects[objectIndex].id;
            surface.drawableObjectTransformMode = "move";
        }

        const drawableObject = selectedDrawableObject();
        if (!drawableObjectIsTransformable(drawableObject)) {
            resetDrawableObjectTransform();
            return false;
        }

        surface.drawableObjectTransformStartX = pointX;
        surface.drawableObjectTransformStartY = pointY;
        surface.drawableObjectTransformOriginal = cloneDrawableObject(drawableObject);
        surface.drawableObjectTransformActive = true;
        return true;
    }

    function constrainedDrawableObjectDelta(pointX, pointY) {
        const deltaX = pointX - surface.drawableObjectTransformStartX;
        const deltaY = pointY - surface.drawableObjectTransformStartY;
        if (Math.abs(deltaX) >= Math.abs(deltaY)) {
            return {
                x: deltaX,
                y: 0
            };
        }
        return {
            x: 0,
            y: deltaY
        };
    }

    function drawableObjectDelta(pointX, pointY, constrained) {
        if (constrained) {
            return constrainedDrawableObjectDelta(pointX, pointY);
        }
        return {
            x: pointX - surface.drawableObjectTransformStartX,
            y: pointY - surface.drawableObjectTransformStartY
        };
    }

    function movedDrawableObject(originalObject, pointX, pointY, constrained) {
        const delta = drawableObjectDelta(pointX, pointY, constrained);
        const movedObject = cloneDrawableObject(originalObject);
        movedObject.x = originalObject.x + delta.x;
        movedObject.y = originalObject.y + delta.y;
        return movedObject;
    }

    function resizedDrawableObjectBounds(originalObject, pointX, pointY) {
        const deltaX = pointX - surface.drawableObjectTransformStartX;
        const deltaY = pointY - surface.drawableObjectTransformStartY;
        var left = originalObject.x;
        var top = originalObject.y;
        var right = originalObject.x + originalObject.width;
        var bottom = originalObject.y + originalObject.height;
        const resizeDirections = surface.drawableObjectTransformMode.slice("resize-".length);

        if (resizeDirections.indexOf("w") >= 0) {
            left = Math.min(right - surface.drawableObjectMinimumDimension, originalObject.x + deltaX);
        }
        if (resizeDirections.indexOf("e") >= 0) {
            right = Math.max(left + surface.drawableObjectMinimumDimension, originalObject.x + originalObject.width + deltaX);
        }
        if (resizeDirections.indexOf("n") >= 0) {
            top = Math.min(bottom - surface.drawableObjectMinimumDimension, originalObject.y + deltaY);
        }
        if (resizeDirections.indexOf("s") >= 0) {
            bottom = Math.max(top + surface.drawableObjectMinimumDimension, originalObject.y + originalObject.height + deltaY);
        }

        return {
            left: left,
            top: top,
            right: right,
            bottom: bottom,
            resizeDirections: resizeDirections
        };
    }

    function aspectLockedDrawableObjectResizeBounds(originalObject, pointX, pointY) {
        const freeBounds = resizedDrawableObjectBounds(originalObject, pointX, pointY);
        const resizeDirections = freeBounds.resizeDirections;
        const originalWidth = Math.max(surface.drawableObjectMinimumDimension, originalObject.width);
        const originalHeight = Math.max(surface.drawableObjectMinimumDimension, originalObject.height);
        const minimumScale = Math.max(surface.drawableObjectMinimumDimension / originalWidth, surface.drawableObjectMinimumDimension / originalHeight);
        const freeWidth = Math.max(surface.drawableObjectMinimumDimension, freeBounds.right - freeBounds.left);
        const freeHeight = Math.max(surface.drawableObjectMinimumDimension, freeBounds.bottom - freeBounds.top);
        const widthScale = freeWidth / originalWidth;
        const heightScale = freeHeight / originalHeight;
        var scale = widthScale;

        if (resizeDirections.indexOf("n") >= 0 || resizeDirections.indexOf("s") >= 0) {
            if (resizeDirections.indexOf("e") < 0 && resizeDirections.indexOf("w") < 0) {
                scale = heightScale;
            } else if (Math.abs(heightScale - 1) > Math.abs(widthScale - 1)) {
                scale = heightScale;
            }
        }
        scale = Math.max(minimumScale, scale);

        const lockedWidth = originalWidth * scale;
        const lockedHeight = originalHeight * scale;
        const originalRight = originalObject.x + originalObject.width;
        const originalBottom = originalObject.y + originalObject.height;
        const originalCenterX = originalObject.x + originalObject.width / 2;
        const originalCenterY = originalObject.y + originalObject.height / 2;
        var left = originalObject.x;
        var top = originalObject.y;

        if (resizeDirections.indexOf("w") >= 0) {
            left = originalRight - lockedWidth;
        } else if (resizeDirections.indexOf("e") < 0) {
            left = originalCenterX - lockedWidth / 2;
        }

        if (resizeDirections.indexOf("n") >= 0) {
            top = originalBottom - lockedHeight;
        } else if (resizeDirections.indexOf("s") < 0) {
            top = originalCenterY - lockedHeight / 2;
        }

        return {
            left: left,
            top: top,
            right: left + lockedWidth,
            bottom: top + lockedHeight
        };
    }

    function resizedDrawableObject(originalObject, pointX, pointY, aspectLocked) {
        const bounds = aspectLocked ? aspectLockedDrawableObjectResizeBounds(originalObject, pointX, pointY) : resizedDrawableObjectBounds(originalObject, pointX, pointY);

        const resizedObject = cloneDrawableObject(originalObject);
        resizedObject.x = bounds.left;
        resizedObject.y = bounds.top;
        resizedObject.width = bounds.right - bounds.left;
        resizedObject.height = bounds.bottom - bounds.top;
        return resizedObject;
    }

    function updateDrawableObjectTransform(pointX, pointY, constrained) {
        if (!surface.drawableObjectTransformActive || !surface.drawableObjectTransformOriginal) {
            return;
        }

        const nextObject = surface.drawableObjectTransformMode === "move" ? movedDrawableObject(surface.drawableObjectTransformOriginal, pointX, pointY, constrained) : resizedDrawableObject(surface.drawableObjectTransformOriginal, pointX, pointY, constrained);
        replaceDrawableObjectById(nextObject.id, nextObject);
    }

    function commitDrawableObjectTransform() {
        const drawableObject = selectedDrawableObject();
        if (surface.hostCanvasInputClient && !surface.applyingHostEditCommand && drawableObject && surface.drawableObjectTransformOriginal) {
            const transformedObject = cloneDrawableObject(drawableObject);
            const authoritativeObject = cloneDrawableObject(surface.drawableObjectTransformOriginal);
            replaceDrawableObjectById(authoritativeObject.id, authoritativeObject);
            const requested = surface.requestHostEditCommand({
                type: "transform-object",
                objectId: transformedObject.id,
                x: transformedObject.x,
                y: transformedObject.y,
                width: transformedObject.width,
                height: transformedObject.height
            });
            resetDrawableObjectTransform();
            return requested;
        }
        if (drawableObject) {
            refreshDrawableObjectThumbnailSource(drawableObject);
            rebuildLayerHierarchyRows();
        }
        resetDrawableObjectTransform();
        return drawableObject !== null;
    }

    function fillAt(pointX, pointY) {
        if (surface.toolMode !== "fill") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        if (surface.requestHostEditCommand({
            type: "fill",
            layerKey: surface.currentLayerKey(),
            x: Number(pointX),
            y: Number(pointY),
            color: surface.brushColor.toString()
        })) {
            return true;
        }
        const rasterSurface = activeRasterSurface();
        if (!rasterSurface) {
            return;
        }
        if (rasterSurface.fillAt(pointX, pointY, surface.brushColor)) {
            surface.sessionChanged();
            return true;
        }
        return false;
    }

    function resetCanvasPan() {
        surface.canvasPanOffsetX = 0;
        surface.canvasPanOffsetY = 0;
        surface.panDraggingActive = false;
    }

    function beginPanDrag(pointX, pointY) {
        if (surface.effectiveToolMode() !== "pan") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();
        commitZoomDrag();
        surface.panDragStartX = pointX;
        surface.panDragStartY = pointY;
        surface.panDragStartOffsetX = surface.canvasPanOffsetX;
        surface.panDragStartOffsetY = surface.canvasPanOffsetY;
        surface.panDraggingActive = true;
    }

    function updatePanDrag(pointX, pointY) {
        if (!surface.panDraggingActive) {
            return;
        }

        surface.canvasPanOffsetX = surface.panDragStartOffsetX + pointX - surface.panDragStartX;
        surface.canvasPanOffsetY = surface.panDragStartOffsetY + pointY - surface.panDragStartY;
        surface.ensureInfiniteCanvasForViewport();
    }

    function commitPanDrag() {
        surface.panDraggingActive = false;
    }

    function cancelPanDrag() {
        if (!surface.panDraggingActive) {
            return;
        }
        surface.canvasPanOffsetX = surface.panDragStartOffsetX;
        surface.canvasPanOffsetY = surface.panDragStartOffsetY;
        surface.panDraggingActive = false;
    }

    function boundedCanvasZoomScale(scaleValue) {
        const parsedScale = Number(scaleValue);
        if (!isFinite(parsedScale)) {
            return surface.defaultCanvasZoomScale;
        }
        return Math.max(surface.minimumCanvasZoomScale, Math.min(surface.maximumCanvasZoomScale, parsedScale));
    }

    function wheelZoomFactor(angleDeltaY, pixelDeltaY) {
        const normalizedAngleDelta = Number(angleDeltaY);
        const normalizedPixelDelta = Number(pixelDeltaY);
        let wheelSteps = 0;
        if (isFinite(normalizedAngleDelta) && normalizedAngleDelta !== 0) {
            wheelSteps = normalizedAngleDelta / 120;
        } else if (isFinite(normalizedPixelDelta) && normalizedPixelDelta !== 0) {
            wheelSteps = normalizedPixelDelta / 40;
        }
        const boundedWheelSteps = Math.max(-4, Math.min(4, wheelSteps));
        return Math.pow(1.12, boundedWheelSteps);
    }

    function zoomCanvas(zoomFactor) {
        const normalizedZoomFactor = Number(zoomFactor);
        if (!isFinite(normalizedZoomFactor) || normalizedZoomFactor <= 0) {
            return false;
        }
        if (surface.presentationMode) {
            return surface.zoomPresentationCanvas(normalizedZoomFactor);
        }

        surface.zoomDraggingActive = false;
        surface.canvasZoomScale = surface.boundedCanvasZoomScale(surface.canvasZoomScale * normalizedZoomFactor);
        surface.ensureInfiniteCanvasForViewport();
        return true;
    }

    function zoomCanvasFromWheel(angleDeltaY, pixelDeltaY) {
        const zoomFactor = surface.wheelZoomFactor(angleDeltaY, pixelDeltaY);
        if (zoomFactor === 1) {
            return false;
        }
        return surface.zoomCanvas(zoomFactor);
    }

    function handleCanvasWheel(wheel) {
        const handled = surface.zoomCanvasFromWheel(wheel.angleDelta.y, wheel.pixelDelta.y);
        wheel.accepted = handled;
    }

    function fittedCanvasZoomScale(canvasWidth, canvasHeight) {
        const normalizedWidth = Math.max(surface.minimumCanvasDimension, Number(canvasWidth));
        const normalizedHeight = Math.max(surface.minimumCanvasDimension, Number(canvasHeight));
        if (!isFinite(normalizedWidth) || !isFinite(normalizedHeight)) {
            return surface.defaultCanvasZoomScale;
        }

        return boundedCanvasZoomScale(Math.min(surface.defaultCanvasZoomScale, surface.workspaceCanvasWidth / normalizedWidth, surface.workspaceCanvasHeight / normalizedHeight));
    }

    function fitCanvasZoomToCurrentCanvas() {
        surface.zoomDraggingActive = false;
        surface.canvasZoomScale = fittedCanvasZoomScale(canvasSurface.width, canvasSurface.height);
    }

    function fittedPresentationCanvasZoomScale(canvasWidth, canvasHeight) {
        const normalizedWidth = Math.max(surface.minimumCanvasDimension, Number(canvasWidth));
        const normalizedHeight = Math.max(surface.minimumCanvasDimension, Number(canvasHeight));
        if (!isFinite(normalizedWidth) || !isFinite(normalizedHeight)) {
            return surface.defaultCanvasZoomScale;
        }

        return Math.max(surface.minimumCanvasZoomScale, Math.min(surface.workspaceCanvasWidth / normalizedWidth, surface.workspaceCanvasHeight / normalizedHeight));
    }

    function fitCanvasZoomToPresentationViewport() {
        if (!surface.presentationMode) {
            return;
        }
        surface.resetCanvasPan();
        surface.zoomDraggingActive = false;
        const fittedScale = fittedPresentationCanvasZoomScale(canvasSurface.width, canvasSurface.height);
        surface.presentationFittedCanvasZoomScale = fittedScale;
        surface.canvasZoomScale = fittedScale;
    }

    function zoomPresentationCanvas(zoomFactor) {
        if (!surface.presentationMode) {
            return false;
        }
        const normalizedZoomFactor = Number(zoomFactor);
        if (!isFinite(normalizedZoomFactor) || normalizedZoomFactor <= 0) {
            return false;
        }

        const minimumScale = Math.max(surface.minimumCanvasZoomScale, surface.presentationFittedCanvasZoomScale * surface.presentationMinimumZoomMultiplier);
        const maximumScale = Math.max(minimumScale, surface.presentationFittedCanvasZoomScale * surface.presentationMaximumZoomMultiplier);
        surface.canvasPanOffsetX = 0;
        surface.canvasPanOffsetY = 0;
        surface.panDraggingActive = false;
        surface.zoomDraggingActive = false;
        surface.canvasZoomScale = Math.max(minimumScale, Math.min(maximumScale, surface.canvasZoomScale * normalizedZoomFactor));
        return true;
    }

    function enterPresentationMode() {
        if (surface.presentationMode) {
            return;
        }

        surface.commitActiveText();
        surface.commitActiveShape();
        surface.commitDrawableObjectTransform();
        surface.presentationPreviousCanvasZoomScale = surface.canvasZoomScale;
        surface.presentationPreviousCanvasPanOffsetX = surface.canvasPanOffsetX;
        surface.presentationPreviousCanvasPanOffsetY = surface.canvasPanOffsetY;
        surface.presentationMode = true;
        Qt.callLater(surface.fitCanvasZoomToPresentationViewport);
    }

    function refreshPresentationMode() {
        if (surface.presentationMode) {
            Qt.callLater(surface.fitCanvasZoomToPresentationViewport);
        }
    }

    function exitPresentationMode() {
        if (!surface.presentationMode) {
            return;
        }

        surface.presentationMode = false;
        surface.canvasZoomScale = surface.presentationPreviousCanvasZoomScale;
        surface.canvasPanOffsetX = surface.presentationPreviousCanvasPanOffsetX;
        surface.canvasPanOffsetY = surface.presentationPreviousCanvasPanOffsetY;
        surface.panDraggingActive = false;
        surface.zoomDraggingActive = false;
        surface.forceActiveFocus();
    }

    function beginZoomDrag(pointX) {
        if (surface.effectiveToolMode() !== "zoom") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();
        surface.zoomDragStartX = pointX;
        surface.zoomDragStartScale = surface.canvasZoomScale;
        surface.zoomDraggingActive = true;
    }

    function updateZoomDrag(pointX) {
        if (!surface.zoomDraggingActive) {
            return;
        }

        const deltaX = pointX - surface.zoomDragStartX;
        surface.canvasZoomScale = boundedCanvasZoomScale(surface.zoomDragStartScale * Math.pow(2, deltaX / surface.zoomDragPixelsPerDoubling));
        surface.ensureInfiniteCanvasForViewport();
    }

    function commitZoomDrag() {
        surface.zoomDraggingActive = false;
    }

    function cancelZoomDrag() {
        if (!surface.zoomDraggingActive) {
            return;
        }
        surface.canvasZoomScale = surface.zoomDragStartScale;
        surface.zoomDraggingActive = false;
    }

    function canvasMouseAcceptedButtons() {
        const mode = surface.effectiveToolMode();
        if (mode === "shape" || mode === "move" || mode === "fill" || mode === "text") {
            return Qt.LeftButton;
        }
        return Qt.NoButton;
    }

    function canvasCursorShape(pointX, pointY) {
        const mode = surface.effectiveToolMode();
        if (mode === "brush" || mode === "eraser") {
            if (!surface.hasActiveRasterSurface()) {
                return Qt.ArrowCursor;
            }
            return surface.brushCursorUsesSystemFallback ? Qt.CrossCursor : Qt.BlankCursor;
        }
        if (mode === "zoom" || mode === "shape") {
            return Qt.CrossCursor;
        }
        if (mode === "text") {
            return Qt.IBeamCursor;
        }
        if (mode === "pan") {
            return surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
        }
        if (mode === "move") {
            if (surface.drawableObjectTransformActive) {
                if (surface.drawableObjectTransformMode === "move") {
                    return Qt.SizeAllCursor;
                }
                if (surface.drawableObjectTransformMode.length > 0) {
                    return surface.drawableObjectResizeCursor(surface.drawableObjectTransformMode);
                }
            }

            if (pointX === undefined || pointY === undefined) {
                return Qt.ArrowCursor;
            }

            const handleMode = surface.drawableObjectHandleAt(pointX, pointY);
            if (handleMode.length > 0) {
                return surface.drawableObjectResizeCursor(handleMode);
            }
            return surface.drawableObjectIndexAt(pointX, pointY) >= 0 ? Qt.SizeAllCursor : Qt.ArrowCursor;
        }
        if (mode === "fill") {
            return Qt.PointingHandCursor;
        }
        return Qt.ArrowCursor;
    }

    function cancelActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        surface.shapeDraggingActive = false;
        surface.shapeAspectLocked = false;
        requestShapePreviewPaint();
    }

    function traceEllipsePath(context, x, y, widthValue, heightValue) {
        const kappa = 0.5522847498;
        const ox = widthValue / 2 * kappa;
        const oy = heightValue / 2 * kappa;
        const xe = x + widthValue;
        const ye = y + heightValue;
        const xm = x + widthValue / 2;
        const ym = y + heightValue / 2;

        context.moveTo(x, ym);
        context.bezierCurveTo(x, ym - oy, xm - ox, y, xm, y);
        context.bezierCurveTo(xm + ox, y, xe, ym - oy, xe, ym);
        context.bezierCurveTo(xe, ym + oy, xm + ox, ye, xm, ye);
        context.bezierCurveTo(xm - ox, ye, x, ym + oy, x, ym);
        context.closePath();
    }

    function traceStarPath(context, x, y, widthValue, heightValue) {
        const centerX = x + widthValue / 2;
        const centerY = y + heightValue / 2;
        const outerRadiusX = widthValue / 2;
        const outerRadiusY = heightValue / 2;
        const innerRadiusX = outerRadiusX * 0.45;
        const innerRadiusY = outerRadiusY * 0.45;

        for (let index = 0; index < 10; ++index) {
            const outerPoint = index % 2 === 0;
            const radiusX = outerPoint ? outerRadiusX : innerRadiusX;
            const radiusY = outerPoint ? outerRadiusY : innerRadiusY;
            const angle = -Math.PI / 2 + index * Math.PI / 5;
            const px = centerX + Math.cos(angle) * radiusX;
            const py = centerY + Math.sin(angle) * radiusY;
            if (index === 0) {
                context.moveTo(px, py);
            } else {
                context.lineTo(px, py);
            }
        }
        context.closePath();
    }

    function speechBubbleTailHeight(heightValue) {
        return Math.min(Math.max(surface.speechBubbleTailMinimumHeight, heightValue * surface.speechBubbleTailHeightRatio), heightValue * surface.speechBubbleTailMaximumHeightRatio);
    }

    function speechBubbleBodyHeight(heightValue) {
        return Math.max(surface.shapeToolMinimumDragDistance, heightValue - speechBubbleTailHeight(heightValue));
    }

    function ellipsePoint(x, y, widthValue, heightValue, angle) {
        return {
            x: x + widthValue / 2 + Math.cos(angle) * widthValue / 2,
            y: y + heightValue / 2 + Math.sin(angle) * heightValue / 2
        };
    }

    function traceEllipseArcPath(context, x, y, widthValue, heightValue, startAngle, endAngle) {
        for (let index = 1; index <= surface.ellipseBubbleArcSegmentCount; ++index) {
            const angle = startAngle + (endAngle - startAngle) * index / surface.ellipseBubbleArcSegmentCount;
            const point = ellipsePoint(x, y, widthValue, heightValue, angle);
            context.lineTo(point.x, point.y);
        }
    }

    function traceRectangleBubblePath(context, x, y, widthValue, heightValue) {
        const bodyHeight = speechBubbleBodyHeight(heightValue);
        const bodyBottom = y + bodyHeight;
        const tailLeftX = x + widthValue * surface.speechBubbleTailLeftBaseXRatio;
        const tailTipX = x + widthValue * surface.speechBubbleTailTipXRatio;
        const tailRightX = x + widthValue * surface.speechBubbleTailRightBaseXRatio;

        context.moveTo(x, y);
        context.lineTo(x + widthValue, y);
        context.lineTo(x + widthValue, bodyBottom);
        context.lineTo(tailRightX, bodyBottom);
        context.lineTo(tailTipX, y + heightValue);
        context.lineTo(tailLeftX, bodyBottom);
        context.lineTo(x, bodyBottom);
        context.closePath();
    }

    function traceEllipseBubblePath(context, x, y, widthValue, heightValue) {
        const bodyHeight = speechBubbleBodyHeight(heightValue);
        const tailLeftPoint = ellipsePoint(x, y, widthValue, bodyHeight, surface.ellipseBubbleTailLeftAngle);
        const tailTipX = x + widthValue * surface.speechBubbleTailTipXRatio;

        context.moveTo(tailLeftPoint.x, tailLeftPoint.y);
        traceEllipseArcPath(context, x, y, widthValue, bodyHeight, surface.ellipseBubbleTailLeftAngle, surface.ellipseBubbleTailRightAngle + Math.PI * 2);
        context.lineTo(tailTipX, y + heightValue);
        context.closePath();
    }

    function traceShapePath(context, shapeValue, x, y, widthValue, heightValue) {
        const shape = shapeValue === "triagle" ? "triangle" : shapeValue;
        if (shape === "ellipse") {
            traceEllipsePath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "triangle") {
            context.moveTo(x + widthValue / 2, y);
            context.lineTo(x + widthValue, y + heightValue);
            context.lineTo(x, y + heightValue);
            context.closePath();
            return;
        }
        if (shape === "diamond") {
            context.moveTo(x + widthValue / 2, y);
            context.lineTo(x + widthValue, y + heightValue / 2);
            context.lineTo(x + widthValue / 2, y + heightValue);
            context.lineTo(x, y + heightValue / 2);
            context.closePath();
            return;
        }
        if (shape === "star") {
            traceStarPath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "rectanglebubble") {
            traceRectangleBubblePath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "ellipsebubble") {
            traceEllipseBubblePath(context, x, y, widthValue, heightValue);
            return;
        }

        context.rect(x, y, widthValue, heightValue);
    }

    function paintShapePreview(context, previewWidth, previewHeight) {
        context.clearRect(0, 0, previewWidth, previewHeight);
        if (!surface.shapeDraggingActive) {
            return;
        }

        context.beginPath();
        traceShapePath(context, surface.shapeKind, 0, 0, previewWidth, previewHeight);
        context.fillStyle = surface.brushColor.toString();
        context.fill();
    }

    onToolModeChanged: {
        surface.drawableObjectHoverHandleMode = "";
        if (toolMode !== "text") {
            commitActiveText();
        }
        if (toolMode !== "shape") {
            cancelActiveShape();
        }
        if (toolMode !== "move") {
            resetDrawableObjectTransform();
        }
        if (toolMode !== "pan") {
            commitPanDrag();
        }
        if (toolMode !== "zoom") {
            commitZoomDrag();
        }
    }

    onTextEditingActiveChanged: {
        if (textEditingActive) {
            endTemporaryCameraMode();
        }
    }

    onToolShortcutsEnabledChanged: {
        if (!toolShortcutsEnabled) {
            endTemporaryCameraMode();
        }
    }

    onActiveFocusChanged: {
        if (!activeFocus) {
            endTemporaryCameraMode();
        }
    }

    Keys.onPressed: function (event) {
        if (event.isAutoRepeat) {
            return;
        }
        if (event.key === Qt.Key_Space && surface.setTemporaryCameraMode(surface.temporaryCameraModeForModifiers(event.modifiers))) {
            event.accepted = true;
            return;
        }
        if (surface.temporaryCameraMode !== "" && (event.key === Qt.Key_Control || event.key === Qt.Key_Meta) && surface.beginSpaceZoomMode()) {
            event.accepted = true;
        }
    }

    Keys.onReleased: function (event) {
        if (event.isAutoRepeat) {
            return;
        }
        if (event.key === Qt.Key_Space && surface.endTemporaryCameraMode()) {
            event.accepted = true;
            return;
        }
        if (surface.temporaryCameraMode !== "" && (event.key === Qt.Key_Control || event.key === Qt.Key_Meta) && surface.setTemporaryCameraMode(surface.temporaryCameraModeForModifiers(event.modifiers))) {
            event.accepted = true;
        }
    }

    onWidthChanged: {
        if (surface.presentationMode) {
            surface.refreshPresentationMode();
            return;
        }
        if (!surface.canvasSizeCreated) {
            syncCanvasItemSizeToWorkspace();
        }
    }

    onHeightChanged: {
        if (surface.presentationMode) {
            surface.refreshPresentationMode();
            return;
        }
        if (!surface.canvasSizeCreated) {
            syncCanvasItemSizeToWorkspace();
        }
    }

    Item {
        id: canvasViewport
        objectName: "canvasViewport"
        x: surface.workspaceCanvasHorizontalInset
        y: surface.workspaceCanvasTopInset
        width: surface.workspaceCanvasWidth
        height: surface.workspaceCanvasHeight

        Rectangle {
            id: canvasPaper
            objectName: "canvasPaper"
            x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)
            y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)
            width: canvasSurface.width
            height: canvasSurface.height
            transformOrigin: Item.Center
            scale: surface.canvasZoomScale
            color: surface.backgroundLayerPresent ? surface.canvasColor : "transparent"
            border.color: surface.imageDropActive ? surface.textToolAccentColor : "#b8bcc4"
            border.width: surface.imageDropActive ? 2 : (canvasPaper.width < surface.width || canvasPaper.height < surface.height ? 1 : 0)

            Image {
                objectName: "transparencyGridBackground"
                anchors.fill: parent
                visible: !surface.backgroundLayerPresent
                source: surface.transparencyGridTileSource
                fillMode: Image.Tile
                smooth: false
                cache: true
            }
        }

        DrawingSurfaceItem {
            id: canvasSurface
            objectName: "canvasSurface"
            x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)
            y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)
            z: 1
            width: 1
            height: 1
            clip: true
            transformOrigin: Item.Center
            scale: surface.canvasZoomScale
            brushColor: surface.brushColor
            brushSize: surface.brushSize
            brushFlow: surface.brushFlow
            brushOpacity: surface.brushOpacity
            brushHardness: surface.brushHardness
            brushSpacing: surface.brushSpacing
            brushSpacingRatio: surface.brushSpacingRatio
            livePreviewFrameIntervalMs: surface.brushLivePreviewFrameIntervalMs
            pressureCurveMinimum: surface.pressureCurveMinimum
            pressureCurveCenter: surface.pressureCurveCenter
            pressureCurveMaximum: surface.pressureCurveMaximum
            brushOpacityEnabled: surface.brushPressureControlsOpacity
            stabilizerStrength: surface.stabilizerStrength
            toolMode: surface.backgroundLayerPresent && !surface.rasterLayerObjectSelected() ? surface.effectiveToolMode() : "move"
            documentViewModel: surface.documentViewModel
            viewId: surface.viewId
            remoteInputMode: surface.hostCanvasInputClient

            Component.onCompleted: {
                surface.canvasItemReady = true;
                surface.syncCanvasItemSizeToWorkspace();
                surface.addDefaultDrawingLayer();
                surface.refreshBackgroundLayerThumbnailSource();
                surface.rebuildLayerHierarchyRows();
            }

            onRasterContentChanged: {
                surface.requestBackgroundLayerThumbnailRefresh();
            }
            onStrokeCountChanged: {
                if (!surface.recentCanvasRestoreInProgress) {
                    surface.sessionChanged();
                }
            }
            onDroppedImageReady: (imageObject, dropX, dropY) => surface.insertDroppedImageObject(imageObject, dropX, dropY)
            onDroppedImageFailed: errorCode => surface.imageDropFailed(errorCode)
            onRemoteStrokeRequested: (points, pressureSensitive) => surface.requestRemoteStroke("raster-canvas", points, pressureSensitive)

            DropArea {
                id: canvasImageDropArea
                objectName: "canvasImageDropArea"
                parent: canvasSurface
                anchors.fill: parent
                z: 20

                onEntered: function (drag) {
                    const accepted = canvasSurface.canImportDroppedImage(drag);
                    drag.accepted = accepted;
                    if (accepted) {
                        drag.acceptProposedAction();
                    }
                }

                onDropped: function (drop) {
                    if (!canvasSurface.canImportDroppedImage(drop)) {
                        drop.accepted = false;
                        return;
                    }
                    canvasSurface.importDroppedImage(drop, Math.max(1, canvasSurface.width * surface.imageInsertionMaximumCanvasRatio), Math.max(1, canvasSurface.height * surface.imageInsertionMaximumCanvasRatio));
                    drop.acceptProposedAction();
                }
            }
        }

        Timer {
            id: layerThumbnailRefreshTimer
            interval: surface.layerHierarchyThumbnailRefreshDelayMs
            repeat: false
            onTriggered: surface.flushPendingLayerThumbnailRefreshes()
        }

        Rectangle {
            id: textToolEditorFrame
            parent: canvasSurface
            visible: surface.textEditingActive
            z: 4
            width: surface.textToolResponsiveWidth()
            height: Math.max(surface.textToolFontPixelSize + surface.textToolFramePadding * 2, textToolEditor.contentHeight + surface.textToolFramePadding * 2)
            color: Qt.rgba(255, 255, 255, 0.88)
            border.width: 1
            border.color: surface.textToolAccentColor

            TextMetrics {
                id: textToolLineMetrics
                font.pixelSize: surface.textToolFontPixelSize
                text: surface.longestTextLine(textToolEditor.text)
            }

            TextEdit {
                id: textToolEditor
                objectName: "textToolEditor"
                anchors.fill: parent
                anchors.margins: surface.textToolFramePadding
                visible: surface.textEditingActive
                color: surface.brushColor
                selectionColor: surface.textToolAccentColor
                selectedTextColor: "#ffffff"
                font.pixelSize: surface.textToolFontPixelSize
                wrapMode: TextEdit.Wrap
                focus: surface.textEditingActive
                selectByMouse: true

                onActiveFocusChanged: {
                    if (!activeFocus && surface.textEditingActive) {
                        surface.commitActiveText();
                    }
                }

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Escape) {
                        surface.cancelActiveText();
                        event.accepted = true;
                        return;
                    }

                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))) {
                        surface.commitActiveText();
                        event.accepted = true;
                    }
                }
            }
        }

        Canvas {
            id: shapePreviewCanvas
            parent: canvasSurface
            visible: surface.shapeDraggingActive
            z: 4
            x: surface.shapePreviewRect().x
            y: surface.shapePreviewRect().y
            width: Math.max(1, surface.shapePreviewRect().width)
            height: Math.max(1, surface.shapePreviewRect().height)
            renderTarget: Canvas.Image
            opacity: 0.9

            onPaint: {
                const context = getContext("2d");
                surface.paintShapePreview(context, width, height);
            }
            onVisibleChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        ListModel {
            id: drawableObjectVisualModel
        }

        Repeater {
            parent: canvasSurface
            model: drawableObjectVisualModel

            delegate: Item {
                id: drawableObjectDelegate
                required property int objectId
                required property string objectType
                required property real objectX
                required property real objectY
                required property real objectWidth
                required property real objectHeight
                required property string objectSource
                required property string objectShapeKind
                required property string objectColor
                required property string objectText
                required property real objectFontPixelSize
                required property real objectOpacity
                required property bool objectVisible
                required property string objectBlendMode
                readonly property bool isRasterLayer: objectType === "layer"
                readonly property int rasterLayerObjectId: objectId

                z: isRasterLayer && surface.selectedDrawableObjectId === rasterLayerObjectId && (surface.effectiveToolMode() === "brush" || surface.effectiveToolMode() === "eraser") ? 4 : 2
                x: isRasterLayer ? 0 : objectX
                y: isRasterLayer ? 0 : objectY
                width: isRasterLayer ? canvasSurface.width : Math.max(1, objectWidth)
                height: isRasterLayer ? canvasSurface.height : Math.max(1, objectHeight)
                visible: objectVisible
                opacity: objectOpacity

                Loader {
                    id: rasterLayerLoader
                    anchors.fill: parent
                    active: drawableObjectDelegate.isRasterLayer
                    sourceComponent: rasterLayerSurfaceComponent

                    onLoaded: {
                        if (item) {
                            surface.registerRasterLayerItem(drawableObjectDelegate.rasterLayerObjectId, item);
                        }
                    }

                    Component.onDestruction: {
                        if (item) {
                            surface.unregisterRasterLayerItem(drawableObjectDelegate.rasterLayerObjectId, item);
                        }
                    }
                }

                Component {
                    id: rasterLayerSurfaceComponent

                    DrawingSurfaceItem {
                        anchors.fill: parent
                        enabled: surface.selectedDrawableObjectId === drawableObjectDelegate.rasterLayerObjectId && (surface.effectiveToolMode() === "brush" || surface.effectiveToolMode() === "eraser")
                        objectName: "rasterLayerSurface-" + drawableObjectDelegate.rasterLayerObjectId
                        brushColor: surface.brushColor
                        brushSize: surface.brushSize
                        brushFlow: surface.brushFlow
                        brushOpacity: surface.brushOpacity
                        brushHardness: surface.brushHardness
                        brushSpacing: surface.brushSpacing
                        brushSpacingRatio: surface.brushSpacingRatio
                        livePreviewFrameIntervalMs: surface.brushLivePreviewFrameIntervalMs
                        pressureCurveMinimum: surface.pressureCurveMinimum
                        pressureCurveCenter: surface.pressureCurveCenter
                        pressureCurveMaximum: surface.pressureCurveMaximum
                        brushOpacityEnabled: surface.brushPressureControlsOpacity
                        stabilizerStrength: surface.stabilizerStrength
                        toolMode: surface.effectiveToolMode()
                        documentViewModel: surface.documentViewModel
                        viewId: surface.viewId
                        remoteInputMode: surface.hostCanvasInputClient

                        onRasterContentChanged: {
                            surface.requestRasterLayerThumbnailRefresh(drawableObjectDelegate.rasterLayerObjectId);
                        }
                        onStrokeCountChanged: {
                            if (!surface.recentCanvasRestoreInProgress) {
                                surface.sessionChanged();
                            }
                        }
                        onRemoteStrokeRequested: (points, pressureSensitive) => surface.requestRemoteStroke(surface.layerKeyForDrawableObjectId(drawableObjectDelegate.rasterLayerObjectId), points, pressureSensitive)
                    }
                }

                Image {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "image"
                    source: drawableObjectDelegate.objectType === "image" ? drawableObjectDelegate.objectSource : ""
                    fillMode: Image.Stretch
                    smooth: true
                }

                Canvas {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "shape"
                    renderTarget: Canvas.Image

                    onPaint: {
                        const context = getContext("2d");
                        context.clearRect(0, 0, width, height);
                        context.beginPath();
                        surface.traceShapePath(context, drawableObjectDelegate.objectShapeKind, 0, 0, width, height);
                        context.fillStyle = drawableObjectDelegate.objectColor;
                        context.fill();
                    }
                    onVisibleChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Component.onCompleted: requestPaint()
                }

                Text {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "text"
                    text: drawableObjectDelegate.objectText
                    color: drawableObjectDelegate.objectColor
                    font.pixelSize: drawableObjectDelegate.objectFontPixelSize
                    wrapMode: Text.Wrap
                    clip: true
                }
            }
        }

        Rectangle {
            id: drawableObjectSelectionFrame
            parent: canvasSurface
            visible: !surface.presentationMode && surface.effectiveToolMode() === "move" && surface.hasTransformableSelectedDrawableObject()
            z: 5
            x: surface.selectedDrawableObjectProperty("x", 0)
            y: surface.selectedDrawableObjectProperty("y", 0)
            width: Math.max(1, surface.selectedDrawableObjectProperty("width", 1))
            height: Math.max(1, surface.selectedDrawableObjectProperty("height", 1))
            color: "transparent"
            border.width: 1
            border.color: surface.textToolAccentColor

            Repeater {
                model: surface.drawableObjectHandles

                delegate: Rectangle {
                    required property var modelData
                    readonly property bool activeHandle: surface.drawableObjectHoverHandleMode === modelData.mode || (surface.drawableObjectTransformActive && surface.drawableObjectTransformMode === modelData.mode)

                    width: surface.drawableObjectHandleSize
                    height: surface.drawableObjectHandleSize
                    x: drawableObjectSelectionFrame.width * modelData.xRatio - width / 2
                    y: drawableObjectSelectionFrame.height * modelData.yRatio - height / 2
                    scale: activeHandle ? 1.2 : 1
                    color: surface.canvasColor
                    border.width: activeHandle ? 2 : 1
                    border.color: surface.textToolAccentColor
                }
            }
        }

        Item {
            id: brushCursorGlassPane
            objectName: "brushCursorGlassPane"
            parent: canvasSurface
            anchors.fill: parent
            z: 7
            visible: !surface.presentationMode

            WheelHandler {
                id: canvasWheelZoomHandler
                objectName: "canvasWheelZoomHandler"
                target: null
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                cursorShape: surface.canvasCursorShape()
                onWheel: function (wheel) {
                    surface.handleCanvasWheel(wheel);
                }
            }

            HoverHandler {
                id: brushCursorHoverHandler
                objectName: "brushCursorHoverHandler"
                blocking: false
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus | PointerDevice.Airbrush
                cursorShape: surface.canvasCursorShape(point.position.x, point.position.y)
            }

            PointHandler {
                id: brushCursorPointHandler
                objectName: "brushCursorPointHandler"
                enabled: surface.brushCursorToolActive
                acceptedButtons: Qt.NoButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus | PointerDevice.Airbrush
                cursorShape: surface.brushCursorUsesSystemFallback ? Qt.CrossCursor : Qt.BlankCursor
            }

            Item {
                id: brushCursorOutline
                objectName: "brushCursorOutline"
                parent: canvasViewport
                z: 8
                readonly property real cursorDiameter: surface.brushCursorScreenDiameter
                readonly property point cursorPosition: brushCursorPointHandler.active ? brushCursorPointHandler.point.position : brushCursorHoverHandler.point.position
                readonly property point cursorPositionInViewport: brushCursorGlassPane.mapToItem(canvasViewport, cursorPosition)

                x: cursorPositionInViewport.x - width / 2
                y: cursorPositionInViewport.y - height / 2
                width: cursorDiameter
                height: cursorDiameter
                visible: surface.brushCursorToolActive && (brushCursorPointHandler.active || brushCursorHoverHandler.hovered)

                Shapes.Shape {
                    id: brushCursorRingShape
                    objectName: "brushCursorRingShape"
                    anchors.centerIn: parent
                    width: brushCursorOutline.width + surface.brushCursorEffectiveOuterStrokeWidth
                    height: width
                    antialiasing: true

                    Shapes.ShapePath {
                        objectName: "brushCursorOuterRing"
                        fillColor: "transparent"
                        strokeColor: Qt.rgba(0, 0, 0, 0.82)
                        strokeWidth: surface.brushCursorEffectiveOuterStrokeWidth

                        PathAngleArc {
                            centerX: brushCursorRingShape.width / 2
                            centerY: brushCursorRingShape.height / 2
                            radiusX: brushCursorOutline.cursorDiameter / 2
                            radiusY: radiusX
                            startAngle: -90
                            sweepAngle: 360
                        }
                    }

                    Shapes.ShapePath {
                        objectName: "brushCursorInnerRing"
                        fillColor: "transparent"
                        strokeColor: Qt.rgba(1, 1, 1, 0.96)
                        strokeWidth: surface.brushCursorEffectiveInnerStrokeWidth

                        PathAngleArc {
                            centerX: brushCursorRingShape.width / 2
                            centerY: brushCursorRingShape.height / 2
                            radiusX: brushCursorOutline.cursorDiameter / 2
                            radiusY: radiusX
                            startAngle: -90
                            sweepAngle: 360
                        }
                    }
                }
            }
        }

        MouseArea {
            id: canvasPanMouseArea
            anchors.fill: parent
            z: 6
            enabled: surface.effectiveToolMode() === "pan"
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: surface.canvasCursorShape(mouseX, mouseY)

            onPressed: function (mouse) {
                surface.beginPanDrag(mouse.x, mouse.y);
                mouse.accepted = true;
            }

            onPositionChanged: function (mouse) {
                if (surface.panDraggingActive) {
                    surface.updatePanDrag(mouse.x, mouse.y);
                    mouse.accepted = true;
                }
            }

            onReleased: function (mouse) {
                surface.updatePanDrag(mouse.x, mouse.y);
                surface.commitPanDrag();
                mouse.accepted = true;
            }

            onCanceled: surface.cancelPanDrag()

            onWheel: function (wheel) {
                surface.handleCanvasWheel(wheel);
            }
        }

        MouseArea {
            id: canvasZoomMouseArea
            objectName: "canvasZoomMouseArea"
            anchors.fill: parent
            z: 6
            enabled: surface.effectiveToolMode() === "zoom"
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: surface.canvasCursorShape(mouseX, mouseY)

            onPressed: function (mouse) {
                surface.beginZoomDrag(mouse.x);
                mouse.accepted = true;
            }

            onPositionChanged: function (mouse) {
                if (surface.zoomDraggingActive) {
                    surface.updateZoomDrag(mouse.x);
                    mouse.accepted = true;
                }
            }

            onReleased: function (mouse) {
                surface.updateZoomDrag(mouse.x);
                surface.commitZoomDrag();
                mouse.accepted = true;
            }

            onCanceled: surface.cancelZoomDrag()

            onWheel: function (wheel) {
                surface.handleCanvasWheel(wheel);
            }
        }
    }

    MouseArea {
        id: canvasPointerMouseArea
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: surface.canvasMouseAcceptedButtons()
        cursorShape: surface.canvasCursorShape(mouseX, mouseY)

        onPressed: function (mouse) {
            const mode = surface.effectiveToolMode();
            if (mode === "move") {
                surface.beginDrawableObjectTransform(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (mode === "shape") {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.beginShapeDrag(mouse.x, mouse.y, aspectLocked);
                mouse.accepted = true;
                return;
            }

            if (mode === "fill") {
                surface.fillAt(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (mode === "text") {
                surface.beginTextPlacement(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onPositionChanged: function (mouse) {
            const mode = surface.effectiveToolMode();
            if (mode === "move" && !surface.drawableObjectTransformActive) {
                surface.updateDrawableObjectHoverHandle(mouse.x, mouse.y);
            }

            if (mode === "move" && surface.drawableObjectTransformActive) {
                const constrained = surface.drawableObjectTransformConstrainedFromMouse(mouse);
                surface.updateDrawableObjectTransform(mouse.x, mouse.y, constrained);
                mouse.accepted = true;
                return;
            }

            if (mode === "shape" && surface.shapeDraggingActive) {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);
                mouse.accepted = true;
            }
        }

        onReleased: function (mouse) {
            const mode = surface.effectiveToolMode();
            if (mode === "move") {
                const constrained = surface.drawableObjectTransformConstrainedFromMouse(mouse);
                surface.updateDrawableObjectTransform(mouse.x, mouse.y, constrained);
                surface.commitDrawableObjectTransform();
                surface.updateDrawableObjectHoverHandle(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (mode === "shape") {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);
                surface.commitActiveShape();
                mouse.accepted = true;
            }
        }

        onExited: {
            surface.drawableObjectHoverHandleMode = "";
        }

        onCanceled: {
            surface.drawableObjectHoverHandleMode = "";
            surface.cancelActiveShape();
            surface.cancelActiveDrawableObjectTransform();
            surface.cancelPanDrag();
            surface.cancelZoomDrag();
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅠ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("brush")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㄷ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("eraser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅗ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("pan")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅍ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("move")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅋ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("zoom")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅕ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("shape")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅎ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("fill")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["ㅅ"]
        enabled: surface.toolShortcutsEnabled && !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("text")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["Delete", "Backspace"]
        enabled: !surface.textEditingActive && !surface.shapeDraggingActive && !surface.drawableObjectTransformActive && surface.canDeleteCurrentLayer()
        onActivated: surface.deleteCurrentLayer()
    }
}
