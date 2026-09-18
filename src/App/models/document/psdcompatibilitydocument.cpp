#include "psdcompatibilitydocument.h"

#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QtMath>
#include <QtGlobal>

namespace {

QString normalizedObjectType(const QVariantMap &drawableObject)
{
    return drawableObject.value(QStringLiteral("type")).toString().trimmed().toLower();
}

QString normalizedBlendModeKey(const QVariantMap &drawableObject)
{
    const QString blendMode = drawableObject.value(QStringLiteral("blendMode")).toString().trimmed().toLower();
    if (blendMode == QStringLiteral("norm") || blendMode == QStringLiteral("normal")) {
        return QStringLiteral("norm");
    }
    if (blendMode == QStringLiteral("mul") || blendMode == QStringLiteral("multiply")) {
        return QStringLiteral("mul ");
    }
    if (blendMode == QStringLiteral("scrn") || blendMode == QStringLiteral("screen")) {
        return QStringLiteral("scrn");
    }
    if (blendMode == QStringLiteral("over") || blendMode == QStringLiteral("overlay")) {
        return QStringLiteral("over");
    }
    if (blendMode == QStringLiteral("dark") || blendMode == QStringLiteral("darken")) {
        return QStringLiteral("dark");
    }
    if (blendMode == QStringLiteral("lite") || blendMode == QStringLiteral("lighten")) {
        return QStringLiteral("lite");
    }
    if (blendMode == QStringLiteral("diff") || blendMode == QStringLiteral("difference")) {
        return QStringLiteral("diff");
    }

    static const QStringList supportedPsdKeys{
        QStringLiteral("diss"),
        QStringLiteral("idiv"),
        QStringLiteral("lbrn"),
        QStringLiteral("dkCl"),
        QStringLiteral("div "),
        QStringLiteral("lddg"),
        QStringLiteral("lgCl"),
        QStringLiteral("sLit"),
        QStringLiteral("hLit"),
        QStringLiteral("vLit"),
        QStringLiteral("lLit"),
        QStringLiteral("pLit"),
        QStringLiteral("hMix"),
        QStringLiteral("smud"),
        QStringLiteral("fsub"),
        QStringLiteral("fdiv"),
        QStringLiteral("hue "),
        QStringLiteral("sat "),
        QStringLiteral("colr"),
        QStringLiteral("lum ")
    };
    if (supportedPsdKeys.contains(drawableObject.value(QStringLiteral("blendMode")).toString().trimmed())) {
        return drawableObject.value(QStringLiteral("blendMode")).toString().trimmed();
    }

    return QStringLiteral("norm");
}

int normalizedOpacity(const QVariantMap &drawableObject)
{
    if (!drawableObject.contains(QStringLiteral("opacity"))) {
        return 255;
    }

    const qreal opacityValue = drawableObject.value(QStringLiteral("opacity")).toReal();
    const int opacityByte = opacityValue <= 1.0
        ? qRound(opacityValue * 255.0)
        : qRound(opacityValue);
    return qBound(0, opacityByte, 255);
}

bool normalizedVisibility(const QVariantMap &drawableObject)
{
    if (!drawableObject.contains(QStringLiteral("visible"))) {
        return true;
    }

    return drawableObject.value(QStringLiteral("visible")).toBool();
}

QRect normalizedLayerBounds(const QVariantMap &drawableObject, const QSize &canvasSize)
{
    const qreal rawX = drawableObject.value(QStringLiteral("x")).toReal();
    const qreal rawY = drawableObject.value(QStringLiteral("y")).toReal();
    const qreal rawWidth = drawableObject.value(QStringLiteral("width")).toReal();
    const qreal rawHeight = drawableObject.value(QStringLiteral("height")).toReal();

    const int canvasWidth = qMax(1, canvasSize.width());
    const int canvasHeight = qMax(1, canvasSize.height());
    const int left = qBound(0, qFloor(rawX), canvasWidth - 1);
    const int top = qBound(0, qFloor(rawY), canvasHeight - 1);
    const int right = qBound(left + 1, qCeil(rawX + qMax<qreal>(1.0, rawWidth)), canvasWidth);
    const int bottom = qBound(top + 1, qCeil(rawY + qMax<qreal>(1.0, rawHeight)), canvasHeight);
    return QRect(left, top, right - left, bottom - top);
}

QString fileNameFromSource(const QString &source)
{
    const QUrl url(source);
    const QString path = url.isValid() && url.isLocalFile() ? url.toLocalFile() : source;
    const QString fileName = QFileInfo(path).fileName();
    return fileName.isEmpty() ? QStringLiteral("Image") : fileName;
}

QString firstTextLine(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Text");
    }

    const QString firstLine = trimmed.split(QLatin1Char('\n')).constFirst().trimmed();
    return firstLine.isEmpty() ? QStringLiteral("Text") : firstLine.left(32);
}

QString titleCasedShapeName(QString shapeKind)
{
    shapeKind = shapeKind.trimmed();
    if (shapeKind.isEmpty()) {
        return QStringLiteral("Shape");
    }

    shapeKind[0] = shapeKind.at(0).toUpper();
    return shapeKind;
}

QString generatedLayerName(const QVariantMap &drawableObject, int layerIndex)
{
    const QString explicitName = drawableObject.value(QStringLiteral("name")).toString().trimmed();
    if (!explicitName.isEmpty()) {
        return explicitName;
    }

    const QString type = normalizedObjectType(drawableObject);
    if (type == QStringLiteral("image")) {
        return fileNameFromSource(drawableObject.value(QStringLiteral("source")).toString());
    }
    if (type == QStringLiteral("text")) {
        return firstTextLine(drawableObject.value(QStringLiteral("text")).toString());
    }
    if (type == QStringLiteral("shape")) {
        return titleCasedShapeName(drawableObject.value(QStringLiteral("shapeKind")).toString());
    }

    return QStringLiteral("Layer %1").arg(layerIndex + 1);
}

QString kindName(PsdLayerRecord::Kind kind)
{
    switch (kind) {
    case PsdLayerRecord::Kind::Image:
        return QStringLiteral("image");
    case PsdLayerRecord::Kind::Text:
        return QStringLiteral("text");
    case PsdLayerRecord::Kind::Shape:
        return QStringLiteral("shape");
    case PsdLayerRecord::Kind::Raster:
        return QStringLiteral("raster");
    }

    return QStringLiteral("raster");
}

PsdLayerRecord::Kind kindFromObjectType(const QString &type)
{
    if (type == QStringLiteral("image")) {
        return PsdLayerRecord::Kind::Image;
    }
    if (type == QStringLiteral("text")) {
        return PsdLayerRecord::Kind::Text;
    }
    if (type == QStringLiteral("shape")) {
        return PsdLayerRecord::Kind::Shape;
    }

    return PsdLayerRecord::Kind::Raster;
}

QVariantMap objectPayload(const QVariantMap &drawableObject)
{
    QVariantMap payload;
    const QString type = normalizedObjectType(drawableObject);

    if (drawableObject.contains(QStringLiteral("id"))) {
        payload.insert(QStringLiteral("sourceObjectId"), drawableObject.value(QStringLiteral("id")));
    }
    if (type == QStringLiteral("image")) {
        payload.insert(QStringLiteral("source"), drawableObject.value(QStringLiteral("source")).toString());
        payload.insert(QStringLiteral("originalWidth"), drawableObject.value(QStringLiteral("originalWidth")).toInt());
        payload.insert(QStringLiteral("originalHeight"), drawableObject.value(QStringLiteral("originalHeight")).toInt());
    } else if (type == QStringLiteral("text")) {
        payload.insert(QStringLiteral("text"), drawableObject.value(QStringLiteral("text")).toString());
        payload.insert(QStringLiteral("fontPixelSize"), drawableObject.value(QStringLiteral("fontPixelSize")).toReal());
        payload.insert(QStringLiteral("color"), drawableObject.value(QStringLiteral("color")).toString());
    } else if (type == QStringLiteral("shape")) {
        payload.insert(QStringLiteral("shapeKind"), drawableObject.value(QStringLiteral("shapeKind")).toString());
        payload.insert(QStringLiteral("color"), drawableObject.value(QStringLiteral("color")).toString());
    }

    return payload;
}

} // namespace

PsdLayerRecord PsdLayerRecord::rasterCanvas(const QSize &canvasSize)
{
    PsdLayerRecord record;
    record.m_kind = Kind::Raster;
    record.m_name = QStringLiteral("Background");
    record.m_bounds = QRect(0, 0, qMax(1, canvasSize.width()), qMax(1, canvasSize.height()));
    return record;
}

PsdLayerRecord PsdLayerRecord::fromDrawableObject(const QVariantMap &drawableObject,
                                                  const QSize &canvasSize,
                                                  int layerIndex)
{
    PsdLayerRecord record;
    record.m_kind = kindFromObjectType(normalizedObjectType(drawableObject));
    record.m_name = generatedLayerName(drawableObject, layerIndex);
    record.m_bounds = normalizedLayerBounds(drawableObject, canvasSize);
    record.m_blendModeKey = normalizedBlendModeKey(drawableObject);
    record.m_opacity = normalizedOpacity(drawableObject);
    record.m_visible = normalizedVisibility(drawableObject);
    record.m_payload = objectPayload(drawableObject);
    return record;
}

PsdLayerRecord::Kind PsdLayerRecord::kind() const
{
    return m_kind;
}

QString PsdLayerRecord::kindName() const
{
    return ::kindName(m_kind);
}

QString PsdLayerRecord::name() const
{
    return m_name;
}

QRect PsdLayerRecord::bounds() const
{
    return m_bounds;
}

QString PsdLayerRecord::blendModeKey() const
{
    return m_blendModeKey;
}

int PsdLayerRecord::opacity() const
{
    return m_opacity;
}

bool PsdLayerRecord::isVisible() const
{
    return m_visible;
}

QVariantMap PsdLayerRecord::toVariantMap() const
{
    QVariantMap layer;
    layer.insert(QStringLiteral("name"), m_name);
    layer.insert(QStringLiteral("kind"), kindName());
    layer.insert(QStringLiteral("top"), m_bounds.top());
    layer.insert(QStringLiteral("left"), m_bounds.left());
    layer.insert(QStringLiteral("bottom"), m_bounds.bottom() + 1);
    layer.insert(QStringLiteral("right"), m_bounds.right() + 1);
    layer.insert(QStringLiteral("width"), m_bounds.width());
    layer.insert(QStringLiteral("height"), m_bounds.height());
    layer.insert(QStringLiteral("blendModeKey"), m_blendModeKey);
    layer.insert(QStringLiteral("opacity"), m_opacity);
    layer.insert(QStringLiteral("visible"), m_visible);
    layer.insert(QStringLiteral("payload"), m_payload);
    return layer;
}

PsdCompatibilityDocument PsdCompatibilityDocument::fromCongregationSession(const QSize &canvasSize,
                                                                      const QVariantList &drawableObjects,
                                                                      bool includeBackgroundLayer)
{
    PsdCompatibilityDocument document;
    document.m_canvasSize = QSize(qMax(1, canvasSize.width()), qMax(1, canvasSize.height()));
    if (includeBackgroundLayer) {
        document.m_layers.append(PsdLayerRecord::rasterCanvas(document.m_canvasSize));
    }

    for (int index = 0; index < drawableObjects.size(); ++index) {
        const QVariantMap drawableObject = drawableObjects.at(index).toMap();
        if (drawableObject.isEmpty()) {
            continue;
        }
        document.m_layers.append(PsdLayerRecord::fromDrawableObject(drawableObject, document.m_canvasSize, index));
    }

    return document;
}

QSize PsdCompatibilityDocument::canvasSize() const
{
    return m_canvasSize;
}

QList<PsdLayerRecord> PsdCompatibilityDocument::layers() const
{
    return m_layers;
}

bool PsdCompatibilityDocument::isPsdCanvasSizeCompatible() const
{
    return m_canvasSize.width() <= maximumPsdCanvasEdge()
        && m_canvasSize.height() <= maximumPsdCanvasEdge();
}

QVariantMap PsdCompatibilityDocument::toManifest() const
{
    QVariantList layers;
    for (const PsdLayerRecord &layer : m_layers) {
        layers.append(layer.toVariantMap());
    }

    QVariantMap manifest;
    manifest.insert(QStringLiteral("formatFamily"), QStringLiteral("psd"));
    manifest.insert(QStringLiteral("compatibilityVersion"), 1);
    manifest.insert(QStringLiteral("canvasWidth"), m_canvasSize.width());
    manifest.insert(QStringLiteral("canvasHeight"), m_canvasSize.height());
    manifest.insert(QStringLiteral("colorMode"), rgbColorMode());
    manifest.insert(QStringLiteral("bitsPerChannel"), bitsPerChannel());
    manifest.insert(QStringLiteral("coordinateSystem"), QStringLiteral("psd-top-left-bottom-right-exclusive"));
    manifest.insert(QStringLiteral("psdCanvasSizeCompatible"), isPsdCanvasSizeCompatible());
    manifest.insert(QStringLiteral("layers"), layers);
    return manifest;
}
