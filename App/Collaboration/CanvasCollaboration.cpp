#include "CanvasCollaboration.h"
#include "Shared/ScopeExit.h"

namespace congregation::native {
CanvasCollaboration::CanvasCollaboration(CanvasSession& session, DocumentPort document)
    : session_(session), document_(std::move(document))
{
    session_.snapshot_provider = [this] { return document_.snapshot ? document_.snapshot() : std::string{}; };
    session_.snapshot_applying = [this](std::string_view bytes, std::uint64_t, std::string_view) {
        return receive(document_.initialize, bytes);
    };
    session_.delta_requested = [this](std::string_view bytes, std::string_view) {
        return receive(document_.apply, bytes);
    };
    session_.delta_received = [this](std::string_view bytes, std::uint64_t, std::string_view origin) {
        if (origin == session_.peer_id) return outgoing_.acknowledge(bytes);
        if (!outgoing_.empty()) return false;
        return receive(document_.apply, bytes);
    };
}
CanvasCollaboration::~CanvasCollaboration()
{
    session_.snapshot_provider = {};
    session_.snapshot_applying = {};
    session_.delta_requested = {};
    session_.delta_received = {};
}
bool CanvasCollaboration::receive(const std::function<bool(std::string_view)>& apply, std::string_view bytes)
{
    receiving_ = true;
    const ScopeExit reset([this] { receiving_ = false; });
    const bool ok = apply && apply(bytes);
    if (received) received(ok);
    return ok;
}
void CanvasCollaboration::close_with_error(std::string message)
{
    outgoing_.clear();
    session_.stop();
    if (status) status(std::move(message));
}
void CanvasCollaboration::flush()
{
    if (session_.state != CanvasSession::State::connected || !outgoing_.mark_sent()) return;
    if (!session_.submit_delta(outgoing_.front(), session_.revision))
        close_with_error("The canvas change could not be sent. Local recovery is retained.");
}
void CanvasCollaboration::local_change(const DocumentChange& change)
{
    if (receiving_ || change.kind == DocumentChangeKind::selection ||
        (session_.state != CanvasSession::State::hosting && session_.state != CanvasSession::State::connected))
        return;
    if (change.kind == DocumentChangeKind::reset) {
        session_.stop();
        return;
    }
    if (!change.edit || change.edit->empty()) {
        close_with_error("The canvas change is unavailable. Local recovery is retained.");
        return;
    }
    const std::string_view bytes(reinterpret_cast<const char*>(change.edit->data()), change.edit->size());
    if (session_.state == CanvasSession::State::hosting) {
        if (!session_.publish_delta(bytes))
            close_with_error("The canvas change could not be shared. Local recovery is retained.");
        return;
    }
    try {
        if (outgoing_.push(std::string(bytes))) { flush(); return; }
    } catch (const std::bad_alloc&) {}
    close_with_error("The canvas change queue is full. Local recovery is retained.");
}
void CanvasCollaboration::session_changed(CanvasSession::Notice notice)
{
    if (notice == CanvasSession::Notice::revision) flush();
    if (notice == CanvasSession::Notice::state && session_.state == CanvasSession::State::idle && !outgoing_.empty()) {
        outgoing_.clear();
        if (status) status("The collaboration connection closed. Local recovery is retained.");
    }
}
}
