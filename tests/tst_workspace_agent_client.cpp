#include <Agent/WorkspaceAgentClient.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace congregation;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
using Update = WorkspaceAgentUpdate;

void responds_to_own_message()
{
    std::vector<std::string> args, inputs;
    unsigned calls=0;
    WorkspaceAgentClient client([&](std::string_view operation, std::string_view input, std::stop_token) {
        args.emplace_back(operation); inputs.emplace_back(input);
        switch (calls++) {
        case 0: return WorkspaceAgentCommandResult{R"({"accepted":true,"agent":{"busy":true,"status":"Preparing","messages":[{"id":"u1","role":"You","text":"hello"}]}})"};
        case 1: return WorkspaceAgentCommandResult{R"({"accepted":true,"agent":{"busy":true,"status":"Responding","messages":[{"id":"u1","role":"You","text":"hello"},{"id":"a1","role":"Agent","text":"first"}]}})"};
        default: return WorkspaceAgentCommandResult{R"({"accepted":true,"agent":{"busy":false,"status":"Ready","messages":[{"id":"u1","role":"You","text":"hello"},{"id":"a1","role":"Agent","text":"first second"}]}})"};
        }
    }, std::chrono::milliseconds(0));
    std::vector<Update> updates;
    client.run("hello", [&](Update u) { updates.push_back(std::move(u)); });
    require(args==std::vector<std::string>({"send","status","status"}) && inputs[0]=="hello",
            "Prompt must travel through stdin and accepted work must be polled");
    require(updates.size()>=3 && updates.front().phase==Update::Phase::accepted &&
                updates.back().phase==Update::Phase::completed && updates.back().response=="first second",
            "The current request must stream and complete with its own response");
}
void rejection_uncertainty_and_changed_conversation()
{
    std::vector<Update> events;
    WorkspaceAgentClient rejected([](auto, auto, auto) { return WorkspaceAgentCommandResult{R"({"accepted":false,"error":"Select a model first.","uncertain":false})"}; }, std::chrono::milliseconds(0));
    rejected.run("hello",[&](Update u){events.push_back(std::move(u));});
    require(events.size()==1 && events[0].phase==Update::Phase::failed && events[0].status.find("Select")!=std::string::npos,
            "Host rejection must be explicit");
    events.clear(); unsigned calls=0;
    WorkspaceAgentClient uncertain([&](auto, auto, auto) { ++calls; return WorkspaceAgentCommandResult{R"({"accepted":false,"error":"lost acknowledgement","uncertain":true})"}; }, std::chrono::milliseconds(0));
    uncertain.run("hello",[&](Update u){events.push_back(std::move(u));});
    require(calls==1 && events.size()==1 && events[0].phase==Update::Phase::uncertain,
            "Uncertain delivery must not be retried");
    require(events[0].status.find("Check Society")!=std::string::npos,
            "Uncertain delivery must warn before a manual retry");
    events.clear(); calls=0;
    WorkspaceAgentClient changed([&](auto, auto, auto) {
        if (!calls++) return WorkspaceAgentCommandResult{R"({"accepted":true,"agent":{"busy":true,"messages":[{"id":"mine","role":"You","text":"hello"}]}})"};
        return WorkspaceAgentCommandResult{R"({"accepted":true,"agent":{"busy":false,"status":"Ready","messages":[{"id":"other","role":"You","text":"else"},{"id":"other-answer","role":"Agent","text":"private"}]}})"};
    },std::chrono::milliseconds(0));
    changed.run("hello",[&](Update u){events.push_back(std::move(u));});
    require(events.back().phase==Update::Phase::failed && events.back().response.empty(),
            "Another conversation must never be shown as this request's response");
}
}
int main(int argc, char** argv) {
    if (argc==4 && std::string_view(argv[1])=="agent" && std::string_view(argv[2])=="send" && std::string_view(argv[3])=="--stdin") {
        std::string input((std::istreambuf_iterator<char>(std::cin)),{}); std::cout << input; return 0;
    }
    try {
        responds_to_own_message(); rejection_uncertainty_and_changed_conversation();
        const auto process=society_agent_command_at(argv[0],"send","stdin only: $HOME 안녕");
        require(process.error.empty() && process.output=="stdin only: $HOME 안녕",
                "The real process adapter must pass the exact prompt through stdin without shell expansion");
    }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
