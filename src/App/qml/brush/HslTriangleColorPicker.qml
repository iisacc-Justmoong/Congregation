pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: picker
    implicitWidth: 280
    implicitHeight: 280

    property color selectedColor: "#1a1a1a"
    property real selectedHue: 0
    property real pureHueWeight: 0
    property real whiteWeight: 0
    property real blackWeight: 1
    property string activeSelectionMode: "none"
    property bool internalColorUpdate: false

    signal colorSelected(color selectedColor)

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    function pickerSize() {
        return Math.max(1, Math.min(width, height));
    }

    function centerPoint() {
        return {
            "x": width / 2,
            "y": height / 2
        };
    }

    function outerRadius() {
        return pickerSize() * 0.46;
    }

    function innerRadius() {
        return outerRadius() * 0.66;
    }

    function triangleRadius() {
        return innerRadius() * 0.84;
    }

    function pointOnCircle(radius, degrees) {
        const center = centerPoint();
        const radians = degrees * Math.PI / 180;
        return {
            "x": center.x + Math.cos(radians) * radius,
            "y": center.y + Math.sin(radians) * radius
        };
    }

    function triangleTopPoint() {
        return pointOnCircle(triangleRadius(), -90);
    }

    function triangleWhitePoint() {
        return pointOnCircle(triangleRadius(), 150);
    }

    function triangleBlackPoint() {
        return pointOnCircle(triangleRadius(), 30);
    }

    function normalizeWeights() {
        pureHueWeight = clamp(pureHueWeight, 0, 1);
        whiteWeight = clamp(whiteWeight, 0, 1);
        blackWeight = clamp(blackWeight, 0, 1);

        const total = pureHueWeight + whiteWeight + blackWeight;
        if (total <= 0) {
            pureHueWeight = 0;
            whiteWeight = 0;
            blackWeight = 1;
            return;
        }

        pureHueWeight /= total;
        whiteWeight /= total;
        blackWeight /= total;
    }

    function colorFromHue(hueValue) {
        return Qt.hsla(((hueValue % 1) + 1) % 1, 1, 0.5, 1);
    }

    function colorFromWeights(hueValue, pureHue, white, black) {
        const total = Math.max(0.0001, pureHue + white + black);
        const normalizedPureHue = clamp(pureHue / total, 0, 1);
        const normalizedWhite = clamp(white / total, 0, 1);
        const saturation = normalizedPureHue;
        const lightness = clamp(normalizedPureHue * 0.5 + normalizedWhite, 0, 1);
        return Qt.hsla(((hueValue % 1) + 1) % 1, saturation, lightness, 1);
    }

    function currentColorFromWeights() {
        return colorFromWeights(selectedHue, pureHueWeight, whiteWeight, blackWeight);
    }

    function weightsForPoint(point) {
        const pureHue = triangleTopPoint();
        const white = triangleWhitePoint();
        const black = triangleBlackPoint();
        const denominator = ((white.y - black.y) * (pureHue.x - black.x)) + ((black.x - white.x) * (pureHue.y - black.y));
        if (Math.abs(denominator) < 0.000001) {
            return null;
        }

        const pureHueAmount = (((white.y - black.y) * (point.x - black.x)) + ((black.x - white.x) * (point.y - black.y))) / denominator;
        const whiteAmount = (((black.y - pureHue.y) * (point.x - black.x)) + ((pureHue.x - black.x) * (point.y - black.y))) / denominator;
        const blackAmount = 1 - pureHueAmount - whiteAmount;
        const tolerance = 0.0001;
        if (pureHueAmount < -tolerance || whiteAmount < -tolerance || blackAmount < -tolerance) {
            return null;
        }

        return {
            "pureHue": clamp(pureHueAmount, 0, 1),
            "white": clamp(whiteAmount, 0, 1),
            "black": clamp(blackAmount, 0, 1)
        };
    }

    function colorFromTrianglePoint(point) {
        const weights = weightsForPoint(point);
        if (weights === null) {
            return selectedColor;
        }
        return colorFromWeights(selectedHue, weights.pureHue, weights.white, weights.black);
    }

    function pointFromWeights() {
        const pureHue = clamp(pureHueWeight, 0, 1);
        const white = clamp(whiteWeight, 0, 1);
        const black = clamp(blackWeight, 0, 1);
        const total = pureHue + white + black;
        const normalizedPureHue = total > 0 ? pureHue / total : 0;
        const normalizedWhite = total > 0 ? white / total : 0;
        const normalizedBlack = total > 0 ? black / total : 1;
        const pureHuePoint = triangleTopPoint();
        const whitePoint = triangleWhitePoint();
        const blackPoint = triangleBlackPoint();
        return {
            "x": pureHuePoint.x * normalizedPureHue + whitePoint.x * normalizedWhite + blackPoint.x * normalizedBlack,
            "y": pureHuePoint.y * normalizedPureHue + whitePoint.y * normalizedWhite + blackPoint.y * normalizedBlack
        };
    }

    function selectionModeAt(point) {
        if (weightsForPoint(point) !== null) {
            return "triangle";
        }

        const center = centerPoint();
        const deltaX = point.x - center.x;
        const deltaY = point.y - center.y;
        const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
        if (distance >= innerRadius() && distance <= outerRadius()) {
            return "hue";
        }

        return "none";
    }

    function selectHueAt(point) {
        const center = centerPoint();
        const angle = Math.atan2(point.y - center.y, point.x - center.x);
        selectedHue = ((angle / (Math.PI * 2)) + 1) % 1;
    }

    function selectTriangleAt(point) {
        const weights = weightsForPoint(point);
        if (weights === null) {
            return false;
        }
        pureHueWeight = weights.pureHue;
        whiteWeight = weights.white;
        blackWeight = weights.black;
        return true;
    }

    function commitSelectedColor() {
        const nextColor = currentColorFromWeights();
        internalColorUpdate = true;
        selectedColor = nextColor;
        internalColorUpdate = false;
        colorSelected(nextColor);
        triangleCanvas.requestPaint();
    }

    function selectAt(point) {
        if (activeSelectionMode === "hue") {
            selectHueAt(point);
            commitSelectedColor();
            return;
        }

        if (activeSelectionMode === "triangle" && selectTriangleAt(point)) {
            commitSelectedColor();
        }
    }

    function syncFromColor(colorValue) {
        const red = clamp(colorValue.r, 0, 1);
        const green = clamp(colorValue.g, 0, 1);
        const blue = clamp(colorValue.b, 0, 1);
        const maximum = Math.max(red, green, blue);
        const minimum = Math.min(red, green, blue);
        const lightness = (maximum + minimum) / 2;
        var hue = selectedHue;
        var saturation = 0;

        if (Math.abs(maximum - minimum) > 0.000001) {
            const delta = maximum - minimum;
            saturation = lightness > 0.5 ? delta / (2 - maximum - minimum) : delta / (maximum + minimum);

            if (maximum === red) {
                hue = (green - blue) / delta + (green < blue ? 6 : 0);
            } else if (maximum === green) {
                hue = (blue - red) / delta + 2;
            } else {
                hue = (red - green) / delta + 4;
            }
            hue /= 6;
        }

        if (saturation > 0.0001) {
            selectedHue = ((hue % 1) + 1) % 1;
        }

        pureHueWeight = clamp(saturation, 0, 1);
        whiteWeight = clamp(lightness - pureHueWeight * 0.5, 0, 1);
        blackWeight = clamp(1 - pureHueWeight - whiteWeight, 0, 1);
        normalizeWeights();
        hueWheelCanvas.requestPaint();
        triangleCanvas.requestPaint();
    }

    onSelectedColorChanged: {
        if (!internalColorUpdate) {
            syncFromColor(selectedColor);
        }
    }

    onSelectedHueChanged: {
        hueWheelCanvas.requestPaint();
        triangleCanvas.requestPaint();
    }

    Component.onCompleted: syncFromColor(selectedColor)

    Canvas {
        id: hueWheelCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image

        onPaint: {
            const context = getContext("2d");
            context.clearRect(0, 0, width, height);

            const center = picker.centerPoint();
            const outer = picker.outerRadius();
            const inner = picker.innerRadius();
            const segments = 180;
            for (let i = 0; i < segments; ++i) {
                const start = i / segments * Math.PI * 2;
                const end = (i + 1.5) / segments * Math.PI * 2;
                context.beginPath();
                context.arc(center.x, center.y, outer, start, end, false);
                context.arc(center.x, center.y, inner, end, start, true);
                context.closePath();
                context.fillStyle = picker.colorFromHue(i / segments).toString();
                context.fill();
            }
        }
    }

    Canvas {
        id: triangleCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image

        onPaint: {
            const context = getContext("2d");
            context.clearRect(0, 0, width, height);

            const top = picker.triangleTopPoint();
            const white = picker.triangleWhitePoint();
            const black = picker.triangleBlackPoint();
            const minimumX = Math.max(0, Math.floor(Math.min(top.x, white.x, black.x)));
            const maximumX = Math.min(width - 1, Math.ceil(Math.max(top.x, white.x, black.x)));
            const minimumY = Math.max(0, Math.floor(Math.min(top.y, white.y, black.y)));
            const maximumY = Math.min(height - 1, Math.ceil(Math.max(top.y, white.y, black.y)));
            for (let y = minimumY; y <= maximumY; ++y) {
                for (let x = minimumX; x <= maximumX; ++x) {
                    const weights = picker.weightsForPoint({
                        "x": x + 0.5,
                        "y": y + 0.5
                    });
                    if (weights === null) {
                        continue;
                    }

                    context.fillStyle = picker.colorFromWeights(picker.selectedHue, weights.pureHue, weights.white, weights.black).toString();
                    context.fillRect(x, y, 1, 1);
                }
            }

            context.beginPath();
            context.moveTo(top.x, top.y);
            context.lineTo(white.x, white.y);
            context.lineTo(black.x, black.y);
            context.closePath();
            context.lineWidth = 1;
            context.strokeStyle = "#111827";
            context.stroke();
        }
    }

    Item {
        anchors.fill: parent

        Rectangle {
            readonly property var markerPoint: picker.pointOnCircle((picker.outerRadius() + picker.innerRadius()) / 2, picker.selectedHue * 360)

            width: 14
            height: 14
            radius: 7
            x: markerPoint.x - width / 2
            y: markerPoint.y - height / 2
            color: "transparent"
            border.width: 2
            border.color: "#ffffff"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: width / 2
                color: "transparent"
                border.width: 1
                border.color: "#111827"
            }
        }

        Rectangle {
            readonly property var markerPoint: picker.pointFromWeights()

            width: 14
            height: 14
            radius: 7
            x: markerPoint.x - width / 2
            y: markerPoint.y - height / 2
            color: picker.selectedColor
            border.width: 2
            border.color: "#ffffff"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: width / 2
                color: "transparent"
                border.width: 1
                border.color: "#111827"
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: picker.activeSelectionMode === "none" ? Qt.CrossCursor : Qt.PointingHandCursor

        onPressed: function (mouse) {
            const point = {
                "x": mouse.x,
                "y": mouse.y
            };
            picker.activeSelectionMode = picker.selectionModeAt(point);
            picker.selectAt(point);
            mouse.accepted = true;
        }

        onPositionChanged: function (mouse) {
            if (!pressed) {
                const point = {
                    "x": mouse.x,
                    "y": mouse.y
                };
                picker.activeSelectionMode = picker.selectionModeAt(point);
                return;
            }

            picker.selectAt({
                "x": mouse.x,
                "y": mouse.y
            });
            mouse.accepted = true;
        }

        onReleased: function (mouse) {
            picker.activeSelectionMode = "none";
            mouse.accepted = true;
        }
    }
}
