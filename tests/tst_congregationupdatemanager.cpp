#include "licensecredentialstore.h"
#include "licensemanager.h"
#include "congregationupdatecredentialprovider.h"
#include "congregationupdatemanager.h"

#include <iiUpdateManager/UpdateCredentialProvider.h>

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>
#include <utility>

namespace
{
const QString validLicenseKey = QStringLiteral("IIL1_0123456789abcdefghijklmnopqrstuv");

QByteArray storedCredentials()
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("email"), QStringLiteral("verified@example.com")},
        {QStringLiteral("licenseKey"), validLicenseKey},
        {QStringLiteral("productId"), QStringLiteral("congregation")},
    }).toJson(QJsonDocument::Compact);
}

class FakeCredentialStore final : public LicenseCredentialStore
{
public:
    bool supported = true;
    ReadStatus readStatus = ReadStatus::NotFound;
    QByteArray readData;
    bool completeReadsImmediately = true;
    int readCount = 0;
    ReadCompletion pendingRead;

    [[nodiscard]] bool persistenceSupported() const override
    {
        return supported;
    }

    void read(ReadCompletion completion) override
    {
        ++readCount;
        if (completeReadsImmediately) {
            completion(readStatus, readData);
        } else {
            pendingRead = std::move(completion);
        }
    }

    void write(const QByteArray &, MutationCompletion completion) override
    {
        completion(true);
    }

    void remove(MutationCompletion completion) override
    {
        completion(true);
    }

    void completeRead()
    {
        ReadCompletion completion = std::move(pendingRead);
        pendingRead = {};
        if (completion) {
            completion(readStatus, readData);
        }
    }
};

QString platformKey()
{
#if defined(Q_OS_MACOS)
    const QString platform = QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    const QString platform = QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    const QString platform = QStringLiteral("linux");
#else
    const QString platform = QStringLiteral("unknown");
#endif

#if defined(Q_PROCESSOR_ARM_64)
    const QString architecture = QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64)
    const QString architecture = QStringLiteral("x64");
#elif defined(Q_PROCESSOR_ARM_32)
    const QString architecture = QStringLiteral("arm32");
#elif defined(Q_PROCESSOR_X86_32)
    const QString architecture = QStringLiteral("x86");
#else
    const QString architecture = QStringLiteral("unknown");
#endif
    return platform + QLatin1Char('-') + architecture;
}

class ManifestServer final : public QTcpServer
{
public:
    explicit ManifestServer(QString version, QObject *parent = nullptr)
        : QTcpServer(parent)
    {
        const QJsonObject manifest{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("productId"), QStringLiteral("congregation")},
            {QStringLiteral("releases"), QJsonObject{
                 {platformKey(), QJsonObject{
                      {QStringLiteral("version"), std::move(version)},
                      {QStringLiteral("updateUrl"),
                       QStringLiteral("https://iisacc.com/Store/Congregation/Download")},
                  }},
             }},
        };
        m_body = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (hasPendingConnections()) {
                QTcpSocket *socket = nextPendingConnection();
                ++m_connectionCount;
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    socket->readAll();
                    const QByteArray response = QByteArrayLiteral(
                                                    "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Connection: close\r\n"
                                                    "Content-Length: ")
                        + QByteArray::number(m_body.size())
                        + QByteArrayLiteral("\r\n\r\n")
                        + m_body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    [[nodiscard]] QUrl manifestUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/manifest.json").arg(serverPort()));
    }

    [[nodiscard]] QUrl grantUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/grant").arg(serverPort()));
    }

    [[nodiscard]] int connectionCount() const noexcept
    {
        return m_connectionCount;
    }

private:
    QByteArray m_body;
    int m_connectionCount = 0;
};
}

class tst_CongregationUpdateManager final : public QObject
{
    Q_OBJECT

private slots:
    void credentialProviderReadsTheExistingStoreOnlyWhenRequested();
    void credentialProviderDistinguishesInvalidStoredJson();
    void credentialProviderCancellationSuppressesLateSecrets();
    void constructionDoesNotCheckAndOneManualCheckMakesOneRequest();
    void updateNowAloneReadsCredentialsAfterAnAvailableCheck();
    void storeManagedBuildRejectsSelfUpdateWithoutNetworkOrCredentialRead();
    void macStoreMarkerOrReceiptDisablesSelfUpdate();
};

void tst_CongregationUpdateManager::credentialProviderReadsTheExistingStoreOnlyWhenRequested()
{
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    CongregationUpdateCredentialProvider provider(&licenseManager);
    QCOMPARE(store.readCount, 1);
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    bool completed = false;
    provider.requestCredentials(
        [&completed](std::optional<iisacc::updates::UpdateCredentials> credentials,
                     const QString &error) {
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY(credentials.has_value());
            QCOMPARE(credentials->email(), QStringLiteral("verified@example.com"));
            QCOMPARE(credentials->licenseKey(), validLicenseKey.toUtf8());
            completed = true;
        });

    QVERIFY(completed);
    QCOMPARE(store.readCount, 2);
}

void tst_CongregationUpdateManager::credentialProviderDistinguishesInvalidStoredJson()
{
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = QByteArrayLiteral("{}");

    CongregationUpdateCredentialProvider provider(&licenseManager);
    bool completed = false;
    provider.requestCredentials(
        [&completed](std::optional<iisacc::updates::UpdateCredentials> credentials,
                     const QString &error) {
            QVERIFY(credentials.has_value());
            QVERIFY(!credentials->isValid());
            QVERIFY(!error.isEmpty());
            completed = true;
        });

    QVERIFY(completed);
    QCOMPARE(store.readCount, 2);
}

void tst_CongregationUpdateManager::credentialProviderCancellationSuppressesLateSecrets()
{
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();
    store.completeReadsImmediately = false;

    CongregationUpdateCredentialProvider provider(&licenseManager);
    bool completed = false;
    provider.requestCredentials(
        [&completed](std::optional<iisacc::updates::UpdateCredentials>, const QString &) {
            completed = true;
        });
    QCOMPARE(store.readCount, 2);

    provider.cancel();
    store.completeRead();
    QVERIFY(!completed);
}

void tst_CongregationUpdateManager::constructionDoesNotCheckAndOneManualCheckMakesOneRequest()
{
    ManifestServer server(QStringLiteral("1.0.0"));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    CongregationUpdateManager updateManager(&licenseManager,
                                       QStringLiteral("1.0.0"),
                                       server.manifestUrl(),
                                       server.grantUrl(),
                                       true);

    QCOMPARE(updateManager.state(), CongregationUpdateManager::State::Idle);
    QCOMPARE(updateManager.progress(), 0.0);
    QCOMPARE(server.connectionCount(), 0);
    QTest::qWait(25);
    QCOMPARE(server.connectionCount(), 0);

    QVERIFY(updateManager.checkForUpdates());
    QTRY_COMPARE_WITH_TIMEOUT(updateManager.state(),
                              CongregationUpdateManager::State::UpToDate,
                              2000);
    QCOMPARE(server.connectionCount(), 1);
    QVERIFY(!updateManager.title().isEmpty());
    QVERIFY(!updateManager.message().isEmpty());
}

void tst_CongregationUpdateManager::updateNowAloneReadsCredentialsAfterAnAvailableCheck()
{
    ManifestServer server(QStringLiteral("1.1.0"));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    CongregationUpdateManager updateManager(&licenseManager,
                                       QStringLiteral("1.0.0"),
                                       server.manifestUrl(),
                                       server.grantUrl(),
                                       true);

    QCOMPARE(store.readCount, 1);
    QVERIFY(updateManager.checkForUpdates());
    QTRY_COMPARE_WITH_TIMEOUT(updateManager.state(),
                              CongregationUpdateManager::State::UpdateAvailable,
                              2000);
    QCOMPARE(server.connectionCount(), 1);
    QCOMPARE(store.readCount, 1);

    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();
    store.completeReadsImmediately = false;

    QVERIFY(updateManager.updateNow());
    QCOMPARE(updateManager.state(), CongregationUpdateManager::State::Authorizing);
    QVERIFY(updateManager.canCancel());
    QCOMPARE(store.readCount, 2);
    QCOMPARE(server.connectionCount(), 1);

    QVERIFY(updateManager.cancelUpdate());
    QCOMPARE(updateManager.state(), CongregationUpdateManager::State::UpdateAvailable);
    store.completeRead();
    QTest::qWait(25);
    QCOMPARE(server.connectionCount(), 1);
    QCOMPARE(store.readCount, 2);
}

void tst_CongregationUpdateManager::storeManagedBuildRejectsSelfUpdateWithoutNetworkOrCredentialRead()
{
    ManifestServer server(QStringLiteral("1.1.0"));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    LicenseManager licenseManager(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                                  1000,
                                  &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    CongregationUpdateManager updateManager(&licenseManager,
                                       QStringLiteral("1.0.0"),
                                       server.manifestUrl(),
                                       server.grantUrl(),
                                       false);

    QVERIFY(!updateManager.selfUpdateSupported());
    QVERIFY(!updateManager.checkForUpdates());
    QVERIFY(!updateManager.updateNow());
    QVERIFY(!updateManager.cancelUpdate());
    QCOMPARE(updateManager.state(), CongregationUpdateManager::State::Failed);
    QCOMPARE(store.readCount, 1);
    QCOMPARE(server.connectionCount(), 0);
    QTest::qWait(25);
    QCOMPARE(store.readCount, 1);
    QCOMPARE(server.connectionCount(), 0);
}

void tst_CongregationUpdateManager::macStoreMarkerOrReceiptDisablesSelfUpdate()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS bundle markers are only interpreted on macOS");
#else
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral("Congregation.app/Contents/MacOS")));
    const QString executableDirectory =
        root.filePath(QStringLiteral("Congregation.app/Contents/MacOS"));
    const QString contentsDirectory =
        root.filePath(QStringLiteral("Congregation.app/Contents"));
    const QString infoPlistPath =
        root.filePath(QStringLiteral("Congregation.app/Contents/Info.plist"));

    const auto writeMarker = [&infoPlistPath](const QString &channel) {
        QFile infoPlist(infoPlistPath);
        if (!infoPlist.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        const QByteArray contents = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<plist version=\"1.0\"><dict>"
            "<key>IISACCDistributionChannel</key><string>%1</string>"
            "</dict></plist>\n").arg(channel).toUtf8();
        return infoPlist.write(contents) == contents.size();
    };

    QVERIFY(writeMarker(QStringLiteral("direct")));
    QVERIFY(CongregationUpdateManager::selfUpdateSupportedForMacApplicationDirectory(
        executableDirectory));

    QVERIFY(writeMarker(QStringLiteral("appstore")));
    QVERIFY(!CongregationUpdateManager::selfUpdateSupportedForMacApplicationDirectory(
        executableDirectory));

    QVERIFY(writeMarker(QStringLiteral("direct")));
    QDir contents(contentsDirectory);
    QVERIFY(contents.mkpath(QStringLiteral("_MASReceipt")));
    QFile receipt(contents.filePath(QStringLiteral("_MASReceipt/receipt")));
    QVERIFY(receipt.open(QIODevice::WriteOnly));
    QVERIFY(receipt.write("receipt") > 0);
    receipt.close();
    QVERIFY(!CongregationUpdateManager::selfUpdateSupportedForMacApplicationDirectory(
        executableDirectory));
#endif
}

QTEST_GUILESS_MAIN(tst_CongregationUpdateManager)

#include "tst_congregationupdatemanager.moc"
