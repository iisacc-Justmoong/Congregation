#include "NearbyDiscovery.h"
#include <set>
#include <sstream>
namespace congregation
{
namespace
{
constexpr auto service = "com.iisacc.congregation.nearby";
bool valid_port(const Value& v)
{
    const auto n = number(v, -1);
    return n >= 1 && n <= 65535 && std::floor(n) == n;
}
bool header(const Value& v)
{
    return v.kind() == ii::core::ValueKind::object && text(field(v, "service")) == service &&
           number(field(v, "version"), -1) == 1;
}
std::size_t utf16_size(std::string_view s)
{
    std::size_t n = 0;
    for (unsigned char c : s)
        if ((c & 192) != 128)
            n += (c >= 240 ? 2 : 1);
    return n;
}
} // namespace
std::string profile_name(std::string name)
{
    if (!ii::core::valid_utf8(name))
        return {};
    std::string result;
    bool pending_space = false;
    std::size_t units = 0;
    for (std::size_t i = 0; i < name.size();)
    {
        auto [c, width] = codepoint(name, i);
        if (unicode_space(c))
        {
            pending_space = !result.empty();
            i += width;
            continue;
        }
        const auto size = c >= 0x10000 ? 2U : 1U;
        if (units + size + (pending_space ? 1 : 0) > 80)
            break;
        if (pending_space)
        {
            result += ' ';
            ++units;
            pending_space = false;
        }
        result.append(name, i, width);
        units += size;
        i += width;
    }
    return result;
}

std::string encode_presence(const Presence& p)
{
    if (!canonical_uuid(p.session))
        return {};
    Object o{{"service", Value(service)},
             {"version", Value(1)},
             {"session", Value(lower(p.session))},
             {"state", Value(p.online ? "online" : "offline")}};
    if (p.online && p.port)
        o.emplace("canvasPort", Value(p.port));
    if (p.online && p.invitations)
        o.emplace("invitationsAllowed", Value(true));
    return json(Value(std::move(o)), 512);
}
std::optional<Presence> decode_presence(std::string_view data)
{
    auto v = parse_json(data, 512);
    if (!v || !header(*v) || !canonical_uuid(text(field(*v, "session"))))
        return {};
    auto o = object(*v);
    auto state = text(field(o, "state"));
    if (state != "online" && state != "offline")
        return {};
    if (o.contains("canvasPort") && !valid_port(field(o, "canvasPort")))
        return {};
    if (o.contains("invitationsAllowed") &&
        field(o, "invitationsAllowed").kind() != ii::core::ValueKind::boolean)
        return {};
    const bool online = state == "online";
    return Presence{lower(text(field(o, "session"))), online,
                    online ? static_cast<std::uint16_t>(number(field(o, "canvasPort")))
                           : std::uint16_t(0),
                    online && boolean(field(o, "invitationsAllowed"))};
}
std::string encode_invitation(const Invitation& i)
{
    auto name = profile_name(i.name);
    if (!canonical_uuid(i.id) || !canonical_uuid(i.sender) || !canonical_uuid(i.target) ||
        lower(i.sender) == lower(i.target) || !i.port || name.empty())
        return {};
    return json(Value(Object{{"service", Value(service)},
                             {"version", Value(1)},
                             {"type", Value("canvas-invitation")},
                             {"invitation", Value(lower(i.id))},
                             {"sender", Value(lower(i.sender))},
                             {"target", Value(lower(i.target))},
                             {"canvasPort", Value(i.port)},
                             {"profileName", Value(name)}}),
                512);
}
std::optional<Invitation> decode_invitation(std::string_view data)
{
    auto v = parse_json(data, 512);
    if (!v || !header(*v) || text(field(*v, "type")) != "canvas-invitation")
        return {};
    auto id = text(field(*v, "invitation")), sender = text(field(*v, "sender")),
         target = text(field(*v, "target")), name = profile_name(text(field(*v, "profileName")));
    if (!canonical_uuid(id) || !canonical_uuid(sender) || !canonical_uuid(target) ||
        lower(sender) == lower(target) || !valid_port(field(*v, "canvasPort")) || name.empty() ||
        utf16_size(text(field(*v, "profileName"))) > 80)
        return {};
    return Invitation{lower(id), lower(sender), lower(target),
                      static_cast<std::uint16_t>(number(field(*v, "canvasPort"))), name};
}
NearbyDiscovery::NearbyDiscovery(ii::runtime::EventLoop& loop, Configuration config)
    : loop_(loop), config_(std::move(config)), socket_(loop)
{
    config_.heartbeat_ms = std::max(50, config_.heartbeat_ms);
    config_.timeout_ms = std::max(config_.heartbeat_ms * 2, config_.timeout_ms);
    config_.prune_ms = std::clamp(config_.prune_ms, 25, config_.timeout_ms);
    if (!config_.port)
        config_.port = 52683;
    socket_.received = [this](auto d) { receive(std::move(d)); };
}
NearbyDiscovery::~NearbyDiscovery()
{
    changed = {};
    invited = {};
    stop();
}
void NearbyDiscovery::notify()
{
    if (changed)
        changed();
}
void NearbyDiscovery::start()
{
    if (running)
        return;
    error.clear();
    if (!socket_.bind_multicast(config_.group, config_.port, config_.loopback))
    {
        error = socket_.error();
        notify();
        return;
    }
    running = true;
    auto h = loop_.every(std::chrono::milliseconds(config_.heartbeat_ms), [this] { announce(); });
    auto p = loop_.every(std::chrono::milliseconds(config_.prune_ms), [this] { prune(); });
    heartbeat_ = h.token;
    prune_ = p.token;
    if (!h || !p)
    {
        stop();
        error = "Core discovery timer admission failed";
        notify();
        return;
    }
    announce();
    notify();
}
void NearbyDiscovery::stop()
{
    heartbeat_.cancel();
    prune_.cancel();
    if (running)
        announce(false);
    socket_.close();
    running = false;
    peers_.clear();
    invitations_.clear();
    error.clear();
    notify();
}
void NearbyDiscovery::announce(bool online)
{
    if (online)
        (void) socket_.refresh_interfaces();
    auto payload = encode_presence({id, online, hosted_port, invitations});
    if (!socket_.send(payload, config_.group, config_.port))
        error = "Local network multicast sending is unavailable.";
    else
        error.clear();
}
void NearbyDiscovery::set_port(std::uint16_t port)
{
    if (hosted_port == port)
        return;
    hosted_port = port;
    if (running)
        announce();
    notify();
}
void NearbyDiscovery::set_invitations(bool allowed)
{
    if (invitations == allowed)
        return;
    invitations = allowed;
    if (running)
        announce();
    notify();
}
bool NearbyDiscovery::invite(std::string invitation, std::string target, std::uint16_t port,
                             std::string name)
{
    auto peer = peers_.find(lower(target));
    if (!running || peer == peers_.end() || !peer->second.invitations)
        return false;
    auto bytes =
        encode_invitation({std::move(invitation), id, lower(target), port, std::move(name)});
    return !bytes.empty() && socket_.send(bytes, config_.group, config_.port);
}
void NearbyDiscovery::receive(ii::network::Datagram data)
{
    if (data.bytes.size() > 512 || (config_.ignore_local && ii::network::own_address(data.address)))
        return;
    auto now = ii::runtime::Clock::now();
    if (auto i = decode_invitation(data.bytes))
    {
        auto sender = peers_.find(i->sender);
        if (!invitations || i->target != id || i->sender == id || sender == peers_.end() ||
            sender->second.address != data.address || invitations_.contains(i->id))
            return;
        if (invitations_.size() >= 256)
        {
            auto oldest = std::min_element(invitations_.begin(), invitations_.end(),
                                           [](auto& a, auto& b) { return a.second < b.second; });
            invitations_.erase(oldest);
        }
        invitations_[i->id] = now;
        if (invited)
            invited(Object{{"invitationId", Value(i->id)},
                           {"sessionId", Value(i->sender)},
                           {"address", Value(data.address)},
                           {"port", Value(i->port)},
                           {"profileName", Value(i->name)}});
        return;
    }
    auto p = decode_presence(data.bytes);
    if (!p || p->session == id)
        return;
    if (p->online)
    {
        if (!peers_.contains(p->session) && peers_.size() >= 256)
        {
            auto oldest = std::min_element(peers_.begin(), peers_.end(), [](auto& a, auto& b)
                                           { return a.second.seen < b.second.seen; });
            peers_.erase(oldest);
        }
        peers_.insert_or_assign(p->session, Peer{data.address, now, p->port, p->invitations});
    }
    else
        peers_.erase(p->session);
    notify();
}
void NearbyDiscovery::prune()
{
    auto oldest = ii::runtime::Clock::now() - std::chrono::milliseconds(config_.timeout_ms);
    auto removed = std::erase_if(peers_, [oldest](auto& p) { return p.second.seen < oldest; });
    std::erase_if(invitations_, [oldest](auto& p) { return p.second < oldest; });
    if (removed)
        notify();
}
int NearbyDiscovery::count() const
{
    std::set<std::string> addresses;
    for (const auto& [id, p] : peers_)
        addresses.insert(p.address);
    return static_cast<int>(addresses.size());
}
Array NearbyDiscovery::canvases() const
{
    Array out;
    for (const auto& [id, p] : peers_)
        if (p.port)
            out.emplace_back(Object{
                {"sessionId", Value(id)}, {"address", Value(p.address)}, {"port", Value(p.port)}});
    return out;
}
Array NearbyDiscovery::targets() const
{
    Array out;
    for (const auto& [id, p] : peers_)
        if (p.invitations)
            out.emplace_back(Object{{"sessionId", Value(id)}, {"address", Value(p.address)}});
    return out;
}
} // namespace congregation
