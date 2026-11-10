#include "SocietyFiles.h"
#include "SocietyAssets.h"
#include <MobileStorage.h>
#include <iiFileProviderNative.h>
#include <algorithm>
#include <array>

namespace congregation::native {
namespace fs=std::filesystem;
namespace {
using namespace ii::network::wire;
fs::path utf8_path(const std::string& value) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
bool small_regular_file(const fs::path& path) {
    std::error_code error;
    const auto status=fs::symlink_status(path,error);
    if (error || !fs::is_regular_file(status) || fs::is_symlink(status)) return false;
    const auto size=fs::file_size(path,error);
    return !error && size>0 && size<=65536;
}
bool direct_directory(const fs::path& value) {
    std::error_code error;
    if(!value.is_absolute() || value.lexically_normal()!=value) return false;
    const auto status=fs::symlink_status(value,error);
    if(error || !fs::is_directory(status) || fs::is_symlink(status)) return false;
    const auto canonical=fs::canonical(value,error);
    return !error && canonical==value;
}
bool manifest_valid(const fs::path& container,const std::string& expected) {
    const auto path=container/".society-drive.json";
    if (!small_regular_file(path)) return false;
    const auto data=read_society_json(path);
    if(!data || text(field(*data,"type"))!="SocietyDrive" || number(field(*data,"schemaVersion"))!=1 ||
       (text(field(*data,"identifier"))!=expected && text(field(*data,"localIdentifier"))!=expected &&
        text(field(*data,"previousIdentifier"))!=expected) ||
       !boolean(field(*data,"replicaReady"),true)) return false;
    const auto display=text(field(*data,"displayName"));
    if (display!="Society" && display!="Society Container") return false;
    const auto& sections=array(field(*data,"sections"));
    constexpr std::array<std::pair<std::string_view,std::string_view>,9> expected_sections{{
        {"asset-library","Asset Library"},{"deleted","Deleted"},{"files","Files"},
        {"forked","Forked"},{"generation-history","Generation History"},{"models","Models"},
        {"photos","Photos"},{"published","Published"},{"thinking-space","Thinking Space"}}};
    const bool legacy=sections.size()==8;
    if (!legacy && sections.size()!=expected_sections.size()) return false;
    std::size_t i=0;
    for (const auto& [key,name]:expected_sections) {
        if (legacy && key=="photos") continue;
        const auto& section=sections[i++];
        if (text(field(section,"id"))!=key || text(field(section,"name"))!=name ||
            text(field(section,"path"))!=name || !direct_directory(container/fs::path(name))) return false;
    }
    return true;
}
bool under(const fs::path& root,const fs::path& value) {
    auto r=root.begin(),v=value.begin();
    for(;r!=root.end();++r,++v) if(v==value.end() || *r!=*v) return false;
    return v!=value.end();
}
}
std::expected<SocietyFiles,std::string> SocietyFiles::from_registry(const fs::path& registry) {
    if(!registry.is_absolute()) return std::unexpected("Society storage settings are unavailable");
    if(!small_regular_file(registry))
        return std::unexpected("Connect Society storage before creating a canvas");
    const auto config=read_society_json(registry);
    if(!config || number(field(*config,"schemaVersion"))!=1)
        return std::unexpected("Society storage settings are invalid");
    const auto id=text(field(*config,"containerId"));
    const auto root_text=text(field(*config,"path"));
    if (root_text.find('\0')!=std::string::npos)
        return std::unexpected("Society storage settings are invalid");
    const fs::path root=utf8_path(root_text);
    const auto files=root/"Files";
    if(id.empty() || !direct_directory(root) || !direct_directory(files) || !manifest_valid(root,id))
        return std::unexpected("The selected Society Files storage is unavailable or has changed");
    return SocietyFiles(root,files,id,registry);
}
bool SocietyFiles::available() const {
    if (!direct_directory(container_) || !direct_directory(files_) || !manifest_valid(container_,id_)) return false;
    if (!group_.empty()) {
        const auto selected=from_group(group_);
        return selected && selected->container_==container_ && selected->id_==id_ &&
               selected->storage_id_==storage_id_ && selected->storage_kind_==storage_kind_ &&
               selected->owner_==owner_;
    }
    if (registry_.empty()) return true;
    if (!small_regular_file(registry_)) return false;
    const auto config=read_society_json(registry_);
    return config && number(field(*config,"schemaVersion"))==1 &&
           text(field(*config,"containerId"))==id_ && utf8_path(text(field(*config,"path")))==container_;
}
std::expected<SocietyFiles,std::string> SocietyFiles::from_group(const fs::path& group) {
    if (!direct_directory(group)) return std::unexpected("The Society shared group is unavailable or redirected");
    const auto registry=group/"Library/Application Support/iisacc/Society/storage.json";
    std::error_code error;
    const auto status=fs::symlink_status(registry,error);
    const bool missing=error==std::errc::no_such_file_or_directory || (!error && !fs::exists(status));
    if (error && !missing) return std::unexpected("Cannot inspect the Society shared registration");
    fs::path root;
    std::string id,storage_id,kind,owner;
    if (missing) {
        root=group/"Society.societycontainer";
        const auto manifest=read_society_json(root/".society-drive.json");
        if (manifest) id=text(field(*manifest,"identifier"));
    } else {
        if (!small_regular_file(registry))
            return std::unexpected("The Society shared registration is redirected");
        const auto record=read_society_json(registry);
        if (!record || number(field(*record,"schemaVersion"))!=1)
            return std::unexpected("The Society shared registration is invalid");
        id=text(field(*record,"containerId"));
        storage_id=text(field(*record,"storageId"));
        kind=text(field(*record,"storageKind"));
        owner=text(field(*record,"ownerApplication"));
        const auto relative_text=text(field(*record,"path"));
        const fs::path relative=utf8_path(relative_text);
        if (relative.empty() || relative.is_absolute() || relative.lexically_normal()!=relative ||
            relative_text.find_first_of("\\:\0",0,3)!=std::string::npos ||
            id.empty() || storage_id.empty() || (kind!="package" && kind!="logical") || owner.empty())
            return std::unexpected("The Society shared registration is invalid");
        for (const auto& part:relative) if (part==".." || part==".")
            return std::unexpected("The Society shared registration escapes its group");
        root=group/relative;
        if (!under(group,root)) return std::unexpected("The Society shared registration escapes its group");
        const auto manifest=read_society_json(root/".society-drive.json");
        const auto local=manifest ? text(field(*manifest,"localIdentifier")) : std::string{};
        const auto identifier=manifest ? text(field(*manifest,"identifier")) : std::string{};
        if (storage_id!=(local.empty()?identifier:local))
            return std::unexpected("The Society shared disk identity has changed");
        if (kind=="package" && root.extension()!=".societycontainer")
            return std::unexpected("The Society shared package is invalid");
        if (kind=="logical") {
            try {
                const auto marker=root/".society-logical-disk";
                const auto marker_status=fs::symlink_status(marker);
                if (!fs::is_regular_file(marker_status) || fs::is_symlink(marker_status) ||
                    iiFileProvider::native::File::read(marker,256)!="iisacc.logical-disk.v1\n"+owner+"\n")
                    return std::unexpected("The Society shared disk owner has changed");
            } catch (...) { return std::unexpected("The Society shared disk owner is unavailable"); }
        }
    }
    if (id.empty() || !direct_directory(root) || !direct_directory(root/"Files") || !manifest_valid(root,id))
        return std::unexpected("The Society shared Files storage is unavailable or has changed");
    SocietyFiles result(root,root/"Files",id);
    result.group_=group; result.storage_id_=storage_id; result.storage_kind_=kind; result.owner_=owner;
    return result;
}
std::expected<SocietyFiles,std::string> SocietyFiles::discover() {
#if !defined(__ANDROID__)
    if (const char* selected=std::getenv("SOCIETY_CONTAINER_PATH"); selected && *selected) {
        const fs::path root=selected;
        const auto manifest=read_society_json(root/".society-drive.json");
        const auto id=manifest ? text(field(*manifest,"identifier")) : std::string{};
        if (id.empty() || !direct_directory(root) || !direct_directory(root/"Files") || !manifest_valid(root,id))
            return std::unexpected("The selected Society Files storage is unavailable or has changed");
        return SocietyFiles(root,root/"Files",id);
    }
    const auto registry=society_registry();
    std::error_code error;
    const auto status=fs::symlink_status(registry,error);
    const bool missing=error==std::errc::no_such_file_or_directory || (!error && !fs::exists(status));
    const char* settings_override=std::getenv("SOCIETY_STORAGE_SETTINGS_PATH");
    if (missing && (!settings_override || !*settings_override)) {
        if (const char* group=std::getenv("IISACC_SHARED_STORAGE_ROOT"); group && *group)
            return from_group(fs::path(group));
    }
    return from_registry(registry);
#else
    return std::unexpected("Connect the Society storage provider before creating a canvas");
#endif
}
std::expected<SocietyFiles,std::string> SocietyFiles::from_mobile(
    const fs::path& application_storage, const std::optional<fs::path>& system_group, bool migrate) {
    try {
        if(system_group) {
            const auto registry=*system_group/"Library/Application Support/iisacc/Society/storage.json";
            const auto package=*system_group/"Society.societycontainer";
            std::error_code error;
            const auto present=[&](const fs::path& value) {
                error.clear();const auto status=fs::symlink_status(value,error);
                if(error && error!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("Inspect Society storage",value,error);
                return !error && fs::exists(status);
            };
            const bool has_registry=present(registry);
            if(has_registry || present(package)) {
                auto shared=from_group(*system_group);
                if(!shared)return std::unexpected(shared.error());
                const auto record=has_registry ? read_society_json(registry) : std::nullopt;
                const auto manager=record ? ii::network::wire::text(ii::network::wire::field(*record,"managingApplication")) : std::string{};
                if(has_registry && manager.empty())return std::unexpected("The Society storage manager is unavailable");
                if(!has_registry || manager=="com.iisacc.society") {
                    if(present(application_storage/"Society.societycontainer")) {
                        auto local=iiSocietyContainer::MobileStorage::prepare(application_storage);
                        if(!local)return std::unexpected(local.error());
                        if(!migrate) {
                            const auto root=local->root;
                            return SocietyFiles(root,root/"Files",local->identifier);
                        }
                        auto result=iiSocietyContainer::MobileStorage::migrate(*local,shared->container_);
                        if(!result)return std::unexpected(result.error());
                    }
                    return shared;
                }
            }
        }
        auto local=iiSocietyContainer::MobileStorage::prepare(application_storage);
        if(!local)return std::unexpected(local.error());
        const auto root=local->root;
        return SocietyFiles(root,root/"Files",local->identifier);
    } catch(const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<fs::path,std::string> SocietyFiles::destination(const fs::path& path) const {
    if(!available()) return std::unexpected("Society Files storage is unavailable or has changed");
    if(!path.is_absolute() || path.lexically_normal()!=path || !under(files_,path) ||
       path.filename().empty() || path.extension()!=".iisc")
        return std::unexpected("Choose an .iisc path inside Society Files");
    const auto parent=path.parent_path();
    if(!direct_directory(parent)) return std::unexpected("The selected folder is unavailable or redirected");
    std::error_code error;
    const auto status=fs::symlink_status(path,error);
    if(error && error!=std::errc::no_such_file_or_directory)
        return std::unexpected("Cannot inspect the selected canvas path");
    if(!error && fs::exists(status) && (!fs::is_regular_file(status) || fs::is_symlink(status)))
        return std::unexpected("The selected canvas path is not a regular file");
    return path;
}
}
