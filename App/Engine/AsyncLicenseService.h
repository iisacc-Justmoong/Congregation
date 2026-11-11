#pragma once

#include "LicenseService.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace congregation {

// UI-thread facade. Account preparation, session callbacks, signature checks,
// and network completion run on an owned background event loop. The facade
// receives value snapshots on the caller's event loop.
class AsyncLicenseService final {
public:
    using OptionsFactory = std::function<iisacc::accounts::native::SocietySessionOptions()>;
    AsyncLicenseService(ii::runtime::EventLoop&, bool enforced, LicenseService::Authority,
                        OptionsFactory, iisacc::accounts::native::SessionPersistence = {});
    ~AsyncLicenseService();
    AsyncLicenseService(const AsyncLicenseService&) = delete;
    AsyncLicenseService& operator=(const AsyncLicenseService&) = delete;

    std::function<void(LicenseService::Notice)> changed;
    std::function<void(bool)> validated;
    std::function<void()> open_society_action;
    std::function<void(std::string)> open_browser_action;
    std::function<void(std::string)> device_ready;
    bool enforced, licensed, verifying=false, signed_in=false;
    std::string code, email;

    [[nodiscard]] bool workspace_visible() const noexcept;
    [[nodiscard]] bool local_edits_allowed() const noexcept;
    void restore();
    void retry();
    void sign_in();
    void open_society();
    void cancel();
    void forget();

private:
    struct Receiver;
    struct Worker;
    enum class Command { restore, retry, sign_in, open_society, cancel, forget };
    void send(Command);
    void fail_to_start(std::string reason = "verification_unavailable");
    std::shared_ptr<Receiver> receiver_;
    std::shared_ptr<Worker> worker_;
    bool pending_=true;
    bool initial_decided_=false;
    bool launch_granted_=false;
    bool started_=false;
    std::uint64_t delivered_revision_=0;
};
}
