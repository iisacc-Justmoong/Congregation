#pragma once
#include "Data.h"
#include "LicensePresentation.h"
#include <iiAccountManager/SocietySession.h>
namespace congregation {
class WebLicenseService;
class LicenseService final {
public:
    enum class Notice { licensed, verifying, stored, code };
    enum class Authority { society, web };
    LicenseService(ii::runtime::EventLoop&, iisacc::accounts::native::SocietySessionOptions, bool enforced,
                   Authority = Authority::society, iisacc::accounts::native::SessionPersistence = {});
    ~LicenseService();
    std::function<void(Notice)> changed;
    std::function<void(bool)> validated;
    std::function<void()> open_society_action;
    std::function<void(std::string)> open_browser_action;
    bool enforced=true, licensed=false, verifying=false, signed_in=false;
    std::string code, email;
    // Visibility is not an authorization decision; commands still require licensed.
    bool workspace_visible() const { return license_workspace_visible(enforced, licensed, initial_verification_pending_); }
    void sign_in();
    void restore();
    void retry();
    void open_society();
    void cancel();
    void forget();
private:
    void session_changed(iisacc::accounts::native::SocietySessionSnapshot);
    void web_changed();
    void notify_state();
    std::unique_ptr<iisacc::accounts::native::SocietySession> session_;
    std::unique_ptr<WebLicenseService> web_;
    bool initial_verification_pending_=true;
    bool last_validated_=false;
    bool launch_granted_=false;
    std::shared_ptr<bool> alive_=std::make_shared<bool>(true);
};
}
