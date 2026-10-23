#include "qtenginehost.h"
#include <Engine/ApplicationEngine.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <QSignalSpy>
#include <QtTest>
#include <stdexcept>
#include <thread>

class tst_QtEngineHost final : public QObject
{
    Q_OBJECT
private slots:
    void dispatchesStartupOnHostOwner()
    {
        congregation::ApplicationEngine engine;
        QtEngineHost host(engine.events());
        int calls = 0;
        std::thread::id dispatched;
        const auto owner = std::this_thread::get_id();
        QVERIFY(engine.start([&] { dispatched = std::this_thread::get_id(); ++calls; }));
        QCOMPARE(calls, 0);
        QTRY_COMPARE(calls, 1);
        QVERIFY(dispatched == owner);
    }

    void wakesIdleHostFromCoreWorkerAndHonorsDeadline()
    {
        congregation::ApplicationEngine engine;
        QtEngineHost host(engine.events());
        ii::runtime::Runtime workers({2, 16});
        const auto owner = std::this_thread::get_id();
        const auto start = ii::runtime::Clock::now();
        bool delivered = false;
        bool correctThread = false;
        std::chrono::steady_clock::duration elapsed{};
        QVERIFY(workers.post([sender = engine.events().sender(), &delivered, &correctThread,
                              &elapsed, owner, start] {
            const auto delivery = sender.after(std::chrono::milliseconds(30),
                [&delivered, &correctThread, &elapsed, owner, start] {
                    correctThread = std::this_thread::get_id() == owner;
                    elapsed = ii::runtime::Clock::now() - start;
                    delivered = true;
                });
            if (!delivery) throw std::runtime_error("host delivery rejected");
        }));
        QTRY_VERIFY(delivered);
        QVERIFY(correctThread);
        QVERIFY(elapsed >= std::chrono::milliseconds(30));
        QVERIFY(workers.join());
    }

    void cancellationRetiresHostDeadline()
    {
        congregation::ApplicationEngine engine;
        QtEngineHost host(engine.events());
        bool called = false;
        const auto pending = engine.events().after(std::chrono::milliseconds(20), [&] { called = true; });
        QVERIFY(pending);
        QVERIFY(pending.token.cancel());
        QTest::qWait(50);
        QVERIFY(!called);
        QVERIFY(!engine.events().next_deadline());
    }

    void detachingHostDiscardsQueuedWakeAndAllowsReattachment()
    {
        ii::runtime::EventLoop events;
        int calls = 0;
        {
            QtEngineHost first(events);
            QVERIFY(events.post([&] { ++calls; }));
        }
        QCoreApplication::processEvents();
        QCOMPARE(calls, 0);
        QtEngineHost replacement(events);
        QTRY_COMPARE(calls, 1);
    }

    void dispatchFailureClosesAdmission()
    {
        ii::runtime::EventLoop events;
        QtEngineHost host(events);
        QSignalSpy failures(&host, &QtEngineHost::dispatchFailed);
        QVERIFY(events.post([] { throw std::runtime_error("test dispatch failure"); }));
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(failures.first().first().toString(), QStringLiteral("test dispatch failure"));
        QVERIFY(!events.post([] {}));
    }
};

QTEST_GUILESS_MAIN(tst_QtEngineHost)
#include "tst_qtenginehost.moc"
