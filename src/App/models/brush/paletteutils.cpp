#include "paletteutils.h"

#include <QSet>
#include <QtMath>

#include <algorithm>

PaletteUtils::PaletteUtils(QObject *parent)
    : QObject(parent)
{
}

QVariantList PaletteUtils::buildDefaultPalette(const QVariantList &primary,
                                               const QVariantList &extended) const
{
    const QVector<PaletteEntry> primaryEntries = toEntries(primary);
    const QVector<PaletteEntry> extendedEntries = toEntries(extended);
    const QVector<PaletteEntry> merged = mergeUnique(primaryEntries, extendedEntries);

    QVector<PaletteBucket> neutrals;
    QVector<PaletteBucket> colored;
    neutrals.reserve(merged.size());
    colored.reserve(merged.size());

    for (const PaletteEntry &entry : merged) {
        int hue = 0;
        int saturation = 0;
        int lightness = 0;
        rgbToHsl(entry.color, hue, saturation, lightness);

        PaletteBucket bucket;
        bucket.entry = entry;
        bucket.hue = hue;
        bucket.saturation = saturation;
        bucket.lightness = lightness;

        if (bucket.saturation < 15) {
            neutrals.push_back(bucket);
        } else {
            colored.push_back(bucket);
        }
    }

    std::sort(neutrals.begin(), neutrals.end(), [](const PaletteBucket &a, const PaletteBucket &b) {
        return a.lightness < b.lightness;
    });

    std::sort(colored.begin(), colored.end(), [](const PaletteBucket &a, const PaletteBucket &b) {
        if (a.hue == b.hue) {
            return a.lightness < b.lightness;
        }
        return a.hue < b.hue;
    });

    QVector<PaletteEntry> result;
    result.reserve(merged.size());

    for (const PaletteBucket &bucket : neutrals) {
        if (bucket.lightness < 50) {
            result.push_back(bucket.entry);
        }
    }
    for (const PaletteBucket &bucket : colored) {
        result.push_back(bucket.entry);
    }
    for (const PaletteBucket &bucket : neutrals) {
        if (bucket.lightness >= 50) {
            result.push_back(bucket.entry);
        }
    }

    return toVariantList(result);
}

QVector<PaletteUtils::PaletteEntry> PaletteUtils::toEntries(const QVariantList &entries)
{
    QVector<PaletteEntry> result;
    result.reserve(entries.size());

    for (const QVariant &entryVar : entries) {
        if (!entryVar.isValid()) {
            continue;
        }
        const QVariantMap map = entryVar.toMap();
        if (map.isEmpty()) {
            continue;
        }

        const QVariant colorVar = map.value(QStringLiteral("color"));
        QColor color;
        if (colorVar.canConvert<QColor>()) {
            color = colorVar.value<QColor>();
        }
        if (!color.isValid()) {
            const QString colorString = colorVar.toString();
            if (!colorString.isEmpty()) {
                color = QColor(colorString);
            }
        }
        if (!color.isValid()) {
            continue;
        }

        PaletteEntry entry;
        entry.name = map.value(QStringLiteral("name")).toString();
        entry.color = color;
        result.push_back(entry);
    }

    return result;
}

QVector<PaletteUtils::PaletteEntry> PaletteUtils::mergeUnique(const QVector<PaletteEntry> &primary,
                                                              const QVector<PaletteEntry> &extended)
{
    QVector<PaletteEntry> merged;
    merged.reserve(primary.size() + extended.size());

    QSet<QString> seen;

    auto appendEntry = [&](const PaletteEntry &entry) {
        if (!entry.color.isValid()) {
            return;
        }
        const QString key = entry.color.name(QColor::HexRgb).toLower();
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        merged.push_back(entry);
    };

    for (const PaletteEntry &entry : primary) {
        appendEntry(entry);
    }
    for (const PaletteEntry &entry : extended) {
        appendEntry(entry);
    }

    return merged;
}

void PaletteUtils::rgbToHsl(const QColor &color, int &hue, int &saturation, int &lightness)
{
    const double red = color.redF();
    const double green = color.greenF();
    const double blue = color.blueF();

    const double max = std::max({red, green, blue});
    const double min = std::min({red, green, blue});
    const double l = (max + min) / 2.0;

    double h = 0.0;
    double s = 0.0;

    if (max != min) {
        const double d = max - min;
        s = l > 0.5 ? d / (2.0 - max - min) : d / (max + min);

        if (max == red) {
            h = (green - blue) / d + (green < blue ? 6.0 : 0.0);
        } else if (max == green) {
            h = (blue - red) / d + 2.0;
        } else {
            h = (red - green) / d + 4.0;
        }
        h /= 6.0;
    }

    hue = qRound(h * 360.0);
    saturation = qRound(s * 100.0);
    lightness = qRound(l * 100.0);
}

QVariantList PaletteUtils::toVariantList(const QVector<PaletteEntry> &entries)
{
    QVariantList list;
    list.reserve(entries.size());

    for (const PaletteEntry &entry : entries) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), entry.name);
        map.insert(QStringLiteral("color"), entry.color);
        list.push_back(map);
    }

    return list;
}
