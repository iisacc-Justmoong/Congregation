#include "Artwork.h"
#include "StreamingBrush.h"
#include <Render/Compositor.h>
#include "Shapes.h"
#include "LegacyCanvas.h"
#include <Engine/Data.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderCompatibility.h>
#include <iiFileProviderPresentation.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <numbers>
#include <optional>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace congregation::native
{
using namespace iiSharedCanvas;
namespace
{
struct FileIdentity final { std::uint64_t device, inode; };
std::optional<FileIdentity> file_identity(const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool ok = GetFileInformationByHandle(handle, &info) &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
    CloseHandle(handle);
    if (!ok) return std::nullopt;
    return FileIdentity{info.dwVolumeSerialNumber,
                        (std::uint64_t(info.nFileIndexHigh) << 32) | info.nFileIndexLow};
#else
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return std::nullopt;
    return FileIdentity{std::uint64_t(info.st_dev), std::uint64_t(info.st_ino)};
#endif
}
std::optional<std::string> document_digest(const Document& document,
                                           const DocumentAuthorship& authorship,
                                           FormatVersion format)
{
    const auto digest = [](const Document& value) -> std::optional<std::string> {
        const auto bytes = encodeIisc(value);
        if (!bytes.ok()) return std::nullopt;
        return iiFileProvider::native::sha256({reinterpret_cast<const char*>(bytes.bytes.data()), bytes.bytes.size()});
    };
    if (document.authorship.dump() == authorship.dump() &&
        document.formatVersion.major == format.major && document.formatVersion.minor == format.minor)
        return digest(document);
    Document copy = document;
    copy.authorship = authorship;
    copy.formatVersion = format;
    return digest(copy);
}
constexpr std::size_t HistoryCount = 32;
bool currentBitmapFormat(FormatVersion version) noexcept
{
    return version.major == CurrentFormatMajor && version.minor == CurrentFormatMinor;
}
DevicePixelRect unionStrokeDamage(DevicePixelRect a, DevicePixelRect b) noexcept
{
    if (a.width <= 0 || a.height <= 0) return b;
    if (b.width <= 0 || b.height <= 0) return a;
    const auto left = std::min<std::int64_t>(a.origin.x, b.origin.x);
    const auto top = std::min<std::int64_t>(a.origin.y, b.origin.y);
    const auto right = std::max(std::int64_t(a.origin.x) + a.width, std::int64_t(b.origin.x) + b.width);
    const auto bottom = std::max(std::int64_t(a.origin.y) + a.height, std::int64_t(b.origin.y) + b.height);
    return {{int(left), int(top)}, int(right - left), int(bottom - top)};
}
std::optional<CanvasRegion> intersectCanvasRegions(CanvasRegion a, CanvasRegion b)
{
    const auto left=std::max<std::int64_t>(a.origin.x,b.origin.x),top=std::max<std::int64_t>(a.origin.y,b.origin.y);
    const auto right=std::min(std::int64_t(a.origin.x)+a.extent.width,std::int64_t(b.origin.x)+b.extent.width);
    const auto bottom=std::min(std::int64_t(a.origin.y)+a.extent.height,std::int64_t(b.origin.y)+b.extent.height);
    if(right<=left || bottom<=top)return std::nullopt;
    return CanvasRegion{{int(left),int(top)},{int(right-left),int(bottom-top)}};
}
// Builds canonical finite 256px chunks from non-overlapping 64px mask regions.
class SparseRasterBuilder final {
public:
    explicit SparseRasterBuilder(std::size_t budget):budget_(budget){}
    bool add(CanvasRegion region,const ::RasterLayer& pixels) {
        constexpr int side=256;
        if(std::none_of(pixels.pixels.begin(),pixels.pixels.end(),[](std::uint32_t p){return p>>24;}))return true;
        const int column=region.origin.x/side,row=region.origin.y/side;
        const int ox=region.origin.x%side,oy=region.origin.y%side;
        if(region.origin.x<0 || region.origin.y<0 || ox+region.extent.width>side || oy+region.extent.height>side ||
           pixels.width!=region.extent.width || pixels.height!=region.extent.height) {error_="Sparse raster tile alignment is invalid";return false;}
        auto it=tiles_.find({row,column});
        if(it==tiles_.end()) {
            constexpr std::size_t cost=std::size_t(side)*side*sizeof(std::uint32_t)+sizeof(RasterChunk);
            if(tiles_.size()>=budget_/cost){error_="Sparse raster exceeds the working or history budget";return false;}
            it=tiles_.emplace(std::pair{row,column},makeRasterLayer(side,side,0)).first;
        }
        for(int y=0;y<region.extent.height;++y)
            std::copy_n(pixels.pixels.begin()+std::size_t(y)*region.extent.width,region.extent.width,
                        it->second.pixels.begin()+std::size_t(oy+y)*side+ox);
        return true;
    }
    bool empty() const noexcept {return tiles_.empty();}
    const std::string& error() const noexcept {return error_;}
    ChunkedRasterAsset take() {
        ChunkedRasterAsset result;result.chunks.reserve(tiles_.size());
        for(auto& [key,pixels]:tiles_)result.chunks.push_back({key.second,key.first,std::move(pixels)});
        return result;
    }
private:
    std::size_t budget_;
    std::map<std::pair<int,int>,::RasterLayer> tiles_;
    std::string error_;
};
const iiSharedCanvas::Layer* layer(const Document& d, std::string_view key)
{
    auto p = std::ranges::find_if(d.layers, [&](const auto& value)
                                  { return layerProperties(value).id == key; });
    return p == d.layers.end() ? nullptr : &*p;
}
std::optional<DocumentPoint> localPoint(DocumentPoint point, const AffineTransform& t)
{
    const double determinant = t.m11 * t.m22 - t.m12 * t.m21;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        !std::isfinite(determinant) || determinant == 0)
        return {};
    const auto x = point.x - t.translationX, y = point.y - t.translationY;
    const DocumentPoint result{(t.m22 * x - t.m21 * y) / determinant,
                               (-t.m12 * x + t.m11 * y) / determinant};
    if (!std::isfinite(result.x) || !std::isfinite(result.y))
        return {};
    return result;
}

std::optional<CanvasRegion> assetBoundsForCanvas(CanvasRegion canvas, const AffineTransform& transform)
{
    const double left=canvas.origin.x,top=canvas.origin.y,right=left+canvas.extent.width,bottom=top+canvas.extent.height;
    const std::array corners{localPoint({left,top},transform),localPoint({right,top},transform),
                             localPoint({left,bottom},transform),localPoint({right,bottom},transform)};
    double x0=std::numeric_limits<double>::infinity(),y0=x0,x1=-x0,y1=-x0;
    for(const auto& corner:corners) {
        if(!corner)return std::nullopt;
        x0=std::min(x0,corner->x);y0=std::min(y0,corner->y);x1=std::max(x1,corner->x);y1=std::max(y1,corner->y);
    }
    x0=std::floor(x0);y0=std::floor(y0);x1=std::ceil(x1);y1=std::ceil(y1);
    if(x0 < -1000000000. || y0 < -1000000000. || x1 > 1000000000. || y1 > 1000000000. || x1<=x0 || y1<=y0)return std::nullopt;
    return CanvasRegion{{int(x0),int(y0)},{int(x1-x0),int(y1-y0)}};
}

// A full-canvas opaque vector underneath all visible bitmap layers supplies a
// uniform exterior. Other vector content remains unsupported by the bounded
// composite reference because it may vary outside the rendered region.
std::optional<std::uint32_t> uniformCanvasBackground(const Document& document,const iiSharedCanvas::Layer& layer,FrameIndex frame)
{
    if(document.canvasMode!=CanvasMode::Finite || !std::holds_alternative<StaticVectorLayer>(layer))return std::nullopt;
    const auto& props=layerProperties(layer);const auto sample=sampleLayerAt(document,layer,frame);const auto& t=sample.transform;
    if(!sample.visible || sample.opacity!=1 || props.blendMode!=RasterBlendMode::SourceOver || props.artboardId ||
       !(props.appearance==LayerAppearance{}) || t.m11!=1 || t.m12!=0 || t.m21!=0 || t.m22!=1 || t.translationX!=0 || t.translationY!=0)return std::nullopt;
    const auto* source=resolveAssetAt(document,layer,frame);const auto* vector=source?std::get_if<VectorAsset>(source):nullptr;
    if(!vector || vector->viewport!=document.extent || vector->paths.size()!=1)return std::nullopt;
    const auto& path=vector->paths[0];if(!path.fill || (path.fill->argb>>24)!=255 || path.stroke || path.commands.size()!=5)return std::nullopt;
    const auto* first=std::get_if<MoveTo>(&path.commands[0]);const auto* second=std::get_if<LineTo>(&path.commands[1]);
    const auto* third=std::get_if<LineTo>(&path.commands[2]);const auto* fourth=std::get_if<LineTo>(&path.commands[3]);
    if(!first || !second || !third || !fourth || !std::holds_alternative<ClosePath>(path.commands[4]))return std::nullopt;
    const double w=document.extent.width,h=document.extent.height;
    if(first->point!=Point{0,0} || second->point!=Point{w,0} || third->point!=Point{w,h} || fourth->point!=Point{0,h})return std::nullopt;
    return path.fill->argb;
}

struct ReferenceBounds final {
    bool any=false;
    double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
};
bool includeReferenceAsset(const Asset& asset,const Document& document,const AffineTransform& transform,ReferenceBounds& result)
{
    double left=0,top=0,right=0,bottom=0;
    if(const auto* raster=std::get_if<RasterAsset>(&asset)) {
        if(raster->pixels.width<=0 || raster->pixels.height<=0 ||
           std::uint64_t(raster->pixels.width)*raster->pixels.height>64ULL*1024*1024)return false;
        right=raster->pixels.width;bottom=raster->pixels.height;
    } else if(const auto* chunks=std::get_if<ChunkedRasterAsset>(&asset)) {
        const auto size=document.infiniteCanvas.chunkSize;
        if(size<=0 || size>4096 || chunks->chunks.size()>65536)return false;
        if(chunks->chunks.empty())return true;
        std::int64_t x0=std::numeric_limits<std::int64_t>::max(),y0=x0,x1=std::numeric_limits<std::int64_t>::min(),y1=x1;
        for(const auto& chunk:chunks->chunks) {
            if(chunk.pixels.width!=size || chunk.pixels.height!=size ||
               chunk.pixels.pixels.size()!=std::size_t(size)*size)return false;
            const auto x=std::int64_t(chunk.column)*size,y=std::int64_t(chunk.row)*size;
            x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x+size);y1=std::max(y1,y+size);
        }
        left=double(x0);top=double(y0);right=double(x1);bottom=double(y1);
    } else return false;
    const std::array<Point,4> corners{{{left,top},{right,top},{right,bottom},{left,bottom}}};
    for(const auto& point:corners) {
        const double x=transform.m11*point.x+transform.m21*point.y+transform.translationX;
        const double y=transform.m12*point.x+transform.m22*point.y+transform.translationY;
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        result.left=std::min(result.left,x);result.right=std::max(result.right,x);
        result.top=std::min(result.top,y);result.bottom=std::max(result.bottom,y);
    }
    result.any=true;return true;
}

} // namespace
std::filesystem::path native_path(std::string_view value)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
std::string path_text(const std::filesystem::path& p)
{
    auto value = p.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
Artwork::Artwork(std::filesystem::path directory, std::size_t history_budget)
    : history_budget_(history_budget), directory_(std::move(directory))
{
    iiFileProvider::native::File::createDirectories(directory_);
}
Artwork::~Artwork() { close_temporary(); }
bool Artwork::create_startup_placeholder()
{
    if (file_.isOpen()) return fail("A startup placeholder requires an empty artwork owner");
    if (!create(1,1)) return false;
    try {
        const auto identity=file_identity(recovery_);
        const auto digest=document_digest(draft_,draft_.authorship,draft_.formatVersion);
        if (!identity || !digest) return fail("Cannot verify the startup placeholder");
        startup_placeholder_.emplace(StartupPlaceholder{recovery_,*digest,draft_.authorship,
                                                          draft_.formatVersion,identity->device,identity->inode});
        return true;
    } catch (...) { return fail("Cannot prepare the startup placeholder"); }
}
bool Artwork::removable_startup_placeholder() const noexcept
{
    try {
        if (!startup_placeholder_ || !file_.isOpen() || !source_.empty() || stroke_active() || file_.revision()!=0) return false;
        const auto& temp=*startup_placeholder_;
        if (temp.path!=recovery_ || temp.path.parent_path().lexically_normal()!=directory_.lexically_normal()) return false;
        const auto identity=file_identity(temp.path);
        if (!identity || identity->device!=temp.device || identity->inode!=temp.inode) return false;
        DocumentFileInfo info;
        if (!DocumentFile::readStorageInfo(path_text(temp.path),info).ok() || info.revision!=0) return false;
        const auto current=document_digest(draft_,temp.authorship,temp.format);
        if (!current || *current!=temp.initial_digest || draft_.authorship.dump()!=temp.authorship.dump()) return false;
        Document disk;
        if (!DocumentFile::readSnapshot(path_text(temp.path),disk).ok() || disk.authorship.dump()!=temp.authorship.dump()) return false;
        const auto persisted=document_digest(disk,temp.authorship,temp.format);
        return persisted && *persisted==temp.initial_digest;
    } catch (...) { return false; }
}
void Artwork::remove_startup_placeholder(const StartupPlaceholder& temp) noexcept
{
    try {
        const auto identity=file_identity(temp.path);
        if (identity && identity->device==temp.device && identity->inode==temp.inode)
            (void)iiFileProvider::native::File::remove(temp.path);
    } catch (...) {}
}
bool Artwork::removable_temporary() const noexcept
{
    try {
        if (!society_temporary_ || stroke_active() || !file_.isOpen()) return false;
        const auto& temp = *society_temporary_;
        const auto identity = file_identity(temp.path);
        if (!temp.files.available() || !identity || identity->device != temp.device || identity->inode != temp.inode) return false;
        DocumentFileInfo info;
        if (!DocumentFile::readStorageInfo(path_text(temp.path), info).ok() || info.revision != file_.revision()) return false;
        const auto current = document_digest(draft_, temp.authorship, temp.format);
        if (!current) return false;
        if (source_.empty()) {
            // Editing timestamps and revision may change after an Undo, but
            // author profiles, links and the active author are document data.
            if (*current != temp.initial_digest ||
                draft_.authorship.firstEditor() != temp.authorship.firstEditor() ||
                draft_.authorship.participants() != temp.authorship.participants() ||
                draft_.authorship.links() != temp.authorship.links() ||
                draft_.authorship.hasActiveAuthor() != temp.authorship.hasActiveAuthor()) return false;
        } else {
            const auto saved_identity = file_identity(source_);
            if (source_ != temp.saved_path || !temp.files.destination(source_) || !saved_identity ||
                saved_identity->device != temp.saved_device || saved_identity->inode != temp.saved_inode ||
                file_.revision() != temp.saved_revision) return false;
            Document saved;
            if (!DocumentFile::readSnapshot(path_text(source_), saved).ok()) return false;
            if (saved.authorship.dump() != draft_.authorship.dump()) return false;
            const auto saved_digest = document_digest(saved, temp.authorship, temp.format);
            if (!saved_digest || *saved_digest != *current) return false;
        }
        Document disk;
        if (!DocumentFile::readSnapshot(path_text(temp.path), disk).ok()) return false;
        if (disk.authorship.dump() != draft_.authorship.dump()) return false;
        const auto persisted = document_digest(disk, temp.authorship, temp.format);
        return persisted && *persisted == *current;
    } catch (...) { return false; }
}
void Artwork::remove_temporary(const TemporaryCanvas& temp) noexcept
{
    try {
        const auto identity = file_identity(temp.path);
        const auto saved_identity = temp.saved_path.empty() ? std::optional<FileIdentity>{} : file_identity(temp.saved_path);
        const bool saved_unchanged = temp.saved_path.empty() ||
            (saved_identity && saved_identity->device == temp.saved_device && saved_identity->inode == temp.saved_inode &&
             bool(temp.files.destination(temp.saved_path)));
        if (saved_unchanged && temp.files.available() && identity && identity->device == temp.device && identity->inode == temp.inode)
            (void)iiFileProvider::native::File::remove(temp.path);
    } catch (...) {}
}
void Artwork::close_temporary() noexcept
{
    if (!society_temporary_ && !startup_placeholder_) return;
    try {
        const auto& path=society_temporary_ ? society_temporary_->path : startup_placeholder_->path;
        iiFileProvider::native::SourceAccess access(path);
        std::lock_guard lock(access);
        bool remove = removable_temporary(), remove_placeholder=removable_startup_placeholder();
        try { cancel(); } catch (...) { remove = false; remove_placeholder=false; }
        file_.close();
        if (remove) remove_temporary(*society_temporary_);
        if (remove_placeholder) remove_startup_placeholder(*startup_placeholder_);
    } catch (...) {
        try { cancel(); } catch (...) {}
        file_.close();
    }
    society_temporary_.reset();
    startup_placeholder_.reset();
}
bool Artwork::has_selection() const noexcept { return bool(selection_); }
const RasterMask* Artwork::selection_mask() const noexcept { return selection_ ? selection_->dense.get() : nullptr; }
std::shared_ptr<const RasterMask> Artwork::selection_mask_handle() const noexcept { return selection_ ? selection_->dense : nullptr; }
std::shared_ptr<const TiledMask> Artwork::tiled_selection_handle() const noexcept { return selection_ ? selection_->tiled : nullptr; }
bool Artwork::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
std::vector<std::uint8_t> Artwork::capture() const
{
    SerializationLimits limits;
    limits.maximumContainerBytes = history_budget_ / 2;
    auto encoded = encodeIisc(draft_, limits);
    return encoded.ok() ? std::move(encoded.bytes) : std::vector<std::uint8_t>{};
}
bool Artwork::replace(Document document)
{
    if (pending_society_ && !pending_society_->available())
        return fail("Society Files storage is unavailable or has changed");
    auto next = (pending_society_ ? pending_society_->root() : directory_) / (uuid() + ".iisc");
    auto staged = std::make_unique<DocumentFile>();
    auto created = staged->create(path_text(next), document);
    if (!created.ok())
        return fail(created.message);
    std::optional<TemporaryCanvas> next_temporary;
    if (pending_society_) {
        try {
            const auto identity = file_identity(next);
            const auto digest = document_digest(*staged->document(), staged->document()->authorship,
                                                staged->document()->formatVersion);
            if (!identity || !digest) {
                staged->close();
                return fail("Cannot verify the new Society canvas");
            }
            next_temporary.emplace(TemporaryCanvas{*pending_society_, next, *digest,
                staged->document()->authorship, staged->document()->formatVersion, identity->device, identity->inode});
        } catch (...) {
            staged->close();
            return fail("Cannot prepare the new Society canvas");
        }
    }
    std::unique_ptr<iiFileProvider::native::SourceAccess> previous_access;
    std::unique_lock<iiFileProvider::native::SourceAccess> previous_lock;
    if (society_temporary_ || startup_placeholder_) {
        try {
            const auto& path=society_temporary_ ? society_temporary_->path : startup_placeholder_->path;
            previous_access = std::make_unique<iiFileProvider::native::SourceAccess>(path);
            previous_lock = std::unique_lock<iiFileProvider::native::SourceAccess>(*previous_access);
        } catch (...) {}
    }
    const bool remove_previous = previous_lock.owns_lock() && removable_temporary();
    const bool remove_placeholder = previous_lock.owns_lock() && removable_startup_placeholder();
    const auto previous_path = recovery_;
    cancel();
    staged->close();
    file_.close();
    auto opened = file_.open(path_text(next));
    if (!opened.ok()) {
        if (!previous_path.empty()) (void)file_.open(path_text(previous_path));
        return fail(opened.message);
    }
    if (remove_previous && society_temporary_) remove_temporary(*society_temporary_);
    if (remove_placeholder && startup_placeholder_) remove_startup_placeholder(*startup_placeholder_);
    society_temporary_ = std::move(next_temporary);
    startup_placeholder_.reset();
    draft_ = *file_.document();
    recovery_ = next;
    source_.clear();
    creation_.reset();
    selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
    undo_.clear();
    redo_.clear();
    history_bytes_ = 0;
    selection_.reset();
    stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
    bind();
    error_.clear();
    if (changed)
        changed();
    return true;
}
bool Artwork::create_in_society(const CanvasDefinition& value, const SocietyFiles& files)
{
    if (!files.available()) return fail("Society Files storage is unavailable or has changed");
    pending_society_ = files;
    try {
        const bool result = create(value);
        pending_society_.reset();
        return result;
    } catch (...) {
        pending_society_.reset();
        return fail("Cannot create the Society canvas");
    }
}
bool Artwork::create(int width, int height, bool infinite)
{
    if (infinite)
        return fail("Infinite canvas is no longer supported");
    if (width < 1 || height < 1 || width > CanvasMaximumDimension || height > CanvasMaximumDimension ||
        std::uint64_t(width) * height > CanvasMaximumPixels)
        return fail("Canvas extent exceeds the document limit");
    Document document;
    document.extent = {width, height};
    if (std::uint64_t(width) * height > 64ULL * 1024 * 1024)
        document.assets.emplace_back(ChunkedRasterAsset{"paint", {}});
    else
        document.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(width, height, 0)});
    document.layers.emplace_back(
        StaticBitmapLayer{{"paint-layer", "Paint layer"}, StaticSource{"paint"}});
    return replace(std::move(document));
}
bool Artwork::create(const CanvasDefinition& value)
{
    if (value.pixel_width < 1 || value.pixel_height < 1 || value.pixel_width > CanvasMaximumDimension ||
        value.pixel_height > CanvasMaximumDimension ||
        std::uint64_t(value.pixel_width) * value.pixel_height > CanvasMaximumPixels ||
        (value.background != "White" && value.background != "Transparent" &&
         value.background != "Black"))
        return fail("Enter valid canvas dimensions and background");
    Document document;
    document.extent = {value.pixel_width, value.pixel_height};
    if (value.background != "Transparent")
    {
        VectorPath path;
        const double w = value.pixel_width, h = value.pixel_height;
        path.commands = {MoveTo{{0, 0}}, LineTo{{w, 0}}, LineTo{{w, h}}, LineTo{{0, h}},
                         ClosePath{}};
        path.fill = SolidPaint{value.background == "Black" ? 0xff000000U : 0xffffffffU};
        document.assets.emplace_back(VectorAsset{"canvas.background", document.extent, {path}});
        document.layers.emplace_back(StaticVectorLayer{{"canvas.background.layer", "Background"},
                                                       StaticSource{"canvas.background"}});
    }
    if (std::uint64_t(value.pixel_width) * value.pixel_height > 64ULL * 1024 * 1024)
        document.assets.emplace_back(ChunkedRasterAsset{"paint", {}});
    else
        document.assets.emplace_back(
            RasterAsset{"paint", makeRasterLayer(value.pixel_width, value.pixel_height, 0)});
    document.layers.emplace_back(
        StaticBitmapLayer{{"paint-layer", "Paint layer"}, StaticSource{"paint"}});
    if (!replace(std::move(document)))
        return false;
    creation_ = value;
    if (changed)
        changed();
    return true;
}
bool Artwork::open(const std::filesystem::path& path)
{
    std::string prefix;
    try
    {
        auto reader = iiFileProvider::native::File::openRead(path);
        prefix = reader->read(16);
    }
    catch (const std::exception& error)
    {
        return fail(error.what());
    }
    Document doc;
    const bool native = prefix.starts_with(std::string_view{"IISC\r\n\x1a\n", 8}) ||
                        prefix.starts_with(std::string_view{"SQLite format 3\0", 16});
    if (native)
    {
        const auto read = DocumentFile::readSnapshot(path_text(path), doc);
        if (!read.ok())
            return fail(read.message.empty() ? "Cannot read the native document" : read.message);
        if (doc.canvasMode == CanvasMode::Infinite)
            return fail("Infinite canvas documents are no longer supported");
        if (!replace(std::move(doc)))
            return false;
        source_ = path;
        return true;
    }
    const auto extension = lower(path.extension().string());
    if ((extension == ".vrc" || extension == ".iisc") && prefix.starts_with("CONGREGATIONRC"))
    {
        try
        {
            auto bytes = iiFileProvider::native::File::read(path, maximum_canvas_bytes);
            auto legacy = decode_canvas(bytes);
            if (!legacy)
                return fail(legacy.error());
            auto migrated = migrate_legacy_canvas(*legacy);
            if (!migrated)
                return fail(migrated.error());
            if (migrated->canvasMode == CanvasMode::Infinite)
                return fail("Infinite canvas documents are no longer supported");
            return replace(std::move(*migrated));
        }
        catch (const std::exception& error)
        {
            return fail(error.what());
        }
    }
    if (extension == ".psd" || extension == ".ora")
    {
        auto decoded = importLayeredDocument(path_text(path));
        if (!decoded.ok())
            return fail(decoded.result.message);
        if (!replace(std::move(decoded.document)))
            return false;
        return true;
    }
    auto bitmap = importBitmap(path_text(path));
    if (!bitmap.ok())
        return fail(bitmap.result.message);
    doc.extent = {bitmap.asset.pixels.width, bitmap.asset.pixels.height};
    doc.layers.emplace_back(StaticBitmapLayer{{"image-layer", path_text(path.filename())},
                                              StaticSource{bitmap.asset.id}});
    doc.assets.emplace_back(std::move(bitmap.asset));
    return replace(std::move(doc));
}
bool Artwork::recover(const std::filesystem::path& path)
{
    if (!open(path))
        return false;
    source_.clear();
    return true;
}
bool Artwork::save(const std::filesystem::path& path)
{
    if (stroke_active())
        return fail("Finish the current stroke before saving");
    auto encoded = encodeIisc(draft_);
    if (!encoded.ok())
        return fail("Cannot encode the canonical document");
    try
    {
        iiFileProvider::native::writeCompatible(
            path, {reinterpret_cast<const char*>(encoded.bytes.data()), encoded.bytes.size()});
        source_ = path;
        error_.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        return fail(e.what());
    }
}
bool Artwork::save_in_society(const std::filesystem::path& path)
{
    if (!society_temporary_) return fail("The canvas is not associated with Society Files");
    const auto destination = society_temporary_->files.destination(path);
    if (!destination) return fail(destination.error());
    if (*destination == recovery_) return fail("Choose a new path for the saved canvas");
    if (!save(*destination)) return false;
    const auto saved = file_identity(*destination);
    if (saved) {
        society_temporary_->saved_path = *destination;
        society_temporary_->saved_device = saved->device;
        society_temporary_->saved_inode = saved->inode;
        society_temporary_->saved_revision = file_.revision();
    }
    return true;
}
bool Artwork::export_image(const std::filesystem::path& path, std::string format)
{
    if (stroke_active())
        return fail("Finish the current stroke before export");
    MediaIoResult result;
    if (format == "psd")
        result = exportPsd(draft_, path_text(path), {.overwrite = true});
    else
    {
        BitmapExportOptions options;
        options.format = std::move(format);
        options.overwrite = true;
        result = exportBitmapFrame(draft_, 0, path_text(path), options);
    }
    if (!result.ok())
        return fail(result.message);
    error_.clear();
    return true;
}
bool Artwork::bind()
{
    raster_.unbind();
    chunks_.unbind();
    auto* current = layer(draft_, selected_);
    if (!current)
        return false;
    auto* source = staticLayerSource(*current);
    if (!source)
        return false;
    auto* asset = findAsset(draft_, source->assetId);
    if (!asset)
        return false;
    if (std::holds_alternative<RasterAsset>(*asset))
        return raster_.bind(draft_, source->assetId);
    if (std::holds_alternative<ChunkedRasterAsset>(*asset))
        return chunks_.bind(draft_, source->assetId);
    return false;
}
bool Artwork::replace_frame_bitmap(DocumentEditor& editor, FrameIndex frame,
                                   RasterLayer pixels, std::string& replacement)
{
    const auto* selected = layer(draft_, selected_);
    if (!selected || frame >= draft_.timeline.frameCount)
        return fail("Select a bitmap layer on the current frame");
    const auto* asset = resolveAssetAt(draft_, *selected, frame);
    const auto* bitmap = asset ? std::get_if<RasterAsset>(asset) : nullptr;
    if (!bitmap) return fail("The current frame has no editable bitmap");
    const std::string prior = bitmap->id;
    const bool dynamic = std::holds_alternative<DynamicBitmapLayer>(*selected);
    const bool is_static = std::holds_alternative<StaticBitmapLayer>(*selected);
    if (!dynamic && !is_static) return fail("Select a bitmap layer on the current frame");
    const auto range = layerProperties(*selected).frameRange;
    const bool one_frame = draft_.timeline.frameCount == 1 ||
        (range && range->firstFrame == frame && range->lastFrame == frame);
    const bool restore_next = dynamic && frame + 1 < draft_.timeline.frameCount &&
        !findKeyframe(draft_, selected_, frame + 1);
    replacement = uuid();
    auto apply = [&](const DocumentEditResult& result) {
        return result.ok() || fail(result.message.empty() ? "Cannot replace frame bitmap" : result.message);
    };
    if (dynamic) {
        if (!apply(editor.setDynamicFrameContent(selected_, frame, RasterAsset{replacement, std::move(pixels)}))) return false;
        if (restore_next && !apply(editor.insertKeyframe(selected_, frame + 1, prior))) return false;
    } else {
        if (!apply(editor.insertRasterAsset(replacement, std::move(pixels)))) return false;
        if (one_frame) {
            if (!apply(editor.setStaticSource(selected_, replacement))) return false;
        } else {
            std::vector<KeyframePlacement> keys;
            if (frame > 0) keys.push_back({0, prior});
            keys.push_back({frame, replacement});
            if (frame + 1 < draft_.timeline.frameCount) keys.push_back({frame + 1, prior});
            if (!apply(editor.setKeyframedSource(selected_, std::move(keys)))) return false;
        }
    }
    if (assetReferences(draft_, prior).empty() && !apply(editor.removeAsset(prior))) return false;
    return true;
}

std::optional<Artwork::FrameSourceHistory> Artwork::capture_frame_source(const Document& doc, const std::string& id)
{
    const auto* item=layer(doc,id);if(!item)return std::nullopt;
    FrameSourceHistory result;
    if(const auto* source=staticLayerSource(*item)) result.asset=source->assetId;
    else if(keyframedLayerSource(*item)) {
        result.dynamic=true;
        for(const auto& frame:doc.frames)for(const auto& key:frame.keyframes)
            if(key.layerId==id)result.keyframes.push_back({frame.index,key.assetId});
    } else return std::nullopt;
    return result;
}
bool Artwork::restore_frame_source(const std::string& id, const FrameSourceHistory& source)
{
    DocumentEditor editor(draft_);
    const auto result=source.dynamic ? editor.setKeyframedSource(id,source.keyframes) : editor.setStaticSource(id,source.asset);
    if(!result.ok())return fail(result.message.empty()?"Cannot restore frame source":result.message);
    return true;
}

bool Artwork::replace_frame_chunked(DocumentEditor& editor, FrameIndex frame,
                                    ChunkedRasterAsset content, std::string& replacement)
{
    const auto* selected = layer(draft_, selected_);
    if (!selected || frame >= draft_.timeline.frameCount)
        return fail("Select a chunk bitmap layer on the current frame");
    const auto* resolved = resolveAssetAt(draft_, *selected, frame);
    const auto* current = resolved ? std::get_if<ChunkedRasterAsset>(resolved) : nullptr;
    if (!current || current->id != content.id) return fail("The current frame has no editable chunk bitmap");
    const auto prior = content.id;
    const bool dynamic = std::holds_alternative<DynamicBitmapLayer>(*selected);
    const bool is_static = std::holds_alternative<StaticBitmapLayer>(*selected);
    if (!dynamic && !is_static) return fail("Select a bitmap layer on the current frame");
    const auto range = layerProperties(*selected).frameRange;
    const bool one_frame = draft_.timeline.frameCount == 1 ||
        (range && range->firstFrame == frame && range->lastFrame == frame);
    const bool existing_key = dynamic && findKeyframe(draft_, selected_, frame);
    const bool restore_next = dynamic && frame + 1 < draft_.timeline.frameCount &&
        !findKeyframe(draft_, selected_, frame + 1);
    const auto apply = [&](const DocumentEditResult& result) {
        return result.ok() || fail(result.message.empty() ? "Cannot replace frame chunks" : result.message);
    };
    replacement = uuid(); content.id = replacement;
    if (!apply(editor.insertChunkedRasterAsset(std::move(content)))) return false;
    if (dynamic) {
        if (!apply(existing_key ? editor.setKeyframeAsset(selected_, frame, replacement)
                                : editor.insertKeyframe(selected_, frame, replacement))) return false;
        if (restore_next && !apply(editor.insertKeyframe(selected_, frame + 1, prior))) return false;
    } else if (one_frame) {
        if (!apply(editor.setStaticSource(selected_, replacement))) return false;
    } else {
        std::vector<KeyframePlacement> keys;
        if (frame > 0) keys.push_back({0, prior});
        keys.push_back({frame, replacement});
        if (frame + 1 < draft_.timeline.frameCount) keys.push_back({frame + 1, prior});
        if (!apply(editor.setKeyframedSource(selected_, std::move(keys)))) return false;
    }
    if (assetReferences(draft_, prior).empty() && !apply(editor.removeAsset(prior))) return false;
    return true;
}

bool Artwork::select(std::string key)
{
    if (stroke_active())
        cancel();
    if (!layer(draft_, key))
        return fail("Layer no longer exists");
    const bool different = selected_ != key;
    selected_ = std::move(key);
    bind();
    if (different && selection_changed)
        selection_changed();
    return true;
}
bool Artwork::commit(std::vector<std::uint8_t> before, std::string before_selection, bool history,
                     std::optional<SelectionTransition> selection)
{
    std::vector<std::uint8_t> after;
    if (history)
    {
        const auto rollback = [&](std::string message) {
            draft_ = *file_.document(); selected_ = before_selection; bind(); return fail(std::move(message));
        };
        if (before.empty()) return rollback("Edit exceeds the undo budget");
        try {
            after = capture();
            if (after.empty()) return rollback("Edit exceeds the undo budget");
            // DocumentFile stamps authorship only for a changed document whose
            // caller did not already alter it. Do this before writing so the
            // exact post-edit state fits in history before any durable commit.
            if (after != before && draft_.authorship.dump() == file_.document()->authorship.dump()) {
                recordDocumentChange(draft_);
                after = capture();
            }
            const std::size_t maskBytes = selection ?
                (selection->before ? selection->before->bytes() : 0) +
                (selection->after ? selection->after->bytes() : 0) : 0;
            if (after.empty() || before.size() > history_budget_ || after.size() > history_budget_ - before.size() ||
                maskBytes > history_budget_ - before.size() - after.size())
                return rollback("Edit exceeds the undo budget");
        } catch (const std::bad_alloc&) { return rollback("Insufficient memory for edit history"); }
    }
    auto committed = file_.edit(
        [&](Document& doc)
        {
            doc = draft_;
            return true;
        });
    if (!committed.ok())
    {
        draft_ = *file_.document();
        selected_ = std::move(before_selection);
        bind();
        return fail(committed.message);
    }
    if (!committed.changed)
    {
        selected_ = std::move(before_selection);
        bind();
        error_.clear();
        return true;
    }
    draft_ = *file_.document();
    bind();
    if (selection) selection_ = selection->after;
    if (history)
    {
        for (const auto& entry : redo_)
            history_bytes_ -= entry.bytes();
        redo_.clear();
        History entry{std::move(before), std::move(after), std::move(before_selection), selected_};
        if (selection) { entry.mask_change=true;entry.mask_before=std::move(selection->before);entry.mask_after=std::move(selection->after); }
        history_bytes_ += entry.bytes();
        undo_.push_back(std::move(entry));
        while (history_bytes_ > history_budget_ || undo_.size() > HistoryCount)
        {
            history_bytes_ -= undo_.front().bytes();
            undo_.pop_front();
        }
    }
    error_.clear();
    if (changed)
        changed();
    return true;
}
bool Artwork::edit(const std::function<bool(DocumentEditor&)>& action)
{
    cancel();
    auto before = capture();
    if (before.empty())
        return fail("Edit exceeds the undo budget");
    auto selection = selected_;
    DocumentEditor editor(draft_);
    if (!action(editor))
    {
        auto failure = editor.lastResult().message;
        draft_ = *file_.document();
        selected_ = std::move(selection);
        bind();
        return fail(failure.empty() ? "Document edit was rejected" : failure);
    }
    if (capture().empty())
    {
        draft_ = *file_.document();
        selected_ = std::move(selection);
        bind();
        return fail("Edit exceeds the undo budget");
    }
    return commit(std::move(before), std::move(selection));
}
bool Artwork::add_layer(std::string name)
{
    const auto asset = uuid(), key = uuid();
    return edit(
        [&](DocumentEditor& editor)
        {
            if (draft_.canvasMode == CanvasMode::Infinite ||
                std::uint64_t(draft_.extent.width) * draft_.extent.height > 64ULL * 1024 * 1024)
            {
                if (!editor.insertChunkedRasterAsset({asset, {}}).ok())
                    return false;
            }
            else if (!editor
                          .insertRasterAsset(
                              asset, makeRasterLayer(draft_.extent.width, draft_.extent.height, 0))
                          .ok())
                return false;
            if (!editor.insertLayer(StaticBitmapLayer{{key, std::move(name)}, StaticSource{asset}})
                     .ok())
                return false;
            selected_ = key;
            return true;
        });
}
bool Artwork::duplicate_layer()
{
    cancel();
    auto* original = layer(draft_, selected_);
    if (!original)
        return false;
    auto copy = *original;
    auto& properties = layerProperties(copy);
    properties.id = uuid();
    properties.name += " copy";
    auto* source = staticLayerSource(copy);
    if (!source)
        return fail("Animated layer duplication needs an explicit frame");
    auto* original_asset = findAsset(draft_, source->assetId);
    if (!original_asset)
        return false;
    auto asset = *original_asset;
    std::visit(
        [&](auto& value)
        {
            value.id = uuid();
            source->assetId = value.id;
        },
        asset);
    return edit(
        [&](DocumentEditor& editor)
        {
            auto inserted = std::visit(
                [&](auto value) -> DocumentEditResult
                {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::same_as<T, RasterAsset>)
                        return editor.insertRasterAsset(std::move(value.id),
                                                        std::move(value.pixels));
                    else if constexpr (std::same_as<T, ChunkedRasterAsset>)
                        return editor.insertChunkedRasterAsset(std::move(value));
                    else if constexpr (std::same_as<T, VectorAsset>)
                        return editor.insertVectorAsset(std::move(value.id), value.viewport,
                                                        std::move(value.paths));
                    else
                        return {DocumentEditCode::InvalidArgument,
                                false,
                                {},
                                "Duplicate requires a bitmap or vector layer"};
                },
                asset);
            if (!inserted.ok())
                return false;
            const auto key = properties.id;
            auto result = editor.insertLayer(std::move(copy));
            if (result.ok())
                selected_ = key;
            return result.ok();
        });
}
bool Artwork::remove_layer()
{
    auto key = selected_;
    return edit(
        [&](DocumentEditor& editor)
        {
            auto* current = layer(draft_, key);
            const auto* source = current ? staticLayerSource(*current) : nullptr;
            const auto asset = source ? source->assetId : std::string{};
            if (!editor.removeLayer(key).ok())
                return false;
            if (!asset.empty())
            {
                // Frame content and masks may share the asset even when no
                // remaining layer has a matching static source.
                if (assetReferences(draft_, asset).empty() && !editor.removeAsset(asset).ok())
                    return false;
            }
            selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
            return true;
        });
}
bool Artwork::rename(std::string key, std::string name)
{
    return edit([&](DocumentEditor& e) { return e.setLayerName(key, std::move(name)).ok(); });
}
bool Artwork::visible(std::string key, bool value)
{
    return edit([&](DocumentEditor& e) { return e.setLayerVisible(key, value).ok(); });
}
bool Artwork::opacity(std::string key, double value)
{
    return edit([&](DocumentEditor& e)
                { return e.setLayerOpacity(key, std::clamp(value, 0., 1.)).ok(); });
}
bool Artwork::cycle_blend(std::string key)
{
    auto found = std::ranges::find_if(draft_.layers,
                                      [&](const auto& l) { return layerProperties(l).id == key; });
    if (found == draft_.layers.end())
        return fail("Layer not found");
    const auto mode = layerProperties(*found).blendMode;
    const auto next = mode == RasterBlendMode::SourceOver ? RasterBlendMode::Multiply
                      : mode == RasterBlendMode::Multiply ? RasterBlendMode::Screen
                                                          : RasterBlendMode::SourceOver;
    return edit([&](DocumentEditor& e) { return e.setLayerBlendMode(key, next).ok(); });
}
bool Artwork::timing(int rate, int frames)
{
    if (rate < 1 || rate > 240 || frames < 1)
        return fail("Invalid document timing");
    return edit(
        [&](DocumentEditor& e)
        {
            return e.setFrameRate({std::uint32_t(rate), 1}).ok() &&
                   e.setFrameCount(std::uint32_t(frames)).ok();
        });
}
bool Artwork::move(std::string key, std::string before)
{
    return edit(
        [&](DocumentEditor& e)
        {
            // The UI exposes the compositing stack from top to bottom.
            // "before" identifies the next row in that displayed order.
            std::size_t index = 0;
            if (!before.empty())
            {
                auto p = std::ranges::find_if(draft_.layers, [&](const auto& v)
                                              { return layerProperties(v).id == before; });
                if (p == draft_.layers.end())
                    return false;
                const auto source =
                    std::ranges::find_if(draft_.layers, [&](const auto& value)
                                         { return layerProperties(value).id == key; });
                if (source == draft_.layers.end())
                    return false;
                if (source == p)
                    return true;
                index = std::size_t(p - draft_.layers.begin()) + 1 - (source < p ? 1 : 0);
            }
            return e.moveLayer(key, index).ok();
        });
}
bool Artwork::transform(AffineTransform transform)
{
    return edit([&](DocumentEditor& e) { return e.setLayerTransform(selected_, transform).ok(); });
}
bool Artwork::transform(LayerTransform command)
{
    const auto* current = layer(draft_, selected_);
    if (!current)
        return fail("Layer not found");
    const auto old = layerProperties(*current).transform;
    auto next = old;
    if (command == LayerTransform::flip_horizontal)
    {
        next.m11 = -old.m11;
        next.m21 = -old.m21;
        next.translationX = draft_.extent.width - old.translationX;
    }
    else if (command == LayerTransform::flip_vertical)
    {
        next.m12 = -old.m12;
        next.m22 = -old.m22;
        next.translationY = draft_.extent.height - old.translationY;
    }
    else
    {
        const double direction = command == LayerTransform::rotate_left ? -1 : 1;
        next.m11 = -direction * old.m12;
        next.m21 = -direction * old.m22;
        next.m12 = direction * old.m11;
        next.m22 = direction * old.m21;
        const double x = draft_.extent.width / 2., y = draft_.extent.height / 2.;
        next.translationX = x - direction * (old.translationY - y);
        next.translationY = y + direction * (old.translationX - x);
    }
    return transform(next);
}
bool Artwork::import_pixels(RasterLayer pixels, std::string name, double x, double y)
{
    const auto asset = uuid(), key = uuid();
    return edit(
        [&](DocumentEditor& e)
        {
            if (!e.insertRasterAsset(asset, std::move(pixels)).ok())
                return false;
            LayerProperties properties{key, std::move(name)};
            properties.transform.translationX = x;
            properties.transform.translationY = y;
            if (!e.insertLayer(StaticBitmapLayer{std::move(properties), StaticSource{asset}}).ok())
                return false;
            selected_ = key;
            return true;
        });
}
bool Artwork::import_image(const std::filesystem::path& path, double x, double y)
{
    auto decoded = importBitmap(path_text(path));
    if (!decoded.ok())
        return fail(decoded.result.message);
    return import_pixels(std::move(decoded.asset.pixels), path_text(path.filename()), x, y);
}
bool Artwork::grow(DocumentPoint point, double radius)
{
    if (draft_.canvasMode != CanvasMode::Infinite || (selection_ && selection_->tiled))
        return true;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > 1e8 ||
        std::abs(point.y) > 1e8)
        return fail("Painting coordinates exceed the canvas range");
    DocumentEditor editor(draft_);
    auto result = editor.ensureInfiniteCanvasRegion(
        {{int(std::floor(point.x - radius)), int(std::floor(point.y - radius))},
         {int(std::ceil(radius * 2 + 2)), int(std::ceil(radius * 2 + 2))}});
    return result.ok() || fail(result.message);
}
bool Artwork::set_selection(SelectionGeometry shape, Point first, Point second, MaskOperation op, SelectionOptions options)
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (op!=MaskOperation::Replace && op!=MaskOperation::Add && op!=MaskOperation::Subtract && op!=MaskOperation::Intersect)
        return fail("Invalid selection operation");
    if (!std::isfinite(options.feather) || options.feather < 0 || options.feather > 256) return fail("Invalid selection feather radius");
    const auto origin = canvasOrigin(draft_);
    const bool tiled = draft_.canvasMode == CanvasMode::Infinite ||
        std::uint64_t(draft_.extent.width)*draft_.extent.height > 64ULL*1024*1024;
    try {
        if (tiled) {
            if (shape != SelectionGeometry::Rectangle && shape != SelectionGeometry::Ellipse)
                return fail("Unsupported tiled selection geometry");
            auto made = shape == SelectionGeometry::Rectangle ?
                TiledMask::rectangle({origin,draft_.extent},first,second,options.antialias) :
                TiledMask::ellipse({origin,draft_.extent},first,second,options.antialias);
            if (!made) return fail(made.error);
            if (options.feather>0) { made=made->feathered(options.feather);if(!made)return fail(made.error); }
            return combine_tiled_selection(std::move(*made),op);
        }
        if (selection_ && !selection_->dense) return fail("Selection representation does not match the canvas");
        first.x -= origin.x; first.y -= origin.y;
        second.x -= origin.x; second.y -= origin.y;
        auto made = selectionGeometry(draft_.extent, shape, first, second, options.antialias);
        if (!made.ok()) return fail(made.error);
        if (options.feather > 0) {
            made = featherMask(made.mask, options.feather);
            if (!made.ok()) return fail(made.error);
        }
        return combine_selection(std::move(made.mask), op);
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
bool Artwork::set_polygon_selection(std::span<const Point> points, MaskOperation op, SelectionOptions options)
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (!std::isfinite(options.feather) || options.feather < 0 || options.feather > 256) return fail("Invalid selection feather radius");
    if (points.size() > 65536) return fail("Selection path has too many points");
    try {
        const auto origin = canvasOrigin(draft_);
        const bool tiled=draft_.canvasMode == CanvasMode::Infinite ||
            std::uint64_t(draft_.extent.width)*draft_.extent.height > 64ULL*1024*1024;
        if (tiled) {
            auto made=TiledMask::polygon({origin,draft_.extent},points,options.antialias);
            if (!made) return fail(made.error);
            if (options.feather>0) { made=made->feathered(options.feather);if(!made)return fail(made.error); }
            return combine_tiled_selection(std::move(*made),op);
        }
        std::vector<Point> local; local.reserve(points.size());
        for (const auto& point : points) local.push_back({point.x - origin.x, point.y - origin.y});
        auto made = selectionPolygon(draft_.extent, local, options.antialias);
        if (!made.ok()) return fail(made.error);
        if (options.feather > 0) {
            made = featherMask(made.mask, options.feather);
            if (!made.ok()) return fail(made.error);
        }
        return combine_selection(std::move(made.mask), op);
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
bool Artwork::set_wand_selection(Point point, FrameIndex frame, double tolerance, bool contiguous, MaskOperation op, bool current_layer, SelectionOptions options)
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (frame >= draft_.timeline.frameCount) return fail("The current frame is outside the document timeline");
    if (!std::isfinite(options.feather) || options.feather < 0 || options.feather > 256) return fail("Invalid selection feather radius");
    const bool tiled=draft_.canvasMode==CanvasMode::Infinite ||
        std::uint64_t(draft_.extent.width)*draft_.extent.height>64ULL*1024*1024;
    if (tiled) {
        TiledMask mask;if(!tiled_wand_mask(point,frame,tolerance,contiguous,options.antialias,current_layer,mask))return false;
        if(options.feather>0) {auto softened=mask.feathered(options.feather);if(!softened)return fail(softened.error);mask=std::move(*softened);}
        return combine_tiled_selection(std::move(mask),op);
    }
    if (draft_.canvasMode != CanvasMode::Finite) return fail("Selection currently requires a finite canvas");
    const auto origin = canvasOrigin(draft_);
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < origin.x || point.y < origin.y ||
        point.x >= origin.x + draft_.extent.width || point.y >= origin.y + draft_.extent.height)
        return fail("Choose a point inside the canvas");
    if (std::uint64_t(draft_.extent.width) * draft_.extent.height > 64ULL * 1024 * 1024)
        return fail("Wand selection exceeds the 64 megapixel working budget");
    try {
        RasterLayer pixels;
        if (!reference_pixels(frame, current_layer, pixels)) return false;
        auto mask = selectionWand(pixels,
            {int(std::floor(point.x - origin.x)), int(std::floor(point.y - origin.y))}, tolerance, contiguous);
        if (!mask.ok()) return fail(mask.error);
        if(options.antialias) {mask=antialiasBinaryMask(mask.mask);if(!mask.ok())return fail(mask.error);}
        if(options.feather>0) {mask=featherMask(mask.mask,options.feather);if(!mask.ok())return fail(mask.error);}
        return combine_selection(std::move(mask.mask), op);
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for wand selection"); }
}
bool Artwork::sparse_current_wand(Point point,FrameIndex frame,double tolerance,TiledMask& output)
{
    const auto* selected=layer(draft_,selected_);if(!selected)return fail("Select a bitmap reference");
    const auto* asset=resolveAssetAt(draft_,*selected,frame);const auto* chunks=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;
    if(!chunks)return fail("Sparse reference requires a chunked bitmap");
    const auto found=std::find_if(draft_.layers.begin(),draft_.layers.end(),[&](const auto& l){return layerProperties(l).id==selected_;});
    const auto index=std::size_t(std::distance(draft_.layers.begin(),found));const auto origin=canvasOrigin(draft_);const CanvasRegion domain{origin,draft_.extent};
    const auto read=[&](CanvasRegion region)->std::optional<::RasterLayer>{
        auto rendered=renderFrameLayerTiles(draft_,frame,index,{{region,region.extent}});
        if(!rendered.ok() || !rendered.visible || !rendered.spatial || rendered.tiles.size()!=1)return std::nullopt;
        return std::move(rendered.tiles.front().pixels);
    };
    const CanvasRegion seedRegion{{int(std::floor(point.x)),int(std::floor(point.y))},{1,1}};
    auto seed=read(seedRegion);if(!seed)return fail("Cannot render the current layer reference");
    auto probe=makeRasterLayer(2,1,0);probe.pixels[0]=seed->pixels[0];auto outside=selectionWand(probe,{0,0},tolerance,false);
    if(!outside.ok())return fail(outside.error);const std::uint8_t background=outside.mask.alpha[1];
    RasterMask initial{{1,1},{background}};auto mask=TiledMask::fromRaster(domain,origin,initial,background);if(!mask)return fail(mask.error);
    std::vector<TiledMaskPatch> patches;patches.reserve(chunks->chunks.size());std::uint64_t total=0;
    const auto side=draft_.infiniteCanvas.chunkSize;
    for(const auto& chunk:chunks->chunks) {
        const auto left=std::max<std::int64_t>(origin.x,std::int64_t(chunk.column)*side),top=std::max<std::int64_t>(origin.y,std::int64_t(chunk.row)*side);
        const auto right=std::min(std::int64_t(origin.x)+draft_.extent.width,std::int64_t(chunk.column)*side+side),bottom=std::min(std::int64_t(origin.y)+draft_.extent.height,std::int64_t(chunk.row)*side+side);
        if(right<=left || bottom<=top)continue;
        const CanvasRegion region{{int(left),int(top)},{int(right-left),int(bottom-top)}};
        total+=std::uint64_t(region.extent.width)*region.extent.height;if(total>64ULL*1024*1024)return fail("Sparse current-layer reference exceeds the working budget");
        auto pixels=read(region);if(!pixels)return fail("Cannot render the current layer reference");
        auto comparison=makeRasterLayer(region.extent.width+1,region.extent.height,0);
        for(int y=0;y<region.extent.height;++y)std::copy_n(pixels->pixels.begin()+std::size_t(y)*region.extent.width,region.extent.width,comparison.pixels.begin()+std::size_t(y)*comparison.width);
        comparison.pixels[region.extent.width]=seed->pixels[0];
        auto matched=selectionWand(comparison,{region.extent.width,0},tolerance,false);if(!matched.ok())return fail(matched.error);
        RasterMask values{region.extent,std::vector<std::uint8_t>(std::size_t(region.extent.width)*region.extent.height)};
        for(int y=0;y<region.extent.height;++y)std::copy_n(matched.mask.alpha.begin()+std::size_t(y)*comparison.width,region.extent.width,values.alpha.begin()+std::size_t(y)*region.extent.width);
        patches.push_back({region.origin,std::move(values)});
    }
    auto patched=mask->patched(patches);if(!patched)return fail(patched.error);output=std::move(*patched);return true;
}

bool Artwork::tiled_wand_mask(Point point,FrameIndex frame,double tolerance,bool contiguous,bool antialias,bool current_layer,TiledMask& output)
{
    if (frame>=draft_.timeline.frameCount) return fail("The current frame is outside the document timeline");
    if (!std::isfinite(tolerance) || tolerance<0 || tolerance>1) return fail("Invalid wand tolerance");
    const auto origin=canvasOrigin(draft_);const CanvasRegion domain{origin,draft_.extent};
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x<origin.x || point.y<origin.y ||
        point.x>=double(origin.x)+draft_.extent.width || point.y>=double(origin.y)+draft_.extent.height)
        return fail("Choose a point inside the canvas");
    try {
        // Noncontiguous matching of widely separated, untransformed chunks can
        // be evaluated independently without rendering their empty gap.
        if(current_layer && !contiguous && !antialias && draft_.canvasMode==CanvasMode::Finite) {
            const auto* item=layer(draft_,selected_);const auto* asset=item?resolveAssetAt(draft_,*item,frame):nullptr;
            const auto* chunked=asset?std::get_if<ChunkedRasterAsset>(asset):nullptr;
            if(item && chunked && layerProperties(*item).appearance.filters.empty()) {
                const auto sample=sampleLayerAt(draft_,*item,frame);const auto& t=sample.transform;
                if(sample.visible && t.m11==1 && t.m12==0 && t.m21==0 && t.m22==1 && t.translationX==0 && t.translationY==0) {
                    ReferenceBounds bounds;
                    if(includeReferenceAsset(*asset,draft_,t,bounds) && bounds.any &&
                       (bounds.right-bounds.left+2)*(bounds.bottom-bounds.top+2)>double(64ULL*1024*1024))
                        return sparse_current_wand(point,frame,tolerance,output);
                }
            }
        }
        CanvasRegion region;RasterLayer reference;std::uint32_t exterior=0;
        if (!(current_layer ? bounded_current_reference(frame,region,reference) : bounded_composite_reference(frame,region,reference,exterior))) return false;
        const auto left=std::int64_t(region.origin.x),top=std::int64_t(region.origin.y);
        const auto right=left+region.extent.width,bottom=top+region.extent.height;
        const auto x=std::clamp<std::int64_t>(std::int64_t(std::floor(point.x)),left,right-1);
        const auto y=std::clamp<std::int64_t>(std::int64_t(std::floor(point.y)),top,bottom-1);
        const CanvasOrigin seed{int(x-left),int(y-top)};
        auto selected=selectionWand(reference,seed,tolerance,contiguous);
        if (!selected.ok()) return fail(selected.error);
        const bool gapLeft=left>origin.x,gapTop=top>origin.y;
        const bool gapRight=right<std::int64_t(origin.x)+draft_.extent.width;
        const bool gapBottom=bottom<std::int64_t(origin.y)+draft_.extent.height;
        // Every unrendered pixel must have the same appearance as the halo.
        // Fully transparent pixels are equivalent regardless of hidden RGB.
        const auto matchesExterior=[&](int col,int row){const auto pixel=reference.pixels[std::size_t(row)*region.extent.width+col];return pixel==exterior || (!(pixel>>24) && !(exterior>>24));};
        if (gapLeft || gapRight) for(int row=0;row<region.extent.height;++row)
            if ((gapLeft && !matchesExterior(0,row)) || (gapRight && !matchesExterior(region.extent.width-1,row)))
                return fail("Reference has no uniform exterior halo");
        if (gapTop || gapBottom) for(int col=0;col<region.extent.width;++col)
            if ((gapTop && !matchesExterior(col,0)) || (gapBottom && !matchesExterior(col,region.extent.height-1)))
                return fail("Reference has no uniform exterior halo");
        bool reachesOutside=false;
        if (!contiguous) {
            auto probe=makeRasterLayer(2,1,0);probe.pixels[0]=reference.pixels[std::size_t(seed.y)*region.extent.width+seed.x];probe.pixels[1]=exterior;
            const auto matching=selectionWand(probe,{0,0},tolerance,false);
            if(!matching.ok())return fail(matching.error);
            reachesOutside=(gapLeft||gapRight||gapTop||gapBottom) && matching.mask.alpha[1]!=0;
        } else {
            const auto& a=selected.mask.alpha;const int w=region.extent.width,h=region.extent.height;
            if (gapLeft || gapRight) for(int row=0;row<h && !reachesOutside;++row)
                reachesOutside=(gapLeft && a[std::size_t(row)*w]) || (gapRight && a[std::size_t(row)*w+w-1]);
            if (gapTop || gapBottom) for(int col=0;col<w && !reachesOutside;++col)
                reachesOutside=(gapTop && a[col]) || (gapBottom && a[std::size_t(h-1)*w+col]);
            if (reachesOutside && ((gapLeft && gapRight && !gapTop && !gapBottom) ||
                                   (gapTop && gapBottom && !gapLeft && !gapRight)))
                return fail("Tiled wand cannot safely join disconnected exterior regions");
        }
        if (antialias) {
            auto softened=antialiasBinaryMask(selected.mask);
            if (!softened.ok()) return fail(softened.error);
            selected=std::move(softened);
        }
        auto mask=TiledMask::fromRaster(domain,region.origin,selected.mask,reachesOutside?255:0);
        if (!mask) return fail(mask.error);
        output=std::move(*mask);return true;
    } catch(const std::bad_alloc&) { return fail("Insufficient memory for wand selection"); }
}
bool Artwork::bounded_current_reference(FrameIndex frame,CanvasRegion& region,::RasterLayer& pixels)
{
    const auto* selected=layer(draft_,selected_);
    if (!selected || (!std::holds_alternative<StaticBitmapLayer>(*selected) && !std::holds_alternative<DynamicBitmapLayer>(*selected)))
        return fail("Select a bitmap layer for the reference");
    const auto sample=sampleLayerAt(draft_,*selected,frame);
    if (!sample.visible || !localPoint({0,0},sample.transform)) return fail("The reference layer is hidden or has a degenerate transform");
    const auto* resolved=resolveAssetAt(draft_,*selected,frame);
    if(!resolved || !layerProperties(*selected).appearance.filters.empty())return fail("The reference requires an unfiltered raster layer");
    ReferenceBounds bounds;if(!includeReferenceAsset(*resolved,draft_,sample.transform,bounds))return fail("Current-layer reference has an unsupported or oversized asset");
    const auto origin=canvasOrigin(draft_);
    if(!bounds.any) {region={origin,{1,1}};pixels=makeRasterLayer(1,1,0);return true;}
    double left=bounds.left,top=bounds.top,right=bounds.right,bottom=bounds.bottom;
    // One transparent pixel on each available side preserves exterior connectivity.
    left=std::clamp(std::floor(left)-1.,double(origin.x),double(origin.x)+draft_.extent.width);
    top=std::clamp(std::floor(top)-1.,double(origin.y),double(origin.y)+draft_.extent.height);
    right=std::clamp(std::ceil(right)+1.,double(origin.x),double(origin.x)+draft_.extent.width);
    bottom=std::clamp(std::ceil(bottom)+1.,double(origin.y),double(origin.y)+draft_.extent.height);
    if(right<=left || bottom<=top)return fail("The current layer reference is outside the canvas");
    region={{int(left),int(top)},{int(right-left),int(bottom-top)}};
    if(std::uint64_t(region.extent.width)*region.extent.height>64ULL*1024*1024)
        return fail("Current-layer reference exceeds the working budget");
    const auto found=std::find_if(draft_.layers.begin(),draft_.layers.end(),[&](const auto& l){return layerProperties(l).id==selected_;});
    const auto index=std::size_t(std::distance(draft_.layers.begin(),found));
    auto rendered=renderFrameLayerTiles(draft_,frame,index,{{region,region.extent}});
    if(!rendered.ok() || !rendered.visible || !rendered.spatial || rendered.tiles.size()!=1)
        return fail(rendered.message.empty()?"Cannot render the current layer reference":rendered.message);
    pixels=std::move(rendered.tiles.front().pixels);return true;
}
bool Artwork::bounded_composite_reference(FrameIndex frame,CanvasRegion& region,::RasterLayer& pixels,std::uint32_t& exterior)
{
    if(!draft_.artboards.empty())return fail("Composite reference does not yet support artboards");
    const auto origin=canvasOrigin(draft_);ReferenceBounds bounds;exterior=0;bool seenVisible=false;
    for(const auto& item:draft_.layers) {
        if(!layerExistsAt(draft_,item,frame))continue;
        const auto sample=sampleLayerAt(draft_,item,frame);if(!sample.visible)continue;
        if(!std::holds_alternative<StaticBitmapLayer>(item) && !std::holds_alternative<DynamicBitmapLayer>(item)) {
            const auto background=seenVisible ? std::optional<std::uint32_t>{} : uniformCanvasBackground(draft_,item,frame);
            if(!background)return fail("Composite reference requires bitmap layers over a uniform background");
            exterior=*background;seenVisible=true;continue;
        }
        seenVisible=true;
        if(!layerProperties(item).appearance.filters.empty() || !localPoint({0,0},sample.transform))
            return fail("Composite reference requires unfiltered, nondegenerate bitmap layers");
        const auto* resolved=resolveAssetAt(draft_,item,frame);
        if(!resolved || !includeReferenceAsset(*resolved,draft_,sample.transform,bounds))
            return fail("Composite reference has an unsupported or oversized asset");
    }
    bool any=bounds.any;double left=bounds.left,top=bounds.top,right=bounds.right,bottom=bounds.bottom;
    if(any) {
        left=std::clamp(std::floor(left)-2.,double(origin.x),double(origin.x)+draft_.extent.width);
        top=std::clamp(std::floor(top)-2.,double(origin.y),double(origin.y)+draft_.extent.height);
        right=std::clamp(std::ceil(right)+2.,double(origin.x),double(origin.x)+draft_.extent.width);
        bottom=std::clamp(std::ceil(bottom)+2.,double(origin.y),double(origin.y)+draft_.extent.height);
        any=right>left && bottom>top;
    }
    if(any)region={{int(left),int(top)},{int(right-left),int(bottom-top)}};
    else region={origin,{1,1}};
    if(std::uint64_t(region.extent.width)*region.extent.height>64ULL*1024*1024)
        return fail("Composite reference exceeds the working budget");
    auto rendered=renderFrameRegion(draft_,frame,region,region.extent);
    if(!rendered.ok())return fail(rendered.message.empty()?"Cannot render the composite reference":rendered.message);
    pixels=std::move(rendered.pixels);return true;
}
bool Artwork::reference_pixels(FrameIndex frame, bool current_layer, RasterLayer& pixels)
{
    if (current_layer) {
        auto found = std::find_if(draft_.layers.begin(), draft_.layers.end(),
                                  [&](const auto& value) { return layerProperties(value).id == selected_; });
        if (found == draft_.layers.end()) return fail("Select a layer to use as the reference");
        const auto index = std::size_t(std::distance(draft_.layers.begin(), found));
        const auto origin = canvasOrigin(draft_);
        auto rendered = renderFrameLayerTiles(draft_, frame, index, {{{origin, draft_.extent}, draft_.extent}});
        if (!rendered.ok()) return fail(rendered.message);
        if (!rendered.visible || !rendered.spatial || rendered.tiles.size() != 1)
            return fail("The selected layer is not a visible image reference");
        pixels = std::move(rendered.tiles.front().pixels);
    } else {
        auto rendered = renderFrame(draft_, frame);
        if (!rendered.ok()) return fail(rendered.message);
        pixels = std::move(rendered.pixels);
    }
    return true;
}
bool Artwork::clone_target_ready(FrameIndex frame)
{
    const auto* selected=layer(draft_,selected_);const auto* asset=selected?resolveAssetAt(draft_,*selected,frame):nullptr;
    const auto* raster=asset?std::get_if<RasterAsset>(asset):nullptr;
    if(!selected || !raster || (!std::holds_alternative<StaticBitmapLayer>(*selected) && !std::holds_alternative<DynamicBitmapLayer>(*selected)))return fail("Clone target requires an editable raster layer");
    const auto sample=sampleLayerAt(draft_,*selected,frame);const auto& t=sample.transform;
    if(!sample.visible || t.m11!=1 || t.m12!=0 || t.m21!=0 || t.m22!=1 || t.translationX!=0 || t.translationY!=0 || !layerProperties(*selected).appearance.filters.empty())
        return fail("Clone target requires a visible, untransformed raster layer without filters");
    return true;
}
bool Artwork::clone_reference(FrameIndex frame,bool current_layer,::RasterLayer& pixels)
{
    if(stroke_active())return fail("Finish the current stroke before sampling");
    if(frame>=draft_.timeline.frameCount || draft_.canvasMode!=CanvasMode::Finite ||
       std::uint64_t(draft_.extent.width)*draft_.extent.height>16ULL*1024*1024)
        return fail("Clone source requires a finite canvas within the working budget");
    if(!clone_target_ready(frame))return false;
    try {if(!reference_pixels(frame,current_layer,pixels))return false;error_.clear();return true;}
    catch(const std::bad_alloc&){return fail("Insufficient memory for clone source");}
}
std::optional<SelectionPreview> Artwork::clone_preview(const ::RasterLayer& source,const ::RasterLayer& stroke,CanvasOrigin offset,FrameIndex frame,::DevicePixelRect dirty) const
{
    const auto extent=draft_.extent;const auto count=std::uint64_t(extent.width)*extent.height;
    if(frame>=draft_.timeline.frameCount || draft_.canvasMode!=CanvasMode::Finite || count>16ULL*1024*1024 ||
       source.width!=extent.width || source.height!=extent.height || source.pixels.size()!=count ||
       stroke.width!=extent.width || stroke.height!=extent.height || stroke.pixels.size()!=count)return std::nullopt;
    const auto* selected=layer(draft_,selected_);const auto* asset=selected?resolveAssetAt(draft_,*selected,frame):nullptr;
    const auto* raster=asset?std::get_if<RasterAsset>(asset):nullptr;if(!selected || !raster)return std::nullopt;
    const auto sample=sampleLayerAt(draft_,*selected,frame);const auto& t=sample.transform;
    if(!sample.visible || t.m11!=1 || t.m12!=0 || t.m21!=0 || t.m22!=1 || t.translationX!=0 || t.translationY!=0 || !layerProperties(*selected).appearance.filters.empty())return std::nullopt;
    constexpr int side=BitmapPreviewTileSide;
    const auto left=std::max(0,dirty.origin.x),top=std::max(0,dirty.origin.y);
    const auto right=std::min<std::int64_t>({extent.width,raster->pixels.width,std::int64_t(dirty.origin.x)+dirty.width});
    const auto bottom=std::min<std::int64_t>({extent.height,raster->pixels.height,std::int64_t(dirty.origin.y)+dirty.height});
    SelectionPreview preview;preview.asset_id=raster->id;if(right<=left || bottom<=top)return preview;
    const auto x0=(left/side)*side,y0=(top/side)*side;const auto tiles=std::uint64_t((right-x0+side-1)/side)*std::uint64_t((bottom-y0+side-1)/side);
    if(tiles>1024)return std::nullopt;
    try {
        preview.pixels.reserve(std::size_t(tiles));std::vector<DevicePixelRect> regions;regions.reserve(std::size_t(tiles));
        for(int y=y0;y<bottom;y+=side)for(int x=x0;x<right;x+=side) {
            std::vector<std::uint32_t> pixels(side*side,0);bool changed=false;
            for(int dy=0;dy<side;++dy)for(int dx=0;dx<side;++dx) {
                const int px=x+dx,py=y+dy;if(px>=raster->pixels.width || py>=raster->pixels.height || px>=extent.width || py>=extent.height)continue;
                auto& value=pixels[std::size_t(dy)*side+dx];value=raster->pixels.pixels[std::size_t(py)*raster->pixels.width+px];
                const auto sx=std::int64_t(px)+offset.x,sy=std::int64_t(py)+offset.y;if(sx<0 || sy<0 || sx>=extent.width || sy>=extent.height)continue;
                const auto sourcePixel=source.pixels[std::size_t(sy)*extent.width+std::size_t(sx)];
                const auto mask=stroke.pixels[std::size_t(py)*extent.width+px]>>24;
                const auto alpha=(unsigned(sourcePixel>>24)*mask*selection_coverage(px+.5,py+.5)+32512)/65025;if(!alpha)continue;
                const auto next=compositeRasterPixel((sourcePixel&0xffffffU)|(alpha<<24),value,RasterBlendMode::SourceOver);changed|=next!=value;value=next;
            }
            if(changed){preview.pixels.push_back(std::move(pixels));regions.push_back({{x,y},side,side});}
        }
        preview.tiles.reserve(regions.size());for(std::size_t i=0;i<regions.size();++i)preview.tiles.push_back({regions[i],preview.pixels[i]});
        return preview;
    } catch(const std::bad_alloc&){return std::nullopt;}
}

bool Artwork::clone_stamp(const ::RasterLayer& source,const RasterMask& stroke,CanvasOrigin offset,FrameIndex frame)
{
    if(stroke_active())return fail("Finish the current stroke before cloning");
    const auto extent=draft_.extent;const auto count=std::uint64_t(extent.width)*extent.height;
    if(frame>=draft_.timeline.frameCount || draft_.canvasMode!=CanvasMode::Finite || count>16ULL*1024*1024 ||
       source.width!=extent.width || source.height!=extent.height || source.pixels.size()!=count ||
       stroke.extent!=extent || stroke.alpha.size()!=count)return fail("Invalid clone source or stroke mask");
    if(selection_ && ((selection_->dense && selection_->dense->extent!=extent) ||
       (selection_->tiled && selection_->tiled->domain()!=CanvasRegion{{0,0},extent})))return fail("Selection no longer matches the canvas");
    if(!clone_target_ready(frame))return false;
    const auto* selected=layer(draft_,selected_);const auto* raster=std::get_if<RasterAsset>(resolveAssetAt(draft_,*selected,frame));
    try {
        auto output=raster->pixels;bool changed=false;
        for(int y=0;y<output.height && y<extent.height;++y)for(int x=0;x<output.width && x<extent.width;++x) {
            const auto i=std::size_t(y)*extent.width+x;const auto sx=std::int64_t(x)+offset.x,sy=std::int64_t(y)+offset.y;
            if(!stroke.alpha[i] || sx<0 || sy<0 || sx>=extent.width || sy>=extent.height)continue;
            const auto sourcePixel=source.pixels[std::size_t(sy)*extent.width+std::size_t(sx)];
            const auto alpha=(unsigned(sourcePixel>>24)*unsigned(stroke.alpha[i])*selection_coverage(x+.5,y+.5)+32512)/65025;
            if(!alpha)continue;auto& destination=output.pixels[std::size_t(y)*output.width+x];
            const auto value=compositeRasterPixel((sourcePixel&0xffffffU)|(alpha<<24),destination,RasterBlendMode::SourceOver);
            if(value!=destination){destination=value;changed=true;}
        }
        if(!changed){error_.clear();return true;}
        if(capture().empty())return fail("Clone exceeds the undo budget");
        return edit([&](DocumentEditor& editor){std::string replacement;return replace_frame_bitmap(editor,frame,std::move(output),replacement) && !capture().empty();});
    } catch(const std::bad_alloc&){return fail("Insufficient memory for clone stroke");}
}

bool Artwork::combine_tiled_selection(TiledMask mask, MaskOperation op)
{
    if (op!=MaskOperation::Replace && op!=MaskOperation::Add && op!=MaskOperation::Subtract && op!=MaskOperation::Intersect)
        return fail("Invalid selection operation");
    try {
        if (op!=MaskOperation::Replace && selection_) {
            if (!selection_->tiled) return fail("Selection representation does not match the canvas");
            auto combined=selection_->tiled->combine(mask,op);
            if (!combined) return fail(combined.error);
            mask=std::move(*combined);
        } else if ((op==MaskOperation::Subtract || op==MaskOperation::Intersect) && !selection_) return true;
        auto value=std::make_shared<SelectionValue>();value->tiled=std::make_shared<TiledMask>(std::move(mask));
        return change_selection_value(std::move(value));
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
bool Artwork::combine_selection(RasterMask mask, MaskOperation op)
{
    if (op!=MaskOperation::Replace && op!=MaskOperation::Add && op!=MaskOperation::Subtract && op!=MaskOperation::Intersect)
        return fail("Invalid selection operation");
    try {
        if (selection_ && !selection_->dense) return fail("Tiled selection is not supported by this operation");
        if (op != MaskOperation::Replace && selection_) {
            auto combined = combineMasks(*selection_->dense, mask, op);
            if (!combined.ok()) return fail(combined.error);
            mask = std::move(combined.mask);
        } else if ((op == MaskOperation::Subtract || op == MaskOperation::Intersect) && !selection_)
            return true;
        return change_selection(std::make_shared<RasterMask>(std::move(mask)));
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
bool Artwork::change_selection(std::shared_ptr<const RasterMask> next)
{
    try {
        if (!next) return change_selection_value({});
        auto value=std::make_shared<SelectionValue>();value->dense=std::move(next);
        return change_selection_value(std::move(value));
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
bool Artwork::change_selection_value(std::shared_ptr<const SelectionValue> next)
{
    if ((!next && !selection_) || (next && selection_ && next->dense && selection_->dense &&
        next->dense->extent == selection_->dense->extent && next->dense->alpha == selection_->dense->alpha) ||
        (next && selection_ && next->tiled && selection_->tiled && next->tiled->equivalent(*selection_->tiled))) { error_.clear(); return true; }
    History entry; entry.mask_change = true; entry.mask_before = selection_; entry.mask_after = next;
    const auto bytes = entry.bytes();
    if (bytes > history_budget_) return fail("Selection exceeds the history budget");
    try { undo_.push_back(std::move(entry)); }
    catch (const std::bad_alloc&) { return fail("Insufficient memory for selection history"); }
    for (const auto& item : redo_) history_bytes_ -= item.bytes();
    redo_.clear(); history_bytes_ += bytes;
    while (history_bytes_ > history_budget_ || undo_.size() > HistoryCount) {
        history_bytes_ -= undo_.front().bytes(); undo_.pop_front();
    }
    selection_ = std::move(next);
    error_.clear();
    return true;
}
bool Artwork::apply_selection_mask(RasterMask mask)
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (draft_.canvasMode != CanvasMode::Finite || mask.extent != draft_.extent ||
        mask.extent.width <= 0 || mask.extent.height <= 0 ||
        std::uint64_t(mask.extent.width)*mask.extent.height > 64ULL*1024*1024 ||
        mask.alpha.size() != std::size_t(mask.extent.width)*mask.extent.height)
        return fail("Quick mask no longer matches the finite canvas");
    return combine_selection(std::move(mask), MaskOperation::Replace);
}
bool Artwork::apply_tiled_selection_mask(TiledMask mask)
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (!mask || mask.domain()!=CanvasRegion{canvasOrigin(draft_),draft_.extent} ||
        (draft_.canvasMode!=CanvasMode::Infinite && std::uint64_t(draft_.extent.width)*draft_.extent.height<=64ULL*1024*1024))
        return fail("Quick mask no longer matches the tiled canvas");
    return combine_tiled_selection(std::move(mask),MaskOperation::Replace);
}
bool Artwork::reframe_canvas(CanvasRegion region)
{
    if (stroke_active()) return fail("Finish the current stroke before cropping");
    const auto& extent=draft_.extent;const auto x=region.origin.x,y=region.origin.y,w=region.extent.width,h=region.extent.height;
    if (draft_.canvasMode!=CanvasMode::Finite || x<0 || y<0 || w<=0 || h<=0 ||
        std::int64_t(x)+w>extent.width || std::int64_t(y)+h>extent.height)
        return fail("Crop region must lie within a finite canvas");
    if (x==0 && y==0 && region.extent==extent) {error_.clear();return true;}
    auto before=capture();if(before.empty())return fail("Crop exceeds the undo budget");
    try {
        std::optional<SelectionTransition> selection;
        if (selection_) {
            const auto pixels=std::uint64_t(w)*h;
            auto value=std::make_shared<SelectionValue>();
            if(selection_->tiled) {
                if(selection_->tiled->domain()!=CanvasRegion{canvasOrigin(draft_),extent})
                    return fail("Selection no longer matches the canvas");
                if(pixels<=64ULL*1024*1024) {
                    if(pixels>history_budget_ || selection_->bytes()>history_budget_-pixels)
                        return fail("Crop exceeds the selection history budget");
                    auto next=selection_->tiled->materialize(region);
                    if(!next)return fail(next.error());
                    value->dense=std::make_shared<RasterMask>(std::move(*next));
                } else {
                    auto next=selection_->tiled->reframed(region,{0,0});
                    if(!next)return fail(next.error);
                    value->tiled=std::make_shared<TiledMask>(std::move(*next));
                }
            } else {
                if(!selection_->dense || selection_->dense->extent!=extent || selection_->dense->alpha.size()!=std::size_t(extent.width)*extent.height)
                    return fail("Selection no longer matches the canvas");
                auto next=std::make_shared<RasterMask>(RasterMask{region.extent,std::vector<std::uint8_t>(std::size_t(w)*h)});
                for(int row=0;row<h;++row) std::copy_n(selection_->dense->alpha.begin()+std::size_t(y+row)*extent.width+x,w,next->alpha.begin()+std::size_t(row)*w);
                value->dense=std::move(next);
            }
            selection=SelectionTransition{selection_,std::move(value)};
        }
        DocumentEditor editor(draft_);auto result=editor.reframeCanvas(region);
        if (!result.ok()) return fail(result.message);
        if (!result.changed) {error_.clear();return true;}
        const auto after=capture();const std::size_t maskBytes=selection ? selection->before->bytes()+selection->after->bytes() : 0;
        if (after.empty() || before.size()>history_budget_ || after.size()>history_budget_-before.size() ||
            maskBytes>history_budget_-before.size()-after.size()) {
            draft_=*file_.document();bind();return fail("Crop exceeds the undo budget");
        }
        return commit(std::move(before),selected_,true,std::move(selection));
    } catch(const std::bad_alloc&) {draft_=*file_.document();bind();return fail("Insufficient memory for crop");}
}
bool Artwork::clear_selection()
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    return change_selection({});
}
bool Artwork::invert_selection()
{
    if (stroke_active()) return fail("Finish the current stroke before changing selection");
    if (!selection_) return true;
    try {
        if (selection_->tiled) {
            if (selection_->tiled->domain() != CanvasRegion{canvasOrigin(draft_),draft_.extent}) return fail("Selection no longer matches the canvas");
            auto inverted=selection_->tiled->inverted();if(!inverted)return fail(inverted.error);
            auto value=std::make_shared<SelectionValue>();value->tiled=std::make_shared<TiledMask>(std::move(*inverted));
            return change_selection_value(std::move(value));
        }
        if (draft_.canvasMode != CanvasMode::Finite || !selection_->dense || selection_->dense->extent != draft_.extent)
            return fail("Selection no longer matches the canvas");
        auto inverted = std::make_shared<RasterMask>(*selection_->dense);
        for (auto& alpha : inverted->alpha) alpha = 255 - alpha;
        return change_selection(std::move(inverted));
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for selection"); }
}
std::uint8_t Artwork::selection_coverage(double x,double y) const noexcept
{
    if (!selection_) return 255;
    if (!std::isfinite(x) || !std::isfinite(y)) return 0;
    if (selection_->tiled) {
        if (x<std::numeric_limits<std::int32_t>::min() || y<std::numeric_limits<std::int32_t>::min() ||
            x>=double(std::numeric_limits<std::int32_t>::max())+1 || y>=double(std::numeric_limits<std::int32_t>::max())+1) return 0;
        return selection_->tiled->coverage(std::int32_t(std::floor(x)),std::int32_t(std::floor(y)));
    }
    if (!selection_->dense) return 0;
    const auto origin=canvasOrigin(draft_);x-=origin.x;y-=origin.y;
    if (x<0 || y<0 || x>=selection_->dense->extent.width || y>=selection_->dense->extent.height) return 0;
    return selection_->dense->alpha[std::size_t(y)*selection_->dense->extent.width+std::size_t(x)];
}
std::optional<CanvasRegion> Artwork::selection_bounds() const noexcept
{
    if (!selection_) return std::nullopt;
    if (selection_->tiled) return selection_->tiled->nonzeroBounds();
    if (!selection_->dense) return std::nullopt;
    const auto& mask=*selection_->dense;
    const int width=mask.extent.width,height=mask.extent.height;
    if (width<=0 || height<=0 || std::uint64_t(width)*height!=mask.alpha.size()) return std::nullopt;
    int left=width,top=height,right=0,bottom=0;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        if(mask.alpha[std::size_t(y)*width+x]) {left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);}
    if(right<=left || bottom<=top) return std::nullopt;
    const auto origin=canvasOrigin(draft_);
    return CanvasRegion{{origin.x+left,origin.y+top},{right-left,bottom-top}};
}
std::uint8_t Artwork::stroke_chunk_coverage(double x,double y) const noexcept
{
    const auto& t=stroke_transform_;const double wx=t.m11*x+t.m21*y+t.translationX,wy=t.m12*x+t.m22*y+t.translationY;
    const auto origin=canvasOrigin(draft_);
    if(!std::isfinite(wx)||!std::isfinite(wy)||wx<origin.x||wy<origin.y||
       wx>=double(origin.x)+draft_.extent.width||wy>=double(origin.y)+draft_.extent.height)return 0;
    return selection_coverage(wx,wy);
}

void Artwork::record_stroke_damage(DevicePixelRect bounds) noexcept
{
    if (bounds.width <= 0 || bounds.height <= 0) return;
    stroke_dirty_total_ = unionStrokeDamage(stroke_dirty_total_, bounds);
    stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, bounds);
    if (stroke_tiles_.empty() || !stroke_tile_columns_) return;
    const auto* pixels = raster_.pixels();
    const int width = pixels ? pixels->width : chunks_.isBound() ? draft_.extent.width : 0;
    const int height = pixels ? pixels->height : chunks_.isBound() ? draft_.extent.height : 0;
    const auto left = std::max<std::int64_t>(0, bounds.origin.x);
    const auto top = std::max<std::int64_t>(0, bounds.origin.y);
    const auto right = std::min<std::int64_t>(width, std::int64_t(bounds.origin.x) + bounds.width);
    const auto bottom = std::min<std::int64_t>(height, std::int64_t(bounds.origin.y) + bounds.height);
    if (right <= left || bottom <= top) return;
    constexpr int side = BitmapPreviewTileSide;
    for (auto y = top / side; y <= (bottom - 1) / side; ++y)
        for (auto x = left / side; x <= (right - 1) / side; ++x)
            stroke_tiles_[std::size_t(y) * stroke_tile_columns_ + std::size_t(x)] = 1;
}

void Artwork::collect_stroke_damage() noexcept
{
    if (raster_.isBound()) {
        record_stroke_damage(raster_.dirtyBounds());
        raster_.clearDirtyBounds();
    } else if (chunks_.isBound()) {
        record_stroke_damage(chunks_.dirtyBounds());
        chunks_.clearDirtyBounds();
    }
}

std::optional<StrokeDamage> Artwork::take_stroke_damage() noexcept
{
    const auto bounds = std::exchange(stroke_dirty_pending_, DevicePixelRect{});
    if (bounds.width <= 0 || bounds.height <= 0) return std::nullopt;
    // Smooth sampling reads adjacent source pixels; transform their expanded
    // rectangle instead of assuming that a layer is translated or unscaled.
    const double left = double(bounds.origin.x) - 1, top = double(bounds.origin.y) - 1;
    const double right = double(bounds.origin.x) + bounds.width + 1;
    const double bottom = double(bounds.origin.y) + bounds.height + 1;
    const auto& t = stroke_transform_;
    const auto map = [&](double x, double y) {
        return Point{t.m11*x + t.m21*y + t.translationX,
                     t.m12*x + t.m22*y + t.translationY};
    };
    const std::array points{map(left,top), map(right,top), map(left,bottom), map(right,bottom)};
    StrokeDamage result{points[0].x, points[0].y, points[0].x, points[0].y};
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            const auto infinity = std::numeric_limits<double>::infinity();
            return StrokeDamage{-infinity, -infinity, infinity, infinity};
        }
        result.left = std::min(result.left, point.x);
        result.top = std::min(result.top, point.y);
        result.right = std::max(result.right, point.x);
        result.bottom = std::max(result.bottom, point.y);
    }
    --result.left; --result.top; ++result.right; ++result.bottom;
    return result;
}

std::optional<SelectionPreview> Artwork::selection_preview(std::optional<CanvasRegion> document_region) const
{
    if (!selection_ && !stroke_asset_region_) return std::nullopt;
    std::optional<CanvasRegion> local_region;
    if (document_region) {
        if (const auto local = assetBoundsForCanvas(*document_region, stroke_transform_)) {
            // The compositor's smooth sampler can read beyond the nominal
            // document rectangle, including across a transformed tile edge.
            const auto left = std::max<std::int64_t>(-1000000000, std::int64_t(local->origin.x) - 2);
            const auto top = std::max<std::int64_t>(-1000000000, std::int64_t(local->origin.y) - 2);
            const auto right = std::min<std::int64_t>(1000000000, std::int64_t(local->origin.x) + local->extent.width + 2);
            const auto bottom = std::min<std::int64_t>(1000000000, std::int64_t(local->origin.y) + local->extent.height + 2);
            if (right > left && bottom > top)
                local_region = CanvasRegion{{int(left), int(top)}, {int(right-left), int(bottom-top)}};
        }
    }
    const auto restrict_to_requested = [&](std::int64_t& left, std::int64_t& top,
                                           std::int64_t& right, std::int64_t& bottom) {
        if (!local_region) return;
        left = std::max(left, std::int64_t(local_region->origin.x));
        top = std::max(top, std::int64_t(local_region->origin.y));
        right = std::min(right, std::int64_t(local_region->origin.x) + local_region->extent.width);
        bottom = std::min(bottom, std::int64_t(local_region->origin.y) + local_region->extent.height);
    };
    if((stroke_original_chunks_ || stroke_compact_) && chunks_.strokeActive()) {
        try {
            const auto* original=stroke_original_chunks_ ? &*stroke_original_chunks_ :
                (file_.document() ? findChunkedRasterAsset(*file_.document(),stroke_frame_history_ ? stroke_frame_history_->prior : chunks_.boundAssetId()) : nullptr);
            if(!original)return std::nullopt;
            const auto bounds=stroke_dirty_total_;const auto clip=stroke_asset_region_.value_or(CanvasRegion{canvasOrigin(draft_),draft_.extent});const int side=BitmapPreviewTileSide,size=draft_.infiniteCanvas.chunkSize;
            auto left=std::max<std::int64_t>(bounds.origin.x,clip.origin.x),top=std::max<std::int64_t>(bounds.origin.y,clip.origin.y);
            auto right=std::min(std::int64_t(bounds.origin.x)+bounds.width,std::int64_t(clip.origin.x)+clip.extent.width);
            auto bottom=std::min(std::int64_t(bounds.origin.y)+bounds.height,std::int64_t(clip.origin.y)+clip.extent.height);
            restrict_to_requested(left,top,right,bottom);
            SelectionPreview preview;preview.asset_id=chunks_.boundAssetId();
            if(right<=left||bottom<=top)return std::optional<SelectionPreview>(std::move(preview));
            const auto floorDiv=[](std::int64_t value,int divisor){return value>=0?value/divisor:(value-divisor+1)/divisor;};
            const auto x0=floorDiv(left,side),y0=floorDiv(top,side),x1=floorDiv(right-1,side),y1=floorDiv(bottom-1,side);
            const auto count=std::uint64_t(x1-x0+1)*std::uint64_t(y1-y0+1);if(count>1024)return std::nullopt;
            preview.pixels.reserve(std::size_t(count));std::vector<DevicePixelRect> regions;regions.reserve(std::size_t(count));
            for(auto ty=y0;ty<=y1;++ty)for(auto tx=x0;tx<=x1;++tx) {
                const auto px=tx*side,py=ty*side;auto before=makeRasterLayer(side,side,0),after=makeRasterLayer(side,side,0);
                RasterMask mask{{side,side},std::vector<std::uint8_t>(std::size_t(side)*side,0)};
                for(int dy=0;dy<side;++dy)for(int dx=0;dx<side;++dx) {
                    const auto x=px+dx,y=py+dy;if(x<clip.origin.x||y<clip.origin.y||x>=std::int64_t(clip.origin.x)+clip.extent.width||y>=std::int64_t(clip.origin.y)+clip.extent.height)continue;
                    const auto col=floorDiv(x,size),row=floorDiv(y,size);const auto* chunk=findRasterChunk(*original,std::int32_t(col),std::int32_t(row));
                    const auto index=std::size_t(dy)*side+dx;
                    if(chunk)before.pixels[index]=chunk->pixels.pixels[std::size_t(y-row*size)*size+std::size_t(x-col*size)];
                    const auto painted=chunks_.assetPixelAt(std::int32_t(x),std::int32_t(y));if(!painted)return std::nullopt;
                    after.pixels[index]=*painted;mask.alpha[index]=stroke_chunk_coverage(double(x)+.5,double(y)+.5);
                }
                auto result=blendRaster(before,after,mask);if(!result.ok())return std::nullopt;
                preview.pixels.push_back(std::move(result.pixels.pixels));regions.push_back({{int(px),int(py)},side,side});
            }
            preview.tiles.reserve(preview.pixels.size());for(std::size_t i=0;i<preview.pixels.size();++i)preview.tiles.push_back({regions[i],preview.pixels[i]});
            return std::optional<SelectionPreview>(std::move(preview));
        } catch(const std::bad_alloc&) {return std::nullopt;}
    }
    const RasterLayer* original = stroke_original_ ? &*stroke_original_ : nullptr;
    if (!original && stroke_raster_history_ && file_.document()) {
        if (const auto* asset = findRasterAsset(*file_.document(), raster_.boundAssetId())) original = &asset->pixels;
    }
    if (!original || !raster_.strokeActive() || !raster_.pixels()) return std::nullopt;
    try {
        const auto *painted = raster_.pixels();
        if (painted->width != original->width || painted->height != original->height) return std::nullopt;
        const auto bounds = stroke_dirty_total_;
        const int side = BitmapPreviewTileSide;
        auto left=std::max<std::int64_t>(0,bounds.origin.x), top=std::max<std::int64_t>(0,bounds.origin.y);
        auto right=std::min<std::int64_t>(painted->width,std::int64_t(bounds.origin.x)+bounds.width);
        auto bottom=std::min<std::int64_t>(painted->height,std::int64_t(bounds.origin.y)+bounds.height);
        restrict_to_requested(left,top,right,bottom);
        SelectionPreview preview; preview.asset_id = raster_.boundAssetId();
        if (right<=left || bottom<=top) return std::optional<SelectionPreview>(std::move(preview));
        const int x0=int((left/side)*side), y0=int((top/side)*side);
        const auto count = std::uint64_t((right-x0+side-1)/side)*std::uint64_t((bottom-y0+side-1)/side);
        if (count>1024) return std::nullopt;
        preview.pixels.reserve(static_cast<std::size_t>(count));
        std::vector<DevicePixelRect> regions; regions.reserve(static_cast<std::size_t>(count));
        const auto &t=stroke_transform_;
        for (int y=y0;y<bottom;y+=side) for (int x=x0;x<right;x+=side) {
            auto before=makeRasterLayer(side,side,0), after=makeRasterLayer(side,side,0);
            RasterMask selection{{side,side},std::vector<std::uint8_t>(side*side,0)};
            for(int dy=0;dy<side;++dy) for(int dx=0;dx<side;++dx) {
                const int ax=x+dx,ay=y+dy; if(ax>=painted->width || ay>=painted->height) continue;
                const auto tile=std::size_t(dy)*side+dx, source=std::size_t(ay)*painted->width+ax;
                before.pixels[tile]=original->pixels[source]; after.pixels[tile]=painted->pixels[source];
                const double wx=t.m11*(ax+.5)+t.m21*(ay+.5)+t.translationX;
                const double wy=t.m12*(ax+.5)+t.m22*(ay+.5)+t.translationY;
                selection.alpha[tile]=selection_coverage(wx,wy);
            }
            auto result=blendRaster(before,after,selection); if(!result.ok()) return std::nullopt;
            preview.pixels.push_back(std::move(result.pixels.pixels)); regions.push_back({{x,y},side,side});
        }
        preview.tiles.reserve(preview.pixels.size());
        for(std::size_t i=0;i<preview.pixels.size();++i) preview.tiles.push_back({regions[i],preview.pixels[i]});
        return std::optional<SelectionPreview>(std::move(preview));
    } catch(const std::bad_alloc&) { return std::nullopt; }
}

bool Artwork::begin(DocumentPoint point, double pressure, BitmapBrush brush, FrameIndex frame)
{
    legacy_stroke_time_ = 1.0 / 120.0;
    StrokePoint sample; sample.position = point; sample.pressure = pressure; sample.time = legacy_stroke_time_;
    return begin_input(sample, std::move(brush), frame);
}
bool Artwork::begin_input(StrokePoint sample, BitmapBrush brush, FrameIndex frame, StrokeTaper taper)
{
    auto point = sample.position;
    if (stroke_active())
        cancel();
    stroke_dirty_total_ = {};
    stroke_dirty_pending_ = {};
    const auto unit = [](double value) { return std::isfinite(value) && value >= 0 && value <= 1; };
    if (!unit(taper.start) || !unit(taper.end) || !unit(taper.minimum) || !unit(taper.strength))
        return fail("Invalid brush taper settings");
    if (frame >= draft_.timeline.frameCount)
        return fail("The current frame is outside the document timeline");
    const auto* selected = layer(draft_, selected_);
    if (!selected) return fail("Select a bitmap layer before painting");
    const auto sampled = sampleLayerAt(draft_, *selected, frame);
    if (!sampled.visible) return fail("The selected layer is not visible on this frame");
    // Keep the exact displayed transform stable for every point in the stroke.
    stroke_transform_ = sampled.transform;
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted) return fail("Cannot paint through a degenerate layer transform");
    point = *converted;
    sample.position = point;
    stroke_before_.clear();
    stroke_tiles_.clear();
    stroke_tile_columns_ = 0;
    stroke_raster_history_ = false;
    stroke_frame_history_.reset();
    stroke_compact_ = false;
    stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
    stroke_frame_copy_ = false;
    stroke_selection_ = selected_;
    const auto rollback = [&](std::string error)
    {
        raster_.cancelStroke();
        chunks_.cancelStroke();
        draft_ = *file_.document();
        stroke_before_.clear();
        stroke_tiles_.clear(); stroke_tile_columns_ = 0;
        stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
        stroke_raster_history_ = false;
        streaming_brush_.reset();
        stroke_frame_history_.reset();
        stroke_frame_copy_ = false;
        bind();
        return fail(std::move(error));
    };
    if (selection_ && ((selection_->dense && (selection_->dense->extent != draft_.extent || draft_.canvasMode != CanvasMode::Finite)) ||
        (selection_->tiled && selection_->tiled->domain() != CanvasRegion{canvasOrigin(draft_),draft_.extent})))
        return fail("Selection no longer matches the canvas");
    try {
        const auto* resolved = resolveAssetAt(draft_, *selected, frame);
        const auto* bitmap = resolved ? std::get_if<RasterAsset>(resolved) : nullptr;
        const auto* chunked = resolved ? std::get_if<ChunkedRasterAsset>(resolved) : nullptr;
        const bool dynamic = std::holds_alternative<DynamicBitmapLayer>(*selected);
        const auto range = layerProperties(*selected).frameRange;
        const bool spans_frames = draft_.timeline.frameCount > 1 &&
            !(range && range->firstFrame == frame && range->lastFrame == frame);
        const bool shared = (bitmap && assetReferences(draft_, bitmap->id).size() > 1) ||
                            (chunked && assetReferences(draft_, chunked->id).size() > 1);
        const bool copy_frame = dynamic || spans_frames || shared;
        constexpr auto minimum_tile_history = std::size_t(BitmapPreviewTileSide) * BitmapPreviewTileSide *
                                              2 * sizeof(std::uint32_t);
        stroke_raster_history_ = bitmap && draft_.canvasMode == CanvasMode::Finite &&
                                 !copy_frame && history_budget_ >= minimum_tile_history;
        const auto& transform = stroke_transform_;
        const bool identity = transform.m11 == 1 && transform.m12 == 0 && transform.m21 == 0 &&
                              transform.m22 == 1 && transform.translationX == 0 && transform.translationY == 0;
        const auto origin = canvasOrigin(draft_);
        stroke_compact_ = chunked && draft_.canvasMode == CanvasMode::Finite && !copy_frame &&
                          identity && origin.x == 0 && origin.y == 0 && history_budget_ >= minimum_tile_history;
        const bool indexedRaster = stroke_raster_history_ && selection_;
        const bool indexedChunk = stroke_compact_ && selection_;
        if (indexedRaster || indexedChunk) {
            const auto width = indexedRaster ? bitmap->pixels.width : draft_.extent.width;
            const auto height = indexedRaster ? bitmap->pixels.height : draft_.extent.height;
            const auto columns = (std::size_t(width) + BitmapPreviewTileSide - 1) / BitmapPreviewTileSide;
            const auto rows = (std::size_t(height) + BitmapPreviewTileSide - 1) / BitmapPreviewTileSide;
            if (rows && columns > stroke_tiles_.max_size() / rows)
                return rollback("Stroke tile index exceeds the supported size");
            stroke_tiles_.assign(columns * rows, 0);
            stroke_tile_columns_ = columns;
        }
        if (!stroke_raster_history_ && !stroke_compact_) stroke_before_ = capture();
        if (!stroke_raster_history_ && !stroke_compact_ && stroke_before_.empty()) {
            if (!chunked)
                return rollback("Stroke exceeds the undo budget");
            stroke_compact_=true;
        }
        if (chunked) {
            const auto& t=stroke_transform_;
            const bool transformed=t.m11!=1 || t.m12!=0 || t.m21!=0 || t.m22!=1 || t.translationX!=0 || t.translationY!=0;
            if(transformed) {
                stroke_asset_region_=assetBoundsForCanvas({canvasOrigin(draft_),draft_.extent},t);
                if(!stroke_asset_region_)return rollback("Transformed stroke exceeds the supported asset coordinate range");
            }
            if(selection_ || copy_frame || transformed) {
                const auto size=draft_.infiniteCanvas.chunkSize;
                if(size<=0 || std::uint64_t(size)*size>4ULL*1024*1024)return rollback("Chunk size exceeds the stroke budget");
            }
        }
        if (copy_frame) {
            DocumentEditor editor(draft_);
            std::optional<FrameSourceHistory> sourceBefore;
            std::string priorAsset;
            if(stroke_compact_) {
                sourceBefore=capture_frame_source(draft_,selected_);
                if(!sourceBefore || !chunked)return rollback("Compact frame source is unavailable");
                priorAsset=chunked->id;
            }
            std::string asset_id;
            if (chunked) {
                auto content = *chunked;
                if (!replace_frame_chunked(editor, frame, std::move(content), asset_id)) return rollback(error_);
                if(stroke_compact_) {
                    auto sourceAfter=capture_frame_source(draft_,selected_);
                    if(!sourceAfter)return rollback("Compact frame source is unavailable");
                    stroke_frame_history_=ChunkFrameHistory{selected_,std::move(priorAsset),asset_id,std::move(*sourceBefore),std::move(*sourceAfter)};
                }
                raster_.unbind(); chunks_.unbind();
                if (!chunks_.bind(draft_, asset_id)) return rollback("Cannot bind the current frame chunks");
            } else {
                if (!bitmap || draft_.canvasMode != CanvasMode::Finite)
                    return fail("Frame editing currently requires a finite bitmap layer");
                stroke_original_ = bitmap->pixels;
                if (!replace_frame_bitmap(editor, frame, *stroke_original_, asset_id)) return rollback(error_);
                raster_.unbind(); chunks_.unbind();
                if (!raster_.bind(draft_, asset_id)) return rollback("Cannot bind the current frame bitmap");
            }
            stroke_frame_copy_ = true;
        } else if (!bind()) return fail("Select a bitmap layer before painting");
        if (selection_ || (chunked && copy_frame) || stroke_asset_region_) {
            if (chunks_.isBound()) {
                const auto* original=findChunkedRasterAsset(draft_,chunks_.boundAssetId());
                if (!original) return rollback("Chunk stroke target is unavailable");
                if (!stroke_compact_) stroke_original_chunks_ = *original;
            } else {
                if (draft_.canvasMode != CanvasMode::Finite || !raster_.isBound() || !raster_.pixels())
                    return rollback("Selection and tapered painting currently require a finite bitmap layer");
                if (!stroke_raster_history_) {
                    if (stroke_before_.empty()) return rollback("Stroke exceeds the undo budget");
                    if (!stroke_original_) stroke_original_ = *raster_.pixels();
                }
            }
        }
    } catch (const std::bad_alloc&) { return rollback("Insufficient memory for stroke snapshot"); }
    if (!stroke_asset_region_ && !stroke_compact_ && !grow(point, std::ceil(brush.size / 2) + 2))
        return rollback(error_);
    if (stroke_compact_ && draft_.canvasMode == CanvasMode::Infinite && !selection_ &&
        !stroke_asset_region_ && chunks_.isBound() && !chunks_.setStrokeCanvasGrowth(true))
        return rollback(chunks_.lastError());
    if (raster_.isBound())
    {
        // Non-structural finite strokes retain only changed tiles.
        const bool configured = stroke_raster_history_ ? raster_.setDeferredStrokeHistory(true)
                                                       : raster_.setExternalStrokeHistory(true);
        if (!configured)
            return rollback(raster_.lastError());
        if (!raster_.setBrush(brush))
            return rollback(raster_.lastError());
        raster_.clearDirtyBounds();
        if (!raster_.beginProjectedStroke()) return rollback(raster_.lastError());
    }
    else {
        const bool bounded = draft_.canvasMode == CanvasMode::Finite || selection_ || stroke_asset_region_;
        if (bounded) {
            // Compact documents use the editor's tile history on release;
            // ordinary documents use Artwork's snapshot instead.
            const bool configured = stroke_compact_ ? chunks_.setDeferredStrokeHistory(true)
                                                   : chunks_.setExternalStrokeHistory(true);
            if (!configured) return rollback(chunks_.lastError());
        }
        if (!chunks_.setBrush(brush)) return rollback(chunks_.lastError());
        chunks_.clearDirtyBounds();
        if (bounded) {
            if (!chunks_.beginProjectedStroke(stroke_asset_region_)) return rollback(chunks_.lastError());
        } else {
            // Compatibility for pre-existing unbounded documents: their
            // growth editor is separate from the finite canvas workflow.
            if (!chunks_.beginStrokeInput(sample)) return rollback(chunks_.lastError());
            collect_stroke_damage();
            legacy_stroke_time_ = sample.time;
            return true;
        }
    }
    try {
        auto state = raster_.isBound() ? raster_.currentStrokeBrushState() : chunks_.currentStrokeBrushState();
        if (!state) return rollback("Unable to initialize the active brush");
        streaming_brush_ = std::make_shared<StreamingBrush>(std::move(*state), taper.start, taper.end,
                                                            taper.minimum, taper.strength);
        if (!update_stream_input(sample, false)) return rollback(error_);
    } catch (const std::bad_alloc&) { return rollback("Insufficient memory for stroke input"); }
    legacy_stroke_time_ = sample.time;
    return true;
}

bool Artwork::update_stream_input(StrokePoint sample, bool finish)
{
    if (!streaming_brush_) return fail("No streaming brush is active");
    DevicePixelRect clip{};
    if (raster_.isBound()) clip = {{0, 0}, raster_.width(), raster_.height()};
    else {
        const auto region = stroke_asset_region_.value_or(CanvasRegion{canvasOrigin(draft_), draft_.extent});
        clip = {{region.origin.x, region.origin.y}, region.extent.width, region.extent.height};
    }
    struct Source { const Artwork* artwork; DevicePixelPoint origin; };
    const Source context{this, clip.origin};
    const RasterSourceSampler source{&context, [](const void* value, DevicePixelPoint point) {
        const auto& source = *static_cast<const Source*>(value);
        point.x += source.origin.x; point.y += source.origin.y;
        const auto& art = *source.artwork;
        return art.raster_.isBound() ? art.raster_.strokeSourceAt(point.x, point.y).value_or(0) :
                                       art.chunks_.strokeSourceAt(point.x, point.y).value_or(0);
    }, clip.origin, clip.width, clip.height};
    auto pixels = streaming_brush_->append(sample, finish, source, clip);
    if (!pixels) return fail(pixels.error());
    if (pixels->unchanged) return true;
    const bool accepted = raster_.isBound() ? raster_.updateProjectedStroke(pixels->stable, pixels->transient) :
                                              chunks_.updateProjectedStroke(pixels->stable, pixels->transient);
    if (!accepted) return fail(raster_.isBound() ? raster_.lastError() : chunks_.lastError());
    collect_stroke_damage();
    return true;
}

bool Artwork::extend(DocumentPoint point, double pressure)
{
    StrokePoint sample; sample.position = point; sample.pressure = pressure; sample.time = (legacy_stroke_time_ += 1.0 / 120.0);
    return extend_input(sample);
}
bool Artwork::extend_input(StrokePoint sample)
{
    auto point = sample.position;
    if (!stroke_active())
        return fail("No stroke is active");
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted)
        return reject_stroke("Cannot paint through a degenerate layer transform");
    point = *converted;
    if (!stroke_asset_region_ && !stroke_compact_ && !grow(point,
              std::ceil(chunks_.isBound() ? chunks_.brush().size / 2 : raster_.brush().size / 2) +
                  2))
        return reject_stroke(error_);
    sample.position = point;
    if (streaming_brush_) {
        if (!update_stream_input(sample, false)) return reject_stroke(error_);
    } else {
        if (!chunks_.continueStrokeInput(sample)) return reject_stroke(chunks_.lastError());
        collect_stroke_damage();
    }
    legacy_stroke_time_ = sample.time;
    return true;
}
bool Artwork::end(DocumentPoint point, double pressure)
{
    StrokePoint sample; sample.position = point; sample.pressure = pressure; sample.time = (legacy_stroke_time_ += 1.0 / 120.0);
    return end_input(sample);
}
bool Artwork::end_input(StrokePoint sample)
{
    auto point = sample.position;
    if (!stroke_active())
        return fail("No stroke is active");
    const auto converted = localPoint(point, stroke_transform_);
    if (!converted)
    {
        cancel();
        return fail("Cannot paint through a degenerate layer transform");
    }
    point = *converted;
    if (!stroke_asset_region_ && !stroke_compact_ && !grow(point,
              std::ceil(chunks_.isBound() ? chunks_.brush().size / 2 : raster_.brush().size / 2) +
                  2))
    {
        cancel();
        return false;
    }
    sample.position = point;
    bool result;
    if (streaming_brush_) {
        if (!update_stream_input(sample, true)) return reject_stroke(error_);
        result = raster_.isBound() ? raster_.endProjectedStroke() : chunks_.endProjectedStroke();
    } else result = chunks_.endStrokeInput(sample);
    if (!result)
        return reject_stroke(raster_.isBound() ? raster_.lastError() : chunks_.lastError());
    streaming_brush_.reset();
    collect_stroke_damage();
    if (stroke_raster_history_) {
        stroke_raster_history_ = false;
        if (selection_)
            return finish_masked_raster_stroke();
        if (!raster_.canUndo()) {
            // The deferred journal established that no pixel changed. Restore
            // only metadata; copying the entire raster here would make even
            // a transparent click scale with document size.
            draft_.authorship = file_.document()->authorship;
            draft_.formatVersion = file_.document()->formatVersion;
            stroke_before_.clear();
            bind(); error_.clear();
            return true;
        }
        try {
            return commit_raster_stroke_history(std::make_shared<BitmapEditor>(raster_),
                                                std::move(stroke_selection_));
        } catch (const std::bad_alloc&) {
            return reject_stroke("Insufficient memory for raster stroke history");
        }
    }
    if (selection_ || stroke_asset_region_)
        stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
    if (stroke_compact_ && draft_.canvasMode == CanvasMode::Finite && !selection_ &&
        !stroke_asset_region_ && !stroke_frame_history_) {
        stroke_compact_ = false;
        if (!chunks_.canUndo()) {
            draft_.authorship = file_.document()->authorship;
            draft_.formatVersion = file_.document()->formatVersion;
            bind(); error_.clear(); return true;
        }
        try {
            const bool committed = commit_compact_chunk_history(std::make_shared<ChunkedBitmapEditor>(chunks_),
                std::move(stroke_selection_), std::nullopt, std::nullopt);
            if (!committed)
                stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
            return committed;
        } catch (const std::bad_alloc&) {
            return reject_stroke("Insufficient memory for chunk stroke history");
        }
    }
    if (stroke_compact_) return finish_compact_chunk_stroke();
    if (stroke_frame_copy_ && !selection_ && stroke_original_) {
        const auto* asset = findRasterAsset(draft_, raster_.boundAssetId());
        if (!asset) return reject_stroke("Frame stroke target changed");
        if (asset->pixels.pixels == stroke_original_->pixels) {
            draft_ = *file_.document(); stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
            stroke_before_.clear(); stroke_frame_copy_ = false; bind(); error_.clear(); return true;
        }
    }
    if (!selection_ && stroke_original_chunks_ && stroke_frame_copy_) {
        const auto* asset = findChunkedRasterAsset(draft_, chunks_.boundAssetId());
        if (!asset || asset->id != stroke_original_chunks_->id) return reject_stroke("Chunk stroke target changed");
        bool changed = asset->chunks.size() != stroke_original_chunks_->chunks.size();
        if (!changed) for (const auto& original : stroke_original_chunks_->chunks) {
            const auto* current = findRasterChunk(*asset, original.column, original.row);
            if (!current || current->pixels.width != original.pixels.width || current->pixels.height != original.pixels.height ||
                current->pixels.pixels != original.pixels.pixels) { changed = true; break; }
        }
        if (!changed) {
            draft_ = *file_.document(); stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
            stroke_before_.clear(); stroke_frame_copy_ = false; bind(); error_.clear(); return true;
        }
    }
    const bool require_undo_snapshot = bool(selection_) || bool(stroke_asset_region_) || stroke_frame_copy_;
    stroke_frame_copy_ = false;
    if ((selection_ || stroke_asset_region_) && stroke_original_chunks_) {
        try {
            auto* asset=findChunkedRasterAsset(draft_,chunks_.boundAssetId());
            if (!asset || asset->id!=stroke_original_chunks_->id)
                throw std::runtime_error("Selected chunk stroke target changed");
            const auto size=draft_.infiniteCanvas.chunkSize;
            bool changed=false;
            for (const auto& original : stroke_original_chunks_->chunks)
                if (!findRasterChunk(*asset,original.column,original.row))
                    throw std::runtime_error("Selected chunk disappeared during the stroke");
            for (auto it=asset->chunks.begin(); it!=asset->chunks.end();) {
                const auto* original=findRasterChunk(*stroke_original_chunks_,it->column,it->row);
                auto before=original ? original->pixels : makeRasterLayer(size,size,0);
                if (it->pixels.width!=size || it->pixels.height!=size || before.width!=size || before.height!=size)
                    throw std::runtime_error("Invalid selected chunk dimensions");
                if (it->pixels.pixels==before.pixels) {
                    if (!original) it=asset->chunks.erase(it); else ++it;
                    continue;
                }
                RasterMask local{{size,size},std::vector<std::uint8_t>(std::size_t(size)*size)};
                const double ox=double(it->column)*size,oy=double(it->row)*size;
                for(int y=0;y<size;++y) for(int x=0;x<size;++x)
                    local.alpha[std::size_t(y)*size+x]=stroke_chunk_coverage(ox+x+.5,oy+y+.5);
                auto blended=blendRaster(before,it->pixels,local);
                if(!blended.ok()) throw std::runtime_error(blended.error);
                const bool different=blended.pixels.pixels!=before.pixels;
                changed|=different;
                if (!original && !different) it=asset->chunks.erase(it);
                else { it->pixels=std::move(blended.pixels); ++it; }
            }
            if (!changed) {
                draft_=*file_.document();stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
                stroke_before_.clear();bind();error_.clear();return true;
            }
        } catch (const std::exception& e) {
            auto message=std::string(e.what());draft_=*file_.document();stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
            stroke_before_.clear();bind();return fail(std::move(message));
        }
    }
    if (selection_ && stroke_original_) {
        try {
            auto* asset = findRasterAsset(draft_, raster_.boundAssetId());
            if (!asset || asset->pixels.width != stroke_original_->width || asset->pixels.height != stroke_original_->height)
                throw std::runtime_error("Selection stroke target changed");
            RasterMask local{{asset->pixels.width,asset->pixels.height}, std::vector<std::uint8_t>(asset->pixels.pixels.size(),0)};
            const auto &t=stroke_transform_;
            for(int y=0;y<local.extent.height;++y) for(int x=0;x<local.extent.width;++x) {
                const double wx=t.m11*(x+.5)+t.m21*(y+.5)+t.translationX;
                const double wy=t.m12*(x+.5)+t.m22*(y+.5)+t.translationY;
                local.alpha[std::size_t(y)*local.extent.width+x]=selection_coverage(wx,wy);
            }
            auto blended=blendRaster(*stroke_original_,asset->pixels,local);
            if(!blended.ok()) throw std::runtime_error(blended.error);
            if (blended.pixels.pixels == stroke_original_->pixels) {
                draft_ = *file_.document();
                stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
                stroke_before_.clear();
                bind();
                error_.clear();
                return true;
            }
            asset->pixels=std::move(blended.pixels);
        } catch(const std::exception &e) {
            auto message=std::string(e.what()); draft_=*file_.document(); stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false; stroke_before_.clear(); bind(); return fail(std::move(message));
        }
    }
    stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
    if (require_undo_snapshot && capture().empty()) {
        draft_ = *file_.document(); stroke_before_.clear(); bind();
        return fail("Stroke exceeds the undo budget");
    }
    return commit(std::move(stroke_before_), std::move(stroke_selection_));
}
bool Artwork::finish_masked_raster_stroke()
{
    const auto rollback = [&](std::string reason) {
        // Preview frames may have consumed the incremental damage already.
        // A failed publication restores the committed document, so invalidate the
        // entire stroke rather than leaving earlier preview tiles on screen.
        stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
        draft_ = *file_.document();
        stroke_tiles_.clear(); stroke_tile_columns_ = 0;
        bind();
        return fail(std::move(reason));
    };
    try {
        const auto assetId = raster_.boundAssetId();
        const auto* originalAsset = file_.document() ? findRasterAsset(*file_.document(), assetId) : nullptr;
        const auto* painted = raster_.pixels();
        if (!originalAsset || !painted || painted->width != originalAsset->pixels.width ||
            painted->height != originalAsset->pixels.height)
            return rollback("Masked stroke target changed");
        const auto& original = originalAsset->pixels;
        std::vector<RasterLayer> storage;
        std::vector<DevicePixelRect> regions;
        const auto append = [&](DevicePixelRect region, std::span<const std::uint32_t> values) {
            if (region.origin.x < 0 || region.origin.y < 0 || region.width <= 0 || region.height <= 0 ||
                std::int64_t(region.origin.x) + region.width > original.width ||
                std::int64_t(region.origin.y) + region.height > original.height ||
                values.size() != std::size_t(region.width) * region.height)
                throw std::runtime_error("Invalid masked stroke tile");
            auto before = makeRasterLayer(region.width, region.height, 0);
            auto after = makeRasterLayer(region.width, region.height, 0);
            for (int y = 0; y < region.height; ++y) {
                const auto offset = std::size_t(y) * region.width;
                const auto source = std::size_t(y + region.origin.y) * original.width + region.origin.x;
                std::copy_n(original.pixels.begin() + source, region.width, before.pixels.begin() + offset);
                std::copy_n(values.begin() + offset, region.width, after.pixels.begin() + offset);
            }
            if (selection_) {
                RasterMask mask{{region.width, region.height},
                                std::vector<std::uint8_t>(std::size_t(region.width) * region.height)};
                const auto& t = stroke_transform_;
                for (int y = 0; y < region.height; ++y) for (int x = 0; x < region.width; ++x) {
                    const double ax = region.origin.x + x + .5, ay = region.origin.y + y + .5;
                    mask.alpha[std::size_t(y) * region.width + x] = selection_coverage(
                        t.m11 * ax + t.m21 * ay + t.translationX,
                        t.m12 * ax + t.m22 * ay + t.translationY);
                }
                auto blended = blendRaster(before, after, mask);
                if (!blended.ok()) throw std::runtime_error(blended.error);
                after = std::move(blended.pixels);
            }
            if (before.pixels != after.pixels) {
                regions.push_back(region);
                storage.push_back(std::move(after));
            }
        };
        constexpr int side = BitmapPreviewTileSide;
        for (std::size_t index = 0; index < stroke_tiles_.size(); ++index) {
            if (!stroke_tiles_[index]) continue;
            const int x = int(index % stroke_tile_columns_) * side;
            const int y = int(index / stroke_tile_columns_) * side;
            const int width = std::min(side, painted->width - x);
            const int height = std::min(side, painted->height - y);
            std::vector<std::uint32_t> pixels(std::size_t(width) * height);
            for (int row = 0; row < height; ++row)
                std::copy_n(painted->pixels.begin() + std::size_t(y + row) * painted->width + x,
                            width, pixels.begin() + std::size_t(row) * width);
            append({{x, y}, width, height}, pixels);
        }
        stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
        if (raster_.canUndo() && !raster_.undo()) return rollback(raster_.lastError());
        raster_.unbind();
        draft_.authorship = file_.document()->authorship;
        draft_.formatVersion = file_.document()->formatVersion;
        if (!raster_.bind(draft_, assetId)) return rollback("Cannot rebind masked stroke target");
        stroke_tiles_.clear(); stroke_tile_columns_ = 0;
        if (storage.empty()) { bind(); error_.clear(); return true; }
        std::vector<BitmapPatch> patches; patches.reserve(storage.size());
        for (std::size_t index = 0; index < storage.size(); ++index)
            patches.push_back({regions[index], storage[index].pixels});
        if (!raster_.replacePatches(patches)) return rollback(raster_.lastError());
        return commit_raster_stroke_history(std::make_shared<BitmapEditor>(raster_),
                                            std::move(stroke_selection_));
    } catch (const std::exception& error) {
        return rollback(error.what());
    }
}

bool Artwork::finish_compact_chunk_stroke()
{
    auto frameHistory=std::move(stroke_frame_history_);stroke_frame_history_.reset();
    const auto rollback=[&](std::string reason) {
        draft_=*file_.document();stroke_before_.clear();stroke_tiles_.clear();stroke_tile_columns_=0;
        stroke_compact_=false;stroke_asset_region_.reset();bind();
        return fail(std::move(reason));
    };
    try {
        const auto assetId=chunks_.boundAssetId();
        const auto* original=file_.document()?findChunkedRasterAsset(*file_.document(),frameHistory ? frameHistory->prior : assetId):nullptr;
        if(!original)return rollback("Compact stroke source is unavailable");
        const CanvasRegion initialCanvas{canvasOrigin(*file_.document()),file_.document()->extent};
        const CanvasRegion finalCanvas{canvasOrigin(draft_),draft_.extent};
        const auto unchanged=[&]() {
            if(!stroke_tiles_.empty() && !frameHistory && initialCanvas==finalCanvas) {
                if(chunks_.canUndo() && !chunks_.undo())return rollback(chunks_.lastError());
                draft_.authorship=file_.document()->authorship;
                draft_.formatVersion=file_.document()->formatVersion;
            } else draft_=*file_.document();
            stroke_before_.clear();stroke_tiles_.clear();stroke_tile_columns_=0;
            stroke_compact_=false;stroke_asset_region_.reset();bind();error_.clear();return true;
        };
        const auto dirty=unionStrokeDamage(stroke_dirty_total_,chunks_.dirtyBounds());const auto clip=stroke_asset_region_.value_or(CanvasRegion{canvasOrigin(draft_),draft_.extent});
        auto local=std::optional<CanvasRegion>(clip);
        if(selection_) {
            const auto selected=selection_->tiled ? selection_->tiled->nonzeroBounds() :
                    std::optional<CanvasRegion>(CanvasRegion{canvasOrigin(draft_),draft_.extent});
            if(!selected) return unchanged();
            local=assetBoundsForCanvas(*selected,stroke_transform_);
            if(!local)return rollback("Selection exceeds the supported asset coordinate range");
        }
        const auto left=std::max({std::int64_t(dirty.origin.x),std::int64_t(clip.origin.x),std::int64_t(local->origin.x)});
        const auto top=std::max({std::int64_t(dirty.origin.y),std::int64_t(clip.origin.y),std::int64_t(local->origin.y)});
        const auto right=std::min({std::int64_t(dirty.origin.x)+dirty.width,std::int64_t(clip.origin.x)+clip.extent.width,std::int64_t(local->origin.x)+local->extent.width});
        const auto bottom=std::min({std::int64_t(dirty.origin.y)+dirty.height,std::int64_t(clip.origin.y)+clip.extent.height,std::int64_t(local->origin.y)+local->extent.height});
        if(right<=left || bottom<=top) return unchanged();
        const int side=BitmapPreviewTileSide,size=draft_.infiniteCanvas.chunkSize;
        const auto div=[](std::int64_t value,int divisor){return value>=0?value/divisor:(value-divisor+1)/divisor;};
        const auto x0=div(left,side),y0=div(top,side),x1=div(right-1,side),y1=div(bottom-1,side);
        const bool indexed=!stroke_tiles_.empty() && stroke_tile_columns_>0;
        const auto inBounds=[&](std::size_t index) {
            const auto tx=std::int64_t(index%stroke_tile_columns_),ty=std::int64_t(index/stroke_tile_columns_);
            return stroke_tiles_[index] && tx>=x0 && tx<=x1 && ty>=y0 && ty<=y1;
        };
        std::uint64_t count=0;
        if(indexed) {
            for(std::size_t index=0;index<stroke_tiles_.size();++index)if(inBounds(index))++count;
        } else count=std::uint64_t(x1-x0+1)*std::uint64_t(y1-y0+1);
        if((!indexed && count>1024) || count>history_budget_/(std::uint64_t(side)*side*8))
            return rollback("Compact stroke exceeds the tile or history budget");
        std::vector<std::vector<std::uint32_t>> storage;std::vector<DevicePixelRect> regions;
        storage.reserve(std::size_t(count));regions.reserve(std::size_t(count));
        std::string tileError;
        const auto appendTile=[&](std::int64_t tx,std::int64_t ty) {
            const auto lx=std::max<std::int64_t>(tx*side,clip.origin.x),ly=std::max<std::int64_t>(ty*side,clip.origin.y);
            const auto rx=std::min<std::int64_t>((tx+1)*side,std::int64_t(clip.origin.x)+clip.extent.width);
            const auto by=std::min<std::int64_t>((ty+1)*side,std::int64_t(clip.origin.y)+clip.extent.height);
            const int w=int(rx-lx),h=int(by-ly);auto before=makeRasterLayer(w,h,0),after=makeRasterLayer(w,h,0);
            RasterMask mask{{w,h},std::vector<std::uint8_t>(std::size_t(w)*h,0)};
            for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                const auto ax=lx+x,ay=ly+y,col=div(ax,size),row=div(ay,size);const auto index=std::size_t(y)*w+x;
                const auto key=std::pair{std::int32_t(row),std::int32_t(col)};
                const auto it=std::lower_bound(original->chunks.begin(),original->chunks.end(),key,[](const auto& chunk,const auto& value){return std::pair{chunk.row,chunk.column}<value;});
                if(it!=original->chunks.end()&&it->row==row&&it->column==col)before.pixels[index]=it->pixels.pixels[std::size_t(ay-row*size)*size+std::size_t(ax-col*size)];
                const auto painted=chunks_.assetPixelAt(std::int32_t(ax),std::int32_t(ay));
                if(!painted) {tileError="Compact stroke exceeds the asset coordinate range";return false;}
                after.pixels[index]=*painted;mask.alpha[index]=stroke_chunk_coverage(double(ax)+.5,double(ay)+.5);
            }
            auto blended=blendRaster(before,after,mask);if(!blended.ok()) {tileError=blended.error;return false;}
            if(blended.pixels.pixels!=before.pixels) {storage.push_back(std::move(blended.pixels.pixels));regions.push_back({{int(lx),int(ly)},w,h});}
            return true;
        };
        if(indexed) {
            for(std::size_t index=0;index<stroke_tiles_.size();++index)if(inBounds(index) &&
                !appendTile(std::int64_t(index%stroke_tile_columns_),std::int64_t(index/stroke_tile_columns_)))
                return rollback(std::move(tileError));
        } else {
            for(auto ty=y0;ty<=y1;++ty)for(auto tx=x0;tx<=x1;++tx)
                if(!appendTile(tx,ty))return rollback(std::move(tileError));
        }
        if(storage.empty()) return unchanged();
        if(!chunks_.undo())return rollback("Masked stroke undo: "+chunks_.lastError());
        draft_.infiniteCanvas.origin=finalCanvas.origin;draft_.extent=finalCanvas.extent;
        chunks_.unbind();if(!chunks_.bind(draft_,assetId))return rollback("Cannot rebind compact chunk target");
        std::vector<BitmapPatch> patches;patches.reserve(storage.size());
        for(std::size_t i=0;i<storage.size();++i)patches.push_back({regions[i],storage[i]});
        if(!chunks_.replaceAssetPatches(patches))return rollback(chunks_.lastError());
        auto journal=std::make_shared<ChunkedBitmapEditor>(chunks_);
        const auto beforeCanvas=initialCanvas==finalCanvas ? std::optional<CanvasRegion>{} : std::optional(initialCanvas);
        const auto afterCanvas=initialCanvas==finalCanvas ? std::optional<CanvasRegion>{} : std::optional(finalCanvas);
        stroke_before_.clear();stroke_tiles_.clear();stroke_tile_columns_=0;
        stroke_compact_=false;stroke_asset_region_.reset();
        const bool committed = commit_compact_chunk_history(std::move(journal), stroke_selection_,
                                                            beforeCanvas, afterCanvas, std::move(frameHistory));
        if (!committed)
            stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
        return committed;
    } catch(const std::bad_alloc&) {return rollback("Insufficient memory for compact stroke history");}
}

bool Artwork::commit_raster_stroke_history(std::shared_ptr<BitmapEditor> journal, std::string beforeSelection)
{
    const auto rollback = [&](std::string reason) {
        stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
        draft_ = *file_.document();
        bind();
        return fail(std::move(reason));
    };
    bool pendingInserted = false;
    try {
        if (!journal || !journal->historyStatistics().retainedBytes)
            return rollback("Raster stroke has no valid history");
        History pending;
        pending.before_selection = std::move(beforeSelection);
        pending.after_selection = selected_;
        pending.raster_editor = std::move(journal);
        if (draft_.authorship.dump() == file_.document()->authorship.dump()) recordDocumentChange(draft_);
        auto metadata = std::make_shared<BitmapHistoryMetadata>();
        metadata->before = file_.document()->authorship;
        metadata->before_version = file_.document()->formatVersion;
        metadata->after = draft_.authorship;
        metadata->after_version = draft_.formatVersion;
        metadata->authored_bytes = metadata->before.dump().size() + metadata->after.dump().size();
        pending.bitmap_metadata = std::move(metadata);
        const auto cost = pending.bytes();
        if (cost > history_budget_) return rollback("Raster stroke exceeds the undo budget");
        undo_.push_back(std::move(pending));
        pendingInserted = true;
        bool saved = false;
        std::string failure;
        auto& entry = undo_.back();
        if (currentBitmapFormat(entry.bitmap_metadata->before_version) &&
            currentBitmapFormat(entry.bitmap_metadata->after_version)) {
            saved = entry.raster_editor->publishHistoryState(file_, BitmapHistoryState::Applied,
                entry.bitmap_metadata->before, entry.bitmap_metadata->before_version, file_.revision());
            if (!saved) failure = entry.raster_editor->lastError();
        } else {
            const auto result = file_.edit([&](Document& document) { document = draft_; return true; });
            saved = result.ok() && result.changed;
            if (!saved) failure = result.ok() ? "Raster stroke did not change the document" : result.message;
        }
        if (!saved) {
            undo_.pop_back(); pendingInserted = false;
            return rollback(failure.empty() ? "Raster stroke could not be saved" : std::move(failure));
        }
        pendingInserted = false;
        for (const auto& entry : redo_) history_bytes_ -= entry.bytes();
        redo_.clear(); history_bytes_ += cost;
        while (history_bytes_ > history_budget_ || undo_.size() > HistoryCount) {
            history_bytes_ -= undo_.front().bytes(); undo_.pop_front();
        }
        try { bind(); } catch (const std::bad_alloc&) { raster_.unbind(); chunks_.unbind(); }
        error_.clear();
    } catch (const std::bad_alloc&) {
        if (pendingInserted) undo_.pop_back();
        return rollback("Insufficient memory for raster stroke history");
    }
    if (changed) changed();
    return true;
}

bool Artwork::commit_compact_chunk_history(std::shared_ptr<ChunkedBitmapEditor> journal,
                                           std::string beforeSelection,
                                           std::optional<CanvasRegion> beforeCanvas,
                                           std::optional<CanvasRegion> afterCanvas,
                                           std::optional<ChunkFrameHistory> frame)
{
    const auto rollback=[&](std::string reason){draft_=*file_.document();bind();return fail(std::move(reason));};
    bool pendingInserted=false;
    try {
        if(!journal || !journal->historyStatistics().retainedBytes || bool(beforeCanvas)!=bool(afterCanvas))
            return rollback("Compact edit has no valid history");
        History pending;pending.before_selection=std::move(beforeSelection);pending.after_selection=selected_;
        pending.tile_editor=std::move(journal);pending.canvas_before=beforeCanvas;pending.canvas_after=afterCanvas;
        if(frame)pending.chunk_frame=std::make_shared<const ChunkFrameHistory>(std::move(*frame));
        if(draft_.authorship.dump()==file_.document()->authorship.dump())recordDocumentChange(draft_);
        auto metadata=std::make_shared<BitmapHistoryMetadata>();
        metadata->before=file_.document()->authorship;metadata->before_version=file_.document()->formatVersion;
        metadata->after=draft_.authorship;metadata->after_version=draft_.formatVersion;
        metadata->authored_bytes=metadata->before.dump().size()+metadata->after.dump().size();
        pending.bitmap_metadata=std::move(metadata);
        const auto cost=pending.bytes();if(cost>history_budget_)return rollback("Compact edit exceeds the undo budget");
        undo_.push_back(std::move(pending));pendingInserted=true;
        auto& entry=undo_.back();
        bool saved=false;std::string failure;
        if(!entry.chunk_frame && !entry.canvas_before && !entry.canvas_after &&
           currentBitmapFormat(entry.bitmap_metadata->before_version) && currentBitmapFormat(entry.bitmap_metadata->after_version)) {
            saved=entry.tile_editor->publishHistoryState(file_,BitmapHistoryState::Applied,
                entry.bitmap_metadata->before,entry.bitmap_metadata->before_version,file_.revision());
            if(!saved)failure=entry.tile_editor->lastError();
        } else {
            const auto committed=file_.edit([&](Document& document){document=draft_;return true;});
            saved=committed.ok() && committed.changed;
            if(!saved)failure=committed.ok()?"Compact edit did not change the document":committed.message;
        }
        if(!saved) {undo_.pop_back();pendingInserted=false;return rollback(failure.empty()?"Compact edit could not be saved":std::move(failure));}
        pendingInserted=false;
        for(const auto& entry:redo_)history_bytes_-=entry.bytes();redo_.clear();history_bytes_+=cost;
        while(history_bytes_>history_budget_ || undo_.size()>HistoryCount){history_bytes_-=undo_.front().bytes();undo_.pop_front();}
        try {bind();} catch(const std::bad_alloc&) {raster_.unbind();chunks_.unbind();}
        error_.clear();
    } catch(const std::bad_alloc&) {if(pendingInserted)undo_.pop_back();return rollback("Insufficient memory for compact edit history");}
    if(changed)changed();return true;
}

bool Artwork::reject_stroke(std::string reason)
{
    stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
    stroke_tiles_.clear(); stroke_tile_columns_ = 0;
    stroke_raster_history_ = false;
    stroke_frame_history_.reset();
    streaming_brush_.reset();
    stroke_frame_copy_ = false;
    if (stroke_active())
        cancel();
    else
    {
        stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
        stroke_before_.clear();
        draft_ = *file_.document();
        bind();
    }
    return fail(reason.empty() ? "Stroke was rejected" : std::move(reason));
}
void Artwork::cancel()
{
    if (!stroke_active())
        return;
    const bool rasterOnly = stroke_raster_history_ && raster_.isBound();
    collect_stroke_damage();
    stroke_dirty_pending_ = unionStrokeDamage(stroke_dirty_pending_, stroke_dirty_total_);
    stroke_frame_history_.reset();
    raster_.cancelStroke();
    chunks_.cancelStroke();
    stroke_tiles_.clear(); stroke_tile_columns_ = 0;
    streaming_brush_.reset();
    stroke_frame_copy_ = false;
    stroke_raster_history_ = false;
    stroke_original_.reset(); stroke_original_chunks_.reset(); stroke_asset_region_.reset(); stroke_compact_ = false;
    if (rasterOnly && !raster_.strokeActive()) {
        draft_.authorship = file_.document()->authorship;
        draft_.formatVersion = file_.document()->formatVersion;
    } else {
        draft_ = *file_.document();
    }
    stroke_before_.clear();
    bind();
}
bool Artwork::shape(std::string kind, Point from, Point to, std::uint32_t color, double width,
                    bool filled, PolygonOptions polygon, FrameIndex frame)
{
    return shape_impl(std::move(kind),from,to,color,width,filled,polygon,frame,std::nullopt);
}
bool Artwork::styled_shape(std::string kind, Point from, Point to, std::uint32_t color, ShapeStyle style, FrameIndex frame)
{
    if(kind!="rectangle" && kind!="ellipse")return fail("Styled shape requires a rectangle or ellipse");
    return shape_impl(std::move(kind),from,to,color,0,false,{},frame,style);
}
bool Artwork::shape_impl(std::string kind, Point from, Point to, std::uint32_t color, double width,
                         bool filled, PolygonOptions polygon, FrameIndex frame, std::optional<ShapeStyle> style)
{
    if (kind == "line")
        filled = false;
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) ||
        !std::isfinite(to.y) || !std::isfinite(width) || width < 0)
        return fail("Invalid shape geometry");
    auto left = std::min(from.x, to.x), top = std::min(from.y, to.y),
         w = std::max(1., std::abs(to.x - from.x)), h = std::max(1., std::abs(to.y - from.y));
    if(kind=="polygon") {
        if(frame>=draft_.timeline.frameCount)return fail("The current frame is outside the document timeline");
        auto bounds=polygon_bounds(from,to,polygon);if(!bounds)return fail("Invalid polygon geometry or settings");
        left=bounds->left;top=bounds->top;w=bounds->width;h=bounds->height;
        const auto alpha=std::uint32_t(std::lround(double(color>>24)*polygon.opacity));color=(color&0xffffffU)|(alpha<<24);
        if(!alpha) {error_.clear();return true;}
    }
    if(style) {
        if(frame>=draft_.timeline.frameCount)return fail("The current frame is outside the document timeline");
        PolygonOptions geometry;geometry.from_center=style->from_center;geometry.regular=style->regular;
        geometry.fill=style->fill;geometry.outline=style->outline;geometry.stroke_width=style->stroke_width;geometry.opacity=style->opacity;
        auto bounds=polygon_bounds(from,to,geometry);if(!bounds)return fail("Invalid shape geometry or settings");
        left=bounds->left;top=bounds->top;w=bounds->width;h=bounds->height;
        if(!(color>>24) || style->opacity==0){error_.clear();return true;}
    }
    if (w > 32768 || h > 32768)
        return fail("Shape extent exceeds the document limit");
    const auto padding = style ? (style->outline ? std::ceil(style->stroke_width/2) : 0.) : kind=="polygon" ? (polygon.outline ? std::ceil(polygon.stroke_width/2) : 0.) : (filled ? 0. : std::ceil(width / 2));
    auto path = shape_path(kind, w, h, color, width, filled, polygon, style);
    if (kind == "line")
    {
        path.commands = {MoveTo{{from.x - left, from.y - top}}, LineTo{{to.x - left, to.y - top}}};
    }
    for (auto& command : path.commands)
        std::visit(
            [&](auto& item)
            {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::same_as<T, MoveTo> || std::same_as<T, LineTo>)
                {
                    item.point.x += padding;
                    item.point.y += padding;
                }
                else if constexpr (std::same_as<T, QuadraticTo>)
                {
                    item.control.x += padding;
                    item.control.y += padding;
                    item.end.x += padding;
                    item.end.y += padding;
                }
                else if constexpr (std::same_as<T, CubicTo>)
                {
                    item.control1.x += padding;
                    item.control1.y += padding;
                    item.control2.x += padding;
                    item.control2.y += padding;
                    item.end.x += padding;
                    item.end.y += padding;
                }
            },
            command);
    std::optional<RasterLayer> selectionPixels;
    auto origin = canvasOrigin(draft_);
    if (selection_) {
        try {
            const RasterMask* coverage = nullptr;
            std::optional<RasterMask> materialized;
            if (selection_->tiled) {
                if (selection_->tiled->domain() != CanvasRegion{origin, draft_.extent})
                    return fail("Selection no longer matches the canvas");
                const auto bounds = selection_->tiled->nonzeroBounds();
                if (!bounds) { error_.clear(); return true; }
                // Restrict the persistent mask to the shape's conservative pixel bounds.
                // A small shape must not materialize an inverted mask across an enormous canvas.
                const auto x0 = std::max(double(bounds->origin.x), std::floor(left - padding - 2));
                const auto y0 = std::max(double(bounds->origin.y), std::floor(top - padding - 2));
                const auto x1 = std::min(double(bounds->origin.x) + bounds->extent.width, std::ceil(left + w + padding + 2));
                const auto y1 = std::min(double(bounds->origin.y) + bounds->extent.height, std::ceil(top + h + padding + 2));
                if (x1 <= x0 || y1 <= y0) { error_.clear(); return true; }
                if (x0 < std::numeric_limits<int>::min() || x0 > std::numeric_limits<int>::max() ||
                    y0 < std::numeric_limits<int>::min() || y0 > std::numeric_limits<int>::max() ||
                    x1 - x0 > std::numeric_limits<int>::max() || y1 - y0 > std::numeric_limits<int>::max())
                    return fail("Selected shape exceeds the coordinate limit");
                const CanvasRegion region{{int(x0), int(y0)}, {int(x1 - x0), int(y1 - y0)}};
                const auto pixels = std::uint64_t(region.extent.width) * region.extent.height;
                if (pixels > 64ULL * 1024 * 1024 || pixels > history_budget_ / 8)
                    return fail("Selected shape exceeds the working or history budget");
                auto result = selection_->tiled->materialize(region);
                if (!result) return fail(result.error());
                materialized = std::move(*result);
                coverage = &*materialized;
                origin = region.origin;
            } else {
                if (!selection_->dense || draft_.canvasMode != CanvasMode::Finite || selection_->dense->extent != draft_.extent)
                    return fail("Selection no longer matches the canvas");
                coverage = selection_->dense.get();
            }
            if (std::none_of(coverage->alpha.begin(), coverage->alpha.end(),
                             [](std::uint8_t alpha) { return alpha != 0; })) { error_.clear(); return true; }
            selectionPixels = makeRasterLayer(coverage->extent.width, coverage->extent.height, 0);
            for (std::size_t i = 0; i < coverage->alpha.size(); ++i)
                selectionPixels->pixels[i] = (std::uint32_t(coverage->alpha[i]) << 24) | 0xffffffU;
        } catch (const std::bad_alloc&) { return fail("Insufficient memory for shape selection"); }
    }
    const auto asset = uuid(), key = uuid(), maskAsset = selectionPixels ? uuid() : std::string{};
    return edit(
        [&](DocumentEditor& e)
        {
            if (selectionPixels && !e.insertRasterAsset(maskAsset, std::move(*selectionPixels)).ok())
                return false;
            if (!e.insertVectorAsset(
                      asset, {int(std::ceil(w + 2 * padding)), int(std::ceil(h + 2 * padding))},
                      {std::move(path)})
                     .ok())
                return false;
            LayerProperties p{key, kind};
            if(style)p.opacity=style->opacity;
            p.transform.translationX = left - padding;
            p.transform.translationY = top - padding;
            if(kind=="polygon" || style)p.frameRange=LayerFrameRange{frame,frame};
            if (selection_) {
                iiSharedCanvas::LayerMask mask;
                mask.assetId = maskAsset;
                mask.channel = MaskChannel::Alpha;
                mask.linkedToLayer = true;
                mask.outsideCoverage = 0;
                mask.transform.translationX = origin.x - p.transform.translationX;
                mask.transform.translationY = origin.y - p.transform.translationY;
                p.appearance.rasterMask = std::move(mask);
            }
            if (!e.insertLayer(StaticVectorLayer{std::move(p), StaticSource{asset}}).ok())
                return false;
            selected_ = key;
            return true;
        });
}

bool Artwork::apply_raster_insertion(const RasterInsertion& record, bool forward)
{
    DocumentEditor editor(draft_);
    if(forward) {
        if(findAsset(draft_,record.asset_id) || layer(draft_,record.layer_id) ||
           record.asset_index>draft_.assets.size() || record.layer_index>draft_.layers.size())
            return fail("Inserted raster layer can no longer be restored");
        auto asset=std::visit([&](const auto& pixels) {
            using T=std::decay_t<decltype(pixels)>;
            if constexpr(std::is_same_v<T,::RasterLayer>) return editor.insertRasterAsset(record.asset_id,pixels,record.asset_index);
            else return editor.insertChunkedRasterAsset(pixels,record.asset_index);
        },record.pixels);
        if(!asset.ok())return fail(asset.message.empty()?"Cannot restore raster asset":asset.message);
        LayerProperties properties{record.layer_id,record.name};
        properties.transform.translationX=record.origin.x;properties.transform.translationY=record.origin.y;
        properties.frameRange=LayerFrameRange{record.frame,record.frame};properties.blendMode=record.blend;
        auto inserted=editor.insertLayer(StaticBitmapLayer{std::move(properties),StaticSource{record.asset_id}},record.layer_index);
        if(!inserted.ok())return fail(inserted.message.empty()?"Cannot restore raster layer":inserted.message);
    } else {
        const auto* item=layer(draft_,record.layer_id);
        const auto* source=item ? staticLayerSource(*item) : nullptr;
        if(!item || !std::holds_alternative<StaticBitmapLayer>(*item) || !source || source->assetId!=record.asset_id ||
           (std::holds_alternative<::RasterLayer>(record.pixels) ? !findRasterAsset(draft_,record.asset_id) : !findChunkedRasterAsset(draft_,record.asset_id)) || assetReferences(draft_,record.asset_id).size()!=1)
            return fail("Inserted raster layer has unexpected references");
        auto removed=editor.removeLayer(record.layer_id);
        if(!removed.ok())return fail(removed.message.empty()?"Cannot remove raster layer":removed.message);
        auto asset=editor.removeAsset(record.asset_id);
        if(!asset.ok())return fail(asset.message.empty()?"Cannot remove raster asset":asset.message);
    }
    return true;
}
bool Artwork::insert_raster_result(std::variant<::RasterLayer,ChunkedRasterAsset> pixels, std::string name, CanvasOrigin origin,
                                   FrameIndex frame, RasterBlendMode blend)
{
    if(stroke_active())return fail("Finish the current stroke before inserting a layer");
    const auto previous=selected_;bool pendingInserted=false;
    const auto rollback=[&](std::string reason){draft_=*file_.document();selected_=previous;bind();return fail(std::move(reason));};
    try {
        auto assetId=uuid();if(auto* chunks=std::get_if<ChunkedRasterAsset>(&pixels))chunks->id=assetId;
        auto record=std::make_shared<const RasterInsertion>(RasterInsertion{std::move(assetId),uuid(),std::move(name),std::move(pixels),origin,frame,blend,draft_.assets.size(),draft_.layers.size()});
        History pending;pending.before_selection=previous;pending.after_selection=record->layer_id;pending.raster_insertion=record;
        const auto cost=pending.bytes();if(cost>history_budget_)return fail("Raster insertion exceeds the undo budget");
        if(!apply_raster_insertion(*record,true)) {auto reason=error_;return rollback(reason);}
        selected_=record->layer_id;
        if(draft_.authorship.dump()==file_.document()->authorship.dump())recordDocumentChange(draft_);
        undo_.push_back(std::move(pending));pendingInserted=true;
        const auto committed=file_.edit([&](Document& document){document=draft_;return true;});
        if(!committed.ok() || !committed.changed) {undo_.pop_back();pendingInserted=false;return rollback(committed.ok()?"Raster insertion did not change the document":committed.message);}
        pendingInserted=false;
        for(const auto& entry:redo_)history_bytes_-=entry.bytes();redo_.clear();history_bytes_+=cost;
        while(history_bytes_>history_budget_ || undo_.size()>HistoryCount){history_bytes_-=undo_.front().bytes();undo_.pop_front();}
        try {bind();} catch(const std::bad_alloc&) {raster_.unbind();chunks_.unbind();}
        error_.clear();
    } catch(const std::bad_alloc&) {if(pendingInserted)undo_.pop_back();return rollback("Insufficient memory for raster insertion history");}
    if(changed)changed();return true;
}

bool Artwork::sparse_gradient(GradientDefinition definition, FrameIndex frame, RasterBlendMode blend)
{
    // Finite canvas regions use a zero origin and 256px chunks. Enumerated
    // 64px selection tiles align with chunk boundaries, so no empty span is materialized.
    if(!selection_ || !selection_->tiled || draft_.canvasMode!=CanvasMode::Finite || canvasOrigin(draft_)!=CanvasOrigin{})
        return fail("Sparse gradient requires a finite tiled selection");
    auto regions=selection_->tiled->nonzeroRegions(65536);
    if(!regions)return fail(regions.error());
    SparseRasterBuilder builder(history_budget_);
    for(const auto& region:*regions) {
        auto coverage=selection_->tiled->materialize(region);if(!coverage)return fail(coverage.error());
        auto local=definition;local.start.x-=region.origin.x;local.start.y-=region.origin.y;
        local.end.x-=region.origin.x;local.end.y-=region.origin.y;
        auto rendered=gradientRaster(region.extent,local);if(!rendered.ok())return fail(rendered.error);
        auto masked=maskRaster(rendered.pixels,*coverage);if(!masked.ok())return fail(masked.error);
        if(!builder.add(region,masked.pixels))return fail(builder.error());
    }
    if(builder.empty()){error_.clear();return true;}
    auto result=builder.take();
    return insert_raster_result(std::move(result),"Gradient",{},frame,blend);
}

bool Artwork::gradient(GradientDefinition definition, FrameIndex frame, RasterBlendMode blend)
{
    if (stroke_active()) return fail("Finish the current stroke before applying a gradient");
    if (frame >= draft_.timeline.frameCount) return fail("The current frame is outside the document timeline");
    const auto origin=canvasOrigin(draft_);
    CanvasRegion region{origin,draft_.extent};
    std::optional<RasterMask> boundedMask;
    const RasterMask* mask=nullptr;
    try {
        if (selection_ && selection_->tiled) {
            if (selection_->tiled->domain()!=region) return fail("Selection no longer matches the canvas");
            auto bounds=selection_->tiled->nonzeroBounds();
            if (!bounds) { error_.clear(); return true; }
            region=*bounds;
            const auto pixels=std::uint64_t(region.extent.width)*region.extent.height;
            if (pixels>64ULL*1024*1024 || pixels>history_budget_/8)
                return sparse_gradient(std::move(definition),frame,blend);
            auto materialized=selection_->tiled->materialize(region);
            if (!materialized) return fail(materialized.error());
            boundedMask=std::move(*materialized);mask=&*boundedMask;
        } else {
            if (draft_.canvasMode == CanvasMode::Infinite) {
                if (selection_) return fail("Selection no longer matches the canvas");
                const auto pixels=std::uint64_t(region.extent.width)*region.extent.height;
                if (pixels>64ULL*1024*1024 || pixels>history_budget_/8)
                    return fail("Allocated gradient exceeds the working or history budget");
            }
            if (selection_) {
                if (!selection_->dense || selection_->dense->extent!=draft_.extent) return fail("Selection no longer matches the canvas");
                mask=selection_->dense.get();
            }
        }
        definition.start.x-=region.origin.x;definition.start.y-=region.origin.y;
        definition.end.x-=region.origin.x;definition.end.y-=region.origin.y;
        auto result=gradientRaster(region.extent,definition);
        if (!result.ok()) return fail(result.error);
        if (mask) {
            auto masked=maskRaster(result.pixels,*mask);
            if (!masked.ok()) return fail(masked.error);
            result.pixels=std::move(masked.pixels);
        }
        if (std::none_of(result.pixels.pixels.begin(),result.pixels.pixels.end(),
                         [](std::uint32_t pixel){return (pixel>>24)!=0;})) {error_.clear();return true;}
        return insert_raster_result(std::move(result.pixels),"Gradient",region.origin,frame,blend);
    } catch(const std::bad_alloc&) {return fail("Insufficient memory for gradient");}
}

bool Artwork::fill(std::uint32_t color)
{
    return shape("rectangle", {0, 0}, {double(draft_.extent.width), double(draft_.extent.height)},
                 color, 0, true);
}
bool Artwork::sparse_fill(const TiledMask& flood,std::uint32_t color,FrameIndex frame)
{
    if(draft_.canvasMode!=CanvasMode::Finite || canvasOrigin(draft_)!=CanvasOrigin{})return fail("Sparse fill requires a finite canvas");
    auto floodRegions=flood.nonzeroRegions(65536);
    std::optional<std::vector<CanvasRegion>> selectedRegions;
    if(selection_ && selection_->tiled) {
        auto candidate=selection_->tiled->nonzeroRegions(65536);
        if(candidate)selectedRegions=std::move(*candidate);
    }
    if(!floodRegions && !selectedRegions)return fail(floodRegions.error());
    const auto& regions=selectedRegions && (!floodRegions || selectedRegions->size()<floodRegions->size()) ? *selectedRegions : *floodRegions;
    SparseRasterBuilder builder(history_budget_);std::uint64_t visited=0;
    for(const auto& region:regions) {
        visited+=std::uint64_t(region.extent.width)*region.extent.height;
        if(visited>64ULL*1024*1024)return fail("Sparse fill exceeds the working budget");
        auto pixels=makeRasterLayer(region.extent.width,region.extent.height,0);
        for(int y=0;y<region.extent.height;++y)for(int x=0;x<region.extent.width;++x) {
            const int px=region.origin.x+x,py=region.origin.y+y;
            const auto selected=selection_ ? unsigned(selection_coverage(double(px)+.5,double(py)+.5)) : 255U;
            const auto alpha=(unsigned(color>>24)*selected*unsigned(flood.coverage(px,py))+32512)/65025;
            pixels.pixels[std::size_t(y)*region.extent.width+x]=alpha?(color&0xffffffU)|(alpha<<24):0;
        }
        if(!builder.add(region,pixels))return fail(builder.error());
    }
    if(builder.empty()){error_.clear();return true;}
    return insert_raster_result(builder.take(),"Fill",{},frame,RasterBlendMode::SourceOver);
}

bool Artwork::fill_tiled_new_layer(Point point,std::uint32_t color,FrameIndex frame,FloodOptions options)
{
    try {
        TiledMask flood;if(!tiled_wand_mask(point,frame,options.tolerance,options.contiguous,options.antialias,options.current_layer,flood))return false;
        auto bounds=flood.nonzeroBounds();if(!bounds){error_.clear();return true;}
        if(selection_) {
            auto selectedBounds=selection_bounds();if(!selectedBounds){error_.clear();return true;}
            bounds=intersectCanvasRegions(*bounds,*selectedBounds);
            if(!bounds){error_.clear();return true;}
        }
        const auto area=std::uint64_t(bounds->extent.width)*bounds->extent.height;
        if(area>64ULL*1024*1024 || area>history_budget_/8)return sparse_fill(flood,color,frame);
        auto flooded=flood.materialize(*bounds);if(!flooded)return fail(flooded.error());
        auto pixels=makeRasterLayer(bounds->extent.width,bounds->extent.height,0);
        for(std::size_t i=0;i<pixels.pixels.size();++i) {
            const auto x=bounds->origin.x+int(i%std::size_t(bounds->extent.width));
            const auto y=bounds->origin.y+int(i/std::size_t(bounds->extent.width));
            const auto coverage=selection_ ? unsigned(selection_coverage(double(x)+.5,double(y)+.5)) : 255U;
            const auto alpha=(unsigned(color>>24)*coverage*unsigned(flooded->alpha[i])+32512)/65025;
            pixels.pixels[i]=alpha ? (color&0xffffffU)|(alpha<<24) : 0;
        }
        if(std::none_of(pixels.pixels.begin(),pixels.pixels.end(),[](std::uint32_t p){return (p>>24)!=0;})) {error_.clear();return true;}
        return insert_raster_result(std::move(pixels),"Fill",bounds->origin,frame,RasterBlendMode::SourceOver);
    } catch(const std::bad_alloc&) {return fail("Insufficient memory for tiled fill");}
}
bool Artwork::fill_tiled_chunks(Point point,std::uint32_t color,FrameIndex frame,FloodOptions options,
                                const ChunkedRasterAsset& asset,const AffineTransform& transform)
{
    const auto* selected=layer(draft_,selected_);
    if(!selected ||
       (!std::holds_alternative<StaticBitmapLayer>(*selected) && !std::holds_alternative<DynamicBitmapLayer>(*selected)))
        return fail("Direct chunk fill requires a bitmap layer");
    const auto range=layerProperties(*selected).frameRange;
    const bool isolated=draft_.timeline.frameCount==1 || (range && range->firstFrame==frame && range->lastFrame==frame);
    const bool copy_frame=std::holds_alternative<DynamicBitmapLayer>(*selected) || !isolated || assetReferences(draft_,asset.id).size()>1;
    try {
        TiledMask flood;if(!tiled_wand_mask(point,frame,options.tolerance,options.contiguous,options.antialias,options.current_layer,flood))return false;
        auto bounds=flood.nonzeroBounds();if(!bounds){error_.clear();return true;}
        if(selection_) {
            auto selectedBounds=selection_bounds();if(!selectedBounds){error_.clear();return true;}
            bounds=intersectCanvasRegions(*bounds,*selectedBounds);
            if(!bounds){error_.clear();return true;}
        }
        const double worldLeft=bounds->origin.x,worldTop=bounds->origin.y;
        const double worldRight=worldLeft+bounds->extent.width,worldBottom=worldTop+bounds->extent.height;
        const std::array corners{localPoint({worldLeft,worldTop},transform),localPoint({worldRight,worldTop},transform),
                                 localPoint({worldLeft,worldBottom},transform),localPoint({worldRight,worldBottom},transform)};
        double localLeft=std::numeric_limits<double>::infinity(),localTop=localLeft,localRight=-localLeft,localBottom=-localLeft;
        for(const auto& corner:corners) {
            if(!corner)return fail("Cannot fill through a degenerate layer transform");
            localLeft=std::min(localLeft,corner->x);localTop=std::min(localTop,corner->y);
            localRight=std::max(localRight,corner->x);localBottom=std::max(localBottom,corner->y);
        }
        localLeft=std::floor(localLeft);localTop=std::floor(localTop);localRight=std::ceil(localRight);localBottom=std::ceil(localBottom);
        if(localLeft < -1000000000. || localTop < -1000000000. || localRight > 1000000000. || localBottom > 1000000000. ||
           localRight<=localLeft || localBottom<=localTop)return fail("Transformed fill exceeds the asset coordinate range");
        const CanvasRegion local{{int(localLeft),int(localTop)},{int(localRight-localLeft),int(localBottom-localTop)}};
        const auto area=std::uint64_t(local.extent.width)*local.extent.height;
        std::vector<CanvasRegion> regions;
        if(area>64ULL*1024*1024 || area>history_budget_/8) {
            if(draft_.canvasMode!=CanvasMode::Finite || transform.m11!=1 || transform.m12!=0 || transform.m21!=0 || transform.m22!=1 || transform.translationX!=0 || transform.translationY!=0)
                return fail("Direct chunk fill exceeds the working or history budget");
            auto flooded=flood.nonzeroRegions(65536);std::optional<std::vector<CanvasRegion>> selectedRegions;
            if(selection_ && selection_->tiled) {auto candidate=selection_->tiled->nonzeroRegions(65536);if(candidate)selectedRegions=std::move(*candidate);}
            if(!flooded && !selectedRegions)return fail(flooded.error());
            regions=selectedRegions && (!flooded || selectedRegions->size()<flooded->size()) ? std::move(*selectedRegions) : std::move(*flooded);
        } else regions.push_back(local);
        auto before=capture();const bool compact=before.empty();
        std::optional<ChunkFrameHistory> frameHistory;
        raster_.unbind();chunks_.unbind();
        if(!chunks_.bind(draft_,asset.id))return fail("Cannot bind the chunk fill target");
        std::vector<::RasterLayer> buffers;std::vector<CanvasRegion> changedRegions;
        std::uint64_t visited=0,stored=0;
        for(const auto& part:regions) {
            visited+=std::uint64_t(part.extent.width)*part.extent.height;
            if(visited>64ULL*1024*1024){bind();return fail("Direct chunk fill exceeds the working budget");}
            auto pixels=makeRasterLayer(part.extent.width,part.extent.height,0);bool changed=false;
            for(int y=0;y<part.extent.height;++y)for(int x=0;x<part.extent.width;++x) {
                const int ax=part.origin.x+x,ay=part.origin.y+y;const auto old=chunks_.assetPixelAt(ax,ay);
                if(!old){bind();return fail("Chunk fill lies outside the supported asset coordinate range");}
                auto& destination=pixels.pixels[std::size_t(y)*part.extent.width+x];destination=*old;
                const double wx=transform.m11*(ax+.5)+transform.m21*(ay+.5)+transform.translationX;
                const double wy=transform.m12*(ax+.5)+transform.m22*(ay+.5)+transform.translationY;
                if(!std::isfinite(wx)||!std::isfinite(wy)||wx<worldLeft||wy<worldTop||wx>=worldRight||wy>=worldBottom)continue;
                const auto coverage=unsigned(flood.coverage(int(std::floor(wx)),int(std::floor(wy))))*selection_coverage(wx,wy);
                const auto alpha=(unsigned(color>>24)*coverage+32512)/65025;if(!alpha)continue;
                const auto oldAlpha=*old>>24;if(options.preserve_alpha && !oldAlpha)continue;
                const auto ink=(color&0xffffffU)|(alpha<<24);
                auto result=compositeRasterPixel(ink,options.preserve_alpha ? *old|0xff000000U : *old,RasterBlendMode::SourceOver);
                if(options.preserve_alpha)result=(oldAlpha<<24)|(result&0xffffffU);
                if(result!=*old){destination=result;changed=true;}
            }
            if(changed) {
                stored+=std::uint64_t(part.extent.width)*part.extent.height;
                if(stored>history_budget_/8){bind();return fail("Direct chunk fill exceeds the history budget");}
                changedRegions.push_back(part);buffers.push_back(std::move(pixels));
            }
        }
        if(buffers.empty()){bind();error_.clear();return true;}
        if(copy_frame) {
            auto content=asset;DocumentEditor editor(draft_);std::string replacement;
            auto sourceBefore=compact ? capture_frame_source(draft_,selected_) : std::optional<FrameSourceHistory>{};
            if(compact && !sourceBefore) {bind();return fail("Compact frame source is unavailable");}
            const auto prior=content.id;
            if(!replace_frame_chunked(editor,frame,std::move(content),replacement)) {auto reason=error_;draft_=*file_.document();bind();return fail(reason);}
            if(compact) {
                auto sourceAfter=capture_frame_source(draft_,selected_);
                if(!sourceAfter) {draft_=*file_.document();bind();return fail("Compact frame source is unavailable");}
                frameHistory=ChunkFrameHistory{selected_,prior,replacement,std::move(*sourceBefore),std::move(*sourceAfter)};
            }
            chunks_.unbind();
            if(!chunks_.bind(draft_,replacement)) {draft_=*file_.document();bind();return fail("Cannot bind the copied chunk frame");}
        }
        std::vector<BitmapPatch> patches;patches.reserve(buffers.size());
        for(std::size_t i=0;i<buffers.size();++i)patches.push_back({{{changedRegions[i].origin.x,changedRegions[i].origin.y},changedRegions[i].extent.width,changedRegions[i].extent.height},buffers[i].pixels});
        if(!chunks_.replaceAssetPatches(patches)) {auto reason=chunks_.lastError();draft_=*file_.document();bind();return fail(reason);}
        if(compact)return commit_compact_chunk_history(std::make_shared<ChunkedBitmapEditor>(chunks_),selected_,std::nullopt,std::nullopt,std::move(frameHistory));
        return commit(std::move(before),selected_);
    } catch(const std::bad_alloc&) {draft_=*file_.document();bind();return fail("Insufficient memory for chunk fill");}
}
bool Artwork::fill_tiled_selection(Point point, std::uint32_t color, FrameIndex frame, FloodOptions options)
{
    if (stroke_active()) return fail("Finish the current stroke before filling");
    if (selection_ && ((selection_->tiled && selection_->tiled->domain()!=CanvasRegion{canvasOrigin(draft_),draft_.extent}) ||
                       (selection_->dense && selection_->dense->extent!=draft_.extent) ||
                       (!selection_->tiled && !selection_->dense)))
        return fail("Selection no longer matches the canvas");
    if (frame>=draft_.timeline.frameCount) return fail("The current frame is outside the document timeline");
    if (!std::isfinite(options.tolerance) || options.tolerance<0 || options.tolerance>1) return fail("Invalid fill tolerance");
    const auto origin=canvasOrigin(draft_);
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x<origin.x || point.y<origin.y ||
        point.x>=double(origin.x)+draft_.extent.width || point.y>=double(origin.y)+draft_.extent.height)
        return fail("Choose a point inside the canvas");
    if (!(color>>24)) {error_.clear();return true;}
    if (!options.target_current) return fill_tiled_new_layer(point,color,frame,options);
    const auto* selected=layer(draft_,selected_);
    if (!selected || (!std::holds_alternative<StaticBitmapLayer>(*selected) && !std::holds_alternative<DynamicBitmapLayer>(*selected)))
        return fail("Select a bitmap layer to fill directly");
    const auto sample=sampleLayerAt(draft_,*selected,frame);
    if (!sample.visible) return fail("The selected bitmap layer is not visible on this frame");
    if (!localPoint({0,0},sample.transform)) return fail("Cannot fill through a degenerate layer transform");
    const auto* resolved=resolveAssetAt(draft_,*selected,frame);
    if (!layerProperties(*selected).appearance.filters.empty()) return fail("Tiled selection fill does not yet support layer filters");
    if(const auto* chunks=resolved ? std::get_if<ChunkedRasterAsset>(resolved) : nullptr)
        return fill_tiled_chunks(point,color,frame,options,*chunks,sample.transform);
    const auto* asset=resolved ? std::get_if<RasterAsset>(resolved) : nullptr;
    if (!asset) return fail("The current frame has no editable bitmap");
    const auto& original=asset->pixels;
    if (original.width<=0 || original.height<=0 || std::uint64_t(original.width)*original.height>64ULL*1024*1024)
        return fail("Direct fill exceeds the 64 megapixel working budget");
    try {
        const auto& t=sample.transform;TiledMask flood;
        if(!tiled_wand_mask(point,frame,options.tolerance,options.contiguous,options.antialias,options.current_layer,flood))return false;
        auto pixels=original;bool changed=false;
        for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x) {
            const double wx=t.m11*(x+.5)+t.m21*(y+.5)+t.translationX,wy=t.m12*(x+.5)+t.m22*(y+.5)+t.translationY;
            if(!std::isfinite(wx)||!std::isfinite(wy)||wx<origin.x||wy<origin.y||
                wx>=double(origin.x)+draft_.extent.width||wy>=double(origin.y)+draft_.extent.height)continue;
            const auto coverage=unsigned(flood.coverage(int(std::floor(wx)),int(std::floor(wy))))*selection_coverage(wx,wy);
            const auto alpha=(unsigned(color>>24)*coverage+32512)/65025;if(!alpha)continue;
            auto& destination=pixels.pixels[std::size_t(y)*pixels.width+x];const auto old=destination,oldAlpha=old>>24;
            if(options.preserve_alpha && !oldAlpha)continue;
            const auto ink=(color&0xffffffU)|(alpha<<24);
            auto result=compositeRasterPixel(ink,options.preserve_alpha ? old|0xff000000U : old,RasterBlendMode::SourceOver);
            if(options.preserve_alpha)result=(oldAlpha<<24)|(result&0xffffffU);
            if(result!=old){destination=result;changed=true;}
        }
        if(!changed){error_.clear();return true;}
        if(capture().empty())return fail("Fill exceeds the undo budget");
        return edit([&](DocumentEditor& editor){std::string replacement;return replace_frame_bitmap(editor,frame,std::move(pixels),replacement) && !capture().empty();});
    } catch(const std::bad_alloc&) {return fail("Insufficient memory for tiled fill");}
}
bool Artwork::fill_at(Point point, std::uint32_t color, FrameIndex frame, FloodOptions options)
{
    const auto* target=layer(draft_,selected_);
    const auto* targetAsset=target && frame<draft_.timeline.frameCount ? resolveAssetAt(draft_,*target,frame) : nullptr;
    const bool chunkTarget=options.target_current && targetAsset && std::holds_alternative<ChunkedRasterAsset>(*targetAsset);
    if (chunkTarget || tiled_selection_handle() || draft_.canvasMode==CanvasMode::Infinite ||
        std::uint64_t(draft_.extent.width)*draft_.extent.height>64ULL*1024*1024)
        return fill_tiled_selection(point,color,frame,options);
    if (stroke_active()) return fail("Finish the current stroke before filling");
    if (draft_.canvasMode != CanvasMode::Finite) return fail("Flood fill currently requires a finite canvas");
    if (selection_ && selection_->dense->extent != draft_.extent) return fail("Selection no longer matches the canvas");
    if (frame >= draft_.timeline.frameCount) return fail("The current frame is outside the document timeline");
    const auto origin = canvasOrigin(draft_);
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < origin.x || point.y < origin.y ||
        point.x >= origin.x + draft_.extent.width || point.y >= origin.y + draft_.extent.height)
        return fail("Choose a point inside the canvas");
    if (std::uint64_t(draft_.extent.width) * draft_.extent.height > 64ULL * 1024 * 1024)
        return fail("Flood fill exceeds the 64 megapixel working budget");
    try {
        RasterLayer reference;
        if (!reference_pixels(frame, options.current_layer, reference)) return false;
        auto mask = selectionWand(reference, {int(std::floor(point.x-origin.x)), int(std::floor(point.y-origin.y))},
                                  options.tolerance, options.contiguous);
        if (!mask.ok()) return fail(mask.error);
        if (options.antialias) {
            auto softened = antialiasBinaryMask(mask.mask);
            if (!softened.ok()) return fail(softened.error);
            mask = std::move(softened);
        }
        if (options.target_current) {
            const auto* selected = layer(draft_, selected_);
            if (!selected || (!std::holds_alternative<StaticBitmapLayer>(*selected) &&
                              !std::holds_alternative<DynamicBitmapLayer>(*selected)))
                return fail("Select a bitmap layer to fill directly");
            const auto sample = sampleLayerAt(draft_, *selected, frame);
            if (!sample.visible) return fail("The selected bitmap layer is not visible on this frame");
            if (!localPoint({0, 0}, sample.transform)) return fail("Cannot fill through a degenerate layer transform");
            const auto* resolved = resolveAssetAt(draft_, *selected, frame);
            const auto* asset = resolved ? std::get_if<RasterAsset>(resolved) : nullptr;
            if (!asset) return fail("The current frame has no editable bitmap");
            if (std::uint64_t(asset->pixels.width) * asset->pixels.height > 64ULL * 1024 * 1024)
                return fail("Direct fill exceeds the 64 megapixel working budget");
            auto pixels = asset->pixels;
            bool changed = false;
            for (int y=0; y<pixels.height; ++y) for (int x=0; x<pixels.width; ++x) {
                const auto& t = sample.transform;
                const double wx=t.m11*(x+.5)+t.m21*(y+.5)+t.translationX-origin.x;
                const double wy=t.m12*(x+.5)+t.m22*(y+.5)+t.translationY-origin.y;
                if (!std::isfinite(wx) || !std::isfinite(wy) || wx<0 || wy<0 || wx>=draft_.extent.width || wy>=draft_.extent.height) continue;
                const auto world = std::size_t(wy)*draft_.extent.width + std::size_t(wx);
                const auto coverage = unsigned(mask.mask.alpha[world]) * (selection_ ? selection_->dense->alpha[world] : 255U);
                const auto alpha = (unsigned(color >> 24) * coverage + 32512) / 65025;
                if (!alpha) continue;
                auto& destination = pixels.pixels[std::size_t(y)*pixels.width+x];
                const auto old = destination, oldAlpha = old >> 24;
                if (options.preserve_alpha && !oldAlpha) continue;
                const auto ink = (color & 0xffffffU) | (alpha << 24);
                auto result = compositeRasterPixel(ink, options.preserve_alpha ? old | 0xff000000U : old, RasterBlendMode::SourceOver);
                if (options.preserve_alpha) result = (oldAlpha << 24) | (result & 0xffffffU);
                if (result != old) { destination = result; changed = true; }
            }
            if (!changed) return true;
            if (capture().empty()) return fail("Fill exceeds the undo budget");
            return edit([&](DocumentEditor& editor) {
                std::string replacement;
                if (!replace_frame_bitmap(editor, frame, std::move(pixels), replacement)) return false;
                return !capture().empty();
            });
        }
        auto pixels = makeRasterLayer(draft_.extent.width, draft_.extent.height, 0);
        for (std::size_t i = 0; i < pixels.pixels.size(); ++i) {
            const auto alpha = selection_ ?
                (unsigned(color >> 24) * mask.mask.alpha[i] * selection_->dense->alpha[i] + 32512) / 65025 :
                unsigned(color >> 24) * mask.mask.alpha[i] / 255;
            pixels.pixels[i] = alpha ? (color & 0xffffffU) | (alpha << 24) : 0;
        }
        if (std::none_of(pixels.pixels.begin(), pixels.pixels.end(), [](std::uint32_t p) { return (p >> 24) != 0; })) return true;
        if (capture().empty()) return fail("Fill exceeds the undo budget");
        const auto asset = uuid(), key = uuid();
        return edit([&](DocumentEditor& editor) {
            if (!editor.insertRasterAsset(asset, std::move(pixels)).ok()) return false;
            LayerProperties properties{key, "Fill"};
            properties.transform.translationX = origin.x; properties.transform.translationY = origin.y;
            properties.frameRange = LayerFrameRange{frame, frame};
            if (!editor.insertLayer(StaticBitmapLayer{std::move(properties), StaticSource{asset}}).ok()) return false;
            if (capture().empty()) return false;
            selected_ = key; return true;
        });
    } catch (const std::bad_alloc&) { return fail("Insufficient memory for flood fill"); }
}
bool Artwork::apply_history(bool forward)
{
    cancel();
    auto& source = forward ? redo_ : undo_;
    auto& destination = forward ? undo_ : redo_;
    if (source.empty())
        return false;
    auto& entry = source.back();
    if (entry.raster_editor) {
        bool pendingInserted = false;
        const auto rollback = [&](std::string reason) {
            draft_ = *file_.document();
            bind();
            return fail(std::move(reason));
        };
        try {
            if (!entry.bitmap_metadata) return rollback("Raster history metadata is unavailable");
            auto trial = std::make_shared<BitmapEditor>(*entry.raster_editor);
            if (!(forward ? trial->redo() : trial->undo())) return rollback(trial->lastError());
            auto target = forward ? entry.after_selection : entry.before_selection;
            draft_.authorship = forward ? entry.bitmap_metadata->after : entry.bitmap_metadata->before;
            draft_.formatVersion = forward ? entry.bitmap_metadata->after_version : entry.bitmap_metadata->before_version;
            History next = entry;
            next.raster_editor = std::move(trial);
            destination.push_back(std::move(next)); pendingInserted = true;
            bool saved = false;
            std::string failure;
            auto& pending = destination.back();
            const auto& expected = forward ? pending.bitmap_metadata->before : pending.bitmap_metadata->after;
            const auto version = forward ? pending.bitmap_metadata->before_version : pending.bitmap_metadata->after_version;
            if (currentBitmapFormat(pending.bitmap_metadata->before_version) &&
                currentBitmapFormat(pending.bitmap_metadata->after_version)) {
                saved = pending.raster_editor->publishHistoryState(file_,
                    forward ? BitmapHistoryState::Applied : BitmapHistoryState::Undone,
                    expected, version, file_.revision());
                if (!saved) failure = pending.raster_editor->lastError();
            } else {
                const auto result = file_.edit([&](Document& document) { document = draft_; return true; });
                saved = result.ok() && result.changed;
                if (!saved) failure = result.ok() ? "Raster history did not change the document" : result.message;
            }
            if (!saved) {
                destination.pop_back(); pendingInserted = false;
                return rollback(failure.empty() ? "Raster history could not be saved" : std::move(failure));
            }
            pendingInserted = false;
            selected_ = std::move(target); source.pop_back();
            try { bind(); } catch (const std::bad_alloc&) { raster_.unbind(); chunks_.unbind(); }
            error_.clear();
        } catch (const std::bad_alloc&) {
            if (pendingInserted) destination.pop_back();
            return rollback("Insufficient memory for raster history");
        }
        if (changed) changed();
        return true;
    }
    if (entry.raster_insertion) {
        bool pendingInserted=false;
        try {
            if(!apply_raster_insertion(*entry.raster_insertion,forward)) {
                auto reason=error_;draft_=*file_.document();bind();return fail(std::move(reason));
            }
            auto target=forward?entry.after_selection:entry.before_selection;
            if(draft_.authorship.dump()==file_.document()->authorship.dump())recordDocumentChange(draft_);
            destination.push_back(entry);pendingInserted=true;
            const auto committed=file_.edit([&](Document& document){document=draft_;return true;});
            if(!committed.ok() || !committed.changed) {destination.pop_back();pendingInserted=false;draft_=*file_.document();bind();return fail(committed.ok()?"Raster insertion history did not change the document":committed.message);}
            pendingInserted=false;selected_=std::move(target);source.pop_back();
            try {bind();} catch(const std::bad_alloc&) {raster_.unbind();chunks_.unbind();}
            error_.clear();
        } catch(const std::bad_alloc&) {if(pendingInserted)destination.pop_back();draft_=*file_.document();bind();return fail("Insufficient memory for raster insertion history");}
        if(changed)changed();return true;
    }
    if (entry.tile_editor) {
        bool pendingInserted=false;
        try {
            if(!entry.bitmap_metadata)return fail("Compact history metadata is unavailable");
            auto trial=std::make_shared<ChunkedBitmapEditor>(*entry.tile_editor);
            const auto setCanvas=[&](CanvasRegion region){draft_.infiniteCanvas.origin=region.origin;draft_.extent=region.extent;};
            const auto failFrame=[&](std::string reason) {draft_=*file_.document();bind();return fail(std::move(reason));};
            if(forward && entry.canvas_after)setCanvas(*entry.canvas_after);
            if(forward && entry.chunk_frame) {
                const auto& frame=*entry.chunk_frame;
                const auto* source=findChunkedRasterAsset(draft_,frame.prior);
                if(!source || findAsset(draft_,frame.replacement))return failFrame("Compact frame redo source is unavailable");
                auto copy=*source;copy.id=frame.replacement;DocumentEditor editor(draft_);
                auto inserted=editor.insertChunkedRasterAsset(std::move(copy));
                if(!inserted.ok())return failFrame(inserted.message);
                if(!restore_frame_source(frame.layer,frame.after)) {auto reason=error_;return failFrame(reason);}
                if(assetReferences(draft_,frame.prior).empty()) {
                    auto removed=editor.removeAsset(frame.prior);if(!removed.ok())return failFrame(removed.message);
                }
            }
            if (!(forward ? trial->redo() : trial->undo())) return failFrame(trial->lastError());
            if(!forward && entry.chunk_frame) {
                const auto& frame=*entry.chunk_frame;
                if(!findAsset(draft_,frame.prior)) {
                    const auto* source=findChunkedRasterAsset(draft_,frame.replacement);
                    if(!source)return failFrame("Compact frame undo source is unavailable");
                    auto copy=*source;copy.id=frame.prior;DocumentEditor editor(draft_);
                    auto inserted=editor.insertChunkedRasterAsset(std::move(copy));
                    if(!inserted.ok())return failFrame(inserted.message);
                }
                if(!restore_frame_source(frame.layer,frame.before)) {auto reason=error_;return failFrame(reason);}
                if(!assetReferences(draft_,frame.replacement).empty())return failFrame("Compact frame asset has unexpected references");
                DocumentEditor editor(draft_);auto removed=editor.removeAsset(frame.replacement);
                if(!removed.ok())return failFrame(removed.message);
            }
            if(forward && entry.canvas_after)setCanvas(*entry.canvas_after);
            if(!forward && entry.canvas_before)setCanvas(*entry.canvas_before);
            auto target=forward?entry.after_selection:entry.before_selection;
            draft_.authorship=forward?entry.bitmap_metadata->after:entry.bitmap_metadata->before;
            draft_.formatVersion=forward?entry.bitmap_metadata->after_version:entry.bitmap_metadata->before_version;
            History next=entry;next.tile_editor=std::move(trial);destination.push_back(std::move(next));pendingInserted=true;
            auto& pending=destination.back();bool saved=false;std::string failure;
            if(!pending.chunk_frame && !pending.canvas_before && !pending.canvas_after &&
               currentBitmapFormat(pending.bitmap_metadata->before_version) && currentBitmapFormat(pending.bitmap_metadata->after_version)) {
                const auto& expected=forward?pending.bitmap_metadata->before:pending.bitmap_metadata->after;
                const auto version=forward?pending.bitmap_metadata->before_version:pending.bitmap_metadata->after_version;
                saved=pending.tile_editor->publishHistoryState(file_,forward?BitmapHistoryState::Applied:BitmapHistoryState::Undone,
                    expected,version,file_.revision());
                if(!saved)failure=pending.tile_editor->lastError();
            } else {
                const auto committed=file_.edit([&](Document& document){document=draft_;return true;});
                saved=committed.ok() && committed.changed;
                if(!saved)failure=committed.ok()?"Compact history did not change the document":committed.message;
            }
            if(!saved) {destination.pop_back();pendingInserted=false;draft_=*file_.document();bind();return fail(failure.empty()?"Compact history could not be saved":std::move(failure));}
            pendingInserted=false;selected_=std::move(target);source.pop_back();
            try { bind(); } catch(const std::bad_alloc&) { raster_.unbind();chunks_.unbind(); }
            error_.clear();
        } catch(const std::bad_alloc&) {if(pendingInserted)destination.pop_back();draft_=*file_.document();bind();return fail("Insufficient memory for compact history");}
        if(changed)changed();return true;
    }
    if (entry.mask_change && entry.before.empty() && entry.after.empty()) {
        auto target = forward ? entry.mask_after : entry.mask_before;
        destination.push_back(std::move(entry)); source.pop_back();
        selection_ = std::move(target); error_.clear(); return true;
    }
    auto decoded = decodeIisc(forward ? entry.after : entry.before);
    if (!decoded.ok())
        return fail("History snapshot is invalid");
    auto previous_selection = selected_;
    const auto previous_mask=selection_;
    if (entry.mask_change) selection_=forward ? entry.mask_after : entry.mask_before;
    draft_ = std::move(decoded.document);
    selected_ = forward ? entry.after_selection : entry.before_selection;
    if (!commit({}, std::move(previous_selection), false)) {
        selection_=previous_mask;
        return false;
    }
    destination.push_back(std::move(entry));
    source.pop_back();
    return true;
}
bool Artwork::undo()
{
    return apply_history(false);
}
bool Artwork::redo()
{
    return apply_history(true);
}
std::string Artwork::snapshot() const
{
    auto encoded = encodeIisc(draft_);
    return encoded.ok() ? std::string(reinterpret_cast<const char*>(encoded.bytes.data()),
                                      encoded.bytes.size())
                        : std::string{};
}
bool Artwork::receive(std::string_view bytes)
{
    auto decoded = decodeIisc({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    if (!decoded.ok())
        return fail("Received document is invalid");
    if (decoded.document.canvasMode == CanvasMode::Infinite)
        return fail("Infinite canvas documents are no longer supported");
    if (selection_ && selection_->tiled && selection_->tiled->domain()!=CanvasRegion{canvasOrigin(decoded.document),decoded.document.extent})
        return fail("Received document no longer matches the tiled selection");
    cancel();
    auto before = capture();
    auto previous_selection = selected_;
    draft_ = std::move(decoded.document);
    selected_ = draft_.layers.empty() ? "" : layerProperties(draft_.layers.back()).id;
    return commit(std::move(before), std::move(previous_selection));
}
} // namespace congregation::native
