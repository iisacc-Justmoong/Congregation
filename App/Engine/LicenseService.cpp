#include "LicenseService.h"
#include "WebLicenseService.h"
#include <exception>
#include <utility>

namespace congregation {
using namespace iisacc::accounts::native;
LicenseService::LicenseService(ii::runtime::EventLoop& loop, SocietySessionOptions options, bool enforcement,
                               Authority authority, SessionPersistence store)
    : enforced(enforcement), licensed(!enforcement) {
    if(authority==Authority::web) {
        BrowserSessionOptions config{.product_id=std::move(options.product_id),
                                     .device=std::move(options.device),
                                     .timeout=std::chrono::seconds(15)};
        try { web_=std::make_unique<WebLicenseService>(loop,std::move(config),std::move(store)); }
        catch(const std::exception&) { code="verification_unavailable"; return; }
        web_->changed=[this]{web_changed();};
        web_->open_browser=[this](const std::string& url){if(open_browser_action)open_browser_action(url);};
        return;
    }
    try { session_ = std::make_unique<SocietySession>(loop, std::move(options)); }
    catch (const std::exception&) { code = "society_unavailable"; return; }
    session_->changed = [this](auto snapshot) { session_changed(std::move(snapshot)); };
}
LicenseService::~LicenseService() { *alive_ = false; if (session_) session_->changed = {}; }
void LicenseService::session_changed(SocietySessionSnapshot value) {
    if (launch_granted_) return;
    signed_in = value.connected;
    email = signed_in ? "Society account" : "";
    verifying = value.checking;
    licensed = !enforced || value.licensed;
    code = std::move(value.code);
    if (enforced && value.licensed) {
        launch_granted_ = true;
        session_.reset();
    }
    notify_state();
}
void LicenseService::web_changed() {
    if(launch_granted_ || !web_) return;
    signed_in=web_->signed_in;
    email=web_->email;
    verifying=web_->verifying;
    licensed=!enforced || web_->licensed;
    code=web_->code;
    if(enforced && web_->licensed) launch_granted_=true;
    notify_state();
}
void LicenseService::notify_state() {
    const bool transition = licensed != last_validated_;
    last_validated_ = licensed;
    auto weak = std::weak_ptr(alive_);
    if (changed) changed(Notice::code);
    auto alive = weak.lock();
    if (alive && *alive && transition && validated) validated(licensed);
}
void LicenseService::sign_in() {
    if (!enforced) return;
    if(web_) { web_->sign_in(); return; }
    auto weak = std::weak_ptr(alive_);
    open_society();
    if (auto alive = weak.lock(); alive && *alive && session_) session_->refresh();
}
void LicenseService::restore() { if(enforced && web_) web_->restore(); else if(enforced && session_) session_->refresh(); }
void LicenseService::retry() { if(enforced && web_) web_->retry(); else if(enforced && session_) session_->refresh(true); }
void LicenseService::open_society() {
    if(web_) {
        if(!web_->signed_in) web_->sign_in();
        else if(open_browser_action) open_browser_action("https://iisacc.com/Account/Dashboard");
    } else if(open_society_action) open_society_action();
}
void LicenseService::cancel() { if(enforced && web_) web_->cancel(); else if(enforced && session_) session_->cancel(); }
void LicenseService::forget() { if(enforced && web_) web_->forget(); else open_society(); }
}
