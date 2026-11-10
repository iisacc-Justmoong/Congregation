#include <Native/Artwork.h>
#include <Native/SocietyFiles.h>
#include <Native/StartupCanvas.h>
#include <iiFileProviderNative.h>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>

using namespace congregation::native;
namespace fs = std::filesystem;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::string json_path(const fs::path& path) {
    const auto u8=path.u8string();
    std::string result;
    for (const auto raw:u8) {
        const auto c=static_cast<unsigned char>(raw);
        if (c=='\\' || c=='"') result.push_back('\\');
        if (c<32) throw std::runtime_error("fixture path contains a control character");
        result.push_back(static_cast<char>(c));
    }
    return result;
}

int main() try {
    iiFileProvider::native::TemporaryDirectory fixture(fs::temp_directory_path());
    check(fixture.isValid(),"fixture unavailable");
    const auto container=fs::canonical(fixture.path())/"Society.societycontainer";
    const auto files=container/"Files";
    constexpr std::array<std::pair<const char*,const char*>,9> sections{{
        {"asset-library","Asset Library"},{"deleted","Deleted"},{"files","Files"},
        {"forked","Forked"},{"generation-history","Generation History"},{"models","Models"},
        {"photos","Photos"},{"published","Published"},{"thinking-space","Thinking Space"}}};
    for (const auto& [key,name]:sections) iiFileProvider::native::File::createDirectories(container/name);
    iiFileProvider::native::File::createDirectories(files/"Art");
    constexpr auto id="12345678-1234-4234-9234-123456789abc";
    std::string manifest=std::string(R"({"type":"SocietyDrive","schemaVersion":1,"displayName":"Society","identifier":")")+id+R"(","sections":[)";
    for (const auto& [key,name]:sections) {
        if (manifest.back()!='[') manifest+=',';
        manifest+=std::string(R"({"id":")")+key+R"(","name":")"+name+R"(","path":")"+name+R"("})";
    }
    manifest+="]}";
    iiFileProvider::native::File::write(container/".society-drive.json",manifest);
    const auto registry=fixture.path()/"storage.json";
    iiFileProvider::native::File::write(registry,std::string(R"({"schemaVersion":1,"containerId":")")+id+
        R"(","path":")"+json_path(container)+R"("})");
    auto storage=SocietyFiles::from_registry(registry);
    check(bool(storage),"cannot resolve Society Files");
#ifdef _WIN32
    _putenv_s("SOCIETY_CONTAINER_PATH", "");
    _putenv_s("SOCIETY_STORAGE_SETTINGS_PATH", registry.string().c_str());
#else
    unsetenv("SOCIETY_CONTAINER_PATH");
    setenv("SOCIETY_STORAGE_SETTINGS_PATH", registry.c_str(), 1);
#endif
    const auto discovered=SocietyFiles::discover();
    check(discovered && discovered->root()==storage->root(),"selected Society storage was not discovered");
    check(storage->root()==fs::canonical(files),"wrong Society Files root");
    check(!storage->destination(fixture.path()/"outside.iisc"),"save escaped Society Files");
    check(!storage->destination(files/"../escape.iisc"),"save accepted parent traversal");
#ifndef _WIN32
    fs::create_directory_symlink(fixture.path(), files/"redirected");
    check(!storage->destination(files/"redirected/outside.iisc"),"save accepted a redirected folder");
    iiFileProvider::native::File::write(fixture.path()/"outside.iisc","outside content");
    fs::create_symlink(fixture.path()/"outside.iisc", files/"Art/redirected.iisc");
    check(!storage->destination(files/"Art/redirected.iisc"),"save accepted a redirected file");
#endif
    const auto group=fs::canonical(fixture.path());
    auto shared=SocietyFiles::from_group(group);
    check(shared && shared->root()==storage->root(),"unregistered shared package was not discovered");
    const auto group_registry=group/"Library/Application Support/iisacc/Society/storage.json";
    iiFileProvider::native::File::createDirectories(group_registry.parent_path());
    iiFileProvider::native::File::write(group_registry,std::string(R"({"schemaVersion":1,"containerId":")")+id+
        R"(","storageId":")"+id+R"(","storageKind":"package","ownerApplication":"com.iisacc.society","managingApplication":"com.iisacc.society","path":"Society.societycontainer"})");
    shared=SocietyFiles::from_group(group);
    check(shared && shared->root()==storage->root(),"registered shared storage was not discovered");
#ifndef _WIN32
    setenv("HOME",group.c_str(),1);
    unsetenv("XDG_CONFIG_HOME");
    setenv("SOCIETY_STORAGE_SETTINGS_PATH","",1);
    setenv("IISACC_SHARED_STORAGE_ROOT",group.c_str(),1);
    const auto discovered_group=SocietyFiles::discover();
    check(discovered_group && discovered_group->root()==storage->root(),"shared group fallback was not discovered");
#endif
    {
        const auto mobile_root=group/"mobile-app";
        iiFileProvider::native::File::createDirectories(mobile_root);
        const auto mobile=SocietyFiles::from_mobile(mobile_root);
        check(mobile && mobile->root()==mobile_root/"Society.societycontainer"/"Files",
              "an app without Society did not receive sandbox storage");
        fs::path working;
        {
            Artwork artwork(group/"mobile-recovery");
            const auto display=startup_canvas_definition({160,90});
            check(display && artwork.create_in_society(*display,*mobile),"mobile display-sized canvas creation failed");
            check(artwork.document().extent==iiSharedCanvas::CanvasExtent{160,90},"mobile canvas lost display dimensions");
            working=artwork.recovery();
            iiSharedCanvas::BitmapBrush brush;brush.size=8;brush.argb=0xff123456;
            check(artwork.begin({16,16},1,brush) && artwork.end({80,16},1),"mobile canvas could not be painted");
        }
        check(fs::is_regular_file(working),"painted mobile working file was lost");
        const auto connected=SocietyFiles::from_mobile(mobile_root,group,true);
        check(connected && connected->root()==storage->root(),"Society connection did not select shared Files");
        check(fs::is_regular_file(working) && fs::is_regular_file(storage->root()/working.filename()),
              "mobile canvas migration did not preserve source and copy the canvas");
    }
    iiFileProvider::native::File::write(group_registry,R"({"schemaVersion":1,"containerId":"bad","storageId":"bad","storageKind":"package","path":"../outside"})");
    check(!shared->available() && !SocietyFiles::from_group(group),"changed or escaping shared registration was accepted");

    CanvasDefinition definition;definition.width=definition.pixel_width=128;
    definition.height=definition.pixel_height=64;definition.background="White";
    const auto recover=fixture.path()/"recovery";
    {
        Artwork artwork(recover);
        check(artwork.create_startup_placeholder(),"startup placeholder failed");
        const auto placeholder=artwork.recovery();
        auto display=startup_canvas_definition({160,90});
        check(display && artwork.create_in_society(*display,*storage),"display-sized Society canvas failed");
        check(artwork.document().extent==iiSharedCanvas::CanvasExtent{160,90},"startup canvas lost display size");
        check(!fs::exists(placeholder),"startup placeholder was not retired");
    }
    {
        fs::path placeholder;
        {
            Artwork artwork(recover);
            check(artwork.create_startup_placeholder(),"unavailable-display placeholder failed");
            placeholder=artwork.recovery();
        }
        check(!fs::exists(placeholder),"closed startup placeholder was not retired");
    }
#ifndef _WIN32
    {
        Artwork artwork(recover);
        check(artwork.create_startup_placeholder(),"placeholder replacement fixture failed");
        const auto placeholder=artwork.recovery();
        fs::rename(placeholder,placeholder.string()+".original");
        iiFileProvider::native::File::create(placeholder,"another user's file");
        auto display=startup_canvas_definition({160,90});
        check(display && artwork.create_in_society(*display,*storage),"replacement canvas failed");
        check(iiFileProvider::native::File::read(placeholder)=="another user's file",
              "replaced startup placeholder was deleted");
    }
#endif
    fs::path blank;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"blank creation failed");
        blank=artwork.recovery();
        check(blank.parent_path()==storage->root() && fs::is_regular_file(blank) &&
              artwork.source().empty(),"new canvas is not an unsaved Society file");
    }
    check(!fs::exists(blank),"unchanged empty temporary canvas survived close");

    fs::path previous;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"first creation failed");
        previous=artwork.recovery();
        auto invalid=definition; invalid.pixel_width=0;
        check(!artwork.create_in_society(invalid,*storage) && fs::exists(previous) &&
              artwork.recovery()==previous,"failed creation lost the original canvas");
        check(artwork.create_in_society(definition,*storage),"replacement creation failed");
        check(!fs::exists(previous),"empty canvas survived replacement");
    }

    fs::path painted;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"painted creation failed");
        painted=artwork.recovery();
        iiSharedCanvas::BitmapBrush brush;brush.size=8;brush.argb=0xff123456;
        check(artwork.begin({16,16},1,brush) && artwork.end({80,16},1),"paint failed");
    }
    check(fs::is_regular_file(painted),"nonempty temporary canvas was deleted");
    {
        iiSharedCanvas::Document saved;
        check(iiSharedCanvas::DocumentFile::readSnapshot(path_text(painted),saved).ok(),"painted temporary canvas could not be reopened");
        const auto has_pixels=std::ranges::any_of(saved.assets,[](const auto& asset) {
            const auto* raster=std::get_if<iiSharedCanvas::RasterAsset>(&asset);
            return raster && std::ranges::any_of(raster->pixels.pixels,[](auto pixel){return (pixel>>24)!=0;});
        });
        check(has_pixels,"painted temporary canvas lost its pixels");
    }

    fs::path in_progress;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"in-progress fixture failed");
        in_progress=artwork.recovery();
        iiSharedCanvas::BitmapBrush brush;brush.size=8;brush.argb=0xff123456;
        check(artwork.begin({16,16},1,brush),"starting an in-progress stroke failed");
        artwork.close_temporary();
    }
    check(fs::exists(in_progress),"an in-progress canvas was deleted on close");

    fs::path undone;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"undo creation failed");
        undone=artwork.recovery();
        iiSharedCanvas::BitmapBrush brush;brush.size=8;brush.argb=0xff123456;
        check(artwork.begin({16,16},1,brush) && artwork.end({80,16},1) && artwork.undo(),"undo failed");
    }
    check(!fs::exists(undone),"empty canvas after undo survived close");

    const auto chosen=files/"Art/Finished.iisc";
    fs::path saved_working;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"save creation failed");
        saved_working=artwork.recovery();
        check(!artwork.save_in_society(fixture.path()/"outside.iisc"),"outside save succeeded");
        check(artwork.save_in_society(chosen),"Society save failed");
    }
    check(fs::is_regular_file(chosen),"explicitly saved canvas was deleted");
    iiSharedCanvas::Document reopened;
    check(iiSharedCanvas::DocumentFile::readSnapshot(path_text(chosen),reopened).ok(),"saved canvas could not be reopened");
    check(!fs::exists(saved_working),"verified saved canvas left a duplicate working file");

    fs::path changed_after_save;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"save edit fixture failed");
        changed_after_save=artwork.recovery();
        check(artwork.save_in_society(files/"Art/Before edit.iisc"),"initial save failed");
        iiSharedCanvas::BitmapBrush brush;brush.size=8;brush.argb=0xff123456;
        check(artwork.begin({16,16},1,brush) && artwork.end({80,16},1),"post-save paint failed");
    }
    check(fs::exists(changed_after_save),"post-save edits were discarded");

    fs::path edited_elsewhere;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"external edit fixture failed");
        edited_elsewhere=artwork.recovery();
        iiSharedCanvas::DocumentFile external;
        check(external.open(path_text(edited_elsewhere)).ok(),"external editor could not open the file");
        check(external.edit([](auto& document){
            iiSharedCanvas::layerProperties(document.layers.back()).name="External edit";
            return true;
        }).ok(),"external editor could not modify the file");
        external.close();
    }
    check(fs::exists(edited_elsewhere),"an externally edited temporary canvas was deleted");

#ifndef _WIN32
    fs::path replaced;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"replacement fixture failed");
        replaced=artwork.recovery();
        fs::rename(replaced,replaced.string()+".original");
        iiFileProvider::native::File::create(replaced,"another user's file");
    }
    check(iiFileProvider::native::File::read(replaced)=="another user's file",
          "a replacement file was deleted");
#endif
    fs::path unverified;
    {
        Artwork artwork(recover);
        check(artwork.create_in_society(definition,*storage),"registry fixture failed");
        unverified=artwork.recovery();
        iiFileProvider::native::File::write(registry,R"({"schemaVersion":1,"containerId":"different","path":"/unknown"})");
        check(!storage->available(),"changed storage selection was accepted");
    }
    check(fs::exists(unverified),"canvas was deleted after its Society selection changed");
    return 0;
} catch(const std::exception& error) { std::fprintf(stderr,"%s\n",error.what());return 1; }
