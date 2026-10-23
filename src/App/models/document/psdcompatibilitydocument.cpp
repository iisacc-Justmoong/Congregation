#include "psdcompatibilitydocument.h"
#include "../../host/structuredvalues.h"
using namespace congregation;
PsdLayerRecord PsdLayerRecord::rasterCanvas(const QSize& size)
{
    PsdLayerRecord result;
    result.m_record = psd_layer({}, size.width(), size.height(), 0, true);
    return result;
}
PsdLayerRecord PsdLayerRecord::fromDrawableObject(const QVariantMap& object, const QSize& size,
                                                  int index)
{
    PsdLayerRecord result;
    result.m_record = psd_layer(congregation::object(presentation::fromVariant(object)),
                                size.width(), size.height(), index);
    return result;
}
PsdLayerRecord::Kind PsdLayerRecord::kind() const
{
    auto name = kindName();
    return name == QStringLiteral("image")   ? Kind::Image
           : name == QStringLiteral("text")  ? Kind::Text
           : name == QStringLiteral("shape") ? Kind::Shape
                                             : Kind::Raster;
}
QString PsdLayerRecord::kindName() const
{
    return QString::fromStdString(text(field(m_record, "kind")));
}
QString PsdLayerRecord::name() const
{
    return QString::fromStdString(text(field(m_record, "name")));
}
QRect PsdLayerRecord::bounds() const
{
    return QRect(static_cast<int>(number(field(m_record, "left"))),
                 static_cast<int>(number(field(m_record, "top"))),
                 static_cast<int>(number(field(m_record, "width"))),
                 static_cast<int>(number(field(m_record, "height"))));
}
QString PsdLayerRecord::blendModeKey() const
{
    return QString::fromStdString(text(field(m_record, "blendModeKey")));
}
int PsdLayerRecord::opacity() const
{
    return static_cast<int>(number(field(m_record, "opacity")));
}
bool PsdLayerRecord::isVisible() const
{
    return boolean(field(m_record, "visible"));
}
QVariantMap PsdLayerRecord::toVariantMap() const
{
    return presentation::toVariant(Value(m_record)).toMap();
}
PsdCompatibilityDocument
PsdCompatibilityDocument::fromCongregationSession(const QSize& size, const QVariantList& objects,
                                                  bool background)
{
    PsdCompatibilityDocument result;
    result.m_canvasSize = QSize(std::max(1, size.width()), std::max(1, size.height()));
    if (background)
        result.m_layers.append(PsdLayerRecord::rasterCanvas(result.m_canvasSize));
    for (int i = 0; i < objects.size(); ++i)
    {
        auto object = objects[i].toMap();
        if (!object.isEmpty())
            result.m_layers.append(
                PsdLayerRecord::fromDrawableObject(object, result.m_canvasSize, i));
    }
    return result;
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
    return m_canvasSize.width() <= maximumPsdCanvasEdge() &&
           m_canvasSize.height() <= maximumPsdCanvasEdge();
}
QVariantMap PsdCompatibilityDocument::toManifest() const
{
    Array layers;
    for (const auto& layer : m_layers)
        layers.push_back(presentation::fromVariant(layer.toVariantMap()));
    return presentation::toVariant(
               Value(psd_manifest(m_canvasSize.width(), m_canvasSize.height(), std::move(layers))))
        .toMap();
}
