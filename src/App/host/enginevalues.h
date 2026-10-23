#pragma once

#include <Engine/Palette.h>
#include <QColor>
#include <QVariantList>
#include <QVariantMap>

namespace congregation::presentation
{
inline Color fromColor(const QColor& color)
{
    return {color.redF(), color.greenF(), color.blueF(), color.alphaF(), color.isValid()};
}

inline QColor toColor(const Color& color)
{
    return color.valid ? QColor::fromRgbF(color.red, color.green, color.blue, color.alpha)
                       : QColor{};
}

inline QVariantList toPalette(const std::vector<PaletteEntry>& entries)
{
    QVariantList result;
    result.reserve(entries.size());
    for (const auto& entry : entries)
    {
        result.push_back(QVariantMap{{QStringLiteral("name"), QString::fromStdString(entry.name)},
                                     {QStringLiteral("color"), toColor(entry.color)}});
    }
    return result;
}
} // namespace congregation::presentation
