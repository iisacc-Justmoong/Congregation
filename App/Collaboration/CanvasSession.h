#pragma once
#include "Shared/Data.h"
#include <iiNetwork>
#include <deque>
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
    static constexpr std::size_t default_peer_pending_bytes = 513ULL * 1024 * 1024;
    using SnapshotWriter = std::function<bool(ii::network::Tcp&, std::shared_ptr<const std::string>)>;
    CanvasSession(ii::runtime::EventLoop&, std::string own_session);
    CanvasSession(ii::runtime::EventLoop&, std::string own_session, std::size_t peer_pending_bytes,
                  SnapshotWriter snapshot_writer = {});
    ~CanvasSession();
    std::function<void(Notice)> changed;
    // A fresh checkpoint is requested only when a participant joins after a delta.
    std::function<std::string()> snapshot_provider;
    // If supplied, the durable application must succeed before the revision advances.
    std::function<bool(std::string_view, std::uint64_t, std::string_view)> snapshot_applying;
    std::function<void(std::string, std::uint64_t, std::string)> snapshot_received;
    std::function<bool(std::string_view, std::string_view)> delta_requested;
    std::function<bool(std::string_view, std::uint64_t, std::string_view)> delta_received;
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
    bool initialize(std::string_view);
    bool publish_delta(std::string_view, std::string_view origin = {});
    bool submit_delta(std::string_view, std::uint64_t base_revision);
    bool remove(std::string_view);
    void receive_invitation(Object);
    bool respond(bool, std::string);
    [[nodiscard]] std::uint16_t port() const;

  private:
    struct Peer final
    {
        std::shared_ptr<ii::network::Tcp> socket;
        std::string buffer, id, name;
        std::shared_ptr<const std::string> pending_snapshot;
        std::deque<std::shared_ptr<const std::string>> pending_deltas;
        std::size_t pending_delta_bytes = 0;
        bool accepted = false;
        bool writing_snapshot = false;
        ii::runtime::TaskToken deadline, retry;
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
    void retry_latest(ii::network::Tcp*);
    bool checkpoint();
    Object hello() const;
    ii::runtime::EventLoop& loop_;
    ii::network::Listener listener_;
    std::size_t peer_pending_bytes_;
    SnapshotWriter snapshot_writer_;
    std::unique_ptr<ii::network::Tcp> client_;
    std::map<ii::network::Tcp*, Peer> peers_;
    ii::runtime::TaskToken deadline_;
    std::string buffer_;
    std::shared_ptr<const std::string> latest_frame_;
    std::uint64_t session_generation_ = 0;
    bool stopping_ = false;
};
} // namespace congregation
