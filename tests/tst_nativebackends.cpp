#include <Engine/CanvasSession.h>
#include <Engine/RecentCanvas.h>
#include <Engine/EditCommand.h>
#include <Engine/NearbyDiscovery.h>
#include <Engine/LicenseService.h>
#include <Engine/WebLicenseService.h>
#include <Engine/SocietyUpdateFeed.h>
#include <iiAccountManager/LocalChannel.h>
#include "LicenseTokenFixture.h"
#include <Engine/Preferences.h>
#include <Engine/DocumentMetadata.h>
#include <iiUpdateManager/NativeUpdateManager.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderCompatibility.h>
#include <iiFileProviderPresentation.h>
#include <iiSharedCanvasNative.h>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <stdexcept>
#ifndef _WIN32
#include <sys/wait.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#endif
using namespace congregation;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F>
void until(ii::runtime::EventLoop& loop, F predicate,
           std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
    auto end = ii::runtime::Clock::now() + timeout;
    while (!predicate() && ii::runtime::Clock::now() < end)
    {
        loop.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(predicate(), "Core callback timed out");
}
struct Server final
{
    ii::network::Listener listener;
    std::vector<std::unique_ptr<ii::network::Tcp>> clients;
    std::size_t requests = 0;
    std::function<std::string(std::string)> response;
    explicit Server(ii::runtime::EventLoop& loop) : listener(loop)
    {
        listener.accepted = [this](auto socket)
        {
            auto* raw = socket.get();
            auto request = std::make_shared<std::string>();
            raw->readable = [this, raw, request]
            {
                *request += raw->read();
                if (request->find("\r\n\r\n") == request->npos)
                    return;
                auto marker = request->find("Content-Length:");
                std::size_t length = 0;
                if (marker != request->npos)
                    length = std::stoul(request->substr(marker + 15));
                auto body = request->find("\r\n\r\n") + 4;
                if (request->size() < body + length)
                    return;
                ++requests;
                auto content = response ? response(*request) : std::string{};
                raw->readable = {};
                require(raw->write("HTTP/1.1 200 OK\r\nContent-Type: "
                                   "application/json\r\nConnection: close\r\nContent-Length: " +
                                   std::to_string(content.size()) + "\r\n\r\n" + content),
                        "HTTP fixture write rejected");
                raw->close(true);
            };
            clients.push_back(std::move(socket));
        };
        require(listener.listen(), "Native HTTP fixture did not listen");
    }
    std::string endpoint(std::string suffix = "")
    {
        return "http://127.0.0.1:" + std::to_string(listener.port()) + "/" + suffix;
    }
};
void web_license()
{
    using namespace ii::network::wire;
    using namespace iisacc::accounts::native;
    ii::runtime::EventLoop loop;
    Server server(loop);
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string token(128, 'T'), device(64, 'a');
    std::string saved, scenario="denied";
    std::mutex mutex;
    int license_requests=0;
    server.response=[&](const std::string& request) {
        auto body=parse_json(request.substr(request.find("\r\n\r\n")+4));
        require(bool(body),"Web license request was not JSON");
        const auto input=object(field(field(*body,"variables"),"input"));
        const auto operation=text(field(input,"operation"));
        Object reply;
        if(operation=="start") {
            reply={{"requestId",Value(std::string(32,'c'))},
                   {"authorizationUrl",Value("https://iisacc.com/Account/Apps/Connect?request="+std::string(32,'c'))},
                   {"expiresAt",Value(now+300)}};
        } else if(operation=="exchange" || operation=="refresh") {
            reply={{"status",Value("connected")},
                   {"account",Value(Object{{"sub",Value("fixture-owner")},{"email",Value("owner@example.invalid")}})},
                   {"productId",Value("congregation")},{"deviceId",Value(device)},
                   {"sessionId",Value(std::string(32,'b'))},{"expiresAt",Value(now+3600)},
                   {"manager",Value("society")},{"accessToken",Value(token)}};
        } else if(operation=="license") {
            ++license_requests;
            require(request.find("Authorization: Bearer "+token)!=request.npos,
                    "Web license request omitted its scoped session");
            require(text(field(input,"productId"))=="congregation","Wrong license product requested");
            reply={{"productId",Value(scenario=="foreign_product"?"vincent":"congregation")},
                   {"subject",Value(scenario=="foreign_account"?"other-owner":"fixture-owner")},
                   {"licensed",Value(scenario!="denied")},{"checkedAt",Value(now)},
                   {"expiresAt",scenario=="expired"?Value(now-1):Value{}}};
            if(scenario=="invalid_flag")reply.insert_or_assign("licensed",Value("true"));
            if(scenario=="missing_expiry")reply.erase("expiresAt");
        } else throw std::runtime_error("Unexpected web license operation");
        return json(Value(Object{{"data",Value(Object{{"appAccess",Value(std::move(reply))}})}}));
    };
    SessionPersistence store{
        .read=[&]{std::lock_guard lock(mutex);return SecretResult{saved.empty()?SecretResult::Status::not_found:SecretResult::Status::found,saved};},
        .write=[&](std::string_view bytes){std::lock_guard lock(mutex);saved=bytes;return true;},
        .remove=[&]{std::lock_guard lock(mutex);saved.clear();return true;}};
    BrowserSessionOptions options{.product_id="congregation",.origin=server.endpoint().substr(0,server.endpoint().size()-1),
        .device={device,"tablet","Fixture iPad","ios","27.0","com.iisacc.congregation.painter","1.0.2"},.allow_loopback=true};
    WebLicenseService license(loop,options,store);
    bool opened=false;
    license.open_browser=[&](const auto& url){opened=url.starts_with("https://iisacc.com/Account/Apps/Connect?");};
    license.sign_in();
    until(loop,[&]{return license_requests>=1 && !license.verifying;},std::chrono::seconds(8));
    require(opened&&license.signed_in&&!license.licensed&&license.code=="license_not_owned",
            "A web login or denied license granted access");
    for(const auto* invalid:{"foreign_product","foreign_account","invalid_flag","expired","missing_expiry"}) {
        scenario=invalid;const auto previous=license_requests;license.retry();
        until(loop,[&]{return license_requests>previous&&!license.verifying;},std::chrono::seconds(8));
        require(!license.licensed&&license.code=="invalid_license_response","Invalid web license response granted access");
    }
    scenario="allowed";const auto previous=license_requests;license.retry();
    until(loop,[&]{return license_requests>previous&&!license.verifying;},std::chrono::seconds(8));
    require(license.licensed&&license.code=="licensed","Verified web license was not accepted");
    until(loop,[&]{std::lock_guard lock(mutex);return !saved.empty();},std::chrono::seconds(8));
    WebLicenseService restored(loop,options,store);
    bool reopened=false;restored.open_browser=[&](const auto&){reopened=true;};
    const auto before_restore=license_requests;
    restored.restore();
    require(!restored.licensed,"Stored web session granted access without a server check");
    until(loop,[&]{return license_requests>before_restore&&!restored.verifying;},std::chrono::seconds(8));
    require(restored.licensed&&!reopened,"Stored web session did not recheck the server");
}
void society_update_feed(const std::filesystem::path& root)
{
#if defined(__unix__) || defined(__APPLE__)
    using namespace ii::network::wire;
    using namespace iisacc::accounts::local;
    auto pattern=(root.parent_path()/"society-u-XXXXXX").string();
    auto* created=::mkdtemp(pattern.data()); require(created!=nullptr,"Cannot create update fixture");
    const std::filesystem::path directory(created);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path,ec); } } cleanup{directory};
    auto listener=Listener::listen(directory/"broker.sock"); require(bool(listener),"Cannot open update fixture");
    ii::runtime::EventLoop loop; SocietyUpdateFeed feed(loop,std::string(64,'a'),directory/"broker.sock");
    std::string error;
    std::thread server([&] {
        pollfd descriptor{listener->native_handle(),POLLIN,0};
        if(::poll(&descriptor,1,5000)<=0){error="no update request";return;}
        auto peer=listener->accept();if(!peer){error="accept failed";return;}
        auto bytes=peer->receive(std::chrono::seconds(3)); auto input=bytes?parse_json(*bytes):std::nullopt;
        if(!input || text(field(*input,"operation"))!="update" || text(field(*input,"action"))!="check" ||
           text(field(*input,"productId"))!="congregation" || text(field(*input,"currentVersion"))!="1.0.0" ||
           !text(field(*input,"accessToken")).empty()) {error="bad update request";return;}
        auto body=json(Value(Object{{"requestId",field(*input,"requestId")},
            {"result",Value(Object{{"status",Value("release_unavailable")}})}}));
        if(!peer->send(body,std::chrono::seconds(3)))error="send failed";
    });
    bool done=false,valid=false;
    feed.check("congregation","macos-arm64","1.0.0",[&](auto result,auto){
        done=true;if(result){auto parsed=parse_json(*result);valid=parsed && text(field(*parsed,"status"))=="release_unavailable";}
    });
    auto deadline=ii::runtime::Clock::now()+std::chrono::seconds(6);
    while(!done&&ii::runtime::Clock::now()<deadline){loop.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    server.join();
    require(error.empty()&&done&&valid,"Society update did not use the local channel");
#else
    (void)root;
#endif
}

void container_and_commands()
{
    require(percent_decode("file%20name.png") == std::optional<std::string>("file name.png") &&
                !percent_decode("%z1"),
            "Native percent decoding failed");
    RecentCanvas canvas;
    canvas.document = "native-iisc";
    canvas.objects.emplace_back(Object{{"type", Value("text")}, {"text", Value("한글 😀")}});
    canvas.assets.push_back({1, 1, "png-bytes"});
    auto encoded = encode_canvas(canvas);
    require(encoded.has_value(), "Native container encoding failed");
    auto decoded = decode_canvas(*encoded);
    require(decoded && decoded->document == canvas.document && decoded->objects == canvas.objects &&
                decoded->assets.size() == 1,
            "Container roundtrip failed");
    auto damaged = *encoded;
    damaged.back() ^= 1;
    require(!decode_canvas(damaged), "Checksum mismatch admitted");
    require(!decode_canvas(*encoded + "x"), "Trailing container data admitted");
    canvas.assets.push_back(canvas.assets.front());
    require(!encode_canvas(canvas), "Duplicate asset admitted");
    require(!parse_json(R"({"a":1,"a":2})"), "Duplicate JSON field admitted");
    Object fill{{"type", Value("fill")},
                {"layerKey", Value("raster-canvas")},
                {"x", Value(24.0)},
                {"y", Value(32.0)},
                {"color", Value("#336699")}};
    auto command = encode_command(fill);
    require(!command.empty(), "CBOR command encoding failed");
    auto restored = decode_command(command);
    require(restored && *restored == fill, "CBOR command roundtrip failed");
    require(!decode_command(command + std::string(1, '\0')), "Trailing CBOR data admitted");
    fill.emplace("unexpected", Value(1));
    require(encode_command(fill).empty(), "Unknown command field admitted");
    fill.erase("unexpected");
    fill.insert_or_assign("color", Value("#１２３"));
    require(encode_command(fill).empty(), "Non-ASCII color admitted");
    Object image{{"type", Value("open-raster")},
                 {"data", Value(ii::core::Bytes{std::byte{1}, std::byte{2}})}};
    auto binary = decode_command(encode_command(image));
    require(binary && *binary == image, "CBOR binary identity lost");
    std::string korean;
    for (int i = 0; i < 256; ++i)
        korean += "한";
    Object rename{{"type", Value("rename-layer")},
                  {"layerKey", Value("raster-canvas")},
                  {"name", Value(korean)}};
    require(!encode_command(rename).empty(), "UTF-16 name limit narrowed to UTF-8 byte count");
    rename.insert_or_assign("name", Value(korean + "글"));
    require(encode_command(rename).empty(), "Overlong Unicode layer name admitted");
    std::string emoji;
    for (int i = 0; i < 128; ++i)
        emoji += "😀";
    rename.insert_or_assign("name", Value(emoji));
    require(!encode_command(rename).empty(), "Astral layer name boundary changed");
    rename.insert_or_assign("name", Value(emoji + "😀"));
    require(encode_command(rename).empty(), "UTF-16 surrogate-pair limit ignored");
    auto layer = psd_layer(Object{{"type", Value("shape")},
                                  {"x", Value(-100.0)},
                                  {"y", Value(9999.0)},
                                  {"width", Value(1e100)},
                                  {"height", Value(5.0)},
                                  {"opacity", Value(2.0)}},
                           320, 240, 0);
    require(number(field(layer, "left")) == 0 && number(field(layer, "top")) == 239 &&
                number(field(layer, "right")) == 320,
            "PSD bounds are not clamped");
    require(xmp_text("<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><x:Data> value </x:Data></x:xmpmeta>",
                     "Data") == "value",
            "Native XMP namespace handling failed");
}
void preferences(const std::filesystem::path& root)
{
    auto path = root / "preferences.ini", storage = root / "canvas";
    iiFileProvider::native::File::write(
        path, "[%General]\nstartWithRecentCanvas=true\ndiscoverNearbyCongregationUsers="
              "false\nrecentCanvasUrl=file:///external.iisc\n");
    Preferences p(path, storage);
    require(p.start_recent && !p.discover_nearby, "Legacy INI migration failed");
    p.set_start_recent(false);
    p.set_discover_nearby(true);
    Preferences reload(path, storage);
    require(!reload.start_recent && reload.discover_nearby, "Native choices not persisted");
    auto file = p.storage_path();
    iiFileProvider::native::File::write(file, "canvas");
    require(p.record_canvas(file), "Internal canvas rejected");
    require(!p.record_canvas(root / "external.vrc"), "External recent canvas admitted");
    auto lock = std::filesystem::path(file.string() + ".iisacc-lock");
    iiFileProvider::native::File::write(lock, "locked");
    require(!p.clear_canvas(), "Competing writer ignored");
    (void) iiFileProvider::native::File::remove(lock);
    require(p.clear_canvas(), "Native canvas removal failed");
#ifndef _WIN32
    auto external = root / "external.vrc";
    iiFileProvider::native::File::write(external, "external");
    std::filesystem::create_symlink(external, file);
    require(p.existing_canvas().empty(), "Symlink recent canvas admitted");
    std::filesystem::remove(file);
#endif
    auto recovery=storage/(uuid()+".iisc");iiFileProvider::native::File::create(recovery,"canonical");
    require(p.record_recovery(recovery)&&p.existing_canvas()==recovery,"Native recovery was not recorded");
    require(!p.record_recovery(root/(uuid()+".iisc")),"External native recovery admitted");
    p.set_invitations_allowed(true);
    require(Preferences(path,storage).invitations_allowed,"Invitation preference was not persisted");
    p.set_profile_name("  한글 Artist\n");Preferences restored(path,storage);
    require(restored.profile_name=="한글 Artist"&&restored.existing_canvas()==recovery,"Profile or native recovery was not restored");
}
void file_storage(const std::filesystem::path& root)
{
    using namespace iiFileProvider::native;
    const auto path = root / "writer-document.iisc";
    auto guard_path = path;
    guard_path += ".iisacc-lock";
    writeCompatible(path, "original");
    std::string ownership;
    {
        FileWriteLock owner(path);
        ownership = File::read(guard_path, 4096);
        bool rejected = false;
        try
        {
            writeCompatible(path, "competing");
        }
        catch (const FileError&)
        {
            rejected = true;
        }
        require(rejected && File::read(path) == "original",
                "Competing native writer changed the document");
    }
    require(!std::filesystem::exists(guard_path), "Native writer guard leaked");
    writeCompatible(path, "replacement");
    require(File::read(path) == "replacement", "Released writer did not admit replacement");
#ifndef _WIN32
    const auto child = ::fork();
    require(child >= 0, "Stale-owner fixture could not fork");
    if (child == 0)
        ::_exit(0);
    int status = 0;
    require(::waitpid(child, &status, 0) == child && WIFEXITED(status),
            "Stale-owner fixture did not exit");
    ownership.replace(0, ownership.find('\n'), std::to_string(child));
    File::write(guard_path, ownership);
    writeCompatible(path, "recovered");
    require(File::read(path) == "recovered" && !std::filesystem::exists(guard_path),
            "Dead local owner was not safely reclaimed");
#endif
}
void session()
{
    ii::runtime::EventLoop loop;
    CanvasSession host(loop, uuid()), client(loop, uuid());
    require(host.host("Host Artist"), "Host could not listen");
    RecentCanvas canvas;
    canvas.document = "iisc";
    auto snapshot = encode_canvas(canvas);
    require(snapshot && host.publish(*snapshot), "Host snapshot rejected");
    std::size_t updates = 0, edits = 0;
    client.snapshot_received = [&](auto bytes, auto revision, auto)
    {
        require(bytes == *snapshot && revision == 1, "Native snapshot identity lost");
        ++updates;
    };
    host.edit_received = [&](auto command, auto peer)
    {
        require(text(field(command, "type")) == "fill" && peer == client.peer_id,
                "Host command identity lost");
        ++edits;
    };
    require(client.join(host.session, "127.0.0.1", host.port(), "Guest"), "Native join rejected");
    until(loop,
          [&] { return updates == 1 && host.profiles.size() == 1 && client.profiles.size() == 1; });
    Object command{{"type", Value("fill")},
                   {"layerKey", Value("raster-canvas")},
                   {"x", Value(1.0)},
                   {"y", Value(2.0)},
                   {"color", Value("#123")}};
    require(client.submit(command), "Client edit rejected");
    until(loop, [&] { return edits == 1; });
    require(host.revision == 1 && !client.publish(*snapshot), "Client obtained snapshot authority");
    require(host.remove(client.peer_id), "Host removal rejected");
    until(loop,
          [&] { return client.state == CanvasSession::State::idle && host.profiles.empty(); });
    require(client.error.find("removed") != std::string::npos, "Host removal reason lost");
    require(!client.join(uuid(), "203.0.113.1", 1234, "Guest"), "Nonlocal endpoint admitted");
    host.stop();
    loop.poll();
}
void native_session()
{
    ii::runtime::EventLoop loop;CanvasSession host(loop,uuid()),client(loop,uuid());
    iiSharedCanvas::Document document;document.extent={8,8};document.assets.emplace_back(iiSharedCanvas::RasterAsset{"paint",makeRasterLayer(8,8,0)});document.layers.emplace_back(iiSharedCanvas::StaticBitmapLayer{{"paint-layer","Paint"},iiSharedCanvas::StaticSource{"paint"}});
    auto encoded=iiSharedCanvas::encodeIisc(document);require(encoded.ok(),"Cannot encode the native session fixture");const std::string snapshot(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());
    require(host.host("Host")&&host.publish(snapshot),"Native host snapshot rejected");int received=0,edits=0;
    client.snapshot_received=[&](auto bytes,auto revision,auto){require(bytes==snapshot&&revision==std::uint64_t(received+1),"Native session changed canonical bytes or revision");++received;};
    host.edit_received=[&](auto command,auto peer){require(text(field(command,"type"))=="nativeDocument"&&peer==client.peer_id,"Native edit identity lost");++edits;require(host.publish(snapshot),"Host could not publish the durable edit");};
    require(client.join(host.session,"127.0.0.1",host.port(),"Guest"),"Native session join rejected");until(loop,[&]{return received==1;});
    Object command{{"type",Value("nativeDocument")},{"document",Value(base64(snapshot))},{"baseRevision",Value(std::uint64_t(1))}};
    require(client.submit(command),"Canonical edit admission failed");until(loop,[&]{return edits==1&&received==2;});
    require(client.submit(command),"Stale edit could not reach host validation");until(loop,[&]{return client.state==CanvasSession::State::idle;});
    require(edits==1&&host.revision==2&&client.error.find("changed")!=std::string::npos,"Stale canonical edit overwrote a newer document");
    command["document"]=Value(base64("invalid"));require(!normalize_command(command),"Malformed canonical document accepted");host.stop();loop.poll();
}
void protocol()
{
    auto id = uuid();
    auto bytes = encode_presence({id, true, 32123, true});
    auto p = decode_presence(bytes);
    require(p && p->session == id && p->port == 32123 && p->invitations,
            "Presence protocol roundtrip failed");
    auto tree = parse_json(bytes);
    require(tree && object(*tree).size() == 6, "Presence leaked profile fields");
    auto i = Invitation{uuid(), uuid(), uuid(), 32123, "Artist"};
    auto invitation = decode_invitation(encode_invitation(i));
    require(invitation && invitation->id == i.id, "Invitation protocol failed");
    require(!decode_presence(encode_invitation(i)), "Invitation mistaken for anonymous presence");
}
void http_deadlines()
{
    ii::runtime::EventLoop loop;
    ii::network::Listener listener(loop);
    std::vector<std::unique_ptr<ii::network::Tcp>> peers;
    bool partial = false;
    listener.accepted = [&](auto socket)
    {
        auto* raw = socket.get();
        raw->readable = [&, raw]
        {
            (void) raw->read();
            if (partial)
                require(
                    raw->write(
                        "HTTP/1.1 200 OK\r\nContent-Length: 20\r\nConnection: close\r\n\r\npart"),
                    "Partial fixture send failed");
            raw->readable = {};
        };
        peers.push_back(std::move(socket));
    };
    require(listener.listen(), "Deadline fixture did not listen");
    ii::network::Http http(loop);
    bool done = false;
    ii::network::HttpRequest request;
    request.url = "http://127.0.0.1:" + std::to_string(listener.port()) + "/";
    request.timeout = std::chrono::seconds(1);
    request.initial_response_timeout = std::chrono::milliseconds(30);
    require(http.start(request,
                       [&](auto response)
                       {
                           require(response.timed_out && response.body.empty(),
                                   "Initial response timeout was not enforced");
                           done = true;
                       }),
            "Initial deadline request rejected");
    until(loop, [&] { return done; });
    done = false;
    partial = true;
    request.initial_response_timeout = std::chrono::milliseconds(300);
    request.inactivity_timeout = std::chrono::milliseconds(30);
    require(http.start(request,
                       [&](auto response)
                       {
                           require(response.timed_out && response.body == "part",
                                   "Read inactivity timeout was not enforced");
                           done = true;
                       }),
            "Inactivity request rejected");
    until(loop, [&] { return done; });
    ii::runtime::EventLoop saturated(1);
    auto occupied = saturated.post([] {});
    require(static_cast<bool>(occupied), "Saturation fixture failed");
    ii::network::Http rejected(saturated);
    require(!rejected.start(request, [](auto) {}), "HTTP admitted without a Core dispatch slot");
    ii::network::Listener rejected_listener(saturated);
    require(!rejected_listener.listen(), "Listener admitted without a Core dispatch slot");
}
void license(const std::filesystem::path& root)
{
    {
        ii::runtime::EventLoop loop;
        iisacc::accounts::native::SocietySessionOptions invalid;
        LicenseService unavailable(loop,std::move(invalid),true);
        require(!unavailable.licensed && unavailable.code=="society_unavailable" && !unavailable.workspace_visible(),
                "Unavailable local transport did not fail closed");
        unavailable.restore(); unavailable.retry(); unavailable.cancel();
    }
    {
        using namespace iisacc::accounts::native;
        ii::runtime::EventLoop loop;
        SocietySessionOptions options{.product_id="congregation",
            .device={std::string(64,'a'),"tablet","Fixture","ios","1","com.iisacc.congregation.painter","1"}};
        SessionPersistence store{.read=[] { return SecretResult{SecretResult::Status::not_found,{}}; }};
        LicenseService web(loop,std::move(options),true,LicenseService::Authority::web,std::move(store));
        require(web.workspace_visible() && !web.licensed, "Initial web check was not pending");
        web.restore();
        until(loop,[&] { return !web.workspace_visible(); },std::chrono::seconds(8));
        require(!web.licensed && !web.verifying,
                "Missing saved session must finish verification without granting access");
    }
#if defined(__unix__) || defined(__APPLE__)
    using namespace iisacc::accounts::native;
    using namespace iisacc::accounts::local;
    using namespace ii::network::wire;
    auto pattern = (root.parent_path() / "society-l-XXXXXX").string();
    auto* created = ::mkdtemp(pattern.data());
    require(created != nullptr, "Cannot create local license fixture");
    const std::filesystem::path directory(created);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{directory};
    auto listener = Listener::listen(directory / "broker.sock");
    require(bool(listener), "Cannot open local license fixture");
    ii::runtime::EventLoop loop;
    SocietySessionOptions options{.product_id="congregation",
        .device={std::string(64,'a'),"pc","Fixture","macos","1","com.iisacc.congregation.painter","1"},
        .socket_path=directory / "broker.sock",.timeout=std::chrono::seconds(2),
        .keys={{"fixture-owned-license",license_fixture::public_key()}}};
    LicenseService license(loop,options,true);
    require(!license.licensed && license.workspace_visible(),
            "Pending license verification must show the workspace without granting access");
    bool opened=false; license.open_society_action=[&]{opened=true;};
    auto run=[&](bool owned, bool foreign, bool open) {
        std::string error;
        std::thread server([&] {
            pollfd descriptor{listener->native_handle(), POLLIN, 0};
            if (::poll(&descriptor, 1, 5000) <= 0) { error="no local request"; return; }
            auto peer=listener->accept(); if(!peer) { error="accept failed"; return; }
            auto bytes=peer->receive(std::chrono::seconds(3));
            auto input=bytes ? parse_json(*bytes) : std::nullopt;
            if(!input || text(field(*input,"productId"))!="congregation" ||
               text(field(*input,"appId"))!="com.iisacc.congregation.painter" ||
               !text(field(*input,"accessToken")).empty()) { error="bad local request"; return; }
            const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            Object payload{{"v",Value(1)},{"iss",Value("https://iisacc.com")},{"aud",Value("society")},
                {"sub",Value("fixture-owner")},{"deviceId",Value(std::string(64,foreign?'b':'a'))},
                {"issuedAt",Value(now)},{"expiresAt",Value(now+2)},
                {"licenses",Value(Array{Value(Object{{"productId",Value("congregation")},
                    {"licensed",Value(owned)},{"expiresAt",Value{}}})})}};
            const auto response=json(Value(Object{{"requestId",field(*input,"requestId")},{"allowed",Value(owned)},
                {"licenseToken",Value(license_fixture::sign(json(Value(payload))))}}));
            if(!peer->send(response,std::chrono::seconds(3))) error="send failed";
        });
        if(open) license.sign_in(); else license.retry();
        const auto deadline=ii::runtime::Clock::now()+std::chrono::seconds(6);
        while(license.verifying && ii::runtime::Clock::now()<deadline) { loop.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        server.join();
        require(error.empty(),"local license fixture failed");
        require(!license.verifying,"local license timed out");
    };
    run(false,false,true);
    require(opened,"Society account action was not forwarded");
    require(license.signed_in && !license.licensed && license.code=="license_not_owned","signed denial was ignored");
    require(!license.workspace_visible(), "A completed denial must show the license screen");
    run(true,true,false);
    require(!license.licensed && !license.signed_in,"foreign-device token was accepted");
    run(true,false,false);
    require(license.licensed && license.workspace_visible(),"valid local authorization did not recover");
    int repeated=0; license.validated=[&](bool valid){if(valid)++repeated;};
    license.retry();
    const auto afterExpiry=ii::runtime::Clock::now()+std::chrono::milliseconds(2500);
    while(ii::runtime::Clock::now()<afterExpiry) { loop.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    require(license.licensed && repeated==0,"the launch grant was revoked during the current run");
    license.cancel(); require(license.licensed,"cancelling a later check revoked the launch grant");
#else
    (void)root; // Native Society local channel is not yet available on Windows.
#endif
}

struct Provider final : iisacc::updates::native::UpdateCredentialProvider
{
    Completion pending;
    std::size_t reads = 0;
    void requestCredentials(Completion done) override
    {
        ++reads;
        pending = std::move(done);
    }
    void cancel() noexcept override { pending = {}; }
};
void updates()
{
    using Manager = iisacc::updates::native::UpdateManager;
    ii::runtime::EventLoop loop;
    Server server(loop);
    server.response = [](auto)
    {
        return json(Value(Object{
            {"schemaVersion", Value(1)},
            {"productId", Value("congregation")},
            {"releases",
             Value(Object{
                 {iisacc::updates::native::platform_key(),
                  Value(Object{{"version", Value("1.1.0")},
                               {"updateUrl",
                                Value("https://iisacc.com/Store/Congregation/Download")}})}})}}));
    };
    Provider provider;
    Manager update(loop, "congregation", "1.0", server.endpoint("manifest"),
                   server.endpoint("grant"));
    update.setCredentialProvider(&provider);
    require(provider.reads == 0 && server.requests == 0, "Updater construction performed work");
    require(update.checkForUpdates(), "Manual native check rejected");
    until(loop, [&] { return update.state() == Manager::State::UpdateAvailable; });
    require(provider.reads == 0 && server.requests == 1,
            "Check read credentials or duplicated request");
    require(update.updateNow() && provider.reads == 1 &&
                update.state() == Manager::State::Authorizing,
            "Explicit authorization did not read credentials");
    auto late = std::move(provider.pending);
    require(update.cancelUpdate(), "Native update cancellation rejected");
    iisacc::updates::native::UpdateCredentials credentials;
    credentials.email = "verified@example.com";
    credentials.licenseKey = "IIL1_0123456789abcdefghijklmnopqrstuv";
    late(std::optional(std::move(credentials)), {});
    loop.poll();
    require(server.requests == 1, "Cancelled credentials reached network");

    Object grant{{"schemaVersion", Value(1)},
                 {"productId", Value("congregation")},
                 {"platformKey", Value(iisacc::updates::native::platform_key())},
                 {"version", Value("1.1.0")},
                 {"fileName", Value("Congregation.pkg")},
                 {"fileSizeBytes", Value(8)},
                 {"sha256", Value(std::string(64, '0'))},
                 {"signatureStatus", Value("notarized")},
                 {"downloadUrl", Value(server.endpoint("artifact"))},
                 {"expiresAt", Value("2020-01-01T00:00:00Z")}};
#ifdef _WIN32
    grant.insert_or_assign("fileName", Value("Congregation.msi"));
    grant.insert_or_assign("signatureStatus", Value("unsigned"));
#endif
    const auto manifest = server.response;
    Object current_grant = grant;
    std::size_t artifacts = 0;
    server.response = [&](std::string request)
    {
        if (request.starts_with("GET /manifest "))
            return manifest(request);
        if (request.starts_with("GET /artifact "))
            ++artifacts;
        require(request.starts_with("POST /grant ") &&
                    request.find("mutation GrantVincentUpdate") != request.npos,
                "Update grant protocol changed");
        return json(
            Value(Object{{"data", Value(Object{{"grantVincentUpdate", Value(current_grant)}})}}));
    };
    auto reject = [&](Object invalid, Manager::UpdateError expected)
    {
        current_grant = std::move(invalid);
        std::optional<Manager::UpdateError> failure;
        update.updateFailed = [&](auto error, auto) { failure = error; };
        require(update.checkForUpdates(), "Retry check rejected");
        until(loop, [&] { return update.state() == Manager::State::UpdateAvailable; });
        require(update.updateNow(), "Explicit grant request rejected");
        auto complete = std::move(provider.pending);
        iisacc::updates::native::UpdateCredentials accepted;
        accepted.email = "verified@example.com";
        accepted.licenseKey = "IIL1_0123456789abcdefghijklmnopqrstuv";
        complete(std::optional(std::move(accepted)), {});
        until(loop, [&] { return failure.has_value(); });
        require(*failure == expected && update.state() == Manager::State::Failed,
                "Unsafe download grant was not rejected with its reason");
        update.updateFailed = {};
        require(artifacts == 0, "Rejected grant reached the artifact endpoint");
    };
    auto invalid = grant;
    invalid.insert_or_assign("productId", Value("other-product"));
    reject(std::move(invalid), Manager::UpdateError::InvalidGrant);
    invalid = grant;
    invalid.insert_or_assign("fileName", Value("../outside.pkg"));
    reject(std::move(invalid), Manager::UpdateError::InvalidGrant);
#if defined(__APPLE__) || defined(_WIN32)
    invalid = grant;
    invalid.insert_or_assign("downloadUrl", Value("https://untrusted.invalid/artifact"));
    reject(std::move(invalid), Manager::UpdateError::InvalidGrant);
    reject(grant, Manager::UpdateError::GrantExpired);
#endif
    bool scoped_request=false;
    server.response=[&](std::string request) {
        if(request.starts_with("GET /manifest ")) return manifest(request);
        scoped_request=true;
        require(request.find("mutation AppAccess")!=request.npos&&request.find("Authorization: Bearer "+std::string(128,'T'))!=request.npos,
                "Scoped update authorization was not used");
        require(request.find("licenseKey")==request.npos&&request.find("email")==request.npos,"Legacy keys entered a scoped update request");
        return json(Value(Object{{"data",Value(Object{{"appAccess",Value(grant)}})}}));
    };
    std::optional<Manager::UpdateError> scoped_failure;update.updateFailed=[&](auto error,auto){scoped_failure=error;};
    require(update.checkForUpdates(),"Scoped retry check rejected");until(loop,[&]{return update.state()==Manager::State::UpdateAvailable;});
    require(update.updateNow(),"Scoped update rejected");iisacc::updates::native::UpdateCredentials scoped;scoped.accessToken=std::string(128,'T');
    auto finish=std::move(provider.pending);finish(std::optional(std::move(scoped)),{});
    until(loop,[&]{return scoped_failure.has_value();});require(scoped_request&&*scoped_failure==Manager::UpdateError::GrantExpired,"Scoped grant bypassed validation");
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "A build-local fixture directory is required");
        std::filesystem::path root = argv[1];
        iiFileProvider::native::File::createDirectories(root);
        container_and_commands();
        file_storage(root);
        preferences(root);
        protocol();
        session();
        native_session();
        http_deadlines();
        web_license();
        license(root);
        society_update_feed(root);
        updates();
        (void) iiFileProvider::native::Presentation::waitForIdle(std::chrono::seconds(5));
        std::cout << "Qt-free native backend contracts: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
