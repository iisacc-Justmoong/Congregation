#include "LicenseService.h"
namespace congregation
{
using namespace iisacc::accounts::native;
namespace
{
std::string serialize(std::string_view email, std::string_view key)
{
    return json(Value(Object{{"schema", Value(1)},
                             {"email", Value(email)},
                             {"licenseKey", Value(key)},
                             {"productId", Value("congregation")}}));
}
std::optional<Credentials> parse(std::string_view bytes)
{
    auto root = parse_json(bytes, 64 * 1024);
    if (!root || root->kind() != ii::core::ValueKind::object)
        return {};
    auto o = object(*root);
    auto email = text(field(o, "email")), key = text(field(o, "licenseKey"));
    if (o.size() != 4 || number(field(o, "schema")) != 1 ||
        text(field(o, "productId")) != "congregation" || !valid_email(email) ||
        !valid_license_key(key) || normalize_email(email) != email || trim(key) != key ||
        serialize(email, key) != bytes)
        return {};
    return Credentials(std::move(email), std::move(key));
}
void erase(std::string& s)
{
    for (volatile char& c : s)
        c = 0;
    s.clear();
}
} // namespace
Credentials::~Credentials()
{
    clear();
}
void Credentials::clear()
{
    erase(email);
    erase(key);
}
LicenseService::LicenseService(ii::runtime::EventLoop& loop, std::string endpoint, int timeout,
                               bool enforcement, CredentialPort store)
    : enforced(enforcement), licensed(!enforcement), http_(loop), endpoint_(std::move(endpoint)),
      timeout_(std::max(1, timeout)), store_(std::move(store))
{
}
LicenseService::~LicenseService()
{
    *alive_ = false;
    http_.cancel();
}
bool LicenseService::persistence() const
{
    return store_.supported && store_.supported();
}
void LicenseService::set_licensed(bool value)
{
    if (licensed == value)
        return;
    licensed = value;
    if (changed)
        changed(Notice::licensed);
}
void LicenseService::set_verifying(bool value)
{
    if (verifying == value)
        return;
    verifying = value;
    if (changed)
        changed(Notice::verifying);
}
void LicenseService::set_stored(bool value)
{
    if (stored == value)
        return;
    stored = value;
    if (changed)
        changed(Notice::stored);
}
void LicenseService::set_code(std::string value)
{
    if (code == value)
        return;
    code = std::move(value);
    if (changed)
        changed(Notice::code);
}
void LicenseService::complete(bool valid)
{
    set_verifying(false);
    if (validated)
        validated(valid);
}
void LicenseService::credentials(std::function<void(StoredStatus, Credentials)> completion)
{
    if (!completion)
        return;
    if (!persistence())
    {
        completion(StoredStatus::unavailable, {});
        return;
    }
    auto weak = std::weak_ptr(alive_);
    store_.read(
        [weak, completion = std::move(completion)](auto status, std::string data) mutable
        {
            auto alive = weak.lock();
            if (!alive || !*alive)
            {
                erase(data);
                return;
            }
            if (status != SecretResult::Status::found)
            {
                erase(data);
                completion(status == SecretResult::Status::not_found ? StoredStatus::not_found
                                                                     : StoredStatus::unavailable,
                           {});
                return;
            }
            auto c = parse(data);
            erase(data);
            if (!c)
            {
                completion(StoredStatus::invalid, {});
                return;
            }
            completion(StoredStatus::available, std::move(*c));
        });
}
void LicenseService::restore()
{
    if (!enforced || !persistence() || verifying || licensed)
        return;
    set_verifying(true);
    auto weak = std::weak_ptr(alive_);
    credentials(
        [this, weak](auto status, Credentials c)
        {
            auto alive = weak.lock();
            if (!alive || !*alive)
                return;
            set_verifying(false);
            if (status == StoredStatus::unavailable)
            {
                set_code("secure_storage_unavailable");
                return;
            }
            if (status == StoredStatus::invalid)
            {
                set_code("stored_license_removed");
                clear_stored();
                return;
            }
            if (status != StoredStatus::available)
                return;
            saved_ = std::move(c);
            set_stored(true);
            start(saved_.email, saved_.key, true);
        });
}
void LicenseService::validate(std::string email, std::string key)
{
    if (!enforced || licensed || verifying)
        return;
    email = normalize_email(std::move(email));
    key = trim(std::move(key));
    if (!valid_email(email) || !valid_license_key(key))
    {
        set_licensed(false);
        set_code("invalid_input");
        if (validated)
            validated(false);
        return;
    }
    start(std::move(email), std::move(key), false);
}
void LicenseService::retry()
{
    if (enforced && !licensed && !verifying && stored && valid_email(saved_.email) &&
        valid_license_key(saved_.key))
        start(saved_.email, saved_.key, true);
}
void LicenseService::forget()
{
    if (!enforced || licensed)
        return;
    http_.cancel();
    set_verifying(false);
    set_licensed(false);
    set_code({});
    clear_stored();
}
void LicenseService::clear_stored()
{
    active_.clear();
    saved_.clear();
    set_stored(false);
    if (!persistence())
        return;
    auto weak = std::weak_ptr(alive_);
    store_.remove(
        [this, weak](bool removed)
        {
            auto alive = weak.lock();
            if (alive && *alive && !removed)
                set_code("secure_storage_unavailable");
        });
}
void LicenseService::start(std::string email, std::string key, bool restored)
{
    set_code({});
    set_verifying(true);
    uses_saved_ = restored;
    active_ = Credentials(std::move(email), std::move(key));
    ii::network::HttpRequest request;
    request.url = endpoint_;
    request.method = "POST";
    request.timeout = std::chrono::milliseconds(timeout_);
    request.maximum_bytes = 64 * 1024;
    request.headers = {{"Content-Type", "application/json"},
                       {"Accept", "application/json"},
                       {"Cache-Control", "no-store"},
                       {"Pragma", "no-cache"}};
    request.body = graphql_request(
        "mutation ValidateLicense($input: LicenseValidationInput!) { validateLicense(input: "
        "$input) { valid code productId checkedAt expiresAt } }",
        Object{{"email", Value(active_.email)},
               {"licenseKey", Value(active_.key)},
               {"productId", Value("congregation")}});
    if (!http_.start(std::move(request), [this](auto response) { finish(std::move(response)); }))
    {
        set_licensed(false);
        set_code("verification_unavailable");
        complete(false);
    }
}
void LicenseService::finish(ii::network::HttpResponse response)
{
    bool authoritative = false, valid = false;
    auto content_type = lower(response.headers["content-type"]);
    if (response.error.empty() && response.status == 200 && !response.too_large &&
        (content_type == "application/json" || content_type.starts_with("application/json;")))
    {
        auto o = graphql_result(response.body, "validateLicense", response.status);
        if (o && field(*o, "valid").kind() == ii::core::ValueKind::boolean)
        {
            const auto& p = field(*o, "productId");
            const bool match = p.kind() == ii::core::ValueKind::null || text(p) == "congregation";
            if (match)
            {
                valid = boolean(field(*o, "valid"));
                authoritative = !valid || text(p) == "congregation";
            }
        }
    }
    if (authoritative && valid && !uses_saved_ && persistence())
    {
        saved_ = Credentials(active_.email, active_.key);
        auto data = serialize(saved_.email, saved_.key);
        auto weak = std::weak_ptr(alive_);
        store_.write(data,
                     [this, weak](bool stored)
                     {
                         auto alive = weak.lock();
                         if (!alive || !*alive)
                             return;
                         set_stored(stored);
                         saved_.clear();
                         set_code(stored ? "" : "secure_storage_unavailable");
                         set_licensed(true);
                         complete(true);
                     });
        erase(data);
        active_.clear();
        return;
    }
    if (authoritative)
    {
        set_licensed(valid);
        set_code(valid ? "" : "invalid_license");
        if (valid)
        {
            active_.clear();
            saved_.clear();
        }
        else if (uses_saved_)
            clear_stored();
        else
            active_.clear();
    }
    else
    {
        set_licensed(false);
        set_code("verification_unavailable");
        if (!uses_saved_)
            active_.clear();
    }
    complete(authoritative && valid);
}
} // namespace congregation
