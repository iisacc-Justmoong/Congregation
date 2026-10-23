#include "paletteutils.h"
#include "../../host/enginevalues.h"

namespace
{
std::vector<congregation::PaletteEntry> toEntries(const QVariantList &values)
{
    std::vector<congregation::PaletteEntry> entries;
    entries.reserve(values.size());
    for (const auto &value : values)
    {
        const auto map = value.toMap();
        const auto variant = map.value(QStringLiteral("color"));
        QColor color = variant.value<QColor>();
        if (!color.isValid()) color = QColor(variant.toString());
        entries.push_back({map.value(QStringLiteral("name")).toString().toStdString(),
                           congregation::presentation::fromColor(color)});
    }
    return entries;
}
} // namespace

PaletteUtils::PaletteUtils(QObject *parent) : QObject(parent)
{
}

QVariantList PaletteUtils::buildDefaultPalette(const QVariantList &primary,
                                               const QVariantList &extended) const
{
    const auto primaryEntries = toEntries(primary);
    const auto extendedEntries = toEntries(extended);
    return congregation::presentation::toPalette(congregation::buildPalette(primaryEntries, extendedEntries));
}
