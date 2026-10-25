# Keep the application icon separate from toolbar glyphs and the editable UI.
set(_app_icon_sources)
if(APPLE)
    set(_app_icon_resources "${CMAKE_SOURCE_DIR}/resources/Appicon.icns")
elseif(WIN32)
    enable_language(RC)
    set(CONGREGATION_APP_ICON_RC "${CMAKE_SOURCE_DIR}/resources/Appicon.ico")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/AppIcon.rc.in"
        "${CMAKE_BINARY_DIR}/AppIcon.rc" @ONLY)
    set(_app_icon_sources "${CMAKE_BINARY_DIR}/AppIcon.rc")
    set(_app_icon_resources "${CMAKE_SOURCE_DIR}/resources/AppIcon/Congregation.png")
else()
    set(_app_icon_resources "${CMAKE_SOURCE_DIR}/resources/AppIcon/Congregation.png")
endif()
add_custom_target(CongregationAppIconCheck
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/cmake/generate_app_icons.py" --check
    COMMENT "Verify Congregation app-icon sources and platform resources" VERBATIM)
if(APPLE)
    add_custom_target(CongregationAppIcons
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/cmake/generate_app_icons.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Regenerate platform icons from the Illustrator PNG master" VERBATIM)
endif()
