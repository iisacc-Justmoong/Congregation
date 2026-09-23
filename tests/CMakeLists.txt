add_executable(tests_canvasdocumentviewmodel
        tst_canvasdocumentviewmodel.cpp
        ../src/App/models/canvas/canvasdocumentviewmodel.cpp
        ../src/App/models/canvas/canvasdocumentviewmodel.h
        ../src/App/models/brush/paletteutils.cpp
        ../src/App/models/brush/paletteutils.h
)

target_include_directories(tests_canvasdocumentviewmodel PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App
        ${CMAKE_SOURCE_DIR}/src/App/models/brush
        ${CMAKE_SOURCE_DIR}/src/App/models/canvas
)

target_link_libraries(tests_canvasdocumentviewmodel PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_canvasdocumentviewmodel)

add_test(NAME tests_canvasdocumentviewmodel COMMAND tests_canvasdocumentviewmodel)

add_executable(tests_psdcompatibilitydocument
        tst_psdcompatibilitydocument.cpp
        ../src/App/models/document/psdcompatibilitydocument.cpp
        ../src/App/models/document/psdcompatibilitydocument.h
)

target_include_directories(tests_psdcompatibilitydocument PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/document
)

target_link_libraries(tests_psdcompatibilitydocument PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_psdcompatibilitydocument)

add_test(NAME tests_psdcompatibilitydocument COMMAND tests_psdcompatibilitydocument)

add_executable(tests_canvastoolbarqmlcontract
        tst_canvastoolbarqmlcontract.cpp
)

target_link_libraries(tests_canvastoolbarqmlcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_canvastoolbarqmlcontract)

add_test(NAME tests_canvastoolbarqmlcontract COMMAND tests_canvastoolbarqmlcontract)

add_executable(tests_mainqmlcontract
        tst_mainqmlcontract.cpp
)

target_link_libraries(tests_mainqmlcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_mainqmlcontract)

add_test(NAME tests_mainqmlcontract COMMAND tests_mainqmlcontract)

add_executable(tests_memberprofilelistbuilder
        tst_memberprofilelistbuilder.cpp
        ../src/App/models/collaboration/memberprofilelistbuilder.cpp
        ../src/App/models/collaboration/memberprofilelistbuilder.h
)

target_include_directories(tests_memberprofilelistbuilder PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/collaboration
)

target_link_libraries(tests_memberprofilelistbuilder PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_memberprofilelistbuilder)

add_test(NAME tests_memberprofilelistbuilder COMMAND tests_memberprofilelistbuilder)

add_executable(tests_profileimageprocessor
        tst_profileimageprocessor.cpp
        ../src/App/models/profile/profileimageprocessor.cpp
        ../src/App/models/profile/profileimageprocessor.h
)

target_include_directories(tests_profileimageprocessor PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/profile
)

target_link_libraries(tests_profileimageprocessor PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_profileimageprocessor)

add_test(NAME tests_profileimageprocessor COMMAND tests_profileimageprocessor)

add_executable(tests_applicationpreferences
        tst_applicationpreferences.cpp
        ../src/App/models/preferences/applicationpreferences.cpp
        ../src/App/models/preferences/applicationpreferences.h
)

target_include_directories(tests_applicationpreferences PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/preferences
)

target_link_libraries(tests_applicationpreferences PRIVATE
        Qt6::Core
        Qt6::Test
        iiFileProvider::iiFileProvider
)
congregation_configure_local_runtime_paths(tests_applicationpreferences)

add_test(NAME tests_applicationpreferences COMMAND tests_applicationpreferences)

add_executable(tests_nearbycongregationdiscovery
        tst_nearbycongregationdiscovery.cpp
        ../src/App/models/network/nearbycongregationdiscovery.cpp
        ../src/App/models/network/nearbycongregationdiscovery.h
)

target_include_directories(tests_nearbycongregationdiscovery PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/network
)

target_link_libraries(tests_nearbycongregationdiscovery PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_nearbycongregationdiscovery)

add_test(NAME tests_nearbycongregationdiscovery COMMAND tests_nearbycongregationdiscovery)

add_executable(tests_localcanvassession
        tst_localcanvassession.cpp
        ../src/App/models/document/recentcanvascontainer.cpp
        ../src/App/models/document/recentcanvascontainer.h
        ../src/App/models/network/localcanvassession.cpp
        ../src/App/models/network/localcanvassession.h
        ../src/App/models/network/nearbycongregationdiscovery.cpp
        ../src/App/models/network/nearbycongregationdiscovery.h
)

target_include_directories(tests_localcanvassession PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/document
        ${CMAKE_SOURCE_DIR}/src/App/models/network
)

target_link_libraries(tests_localcanvassession PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_localcanvassession)

add_test(NAME tests_localcanvassession COMMAND tests_localcanvassession)

add_executable(tests_temporarycamerainput
        tst_temporarycamerainput.cpp
        ../src/App/models/input/temporarycamerainput.cpp
        ../src/App/models/input/temporarycamerainput.h
)

target_include_directories(tests_temporarycamerainput PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/input
)

target_link_libraries(tests_temporarycamerainput PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_temporarycamerainput)

add_test(NAME tests_temporarycamerainput COMMAND tests_temporarycamerainput)

add_executable(tests_licensemanager
        tst_licensemanager.cpp
        ../src/App/models/license/accountmanager.cpp
        ../src/App/models/license/accountmanager.h
        ../src/App/models/license/licensecredentialstore.cpp
        ../src/App/models/license/licensecredentialstore.h
        ../src/App/models/license/licensemanager.cpp
        ../src/App/models/license/licensemanager.h
)

target_include_directories(tests_licensemanager PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/license
)

target_link_libraries(tests_licensemanager PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
        iiLicenseManager::iiLicenseManager
)
if(APPLE OR WIN32)
    target_link_libraries(tests_licensemanager PRIVATE qt6keychain)
endif()
congregation_configure_local_runtime_paths(tests_licensemanager)

add_test(NAME tests_licensemanager COMMAND tests_licensemanager)

add_executable(tests_congregationupdatemanager
        tst_congregationupdatemanager.cpp
        ../src/App/models/license/licensecredentialstore.cpp
        ../src/App/models/license/licensecredentialstore.h
        ../src/App/models/license/licensemanager.cpp
        ../src/App/models/license/licensemanager.h
        ../src/App/models/update/congregationupdatecredentialprovider.cpp
        ../src/App/models/update/congregationupdatecredentialprovider.h
        ../src/App/models/update/congregationupdatemanager.cpp
        ../src/App/models/update/congregationupdatemanager.h
)

target_include_directories(tests_congregationupdatemanager PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App/models/license
        ${CMAKE_SOURCE_DIR}/src/App/models/update
)

target_link_libraries(tests_congregationupdatemanager PRIVATE
        iiAcountManager::iiAcountManager
        Qt6::Core
        Qt6::Network
        Qt6::Test
        iiUpdateManager::iiUpdateManager
)
if(APPLE OR WIN32)
    target_link_libraries(tests_congregationupdatemanager PRIVATE qt6keychain)
endif()
congregation_configure_local_runtime_paths(tests_congregationupdatemanager)

add_test(NAME tests_congregationupdatemanager COMMAND tests_congregationupdatemanager)

add_executable(tests_macosbuildworkflowcontract
        tst_macosbuildworkflowcontract.cpp
)

target_link_libraries(tests_macosbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_macosbuildworkflowcontract)

add_test(NAME tests_macosbuildworkflowcontract COMMAND tests_macosbuildworkflowcontract)

add_executable(tests_linuxbuildworkflowcontract
        tst_linuxbuildworkflowcontract.cpp
)

target_link_libraries(tests_linuxbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_linuxbuildworkflowcontract)

add_test(NAME tests_linuxbuildworkflowcontract COMMAND tests_linuxbuildworkflowcontract)

add_executable(tests_windowsbuildworkflowcontract
        tst_windowsbuildworkflowcontract.cpp
)

target_link_libraries(tests_windowsbuildworkflowcontract PRIVATE
        Qt6::Core
        Qt6::Test
)
congregation_configure_local_runtime_paths(tests_windowsbuildworkflowcontract)

add_test(NAME tests_windowsbuildworkflowcontract COMMAND tests_windowsbuildworkflowcontract)

if(WIN32)
    add_executable(tests_windowsbinarycontract
            tst_windowsbinarycontract.cpp
    )

    target_compile_definitions(tests_windowsbinarycontract PRIVATE
            CONGREGATION_EXECUTABLE_PATH="$<TARGET_FILE:Congregation>"
    )
    target_link_libraries(tests_windowsbinarycontract PRIVATE
            Qt6::Core
            Qt6::Test
            version
    )
    add_dependencies(tests_windowsbinarycontract Congregation)
    congregation_configure_local_runtime_paths(tests_windowsbinarycontract)

    add_test(NAME tests_windowsbinarycontract COMMAND tests_windowsbinarycontract)
endif()

add_executable(tests_drawingsurfaceitem
        tst_drawingsurfaceitem.cpp
        ../src/App/models/brush/paletteutils.cpp
        ../src/App/models/brush/paletteutils.h
        ../src/App/models/canvas/canvasdocumentviewmodel.cpp
        ../src/App/models/canvas/canvasdocumentviewmodel.h
        ../src/App/models/canvas/canvasviewmodelbridge.cpp
        ../src/App/models/canvas/canvasviewmodelbridge.h
        ../src/App/models/document/psdcompatibilitydocument.cpp
        ../src/App/models/document/psdcompatibilitydocument.h
        ../src/App/models/document/psdimagereader.cpp
        ../src/App/models/document/psdimagereader.h
        ../src/App/models/document/psdimagewriter.cpp
        ../src/App/models/document/psdimagewriter.h
        ../src/App/models/document/recentcanvascontainer.cpp
        ../src/App/models/document/recentcanvascontainer.h
        ../src/App/models/painting/drawingsurfaceitem.cpp
        ../src/App/models/painting/drawingsurfaceitem.h
)

target_include_directories(tests_drawingsurfaceitem PRIVATE
        ${CMAKE_SOURCE_DIR}/src/App
        ${CMAKE_SOURCE_DIR}/src/App/models/brush
        ${CMAKE_SOURCE_DIR}/src/App/models/canvas
        ${CMAKE_SOURCE_DIR}/src/App/models/document
        ${CMAKE_SOURCE_DIR}/src/App/models/painting
)

target_link_libraries(tests_drawingsurfaceitem PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Network
        Qt6::Test
        iiPaintEngine::iiPaintEngine
        iiFileProvider::iiFileProvider
        iiSharedCanvas::iiSharedCanvas
        congregation_psd_sdk
)
congregation_configure_local_runtime_paths(tests_drawingsurfaceitem)

add_test(NAME tests_drawingsurfaceitem COMMAND tests_drawingsurfaceitem)

if(APPLE)
    add_test(NAME tests_filesdkruntime
            COMMAND ${CMAKE_COMMAND}
                    "-DTEST_EXECUTABLE=$<TARGET_FILE:tests_drawingsurfaceitem>"
                    "-DIIFILEPROVIDER_LIBRARY=$<TARGET_FILE:iiFileProvider::iiFileProvider>"
                    "-DIISHAREDCANVAS_LIBRARY=$<TARGET_FILE:iiSharedCanvas::iiSharedCanvas>"
                    "-DIIPAINTENGINE_LIBRARY=$<TARGET_FILE:iiPaintEngine::iiPaintEngine>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/FileSdkRuntimeTest.cmake")
endif()

set(_congregation_test_environment
        "DYLD_LIBRARY_PATH="
        "DYLD_FRAMEWORK_PATH="
        "DYLD_FALLBACK_LIBRARY_PATH="
        "CMAKE_PREFIX_PATH="
        "QML2_IMPORT_PATH="
        "QTDIR="
        "QT_PLUGIN_PATH="
        "QT_PREFIX="
        "QT_QML_IMPORT_PATH="
)

set_tests_properties(
        tests_canvasdocumentviewmodel
        tests_psdcompatibilitydocument
        tests_canvastoolbarqmlcontract
        tests_mainqmlcontract
        tests_memberprofilelistbuilder
        tests_profileimageprocessor
        tests_applicationpreferences
        tests_nearbycongregationdiscovery
        tests_temporarycamerainput
        tests_licensemanager
        tests_congregationupdatemanager
        tests_macosbuildworkflowcontract
        tests_linuxbuildworkflowcontract
        tests_windowsbuildworkflowcontract
        tests_drawingsurfaceitem
        PROPERTIES
        ENVIRONMENT "${_congregation_test_environment}"
)

if(WIN32)
    find_program(_congregation_powershell_executable NAMES powershell.exe REQUIRED)
    add_test(
            NAME tests_windowsauthenticodepolicy
            COMMAND "${_congregation_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsauthenticodepolicy.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
    )
    set_tests_properties(tests_windowsauthenticodepolicy PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )

    add_test(
            NAME tests_necessaryauthenticode
            COMMAND "${_congregation_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessaryauthenticode.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-authenticode.ps1"
    )
    set_tests_properties(tests_necessaryauthenticode PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )

    add_test(
            NAME tests_necessarywindowsrelease
            COMMAND "${_congregation_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessarywindowsrelease.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-windows-release.ps1"
    )
    set_tests_properties(tests_necessarywindowsrelease PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )

    find_program(_congregation_pwsh_executable NAMES pwsh.exe)
    if(_congregation_pwsh_executable)
        add_test(
                NAME tests_windowsauthenticodepolicy_pwsh
                COMMAND "${_congregation_pwsh_executable}"
                        -NoProfile
                        -ExecutionPolicy Bypass
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsauthenticodepolicy.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
        )
        set_tests_properties(tests_windowsauthenticodepolicy_pwsh PROPERTIES
                ENVIRONMENT "${_congregation_test_environment}"
        )

        add_test(
                NAME tests_necessaryauthenticode_pwsh
                COMMAND "${_congregation_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessaryauthenticode.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-authenticode.ps1"
        )
        set_tests_properties(tests_necessaryauthenticode_pwsh PROPERTIES
                ENVIRONMENT "${_congregation_test_environment}"
        )

        add_test(
                NAME tests_necessarywindowsrelease_pwsh
                COMMAND "${_congregation_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_necessarywindowsrelease.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/tools/necessary-windows-release.ps1"
        )
        set_tests_properties(tests_necessarywindowsrelease_pwsh PROPERTIES
                ENVIRONMENT "${_congregation_test_environment}"
        )
    endif()

    add_test(
            NAME tests_windowsmsiauthoringcontract
            COMMAND "${_congregation_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsmsiauthoringcontract.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows.ps1"
    )
    set_tests_properties(tests_windowsmsiauthoringcontract PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )

    add_test(
            NAME tests_windowsstorepackagecontract
            COMMAND "${_congregation_powershell_executable}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsstorepackagecontract.ps1"
                    -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows-store.ps1"
                    -ManifestTemplatePath "${CMAKE_SOURCE_DIR}/packaging/windows/AppxManifest.xml.in"
    )
    set_tests_properties(tests_windowsstorepackagecontract PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )
    if(_congregation_pwsh_executable)
        add_test(
                NAME tests_windowsstorepackagecontract_pwsh
                COMMAND "${_congregation_pwsh_executable}"
                        -NoProfile
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/tst_windowsstorepackagecontract.ps1"
                        -ScriptPath "${CMAKE_SOURCE_DIR}/build-windows-store.ps1"
                        -ManifestTemplatePath "${CMAKE_SOURCE_DIR}/packaging/windows/AppxManifest.xml.in"
        )
        set_tests_properties(tests_windowsstorepackagecontract_pwsh PROPERTIES
                ENVIRONMENT "${_congregation_test_environment}"
        )
    endif()

    set_tests_properties(tests_windowsbinarycontract PROPERTIES
            ENVIRONMENT "${_congregation_test_environment}"
    )
endif()

if(WIN32)
    set_tests_properties(
            tests_canvasdocumentviewmodel
            tests_psdcompatibilitydocument
            tests_canvastoolbarqmlcontract
            tests_mainqmlcontract
            tests_memberprofilelistbuilder
            tests_profileimageprocessor
            tests_applicationpreferences
            tests_nearbycongregationdiscovery
            tests_temporarycamerainput
            tests_licensemanager
            tests_congregationupdatemanager
            tests_macosbuildworkflowcontract
            tests_linuxbuildworkflowcontract
            tests_windowsbuildworkflowcontract
            tests_windowsauthenticodepolicy
            tests_necessaryauthenticode
            tests_necessarywindowsrelease
            tests_windowsmsiauthoringcontract
            tests_windowsstorepackagecontract
            tests_windowsbinarycontract
            tests_drawingsurfaceitem
            PROPERTIES
            ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiSharedCanvas::iiSharedCanvas>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiFileProvider::iiFileProvider>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiUpdateManager::iiUpdateManager>;PATH=path_list_prepend:$<TARGET_FILE_DIR:iiLicenseManager::iiLicenseManager>;PATH=path_list_prepend:$<TARGET_FILE_DIR:LVRS::LVRS>"
    )
endif()

set_property(
        TEST tests_drawingsurfaceitem
        APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "QT_QPA_PLATFORM=set:offscreen"
)
