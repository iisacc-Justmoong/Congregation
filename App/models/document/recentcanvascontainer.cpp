#include "recentcanvascontainer.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <limits>
#include <utility>

namespace
{
const QByteArray containerMagic = QByteArrayLiteral("CONGREGATIONRC\r\n\x1a\n");
constexpr quint32 containerVersion = 1;
constexpr qsizetype checksumSize = 32;
constexpr qsizetype maximumManifestBytes = 1024 * 1024;
constexpr qsizetype maximumSharedCanvasBytes = 256 * 1024 * 1024;
constexpr qsizetype maximumEmbeddedAssetBytes = 256 * 1024 * 1024;
constexpr qsizetype maximumObjectCount = 256;
constexpr qsizetype maximumEmbeddedAssetCount = 256;

void configureStream(QDataStream& stream)
{
    stream.setVersion(QDataStream::Qt_6_8);
    stream.setByteOrder(QDataStream::BigEndian);
}

QString assetIdentity(const RecentCanvasEmbeddedAsset& asset)
{
    return QStringLiteral("%1:%2").arg(asset.objectId).arg(static_cast<quint8>(asset.kind));
}

bool validAssetKind(RecentCanvasAssetKind kind)
{
    return kind == RecentCanvasAssetKind::Image || kind == RecentCanvasAssetKind::RasterLayer;
}

bool readBoundedByteArray(QDataStream& stream, qsizetype maximumSize, QByteArray& bytes)
{
    quint32 rawSize = 0;
    stream >> rawSize;
    QIODevice* device = stream.device();
    if (stream.status() != QDataStream::Ok || !device ||
        rawSize == std::numeric_limits<quint32>::max() ||
        static_cast<quint64>(rawSize) > static_cast<quint64>(maximumSize) ||
        static_cast<qint64>(rawSize) > device->bytesAvailable())
    {
        return false;
    }

    QByteArray decoded(static_cast<qsizetype>(rawSize), Qt::Uninitialized);
    if (rawSize > 0 &&
        stream.readRawData(decoded.data(), static_cast<int>(rawSize)) != static_cast<int>(rawSize))
    {
        return false;
    }
    bytes = std::move(decoded);
    return true;
}

RecentCanvasDecodeResult failure(const QString& error)
{
    RecentCanvasDecodeResult result;
    result.error = error;
    return result;
}
} // namespace

QByteArray encodeRecentCanvasContainer(const RecentCanvasContainer& container, QString* error)
{
    const auto fail = [error](const QString& message)
    {
        if (error)
        {
            *error = message;
        }
        return QByteArray{};
    };

    if (container.sharedCanvasDocument.isEmpty() ||
        container.sharedCanvasDocument.size() > maximumSharedCanvasBytes)
    {
        return fail(QStringLiteral("invalid shared canvas document"));
    }
    if (container.drawableObjects.size() > maximumObjectCount ||
        container.embeddedAssets.size() > maximumEmbeddedAssetCount)
    {
        return fail(QStringLiteral("recent canvas object limit exceeded"));
    }

    QSet<QString> assetIdentities;
    for (const RecentCanvasEmbeddedAsset& asset : container.embeddedAssets)
    {
        if (asset.objectId <= 0 || !validAssetKind(asset.kind) || asset.pngData.isEmpty() ||
            asset.pngData.size() > maximumEmbeddedAssetBytes ||
            assetIdentities.contains(assetIdentity(asset)))
        {
            return fail(QStringLiteral("invalid embedded asset"));
        }
        assetIdentities.insert(assetIdentity(asset));
    }

    const QJsonObject manifest{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("backgroundLayerPresent"), container.backgroundLayerPresent},
        {QStringLiteral("drawableObjects"), QJsonArray::fromVariantList(container.drawableObjects)},
    };
    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    if (manifestBytes.isEmpty() || manifestBytes.size() > maximumManifestBytes)
    {
        return fail(QStringLiteral("invalid recent canvas manifest"));
    }

    QByteArray payload;
    QDataStream payloadStream(&payload, QIODevice::WriteOnly);
    configureStream(payloadStream);
    payloadStream << manifestBytes << container.sharedCanvasDocument
                  << static_cast<quint32>(container.embeddedAssets.size());
    for (const RecentCanvasEmbeddedAsset& asset : container.embeddedAssets)
    {
        payloadStream << static_cast<qint32>(asset.objectId) << static_cast<quint8>(asset.kind)
                      << asset.pngData;
    }
    if (payloadStream.status() != QDataStream::Ok ||
        payload.size() > RecentCanvasMaximumContainerBytes)
    {
        return fail(QStringLiteral("recent canvas payload is too large"));
    }

    const QByteArray checksum = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    configureStream(stream);
    if (stream.writeRawData(containerMagic.constData(), containerMagic.size()) !=
        containerMagic.size())
    {
        return fail(QStringLiteral("recent canvas header write failed"));
    }
    stream << containerVersion << static_cast<quint64>(payload.size());
    if (stream.writeRawData(checksum.constData(), checksum.size()) != checksum.size() ||
        stream.writeRawData(payload.constData(), payload.size()) != payload.size() ||
        stream.status() != QDataStream::Ok || encoded.size() > RecentCanvasMaximumContainerBytes)
    {
        return fail(QStringLiteral("recent canvas write failed"));
    }

    if (error)
    {
        error->clear();
    }
    return encoded;
}

RecentCanvasDecodeResult decodeRecentCanvasContainer(const QByteArray& bytes)
{
    const qsizetype minimumSize = containerMagic.size() +
                                  static_cast<qsizetype>(sizeof(quint32) + sizeof(quint64)) +
                                  checksumSize;
    if (bytes.size() < minimumSize || bytes.size() > RecentCanvasMaximumContainerBytes)
    {
        return failure(QStringLiteral("invalid recent canvas size"));
    }

    QBuffer encodedBuffer;
    encodedBuffer.setData(bytes);
    if (!encodedBuffer.open(QIODevice::ReadOnly))
    {
        return failure(QStringLiteral("recent canvas cannot be read"));
    }
    QDataStream stream(&encodedBuffer);
    configureStream(stream);

    QByteArray magic(containerMagic.size(), Qt::Uninitialized);
    if (stream.readRawData(magic.data(), magic.size()) != magic.size() || magic != containerMagic)
    {
        return failure(QStringLiteral("invalid recent canvas magic"));
    }

    quint32 version = 0;
    quint64 payloadSize = 0;
    stream >> version >> payloadSize;
    if (stream.status() != QDataStream::Ok || version != containerVersion ||
        payloadSize > static_cast<quint64>(RecentCanvasMaximumContainerBytes) ||
        payloadSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max()))
    {
        return failure(QStringLiteral("unsupported recent canvas version"));
    }

    QByteArray expectedChecksum(checksumSize, Qt::Uninitialized);
    if (stream.readRawData(expectedChecksum.data(), expectedChecksum.size()) !=
        expectedChecksum.size())
    {
        return failure(QStringLiteral("recent canvas checksum is missing"));
    }
    if (payloadSize != static_cast<quint64>(encodedBuffer.bytesAvailable()))
    {
        return failure(QStringLiteral("recent canvas payload size mismatch"));
    }

    QByteArray payload(static_cast<qsizetype>(payloadSize), Qt::Uninitialized);
    if (stream.readRawData(payload.data(), payload.size()) != payload.size() ||
        !encodedBuffer.atEnd())
    {
        return failure(QStringLiteral("recent canvas payload is incomplete"));
    }
    const QByteArray actualChecksum = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    if (actualChecksum != expectedChecksum)
    {
        return failure(QStringLiteral("recent canvas checksum mismatch"));
    }

    QBuffer payloadBuffer;
    payloadBuffer.setData(payload);
    if (!payloadBuffer.open(QIODevice::ReadOnly))
    {
        return failure(QStringLiteral("recent canvas payload cannot be read"));
    }
    QDataStream payloadStream(&payloadBuffer);
    configureStream(payloadStream);

    QByteArray manifestBytes;
    QByteArray sharedCanvasDocument;
    quint32 assetCount = 0;
    if (!readBoundedByteArray(payloadStream, maximumManifestBytes, manifestBytes) ||
        !readBoundedByteArray(payloadStream, maximumSharedCanvasBytes, sharedCanvasDocument))
    {
        return failure(QStringLiteral("invalid recent canvas payload"));
    }
    payloadStream >> assetCount;
    if (payloadStream.status() != QDataStream::Ok || manifestBytes.isEmpty() ||
        sharedCanvasDocument.isEmpty() || assetCount > maximumEmbeddedAssetCount)
    {
        return failure(QStringLiteral("invalid recent canvas payload"));
    }

    QList<RecentCanvasEmbeddedAsset> assets;
    assets.reserve(static_cast<qsizetype>(assetCount));
    QSet<QString> assetIdentities;
    for (quint32 index = 0; index < assetCount; ++index)
    {
        qint32 objectId = 0;
        quint8 rawKind = 0;
        QByteArray pngData;
        payloadStream >> objectId >> rawKind;
        if (!readBoundedByteArray(payloadStream, maximumEmbeddedAssetBytes, pngData))
        {
            return failure(QStringLiteral("invalid recent canvas embedded asset"));
        }
        const auto kind = static_cast<RecentCanvasAssetKind>(rawKind);
        RecentCanvasEmbeddedAsset asset{objectId, kind, std::move(pngData)};
        if (payloadStream.status() != QDataStream::Ok || asset.objectId <= 0 ||
            !validAssetKind(asset.kind) || asset.pngData.isEmpty() ||
            asset.pngData.size() > maximumEmbeddedAssetBytes ||
            assetIdentities.contains(assetIdentity(asset)))
        {
            return failure(QStringLiteral("invalid recent canvas embedded asset"));
        }
        assetIdentities.insert(assetIdentity(asset));
        assets.append(std::move(asset));
    }
    if (payloadStream.status() != QDataStream::Ok || !payloadBuffer.atEnd())
    {
        return failure(QStringLiteral("recent canvas payload has trailing data"));
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject())
    {
        return failure(QStringLiteral("invalid recent canvas manifest"));
    }
    const QJsonObject manifest = manifestDocument.object();
    if (manifest.value(QStringLiteral("schema")).toInt() != 1 ||
        !manifest.value(QStringLiteral("backgroundLayerPresent")).isBool() ||
        !manifest.value(QStringLiteral("drawableObjects")).isArray() ||
        manifest.value(QStringLiteral("drawableObjects")).toArray().size() > maximumObjectCount)
    {
        return failure(QStringLiteral("unsupported recent canvas manifest"));
    }

    RecentCanvasDecodeResult result;
    result.container.sharedCanvasDocument = std::move(sharedCanvasDocument);
    result.container.drawableObjects =
        manifest.value(QStringLiteral("drawableObjects")).toArray().toVariantList();
    result.container.embeddedAssets = std::move(assets);
    result.container.backgroundLayerPresent =
        manifest.value(QStringLiteral("backgroundLayerPresent")).toBool();
    return result;
}
