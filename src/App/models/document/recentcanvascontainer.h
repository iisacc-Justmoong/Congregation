#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantList>

enum class RecentCanvasAssetKind : quint8
{
    Image = 1,
    RasterLayer = 2,
};

struct RecentCanvasEmbeddedAsset
{
    int objectId = 0;
    RecentCanvasAssetKind kind = RecentCanvasAssetKind::Image;
    QByteArray pngData;
};

struct RecentCanvasContainer
{
    QByteArray sharedCanvasDocument;
    QVariantList drawableObjects;
    QList<RecentCanvasEmbeddedAsset> embeddedAssets;
    bool backgroundLayerPresent = true;
};

struct RecentCanvasDecodeResult
{
    RecentCanvasContainer container;
    QString error;

    [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

inline constexpr qint64 RecentCanvasMaximumContainerBytes = 512LL * 1024LL * 1024LL;

[[nodiscard]] QByteArray encodeRecentCanvasContainer(const RecentCanvasContainer& container,
                                                     QString* error = nullptr);
[[nodiscard]] RecentCanvasDecodeResult decodeRecentCanvasContainer(const QByteArray& bytes);
