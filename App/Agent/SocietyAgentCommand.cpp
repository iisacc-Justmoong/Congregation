#include "WorkspaceAgentClient.h"
#include <iiSDKCore>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace congregation {
namespace {
std::filesystem::path society_cli() {
    std::vector<std::filesystem::path> candidates;
#if defined(__APPLE__) && !TARGET_OS_IPHONE
    candidates.emplace_back("/Applications/Society.app/Contents/MacOS/society-cli");
    if (const auto* home=std::getenv("HOME")) candidates.emplace_back(std::filesystem::path(home)/"Applications/Society.app/Contents/MacOS/society-cli");
#endif
#ifdef _WIN32
    constexpr char separator=';'; constexpr auto name="society-cli.exe";
#else
    constexpr char separator=':'; constexpr auto name="society-cli";
#endif
    if (const auto* env=std::getenv("PATH")) {
        std::string paths(env); std::size_t start=0;
        while (start<=paths.size()) {
            auto end=paths.find(separator,start); if (end==std::string::npos) end=paths.size();
            if (end>start) candidates.push_back(std::filesystem::path(paths.substr(start,end-start))/name);
            start=end+1;
        }
    }
    for (const auto& path:candidates) { std::error_code ec; if (std::filesystem::is_regular_file(path,ec)) return path; }
    return {};
}
}
WorkspaceAgentCommandResult society_agent_command(std::string_view operation, std::string_view input, std::stop_token stop) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
    (void)operation; (void)input; (void)stop;
    return {{},"The Society workspace agent is not available on this device.",false};
#else
    auto path=society_cli();
    if (path.empty()) return {{},"Install and open Society to use its workspace agent.",false};
    return society_agent_command_at(path,operation,input,stop);
#endif
}
WorkspaceAgentCommandResult society_agent_command_at(const std::filesystem::path& path, std::string_view operation, std::string_view input, std::stop_token stop) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
    (void)path; (void)operation; (void)input; (void)stop;
    return {{},"The Society workspace agent is not available on this device.",false};
#else
    if (operation!="send" && operation!="status") return {{},"Unsupported Society agent operation.",false};
    ii::process::Launch launch;
    launch.arguments={path.string(),"agent",std::string(operation)};
    if (operation=="send") launch.arguments.push_back("--stdin");
    auto child=ii::process::Process::start(launch);
    if (!child) return {{},"Could not start the Society agent client.",false};
    ii::process::CommunicationOptions options;
    options.stop=stop; options.deadline=ii::core::Deadline::after(std::chrono::seconds(15));
    options.output_limit=4*1024*1024; options.error_limit=64*1024;
    auto result=child->communicate(ii::core::byte_view(input),std::move(options));
    if (!result) return {{},"The Society agent client could not complete the request.",operation=="send"};
    if (result->output_truncated || result->input_closed_early || result->exit.reason!=ii::process::ExitReason::exited)
        return {{},"The Society agent client did not complete the request.",operation=="send"};
    const auto& bytes=result->output;
    std::string output;
    if (!bytes.empty()) output.assign(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if (output.empty()) return {{},"The Society agent client returned no response.",operation=="send"};
    return {std::move(output),{},false};
#endif
}
}
