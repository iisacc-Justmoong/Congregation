#include <Serialization/IiscCodec.h>
#include "CanvasSession.h"
#include "EditCommand.h"
#include "NearbyDiscovery.h"
#include "RecentCanvas.h"
#include <set>
namespace congregation
{
namespace
{
using namespace std::chrono_literals;
constexpr std::size_t max_control = 64 * 1024, max_frame = maximum_canvas_bytes + max_control;
enum Message
{
    Hello = 1,
    Accepted,
    Edit,
    Snapshot,
    Participants,
    Error
};
std::string frame(unsigned type, std::string_view payload)
{
    if (payload.size() > max_frame)
        return {};
    std::string out = "VCAN";
    out += char(1);
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
    if (buffer.compare(0, 4, "VCAN") || buffer[4] != 1 || buffer[6] != 0 || buffer[7] != 0)
        return -1;
    auto bytes = std::string_view(buffer).substr(8);
    std::uint64_t size = 0;
    take_be(bytes, size, 8);
    const auto message = static_cast<unsigned char>(buffer[5]);
    const auto bound = from_client ? (message == Hello  ? max_control
                                      : message == Edit ? 64ULL * 1024 * 1024
                                                        : 0ULL)
                                   : (message == Snapshot ? max_frame : max_control);
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
    : own_session(std::move(own)), session(own_session), loop_(loop), listener_(loop)
{
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
            {"version", Value(1)},
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
    request_ = loop_
                   .post(
                       [this]
                       {
                           if (state == State::hosting && !latest_frame_ && snapshot_requested)
                               snapshot_requested();
                       })
                   .token;
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
    stopping_ = true;
    deadline_.cancel();
    request_.cancel();
    listener_.close();
    for (auto& [id, p] : peers_)
    {
        p.deadline.cancel();
        p.socket->close();
    }
    peers_.clear();
    client_.reset();
    buffer_.clear();
    latest_frame_.reset();
    origin_.clear();
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
bool CanvasSession::publish(std::string_view snapshot)
{
    if (!decode_canvas(snapshot) && !iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(snapshot.data()),snapshot.size()}).ok())
    {
        set_error("The canvas snapshot is invalid or too large.");
        return false;
    }
    if (state != State::hosting || revision == UINT64_MAX)
        return false;
    origin_ = peer_id;
    ++revision;
    std::string outgoing = "VCAN";
    outgoing += char(1);
    outgoing += char(Snapshot);
    outgoing += char(0);
    outgoing += char(0);
    append_be(outgoing, snapshot.size() + 44, 8);
    append_be(outgoing, revision, 8);
    outgoing += origin_;
    outgoing += snapshot;
    latest_frame_ = std::make_shared<const std::string>(std::move(outgoing));
    notify(Notice::revision);
    for (const auto& [id, p] : peers_)
        if (p.accepted)
            latest(id);
    set_error({});
    return true;
}
bool CanvasSession::submit(const Object& command)
{
    if (state != State::connected || !client_)
        return false;
    auto payload = encode_command(command);
    if (payload.empty() || !send(client_.get(), Edit, payload))
    {
        set_error("The host canvas command could not be sent.");
        return false;
    }
    set_error({});
    return true;
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
    if (!ii::network::local_address(socket->peer_address()) || peers_.size() >= 16)
    {
        socket->close();
        return;
    }
    socket->receive_limit(max_frame + 16);
    socket->readable = [this, id] { host_read(id); };
    socket->disconnected = [this, id]
    {
        auto i = peers_.find(id);
        if (i == peers_.end())
            return;
        auto accepted = i->second.accepted;
        i->second.deadline.cancel();
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
        auto h = control(payload);
        if (!h || text(field(*h, "service")) != "com.iisacc.congregation.canvas" ||
            number(field(*h, "version")) != 1 || text(field(*h, "session")) != session)
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
        p.id = remote;
        p.name = display_name(text(field(*h, "name")));
        if (!p.accepted)
        {
            p.accepted = true;
            p.deadline.cancel();
            send(id, Accepted, json(Value(hello())));
        }
        participants();
        if (bool(latest_frame_))
            latest(id);
        else if (snapshot_requested)
            snapshot_requested();
        return;
    }
    if (!p.accepted || type != Edit)
    {
        reject(id, "protocol", "The canvas connection sent an unexpected message.");
        return;
    }
    auto command = decode_command(payload);
    if (!command)
    {
        reject(id, "command", "The host canvas command is invalid or too large.");
        return;
    }
    if(text(field(*command,"type"))=="nativeDocument"){const auto& version=field(*command,"baseRevision");auto u=version.get<std::uint64_t>();auto n=version.get<std::int64_t>();if((u?u->get():n?std::uint64_t(n->get()):UINT64_MAX)!=revision){reject(id,"revision","The shared canvas changed. Local recovery is retained; reconnect before sending another edit.");return;}}
    auto remote = p.id;
    if (edit_received)
        edit_received(std::move(*command), std::move(remote));
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
            number(field(*h, "version")) != 1 || text(field(*h, "session")) != session ||
            !canonical_uuid(text(field(*h, "peer"))))
        {
            set_error("The host returned an invalid canvas handshake.");
            client_closed();
            return;
        }
        host_peer = lower(text(field(*h, "peer")));
        deadline_.cancel();
        set_error({});
        set_state(State::connected);
        return;
    }
    if (state != State::connected)
        return;
    if (type == Participants)
    {
        apply_profiles(payload);
        return;
    }
    if (type != Snapshot)
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
    if (!canonical_uuid(origin) || (!decode_canvas(payload) && !iiSharedCanvas::decodeIisc({reinterpret_cast<const std::uint8_t*>(payload.data()),payload.size()}).ok()))
    {
        set_error("The host sent an invalid canvas update.");
        client_closed();
        return;
    }
    if (incoming <= revision)
        return;
    revision = incoming;
    notify(Notice::revision);
    if (snapshot_received)
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
void CanvasSession::latest(ii::network::Tcp* socket)
{
    if (latest_frame_ && socket)
        (void) socket->write(latest_frame_);
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
