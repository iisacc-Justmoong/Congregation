pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: laserPointer
    clip: true
    enabled: visible

    readonly property int trailLifetimeMs: 2000
    readonly property int repaintIntervalMs: 16
    readonly property int maximumTrailPointCount: 256
    readonly property int trailOpacityStepCount: 24
    readonly property real minimumTrailSampleDistance: 2
    readonly property real trailLineWidth: 6
    readonly property real trailGlowWidth: 16
    readonly property color laserColor: "#ff2b2b"

    property var trailPoints: []
    property bool laserActive: false
    property real currentPointX: 0
    property real currentPointY: 0

    signal wheelZoomRequested(real angleDeltaY, real pixelDeltaY)

    function appendTrailPoint(x, y, forceSample, startsStroke) {
        const now = Date.now();
        const points = laserPointer.trailPoints.slice();
        if (points.length > 0) {
            const previousPoint = points[points.length - 1];
            const deltaX = x - previousPoint.x;
            const deltaY = y - previousPoint.y;
            if (!startsStroke && deltaX === 0 && deltaY === 0) {
                return;
            }
            if (!forceSample) {
                const minimumDistanceSquared = laserPointer.minimumTrailSampleDistance * laserPointer.minimumTrailSampleDistance;
                if (deltaX * deltaX + deltaY * deltaY < minimumDistanceSquared) {
                    return;
                }
            }
        }

        points.push({
            x: x,
            y: y,
            createdAt: now,
            startsStroke: startsStroke === true
        });
        if (points.length > laserPointer.maximumTrailPointCount) {
            points.splice(0, points.length - laserPointer.maximumTrailPointCount);
        }
        laserPointer.trailPoints = points;
        trailCanvas.requestPaint();
    }

    function beginLaserPoint(x, y) {
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.laserActive = true;
        laserPointer.appendTrailPoint(x, y, true, true);
    }

    function updateLaserPoint(x, y) {
        if (!laserPointer.laserActive) {
            return;
        }
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.appendTrailPoint(x, y, false, false);
    }

    function endLaserPoint(x, y) {
        if (!laserPointer.laserActive) {
            return;
        }
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.appendTrailPoint(x, y, true, false);
        laserPointer.laserActive = false;
        trailCanvas.requestPaint();
    }

    function trailPointOpacity(point, now) {
        const ageMs = now - point.createdAt;
        return Math.max(0, Math.min(1, 1 - ageMs / laserPointer.trailLifetimeMs));
    }

    function appendSmoothTrailRun(context, runPoints) {
        if (runPoints.length < 2) {
            return;
        }

        context.moveTo(runPoints[0].x, runPoints[0].y);
        for (let pointIndex = 0; pointIndex < runPoints.length - 1; ++pointIndex) {
            const previousPoint = runPoints[Math.max(0, pointIndex - 1)];
            const point = runPoints[pointIndex];
            const nextPoint = runPoints[pointIndex + 1];
            const followingPoint = runPoints[Math.min(runPoints.length - 1, pointIndex + 2)];
            const firstControlPointX = point.x + (nextPoint.x - previousPoint.x) / 6;
            const firstControlPointY = point.y + (nextPoint.y - previousPoint.y) / 6;
            const secondControlPointX = nextPoint.x - (followingPoint.x - point.x) / 6;
            const secondControlPointY = nextPoint.y - (followingPoint.y - point.y) / 6;
            context.bezierCurveTo(firstControlPointX, firstControlPointY, secondControlPointX, secondControlPointY, nextPoint.x, nextPoint.y);
        }
    }

    function strokeTrailLayer(context, points, now, lineWidth, maximumOpacity) {
        if (points.length < 2) {
            return;
        }

        const opacityIncrement = maximumOpacity / laserPointer.trailOpacityStepCount;
        context.lineWidth = lineWidth;

        for (let stepIndex = 1; stepIndex <= laserPointer.trailOpacityStepCount; ++stepIndex) {
            const minimumOpacity = stepIndex / laserPointer.trailOpacityStepCount;
            const accumulatedOpacity = opacityIncrement * (stepIndex - 1);
            let runPoints = [];
            context.beginPath();

            for (let pointIndex = 1; pointIndex < points.length; ++pointIndex) {
                const point = points[pointIndex];
                const previousPoint = points[pointIndex - 1];
                const segmentOpacity = Math.min(laserPointer.trailPointOpacity(previousPoint, now), laserPointer.trailPointOpacity(point, now));
                if (point.startsStroke || segmentOpacity < minimumOpacity) {
                    laserPointer.appendSmoothTrailRun(context, runPoints);
                    runPoints = [];
                    continue;
                }

                if (runPoints.length === 0) {
                    runPoints.push(previousPoint);
                }
                runPoints.push(point);
            }
            laserPointer.appendSmoothTrailRun(context, runPoints);

            context.globalAlpha = opacityIncrement / (1 - accumulatedOpacity);
            context.stroke();
        }
    }

    function cancelLaserPoint() {
        laserPointer.laserActive = false;
        trailCanvas.requestPaint();
    }

    function pruneExpiredTrailPoints() {
        const cutoff = Date.now() - laserPointer.trailLifetimeMs;
        const remainingPoints = laserPointer.trailPoints.filter(point => point.createdAt > cutoff);
        if (remainingPoints.length !== laserPointer.trailPoints.length) {
            laserPointer.trailPoints = remainingPoints;
        }
        trailCanvas.requestPaint();
    }

    function clearLaserTrail() {
        laserPointer.laserActive = false;
        laserPointer.trailPoints = [];
        trailCanvas.requestPaint();
    }

    onVisibleChanged: if (!visible) {
        laserPointer.clearLaserTrail();
    }

    Timer {
        id: trailRepaintTimer
        interval: laserPointer.repaintIntervalMs
        repeat: true
        running: laserPointer.laserActive || laserPointer.trailPoints.length > 0
        onTriggered: laserPointer.pruneExpiredTrailPoints()
    }

    Canvas {
        id: trailCanvas
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.Image

        onPaint: {
            const context = getContext("2d");
            context.clearRect(0, 0, width, height);

            const now = Date.now();
            const points = laserPointer.trailPoints;
            context.lineCap = "butt";
            context.lineJoin = "bevel";
            context.strokeStyle = laserPointer.laserColor;
            laserPointer.strokeTrailLayer(context, points, now, laserPointer.trailGlowWidth, 0.18);
            laserPointer.strokeTrailLayer(context, points, now, laserPointer.trailLineWidth, 0.55);
            context.globalAlpha = 1;
        }
    }

    Item {
        id: activeLaserPoint
        visible: laserPointer.laserActive
        x: laserPointer.currentPointX - width / 2
        y: laserPointer.currentPointY - height / 2
        width: 28
        height: 28

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.rgba(1, 0.17, 0.17, 0.14)
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: width / 2
            color: Qt.rgba(1, 0.17, 0.17, 0.28)
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: width / 2
            color: "#ff2b2b"
            antialiasing: true
        }
    }

    MouseArea {
        id: laserMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        cursorShape: pressed ? Qt.BlankCursor : Qt.ArrowCursor

        onPressed: function (mouse) {
            laserPointer.beginLaserPoint(mouse.x, mouse.y);
            mouse.accepted = true;
        }

        onPositionChanged: function (mouse) {
            if (pressed) {
                laserPointer.updateLaserPoint(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onReleased: function (mouse) {
            laserPointer.endLaserPoint(mouse.x, mouse.y);
            mouse.accepted = true;
        }

        onWheel: function (wheel) {
            if (wheel.angleDelta.y === 0 && wheel.pixelDelta.y === 0) {
                wheel.accepted = false;
                return;
            }
            laserPointer.wheelZoomRequested(wheel.angleDelta.y, wheel.pixelDelta.y);
            wheel.accepted = true;
        }

        onCanceled: laserPointer.cancelLaserPoint()
    }
}
