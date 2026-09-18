#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantMap>

struct PsdImportedLayer {
    QString name;
    QRect bounds;
    QString blendModeKey;
    int opacity = 255;
    bool visible = true;
    bool hasUserMask = false;
    bool hasVectorMask = false;
    QImage image;
};

struct PsdImportedDocument {
    QSize canvasSize;
    int bitsPerChannel = 0;
    int colorMode = 0;
    bool hasRealMergedImage = false;
    QImage mergedImage;
    QString xmpMetadata;
    QVariantMap congregationManifest;
    QList<PsdImportedLayer> layers;
    QStringList compatibilityWarnings;

    [[nodiscard]] bool isValid() const
    {
        return canvasSize.isValid() && (!mergedImage.isNull() || !layers.isEmpty());
    }
};

class PsdImageReader
{
public:
    [[nodiscard]] static bool canReadPath(const QString &filePath);
    [[nodiscard]] static QImage readMergedImage(const QString &filePath);
    [[nodiscard]] static PsdImportedDocument readDocument(const QString &filePath);
};
