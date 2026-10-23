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

};
