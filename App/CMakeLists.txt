add_library(CongregationEngine STATIC
        Application/ApplicationEngine.cpp Application/ApplicationEngine.h
        Document/CanvasDocument.cpp Document/CanvasDocument.h
        Color/Color.h Color/Palette.cpp Color/Palette.h Account/AsyncLicenseService.cpp Account/AsyncLicenseService.h Agent/WorkspaceAgentClient.cpp Agent/WorkspaceAgentClient.h Agent/SocietyAgentCommand.cpp
        Shared/Data.h
        Document/RecentCanvas.cpp Document/RecentCanvas.h
        Preferences/Preferences.cpp Preferences/Preferences.h
        Collaboration/NearbyDiscovery.cpp Collaboration/NearbyDiscovery.h
        Collaboration/CanvasSession.cpp Collaboration/CanvasSession.h
        Collaboration/CanvasCollaboration.cpp Collaboration/CanvasCollaboration.h
        Collaboration/CborWire.cpp Collaboration/LocalChangeQueue.h
        Document/EditCommand.cpp Document/EditCommand.h
        Account/LicenseService.cpp Account/LicenseService.h
        Account/WebLicenseService.cpp Account/WebLicenseService.h
        Update/SocietyUpdateFeed.cpp Update/SocietyUpdateFeed.h
        Document/DocumentMetadata.cpp Document/DocumentMetadata.h)
add_library(Congregation::Engine ALIAS CongregationEngine)
target_compile_features(CongregationEngine PUBLIC cxx_std_23)
target_include_directories(CongregationEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(_congregation_update_prefix "${iiUpdateManagerNative_DIR}/../../.." ABSOLUTE)
get_filename_component(_congregation_account_prefix "${iiAccountManagerNative_DIR}/../../.." ABSOLUTE)
target_include_directories(CongregationEngine BEFORE PUBLIC
        "${_congregation_update_prefix}/include" "${_congregation_account_prefix}/include")
target_link_libraries(CongregationEngine PUBLIC iiSDKCore::Runtime iiJson::iiJson iiNetwork::iiNetwork iiAccountManager::Native iiUpdateManager::Native iiFileProvider::Native iiFileProvider::CoreIO iiFileProvider::Compatibility LibXml2::LibXml2 iiSharedCanvas::Native)
if(APPLE)
    target_link_libraries(CongregationEngine PRIVATE "-framework CoreFoundation")
endif()
set_target_properties(CongregationEngine PROPERTIES
        AUTOMOC OFF AUTORCC OFF AUTOUIC OFF POSITION_INDEPENDENT_CODE ON)
