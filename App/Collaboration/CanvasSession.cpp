#include <Serialization/IiscCodec.h>
#include "CanvasSession.h"
#include "NearbyDiscovery.h"
#include "Document/RecentCanvas.h"
#include <array>
#include <new>
#include <set>
#include <stdexcept>
namespace congregation
{
namespace
{
using namespace std::chrono_literals;
constexpr std::size_t max_control = 64 * 1024, max_delta = maximum_canvas_bytes,
                      max_frame = maximum_canvas_bytes + max_control,
                      max_peers = 16;
constexpr unsigned protocol_version = 2;
enum Message
{
    Hello = 1,
    Accepted,
    Snapshot = 4,
    Participants,
    Error,
    DeltaRequest,
    Delta
};
std::string frame(unsigned type, std::string_view payload)
{
    if (payload.size() > max_frame)
        return {};
    std::string out = "VCAN";
    out += char(protocol_version);
    out += static_cast<char>(type);
    out += char(0);
    out += char(0);
    append_be(out, payload.size(), 8);
    out += payload;
    return out;
}
int take(std::string& buffer, unsigned& type, std::string& payload, bool from_client = false)
{
    if (buffer.size() < 16)
        return 0;
    if (buffer.compare(0, 4, "VCAN") || buffer[4] != protocol_version || buffer[6] != 0 || buffer[7] != 0)
        return -1;
    auto bytes = std::string_view(buffer).substr(8);
    std::uint64_t size = 0;
    take_be(bytes, size, 8);
    const auto message = static_cast<unsigned char>(buffer[5]);
    const auto bound = from_client ? (message == Hello  ? max_control
                                      : message == DeltaRequest ? max_delta
                                                        : 0ULL)
                                   : (message == Snapshot ? max_frame : message == Delta ? max_delta : max_control);
    if (size > bound || !bound)
        return -1;
    if (size > buffer.size() - 16)
        return 0;
    type = static_cast<unsigned char>(buffer[5]);
    payload = buffer.substr(16, size);
    buffer.erase(0, 16 + size);
    return 1;
}
bool send(ii::network::Tcp* socket, unsigned type, std::string_view payload)
{
    if (!socket || !socket->is_connected())
        return false;
    auto bytes = frame(type, payload);
    return !bytes.empty() && socket->write(bytes);
}
std::optional<Object> control(std::string_view bytes)
{
    auto p = parse_json(bytes, max_control);
    if (!p || p->kind() != ii::core::ValueKind::object)
        return {};
    return object(*p);
}
std::string display_name(std::string s)
{
    s = profile_name(std::move(s));
    return s.empty() ? "Unnamed member" : s;
}
Value profile_record(std::string peer, std::string name, bool host, bool removable)
{
    return Value(Object{{"peerId", Value(peer)},
                        {"profileName", Value(name)},
                        {"profileImageSource", Value("")},
                        {"isHost", Value(host)},
                        {"isMe", Value(false)},
                        {"removable", Value(removable)}});
}
} // namespace
CanvasSession::CanvasSession(ii::runtime::EventLoop& loop, std::string own)
    : CanvasSession(loop, std::move(own), default_peer_pending_bytes)
{}
CanvasSession::CanvasSession(ii::runtime::EventLoop& loop, std::string own,
                             std::size_t peer_pending_bytes, SnapshotWriter snapshot_writer)
    : own_session(std::move(own)), session(own_session), loop_(loop), listener_(loop),
      peer_pending_bytes_(peer_pending_bytes), snapshot_writer_(std::move(snapshot_writer))
{
    if (!peer_pending_bytes_ || peer_pending_bytes_ > default_peer_pending_bytes)
        throw std::invalid_argument("Invalid canvas peer send budget");
    listener_.accepted = [this](auto socket) { accepted(std::move(socket)); };
}
CanvasSession::~CanvasSession()
{
    changed = {};
    advertise = {};
    stop();
}
void CanvasSession::notify(Notice n)
{
    if (changed)
        changed(n);
}
void CanvasSession::set_state(State value)
{
    if (state == value)
        return;
    state = value;
    notify(Notice::state);
}
void CanvasSession::set_error(std::string value)
{
    if (error == value)
        return;
    error = std::move(value);
    notify(Notice::error);
}
std::uint16_t CanvasSession::port() const
{
    return listener_.port();
}
Object CanvasSession::hello() const
{
    return {{"service", Value("com.iisacc.congregation.canvas")},
            {"version", Value(protocol_version)},
            {"session", Value(session)},
            {"peer", Value(peer_id)},
            {"name", Value(profile)}};
}
bool CanvasSession::host(std::string name)
{
    stop();
    profile = display_name(std::move(name));
    session = own_session;
    if (!listener_.listen())
    {
        set_error(listener_.error());
        return false;
    }
    set_error({});
    set_state(State::hosting);
    if (advertise)
        advertise(port());
    return true;
}
bool CanvasSession::join(std::string requested, std::string address, int port, std::string name)
{
    if (!canonical_uuid(requested) || !ii::network::local_address(address) || port <= 0 ||
        port > 65535)
    {
        set_error("The nearby canvas address is invalid.");
        return false;
    }
    stop();
    session = lower(requested);
    profile = display_name(std::move(name));
    client_ = std::make_unique<ii::network::Tcp>(loop_);
    client_->receive_limit(max_frame + 16);
    client_->connected = [this]
    {
        if (state == State::joining && !send(client_.get(), Hello, json(Value(hello()))))
        {
            set_error("The canvas connection handshake could not be sent.");
            client_closed();
        }
    };
    client_->readable = [this] { client_read(); };
    client_->disconnected = [this] { client_closed(); };
    client_->failed = [this](auto message)
    {
        if (error.empty())
            set_error(std::move(message));
    };
    set_error({});
    set_state(State::joining);
    deadline_ = loop_
                    .after(8s,
                           [this]
                           {
                               if (state == State::joining)
                               {
                                   set_error("The nearby canvas did not respond.");
                                   client_closed();
                               }
                           })
                    .token;
    if (!client_->connect(address, static_cast<std::uint16_t>(port)))
    {
        set_error(client_->error());
        client_closed();
        return false;
    }
    return true;
}
void CanvasSession::stop()
{
    ++session_generation_;
    stopping_ = true;
    deadline_.cancel();
    listener_.close();
    for (auto& [id, p] : peers_)
    {
        p.deadline.cancel();
        p.retry.cancel();
        p.socket->close();
    }
    peers_.clear();
    client_.reset();
    buffer_.clear();
    latest_frame_.reset();
    host_peer.clear();
    if (advertise)
        advertise(0);
    session = own_session;
    if (revision)
    {
        revision = 0;
        notify(Notice::revision);
    }
    if (!profiles.empty())
    {
        profiles.clear();
        notify(Notice::profiles);
    }
    stopping_ = false;
    set_state(State::idle);
}
void CanvasSession::set_profile(std::string name)
{
    auto value = display_name(std::move(name));
    if (profile == value)
        return;
    profile = std::move(value);
    if (state == State::hosting)
        participants();
    else if (state == State::connected)
    {
        auto h = hello();
        h.emplace("update", Value(true));
        send(client_.get(), Hello, json(Value(std::move(h))));
    }
}
void CanvasSession::set_invitations(bool value)
{
    if (invitations == value)
        return;
    invitations = value;
    if (!value && !pending.empty())
    {
        pending.clear();
        notify(Notice::pending);
    }
    notify(Notice::invitations);
}
bool CanvasSession::checkpoint()
{
    if (!snapshot_provider || state != State::hosting || !revision)
        return false;
    try {
        const auto generation = session_generation_;
        const auto current_revision = revision;
        const auto snapshot = snapshot_provider();
        if (generation != session_generation_ || state != State::hosting || revision != current_revision)
            return false;
        if ((!decode_canvas(snapshot) && !iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(snapshot.data()), snapshot.size()}).ok()) ||
            snapshot.size() > max_frame - 44 || peer_pending_bytes_ <= 60 ||
            snapshot.size() > peer_pending_bytes_ - 60)
            return false;
        std::string payload;
        append_be(payload, current_revision, 8);
        payload += peer_id;
        payload += snapshot;
        auto encoded = frame(Snapshot, payload);
        if (encoded.empty()) return false;
        latest_frame_ = std::make_shared<const std::string>(std::move(encoded));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
bool CanvasSession::initialize(std::string_view snapshot)
{
    if (state != State::hosting || revision != 0) return false;
    if (snapshot.empty() || snapshot.size() > max_frame - 44 || peer_pending_bytes_ <= 60 ||
        snapshot.size() > peer_pending_bytes_ - 60) {
        set_error("The initial canvas exceeds the participant send budget.");
        return false;
    }
    if (!decode_canvas(snapshot) &&
        !iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(snapshot.data()), snapshot.size()}).ok()) {
        set_error("The initial canvas is invalid.");
        return false;
    }
    try {
        std::string payload;
        append_be(payload, 1, 8);
        payload += peer_id;
        payload += snapshot;
        latest_frame_ = std::make_shared<const std::string>(frame(Snapshot, payload));
        const auto generation = session_generation_;
        revision = 1;
        notify(Notice::revision);
        if (session_generation_ != generation || state != State::hosting) return false;
        set_error({});
        return session_generation_ == generation && state == State::hosting;
    } catch (const std::bad_alloc&) {
        set_error("The initial canvas could not be allocated.");
        return false;
    }
}
bool CanvasSession::publish_delta(std::string_view delta, std::string_view origin)
{
    if (state != State::hosting || !revision || revision == UINT64_MAX || delta.empty() ||
        delta.size() > max_delta - 44 || (origin.size() && !canonical_uuid(origin)))
        return false;
    try {
        std::string payload;
        append_be(payload, revision + 1, 8);
        payload += origin.empty() ? peer_id : std::string(origin);
        payload += delta;
        auto bytes = frame(Delta, payload);
        if (bytes.empty() || bytes.size() > peer_pending_bytes_) {
            set_error("The canvas change exceeds the participant send budget.");
            return false;
        }
        const auto outgoing = std::make_shared<const std::string>(std::move(bytes));
        const auto generation = session_generation_;
        std::array<std::shared_ptr<ii::network::Tcp>, max_peers> targets, rejected;
        std::size_t count = 0, failed = 0;
        const auto available = peer_pending_bytes_ - outgoing->size();
        for (auto& [id, peer] : peers_) {
            if (!peer.accepted) continue;
            const auto snapshotBytes = peer.pending_snapshot ? peer.pending_snapshot->size() : 0;
            if (peer.pending_delta_bytes > available ||
                snapshotBytes > available - peer.pending_delta_bytes ||
                peer.socket->pending_bytes() > available - peer.pending_delta_bytes - snapshotBytes) {
                rejected[failed++] = peer.socket;
                continue;
            }
            try {
                peer.pending_deltas.push_back(outgoing);
                peer.pending_delta_bytes += outgoing->size();
                targets[count++] = peer.socket;
            } catch (const std::bad_alloc&) {
                rejected[failed++] = peer.socket;
            }
        }
        ++revision;
        latest_frame_.reset();
        for (std::size_t i = 0; i < failed && generation == session_generation_; ++i)
            reject(rejected[i].get(), "backpressure", "The participant could not keep up with canvas changes.");
        if (generation != session_generation_ || state != State::hosting) return false;
        notify(Notice::revision);
        for (std::size_t i = 0; i < count && generation == session_generation_; ++i)
            retry_latest(targets[i].get());
        return session_generation_ == generation && state == State::hosting;
    } catch (const std::exception&) {
        set_error("The canvas change could not be queued.");
        return false;
    }
}
bool CanvasSession::submit_delta(std::string_view delta, std::uint64_t base_revision)
{
    if (state != State::connected || !client_ || delta.empty() || delta.size() > max_delta - 44)
        return false;
    try {
        std::string payload;
        append_be(payload, base_revision, 8);
        payload += delta;
        if (send(client_.get(), DeltaRequest, payload)) { set_error({}); return true; }
    } catch (const std::bad_alloc&) {}
    set_error("The host canvas change could not be sent.");
    return false;
}
bool CanvasSession::remove(std::string_view peer)
{
    if (state != State::hosting || !canonical_uuid(peer))
        return false;
    for (const auto& [id, p] : peers_)
        if (p.accepted && p.id == peer)
        {
            reject(id, "removed", "The host removed you from this canvas.");
            return true;
        }
    return false;
}
void CanvasSession::receive_invitation(Object invitation)
{
    if (!invitations)
        return;
    auto id = lower(text(field(invitation, "invitationId"))),
         sender = lower(text(field(invitation, "sessionId"))),
         address = text(field(invitation, "address"));
    auto port = number(field(invitation, "port"));
    if (!canonical_uuid(id) || !canonical_uuid(sender) || sender == own_session ||
        !ii::network::local_address(address) || port < 1 || port > 65535 ||
        std::floor(port) != port)
        return;
    for (const auto& p : pending)
        if (text(field(p, "invitationId")) == id)
            return;
    std::erase_if(pending, [&](auto& p) { return text(field(p, "sessionId")) == sender; });
    if (pending.size() >= 16)
        pending.erase(pending.begin());
    invitation.insert_or_assign("profileName",
                                Value(display_name(text(field(invitation, "profileName")))));
    invitation.insert_or_assign("profileImageSource", Value(""));
    pending.emplace_back(std::move(invitation));
    notify(Notice::pending);
}
bool CanvasSession::respond(bool accepted, std::string name)
{
    if (pending.empty())
        return false;
    auto i = object(pending.front());
    pending.erase(pending.begin());
    notify(Notice::pending);
    if (!accepted)
        return true;
    return join(text(field(i, "sessionId")), text(field(i, "address")),
                static_cast<int>(number(field(i, "port"))), std::move(name));
}
void CanvasSession::accepted(std::unique_ptr<ii::network::Tcp> socket)
{
    auto* id = socket.get();
    if (!ii::network::local_address(socket->peer_address()) || peers_.size() >= max_peers)
    {
        socket->close();
        return;
    }
    socket->receive_limit(max_frame + 16);
    socket->send_limit(peer_pending_bytes_);
    socket->readable = [this, id] { host_read(id); };
    socket->bytes_written = [this, id](std::size_t) { retry_latest(id); };
    socket->disconnected = [this, id]
    {
        auto i = peers_.find(id);
        if (i == peers_.end())
            return;
        auto accepted = i->second.accepted;
        i->second.deadline.cancel();
        i->second.retry.cancel();
        peers_.erase(i);
        if (accepted && state == State::hosting && !stopping_)
            participants();
    };
    Peer peer;
    peer.socket = std::shared_ptr<ii::network::Tcp>(std::move(socket));
    peer.deadline =
        loop_
            .after(8s,
                   [this, id]
                   {
                       auto i = peers_.find(id);
                       if (i != peers_.end() && !i->second.accepted)
                           reject(id, "timeout", "The canvas connection handshake timed out.");
                   })
            .token;
    peers_.emplace(id, std::move(peer));
}
void CanvasSession::host_read(ii::network::Tcp* id)
{
    auto i = peers_.find(id);
    if (i == peers_.end())
        return;
    auto keep = i->second.socket;
    i->second.buffer += id->read();
    for (;;)
    {
        unsigned type = 0;
        std::string payload;
        auto read = take(i->second.buffer, type, payload, true);
        if (read == 0)
            return;
        if (read < 0)
        {
            reject(id, "protocol", "The canvas connection used an invalid protocol frame.");
            return;
        }
        host_frame(id, type, payload);
        i = peers_.find(id);
        if (i == peers_.end() || !id->is_connected())
            return;
    }
}
void CanvasSession::host_frame(ii::network::Tcp* id, unsigned type, std::string_view payload)
{
    auto i = peers_.find(id);
    if (i == peers_.end())
        return;
    auto& p = i->second;
    if (type == Hello)
    {
        const auto held = p.socket;
        const auto generation = session_generation_;
        auto h = control(payload);
        if (!h || text(field(*h, "service")) != "com.iisacc.congregation.canvas" ||
            number(field(*h, "version")) != protocol_version || text(field(*h, "session")) != session)
        {
            reject(id, "handshake", "The canvas connection handshake is invalid.");
            return;
        }
        auto remote = lower(text(field(*h, "peer")));
        if (!canonical_uuid(remote) || remote == peer_id)
        {
            reject(id, "identity", "The canvas participant identity is invalid.");
            return;
        }
        for (const auto& [other, q] : peers_)
            if (other != id && q.accepted && q.id == remote)
            {
                reject(id, "duplicate", "This Congregation session is already connected.");
                return;
            }
        if (p.accepted && (!boolean(field(*h, "update")) || p.id != remote))
        {
            reject(id, "identity",
                   "The canvas participant identity changed or repeated its handshake.");
            return;
        }
        p.name = display_name(text(field(*h, "name")));
        if (p.accepted) {
            participants();
            return;
        }
        if (!latest_frame_ && !checkpoint()) {
            if (auto current = peers_.find(id); generation == session_generation_ &&
                current != peers_.end() && current->second.socket == held)
                reject(id, "checkpoint", "The initial canvas could not be prepared.");
            return;
        }
        auto current = peers_.find(id);
        if (generation != session_generation_ || state != State::hosting || current == peers_.end() ||
            current->second.socket != held) return;
        if (!send(id, Accepted, json(Value(hello())))) {
            reject(id, "handshake", "The canvas connection could not be initialized.");
            return;
        }
        current->second.id = remote;
        current->second.accepted = true;
        current->second.deadline.cancel();
        current->second.pending_snapshot = latest_frame_;
        retry_latest(id);
        if (generation == session_generation_ && state == State::hosting) participants();
        return;
    }
    if (!p.accepted || type != DeltaRequest)
    {
        reject(id, "protocol", "The canvas connection sent an unexpected message.");
        return;
    }
    if (type == DeltaRequest)
    {
        std::uint64_t base = 0;
        if (!take_be(payload, base, 8) || payload.empty() || base != revision)
        {
            reject(id, "revision", "The shared canvas changed. Local recovery is retained; reconnect before sending another edit.");
            return;
        }
        const auto remote = p.id;
        const auto held = p.socket;
        const auto generation = session_generation_;
        bool applied = false;
        try { applied = delta_requested && delta_requested(payload, remote); }
        catch (const std::exception&) {}
        if (!applied)
        {
            if (auto current = peers_.find(id); current != peers_.end() && current->second.socket == held)
                reject(id, "change", "The canvas change could not be applied.");
            return;
        }
        if (generation != session_generation_ || state != State::hosting) return;
        if (revision != base) {
            set_error("The shared canvas changed while a remote edit was being applied.");
            stop(); return;
        }
        if (!publish_delta(payload, remote))
        {
            set_error("The accepted canvas change could not be published; reconnect to synchronize.");
            stop();
        }
        return;
    }

}
void CanvasSession::client_read()
{
    if (!client_)
        return;
    buffer_ += client_->read();
    while (client_)
    {
        unsigned type = 0;
        std::string payload;
        auto read = take(buffer_, type, payload);
        if (read == 0)
            return;
        if (read < 0)
        {
            set_error("The host sent an invalid canvas protocol frame.");
            client_closed();
            return;
        }
        client_frame(type, payload);
    }
}
void CanvasSession::client_frame(unsigned type, std::string_view payload)
{
    if (type == Error)
    {
        auto e = control(payload);
        set_error(e ? text(field(*e, "message")) : "The host rejected the canvas connection.");
        client_closed();
        return;
    }
    if (type == Accepted)
    {
        auto h = control(payload);
        if (state != State::joining || !h ||
            text(field(*h, "service")) != "com.iisacc.congregation.canvas" ||
            number(field(*h, "version")) != protocol_version || text(field(*h, "session")) != session ||
            !canonical_uuid(text(field(*h, "peer"))))
        {
            set_error("The host returned an invalid canvas handshake.");
            client_closed();
            return;
        }
        if (!host_peer.empty()) {
            set_error("The host repeated the canvas handshake.");
            client_closed();
            return;
        }
        host_peer = lower(text(field(*h, "peer")));
        return;
    }
    if (state != State::connected && (state != State::joining || host_peer.empty()))
        return;
    if (type == Participants)
    {
        apply_profiles(payload);
        return;
    }
    if (type == Delta)
    {
        std::uint64_t incoming = 0;
        if (!take_be(payload, incoming, 8) || !incoming || payload.size() <= 36)
        {
            set_error("The host sent an invalid canvas change.");
            client_closed(); return;
        }
        const auto origin = std::string(payload.substr(0, 36));
        payload.remove_prefix(36);
        if (state != State::connected || !canonical_uuid(origin) ||
            revision == UINT64_MAX || incoming != revision + 1 || !delta_received)
        {
            set_error("The shared canvas change is out of order; local recovery is retained.");
            client_closed(); return;
        }
        const auto generation = session_generation_;
        bool applied = false;
        try { applied = delta_received(payload, incoming, origin); }
        catch (const std::exception&) {}
        if (!applied)
        {
            set_error("The shared canvas change could not be applied; local recovery is retained.");
            client_closed(); return;
        }
        if (generation != session_generation_ || state != State::connected || !client_) return;
        revision = incoming;
        notify(Notice::revision);
        return;
    }
    if (type != Snapshot || state != State::joining || revision != 0)
    {
        set_error("The host sent an unexpected canvas message.");
        client_closed();
        return;
    }
    std::uint64_t incoming = 0;
    if (!take_be(payload, incoming, 8) || !incoming || payload.size() <= 36)
    {
        set_error("The host sent an invalid canvas update.");
        client_closed();
        return;
    }
    auto origin = std::string(payload.substr(0, 36));
    payload.remove_prefix(36);
    if (origin != host_peer || (!snapshot_applying && !decode_canvas(payload) &&
        !iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(payload.data()),payload.size()}).ok()))
    {
        set_error("The host sent an invalid canvas update.");
        client_closed();
        return;
    }
    const auto generation = session_generation_;
    bool applied = true;
    try { if (snapshot_applying) applied = snapshot_applying(payload, incoming, origin); }
    catch (const std::exception&) { applied = false; }
    if (generation != session_generation_ || state != State::joining || !client_) return;
    if (!applied) {
        set_error("The shared canvas checkpoint could not be applied; local recovery is retained.");
        client_closed();
        return;
    }
    revision = incoming;
    deadline_.cancel();
    set_state(State::connected);
    if (generation != session_generation_ || state != State::connected || !client_) return;
    notify(Notice::revision);
    if (generation == session_generation_ && state == State::connected && client_ && snapshot_received)
        snapshot_received(std::string(payload), incoming, std::move(origin));
}
void CanvasSession::client_closed()
{
    if (!client_)
        return;
    client_->close();
    client_.reset();
    deadline_.cancel();
    buffer_.clear();
    if (!profiles.empty())
    {
        profiles.clear();
        notify(Notice::profiles);
    }
    if (stopping_)
        return;
    if (error.empty())
        set_error("The host closed the canvas session.");
    session = own_session;
    set_state(State::idle);
}
void CanvasSession::retry_latest(ii::network::Tcp* socket)
{
    auto peer = peers_.find(socket);
    if (peer == peers_.end() || (!peer->second.pending_snapshot && peer->second.pending_deltas.empty()) || !socket->is_connected() ||
        peer->second.writing_snapshot)
        return;
    // A writer can synchronously stop/rehost or publish another update. Keep
    // the socket and attempted frame alive, and reacquire the peer afterwards.
    const auto held_socket = peer->second.socket;
    const auto attempted = peer->second.pending_snapshot ? peer->second.pending_snapshot : peer->second.pending_deltas.front();
    const auto generation = session_generation_;
    peer->second.writing_snapshot = true;
    const auto current_peer = [&]() -> Peer*
    {
        if (session_generation_ != generation || state != State::hosting)
            return nullptr;
        auto current = peers_.find(socket);
        return current != peers_.end() && current->second.socket == held_socket
                   ? &current->second : nullptr;
    };
    bool accepted = false;
    const auto retry_failed = [&] {
        set_error("A canvas update could not be scheduled for retry.");
        if (current_peer())
            reject(socket, "backpressure", "The participant could not receive canvas changes.");
    };
    try
    {
        accepted = snapshot_writer_ ? snapshot_writer_(*held_socket, attempted)
                                    : held_socket->write(attempted);
    }
    catch (const std::exception&)
    {
        // Keep this exact frame at the front until the writer accepts it.
    }
    auto* current = current_peer();
    if (!current)
        return;
    current->writing_snapshot = false;
    if (accepted)
    {
        if (current->pending_snapshot == attempted)
            current->pending_snapshot.reset();
        else if (!current->pending_deltas.empty() && current->pending_deltas.front() == attempted) {
            current->pending_delta_bytes -= attempted->size();
            current->pending_deltas.pop_front();
        }
        if (!current->pending_snapshot && current->pending_deltas.empty()) {
            current->retry.cancel();
            current->retry = {};
            return;
        }
    }
    if ((!current->pending_snapshot && current->pending_deltas.empty()) || !held_socket->is_connected() || current->retry.pending())
        return;
    try
    {
        auto retry = loop_.after(50ms, [this, socket]
        {
            auto current = peers_.find(socket);
            if (current == peers_.end())
                return;
            current->second.retry = {};
            retry_latest(socket);
        });
        if (retry)
            current->retry = retry.token;
        else
            retry_failed();
    }
    catch (const std::bad_alloc&)
    {
        retry_failed();
    }
}

void CanvasSession::reject(ii::network::Tcp* socket, std::string code, std::string message)
{
    send(socket, Error, json(Value(Object{{"code", Value(code)}, {"message", Value(message)}})));
    socket->close(true);
}
void CanvasSession::participants()
{
    Array visible, wire;
    wire.emplace_back(
        Object{{"peer", Value(peer_id)}, {"name", Value(profile)}, {"host", Value(true)}});
    std::map<std::string, const Peer*> sorted;
    for (const auto& [id, p] : peers_)
        if (p.accepted)
            sorted.emplace(p.id, &p);
    for (const auto& [id, p] : sorted)
    {
        visible.push_back(profile_record(id, p->name, false, true));
        wire.emplace_back(
            Object{{"peer", Value(id)}, {"name", Value(p->name)}, {"host", Value(false)}});
    }
    if (profiles != visible)
    {
        profiles = std::move(visible);
        notify(Notice::profiles);
    }
    auto payload = json(Value(std::move(wire)), max_control);
    for (const auto& [id, p] : peers_)
        if (p.accepted)
            send(id, Participants, payload);
}
void CanvasSession::apply_profiles(std::string_view payload)
{
    auto root = parse_json(payload, max_control);
    if (!root || root->kind() != ii::core::ValueKind::array)
        return;
    auto entries = array(*root);
    if (entries.size() > 17)
        return;
    std::set<std::string> ids;
    int hosts = 0;
    bool me = false;
    Array visible;
    for (const auto& p : entries)
    {
        if (p.kind() != ii::core::ValueKind::object)
            return;
        auto id = text(field(p, "peer"));
        if (!canonical_uuid(id) || !ids.insert(id).second)
            return;
        bool host = boolean(field(p, "host"));
        hosts += host;
        if (id == peer_id)
        {
            me = true;
            continue;
        }
        visible.push_back(profile_record(id, display_name(text(field(p, "name"))), host, false));
    }
    if (hosts != 1 || !me)
        return;
    if (profiles != visible)
    {
        profiles = std::move(visible);
        notify(Notice::profiles);
    }
}
} // namespace congregation
