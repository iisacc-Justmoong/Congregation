#include "WebUpdateFeed.h"
#include <iiNetwork/Wire.h>

namespace congregation {
using namespace ii::network::wire;
using namespace iisacc::accounts::native;
WebUpdateFeed::WebUpdateFeed(ii::runtime::EventLoop& loop, std::function<std::string()> token,
                             std::string origin)
    : http_(loop), token_(std::move(token)), origin_(std::move(origin)) {}
WebUpdateFeed::~WebUpdateFeed() { *alive_=false; cancel(); }
void WebUpdateFeed::cancel() noexcept { ++generation_; http_.cancel(); }
void WebUpdateFeed::request(std::string product, std::string platform, std::string action,
                            std::string version, std::string requestId, Completion done) {
    cancel();
    const auto token=token_ ? token_() : std::string{};
    if(product!="congregation" || token.empty()) { done(std::nullopt,"Sign in to check for updates"); return; }
    Object input{{"operation",Value("update")},{"productId",Value(product)},
                 {"platformKey",Value(platform)},{"action",Value(action)},{"requestId",Value(requestId)}};
    input.emplace(action=="check" ? "currentVersion" : "version",Value(version));
    auto request=app_access_request(origin_+"/graphql",std::move(input),token);
    const auto revision=generation_; auto weak=std::weak_ptr(alive_);
    auto completion=std::make_shared<Completion>(std::move(done));
    if(!http_.start(std::move(request),[this,weak,revision,completion](auto response) {
        auto alive=weak.lock(); if(!alive || !*alive || generation_!=revision)return;
        auto result=app_access_result(response);
        if(!result) { (*completion)(std::nullopt,"Update service unavailable"); return; }
        (*completion)(json(Value(*result)),{});
    })) (*completion)(std::nullopt,"Update service unavailable");
}
void WebUpdateFeed::check(std::string product,std::string platform,std::string current,Completion done) {
    request(std::move(product),std::move(platform),"check",std::move(current),uuid(),std::move(done));
}
void WebUpdateFeed::grant(std::string product,std::string platform,std::string version,std::string id,Completion done) {
    request(std::move(product),std::move(platform),"grant",std::move(version),std::move(id),std::move(done));
}
}
