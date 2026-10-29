#include <Native/Artwork.h>
#include <Engine/Data.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using congregation::native::Artwork;
using namespace iiSharedCanvas;
namespace {
void check(bool condition,const std::string& message) { if(!condition) throw std::runtime_error(message); }
enum class Reference { Static, Dynamic, Mask, Unshared };
const char* label(Reference kind) {
    switch(kind) {
    case Reference::Static:return "static";
    case Reference::Dynamic:return "dynamic";
    case Reference::Mask:return "mask";
    case Reference::Unshared:return "unshared";
    }
    return "invalid";
}
Document document(Reference kind) {
    Document d; d.extent={8,8};
    auto shared=makeRasterLayer(8,8,0xff224466U);
    shared.pixels[7]=0xffabcdefU;
    d.assets.emplace_back(RasterAsset{"shared",std::move(shared)});
    d.assets.emplace_back(RasterAsset{"other",makeRasterLayer(8,8,0xff668844U)});
    LayerProperties other{"other-layer","Other"};
    if(kind==Reference::Mask) other.appearance.rasterMask=LayerMask{.assetId="shared"};
    d.layers.emplace_back(StaticBitmapLayer{std::move(other),StaticSource{"other"}});
    if(kind==Reference::Static)
        d.layers.emplace_back(StaticBitmapLayer{{"consumer","Consumer"},StaticSource{"shared"}});
    if(kind==Reference::Dynamic) {
        d.layers.emplace_back(DynamicBitmapLayer{{"consumer","Consumer"},KeyframedSource{{0}}});
        d.frames.push_back(Frame{0,{{"consumer","shared"}}});
    }
    d.layers.emplace_back(StaticBitmapLayer{{"owner","Owner"},StaticSource{"shared"}});
    return d;
}
void verifyRetained(const Document& doc,Reference kind,bool ownerPresent) {
    check(bool(findLayer(doc,"owner"))==ownerPresent,"owner layer state is incorrect");
    check(bool(findRasterAsset(doc,"other")),"unrelated asset disappeared");
    const bool expected=ownerPresent || kind!=Reference::Unshared;
    const auto* shared=findRasterAsset(doc,"shared");
    check(bool(shared)==expected,"shared asset retention is incorrect");
    if(shared) check(shared->pixels.pixels.size()==64 && shared->pixels.pixels[7]==0xffabcdefU,
                     "shared asset pixels changed");
    if(kind==Reference::Dynamic) {
        const auto* consumer=findLayer(doc,"consumer");
        check(consumer && resolveAssetAt(doc,*consumer,0) && assetId(*resolveAssetAt(doc,*consumer,0))=="shared",
              "dynamic frame reference was not preserved");
    }
    if(kind==Reference::Mask) {
        const auto* other=findLayer(doc,"other-layer");
        check(other && layerProperties(*other).appearance.rasterMask &&
              layerProperties(*other).appearance.rasterMask->assetId=="shared",
              "mask reference was not preserved");
    }
}
void test(const std::filesystem::path& root,Reference kind) {
    Artwork artwork(root);
    check(artwork.replace(document(kind)),std::string(label(kind))+" fixture: "+artwork.error());
    check(artwork.selected()=="owner","fixture selection is wrong");
    const auto before=artwork.revision();
    if(!artwork.remove_layer())
        throw std::runtime_error(std::string("delete failed with ")+label(kind)+" reference: "+artwork.error());
    check(artwork.revision()==before+1 && artwork.can_undo(),"delete did not create one edit");
    verifyRetained(artwork.document(),kind,false);
    check(artwork.selected()!="owner","deleted layer stayed selected");
    check(artwork.undo() && artwork.selected()=="owner","undo did not restore selection");
    verifyRetained(artwork.document(),kind,true);
    check(artwork.redo() && artwork.selected()!="owner","redo did not restore selection");
    verifyRetained(artwork.document(),kind,false);
    const auto path=root/(std::string(label(kind))+".iisc");
    check(artwork.save(path),artwork.error());
    Artwork reopened(root);
    check(reopened.open(path),reopened.error());
    verifyRetained(reopened.document(),kind,false);
}
}
int main(int argc,char** argv) {
    std::filesystem::path root,fixture;
    try {
        check(argc==2,"test resource directory missing");
        root=std::filesystem::path(argv[1]);
        check(std::filesystem::is_directory(root) && !std::filesystem::is_symlink(root),
              "test resource directory is unavailable or linked");
        const auto id=ii::network::wire::uuid();
        check(!id.empty() && std::ranges::all_of(id,[](char c) {
            return (c>='0' && c<='9') || (c>='a' && c<='f') || c=='-';
        }),"invalid fixture identity");
        fixture=root/("shared-delete-"+id);
        check(std::filesystem::create_directory(fixture),"cannot create unique fixture");
        for(auto kind:{Reference::Static,Reference::Dynamic,Reference::Mask,Reference::Unshared}) test(fixture,kind);
        check(!std::filesystem::is_symlink(fixture),"fixture became a link");
        std::error_code error;
        std::filesystem::remove_all(fixture,error);
        check(!error,"cannot clean completed fixture");
        std::cout<<"SHARED_ASSET_DELETION_OK\n";
    } catch(const std::exception& error) {
        if(!fixture.empty()) { try { std::cerr<<"preserved_fixture="<<fixture.string()<<'\n'; } catch (...) {} }
        std::cerr<<error.what()<<'\n';return 1;
    }
}
