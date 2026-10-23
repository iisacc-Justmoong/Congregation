#pragma once
#include "nativepaths.h"
#include <QBuffer>
#include <QImage>
namespace congregation::presentation
{
inline bool saveImage(const QString& path, const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return false;
    try
    {
        writeFile(path, bytes);
        return true;
    }
    catch (const iiFileProvider::native::FileError&)
    {
        return false;
    }
}
} // namespace congregation::presentation
