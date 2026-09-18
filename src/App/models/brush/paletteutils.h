#pragma once

#include <QObject>
#include <QVariantList>

#include <QColor>
#include <QString>
#include <QVector>

class PaletteUtils : public QObject
{
    Q_OBJECT

public:
    explicit PaletteUtils(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList buildDefaultPalette(const QVariantList &primary,
                                                 const QVariantList &extended) const;

private:
    struct PaletteEntry
    {
        QString name;
        QColor color;
    };

    struct PaletteBucket
    {
        PaletteEntry entry;
        int hue = 0;
        int saturation = 0;
        int lightness = 0;
    };

    static QVector<PaletteEntry> toEntries(const QVariantList &entries);
    static QVector<PaletteEntry> mergeUnique(const QVector<PaletteEntry> &primary,
                                             const QVector<PaletteEntry> &extended);
    static void rgbToHsl(const QColor &color, int &hue, int &saturation, int &lightness);
    static QVariantList toVariantList(const QVector<PaletteEntry> &entries);
};
