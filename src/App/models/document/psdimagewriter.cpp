#include "psdimagewriter.h"

#include "psdcompatibilitydocument.h"

#include <PsdPch.h>
#include <PsdAlphaChannel.h>
#include <PsdCompressionType.h>
#include <PsdExport.h>
#include <PsdExportChannel.h>
#include <PsdExportColorMode.h>
#include <PsdExportDocument.h>
#include <PsdFile.h>
#include <PsdMallocAllocator.h>

#include <QByteArray>
#include <QtGlobal>

#include <limits>
#include <Engine/PsdFile.h>
#include <Engine/ScopeExit.h>
#include <Engine/DocumentMetadata.h>
#include "../../host/structuredvalues.h"
#include "../../host/nativepaths.h"

namespace {

constexpr int maximumPsdSdkLayerNameBytes = 31;
constexpr int maximumPsdSdkLayerCount = 128;



struct PlanarRgba8 {
    QByteArray red;
    QByteArray green;
    QByteArray blue;
    QByteArray alpha;

    [[nodiscard]] bool isValid() const
    {
        return !red.isEmpty()
            && red.size() == green.size()
            && red.size() == blue.size()
            && red.size() == alpha.size();
    }
};

[[nodiscard]] bool isPsdCompatibleSize(const QSize &size)
{
    return size.width() > 0
        && size.height() > 0
        && size.width() <= PsdCompatibilityDocument::maximumPsdCanvasEdge()
        && size.height() <= PsdCompatibilityDocument::maximumPsdCanvasEdge();
}

[[nodiscard]] PlanarRgba8 toPlanarRgba8(const QImage &sourceImage)
{
    if (!isPsdCompatibleSize(sourceImage.size())) {
        return {};
    }

    const qsizetype pixelCount = static_cast<qsizetype>(sourceImage.width()) * sourceImage.height();
    if (pixelCount > std::numeric_limits<int>::max()) {
        return {};
    }

    const QImage image = sourceImage.convertToFormat(QImage::Format_RGBA8888);
    PlanarRgba8 planar;
    planar.red = QByteArray(static_cast<int>(pixelCount), '\0');
    planar.green = QByteArray(static_cast<int>(pixelCount), '\0');
    planar.blue = QByteArray(static_cast<int>(pixelCount), '\0');
    planar.alpha = QByteArray(static_cast<int>(pixelCount), '\0');

    for (int y = 0; y < image.height(); ++y) {
        const uchar *line = image.constScanLine(y);
        const qsizetype rowOffset = static_cast<qsizetype>(y) * image.width();
        for (int x = 0; x < image.width(); ++x) {
            const qsizetype channelIndex = rowOffset + x;
            const int pixelIndex = x * 4;
            planar.red[static_cast<int>(channelIndex)] = static_cast<char>(line[pixelIndex]);
            planar.green[static_cast<int>(channelIndex)] = static_cast<char>(line[pixelIndex + 1]);
            planar.blue[static_cast<int>(channelIndex)] = static_cast<char>(line[pixelIndex + 2]);
            planar.alpha[static_cast<int>(channelIndex)] = static_cast<char>(line[pixelIndex + 3]);
        }
    }

    return planar;
}

[[nodiscard]] QByteArray psdLayerName(const QString &name)
{
    QByteArray encoded = name.trimmed().toUtf8();
    encoded.replace('\0', ' ');
    if (encoded.isEmpty()) {
        encoded = QByteArrayLiteral("Layer");
    }
    if (encoded.size() > maximumPsdSdkLayerNameBytes) {
        encoded.truncate(maximumPsdSdkLayerNameBytes);
    }
    return encoded;
}

[[nodiscard]] bool isValidLayer(const PsdImageWriter::Layer &layer, const QSize &canvasSize)
{
    if (layer.image.isNull() || !layer.bounds.isValid() || layer.image.size() != layer.bounds.size()) {
        return false;
    }

    return QRect(QPoint(0, 0), canvasSize).contains(layer.bounds);
}

[[nodiscard]] QByteArray compactManifestJson(const QVariantMap &manifest)
{
    return QByteArray::fromStdString(congregation::json(congregation::presentation::fromVariant(manifest)));
}

void addCongregationMetadata(psd::ExportDocument *document,
                        psd::Allocator *allocator,
                        const QVariantMap &manifest,
                        int layerCount)
{
    const QByteArray encodedManifest = QByteArray::fromStdString(congregation::base64(compactManifestJson(manifest).toStdString()));
    const QByteArray layerCountValue = QByteArray::number(layerCount);

    psd::AddMetaData(document, allocator, "CongregationApplication", "Congregation");
    psd::AddMetaData(document, allocator, "CongregationCompatibilityVersion", "1");
    psd::AddMetaData(document, allocator, "CongregationLayerManifestEncoding", "base64-json");
    psd::AddMetaData(document, allocator, "CongregationLayerManifestBase64", encodedManifest.constData());
    psd::AddMetaData(document, allocator, "CongregationLayerCount", layerCountValue.constData());
}

bool addLayer(psd::ExportDocument *document,
              psd::Allocator *allocator,
              const PsdImageWriter::Layer &layer)
{
    const PlanarRgba8 planar = toPlanarRgba8(layer.image);
    if (!planar.isValid()) {
        return false;
    }

    const QByteArray name = psdLayerName(layer.name);
    const unsigned int layerIndex = psd::AddLayer(document, allocator, name.constData());
    const int left = layer.bounds.left();
    const int top = layer.bounds.top();
    const int right = layer.bounds.right() + 1;
    const int bottom = layer.bounds.bottom() + 1;

    psd::UpdateLayer(document,
                     allocator,
                     layerIndex,
                     psd::exportChannel::RED,
                     left,
                     top,
                     right,
                     bottom,
                     reinterpret_cast<const uint8_t *>(planar.red.constData()),
                     psd::compressionType::RAW);
    psd::UpdateLayer(document,
                     allocator,
                     layerIndex,
                     psd::exportChannel::GREEN,
                     left,
                     top,
                     right,
                     bottom,
                     reinterpret_cast<const uint8_t *>(planar.green.constData()),
                     psd::compressionType::RAW);
    psd::UpdateLayer(document,
                     allocator,
                     layerIndex,
                     psd::exportChannel::BLUE,
                     left,
                     top,
                     right,
                     bottom,
                     reinterpret_cast<const uint8_t *>(planar.blue.constData()),
                     psd::compressionType::RAW);
    psd::UpdateLayer(document,
                     allocator,
                     layerIndex,
                     psd::exportChannel::ALPHA,
                     left,
                     top,
                     right,
                     bottom,
                     reinterpret_cast<const uint8_t *>(planar.alpha.constData()),
                     psd::compressionType::RAW);

    return true;
}

} // namespace

bool PsdImageWriter::canWritePath(const QString &filePath)
{
    return congregation::lower(congregation::presentation::nativePath(filePath).extension().string()) == ".psd";
}

bool PsdImageWriter::writeLayeredImage(const QString &filePath,
                                       const QImage &mergedImage,
                                       const QList<Layer> &bottomToTopLayers,
                                       const QVariantMap &manifest)
{
    if (!canWritePath(filePath)
        || mergedImage.isNull()
        || !isPsdCompatibleSize(mergedImage.size())
        || bottomToTopLayers.size() > maximumPsdSdkLayerCount) {
        return false;
    }

    for (const Layer &layer : bottomToTopLayers) {
        if (!isValidLayer(layer, mergedImage.size())) {
            return false;
        }
    }

    const PlanarRgba8 mergedPlanar = toPlanarRgba8(mergedImage);
    if (!mergedPlanar.isValid()) {
        return false;
    }

    psd::MallocAllocator allocator;
    congregation::PsdFile file(&allocator, congregation::presentation::nativePath(filePath), true);
    if (!file.isOpen()) {
        return false;
    }

    psd::ExportDocument *document = psd::CreateExportDocument(&allocator,
                                                              static_cast<unsigned int>(mergedImage.width()),
                                                              static_cast<unsigned int>(mergedImage.height()),
                                                              PsdCompatibilityDocument::bitsPerChannel(),
                                                              psd::exportColorMode::RGB);
    if (!document) {
        return false;
    }
    const auto destroyDocument = congregation::ScopeExit([&] {
        psd::DestroyExportDocument(document, &allocator);
    });

    addCongregationMetadata(document, &allocator, manifest, bottomToTopLayers.size());

    for (const Layer &layer : bottomToTopLayers) {
        if (!addLayer(document, &allocator, layer)) {
            return false;
        }
    }

    psd::UpdateMergedImage(document,
                           &allocator,
                           reinterpret_cast<const uint8_t *>(mergedPlanar.red.constData()),
                           reinterpret_cast<const uint8_t *>(mergedPlanar.green.constData()),
                           reinterpret_cast<const uint8_t *>(mergedPlanar.blue.constData()));

    const unsigned int alphaChannelIndex = psd::AddAlphaChannel(document,
                                                               &allocator,
                                                               "Transparency",
                                                               0,
                                                               0,
                                                               0,
                                                               0,
                                                               100,
                                                               psd::AlphaChannel::Mode::ALPHA);
    psd::UpdateChannel(document,
                       &allocator,
                       alphaChannelIndex,
                       reinterpret_cast<const uint8_t *>(mergedPlanar.alpha.constData()));

    psd::WriteDocument(document, &allocator, &file);
    const bool writeSucceeded = !file.hasWriteError() && file.size() > 0;
    file.Close();
    return writeSucceeded;
}
