#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QResource>
#include <QtTest>

void qml_register_types_LVRS();

class LvrsResources final : public QObject
{
    Q_OBJECT
  private slots:
    void embeddedAssetsRemainAvailable()
    {
        qml_register_types_LVRS();
        const QDir source(QString::fromUtf8(CONGREGATION_LVRS_SOURCE_RESOURCES));
        QVERIFY(source.exists());
        const QStringList embeddedExtensions{QStringLiteral("svg"), QStringLiteral("ttf"),
                                             QStringLiteral("otf"), QStringLiteral("png"),
                                             QStringLiteral("jpg"), QStringLiteral("webp"),
                                             QStringLiteral("qsb")};
        QDirIterator files(source.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        qsizetype checked = 0;
        while (files.hasNext())
        {
            const QString path = files.next();
            if (!embeddedExtensions.contains(QFileInfo(path).suffix().toLower()))
                continue;
            const QString resourcePath =
                QStringLiteral(":/qt/qml/LVRS/resources/") + source.relativeFilePath(path);
            const QResource resource(resourcePath);
            QVERIFY2(resource.isValid() && resource.size() > 0, qPrintable(resourcePath));
            ++checked;
        }
        QVERIFY(checked > 0);
        qInfo() << "Verified LVRS embedded assets:" << checked;
    }
};

QTEST_GUILESS_MAIN(LvrsResources)
#include "tst_lvrsresources.moc"
