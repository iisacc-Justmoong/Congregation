#pragma once
#include <chrono>
#include <functional>
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>

namespace congregation {

struct WorkspaceAgentUpdate {
    enum class Phase { accepted, responding, completed, failed, uncertain };
    Phase phase = Phase::failed;
    std::string status;
    std::string response;
};

struct WorkspaceAgentCommandResult {
    std::string output;
    std::string error;
    bool uncertain = false;
};

// One request. This object must be called on a worker, never the UI thread.
// The injected command is also the boundary for deterministic, inference-free tests.
class WorkspaceAgentClient final {
public:
    using Command = std::function<WorkspaceAgentCommandResult(std::string_view, std::string_view, std::stop_token)>;
    using Observer = std::function<void(WorkspaceAgentUpdate)>;
    explicit WorkspaceAgentClient(Command, std::chrono::milliseconds poll_interval = std::chrono::milliseconds(750));
    void run(std::string_view prompt, Observer, std::stop_token = {}) const;
private:
    Command command_;
    std::chrono::milliseconds poll_interval_;
};

// Launches the separately installed, official Society client without a shell.
// The prompt is passed through stdin; it is never part of process arguments.
[[nodiscard]] WorkspaceAgentCommandResult society_agent_command(std::string_view operation,
                                                                 std::string_view input,
                                                                 std::stop_token stop);
[[nodiscard]] WorkspaceAgentCommandResult society_agent_command_at(const std::filesystem::path& executable,
                                                                    std::string_view operation,
                                                                    std::string_view input,
                                                                    std::stop_token stop = {});
}
