#include "LicenseService.h"
#include <cmath>
#include <charconv>
namespace congregation
{
using namespace iisacc::accounts::native;
namespace
{
std::int64_t now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
} // namespace
LicenseService::LicenseService(ii::runtime::EventLoop &loop, BrowserSessionOptions options,
                               bool enforcement, SessionPersistence store, std::string society_endpoint,
                               std::vector<iisacc::licensing::owned::TrustedKey> keys)
    : enforced(enforcement), licensed(!enforcement), events_(loop.sender()), http_(loop),
      options_(std::move(options)), account_(loop, options_, std::move(store)),
      society_endpoint_(std::move(society_endpoint)), trusted_keys_(std::move(keys))
{
    constexpr std::string_view prefix="http://127.0.0.1:";
    constexpr std::string_view suffix="/v1/authorize";
    unsigned port=0;
    if(!society_endpoint_.starts_with(prefix)||!society_endpoint_.ends_with(suffix))
        throw std::invalid_argument("Invalid Society authorization endpoint");
    const auto part=std::string_view(society_endpoint_).substr(prefix.size(),society_endpoint_.size()-prefix.size()-suffix.size());
    const auto parsed=std::from_chars(part.data(),part.data()+part.size(),port);
    if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||!port||port>65535)
        throw std::invalid_argument("Invalid Society authorization port");
    account_.set_callbacks({[this](auto snapshot) { session_changed(std::move(snapshot)); },
                            [this](auto url)
                            {
                                if (open_browser)
                                    open_browser(std::move(url));
                            }});
}
LicenseService::~LicenseService()
{
    *alive_ = false;
    (void)refresh_.cancel();
    http_.cancel();
}
void LicenseService::publish()
{
    if (changed)
        changed(Notice::code);
}
void LicenseService::session_changed(BrowserSessionSnapshot value)
{
    ++generation_;
    http_.cancel();
    (void)refresh_.cancel();
    licensed = !enforced;
    if(value.state!=BrowserSessionState::connected)last_validated_=false;
    stored = value.persisted;
    email = value.email;
    subject_ = value.subject;
    signed_in = value.state == BrowserSessionState::connected;
    verifying = value.state == BrowserSessionState::connecting ||
                value.state == BrowserSessionState::awaiting_browser ||
                value.state == BrowserSessionState::refreshing;
    code = value.code;
    if (value.state == BrowserSessionState::awaiting_browser)
        code = "waiting_for_browser";
    if (signed_in && enforced)
    {
        fetch();
        return;
    }
    publish();
}
void LicenseService::sign_in()
{
    if (enforced)
        (void)account_.sign_in();
}
void LicenseService::restore()
{
    if (enforced)
    {
        if(!account_.restore(false, trusted_keys_))
            session_changed(account_.snapshot());
    }
}
void LicenseService::retry()
{
    if (!enforced)
        return;
    if (signed_in)
        fetch(true);
    else
        (void)account_.refresh();
}
void LicenseService::open_society() { if(open_browser)open_browser("society://account"); }
void LicenseService::cancel()
{
    if (enforced)
        account_.cancel();
}
void LicenseService::forget()
{
    if (enforced)
        account_.sign_out();
}
std::string LicenseService::access_token() const { return account_.access_token(); }
void LicenseService::fetch(bool force_refresh)
{
    ++generation_;
    http_.cancel();
    (void)refresh_.cancel();
    if(force_refresh)account_.clear_licenses();
    licensed = account_.license_manager().allows(options_.product_id,now());
    const auto token = account_.access_token();
    if (token.empty())
    {
        signed_in = false;
        verifying = false;
        code = "sign_in_required";
        publish();
        return;
    }
    verifying = true;
    code = "checking_license";
    const auto version = generation_;
    auto weak = std::weak_ptr(alive_);
    publish();
    auto alive = weak.lock();
    if (!alive || !*alive || version != generation_)
        return;
    request_id_=ii::network::wire::uuid();
    ii::network::HttpRequest request;
    request.method="POST";request.url=society_endpoint_;request.maximum_bytes=64*1024;
    request.headers={{"Content-Type","application/json"},{"Accept","application/json"},{"Cache-Control","no-store"}};
    request.body=json(Value(Object{{"productId",Value(options_.product_id)},{"appId",Value(options_.device.app_id)},
        {"deviceId",Value(options_.device.id)},{"subject",Value(subject_)},{"requestId",Value(request_id_)},{"refresh",Value(force_refresh)}}));
    request.timeout = options_.timeout;
    if (!http_.start(std::move(request),
                     [this, weak, version](auto response)
                     {
                         auto alive = weak.lock();
                         if (alive && *alive && version == generation_)
                             finish(std::move(response));
                     }))
    {
        finish({});
    }
}
void LicenseService::finish(ii::network::HttpResponse response)
{
    auto result=parse_json(response.body,64*1024);
    const auto media=response.headers.find("content-type");
    const bool valid_transport=response.error.empty()&&response.status==200&&!response.too_large&&
        media!=response.headers.end()&&media->second.starts_with("application/json")&&result&&text(field(*result,"requestId"))==request_id_;
    auto& manager=account_.license_manager();
    const auto token=valid_transport?text(field(*result,"licenseToken")):std::string{};
    verifying=false;
    if(valid_transport) {
        const auto flag=field(*result,"allowed").get<bool>();
        const bool verified=flag&&account_.cache_licenses(token,trusted_keys_);
        const bool agreed=verified&&flag->get()==manager.allows(options_.product_id,now());
        licensed=agreed&&flag->get();
        const auto denied_code=text(field(*result,"code"));
        const bool known_denial=denied_code=="society_account_required"||denied_code=="sign_in_required"||
            denied_code=="verification_unavailable"||denied_code=="request_timeout"||denied_code=="request_unavailable";
        code=agreed?(licensed?"licensed":"license_not_owned"):
            token.empty()&&known_denial?denied_code:"invalid_society_response";
        if(!agreed)account_.clear_licenses();
    } else {
        licensed=manager.allows(options_.product_id,now());
        code=manager.valid(now())?(licensed?"licensed_offline":"license_not_owned"):"society_unavailable";
        if(!manager.valid(now()))account_.clear_licenses();
    }
    const auto expiry=manager.valid(now())?manager.expires_at():now()+30;
    schedule_expiry(std::min(now()+30,expiry));
    const bool transition=last_validated_!=licensed;last_validated_=licensed;
    const auto version=generation_;const auto authorized=licensed;
    auto weak=std::weak_ptr(alive_);publish();const auto alive=weak.lock();
    if(alive&&*alive&&version==generation_&&transition&&validated)validated(authorized);
}
void LicenseService::schedule_expiry(std::int64_t expiry)
{
    auto weak = std::weak_ptr(alive_);
    auto scheduled = events_.after(std::chrono::seconds(std::max<std::int64_t>(1, expiry - now())),
                                   [this, weak]
                                   {
                                       auto alive = weak.lock();
                                       if (alive && *alive)
                                           fetch();
                                   });
    if (scheduled)
        refresh_ = scheduled.token;
    else
    {
        licensed = false;
        code = "request_unavailable";
    }
}
} // namespace congregation
