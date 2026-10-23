#pragma once
#include "Data.h"
#include <iiNetwork/Network.h>
#include <map>
namespace congregation
{
struct Presence final
{
    std::string session;
    bool online = false;
    std::uint16_t port = 0;
    bool invitations = false;
};
struct Invitation final
{
    std::string id, sender, target;
    std::uint16_t port = 0;
    std::string name;
};
std::string encode_presence(const Presence&);
std::optional<Presence> decode_presence(std::string_view);
std::string encode_invitation(const Invitation&);
std::optional<Invitation> decode_invitation(std::string_view);
std::string profile_name(std::string);
class NearbyDiscovery final
{
  public:
    struct Configuration final
    {
        std::string group = "239.255.86.67";
        std::uint16_t port = 52683;
        int heartbeat_ms = 4000, timeout_ms = 14000, prune_ms = 1000;
        bool ignore_local = true, loopback = false;
    };
    NearbyDiscovery(ii::runtime::EventLoop&, Configuration);
    ~NearbyDiscovery();
    std::function<void()> changed;
    std::function<void(Object)> invited;
    const std::string id = uuid();
    bool running = false, invitations = false;
    std::uint16_t hosted_port = 0;
    std::string error;
    void start();
    void stop();
    void set_port(std::uint16_t);
    void set_invitations(bool);
    bool invite(std::string id, std::string target, std::uint16_t port, std::string name);
    [[nodiscard]] int count() const;
    [[nodiscard]] Array canvases() const;
    [[nodiscard]] Array targets() const;

  private:
    struct Peer final
    {
        std::string address;
        ii::runtime::Clock::time_point seen;
        std::uint16_t port;
        bool invitations;
    };
    void announce(bool = true);
    void receive(ii::network::Datagram);
    void prune();
    void notify();
    ii::runtime::EventLoop& loop_;
    Configuration config_;
    ii::network::Udp socket_;
    ii::runtime::TaskToken heartbeat_, prune_;
    std::map<std::string, Peer> peers_;
    std::map<std::string, ii::runtime::Clock::time_point> invitations_;
};
} // namespace congregation
