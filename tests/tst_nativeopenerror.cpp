#include <Native/Artwork.h>
#include <Engine/RecentCanvas.h>
#include <iiFileProviderNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
void checkNativeFailure(congregation::native::Artwork& artwork,const std::filesystem::path& path) {
    iiSharedCanvas::Document output;
    const auto expected=iiSharedCanvas::DocumentFile::readSnapshot(congregation::native::path_text(path),output);
    check(!expected.ok() && !expected.message.empty(),"invalid native fixture did not fail clearly");
    const auto before=artwork.snapshot();
    const auto revision=artwork.revision();
    const auto selection=artwork.selected();
    const bool accepted=artwork.open(path);
    if (accepted || artwork.error()!=expected.message) {
        std::cerr<<"OPEN_NATIVE_ERROR_MISMATCH path="<<path.filename().string()
                 <<" expected="<<expected.message<<" actual="<<artwork.error()<<'\n';
        throw std::runtime_error("native document failure was replaced by an unrelated error");
    }
    check(artwork.snapshot()==before && artwork.revision()==revision && artwork.selected()==selection,
          "failed native open replaced the current document");
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"resource test directory missing");
        iiFileProvider::native::TemporaryDirectory temporary{std::filesystem::canonical(argv[1])};
        check(temporary.isValid(),"cannot create private test resource directory");
        std::cout<<"test-resource-dir="<<temporary.path().string()<<'\n'<<std::flush;
        congregation::native::Artwork artwork(temporary.path());
        check(artwork.create(32,32),"cannot prepare current artwork");
        const auto badIisc=std::string("IISC\r\n\x1a\n",8)+std::string(64,'\0');
        for (const auto* name : {"damaged.iisc","damaged.dat"}) {
            const auto path=temporary.path()/name;
            iiFileProvider::native::File::create(path,badIisc);
            checkNativeFailure(artwork,path);
            check(iiFileProvider::native::File::read(path)==badIisc,"failed native open changed the input");
        }
        const auto huge=temporary.path()/"damaged-large.iisc";
        const auto invalidSqlite=std::string("SQLite format 3\0",16)+std::string(128,'\0');
        iiFileProvider::native::File::create(huge,invalidSqlite);
        std::filesystem::resize_file(huge,congregation::maximum_canvas_bytes+1);
        checkNativeFailure(artwork,huge);
        auto prefix=iiFileProvider::native::File::openRead(huge);
        check(std::filesystem::file_size(huge)==congregation::maximum_canvas_bytes+1 &&
              prefix->read(static_cast<std::int64_t>(invalidSqlite.size()))==invalidSqlite,
              "failed native open changed the sparse input");
        std::cout<<"NATIVE_OPEN_ERROR_OK\n";
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
