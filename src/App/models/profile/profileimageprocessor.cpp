#include "profileimageprocessor.h"

#include <QColorSpace>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QTemporaryFile>

#include <algorithm>

ProfileImageProcessor::ProfileImageProcessor(QObject *parent)
    : QObject(parent)
{
}

ProfileImageProcessor::~ProfileImageProcessor() = default;

QUrl ProfileImageProcessor::imageSource() const
{
    return m_imageSource;
}

QString ProfileImageProcessor::errorString() const
{
    return m_errorString;
}

bool ProfileImageProcessor::processProfileImage(const QUrl &sourceUrl)
{
    if (!sourceUrl.isLocalFile()) {
        setErrorString(tr("The profile image must be a local file."));
        return false;
    }

    QImageReader reader(sourceUrl.toLocalFile());
    reader.setAutoTransform(true);
    const QImage sourceImage = reader.read();
    if (sourceImage.isNull()) {
        setErrorString(reader.errorString().isEmpty()
                           ? tr("The profile image could not be read.")
                           : reader.errorString());
        return false;
    }

    const int cropSize = std::min(sourceImage.width(), sourceImage.height());
    if (cropSize <= 0) {
        setErrorString(tr("The profile image has no usable pixels."));
        return false;
    }

    const QRect sourceRect((sourceImage.width() - cropSize) / 2,
                           (sourceImage.height() - cropSize) / 2,
                           cropSize,
                           cropSize);
    QImage circularImage(cropSize, cropSize, QImage::Format_ARGB32_Premultiplied);
    circularImage.fill(Qt::transparent);
    circularImage.setColorSpace(sourceImage.colorSpace());
    circularImage.setDotsPerMeterX(sourceImage.dotsPerMeterX());
    circularImage.setDotsPerMeterY(sourceImage.dotsPerMeterY());

    QPainterPath circularClip;
    circularClip.addEllipse(QRectF(0, 0, cropSize, cropSize));

    QPainter painter(&circularImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipPath(circularClip);
    painter.drawImage(QPoint(0, 0), sourceImage, sourceRect);
    painter.end();

    auto processedImageFile = std::make_unique<QTemporaryFile>(
        QDir(QDir::tempPath()).filePath(QStringLiteral("Congregation-profile-image-XXXXXX.png")));
    if (!processedImageFile->open()) {
        setErrorString(processedImageFile->errorString());
        return false;
    }
    if (!circularImage.save(processedImageFile.get(), "PNG")) {
        setErrorString(tr("The circular profile image could not be encoded."));
        return false;
    }
    if (!processedImageFile->flush()) {
        setErrorString(processedImageFile->errorString());
        return false;
    }

    const QUrl processedImageSource = QUrl::fromLocalFile(processedImageFile->fileName());
    processedImageFile->close();
    m_processedImageFile = std::move(processedImageFile);
    setErrorString({});
    setImageSource(processedImageSource);
    return true;
}

void ProfileImageProcessor::clearProfileImage()
{
    setErrorString({});
    setImageSource({});
    m_processedImageFile.reset();
}

void ProfileImageProcessor::setImageSource(const QUrl &imageSource)
{
    if (m_imageSource == imageSource) {
        return;
    }
    m_imageSource = imageSource;
    emit imageSourceChanged();
}

void ProfileImageProcessor::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }
    m_errorString = errorString;
    emit errorStringChanged();
}
