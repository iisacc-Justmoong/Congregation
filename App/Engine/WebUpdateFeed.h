#pragma once
#include <iiUpdateManager/NativeUpdateManager.h>
#include <iiAccountManager/BrowserSession.h>
#include <functional>
#include <memory>

namespace congregation {
class WebUpdateFeed final : public iisacc::updates::native::UpdateFeed {
public:
    WebUpdateFeed(ii::runtime::EventLoop&, std::function<std::string()> token,
                  std::string origin = "https://iisacc.com");
    ~WebUpdateFeed() override;
    void check(std::string product, std::string platform, std::string current, Completion) override;
    void grant(std::string product, std::string platform, std::string version,
               std::string requestId, Completion) override;
    void cancel() noexcept override;
private:
    void request(std::string product, std::string platform, std::string action,
                 std::string version, std::string requestId, Completion);
    ii::network::Http http_;
    std::function<std::string()> token_;
    std::string origin_;
    std::uint64_t generation_=0;
    std::shared_ptr<bool> alive_=std::make_shared<bool>(true);
};
}
