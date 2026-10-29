#!/usr/bin/env python3
"""Build the Qt-free iPadOS SDK graph into an isolated Workspace prefix."""
import argparse, fcntl, hashlib, json, os, pathlib, shutil, subprocess, tarfile, urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[3]
SDK=ROOT/'SDK'
WORK=SDK/'build/congregation-ipados'
PREFIX=WORK/'prefix'
VENDOR=WORK/'vendor'
ENV={k:v for k,v in os.environ.items() if not k.startswith('DYLD_') and k not in ('CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','CMAKE_PREFIX_PATH','CMAKE_INCLUDE_PATH','CMAKE_LIBRARY_PATH')}
SYSROOT=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
COMMON=['-G','Ninja','-DCMAKE_SYSTEM_NAME=iOS','-DCMAKE_SYSTEM_VERSION=18.0','-DCMAKE_SYSTEM_PROCESSOR=arm64','-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON','-DCMAKE_OSX_SYSROOT=iphoneos','-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MACOSX_BUNDLE=OFF','-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY','-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF','-DCMAKE_INSTALL_PREFIX='+str(PREFIX),'-DCMAKE_PREFIX_PATH='+str(PREFIX),'-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH','-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER','-DCMAKE_FIND_FRAMEWORK=LAST','-DZLIB_LIBRARY='+SYSROOT+'/usr/lib/libz.tbd','-DZLIB_INCLUDE_DIR='+SYSROOT+'/usr/include','-DLIBXML2_LIBRARY='+SYSROOT+'/usr/lib/libxml2.tbd','-DLIBXML2_INCLUDE_DIR='+SYSROOT+'/usr/include/libxml2','-DVulkan_LIBRARY='+str(PREFIX/'lib/libMoltenVK.a'),'-DVulkan_INCLUDE_DIR='+str(PREFIX/'include'),'-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON','-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON']
def run(args,log,cwd=None,env=None):
 log.parent.mkdir(parents=True,exist_ok=True)
 print('BUILD',log,flush=True)
 with log.open('w') as f:r=subprocess.run([str(a) for a in args],cwd=cwd,env=env or ENV,stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:print(log.read_text(errors='replace')[-6000:],flush=True);raise SystemExit(r.returncode)
def cmake(source,build,options=(),targets=()):
 build.mkdir(parents=True,exist_ok=True)
 native_dependencies=['-DCMAKE_FIND_ROOT_PATH='+str(PREFIX),
  '-DOPENSSL_INCLUDE_DIR='+str(PREFIX/'include'),
  '-DOPENSSL_CRYPTO_LIBRARY='+str(PREFIX/'lib/libcrypto.a'),
  '-DOPENSSL_SSL_LIBRARY='+str(PREFIX/'lib/libssl.a')]
 run(['cmake','--fresh','-S',source,'-B',build,*COMMON,*native_dependencies,*options],build.parent/'configure.log')
 run(['cmake','--build',build,'--parallel','4',*(['--target',*targets] if targets else [])],build.parent/'build.log')
 run(['cmake','--install',build],build.parent/'install.log')
def archive(name,url):
 VENDOR.mkdir(parents=True,exist_ok=True)
 path=VENDOR/name
 if not path.exists():
  print('DOWNLOAD',url,flush=True)
  request=urllib.request.Request(url,headers={'User-Agent':'Congregation-iPadOS-builder'})
  with urllib.request.urlopen(request,timeout=120) as r,path.with_suffix(path.suffix+'.partial').open('wb') as f:shutil.copyfileobj(r,f)
  path.with_suffix(path.suffix+'.partial').replace(path)
 manifest=VENDOR/'sources.json'
 digest=hashlib.sha256(path.read_bytes()).hexdigest()
 with (VENDOR/'sources.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  d=json.loads(manifest.read_text()) if manifest.exists() else {}
  if name in d and d[name]['sha256']!=digest:raise SystemExit('Vendor archive changed: '+name)
  d[name]={'url':url,'sha256':digest};manifest.write_text(json.dumps(d,indent=2)+'\n')
 return path
def unpack(path,name):
 dest=VENDOR/'sources'/name
 if not dest.exists():
  dest.mkdir(parents=True)
  with tarfile.open(path) as t:
   members=t.getmembers();tops={m.name.split('/')[0] for m in members if m.name.strip('./')}
   strip=len(tops)==1
   for m in members:
    n=m.name.split('/',1)[1] if strip and '/' in m.name else '' if strip else m.name
    if not n:continue
    if pathlib.PurePosixPath(n).is_absolute() or '..' in pathlib.PurePosixPath(n).parts:raise SystemExit('Unsafe vendor archive')
    m.name=n;t.extract(m,dest)
 return dest

def prepare_moltenvk():
 a=archive('MoltenVK-ios-1.4.2.tar','https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.2/MoltenVK-ios.tar')
 src=unpack(a,'MoltenVK-1.4.2')
 lib=next(src.glob('**/ios-arm64/libMoltenVK.a'))
 headers=next(p for p in src.glob('**/vulkan/vulkan.h')).parent.parent
 (PREFIX/'lib').mkdir(parents=True,exist_ok=True);shutil.copy2(lib,PREFIX/'lib/libMoltenVK.a')
 shutil.copytree(headers,PREFIX/'include',dirs_exist_ok=True)
 return ['-DVulkan_LIBRARY='+str(PREFIX/'lib/libMoltenVK.a'),'-DVulkan_INCLUDE_DIR='+str(PREFIX/'include')]

def write_sodium_package():
 package=PREFIX/'lib/cmake/Sodium'
 package.mkdir(parents=True,exist_ok=True)
 (package/'SodiumConfig.cmake').write_text('get_filename_component(_sodium_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)\nif(NOT TARGET Sodium::Sodium)\n add_library(Sodium::Sodium STATIC IMPORTED)\n set_target_properties(Sodium::Sodium PROPERTIES IMPORTED_LOCATION "${_sodium_prefix}/lib/libsodium.a" INTERFACE_INCLUDE_DIRECTORIES "${_sodium_prefix}/include")\nendif()\nif(NOT TARGET iiNativeSodium)\n add_library(iiNativeSodium ALIAS Sodium::Sodium)\nendif()\nunset(_sodium_prefix)\n')

def crypto():
 src=unpack(archive('openssl-3.6.5.tar.gz','https://github.com/openssl/openssl/releases/download/openssl-3.6.5/openssl-3.6.5.tar.gz'),'openssl-3.6.5')
 if not (PREFIX/'lib/libssl.a').exists():
  build=VENDOR/'openssl/build';build.mkdir(parents=True,exist_ok=True)
  run(['perl',src/'Configure','ios64-xcrun','no-shared','no-tests','no-apps','--prefix='+str(PREFIX),'--libdir=lib','-miphoneos-version-min=18.0'],build.parent/'configure.log',cwd=build)
  run(['make','-j4'],build.parent/'build.log',cwd=build)
  run(['make','install_sw'],build.parent/'install.log',cwd=build)
 sodium_archive=SDK/'iiLicenseManager/build/dependencies/libsodium-1.0.22.tar.gz'
 if not (VENDOR/'libsodium-1.0.22.tar.gz').exists() and sodium_archive.exists():shutil.copy2(sodium_archive,VENDOR/'libsodium-1.0.22.tar.gz')
 sodium=unpack(archive('libsodium-1.0.22.tar.gz','https://download.libsodium.org/libsodium/releases/libsodium-1.0.22.tar.gz'),'libsodium-1.0.22')
 if not (PREFIX/'lib/libsodium.a').exists():
  build=VENDOR/'sodium/build';build.mkdir(parents=True,exist_ok=True)
  env=dict(ENV,CC='xcrun --sdk iphoneos clang',CFLAGS='-arch arm64 -isysroot '+SYSROOT+' -miphoneos-version-min=18.0 -O2',LDFLAGS='-arch arm64 -isysroot '+SYSROOT+' -miphoneos-version-min=18.0')
  run([sodium/'configure','--host=aarch64-apple-darwin','--disable-shared','--enable-static','--disable-asm','--prefix='+str(PREFIX)],build.parent/'configure.log',cwd=build,env=env)
  run(['make','-j4'],build.parent/'build.log',cwd=build,env=env)
  run(['make','install'],build.parent/'install.log',cwd=build,env=env)
 write_sodium_package()
 curl=unpack(archive('curl-8.23.0.tar.gz','https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.gz'),'curl-8.23.0')
 cmake(curl,VENDOR/'curl/build',['-DBUILD_CURL_EXE=OFF','-DBUILD_EXAMPLES=OFF','-DBUILD_STATIC_LIBS=ON','-DHTTP_ONLY=ON','-DCURL_USE_OPENSSL=ON','-DUSE_APPLE_SECTRUST=ON','-DOPENSSL_ROOT_DIR='+str(PREFIX),'-DOPENSSL_USE_STATIC_LIBS=ON','-DCURL_USE_SECTRANSP=OFF','-DCURL_USE_LIBPSL=OFF','-DCURL_USE_LIBSSH2=OFF','-DUSE_LIBIDN2=OFF','-DUSE_NGHTTP2=OFF','-DCURL_BROTLI=OFF','-DCURL_ZSTD=OFF','-DCURL_USE_GSSAPI=OFF'])
def codecs():
 png=unpack(archive('libpng-1.6.59.tar.gz','https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.59.tar.gz'),'libpng-1.6.59')
 cmake(png,VENDOR/'png/build',['-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF','-DPNG_TOOLS=OFF'])
 jpeg=unpack(archive('libjpeg-turbo-3.2.0.tar.gz','https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz'),'libjpeg-turbo-3.2.0')
 cmake(jpeg,VENDOR/'jpeg/build',['-DENABLE_SHARED=OFF','-DENABLE_STATIC=ON','-DWITH_TURBOJPEG=OFF','-DWITH_TOOLS=OFF','-DWITH_TESTS=OFF'])
 webp=unpack(archive('libwebp-1.6.0.tar.gz','https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz'),'libwebp-1.6.0')
 cmake(webp,VENDOR/'webp/build',['-DWEBP_BUILD_ANIM_UTILS=OFF','-DWEBP_BUILD_CWEBP=OFF','-DWEBP_BUILD_DWEBP=OFF','-DWEBP_BUILD_GIF2WEBP=OFF','-DWEBP_BUILD_IMG2WEBP=OFF','-DWEBP_BUILD_VWEBP=OFF','-DWEBP_BUILD_EXTRAS=OFF'])
 tiff=unpack(archive('tiff-4.7.1.tar.gz','https://download.osgeo.org/libtiff/tiff-4.7.1.tar.gz'),'tiff-4.7.1')
 cmake(tiff,VENDOR/'tiff/build',['-Dtiff-tools=OFF','-Dtiff-tests=OFF','-Dtiff-contrib=OFF','-Dtiff-docs=OFF','-Djbig=OFF','-Dlzma=OFF','-Dzstd=OFF','-Dlibdeflate=OFF'])
 repair_tiff_config()

def repair_tiff_config():
 # Upstream 4.7.1 exports CMath::CMath without finding that dependency.
 # Keep this consumer adapter inside the isolated kit, never the upstream tree.
 config=PREFIX/'lib/cmake/tiff/TiffConfig.cmake'
 contents=config.read_text()
 prelude='if(NOT TARGET CMath::CMath)\n  add_library(CMath::CMath INTERFACE IMPORTED)\n  set_property(TARGET CMath::CMath PROPERTY INTERFACE_LINK_LIBRARIES m)\nendif()\n'
 if not contents.startswith(prelude):config.write_text(prelude+contents)

def main():
 p=argparse.ArgumentParser();p.add_argument('stage',choices=['base','vendors','crypto','codecs','files','network','canvas','gui','all']);a=p.parse_args();PREFIX.mkdir(parents=True,exist_ok=True)
 stages=['base','vendors','crypto','codecs','files','network','canvas','gui'] if a.stage=='all' else [a.stage]
 for stage in stages:
  if stage=='base':
   cmake(SDK/'iiSDKCore',SDK/'iiSDKCore/build/ipados/build',['-DIISDKCORE_BUILD_EXAMPLES=OFF'])
   for m in ['iiJson','LVUI']:cmake(SDK/m,SDK/m/'build/ipados/build')
  elif stage=='vendors':
   prepare_moltenvk()
  elif stage=='crypto':
   crypto()
  elif stage=='codecs':
   codecs()
  elif stage=='files':
   cmake(SDK/'iiFileProvider',SDK/'iiFileProvider/build/ipados/build',['-DIIFILEPROVIDER_BUILD_QT_API=OFF','-DIIFILEPROVIDER_BUILD_CORE_IO=ON','-DIIFILEPROVIDER_BUILD_TOOLS=OFF'])
   cmake(SDK/'iiFileProvider/Compatibility',SDK/'iiFileProvider/build/ipados-compatibility/build')
  elif stage=='network':
   # iiNetwork reserves its top-level build/ for host development. Consume it
   # as a library subproject rather than replacing that host configuration.
   driver=WORK/'driver/iiNetwork';driver.mkdir(parents=True,exist_ok=True)
   (driver/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.31)\nproject(CongregationNetworkKit LANGUAGES CXX)\nadd_subdirectory("'+str(SDK/'iiNetwork')+'" network)\n')
   cmake(driver,SDK/'iiNetwork/build/ipados/build',['-DIINETWORK_BUILD_TESTS=OFF','-DIINETWORK_BUILD_EXAMPLES=OFF','-DOPENSSL_ROOT_DIR='+str(PREFIX),'-DOPENSSL_USE_STATIC_LIBS=ON','-DCURL_LIBRARY='+str(PREFIX/'lib/libcurl.a'),'-DCURL_INCLUDE_DIR='+str(PREFIX/'include')])
   for m,opt in [('iiAccountManager','IIACCOUNTMANAGER_BUILD_QT_API'),('iiUpdateManager','IIUPDATEMANAGER_BUILD_QT_API')]:cmake(SDK/m,SDK/m/'build/ipados/build',['-D'+opt+'=OFF','-DIILICENSEMANAGER_NATIVE_SODIUM_ROOT='+str(PREFIX)])
  elif stage=='canvas':
   cmake(SDK/'iiPaintEngine',SDK/'iiPaintEngine/build/ipados/build',['-DIIPAINTENGINE_BUILD_SHARED=OFF','-DIIPAINTENGINE_BUILD_EXAMPLES=OFF'])
   cmake(SDK/'iiImageCodec',SDK/'iiImageCodec/build/ipados/build')
   cmake(SDK/'iiSharedCanvas',SDK/'iiSharedCanvas/build/ipados/build',['-DIISHAREDCANVAS_BUILD_QT_API=OFF'])
  elif stage=='gui':
   cmake(SDK/'iiGUI',SDK/'iiGUI/build/ipados/build',prepare_moltenvk()+['-DIIGUI_BUILD_EXAMPLES=OFF','-DFETCHCONTENT_SOURCE_DIR_LUNASVG='+str(SDK/'iiGUI/build/dependencies/lunasvg/source')])
   cmake(SDK/'LVRS',SDK/'LVRS/build/ipados/build',['-DLVRS_BACKEND=native','-DLVRS_BUILD_EXAMPLES=OFF'])
 print('IPADOS_SDK_STAGE_OK',a.stage,flush=True)
if __name__=='__main__':main()
