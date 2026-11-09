#include "SocietyUpdateFeed.h"
#include <iiAccountManager/LocalChannel.h>
#include <iiAccountManager/SocietySession.h>
#include <iiNetwork/Wire.h>
#include <thread>

namespace congregation {
using namespace ii::network::wire;
struct SocietyUpdateFeed::State final {
    ii::runtime::EventLoop::Sender sender;
    std::string device;
    std::filesystem::path socket;
    std::uint64_t generation = 0;
    bool alive = true;
    State(ii::runtime::EventLoop& loop, std::string id, std::filesystem::path path)
        : sender(loop.sender()), device(std::move(id)), socket(std::move(path)) {
        if (socket.empty()) socket = iisacc::accounts::native::SocietySession::default_socket_path();
    }
};
SocietyUpdateFeed::SocietyUpdateFeed(ii::runtime::EventLoop& loop, std::string device, std::filesystem::path socket)
    : state_(std::make_shared<State>(loop, std::move(device), std::move(socket))) {}
SocietyUpdateFeed::~SocietyUpdateFeed() { state_->alive = false; ++state_->generation; }
void SocietyUpdateFeed::cancel() noexcept { ++state_->generation; }
void SocietyUpdateFeed::request(std::string product, std::string platform, std::string action,
                                std::string version, std::string id, Completion done) {
    auto state=state_;
    const auto revision=++state->generation;
    if(product!="congregation" || state->device.size()!=64 || !state->socket.is_absolute()) {
        done(std::nullopt,"Society update service unavailable"); return;
    }
    Object input{{"operation",Value("update")},{"productId",Value(product)},
        {"appId",Value("com.iisacc.congregation.painter")},{"deviceId",Value(state->device)},
        {"requestId",Value(id)},{"platformKey",Value(platform)},{"action",Value(action)}};
    input.emplace(action=="check" ? "currentVersion" : "version",Value(version));
    auto body=json(Value(std::move(input))); auto path=state->socket; auto sender=state->sender;
    std::weak_ptr<SocietyUpdateFeed::State> weak=state;
    auto completion=std::make_shared<Completion>(std::move(done));
    try {
        std::thread([weak,sender,revision,id,path,body=std::move(body),completion]() mutable {
            std::string response;
            auto connection=iisacc::accounts::local::Connection::connect(path,std::chrono::seconds(5));
            if(connection && connection->send(body,std::chrono::seconds(5))) {
                auto result=connection->receive(std::chrono::seconds(12)); if(result)response=std::move(*result);
            }
            (void)sender.post([weak,revision,id,response=std::move(response),completion]() mutable {
                auto state=weak.lock(); if(!state||!state->alive||state->generation!=revision)return;
                auto root=parse_json(response,65536);
                if(!root || text(field(*root,"requestId"))!=id || !field(*root,"result").get<Object>()) {
                    (*completion)(std::nullopt,"Society update service unavailable"); return;
                }
                (*completion)(json(field(*root,"result")),{});
            });
        }).detach();
    } catch(const std::exception&) { (*completion)(std::nullopt,"Society update service unavailable"); }
}
void SocietyUpdateFeed::check(std::string product,std::string platform,std::string current,Completion done) {
    request(std::move(product),std::move(platform),"check",std::move(current),uuid(),std::move(done));
}
void SocietyUpdateFeed::grant(std::string product,std::string platform,std::string version,std::string id,Completion done) {
    request(std::move(product),std::move(platform),"grant",std::move(version),std::move(id),std::move(done));
}
}
