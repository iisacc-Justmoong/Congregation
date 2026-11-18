#include "AsyncLicenseService.h"
#include <atomic>
#include <exception>
#include <thread>
#include <utility>

namespace congregation {
struct AsyncLicenseService::Receiver final {
    AsyncLicenseService* owner;
    ii::runtime::EventLoop::Sender ui;
};

struct AsyncLicenseService::Worker final : std::enable_shared_from_this<Worker> {
    struct Snapshot final {
        std::uint64_t revision;
        bool licensed, verifying, signed_in, visible;
        std::string code, email, device;
    };
    ii::runtime::EventLoop loop;
    std::weak_ptr<Receiver> receiver;
    OptionsFactory factory;
    iisacc::accounts::native::SessionPersistence store;
    LicenseService::Authority authority;
    bool enforced;
    std::atomic_bool active{true};
    std::unique_ptr<LicenseService> service;
    std::string device;
    std::uint64_t revision=0;

    Worker(std::weak_ptr<Receiver> target, OptionsFactory make,
           iisacc::accounts::native::SessionPersistence persistence,
           LicenseService::Authority auth, bool enforcement)
        : receiver(std::move(target)), factory(std::move(make)), store(std::move(persistence)),
          authority(auth), enforced(enforcement) {}

    void dispatch(std::shared_ptr<std::function<void(AsyncLicenseService&)>> function) {
        auto target=receiver.lock();
        if (!target || !active) return;
        auto weak=receiver;
        auto queued=target->ui.post([weak, function] {
            if (auto target=weak.lock(); target && target->owner) (*function)(*target->owner);
        });
        if (!queued) {
            auto self=weak_from_this();
            (void)loop.after(std::chrono::milliseconds(10),[self,function=std::move(function)] {
                if (auto worker=self.lock(); worker && worker->active) worker->dispatch(function);
            });
        }
    }
    template<class F> void deliver(F function) {
        dispatch(std::make_shared<std::function<void(AsyncLicenseService&)>>(std::move(function)));
    }
    void publish() {
        if (!service) return;
        Snapshot value{++revision,service->licensed,service->verifying,service->signed_in,
                       service->workspace_visible(),service->code,service->email,device};
        auto weak=receiver;
        deliver([weak,value=std::move(value)](AsyncLicenseService& owner) mutable {
            const auto alive=[&] { auto target=weak.lock(); return target && target->owner==&owner; };
            if (value.revision<=owner.delivered_revision_) return;
            owner.delivered_revision_=value.revision;
            if (owner.launch_granted_ && !value.licensed) return;
            if (owner.enforced && value.licensed) owner.launch_granted_=true;
            const bool changed_license=owner.licensed!=value.licensed;
            owner.licensed=value.licensed;
            owner.verifying=value.verifying;
            owner.signed_in=value.signed_in;
            if (!value.visible || value.licensed) owner.initial_decided_=true;
            owner.pending_=owner.enforced && !owner.initial_decided_ && !value.licensed && value.visible;
            owner.code=std::move(value.code);
            owner.email=std::move(value.email);
            auto on_device=owner.device_ready;
            if (!value.device.empty() && on_device) on_device(std::move(value.device));
            if (!alive()) return;
            auto on_changed=owner.changed;
            if (on_changed) on_changed(LicenseService::Notice::code);
            if (!alive()) return;
            auto on_validated=owner.validated;
            if (changed_license && on_validated) on_validated(owner.licensed);
        });
    }
    void fail(std::string reason = "verification_unavailable") {
        const auto failed_revision=++revision;
        deliver([failed_revision,reason=std::move(reason)](auto& owner) mutable {
            if (failed_revision<=owner.delivered_revision_) return;
            owner.delivered_revision_=failed_revision;
            owner.fail_to_start(std::move(reason));
        });
    }
    void prepare(bool restore_initial = true) {
        try {
            auto options=factory();
            if (!active) return;
            device=options.device.id;
            service=std::make_unique<LicenseService>(loop,std::move(options),enforced,authority,store);
            if (!service->workspace_visible() && !service->licensed) {
                auto reason=service->code;
                service.reset();
                fail(std::move(reason));
                return;
            }
            auto weak=weak_from_this();
            service->changed=[weak](auto) { if (auto self=weak.lock()) self->publish(); };
            service->open_society_action=[weak] {
                if (auto self=weak.lock()) self->deliver([](auto& owner) {
                    auto action=owner.open_society_action;
                    if (action) action();
                });
            };
            service->open_browser_action=[weak](std::string url) {
                if (auto self=weak.lock()) self->deliver([url=std::move(url)](auto& owner) mutable {
                    auto action=owner.open_browser_action;
                    if (action) action(std::move(url));
                });
            };
            publish();
            if (restore_initial && service->workspace_visible()) service->restore();
        } catch (const std::exception&) {
            service.reset();
            fail();
        } catch (...) {
            service.reset();
            fail();
        }
    }
    void run() {
        prepare();
        while (active) {
            try { loop.run(); break; }
            catch (...) {
                service.reset();
                try { fail(); } catch (...) { break; }
            }
        }
        service.reset();
    }
};

AsyncLicenseService::AsyncLicenseService(ii::runtime::EventLoop& ui, bool enforcement,
    LicenseService::Authority authority, OptionsFactory factory,
    iisacc::accounts::native::SessionPersistence store)
    : enforced(enforcement), licensed(!enforcement),
      receiver_(std::make_shared<Receiver>(Receiver{this,ui.sender()})),
      worker_(std::make_shared<Worker>(receiver_,std::move(factory),std::move(store),authority,enforcement)) {}
AsyncLicenseService::~AsyncLicenseService() {
    receiver_->owner=nullptr;
    worker_->active=false;
    worker_->loop.request_stop();
}
bool AsyncLicenseService::workspace_visible() const noexcept {
    return license_workspace_visible(enforced,licensed,pending_);
}
bool AsyncLicenseService::local_edits_allowed() const noexcept {
    return license_local_edits_allowed(enforced,licensed,pending_);
}
void AsyncLicenseService::fail_to_start(std::string reason) {
    if (launch_granted_) { verifying=false; return; }
    initial_decided_=true; pending_=false; verifying=false; licensed=!enforced;
    code=reason.empty() ? "verification_unavailable" : std::move(reason);
    auto on_changed=changed;
    if (on_changed) on_changed(LicenseService::Notice::code);
}
void AsyncLicenseService::send(Command command) {
    if (!started_) return;
    std::weak_ptr<Worker> weak=worker_;
    const auto queued=worker_->loop.post([weak,command] {
        auto worker=weak.lock();
        if (!worker || !worker->active) return;
        if (!worker->service) {
            if (command==Command::restore || command==Command::retry) { worker->prepare(); return; }
            if (command==Command::sign_in || command==Command::open_society) worker->prepare(false);
            if (!worker->service) return;
        }
        switch(command) {
        case Command::restore: worker->service->restore(); break;
        case Command::retry: worker->service->retry(); break;
        case Command::sign_in: worker->service->sign_in(); break;
        case Command::open_society: worker->service->open_society(); break;
        case Command::cancel: worker->service->cancel(); break;
        case Command::forget: worker->service->forget(); break;
        }
    });
    if (!queued) { code="verification_unavailable"; auto on_changed=changed; if (on_changed) on_changed(LicenseService::Notice::code); }
}
void AsyncLicenseService::restore() {
    if (started_) { send(Command::restore); return; }
    started_=true;
    try { std::thread([worker=worker_] { worker->run(); }).detach(); }
    catch (const std::exception&) { started_=false; fail_to_start(); }
}
void AsyncLicenseService::retry() { if (!started_) restore(); else send(Command::retry); }
void AsyncLicenseService::sign_in() {
    if (!started_) {
        auto weak=std::weak_ptr(receiver_);
        restore();
        auto live=weak.lock();
        if (!live || live->owner!=this) return;
    }
    send(Command::sign_in);
}
void AsyncLicenseService::cancel() { send(Command::cancel); }
void AsyncLicenseService::forget() { send(Command::forget); }
void AsyncLicenseService::open_society() {
    if (worker_->authority==LicenseService::Authority::web) {
        if (!started_) {
            auto weak=std::weak_ptr(receiver_);
            restore();
            auto live=weak.lock();
            if (!live || live->owner!=this) return;
        }
        send(Command::open_society);
    }
    else { auto action=open_society_action; if (action) action(); }
}
}
