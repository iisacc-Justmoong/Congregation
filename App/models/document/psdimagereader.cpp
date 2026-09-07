#include "psdimagereader.h"

#include <PsdPch.h>
#include <PsdColorMode.h>
#include <PsdChannel.h>
#include <PsdChannelType.h>
#include <PsdDocument.h>
#include <PsdFile.h>
#include <PsdImageDataSection.h>
#include <PsdImageResourcesSection.h>
#include <PsdLayer.h>
#include <PsdLayerMaskSection.h>
#include <PsdMallocAllocator.h>
#include <PsdParseImageResourcesSection.h>
#include <PsdParseDocument.h>
#include <PsdParseImageDataSection.h>
#include <PsdParseLayerMaskSection.h>
#include <PsdPlanarImage.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QXmlStreamReader>
#include <QtGlobal>

#include <limits>

namespace {

constexpr unsigned int channelNotFound = std::numeric_limits<unsigned int>::max();

class QFilePsdFile final : public psd::File
{
public:
    QFilePsdFile(psd::Allocator *allocator, const QString &filePath)
        : psd::File(allocator)
        , m_file(filePath)
    {
        m_file.open(QIODevice::ReadOnly);
    }

    [[nodiscard]] bool isOpen() const
    {
        return m_file.isOpen();
    }

private:
    struct ReadResult {
        bool ok = false;
    };

    bool DoOpenRead(const wchar_t *filename) override
    {
        if (m_file.isOpen()) {
            return true;
        }
        m_file.setFileName(QString::fromWCharArray(filename));
        return m_file.open(QIODevice::ReadOnly);
    }

    bool DoOpenWrite(const wchar_t *) override
    {
        return false;
    }

    bool DoClose() override
    {
        m_file.close();
        return true;
    }

    psd::File::ReadOperation DoRead(void *buffer, uint32_t count, uint64_t position) override
    {
        auto *result = new ReadResult;
        if (!m_file.isOpen()
                || position > static_cast<uint64_t>(std::numeric_limits<qint64>::max())
                || count > static_cast<uint32_t>(std::numeric_limits<qint64>::max())) {
            return result;
        }

        result->ok = m_file.seek(static_cast<qint64>(position))
            && m_file.read(static_cast<char *>(buffer), static_cast<qint64>(count)) == static_cast<qint64>(count);
        return result;
    }

    bool DoWaitForRead(psd::File::ReadOperation &operation) override
    {
        auto *result = static_cast<ReadResult *>(operation);
        const bool ok = result && result->ok;
        delete result;
        operation = nullptr;
        return ok;
    }

    psd::File::WriteOperation DoWrite(const void *, uint32_t, uint64_t) override
    {
        return nullptr;
    }

    bool DoWaitForWrite(psd::File::WriteOperation &) override
    {
        return false;
    }

    uint64_t DoGetSize() const override
    {
        return m_file.isOpen() ? static_cast<uint64_t>(m_file.size()) : 0;
    }

    QFile m_file;
};

[[nodiscard]] bool isSupportedMergedDocument(const psd::Document &document, const psd::ImageDataSection &imageData)
{
    return document.width > 0
        && document.height > 0
        && document.bitsPerChannel == 8
        && document.colorMode == psd::colorMode::RGB
        && imageData.imageCount >= 3;
}

[[nodiscard]] QImage imageFromMergedData(const psd::Document &document, const psd::ImageDataSection &imageData)
{
    if (!isSupportedMergedDocument(document, imageData)) {
        return {};
    }

    const int width = static_cast<int>(document.width);
    const int height = static_cast<int>(document.height);
    const auto *red = static_cast<const uchar *>(imageData.images[0].data);
    const auto *green = static_cast<const uchar *>(imageData.images[1].data);
    const auto *blue = static_cast<const uchar *>(imageData.images[2].data);
    const auto *alpha = imageData.imageCount >= 4 ? static_cast<const uchar *>(imageData.images[3].data) : nullptr;
    if (!red || !green || !blue) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }

    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        const qsizetype rowOffset = static_cast<qsizetype>(y) * width;
        for (int x = 0; x < width; ++x) {
            const qsizetype channelIndex = rowOffset + x;
            const int pixelIndex = x * 4;
            line[pixelIndex] = red[channelIndex];
            line[pixelIndex + 1] = green[channelIndex];
            line[pixelIndex + 2] = blue[channelIndex];
            line[pixelIndex + 3] = alpha ? alpha[channelIndex] : 255;
        }
    }

    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

[[nodiscard]] QString psdKeyToString(uint32_t key)
{
    QByteArray value;
    value.resize(4);
    value[0] = static_cast<char>((key >> 24) & 0xFF);
    value[1] = static_cast<char>((key >> 16) & 0xFF);
    value[2] = static_cast<char>((key >> 8) & 0xFF);
    value[3] = static_cast<char>(key & 0xFF);
    return QString::fromLatin1(value);
}

[[nodiscard]] QString layerName(const psd::Layer &layer)
{
    if (layer.utf16Name) {
        qsizetype length = 0;
        while (layer.utf16Name[length] != 0) {
            ++length;
        }
        const QString unicodeName = QString::fromUtf16(reinterpret_cast<const char16_t *>(layer.utf16Name), length).trimmed();
        if (!unicodeName.isEmpty()) {
            return unicodeName;
        }
    }

    const QString asciiName = QString::fromLatin1(layer.name.c_str()).trimmed();
    return asciiName.isEmpty() ? QStringLiteral("Layer") : asciiName;
}

[[nodiscard]] unsigned int findChannel(const psd::Layer &layer, int16_t channelType)
{
    for (unsigned int index = 0; index < layer.channelCount; ++index) {
        const psd::Channel &channel = layer.channels[index];
        if (channel.data && channel.type == channelType) {
            return index;
        }
    }
    return channelNotFound;
}

[[nodiscard]] QImage imageFromLayerData(const psd::Document &document, const psd::Layer &layer)
{
    if (document.bitsPerChannel != 8 || document.colorMode != psd::colorMode::RGB) {
        return {};
    }

    const int layerWidth = static_cast<int>(layer.right - layer.left);
    const int layerHeight = static_cast<int>(layer.bottom - layer.top);
    const int canvasWidth = static_cast<int>(document.width);
    const int canvasHeight = static_cast<int>(document.height);
    if (layerWidth <= 0 || layerHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0) {
        return {};
    }

    const unsigned int redIndex = findChannel(layer, psd::channelType::R);
    const unsigned int greenIndex = findChannel(layer, psd::channelType::G);
    const unsigned int blueIndex = findChannel(layer, psd::channelType::B);
    if (redIndex == channelNotFound || greenIndex == channelNotFound || blueIndex == channelNotFound) {
        return {};
    }

    const auto *red = static_cast<const uchar *>(layer.channels[redIndex].data);
    const auto *green = static_cast<const uchar *>(layer.channels[greenIndex].data);
    const auto *blue = static_cast<const uchar *>(layer.channels[blueIndex].data);
    const unsigned int alphaIndex = findChannel(layer, psd::channelType::TRANSPARENCY_MASK);
    const auto *alpha = alphaIndex == channelNotFound ? nullptr : static_cast<const uchar *>(layer.channels[alphaIndex].data);
    if (!red || !green || !blue) {
        return {};
    }

    QImage image(canvasWidth, canvasHeight, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }
    image.fill(Qt::transparent);

    for (int y = 0; y < layerHeight; ++y) {
        const int canvasY = static_cast<int>(layer.top) + y;
        if (canvasY < 0 || canvasY >= canvasHeight) {
            continue;
        }

        uchar *line = image.scanLine(canvasY);
        const qsizetype layerRowOffset = static_cast<qsizetype>(y) * layerWidth;
        for (int x = 0; x < layerWidth; ++x) {
            const int canvasX = static_cast<int>(layer.left) + x;
            if (canvasX < 0 || canvasX >= canvasWidth) {
                continue;
            }

            const qsizetype layerIndex = layerRowOffset + x;
            const int pixelIndex = canvasX * 4;
            line[pixelIndex] = red[layerIndex];
            line[pixelIndex + 1] = green[layerIndex];
            line[pixelIndex + 2] = blue[layerIndex];
            line[pixelIndex + 3] = alpha ? alpha[layerIndex] : 255;
        }
    }

    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

[[nodiscard]] QString xmpElementText(const QString &xmpMetadata, const QString &elementName)
{
    QXmlStreamReader reader(xmpMetadata);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == elementName) {
            return reader.readElementText().trimmed();
        }
    }
    return {};
}

[[nodiscard]] QVariantMap congregationManifestFromXmp(const QString &xmpMetadata)
{
    const QString encodedManifest = xmpElementText(xmpMetadata, QStringLiteral("CongregationLayerManifestBase64"));
    if (encodedManifest.isEmpty()) {
        return {};
    }

    const QByteArray manifestData = QByteArray::fromBase64(encodedManifest.toLatin1());
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestData);
    return manifestDocument.isObject() ? manifestDocument.object().toVariantMap() : QVariantMap();
}

void readImageResources(const psd::Document *document,
                        QFilePsdFile *file,
                        psd::Allocator *allocator,
                        PsdImportedDocument *imported)
{
    psd::ImageResourcesSection *imageResources = psd::ParseImageResourcesSection(document, file, allocator);
    const auto destroyImageResources = qScopeGuard([&] {
        psd::DestroyImageResourcesSection(imageResources, allocator);
    });
    if (!imageResources) {
        return;
    }

    imported->hasRealMergedImage = imageResources->containsRealMergedData;
    if (imageResources->xmpMetadata) {
        imported->xmpMetadata = QString::fromUtf8(imageResources->xmpMetadata);
        imported->congregationManifest = congregationManifestFromXmp(imported->xmpMetadata);
    }
}

void readLayerMaskSection(const psd::Document *document,
                          QFilePsdFile *file,
                          psd::Allocator *allocator,
                          PsdImportedDocument *imported)
{
    psd::LayerMaskSection *layerMaskSection = psd::ParseLayerMaskSection(document, file, allocator);
    const auto destroyLayerMask = qScopeGuard([&] {
        psd::DestroyLayerMaskSection(layerMaskSection, allocator);
    });
    if (!layerMaskSection || layerMaskSection->layerCount == 0) {
        return;
    }

    imported->layers.reserve(static_cast<qsizetype>(layerMaskSection->layerCount));
    for (unsigned int index = 0; index < layerMaskSection->layerCount; ++index) {
        psd::Layer *layer = &layerMaskSection->layers[index];
        psd::ExtractLayer(document, file, allocator, layer);

        PsdImportedLayer importedLayer;
        importedLayer.name = layerName(*layer);
        importedLayer.bounds = QRect(QPoint(layer->left, layer->top),
                                     QPoint(layer->right - 1, layer->bottom - 1)).normalized();
        importedLayer.blendModeKey = psdKeyToString(layer->blendModeKey);
        importedLayer.opacity = layer->opacity;
        importedLayer.visible = layer->isVisible;
        importedLayer.hasUserMask = layer->layerMask != nullptr;
        importedLayer.hasVectorMask = layer->vectorMask != nullptr;
        importedLayer.image = imageFromLayerData(*document, *layer);
        if (!importedLayer.image.isNull()) {
            imported->layers.append(importedLayer);
        }
    }
}

} // namespace

bool PsdImageReader::canReadPath(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("psd"), Qt::CaseInsensitive) == 0;
}

QImage PsdImageReader::readMergedImage(const QString &filePath)
{
    if (!canReadPath(filePath)) {
        return {};
    }

    psd::MallocAllocator allocator;
    QFilePsdFile file(&allocator, filePath);
    if (!file.isOpen()) {
        return {};
    }

    psd::Document *document = psd::CreateDocument(&file, &allocator);
    const auto destroyDocument = qScopeGuard([&] {
        psd::DestroyDocument(document, &allocator);
    });
    if (!document || document->imageDataSection.length == 0) {
        return {};
    }

    psd::ImageDataSection *imageData = psd::ParseImageDataSection(document, &file, &allocator);
    const auto destroyImageData = qScopeGuard([&] {
        psd::DestroyImageDataSection(imageData, &allocator);
    });
    if (!imageData) {
        return {};
    }

    return imageFromMergedData(*document, *imageData);
}

PsdImportedDocument PsdImageReader::readDocument(const QString &filePath)
{
    PsdImportedDocument imported;
    if (!canReadPath(filePath)) {
        return imported;
    }

    psd::MallocAllocator allocator;
    QFilePsdFile file(&allocator, filePath);
    if (!file.isOpen()) {
        return imported;
    }

    psd::Document *document = psd::CreateDocument(&file, &allocator);
    const auto destroyDocument = qScopeGuard([&] {
        psd::DestroyDocument(document, &allocator);
    });
    if (!document) {
        return imported;
    }

    imported.canvasSize = QSize(static_cast<int>(document->width), static_cast<int>(document->height));
    imported.bitsPerChannel = static_cast<int>(document->bitsPerChannel);
    imported.colorMode = static_cast<int>(document->colorMode);
    if (document->bitsPerChannel != 8) {
        imported.compatibilityWarnings.append(QStringLiteral("PSD bit depth is imported only as metadata unless it is 8-bit."));
    }
    if (document->colorMode != psd::colorMode::RGB) {
        imported.compatibilityWarnings.append(QStringLiteral("PSD color mode is imported only as metadata unless it is RGB."));
    }

    readImageResources(document, &file, &allocator, &imported);
    readLayerMaskSection(document, &file, &allocator, &imported);

    if (document->imageDataSection.length > 0) {
        psd::ImageDataSection *imageData = psd::ParseImageDataSection(document, &file, &allocator);
        const auto destroyImageData = qScopeGuard([&] {
            psd::DestroyImageDataSection(imageData, &allocator);
        });
        if (imageData) {
            imported.mergedImage = imageFromMergedData(*document, *imageData);
        }
    }

    if (imported.layers.isEmpty()) {
        imported.compatibilityWarnings.append(QStringLiteral("PSD does not expose importable 8-bit RGB/RGBA raster layers."));
    }

    return imported;
}
