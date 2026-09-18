cmake_minimum_required(VERSION 3.24)

set(_congregation_required_build_dir "${CMAKE_SOURCE_DIR}/build")
set(_congregation_actual_build_dir "${CMAKE_BINARY_DIR}")
cmake_path(ABSOLUTE_PATH _congregation_required_build_dir NORMALIZE)
cmake_path(ABSOLUTE_PATH _congregation_actual_build_dir NORMALIZE)
if(NOT _congregation_actual_build_dir STREQUAL _congregation_required_build_dir)
    message(FATAL_ERROR
            "Congregation only supports the repository-local build/ directory. "
            "Reconfigure with: cmake -S ${CMAKE_SOURCE_DIR} -B ${_congregation_required_build_dir}")
endif()
unset(_congregation_actual_build_dir)
unset(_congregation_required_build_dir)

set(CONGREGATION_MIN_MACOS_VERSION "12.0")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${CONGREGATION_MIN_MACOS_VERSION}" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(Congregation VERSION 1.0.0 LANGUAGES C CXX)

set(BUNDLE_ID "com.iisacc.congregation.painter")
set(CONGREGATION_QML_MODULE_MAJOR "${PROJECT_VERSION_MAJOR}")
# QML import minor는 API revision 없이는 0으로 유지하는 편이 안전하다.
set(CONGREGATION_QML_MODULE_MINOR "0")
set(CONGREGATION_QML_MODULE_VERSION "${CONGREGATION_QML_MODULE_MAJOR}.${CONGREGATION_QML_MODULE_MINOR}")

include(CTest)

set(CONGREGATION_BUNDLE_VERSION "1")
if(NOT CONGREGATION_BUNDLE_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?$")
    message(FATAL_ERROR "CONGREGATION_BUNDLE_VERSION must be one to three dot-separated numeric components")
endif()
message(STATUS "Congregation versions: marketing=${PROJECT_VERSION}, bundle=${CONGREGATION_BUNDLE_VERSION}, qml=${CONGREGATION_QML_MODULE_VERSION}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 경로 힌트(선택)
if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}")
    list(PREPEND CMAKE_MODULE_PATH "${Qt6_DIR}")
endif()

# OpenGL 래퍼 탐색
find_package(WrapOpenGL QUIET MODULE)
if(NOT TARGET WrapOpenGL::WrapOpenGL)
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
    elseif(APPLE)
        find_library(_APPLE_OPENGL_FRAMEWORK OpenGL)
        if(_APPLE_OPENGL_FRAMEWORK)
            add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
            target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "${_APPLE_OPENGL_FRAMEWORK}")
        endif()
        unset(_APPLE_OPENGL_FRAMEWORK)
    endif()
endif()

# Craft 환경 우선 검색
if(DEFINED ENV{CRAFTROOT})
    set(_craft_prefix "$ENV{CRAFTROOT}")
    list(FIND CMAKE_PREFIX_PATH "${_craft_prefix}" _craft_index)
    if(_craft_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_craft_prefix}")
    endif()
    if(NOT DEFINED Qt6_DIR)
        set(_craft_qt_dir "$ENV{CRAFTROOT}/lib/cmake/Qt6")
        if(EXISTS "${_craft_qt_dir}/Qt6Config.cmake")
            set(Qt6_DIR "${_craft_qt_dir}" CACHE PATH "Qt6 config path (CraftRoot)" FORCE)
        endif()
        unset(_craft_qt_dir)
    endif()
    unset(_craft_index)
    unset(_craft_prefix)
endif()

# 로컬 LVRS 설치 경로 힌트(.local/SDK) 우선 반영
set(_congregation_lvrs_prefix "$ENV{HOME}/.local/SDK/LVRS")
if(EXISTS "${_congregation_lvrs_prefix}/LVRSConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_congregation_lvrs_prefix}" _congregation_lvrs_index)
    if(_congregation_lvrs_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_congregation_lvrs_prefix}")
    endif()
    unset(_congregation_lvrs_index)
endif()
unset(_congregation_lvrs_prefix)

# 로컬 iiPaintEngine 설치 경로 힌트(.local/SDK) 우선 반영
set(_congregation_iipaintengine_prefix "$ENV{HOME}/.local/SDK/iiPaintEngine")
if(EXISTS "${_congregation_iipaintengine_prefix}/lib/cmake/iiPaintEngine/iiPaintEngineConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_congregation_iipaintengine_prefix}" _congregation_iipaintengine_index)
    if(_congregation_iipaintengine_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_congregation_iipaintengine_prefix}")
    endif()
    unset(_congregation_iipaintengine_index)
endif()
unset(_congregation_iipaintengine_prefix)

# 로컬 iiSharedCanvas 설치 경로 힌트(.local/SDK) 우선 반영
set(_congregation_iisharedcanvas_prefix "$ENV{HOME}/.local/SDK/iiSharedCanvas")
if(EXISTS "${_congregation_iisharedcanvas_prefix}/lib/cmake/iiSharedCanvas/iiSharedCanvasConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_congregation_iisharedcanvas_prefix}" _congregation_iisharedcanvas_index)
    if(_congregation_iisharedcanvas_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_congregation_iisharedcanvas_prefix}")
    endif()
    unset(_congregation_iisharedcanvas_index)
endif()
unset(_congregation_iisharedcanvas_prefix)

# 로컬 iiUpdateManager 설치 경로 힌트(.local/SDK) 우선 반영
set(_congregation_iiupdatemanager_prefix "$ENV{HOME}/.local/SDK/iiUpdateManager")
if(EXISTS "${_congregation_iiupdatemanager_prefix}/lib/cmake/iiUpdateManager/iiUpdateManagerConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_congregation_iiupdatemanager_prefix}" _congregation_iiupdatemanager_index)
    if(_congregation_iiupdatemanager_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_congregation_iiupdatemanager_prefix}")
    endif()
    unset(_congregation_iiupdatemanager_index)
endif()
unset(_congregation_iiupdatemanager_prefix)

# 로컬 iiLicenseManager 설치 경로 힌트(.local/SDK) 우선 반영
set(_congregation_iilicensemanager_prefix "$ENV{HOME}/.local/SDK/iiLicenseManager")
if(EXISTS "${_congregation_iilicensemanager_prefix}/lib/cmake/iiLicenseManager/iiLicenseManagerConfig.cmake")
    list(FIND CMAKE_PREFIX_PATH "${_congregation_iilicensemanager_prefix}" _congregation_iilicensemanager_index)
    if(_congregation_iilicensemanager_index EQUAL -1)
        list(PREPEND CMAKE_PREFIX_PATH "${_congregation_iilicensemanager_prefix}")
    endif()
    unset(_congregation_iilicensemanager_index)
endif()
unset(_congregation_iilicensemanager_prefix)

# 요구 버전
set(QT_MIN_VERSION "6.8.0")

# 기본 설치 경로 및 요약
include(GNUInstallDirs)
include(FeatureSummary)
include(FetchContent)

# Qt6
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network Qml Quick QuickControls2 Svg)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()
find_package(LVRS CONFIG REQUIRED)
find_package(iiPaintEngine CONFIG REQUIRED)
find_package(iiFileProvider 0.5 CONFIG REQUIRED)
find_package(iiSharedCanvas 0.10.1 EXACT CONFIG REQUIRED)
find_package(iiUpdateManager 0.2 CONFIG REQUIRED)
find_package(iiLicenseManager 0.2 CONFIG REQUIRED)

get_filename_component(_congregation_iilicensemanager_prefix
        "${iiLicenseManager_DIR}/../../.." ABSOLUTE)
set(CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES
        "${_congregation_iilicensemanager_prefix}/share/iiLicenseManager/THIRD_PARTY_NOTICES.md")
if(NOT EXISTS "${CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES}")
    message(FATAL_ERROR
            "iiLicenseManager third-party notices are missing: ${CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES}")
endif()
unset(_congregation_iilicensemanager_prefix)

if(APPLE OR WIN32)
    FetchContent_Declare(qtkeychain
            GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
            GIT_TAG 875f77d9f61bd97fd84cca47ce3bc71186dfbd09
            GIT_SHALLOW FALSE
            UPDATE_DISCONNECTED TRUE
    )
    function(congregation_add_qtkeychain)
        set(BUILD_WITH_QT5 OFF)
        set(BUILD_TEST_APPLICATION OFF)
        set(BUILD_QTQUICK_DEMO OFF)
        set(BUILD_TRANSLATIONS OFF)
        set(BUILD_SHARED_LIBS OFF)
        set(BUILD_TESTING OFF)
        set(USE_CREDENTIAL_STORE ON)
        set(USE_COMPAT_NAMING_SCHEME OFF)
        FetchContent_MakeAvailable(qtkeychain)
    endfunction()
    congregation_add_qtkeychain()
endif()

FetchContent_Declare(psd_sdk
        GIT_REPOSITORY https://github.com/MolecularMatters/psd_sdk.git
        GIT_TAG f51449543273cbf12058ae92b230e0c4209f5066
        UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(psd_sdk)
if(NOT psd_sdk_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(psd_sdk)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()
file(GLOB _congregation_psd_sdk_sources CONFIGURE_DEPENDS
        "${psd_sdk_SOURCE_DIR}/src/Psd/*.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/*.c"
)
list(REMOVE_ITEM _congregation_psd_sdk_sources
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdNativeFile.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdNativeFile_Linux.cpp"
        "${psd_sdk_SOURCE_DIR}/src/Psd/PsdStringUtil.cpp"
)
set_source_files_properties("${psd_sdk_SOURCE_DIR}/src/Psd/Psdminiz.c" PROPERTIES LANGUAGE CXX)
add_library(congregation_psd_sdk STATIC ${_congregation_psd_sdk_sources})
target_include_directories(congregation_psd_sdk PUBLIC "${psd_sdk_SOURCE_DIR}/src/Psd")
target_compile_features(congregation_psd_sdk PUBLIC cxx_std_11)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(congregation_psd_sdk PRIVATE -Wno-pragmas)
endif()
lvrs_apply_platform_build_optimizations(congregation_psd_sdk)

set(_congregation_local_dependency_runtime_paths)
set(_congregation_local_dependency_runtime_candidates)
if(APPLE)
    list(APPEND _congregation_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/SDK/LVRS/platforms/macos/lib"
            "$ENV{HOME}/.local/SDK/iiPaintEngine/platforms/macos/lib"
            "$ENV{HOME}/.local/SDK/iiSharedCanvas/lib"
            "$ENV{HOME}/.local/SDK/iiUpdateManager/lib"
            "$ENV{HOME}/.local/SDK/iiLicenseManager/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _congregation_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/SDK/LVRS/platforms/linux/lib"
            "$ENV{HOME}/.local/SDK/iiPaintEngine/platforms/linux/lib"
            "$ENV{HOME}/.local/SDK/iiSharedCanvas/lib"
            "$ENV{HOME}/.local/SDK/iiUpdateManager/lib"
            "$ENV{HOME}/.local/SDK/iiLicenseManager/lib")
endif()
if(UNIX AND NOT APPLE)
    list(APPEND _congregation_local_dependency_runtime_candidates
            "$ENV{HOME}/.local/SDK/LVRS/lib"
            "$ENV{HOME}/.local/SDK/iiPaintEngine/lib"
            "$ENV{HOME}/.local/SDK/iiSharedCanvas/lib"
            "$ENV{HOME}/.local/SDK/iiUpdateManager/lib"
            "$ENV{HOME}/.local/SDK/iiLicenseManager/lib")
endif()
foreach(_congregation_runtime_path IN LISTS _congregation_local_dependency_runtime_candidates)
    if(IS_DIRECTORY "${_congregation_runtime_path}")
        list(APPEND _congregation_local_dependency_runtime_paths "${_congregation_runtime_path}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _congregation_local_dependency_runtime_paths)
unset(_congregation_local_dependency_runtime_candidates)
unset(_congregation_runtime_path)

function(congregation_configure_local_runtime_paths target_name)
    if(UNIX)
        # The configured packages must win over previously installed ABI-compatible SDKs.
        set_property(TARGET ${target_name} APPEND PROPERTY BUILD_RPATH
                "$<TARGET_FILE_DIR:LVRS::LVRS>"
                "$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>"
                "$<TARGET_FILE_DIR:iiSharedCanvas::iiSharedCanvas>"
                "$<TARGET_FILE_DIR:iiFileProvider::iiFileProvider>"
                "$<TARGET_FILE_DIR:iiUpdateManager::iiUpdateManager>"
                "$<TARGET_FILE_DIR:iiLicenseManager::iiLicenseManager>")
    endif()
    if(_congregation_local_dependency_runtime_paths)
        set_property(TARGET ${target_name} APPEND PROPERTY
                BUILD_RPATH "${_congregation_local_dependency_runtime_paths}")
    endif()
endfunction()

qt_standard_project_setup(REQUIRES ${QT_MIN_VERSION})

# 실행 타깃
qt_add_executable(Congregation
        MANUAL_FINALIZATION
        WIN32
        MACOSX_BUNDLE
)

# 소스 추가(하위 디렉터리에서 타깃에 파일을 attach)
add_subdirectory(src/App App)

# QML 모듈
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set(QT_COPY_QML_FILES_OLD_WAY ON)
endif()
# Preserve the existing QML resource URLs when source files move under src/.
file(GLOB_RECURSE _app_qml_sources CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src/App/*.qml")
foreach(_app_qml IN LISTS _app_qml_sources)
    string(REGEX REPLACE "^src/" "" _app_qml_alias "${_app_qml}")
    set_source_files_properties("${_app_qml}" PROPERTIES QT_RESOURCE_ALIAS "${_app_qml_alias}")
endforeach()

qt_add_qml_module(Congregation
        URI Congregation
        VERSION ${CONGREGATION_QML_MODULE_VERSION}
        QML_FILES
        src/App/qml/Main.qml
        src/App/qml/menus/WindowsApplicationMenuBar.qml
        src/App/qml/license/LicenseActivationPage.qml
        src/App/qml/preferences/PreferencesWindow.qml
        src/App/qml/canvas/PainterCanvasPage.qml
        src/App/qml/canvas/PresentationLaserPointer.qml
        src/App/qml/brush/CanvasToolBar.qml
        src/App/qml/brush/HslTriangleColorPicker.qml
        src/App/qml/painting/DrawingSurface.qml
)

# qmltyperegistrar 빈 메타타입 안전장치
get_target_property(_congregation_binary_dir Congregation BINARY_DIR)
get_property(_congregation_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_congregation_multi_config)
    foreach(_congregation_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${_congregation_config}" _congregation_config_lower)
        string(TOLOWER "Congregation_${_congregation_config}" _congregation_target_lower)
        set(_congregation_meta_dir "${_congregation_binary_dir}/${_congregation_config}/meta_types")
        set(_congregation_meta_file "${_congregation_meta_dir}/qt6${_congregation_target_lower}_metatypes.json")
        file(MAKE_DIRECTORY "${_congregation_meta_dir}")
        if(EXISTS "${_congregation_meta_file}")
            file(SIZE "${_congregation_meta_file}" _congregation_meta_size)
            if(_congregation_meta_size EQUAL 0)
                file(WRITE "${_congregation_meta_file}" "[]\n")
            endif()
        else()
            file(WRITE "${_congregation_meta_file}" "[]\n")
        endif()
    endforeach()
else()
    if(CMAKE_BUILD_TYPE)
        string(TOLOWER "Congregation_${CMAKE_BUILD_TYPE}" _congregation_target_lower)
    else()
        string(TOLOWER "Congregation" _congregation_target_lower)
    endif()
    set(_congregation_meta_dir "${_congregation_binary_dir}/meta_types")
    set(_congregation_meta_file "${_congregation_meta_dir}/qt6${_congregation_target_lower}_metatypes.json")
    file(MAKE_DIRECTORY "${_congregation_meta_dir}")
    if(EXISTS "${_congregation_meta_file}")
        file(SIZE "${_congregation_meta_file}" _congregation_meta_size)
        if(_congregation_meta_size EQUAL 0)
            file(WRITE "${_congregation_meta_file}" "[]\n")
        endif()
    else()
        file(WRITE "${_congregation_meta_file}" "[]\n")
    endif()
endif()
#unset(_congregation_meta_dir _congregation_meta_file _congregation_meta_size _congregation_binary_dir _congregation_multi_config _congregation_config _congregation_config_lower _congregation_target_lower)

# 링크
target_link_libraries(Congregation PRIVATE
        Qt6::Core Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
        iiPaintEngine::iiPaintEngine
        iiFileProvider::iiFileProvider
        iiSharedCanvas::iiSharedCanvas
        iiUpdateManager::iiUpdateManager
        iiLicenseManager::iiLicenseManager
        congregation_psd_sdk
)
if(APPLE OR WIN32)
    target_link_libraries(Congregation PRIVATE qt6keychain)
endif()
target_compile_definitions(Congregation PRIVATE
        CONGREGATION_VERSION="${PROJECT_VERSION}"
)
congregation_configure_local_runtime_paths(Congregation)

if(WIN32 AND MINGW)
    target_link_options(Congregation PRIVATE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:-Wl,--gc-sections>"
    )
endif()

set(_congregation_windows_runtime_files)
if(WIN32 AND MINGW)
    get_filename_component(_congregation_mingw_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_congregation_mingw_runtime_name IN ITEMS
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll)
        string(MAKE_C_IDENTIFIER "${_congregation_mingw_runtime_name}" _congregation_mingw_runtime_var)
        find_file("${_congregation_mingw_runtime_var}"
                NAMES "${_congregation_mingw_runtime_name}"
                PATHS "${_congregation_mingw_runtime_dir}"
                NO_DEFAULT_PATH
                NO_CACHE)
        if(NOT ${_congregation_mingw_runtime_var})
            message(FATAL_ERROR
                    "MinGW runtime ${_congregation_mingw_runtime_name} was not found next to ${CMAKE_CXX_COMPILER}")
        endif()
        list(APPEND _congregation_windows_runtime_files "${${_congregation_mingw_runtime_var}}")
        unset(_congregation_mingw_runtime_var)
    endforeach()
    unset(_congregation_mingw_runtime_dir)
endif()

if(WIN32)
    add_custom_command(TARGET Congregation POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_FILE:LVRS::LVRS>
                    $<TARGET_FILE:iiPaintEngine::iiPaintEngine>
                    $<TARGET_FILE:iiFileProvider::iiFileProvider>
                    $<TARGET_FILE:iiSharedCanvas::iiSharedCanvas>
                    $<TARGET_FILE:iiUpdateManager::iiUpdateManager>
                    $<TARGET_FILE:iiLicenseManager::iiLicenseManager>
                    ${_congregation_windows_runtime_files}
                    $<TARGET_FILE_DIR:Congregation>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Windows runtime DLLs next to Congregation.exe")
endif()
unset(_congregation_windows_runtime_files)

# LVRS QML 모듈 링크 및 import 경로/정적 플러그인 자동 처리
set_target_properties(Congregation PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
lvrs_configure_qml_app(Congregation NO_PLATFORM_RUNTIME_TARGETS)
if(WIN32 AND MINGW)
    # MinGW 13's LTO plugin is not propagated consistently through Ninja's
    # archive and generated-QML steps. Keep the native Release optimization
    # flags and section GC, but do not emit fragile LTO objects on this toolchain.
    set_target_properties(congregation_psd_sdk Congregation PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL FALSE
    )
endif()

set(CONGREGATION_MACOS_APP_ICON "${CMAKE_SOURCE_DIR}/resources/Appicon.icns")
set(CONGREGATION_WINDOWS_APP_ICON "${CMAKE_SOURCE_DIR}/resources/Appicon.ico")
set(CONGREGATION_WINDOWS_MANIFEST_TEMPLATE "${CMAKE_SOURCE_DIR}/resources/windows/Congregation.manifest.in")
set(CONGREGATION_WINDOWS_RESOURCE_TEMPLATE "${CMAKE_SOURCE_DIR}/resources/windows/Congregation.rc.in")
if(WIN32)
    string(REPLACE "." ";" _congregation_windows_version_components "${PROJECT_VERSION}")
    list(LENGTH _congregation_windows_version_components _congregation_windows_version_component_count)
    while(_congregation_windows_version_component_count LESS 4)
        list(APPEND _congregation_windows_version_components "0")
        list(LENGTH _congregation_windows_version_components _congregation_windows_version_component_count)
    endwhile()
    list(JOIN _congregation_windows_version_components "." CONGREGATION_WINDOWS_VERSION_DOTTED)
    list(JOIN _congregation_windows_version_components "," CONGREGATION_WINDOWS_VERSION_COMMA)
    unset(_congregation_windows_version_component_count)
    unset(_congregation_windows_version_components)
endif()

if(APPLE)
    get_filename_component(CONGREGATION_MACOS_APP_ICON_FILE "${CONGREGATION_MACOS_APP_ICON}" NAME)
    set(CONGREGATION_LEGACY_MACOS_APP_ICON_FILE "icon.icns")
    set(CONGREGATION_QTKEYCHAIN_LICENSE
            "${CMAKE_SOURCE_DIR}/packaging/common/licenses/QtKeychain-BSD-3-Clause.txt")
    set_source_files_properties("${CONGREGATION_QTKEYCHAIN_LICENSE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/legal/QtKeychain")
    set_source_files_properties("${CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/legal/iiLicenseManager")
    target_sources(Congregation PRIVATE
            "${CONGREGATION_QTKEYCHAIN_LICENSE}"
            "${CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES}")
endif()

if(APPLE)
    if(NOT EXISTS "${CONGREGATION_MACOS_APP_ICON}")
        message(FATAL_ERROR "macOS app icon is missing: ${CONGREGATION_MACOS_APP_ICON}")
    endif()

    target_sources(Congregation PRIVATE "${CONGREGATION_MACOS_APP_ICON}")
    set_source_files_properties("${CONGREGATION_MACOS_APP_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
elseif(WIN32)
    if(NOT EXISTS "${CONGREGATION_WINDOWS_APP_ICON}")
        message(FATAL_ERROR "Windows app icon is missing: ${CONGREGATION_WINDOWS_APP_ICON}")
    endif()
    if(NOT EXISTS "${CONGREGATION_WINDOWS_MANIFEST_TEMPLATE}")
        message(FATAL_ERROR "Windows application manifest template is missing: ${CONGREGATION_WINDOWS_MANIFEST_TEMPLATE}")
    endif()
    if(NOT EXISTS "${CONGREGATION_WINDOWS_RESOURCE_TEMPLATE}")
        message(FATAL_ERROR "Windows resource template is missing: ${CONGREGATION_WINDOWS_RESOURCE_TEMPLATE}")
    endif()

    set(_congregation_windows_manifest_file "${CMAKE_CURRENT_BINARY_DIR}/Congregation.manifest")
    configure_file("${CONGREGATION_WINDOWS_MANIFEST_TEMPLATE}"
            "${_congregation_windows_manifest_file}" @ONLY)
    file(TO_NATIVE_PATH "${_congregation_windows_manifest_file}" _congregation_windows_manifest_native)
    string(REPLACE "\\" "\\\\" CONGREGATION_WINDOWS_MANIFEST_RC
            "${_congregation_windows_manifest_native}")
    file(TO_NATIVE_PATH "${CONGREGATION_WINDOWS_APP_ICON}" _congregation_windows_app_icon_native)
    string(REPLACE "\\" "\\\\" CONGREGATION_WINDOWS_APP_ICON_RC
            "${_congregation_windows_app_icon_native}")

    set(_congregation_windows_resource_file "${CMAKE_CURRENT_BINARY_DIR}/Congregation.rc")
    configure_file("${CONGREGATION_WINDOWS_RESOURCE_TEMPLATE}"
            "${_congregation_windows_resource_file}" @ONLY)
    set_property(SOURCE "${_congregation_windows_resource_file}" APPEND PROPERTY
            OBJECT_DEPENDS "${_congregation_windows_manifest_file}")
    target_sources(Congregation PRIVATE "${_congregation_windows_resource_file}")
endif()

# 번들 식별자 및 Info.plist 연결
if(APPLE)
    set(MAC_INFO_PLIST_TEMPLATE "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist")
    set(MAC_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/CongregationInfo.plist")
    configure_file("${MAC_INFO_PLIST_TEMPLATE}" "${MAC_INFO_PLIST}" @ONLY)
    set_target_properties(Congregation PROPERTIES
            OUTPUT_NAME "Congregation"
            MACOSX_BUNDLE_INFO_PLIST "${MAC_INFO_PLIST}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_ID}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${CONGREGATION_BUNDLE_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    set_target_properties(Congregation PROPERTIES MACOSX_BUNDLE_ICON_FILE "${CONGREGATION_MACOS_APP_ICON_FILE}")
    add_custom_command(TARGET Congregation POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f
                    "$<TARGET_BUNDLE_DIR:Congregation>/Contents/Resources/${CONGREGATION_LEGACY_MACOS_APP_ICON_FILE}"
            COMMENT "Removing legacy macOS app icon from Congregation bundle")
endif()

# 설치 규칙
if(APPLE)
    install(TARGETS Congregation BUNDLE DESTINATION "Applications" COMPONENT Runtime)
elseif(WIN32)
    install(TARGETS Congregation RUNTIME DESTINATION "." COMPONENT Runtime)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET Congregation PROPERTY
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS Congregation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/com.iisacc.congregation.painter.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
            COMPONENT Runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/Congregation.xcassets/AppIcon.appiconset/icon_256x256.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
            RENAME "com.iisacc.congregation.painter.png"
            COMPONENT Runtime)
    install(FILES "${CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES}"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/Congregation/legal/iiLicenseManager"
            COMPONENT Runtime)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# CPack install 단계에서 Qt 런타임을 함께 배치해 패키지를 독립 실행 가능 상태로 만든다.
if(APPLE)
    install(CODE
            "include(\"${QT_DEPLOY_SUPPORT}\")\nqt6_deploy_runtime_dependencies(\n    EXECUTABLE \"Applications/Congregation.app\"\n    DEPLOY_TOOL_OPTIONS \"-qmldir=${CMAKE_SOURCE_DIR}/src/App/qml\"\n)\n"
            COMPONENT Runtime)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_generate_deploy_qml_app_script(
            TARGET Congregation
            OUTPUT_SCRIPT _congregation_linux_deploy_script
    )
    install(SCRIPT "${_congregation_linux_deploy_script}" COMPONENT Runtime)
endif()

# CPack 구성
set(CPACK_PACKAGE_NAME "Congregation")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
if(APPLE)
    # productbuild로 단일 컴포넌트 .pkg 생성
    set(CPACK_GENERATOR "productbuild")
    set(CPACK_PACKAGE_FILE_NAME "Congregation-${CPACK_PACKAGE_VERSION}-build${CONGREGATION_BUNDLE_VERSION}")

    # 설치 경로: .app을 /Applications로 설치
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")  # 루트 기준
    set(CPACK_PRODUCTBUILD_INSTALLER_DIRECTORY "/") # 내부적으로 사용
    set(CPACK_PRODUCTBUILD_RESOURCES "${CMAKE_SOURCE_DIR}/packaging/macos/resources")

    # 식별자
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.iisacc.congregation.painter")

    # 서명 신원: 기본은 무서명으로 두고, 필요한 경우에만 명시한다.
    set(CONGREGATION_CPACK_SIGNING_IDENTITY "" CACHE STRING "Installer signing identity for CPack productbuild/pkgbuild")
    if(CONGREGATION_CPACK_SIGNING_IDENTITY)
        set(CPACK_PKGBUILD_IDENTITY_NAME "${CONGREGATION_CPACK_SIGNING_IDENTITY}")
        set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${CONGREGATION_CPACK_SIGNING_IDENTITY}")
    endif()
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
    # CPack does not run windeployqt, PE closure checks, or Authenticode signing.
    # Keep its output unmistakably separate from build-windows.ps1 release artifacts.
    set(CPACK_PACKAGE_FILE_NAME "Congregation-${CPACK_PACKAGE_VERSION}-Windows-unsigned-cpack-incomplete")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "Congregation-${CPACK_PACKAGE_VERSION}-Linux")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "Congregation-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif()

# 단일 컴포넌트로 .app을 패키지화
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Congregation")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Congregation ${PROJECT_VERSION} Application")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)
set(CPACK_COMPONENT_RUNTIME_DEPENDS "")

if(APPLE)
    set(CPACK_COMPONENT_RUNTIME_PACKAGE_TYPE "component")
    set(CPACK_COMPONENT_RUNTIME_INSTALL_PATH "/Applications")
    set(CPACK_BUNDLE_NAME "Congregation")
    if(CONGREGATION_CPACK_SIGNING_IDENTITY)
        set(CPACK_BUNDLE_APPLE_CERT_APP "${CONGREGATION_CPACK_SIGNING_IDENTITY}")
    endif()
endif()

include(CPack)

# Qt의 수동 마무리 훅
qt_finalize_executable(Congregation)

# 패키징·서명과 무관한 기본 요약
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
