#pragma once
#include <iiUpdateManager/NativeUpdateManager.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <filesystem>
#include <memory>

namespace congregation {
// Same-user Society broker; authentication and online requests stay in Society.
class SocietyUpdateFeed final : public iisacc::updates::native::UpdateFeed {
public:
    SocietyUpdateFeed(ii::runtime::EventLoop&, std::string device,
                      std::filesystem::path socket = {});
    ~SocietyUpdateFeed() override;
    void check(std::string product, std::string platform, std::string current, Completion) override;
    void grant(std::string product, std::string platform, std::string version,
               std::string requestId, Completion) override;
    void cancel() noexcept override;
private:
    struct State;
    void request(std::string product, std::string platform, std::string action,
                 std::string version, std::string requestId, Completion);
    std::shared_ptr<State> state_;
};
}
