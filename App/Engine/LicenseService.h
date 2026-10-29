#pragma once
#include "Data.h"
#include <iiAccountManager/BrowserSession.h>
namespace congregation
{
// Society authorizes app execution. iiAccountManager owns the license subsystem
// and verifies Society's signed local token; apps never query license ownership.
class LicenseService final
{
  public:
    enum class Notice
    {
        licensed,
        verifying,
        stored,
        code
    };
    LicenseService(ii::runtime::EventLoop &, iisacc::accounts::native::BrowserSessionOptions,
                   bool enforced, iisacc::accounts::native::SessionPersistence = {},
                   std::string society_endpoint = "http://127.0.0.1:49627/v1/authorize",
                   std::vector<iisacc::licensing::owned::TrustedKey> = iisacc::licensing::owned::trusted_keys());
    ~LicenseService();
    std::function<void(Notice)> changed;
    std::function<void(bool)> validated;
    std::function<void(std::string)> open_browser;
    bool enforced = true, licensed = false, verifying = false, stored = false, signed_in = false;
    std::string code, email;
    void sign_in();
    void restore();
    void retry();
    void open_society();
    void cancel();
    void forget();
    [[nodiscard]] std::string access_token() const;

  private:
    void session_changed(iisacc::accounts::native::BrowserSessionSnapshot);
    void fetch(bool force_refresh = false);
    void finish(ii::network::HttpResponse);
    void publish();
    void schedule_expiry(std::int64_t);
    ii::runtime::EventLoop::Sender events_;
    ii::network::Http http_;
    iisacc::accounts::native::BrowserSessionOptions options_;
    iisacc::accounts::native::BrowserSession account_;
    ii::runtime::TaskToken refresh_;
    std::string subject_, society_endpoint_, request_id_;
    std::vector<iisacc::licensing::owned::TrustedKey> trusted_keys_;
    std::uint64_t generation_ = 0;
    bool last_validated_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};
} // namespace congregation
