if(APPLE)
    set(CMAKE_FIND_FRAMEWORK FIRST)
    list(PREPEND CMAKE_SYSTEM_FRAMEWORK_PATH
        "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks"
        "${CMAKE_OSX_SYSROOT}/Library/Frameworks")

    set(CMAKE_DISABLE_FIND_PACKAGE_OpenGL ON CACHE BOOL "" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_WrapOpenGL ON CACHE BOOL "" FORCE)

    if(NOT TARGET WrapOpenGL::WrapOpenGL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "-framework OpenGL")
    endif()

    set(WrapOpenGL_FOUND TRUE CACHE BOOL "" FORCE)
    set(OPENGL_LIBRARIES "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_gl_LIBRARY "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_opengl_LIBRARY "-framework OpenGL" CACHE STRING "" FORCE)
    set(OPENGL_INCLUDE_DIR "" CACHE PATH "" FORCE)
endif()

target_sources(Congregation PRIVATE
        main.cpp
        metatypes_dummy.cpp
        metatypes_dummy.h
        models/brush/paletteutils.cpp
        models/brush/paletteutils.h
        models/canvas/canvasdocumentviewmodel.cpp
        models/canvas/canvasdocumentviewmodel.h
        models/canvas/canvasviewmodelbridge.cpp
        models/canvas/canvasviewmodelbridge.h
        models/collaboration/memberprofilelistbuilder.cpp
        models/collaboration/memberprofilelistbuilder.h
        models/document/psdcompatibilitydocument.cpp
        models/document/psdcompatibilitydocument.h
        models/document/psdimagereader.cpp
        models/document/psdimagereader.h
        models/document/psdimagewriter.cpp
        models/document/psdimagewriter.h
        models/document/recentcanvascontainer.cpp
        models/document/recentcanvascontainer.h
        models/input/temporarycamerainput.cpp
        models/input/temporarycamerainput.h
        models/license/licensemanager.cpp
        models/license/licensemanager.h
        models/license/accountmanager.cpp
        models/license/accountmanager.h
        models/license/licensecredentialstore.cpp
        models/license/licensecredentialstore.h
        models/network/localcanvassession.cpp
        models/network/localcanvassession.h
        models/network/nearbycongregationdiscovery.cpp
        models/network/nearbycongregationdiscovery.h
        models/painting/drawingsurfaceitem.cpp
        models/painting/drawingsurfaceitem.h
        models/preferences/applicationpreferences.cpp
        models/preferences/applicationpreferences.h
        models/profile/profileimageprocessor.cpp
        models/profile/profileimageprocessor.h
        models/update/congregationupdatecredentialprovider.cpp
        models/update/congregationupdatecredentialprovider.h
        models/update/congregationupdatemanager.cpp
        models/update/congregationupdatemanager.h
        resources.qrc
)

target_include_directories(Congregation PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/models/brush
        ${CMAKE_CURRENT_SOURCE_DIR}/models/canvas
        ${CMAKE_CURRENT_SOURCE_DIR}/models/collaboration
        ${CMAKE_CURRENT_SOURCE_DIR}/models/document
        ${CMAKE_CURRENT_SOURCE_DIR}/models/input
        ${CMAKE_CURRENT_SOURCE_DIR}/models/license
        ${CMAKE_CURRENT_SOURCE_DIR}/models/network
        ${CMAKE_CURRENT_SOURCE_DIR}/models/painting
        ${CMAKE_CURRENT_SOURCE_DIR}/models/preferences
        ${CMAKE_CURRENT_SOURCE_DIR}/models/profile
        ${CMAKE_CURRENT_SOURCE_DIR}/models/update
)
