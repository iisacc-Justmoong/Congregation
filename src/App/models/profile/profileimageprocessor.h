#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class QTemporaryFile;

class ProfileImageProcessor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY imageSourceChanged FINAL)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged FINAL)

public:
    explicit ProfileImageProcessor(QObject *parent = nullptr);
    ~ProfileImageProcessor() override;

    [[nodiscard]] QUrl imageSource() const;
    [[nodiscard]] QString errorString() const;

    Q_INVOKABLE bool processProfileImage(const QUrl &sourceUrl);
    Q_INVOKABLE void clearProfileImage();

signals:
    void imageSourceChanged();
    void errorStringChanged();

private:
    void setImageSource(const QUrl &imageSource);
    void setErrorString(const QString &errorString);

    QUrl m_imageSource;
    QString m_errorString;
    std::unique_ptr<QTemporaryFile> m_processedImageFile;
};
