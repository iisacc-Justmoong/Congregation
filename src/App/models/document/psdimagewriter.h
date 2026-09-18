#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <QVariantMap>

class PsdImageWriter
{
public:
    struct Layer {
        QString name;
        QRect bounds;
        QImage image;
    };

    [[nodiscard]] static bool canWritePath(const QString &filePath);
    [[nodiscard]] static bool writeLayeredImage(const QString &filePath,
                                                const QImage &mergedImage,
                                                const QList<Layer> &bottomToTopLayers,
                                                const QVariantMap &manifest);
};
