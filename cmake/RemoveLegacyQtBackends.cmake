if(NOT DEFINED APP_BUNDLE OR NOT IS_DIRECTORY "${APP_BUNDLE}/Contents/Frameworks")
    return()
endif()
# Incremental builds may retain files copied by the former Qt SDK closure.
# Run before runtime deployment/signing; current dependencies are then resolved
# again from the newly linked executable by the single-app deployment step.
foreach(_module iiAccountManager iiLicenseManager iiUpdateManager qt6keychain)
    file(GLOB _obsolete "${APP_BUNDLE}/Contents/Frameworks/lib${_module}*.dylib")
    if(_obsolete)
        file(REMOVE ${_obsolete})
    endif()
endforeach()
