#include "accountmanager.h"
#include "licensemanager.h"
#include "licensecredentialstore.h"

#include <iiLicenseManager/LicenseClient.h>

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

#include <type_traits>

namespace
{
const QString validLicenseKey = QStringLiteral("IIL1_0123456789abcdefghijklmnopqrstuv");
const QByteArray testAccountPublicKey = QByteArrayLiteral(
    "A6EHv_POEL4dcN0Y50vAmWfk1jCbpQ1fHdyGZBJVMbg");
// Ed25519 fixture signed with the public test seed containing bytes 0x00..0x1f.
// Regenerate the signed payload when changing the application-owned product ID.
const QByteArray testAccountCertificate = QByteArrayLiteral(
    "eyJhbGciOiJFZERTQSIsImtpZCI6InRlc3QtMjAyNiIsInR5cCI6IklJU0FDQy1MSUNFTlNFIn0."
    "eyJ2IjoxLCJpc3MiOiJodHRwczovL2lpc2FjYy5jb20iLCJsaWNlbnNlSWQiOiIxMTExMTExMS0x"
    "MTExLTQxMTEtODExMS0xMTExMTExMTExMTEiLCJhY3RpdmF0aW9uSWQiOiIyMjIyMjIyMi0yMjIy"
    "LTQyMjItODIyMi0yMjIyMjIyMjIyMjIiLCJwcm9kdWN0SWQiOiJjb25ncmVnYXRpb24iLCJpbnN0"
    "YWxsYXRpb25JZCI6InRlc3RpbnN0YWxsYXRpb25pZDAxMjM0NTY3ODkwMTIzIiwiaXNzdWVkQXQi"
    "OiIyMDI2LTA4LTE0VDAwOjAwOjAwLjAwMFoifQ.s6t-ConH-l9X3fNHU6ZTe3E3Puo2sOMfnpp6t"
    "XTKzSZ8AjrucMgZnfmLNh2tuFgxC4wGMlZC-usXLg-wD15cAA");

QByteArray storedCredentials(const QString &email = QStringLiteral("verified@example.com"),
                             const QString &licenseKey = validLicenseKey)
{
    QJsonObject object;
    object.insert(QStringLiteral("schema"), 1);
    object.insert(QStringLiteral("email"), email);
    object.insert(QStringLiteral("licenseKey"), licenseKey);
    object.insert(QStringLiteral("productId"), QStringLiteral("congregation"));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

class FakeCredentialStore final : public LicenseCredentialStore
{
public:
    bool supported = true;
    ReadStatus readStatus = ReadStatus::NotFound;
    QByteArray readData;
    bool writeSucceeds = true;
    bool completeWritesImmediately = true;
    bool removeSucceeds = true;
    int readCount = 0;
    int writeCount = 0;
    int removeCount = 0;
    QByteArray lastWrite;
    MutationCompletion pendingWrite;

    [[nodiscard]] bool persistenceSupported() const override
    {
        return supported;
    }

    void read(ReadCompletion completion) override
    {
        ++readCount;
        completion(readStatus, readData);
    }

    void write(const QByteArray &credentials, MutationCompletion completion) override
    {
        ++writeCount;
        lastWrite = credentials;
        if (completeWritesImmediately) {
            completion(writeSucceeds);
        } else {
            pendingWrite = std::move(completion);
        }
    }

    void remove(MutationCompletion completion) override
    {
        ++removeCount;
        readStatus = ReadStatus::NotFound;
        readData.clear();
        completion(removeSucceeds);
    }

    void completeWrite()
    {
        if (!pendingWrite) {
            return;
        }

        MutationCompletion completion = std::move(pendingWrite);
        completion(writeSucceeds);
    }
};

QByteArray httpResponse(int statusCode,
                        const QByteArray &body,
                        const QByteArray &contentType = QByteArrayLiteral("application/json"),
                        const QByteArray &extraHeaders = {})
{
    QByteArray wireBody = body;
    const auto value = QJsonDocument::fromJson(body);
    if (statusCode == 200 && value.isObject() && !value.object().contains("data") && !value.object().contains("errors")) {
        const auto field = value.object().contains("activated") ? "activateLicense" : "validateLicense";
        wireBody = QJsonDocument(QJsonObject{{"data", QJsonObject{{field, value.object()}}}}).toJson(QJsonDocument::Compact);
    }
    const QByteArray reason = statusCode == 200
        ? QByteArrayLiteral("OK")
        : statusCode == 302
            ? QByteArrayLiteral("Found")
            : QByteArrayLiteral("Service Unavailable");
    QByteArray response = QByteArrayLiteral("HTTP/1.1 ")
        + QByteArray::number(statusCode)
        + QByteArrayLiteral(" ")
        + reason
        + QByteArrayLiteral("\r\nContent-Type: ")
        + contentType
        + QByteArrayLiteral("\r\nCache-Control: no-store\r\nConnection: close\r\n")
        + extraHeaders
        + QByteArrayLiteral("Content-Length: ")
        + QByteArray::number(wireBody.size())
        + QByteArrayLiteral("\r\n\r\n")
        + wireBody;
    return response;
}

class SingleResponseServer : public QTcpServer
{
public:
    explicit SingleResponseServer(QByteArray response, QObject *parent = nullptr)
        : QTcpServer(parent)
        , m_response(std::move(response))
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *socket = nextPendingConnection();
            if (!socket) {
                return;
            }

            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                m_request.append(socket->readAll());
                respondWhenComplete(socket);
            });
            ++m_connectionCount;
            respondWhenComplete(socket);
        });
    }

    [[nodiscard]] QUrl endpoint() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/graphql")
                        .arg(serverPort()));
    }

    [[nodiscard]] QByteArray request() const
    {
        return m_request;
    }

    [[nodiscard]] int connectionCount() const
    {
        return m_connectionCount;
    }

private:
    void respondWhenComplete(QTcpSocket *socket)
    {
        const qsizetype headerEnd = m_request.indexOf(QByteArrayLiteral("\r\n\r\n"));
        if (headerEnd < 0) {
            return;
        }

        qsizetype contentLength = 0;
        const QList<QByteArray> headerLines = m_request.left(headerEnd).split('\n');
        for (QByteArray headerLine : headerLines) {
            headerLine = headerLine.trimmed();
            if (headerLine.toLower().startsWith(QByteArrayLiteral("content-length:"))) {
                contentLength = headerLine.mid(headerLine.indexOf(':') + 1).trimmed().toLongLong();
            }
        }

        if (m_request.size() < headerEnd + 4 + contentLength || m_responded) {
            return;
        }

        m_responded = true;
        socket->write(m_response);
        socket->disconnectFromHost();
    }

    QByteArray m_response;
    QByteArray m_request;
    bool m_responded = false;
    int m_connectionCount = 0;
};
}

class tst_LicenseManager : public QObject
{
    Q_OBJECT

private slots:
    void disabledEnforcementStartsUnlockedWithoutAutomaticCredentialOrNetworkAccess();
    void accountEmailComesFromIiLicenseManagerActivation();
    void productIdentityIsApplicationOwned();
    void successfulValidationPostsPrivateFixedContractAndUnlocks();
    void successfulValidationStoresOnlyNormalizedVerifiedCredentials();
    void manualSuccessWaitsForSecureStorageCompletion();
    void secureStorageFailureUnlocksWithVisibleNoticeState();
    void storedCredentialsRestoreAndValidateAutomatically();
    void authoritativeInvalidStoredLicenseIsDeleted();
    void transientFailureKeepsStoredLicenseForCredentialFreeRetry();
    void malformedStoredCredentialsAreDeletedWithoutNetworkAccess();
    void updateCredentialsAreMoveOnlyAndReadOnlyOnExplicitRequest();
    void updateCredentialReadRejectsNonCanonicalStoredJson_data();
    void updateCredentialReadRejectsNonCanonicalStoredJson();
    void updateCredentialReadReportsStorageOutcomes_data();
    void updateCredentialReadReportsStorageOutcomes();
    void forgetStoredLicenseWhileLockedDeletesCredentials();
    void forgetLicenseCannotDiscardLicensedCanvas();
    void invalidLicenseDecisionRemainsLocked();
    void rejectsNonAuthoritativeResponses_data();
    void rejectsNonAuthoritativeResponses();
    void invalidInputDoesNotReachTheNetwork();
    void acceptsSupportedKeyVersionsAndRejectsOutOfRangeVersions_data();
    void acceptsSupportedKeyVersionsAndRejectsOutOfRangeVersions();
    void connectionFailureAndTimeoutRemainFailClosed();
};

void tst_LicenseManager::disabledEnforcementStartsUnlockedWithoutAutomaticCredentialOrNetworkAccess()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral("{\"valid\":true,\"productId\":\"congregation\"}")));
    QVERIFY(server.listen(QHostAddress::LocalHost));

    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    LicenseManager manager(server.endpoint(),
                           1000,
                           &store,
                           LicenseManager::EnforcementMode::Disabled);

    QVERIFY(!manager.enforcementEnabled());
    QVERIFY(manager.licensed());
    QVERIFY(!manager.verifying());
    QCOMPARE(manager.resultCode(), QString{});

    QCoreApplication::processEvents();
    QCOMPARE(store.readCount, 0);
    QCOMPARE(server.connectionCount(), 0);

    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    manager.retryStoredLicense();
    manager.forgetLicense();
    QCoreApplication::processEvents();

    QVERIFY(manager.licensed());
    QCOMPARE(store.readCount, 0);
    QCOMPARE(store.removeCount, 0);
    QCOMPARE(server.connectionCount(), 0);
}

void tst_LicenseManager::accountEmailComesFromIiLicenseManagerActivation()
{
    QTemporaryDir licenseStorage;
    QVERIFY(licenseStorage.isValid());
    QFile installationIdFile(
        QDir(licenseStorage.path()).filePath(QStringLiteral("installation.id")));
    QVERIFY(installationIdFile.open(QIODevice::WriteOnly));
    QCOMPARE(installationIdFile.write("testinstallationid01234567890123\n"), 33);
    installationIdFile.close();

    const QByteArray responseBody = QJsonDocument(QJsonObject{
        {QStringLiteral("activated"), true},
        {QStringLiteral("certificate"), QString::fromLatin1(testAccountCertificate)},
        {QStringLiteral("productId"), QStringLiteral("congregation")},
    }).toJson(QJsonDocument::Compact);
    SingleResponseServer server(httpResponse(200, responseBody));
    QVERIFY(server.listen(QHostAddress::LocalHost));

    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    LicenseManager credentials(QUrl(QStringLiteral("http://127.0.0.1/validate")),
                               1000,
                               &store,
                               LicenseManager::EnforcementMode::Disabled);
    const QList<iisacc::licensing::TrustedLicenseKey> trustedKeys{{
        QStringLiteral("test-2026"),
        QByteArray::fromBase64(testAccountPublicKey,
                               QByteArray::Base64UrlEncoding
                                   | QByteArray::AbortOnBase64DecodingErrors),
    }};
    iisacc::licensing::LicenseClient accountLicense(QStringLiteral("congregation"),
                                                    server.endpoint(),
                                                    licenseStorage.path(),
                                                    trustedKeys);
    AccountManager account(&credentials, &accountLicense);

    QCOMPARE(store.readCount, 0);
    QVERIFY(account.accountEmail().isEmpty());
    QVERIFY(!account.accountEmailLoading());

    QSignalSpy emailSpy(&account, &AccountManager::accountEmailChanged);
    QSignalSpy loadingSpy(&account, &AccountManager::accountEmailLoadingChanged);
    account.refresh();

    QCOMPARE(store.readCount, 1);
    QTRY_COMPARE_WITH_TIMEOUT(account.accountEmail(), QStringLiteral("verified@example.com"), 2000);
    QVERIFY(!account.accountEmailLoading());
    QCOMPARE(emailSpy.size(), 1);
    QCOMPARE(loadingSpy.size(), 2);
    QCOMPARE(accountLicense.accountEmail(), QStringLiteral("verified@example.com"));
    QCOMPARE(server.connectionCount(), 1);

    SingleResponseServer offlineTrap(httpResponse(503, QByteArrayLiteral("{}")));
    QVERIFY(offlineTrap.listen(QHostAddress::LocalHost));
    iisacc::licensing::LicenseClient restartedLicense(QStringLiteral("congregation"),
                                                      offlineTrap.endpoint(),
                                                      licenseStorage.path(),
                                                      trustedKeys);
    AccountManager restartedAccount(&credentials, &restartedLicense);
    restartedAccount.refresh();

    QCOMPARE(restartedAccount.accountEmail(), QStringLiteral("verified@example.com"));
    QVERIFY(!restartedAccount.accountEmailLoading());
    QCOMPARE(store.readCount, 1);
    QCOMPARE(offlineTrap.connectionCount(), 0);
}

void tst_LicenseManager::updateCredentialsAreMoveOnlyAndReadOnlyOnExplicitRequest()
{
    static_assert(!std::is_copy_constructible_v<StoredLicenseCredentials>);
    static_assert(!std::is_copy_assignable_v<StoredLicenseCredentials>);
    static_assert(std::is_move_constructible_v<StoredLicenseCredentials>);
    static_assert(std::is_move_assignable_v<StoredLicenseCredentials>);

    FakeCredentialStore store;
    LicenseManager manager(QUrl(QStringLiteral("http://127.0.0.1/validate")), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();
    bool completed = false;
    manager.requestStoredCredentials(
        [&completed](LicenseManager::StoredCredentialStatus status,
                     StoredLicenseCredentials credentials) {
            QCOMPARE(status, LicenseManager::StoredCredentialStatus::Available);
            QCOMPARE(credentials.email, QStringLiteral("verified@example.com"));
            QCOMPARE(credentials.licenseKey, validLicenseKey);
            completed = true;
        });

    QVERIFY(completed);
    QCOMPARE(store.readCount, 2);
}

void tst_LicenseManager::updateCredentialReadRejectsNonCanonicalStoredJson_data()
{
    QTest::addColumn<QByteArray>("payload");

    QJsonObject extraKeyObject = QJsonDocument::fromJson(storedCredentials()).object();
    extraKeyObject.insert(QStringLiteral("future"), true);
    QTest::newRow("extra-key")
        << QJsonDocument(extraKeyObject).toJson(QJsonDocument::Compact);
    QTest::newRow("surrounding-whitespace")
        << QByteArray(" \n") + storedCredentials() + QByteArray("\n");
    QTest::newRow("duplicate-key")
        << QByteArrayLiteral(
               R"({"email":"verified@example.com","email":"attacker@example.com","licenseKey":"IIL1_0123456789abcdefghijklmnopqrstuv","productId":"congregation","schema":1})");
    QTest::newRow("non-normalized-email")
        << storedCredentials(QStringLiteral("Verified@Example.com"));
}

void tst_LicenseManager::updateCredentialReadRejectsNonCanonicalStoredJson()
{
    QFETCH(QByteArray, payload);

    FakeCredentialStore store;
    LicenseManager manager(QUrl(QStringLiteral("http://127.0.0.1/validate")), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = payload;

    bool completed = false;
    manager.requestStoredCredentials(
        [&completed](LicenseManager::StoredCredentialStatus status,
                     StoredLicenseCredentials credentials) {
            QCOMPARE(status, LicenseManager::StoredCredentialStatus::Invalid);
            QVERIFY(credentials.email.isEmpty());
            QVERIFY(credentials.licenseKey.isEmpty());
            completed = true;
        });

    QVERIFY(completed);
    QCOMPARE(store.readCount, 2);
}

void tst_LicenseManager::updateCredentialReadReportsStorageOutcomes_data()
{
    QTest::addColumn<LicenseCredentialStore::ReadStatus>("readStatus");
    QTest::addColumn<LicenseManager::StoredCredentialStatus>("expectedStatus");

    QTest::newRow("not-found")
        << LicenseCredentialStore::ReadStatus::NotFound
        << LicenseManager::StoredCredentialStatus::NotFound;
    QTest::newRow("error")
        << LicenseCredentialStore::ReadStatus::Error
        << LicenseManager::StoredCredentialStatus::Unavailable;
}

void tst_LicenseManager::updateCredentialReadReportsStorageOutcomes()
{
    QFETCH(LicenseCredentialStore::ReadStatus, readStatus);
    QFETCH(LicenseManager::StoredCredentialStatus, expectedStatus);

    FakeCredentialStore store;
    LicenseManager manager(QUrl(QStringLiteral("http://127.0.0.1/validate")), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    store.readStatus = readStatus;

    bool completed = false;
    manager.requestStoredCredentials(
        [&completed, expectedStatus](LicenseManager::StoredCredentialStatus status,
                                     StoredLicenseCredentials credentials) {
            QCOMPARE(status, expectedStatus);
            QVERIFY(credentials.email.isEmpty());
            QVERIFY(credentials.licenseKey.isEmpty());
            completed = true;
        });

    QVERIFY(completed);
    QCOMPARE(store.readCount, 2);
}

void tst_LicenseManager::productIdentityIsApplicationOwned()
{
    LicenseManager manager(QUrl(QStringLiteral("http://127.0.0.1/validate")), 1000);
    QCOMPARE(manager.productId(), QStringLiteral("congregation"));
    QVERIFY(manager.enforcementEnabled());
    QVERIFY(!manager.licensed());
    QVERIFY(!manager.verifying());
}

void tst_LicenseManager::successfulValidationStoresOnlyNormalizedVerifiedCredentials()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("  Verified@Example.COM "), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);

    QCOMPARE(store.writeCount, 1);
    QCOMPARE(store.removeCount, 0);
    QCOMPARE(store.lastWrite, storedCredentials());
    QVERIFY(manager.hasStoredLicense());

    const QJsonObject storedObject = QJsonDocument::fromJson(store.lastWrite).object();
    QCOMPARE(storedObject.value(QStringLiteral("email")).toString(),
             QStringLiteral("verified@example.com"));
    QCOMPARE(storedObject.value(QStringLiteral("licenseKey")).toString(), validLicenseKey);
    QCOMPARE(storedObject.value(QStringLiteral("productId")).toString(),
             QStringLiteral("congregation"));
}

void tst_LicenseManager::manualSuccessWaitsForSecureStorageCompletion()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.completeWritesImmediately = false;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(store.writeCount, 1, 2000);

    QVERIFY(manager.verifying());
    QVERIFY(!manager.licensed());
    QVERIFY(!manager.hasStoredLicense());
    QCOMPARE(finishedSpy.size(), 0);

    store.completeWrite();
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 1000);
    QVERIFY(!manager.verifying());
    QVERIFY(manager.licensed());
    QVERIFY(manager.hasStoredLicense());
    QVERIFY(manager.resultCode().isEmpty());
}

void tst_LicenseManager::secureStorageFailureUnlocksWithVisibleNoticeState()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.completeWritesImmediately = false;
    store.writeSucceeds = false;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);

    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(store.writeCount, 1, 2000);
    QVERIFY(!manager.licensed());

    store.completeWrite();
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 1000);
    QVERIFY(manager.licensed());
    QVERIFY(!manager.hasStoredLicense());
    QCOMPARE(manager.resultCode(), QStringLiteral("secure_storage_unavailable"));
    QCOMPARE(finishedSpy.first().first().toBool(), true);
}

void tst_LicenseManager::storedCredentialsRestoreAndValidateAutomatically()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    LicenseManager manager(server.endpoint(), 1000, &store);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);

    QVERIFY(manager.licensed());
    QVERIFY(manager.hasStoredLicense());
    QCOMPARE(store.readCount, 1);
    QCOMPARE(store.writeCount, 0);
    QCOMPARE(store.removeCount, 0);
    QVERIFY(server.request().contains(validLicenseKey.toUtf8()));
}

void tst_LicenseManager::authoritativeInvalidStoredLicenseIsDeleted()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"code":"INVALID_LICENSE","valid":false,"productId":null})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    LicenseManager manager(server.endpoint(), 1000, &store);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);

    QVERIFY(!manager.licensed());
    QVERIFY(!manager.hasStoredLicense());
    QCOMPARE(manager.resultCode(), QStringLiteral("invalid_license"));
    QCOMPARE(store.removeCount, 1);
}

void tst_LicenseManager::transientFailureKeepsStoredLicenseForCredentialFreeRetry()
{
    SingleResponseServer server(httpResponse(
        503,
        QByteArrayLiteral(R"({"valid":false})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();

    LicenseManager manager(server.endpoint(), 50, &store);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QVERIFY(!manager.licensed());
    QVERIFY(manager.hasStoredLicense());
    QCOMPARE(store.removeCount, 0);
    QCOMPARE(manager.resultCode(), QStringLiteral("verification_unavailable"));

    manager.retryStoredLicense();
    QTRY_COMPARE_WITH_TIMEOUT(server.connectionCount(), 2, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 2, 2000);
    QVERIFY(manager.hasStoredLicense());
    QCOMPARE(store.removeCount, 0);
}

void tst_LicenseManager::malformedStoredCredentialsAreDeletedWithoutNetworkAccess()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = QByteArrayLiteral("{not-json");

    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.removeCount, 1, 1000);

    QVERIFY(!manager.licensed());
    QVERIFY(!manager.hasStoredLicense());
    QCOMPARE(manager.resultCode(), QStringLiteral("stored_license_removed"));
    QCOMPARE(server.connectionCount(), 0);
}

void tst_LicenseManager::forgetStoredLicenseWhileLockedDeletesCredentials()
{
    SingleResponseServer server(httpResponse(
        503,
        QByteArrayLiteral(R"({"valid":false})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    store.readStatus = LicenseCredentialStore::ReadStatus::Found;
    store.readData = storedCredentials();
    LicenseManager manager(server.endpoint(), 1000, &store);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QVERIFY(!manager.licensed());
    QVERIFY(manager.hasStoredLicense());

    manager.forgetLicense();
    QVERIFY(!manager.licensed());
    QVERIFY(!manager.hasStoredLicense());
    QCOMPARE(store.removeCount, 1);
}

void tst_LicenseManager::forgetLicenseCannotDiscardLicensedCanvas()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    FakeCredentialStore store;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);

    manager.forgetLicense();
    QVERIFY(manager.licensed());
    QVERIFY(manager.hasStoredLicense());
    QCOMPARE(store.removeCount, 0);
}

void tst_LicenseManager::successfulValidationPostsPrivateFixedContractAndUnlocks()
{
    const QByteArray body = QByteArrayLiteral(
        R"({"checkedAt":"2026-08-13T00:00:00.000Z","expiresAt":null,"productId":"congregation","valid":true})");
    SingleResponseServer server(httpResponse(200, body));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    FakeCredentialStore store;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("  Verified@Example.COM "), validLicenseKey);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QVERIFY(manager.licensed());
    QVERIFY(!manager.verifying());
    QVERIFY(manager.resultCode().isEmpty());
    QCOMPARE(finishedSpy.first().first().toBool(), true);

    const QByteArray request = server.request();
    const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(headerEnd > 0);
    const QByteArray headers = request.left(headerEnd).toLower();
    const QByteArray requestBody = request.mid(headerEnd + 4);
    QVERIFY(headers.startsWith(QByteArrayLiteral("post /graphql http/1.1")));
    QVERIFY(!headers.contains(validLicenseKey.toUtf8()));
    QVERIFY(headers.contains(QByteArrayLiteral("content-type: application/json")));
    QVERIFY(headers.contains(QByteArrayLiteral("accept: application/json")));
    QVERIFY(headers.contains(QByteArrayLiteral("cache-control: no-store")));

    const QJsonDocument requestDocument = QJsonDocument::fromJson(requestBody);
    QVERIFY(requestDocument.isObject());
    QVERIFY(requestDocument.object().value("query").toString().contains("validateLicense(input: $input)"));
    const QJsonObject requestObject = requestDocument.object().value("variables").toObject().value("input").toObject();
    QCOMPARE(requestObject.value(QStringLiteral("email")).toString(),
             QStringLiteral("verified@example.com"));
    QCOMPARE(requestObject.value(QStringLiteral("licenseKey")).toString(), validLicenseKey);
    QCOMPARE(requestObject.value(QStringLiteral("productId")).toString(),
             QStringLiteral("congregation"));
}

void tst_LicenseManager::invalidLicenseDecisionRemainsLocked()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"code":"INVALID_LICENSE","valid":false,"productId":null})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    FakeCredentialStore store;
    LicenseManager manager(server.endpoint(), 1000, &store);
    QTRY_COMPARE_WITH_TIMEOUT(store.readCount, 1, 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QVERIFY(!manager.licensed());
    QCOMPARE(manager.resultCode(), QStringLiteral("invalid_license"));
    QCOMPARE(finishedSpy.first().first().toBool(), false);
    QCOMPARE(store.writeCount, 0);
    QCOMPARE(store.removeCount, 0);
}

void tst_LicenseManager::rejectsNonAuthoritativeResponses_data()
{
    QTest::addColumn<QByteArray>("response");

    QTest::newRow("graphql-partial-data")
        << httpResponse(200, QByteArrayLiteral(R"({"data":{"validateLicense":{"valid":true,"productId":"congregation"}},"errors":[{"message":"Failed","extensions":{"status":503}}]})"));
    QTest::newRow("graphql-null-result")
        << httpResponse(200, QByteArrayLiteral(R"({"data":{"validateLicense":null}})"));
    QTest::newRow("server-error")
        << httpResponse(503, QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})"));
    QTest::newRow("redirect")
        << httpResponse(302,
                        QByteArray{},
                        QByteArrayLiteral("application/json"),
                        QByteArrayLiteral("Location: https://iisacc.com/elsewhere\r\n"));
    QTest::newRow("malformed-json")
        << httpResponse(200, QByteArrayLiteral("{not-json"));
    QTest::newRow("string-not-boolean")
        << httpResponse(200,
                        QByteArrayLiteral(R"({"valid":"true","productId":"congregation"})"));
    QTest::newRow("missing-product")
        << httpResponse(200, QByteArrayLiteral(R"({"valid":true})"));
    QTest::newRow("wrong-product")
        << httpResponse(200,
                        QByteArrayLiteral(R"({"valid":true,"productId":"another-product"})"));
    QTest::newRow("wrong-content-type")
        << httpResponse(200,
                        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})"),
                        QByteArrayLiteral("text/plain"));
}

void tst_LicenseManager::rejectsNonAuthoritativeResponses()
{
    QFETCH(QByteArray, response);
    SingleResponseServer server(response);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    LicenseManager manager(server.endpoint(), 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QVERIFY(!manager.licensed());
    QVERIFY(!manager.verifying());
    QCOMPARE(manager.resultCode(), QStringLiteral("verification_unavailable"));
    QCOMPARE(finishedSpy.first().first().toBool(), false);
}

void tst_LicenseManager::invalidInputDoesNotReachTheNetwork()
{
    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    LicenseManager manager(server.endpoint(), 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("person@example"), QStringLiteral("short-key"));

    QCOMPARE(finishedSpy.size(), 1);
    QVERIFY(!manager.licensed());
    QCOMPARE(manager.resultCode(), QStringLiteral("invalid_input"));
    QVERIFY(server.request().isEmpty());
}

void tst_LicenseManager::acceptsSupportedKeyVersionsAndRejectsOutOfRangeVersions_data()
{
    QTest::addColumn<QString>("licenseKey");
    QTest::addColumn<bool>("requestExpected");

    QTest::newRow("version-one")
        << QStringLiteral("IIL1_0123456789abcdefghijklmnopqrstuv") << true;
    QTest::newRow("highest-supported-version")
        << QStringLiteral("IIL32767_0123456789abcdefghijklmnopqrstuv") << true;
    QTest::newRow("out-of-range-version")
        << QStringLiteral("IIL32768_0123456789abcdefghijklmnopqrstuv") << false;
    QTest::newRow("leading-zero-version")
        << QStringLiteral("IIL01_0123456789abcdefghijklmnopqrstuv") << false;
}

void tst_LicenseManager::acceptsSupportedKeyVersionsAndRejectsOutOfRangeVersions()
{
    QFETCH(QString, licenseKey);
    QFETCH(bool, requestExpected);

    SingleResponseServer server(httpResponse(
        200,
        QByteArrayLiteral(R"({"valid":true,"productId":"congregation"})")));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    LicenseManager manager(server.endpoint(), 1000);
    QSignalSpy finishedSpy(&manager, &LicenseManager::validationFinished);
    manager.validateLicense(QStringLiteral("verified@example.com"), licenseKey);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 2000);
    QCOMPARE(!server.request().isEmpty(), requestExpected);
    QCOMPARE(manager.licensed(), requestExpected);
    QCOMPARE(manager.resultCode(),
             requestExpected ? QString{} : QStringLiteral("invalid_input"));
}

void tst_LicenseManager::connectionFailureAndTimeoutRemainFailClosed()
{
    QTcpServer portProbe;
    QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
    const quint16 unusedPort = portProbe.serverPort();
    portProbe.close();

    LicenseManager connectionFailureManager(
        QUrl(QStringLiteral("http://127.0.0.1:%1/validate").arg(unusedPort)),
        500);
    QSignalSpy connectionFinishedSpy(&connectionFailureManager,
                                     &LicenseManager::validationFinished);
    connectionFailureManager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(connectionFinishedSpy.size(), 1, 2000);
    QVERIFY(!connectionFailureManager.licensed());
    QCOMPARE(connectionFailureManager.resultCode(),
             QStringLiteral("verification_unavailable"));

    QTcpServer stalledServer;
    QVERIFY(stalledServer.listen(QHostAddress::LocalHost, 0));
    LicenseManager timeoutManager(
        QUrl(QStringLiteral("http://127.0.0.1:%1/validate").arg(stalledServer.serverPort())),
        50);
    QSignalSpy timeoutFinishedSpy(&timeoutManager, &LicenseManager::validationFinished);
    timeoutManager.validateLicense(QStringLiteral("verified@example.com"), validLicenseKey);
    QTRY_COMPARE_WITH_TIMEOUT(timeoutFinishedSpy.size(), 1, 2000);
    QVERIFY(!timeoutManager.licensed());
    QCOMPARE(timeoutManager.resultCode(), QStringLiteral("verification_unavailable"));
}

QTEST_GUILESS_MAIN(tst_LicenseManager)

#include "tst_licensemanager.moc"
