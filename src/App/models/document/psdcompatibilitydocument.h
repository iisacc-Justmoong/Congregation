#pragma once

#include <Engine/DocumentMetadata.h>
#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class PsdLayerRecord
{
  public:
    enum class Kind
    {
        Raster,
        Image,
        Text,
        Shape
    };

    [[nodiscard]] static PsdLayerRecord rasterCanvas(const QSize& canvasSize);
    [[nodiscard]] static PsdLayerRecord fromDrawableObject(const QVariantMap& drawableObject,
                                                           const QSize& canvasSize, int layerIndex);

    [[nodiscard]] Kind kind() const;
    [[nodiscard]] QString kindName() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QRect bounds() const;
    [[nodiscard]] QString blendModeKey() const;
    [[nodiscard]] int opacity() const;
    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] QVariantMap toVariantMap() const;

  private:
    congregation::Object m_record;
};

class PsdCompatibilityDocument
{
  public:
    [[nodiscard]] static constexpr int rgbColorMode() { return 3; }

    [[nodiscard]] static constexpr int bitsPerChannel() { return 8; }

    [[nodiscard]] static constexpr int maximumPsdCanvasEdge() { return 30000; }

    [[nodiscard]] static PsdCompatibilityDocument
    fromCongregationSession(const QSize& canvasSize, const QVariantList& drawableObjects,
                            bool includeBackgroundLayer = true);

    [[nodiscard]] QSize canvasSize() const;
    [[nodiscard]] QList<PsdLayerRecord> layers() const;
    [[nodiscard]] bool isPsdCanvasSizeCompatible() const;
    [[nodiscard]] QVariantMap toManifest() const;

  private:
    QSize m_canvasSize;
    QList<PsdLayerRecord> m_layers;
};
