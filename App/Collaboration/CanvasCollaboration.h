#pragma once
#include "CanvasSession.h"
#include "LocalChangeQueue.h"
#include "Document/DocumentChange.h"
#include <functional>

namespace congregation::native {
// The document owns edit and recovery state. This object orders delivery.
class CanvasCollaboration final {
public:
    struct DocumentPort {
        std::function<std::string()> snapshot;
        std::function<bool(std::string_view)> initialize;
        std::function<bool(std::string_view)> apply;
    };
    CanvasCollaboration(CanvasSession&, DocumentPort);
    ~CanvasCollaboration();
    CanvasCollaboration(const CanvasCollaboration&) = delete;
    CanvasCollaboration& operator=(const CanvasCollaboration&) = delete;
    void local_change(const DocumentChange&);
    void session_changed(CanvasSession::Notice);
    std::function<void(bool)> received;
    std::function<void(std::string)> status;
private:
    void flush();
    bool receive(const std::function<bool(std::string_view)>&, std::string_view);
    void close_with_error(std::string);
    CanvasSession& session_;
    DocumentPort document_;
    LocalChangeQueue outgoing_;
    bool receiving_ = false;
};
}
