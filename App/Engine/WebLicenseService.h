#pragma once

#include <iiAccountManager/BrowserSession.h>

namespace congregation {

// iPad-only product authorization through an account-scoped web session.
class WebLicenseService final {
public:
    WebLicenseService(ii::runtime::EventLoop&, iisacc::accounts::native::BrowserSessionOptions,
                      iisacc::accounts::native::SessionPersistence = {});
    ~WebLicenseService();

    std::function<void()> changed;
    std::function<void(const std::string&)> open_browser;
    bool licensed=false, signed_in=false, verifying=false;
    std::string code, email;

    void sign_in();
    void restore();
    void retry();
    void cancel();
    void forget();

private:
    void session_changed(iisacc::accounts::native::BrowserSessionSnapshot);
    void fetch();
    void publish();
    ii::network::Http http_;
    iisacc::accounts::native::BrowserSessionOptions options_;
    iisacc::accounts::native::BrowserSession account_;
    std::string subject_;
    std::uint64_t generation_=0;
    std::shared_ptr<bool> alive_=std::make_shared<bool>(true);
};

} // namespace congregation
