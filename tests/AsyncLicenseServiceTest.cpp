#include <Engine/AsyncLicenseService.h>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace congregation;
using namespace std::chrono_literals;

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void until(ii::runtime::EventLoop& loop, F condition) {
    const auto deadline = ii::runtime::Clock::now() + 5s;
    while (!condition() && ii::runtime::Clock::now() < deadline) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    require(condition(), "Timed out waiting for the asynchronous license state");
}
void pending_and_missing_session() {
    using namespace iisacc::accounts::native;
    ii::runtime::EventLoop ui;
    auto release = std::make_shared<std::promise<void>>();
    auto ready = release->get_future().share();
    auto entered = std::make_shared<std::atomic_bool>(false);
    const auto ui_thread = std::this_thread::get_id();
    auto factory_off_ui=std::make_shared<std::atomic_bool>(false);
    AsyncLicenseService service(ui, true, LicenseService::Authority::web,
        [ready, entered, ui_thread, factory_off_ui] {
            *factory_off_ui = std::this_thread::get_id() != ui_thread;
            *entered = true;
            ready.wait();
            return SocietySessionOptions{.product_id="congregation", .device={
                std::string(64,'a'),"tablet","Fixture","ios","1",
                "com.iisacc.congregation.painter","1"}};
        }, SessionPersistence{.read=[] { return SecretResult{SecretResult::Status::not_found,{}}; }});
    bool changed_on_ui = true;
    service.changed = [&](auto) { changed_on_ui &= std::this_thread::get_id()==ui_thread; };
    require(service.workspace_visible() && service.local_edits_allowed() && !service.licensed,
            "Startup must allow local work without claiming a license");
    service.restore();
    until(ui, [&] { return entered->load(); });
    bool heartbeat = false;
    require(bool(ui.post([&] { heartbeat=true; })), "Cannot post UI heartbeat");
    until(ui, [&] { return heartbeat; });
    require(service.workspace_visible() && service.local_edits_allowed() && !service.licensed,
            "Slow account preparation must not lock the workspace");
    release->set_value();
    until(ui, [&] { return !service.workspace_visible(); });
    require(factory_off_ui->load() && changed_on_ui, "License work or UI delivery ran on the wrong thread");
    require(!service.licensed && !service.local_edits_allowed() && !service.verifying,
            "Missing session must resolve to the license view without a grant");
}
void construction_failure_and_lifetime() {
    ii::runtime::EventLoop ui;
    {
        auto attempts=std::make_shared<std::atomic_int>(0);
        AsyncLicenseService failed(ui, true, LicenseService::Authority::society,
            [attempts]() -> iisacc::accounts::native::SocietySessionOptions {
                if (++*attempts==1) throw std::runtime_error("fixture failure");
                return {};
            });
        failed.restore();
        until(ui, [&] { return !failed.workspace_visible(); });
        require(!failed.licensed && failed.code=="verification_unavailable",
                "Initialization failure must resolve to a retryable denial");
        failed.retry();
        until(ui,[&] { return failed.code=="society_unavailable"; });
        require(!failed.workspace_visible() && !failed.licensed,
                "Retry must not reopen the workspace without a verified grant");
    }
    auto release=std::make_shared<std::promise<void>>();
    auto ready=release->get_future().share();
    auto entered=std::make_shared<std::atomic_bool>(false);
    {
        auto closing=std::make_unique<AsyncLicenseService>(ui, true, LicenseService::Authority::society,
            [ready,entered] { *entered=true; ready.wait(); return iisacc::accounts::native::SocietySessionOptions{}; });
        closing->restore();
        until(ui,[&] { return entered->load(); });
        // This must complete before releasing the blocked account operation.
        closing.reset();
    }
    release->set_value();
    ui.poll();
    using namespace iisacc::accounts::native;
    auto dying=std::make_unique<AsyncLicenseService>(ui,true,LicenseService::Authority::web,
        [] { return SocietySessionOptions{.product_id="congregation",.device={
            std::string(64,'a'),"tablet","Fixture","ios","1",
            "com.iisacc.congregation.painter","1"}}; },
        SessionPersistence{.read=[] { return SecretResult{SecretResult::Status::not_found,{}}; }});
    dying->device_ready=[&](std::string) { dying.reset(); };
    dying->restore();
    until(ui,[&] { return !dying; });
}
void full_ui_queue_eventually_receives_failure() {
    ii::runtime::EventLoop ui(1);
    require(bool(ui.post([] {})),"Cannot fill UI fixture queue");
    auto entered=std::make_shared<std::promise<void>>();
    auto ready=entered->get_future();
    AsyncLicenseService service(ui,true,LicenseService::Authority::society,
        [entered]() -> iisacc::accounts::native::SocietySessionOptions {
            entered->set_value();
            throw std::runtime_error("fixture failure");
        });
    service.restore();
    require(ready.wait_for(3s)==std::future_status::ready,"Account worker did not start");
    std::this_thread::sleep_for(50ms);
    until(ui,[&] { return !service.workspace_visible(); });
    require(service.code=="verification_unavailable", "The full UI queue lost the license result");
}
}
int main() {
    try { pending_and_missing_session(); construction_failure_and_lifetime(); full_ui_queue_eventually_receives_failure(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}
