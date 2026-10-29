#include <Engine/CanvasSession.h>
#include <Engine/RecentCanvas.h>
#include <Engine/EditCommand.h>
#include <Engine/NearbyDiscovery.h>
#include <Engine/LicenseService.h>
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
#include <thread>
#include <stdexcept>
#ifndef _WIN32
#include <sys/wait.h>
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
void license()
{
    using namespace iisacc::accounts::native;
    ii::runtime::EventLoop loop;Server server(loop),society(loop);std::string persisted,opened;bool owned=true,foreign=false,tampered=false,society_offline=false;
    const auto now=[] { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); };
    server.response=[&](std::string request) {
        require(request.find("password")==request.npos&&request.find("licenseKey")==request.npos,"App sent legacy credentials");
        const auto body=request.substr(request.find("\r\n\r\n")+4);auto root=parse_json(body);require(bool(root),"Invalid app access document");
        const auto input=object(field(field(*root,"variables"),"input"));const auto operation=text(field(input,"operation"));Object result;
        if(operation=="start") result={{"requestId",Value(std::string(32,'c'))},{"authorizationUrl",Value("https://iisacc.com/Account/Apps/Connect?request="+std::string(32,'c'))},{"expiresAt",Value(now()+300)}};
        else if(operation=="license") throw std::runtime_error("App bypassed Society and queried the license server");
        else result={{"status",Value("connected")},{"account",Value(Object{{"sub",Value("fixture-owner")},{"email",Value("owner@example.invalid")}})},
            {"productId",Value("congregation")},{"deviceId",Value(std::string(64,'a'))},{"sessionId",Value(std::string(32,'b'))},{"expiresAt",Value(now()+3600)},
            {"manager",Value("society")},{"accessToken",Value(std::string(128,'T'))}};
        return json(Value(Object{{"data",Value(Object{{"appAccess",Value(result)}})}}));
    };
    society.response=[&](std::string request) {
        if(society_offline)return std::string("unavailable");
        require(request.find("Authorization:")==request.npos,"App leaked a session credential to loopback");
        auto input=parse_json(request.substr(request.find("\r\n\r\n")+4));require(bool(input),"Invalid Society request");
        const auto timestamp=now();
        const Object payload{{"v",Value(1)},{"iss",Value("https://iisacc.com")},{"aud",Value("society")},
            {"sub",Value(foreign?"wrong-account":"fixture-owner")},{"deviceId",Value(std::string(64,'a'))},
            {"issuedAt",Value(timestamp)},{"expiresAt",Value(timestamp+3600)},
            {"licenses",Value(Array{Value(Object{{"productId",Value("congregation")},{"licensed",Value(owned)},{"expiresAt",Value{}}})})}};
        auto token=license_fixture::sign(json(Value(payload)));if(tampered)token.back()=token.back()=='A'?'B':'A';
        return json(Value(Object{{"allowed",Value(owned)},{"requestId",field(*input,"requestId")},{"licenseToken",Value(token)}}));
    };
    const auto broker=society.endpoint()+"v1/authorize";
    const std::vector<iisacc::licensing::owned::TrustedKey> keys{{"fixture-owned-license",license_fixture::public_key()}};
    SessionPersistence store{.read=[&]{return SecretResult{persisted.empty()?SecretResult::Status::not_found:SecretResult::Status::found,persisted};},
        .write=[&](std::string_view value){persisted=value;return true;},.remove=[&]{persisted.clear();return true;}};
    BrowserSessionOptions options{.product_id="congregation",.origin=server.endpoint().substr(0,server.endpoint().size()-1),
        .device={std::string(64,'a'),"tablet","Fixture iPad","ios","27.0","com.iisacc.congregation.painter","1.0.0"},.allow_loopback=true};
    {
        LicenseService license(loop,options,true,store,broker,keys);license.open_browser=[&](auto url){opened=std::move(url);};
        require(server.requests==0,"License service construction performed network access");license.sign_in();
        until(loop,[&]{return license.licensed;},std::chrono::seconds(6));
        require(!opened.empty()&&license.signed_in&&license.stored&&license.email=="owner@example.invalid","Browser session did not unlock after ownership verification");
        require(persisted.find("licenseKey")==persisted.npos&&persisted.find("password")==persisted.npos,"Legacy credentials persisted");
        owned=false;license.retry();require(!license.licensed,"Recheck retained old authorization");
        until(loop,[&]{return !license.verifying;});require(!license.licensed&&license.code=="license_not_owned","Revocation was not reflected");
        foreign=true;license.retry();until(loop,[&]{return !license.verifying;});require(!license.licensed&&license.code=="invalid_society_response","Another account's license unlocked the app");
        foreign=false;tampered=true;license.retry();until(loop,[&]{return !license.verifying;});require(!license.licensed,"Forged Society decision unlocked the app");
        tampered=false;owned=true;license.retry();until(loop,[&]{return license.licensed;});
        int repeated_unlocks=0;license.validated=[&](bool valid){if(valid)++repeated_unlocks;};
        license.retry();until(loop,[&]{return !license.verifying;});require(license.licensed&&repeated_unlocks==0,"A repeated permission check reset the canvas viewport");
    }
    foreign=false;owned=true;tampered=false;society_offline=true;const auto auth_requests=server.requests;
    LicenseService restored(loop,options,true,store,broker,keys);restored.restore();require(restored.licensed,"Verified Society delegation did not restore locally");
    until(loop,[&]{return !restored.verifying;});require(restored.licensed&&restored.code=="licensed_offline","Suspended Society invalidated an unexpired delegation");
    require(server.requests==auth_requests,"Local restart contacted the account/license server");
    const auto valid_saved=persisted;auto record=parse_json(persisted);auto broken=object(*record);broken["licenseToken"]=Value("tampered");persisted=json(Value(broken));
    LicenseService invalid(loop,options,true,store,broker,keys);invalid.restore();until(loop,[&]{return !invalid.verifying;});require(!invalid.licensed,"A tampered saved delegation unlocked the app");
    persisted=valid_saved;restored.forget();require(persisted.empty()&&!restored.licensed&&restored.access_token().empty(),"Sign-out retained authorization");
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
        license();
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
