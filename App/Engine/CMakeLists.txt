add_library(CongregationEngine STATIC
        ApplicationEngine.cpp ApplicationEngine.h
        CanvasDocument.cpp CanvasDocument.h
        Palette.cpp Palette.h
        Data.h RecentCanvas.cpp RecentCanvas.h Preferences.cpp Preferences.h NearbyDiscovery.cpp NearbyDiscovery.h EditCommand.cpp EditCommand.h CborWire.cpp CanvasSession.cpp CanvasSession.h LicenseService.cpp LicenseService.h DocumentMetadata.cpp DocumentMetadata.h)
add_library(Congregation::Engine ALIAS CongregationEngine)
target_compile_features(CongregationEngine PUBLIC cxx_std_23)
target_include_directories(CongregationEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(CongregationEngine PUBLIC iiSDKCore::Runtime iiJson::iiJson iiNetwork::iiNetwork iiAccountManager::Native iiUpdateManager::Native iiFileProvider::Native iiFileProvider::CoreIO iiFileProvider::Compatibility LibXml2::LibXml2 iiSharedCanvas::Native)
if(APPLE)
    target_link_libraries(CongregationEngine PRIVATE "-framework CoreFoundation")
endif()
set_target_properties(CongregationEngine PROPERTIES
        AUTOMOC OFF AUTORCC OFF AUTOUIC OFF POSITION_INDEPENDENT_CODE ON)
