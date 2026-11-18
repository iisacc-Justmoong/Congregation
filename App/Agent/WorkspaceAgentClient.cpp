#include "WorkspaceAgentClient.h"
#include <iiJson>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <utility>

namespace congregation {
namespace {
using Value = ii::json::Value;
const Value* member(const Value& v, std::string_view key) {
    auto found=v.find(key); return found ? &found->get() : nullptr;
}
std::string text(const Value* v) {
    if (!v) return {}; auto value=v->get<std::string>(); return value ? value->get() : std::string{};
}
bool boolean(const Value* v, bool fallback=false) {
    if (!v) return fallback; auto value=v->get<bool>(); return value ? value->get() : fallback;
}
struct Message { std::string id, role, text; };
struct Snapshot { bool busy=false; std::string status; std::vector<Message> messages; };
struct Reply { bool accepted=false, uncertain=false; std::string error; Snapshot snapshot; };

Reply decode(const WorkspaceAgentCommandResult& result, bool sending) {
    if (!result.error.empty()) return {.uncertain=result.uncertain, .error=result.error};
    auto tree=ii::json::parse(result.output, {.input_bytes=4*1024*1024, .output_bytes=4*1024*1024});
    if (!tree) return {.uncertain=sending, .error="The Society agent returned an invalid response."};
    Reply reply;
    const auto* accepted=member(*tree,"accepted");
    if (!accepted || !accepted->get<bool>())
        return {.uncertain=sending, .error="The Society agent response is incomplete."};
    reply.accepted=boolean(accepted);
    reply.uncertain=boolean(member(*tree,"uncertain"));
    reply.error=text(member(*tree,"error"));
    if (!reply.accepted) {
        if (reply.error.empty()) reply.error="The Society agent rejected the request.";
        return reply;
    }
    const auto* agent=member(*tree,"agent");
    const auto* messages=agent ? member(*agent,"messages") : nullptr;
    const auto* busy=agent ? member(*agent,"busy") : nullptr;
    if (!agent || !messages || !busy || !busy->get<bool>())
        return {.uncertain=sending, .error="The Society agent response is incomplete."};
    auto array=messages->get<Value::Array>();
    if (!array) return {.uncertain=sending, .error="The Society agent response is incomplete."};
    reply.snapshot.busy=boolean(busy);
    reply.snapshot.status=text(member(*agent,"status"));
    for (const auto& value : array->get())
        reply.snapshot.messages.push_back({text(member(value,"id")),text(member(value,"role")),text(member(value,"text"))});
    return reply;
}
}

WorkspaceAgentClient::WorkspaceAgentClient(Command command, std::chrono::milliseconds interval)
    : command_(std::move(command)), poll_interval_(std::max(interval,std::chrono::milliseconds::zero())) {}

void WorkspaceAgentClient::run(std::string_view prompt, Observer observer, std::stop_token stop) const {
    using Phase=WorkspaceAgentUpdate::Phase;
    if (!observer || stop.stop_requested()) return;
    if (!command_) { observer({Phase::failed,"The Society agent is unavailable.",{}}); return; }
    if (prompt.empty() || prompt.size()>16384 || prompt.find_first_not_of(" \t\r\n")==std::string_view::npos) {
        observer({Phase::failed,"Enter a message of at most 16 KiB.",{}}); return;
    }
    auto reply=decode(command_("send",prompt,stop),true);
    if (stop.stop_requested()) return;
    if (!reply.accepted) {
        if (reply.uncertain) reply.error="Delivery is uncertain. Check Society before retrying. "+reply.error;
        observer({reply.uncertain?Phase::uncertain:Phase::failed,reply.error,{}}); return;
    }
    std::string message_id;
    for (auto it=reply.snapshot.messages.rbegin(); it!=reply.snapshot.messages.rend(); ++it) {
        if (it->role=="You" && it->text==prompt) { message_id=it->id; break; }
    }
    observer({Phase::accepted,reply.snapshot.status.empty()?"Preparing response…":reply.snapshot.status,{}});
    if (message_id.empty()) {
        observer({Phase::uncertain,"The request was accepted, but its response cannot be identified.",{}}); return;
    }
    auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    std::string last_response, last_status;
    auto apply=[&](const Snapshot& snapshot) -> bool {
        const auto found=std::ranges::find_if(snapshot.messages,[&](const auto& message){return message.id==message_id && message.role=="You";});
        if (found==snapshot.messages.end()) {
            observer({Phase::failed,"The Society conversation changed before this response was found.",{}}); return true;
        }
        std::string response; bool next_prompt=false;
        for (auto it=found+1;it!=snapshot.messages.end();++it) {
            if (it->role=="You") { next_prompt=true; break; }
            if (it->role=="Agent") response=it->text;
        }
        if (next_prompt || !snapshot.busy) {
            if (response.empty()) observer({Phase::failed,snapshot.status.empty()?"The agent did not return a response.":snapshot.status,{}});
            else observer({Phase::completed,snapshot.status.empty()?"Ready":snapshot.status,std::move(response)});
            return true;
        }
        if (last_response!=response || last_status!=snapshot.status) {
            last_response=response; last_status=snapshot.status;
            observer({Phase::responding,snapshot.status.empty()?"Responding…":snapshot.status,std::move(response)});
        }
        return false;
    };
    if (apply(reply.snapshot)) return;
    std::mutex mutex; std::condition_variable_any wake;
    while (!stop.stop_requested() && std::chrono::steady_clock::now()<deadline) {
        { std::unique_lock lock(mutex); wake.wait_for(lock,stop,poll_interval_,[]{return false;}); }
        if (stop.stop_requested()) return;
        reply=decode(command_("status",{},stop),false);
        if (stop.stop_requested()) return;
        if (!reply.accepted) {
            observer({Phase::failed,"The response may still be running in Society. "+reply.error,last_response}); return;
        }
        if (apply(reply.snapshot)) return;
    }
    if (!stop.stop_requested()) observer({Phase::failed,"The response may still be running in Society. Reopen Society to check it.",last_response});
}
}
