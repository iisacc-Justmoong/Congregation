#pragma once
#include "Data.h"
#include <iiNetwork/Network.h>
#include <map>
namespace congregation
{
class CanvasSession final
{
  public:
    enum class State
    {
        idle,
        hosting,
        joining,
        connected
    };
    enum class Notice
    {
        state,
        error,
        profiles,
        pending,
        revision,
        invitations
    };
    CanvasSession(ii::runtime::EventLoop&, std::string own_session);
    ~CanvasSession();
    std::function<void(Notice)> changed;
    std::function<void()> snapshot_requested;
    std::function<void(std::string, std::uint64_t, std::string)> snapshot_received;
    std::function<void(Object, std::string)> edit_received;
    std::function<void(std::uint16_t)> advertise;
    const std::string own_session, peer_id = uuid();
    std::string session, error, profile, host_peer;
    State state = State::idle;
    Array profiles, pending;
    std::uint64_t revision = 0;
    bool invitations = false;
    bool host(std::string name);
    bool join(std::string session, std::string address, int port, std::string name);
    void stop();
    void set_profile(std::string);
    void set_invitations(bool);
    bool publish(std::string_view);
    bool submit(const Object&);
    bool remove(std::string_view);
    void receive_invitation(Object);
    bool respond(bool, std::string);
    [[nodiscard]] std::uint16_t port() const;

  private:
    struct Peer final
    {
        std::shared_ptr<ii::network::Tcp> socket;
        std::string buffer, id, name;
        bool accepted = false;
        ii::runtime::TaskToken deadline;
    };
    void notify(Notice);
    void set_state(State);
    void set_error(std::string);
    void accepted(std::unique_ptr<ii::network::Tcp>);
    void host_read(ii::network::Tcp*);
    void host_frame(ii::network::Tcp*, unsigned, std::string_view);
    void client_read();
    void client_frame(unsigned, std::string_view);
    void client_closed();
    void reject(ii::network::Tcp*, std::string code, std::string message);
    void participants();
    void apply_profiles(std::string_view);
    void latest(ii::network::Tcp*);
    Object hello() const;
    ii::runtime::EventLoop& loop_;
    ii::network::Listener listener_;
    std::unique_ptr<ii::network::Tcp> client_;
    std::map<ii::network::Tcp*, Peer> peers_;
    ii::runtime::TaskToken deadline_, request_;
    std::string buffer_, origin_;
    std::shared_ptr<const std::string> latest_frame_;
    bool stopping_ = false;
};
} // namespace congregation
