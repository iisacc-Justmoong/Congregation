#include "WebLicenseService.h"

#include <cmath>
#include <utility>

namespace congregation {
using namespace iisacc::accounts::native;
using namespace ii::network::wire;

namespace {
std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
}

WebLicenseService::WebLicenseService(ii::runtime::EventLoop& loop, BrowserSessionOptions options,
                                     SessionPersistence store)
    : http_(loop), options_(std::move(options)), account_(loop, options_, std::move(store)) {
    account_.set_callbacks({
        [this](auto snapshot) { session_changed(std::move(snapshot)); },
        [this](const auto& url) { if(open_browser) open_browser(url); }
    });
}
WebLicenseService::~WebLicenseService() {
    *alive_=false;
    ++generation_;
    http_.cancel();
    account_.set_callbacks({});
}
void WebLicenseService::publish() { if(changed) changed(); }
void WebLicenseService::session_changed(BrowserSessionSnapshot state) {
    ++generation_;
    http_.cancel();
    licensed=false;
    signed_in=state.state==BrowserSessionState::connected;
    verifying=state.state==BrowserSessionState::connecting ||
              state.state==BrowserSessionState::awaiting_browser ||
              state.state==BrowserSessionState::refreshing;
    email=state.email;
    subject_=state.subject;
    code=state.state==BrowserSessionState::awaiting_browser ? "waiting_for_browser" : state.code;
    if(signed_in) { fetch(); return; }
    publish();
}
void WebLicenseService::sign_in() { (void)account_.sign_in(); }
void WebLicenseService::restore() {
    if(!account_.restore(true)) session_changed(account_.snapshot());
}
void WebLicenseService::retry() {
    if(signed_in) fetch();
    else sign_in();
}
void WebLicenseService::cancel() {
    ++generation_;
    http_.cancel();
    account_.cancel();
}
void WebLicenseService::forget() { account_.sign_out(); }
void WebLicenseService::fetch() {
    ++generation_;
    http_.cancel();
    licensed=false;
    const auto token=account_.access_token();
    if(token.empty()) {
        signed_in=false; verifying=false; code="sign_in_required"; publish(); return;
    }
    verifying=true;
    code="checking_license";
    const auto generation=generation_;
    auto weak=std::weak_ptr(alive_);
    publish();
    auto alive=weak.lock();
    if(!alive || !*alive || generation!=generation_) return;
    try {
        auto request=app_access_request(options_.origin+"/graphql",
            Object{{"operation",Value("license")},{"productId",Value(options_.product_id)}},token);
        request.timeout=options_.timeout;
        if(http_.start(std::move(request),[this,weak,generation](ii::network::HttpResponse response) {
            auto alive=weak.lock();
            if(!alive || !*alive || generation!=generation_) return;
            const auto result=app_access_result(response);
            std::optional<bool> flag;
            if(result) if(const auto value=field(*result,"licensed").get<bool>()) flag=value->get();
            const auto checked=result ? number(field(*result,"checkedAt")) : 0.;
            const auto expiry=result ? field(*result,"expiresAt") : Value{};
            const auto expiration=number(expiry);
            const bool expiry_valid=expiry.kind()==ii::core::ValueKind::null ||
                (std::isfinite(expiration) && std::floor(expiration)==expiration && expiration>now());
            const bool valid=result && result->contains("expiresAt") && flag && text(field(*result,"productId"))==options_.product_id &&
                text(field(*result,"subject"))==subject_ && !subject_.empty() &&
                std::isfinite(checked) && std::floor(checked)==checked && checked>0 &&
                checked<=now()+300 && checked>=now()-300 && expiry_valid;
            verifying=false;
            licensed=valid && *flag;
            code=!valid ? "invalid_license_response" : licensed ? "licensed" : "license_not_owned";
            publish();
        })) return;
    } catch(const std::exception&) {}
    verifying=false;
    code="verification_unavailable";
    publish();
}
} // namespace congregation
