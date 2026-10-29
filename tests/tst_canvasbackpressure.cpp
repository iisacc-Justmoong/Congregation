#include <Engine/CanvasSession.h>
#include <Engine/RecentCanvas.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace congregation;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class Predicate> bool wait(ii::runtime::EventLoop& loop,Predicate predicate,
                                    std::chrono::milliseconds duration=5000ms) {
    const auto deadline=ii::runtime::Clock::now()+duration;
    while(!predicate() && ii::runtime::Clock::now()<deadline) {
        loop.poll(); std::this_thread::sleep_for(1ms);
    }
    return predicate();
}
std::string snapshot(char value,std::size_t count=8192) {
    RecentCanvas canvas; canvas.document=std::string(count,value);
    auto encoded=encode_canvas(canvas); check(bool(encoded),"cannot encode fixture");
    return std::move(*encoded);
}
void run(const std::string& mode) {
    if(mode=="invalid_limits") {
        ii::runtime::EventLoop loop; bool zero=false, excessive=false;
        try { CanvasSession invalid(loop,uuid(),0); } catch(const std::invalid_argument&) { zero=true; }
        try { CanvasSession invalid(loop,uuid(),CanvasSession::default_peer_pending_bytes+1); }
        catch(const std::invalid_argument&) { excessive=true; }
        if(!zero || !excessive) {
            std::cerr << "BACKPRESSURE_INVALID_LIMIT_ACCEPTED zero=" << zero << " excessive=" << excessive << '\n';
            throw std::runtime_error("invalid peer budget was accepted");
        }
        std::cout << "BACKPRESSURE_OK invalid_limits\n"; return;
    }
    const auto initial=snapshot('a'), one=snapshot('b'), two=snapshot('c'), latest=snapshot('d');
    check(initial.size()==one.size() && one.size()==two.size() && two.size()==latest.size(),"fixture sizes differ");
    const auto budget=initial.size()+60+1024;
    ii::runtime::EventLoop loop;
    CanvasSession host(loop,uuid(),budget);
    check(host.host("Host") && host.publish(initial),"host setup failed");
    if(mode=="oversized") {
        const auto prior=host.revision;
        const auto tooLarge=snapshot('x',budget*2);
        if(host.publish(tooLarge)) {
            std::cerr << "BACKPRESSURE_OVERSIZE_ACCEPTED budget=" << budget << " snapshot=" << tooLarge.size() << '\n';
            throw std::runtime_error("undeliverable snapshot was accepted");
        }
        check(host.revision==prior && !host.error.empty(),"rejected snapshot changed revision or lost error");
        host.stop(); std::cout << "BACKPRESSURE_OK oversized\n"; return;
    }
    CanvasSession client(loop,uuid()), client2(loop,uuid());
    std::vector<std::uint64_t> seen, seen2; std::string last, last2;
    client.snapshot_received=[&](std::string bytes,std::uint64_t revision,std::string) {
        seen.push_back(revision); last=std::move(bytes);
    };
    client2.snapshot_received=[&](std::string bytes,std::uint64_t revision,std::string) {
        seen2.push_back(revision); last2=std::move(bytes);
    };
    check(client.join(host.session,"127.0.0.1",host.port(),"Guest"),"client join failed");
    if(mode=="multiple") check(client2.join(host.session,"127.0.0.1",host.port(),"Second"),"second join failed");
    check(wait(loop,[&] { return seen.size()==1 && (mode!="multiple" || seen2.size()==1); }),"initial snapshot missing");
    check(last==initial && (mode!="multiple" || last2==initial),"initial snapshot incorrect");
    check(host.publish(one) && host.publish(two) && host.publish(latest),"host publish rejected");
    check(host.revision==4,"host revision incorrect");
    if(mode=="reconnect") {
        client.stop();
        check(wait(loop,[&] { return host.profiles.empty(); }),"disconnected peer remained");
        check(client.join(host.session,"127.0.0.1",host.port(),"Guest"),"rejoin failed");
        check(wait(loop,[&] { return client.revision==4 && last==latest; }),"latest snapshot missing on rejoin");
    } else if(!wait(loop,[&] { return client.revision==4 && last==latest &&
                                    (mode!="multiple" || (client2.revision==4 && last2==latest)); })) {
        std::cerr << "BACKPRESSURE_LATEST_LOST host=" << host.revision << " client=" << client.revision
                  << " second=" << client2.revision << '\n';
        throw std::runtime_error("latest snapshot was not delivered after backpressure");
    }
    auto increasing=[](const auto& revisions) {
        for(std::size_t i=1;i<revisions.size();++i) if(revisions[i]<=revisions[i-1]) return false;
        return true;
    };
    check(increasing(seen) && (mode!="multiple" || increasing(seen2)),"delivered revisions were not increasing");
    client.stop(); client2.stop(); host.stop(); loop.poll();
    std::cout << "BACKPRESSURE_OK " << mode << '\n';
}
}
int main(int argc,char** argv) {
    try { check(argc==2,"expected test case"); const std::string mode=argv[1];
          check(mode=="latest" || mode=="multiple" || mode=="reconnect" || mode=="oversized" || mode=="invalid_limits","unknown case");
          run(mode); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
