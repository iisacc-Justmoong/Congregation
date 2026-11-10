cmake_minimum_required(VERSION 3.31)
project(CongregationInteractiveCanvasTest LANGUAGES C CXX)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
file(REAL_PATH "${APP_ROOT}/resources" APP_RESOURCE_ROOT)
find_package(iiSDKCore 0.18 CONFIG REQUIRED COMPONENTS Runtime)
find_package(iiJson 0.1 CONFIG REQUIRED)
include("${APP_ROOT}/../../SDK/iiSocietyContainer/cmake/MobileStorageTarget.cmake")
iisociety_add_mobile_storage()
find_package(iiFileProviderNative CONFIG REQUIRED)
find_package(iiFileProviderCoreIO CONFIG REQUIRED)
find_package(iiFileProviderCompatibility CONFIG REQUIRED)
find_package(iiNetwork CONFIG REQUIRED)
find_package(iiAccountManagerNative CONFIG REQUIRED)
find_package(iiUpdateManagerNative CONFIG REQUIRED)
find_package(iiSharedCanvasNative 0.35.39 CONFIG REQUIRED)
get_target_property(canvas_include iiSharedCanvas::Native INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${canvas_include})
find_package(iiBrushTools CONFIG REQUIRED)
find_package(iiGraphicAdjust CONFIG REQUIRED)
find_package(iiImageCodec CONFIG REQUIRED)
find_package(iiGUI CONFIG REQUIRED)
find_package(LVUI CONFIG REQUIRED COMPONENTS Native Color)
find_package(iiColorManager 0.1.1 CONFIG REQUIRED COMPONENTS iiColorManager)
get_target_property(iigui_include iiGUI::iiGUI INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${iigui_include})
get_target_property(lvui_include LVUI::Native INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${lvui_include})
find_package(LibXml2 REQUIRED)
add_subdirectory("${APP_ROOT}/App/Engine" engine)
lvui_generate_styles(NAME CongregationStyle SOURCE "${APP_ROOT}/App/Native/views/Layout.lvui"
    NAMESPACE congregation.native.style OUTPUT_VARIABLE style_header)
add_custom_target(style_header DEPENDS "${style_header}")
add_library(artwork STATIC "${APP_ROOT}/App/Native/QuickMask.cpp" "${APP_ROOT}/App/Native/Artwork.cpp" "${APP_ROOT}/App/Native/SocietyFiles.cpp"
    "${APP_ROOT}/App/Native/ArtworkPerspective.cpp" "${APP_ROOT}/App/Native/Shapes.cpp"
    "${APP_ROOT}/App/Native/LegacyCanvas.cpp")
target_compile_features(artwork PUBLIC cxx_std_23)
target_include_directories(artwork PUBLIC "${APP_ROOT}/App")
target_compile_definitions(artwork PUBLIC CONGREGATION_NATIVE_WIDGET_ABI="${iiGUI_NATIVE_ABI_ID}")
target_link_libraries(artwork PUBLIC Congregation::Engine iiSharedCanvas::Native iiSocietyContainer::MobileStorage
    PRIVATE iiGUI::iiGUI iiBrushTools::iiBrushTools iiGraphicAdjust::iiGraphicAdjust)
add_library(tool_settings STATIC "${APP_ROOT}/App/Native/ToolSettings.cpp"
    "${APP_ROOT}/App/Native/ToolSettingsBrush.cpp" "${APP_ROOT}/App/Native/GradientSettings.cpp"
    "${APP_ROOT}/App/Native/TimelineModel.cpp")
target_compile_features(tool_settings PUBLIC cxx_std_23)
target_include_directories(tool_settings PUBLIC "${APP_ROOT}/App")
target_link_libraries(tool_settings PUBLIC Congregation::Engine iiSharedCanvas::Native iiBrushTools::iiBrushTools)
add_library(canvas_view STATIC "${APP_ROOT}/App/Native/CanvasView.cpp" "${APP_ROOT}/App/Native/EditorLayout.cpp")
add_dependencies(canvas_view style_header)
target_compile_features(canvas_view PUBLIC cxx_std_23)
target_include_directories(canvas_view PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(canvas_view PUBLIC artwork tool_settings iiGUI::iiGUI)
lvui_generate(NAME MainView SOURCE "${APP_ROOT}/App/Native/views/Main.lvui" OUTPUT_VARIABLE main_view
    STYLE_FILE "${APP_ROOT}/App/Native/views/Layout.lvui" INCLUDES Native/EditorBackend.h Native/NativeViewBackend.h)
lvui_generate(NAME PreferencesView SOURCE "${APP_ROOT}/App/Native/views/Preferences.lvui" OUTPUT_VARIABLE preferences_view
    STYLE_FILE "${APP_ROOT}/App/Native/views/Layout.lvui" INCLUDES Native/EditorBackend.h)
lvui_generate(NAME CanvasSetupView SOURCE "${APP_ROOT}/App/Native/views/CanvasSetup.lvui" OUTPUT_VARIABLE setup_view
    STYLE_FILE "${APP_ROOT}/App/Native/views/Layout.lvui" INCLUDES Native/CanvasSetupModel.h Native/NativeViewBackend.h)
lvui_generate(NAME ToolPanelView SOURCE "${APP_ROOT}/App/Native/views/ToolPanel.lvui" OUTPUT_VARIABLE tool_panel_view
    STYLE_FILE "${APP_ROOT}/App/Native/views/Layout.lvui" INCLUDES Native/ToolPanelBackend.h)
add_custom_target(tool_panel_generated DEPENDS "${tool_panel_view}")
add_library(application-source-check OBJECT EXCLUDE_FROM_ALL "${APP_ROOT}/App/Native/Application.cpp"
    "${main_view}" "${preferences_view}" "${setup_view}")
add_dependencies(application-source-check style_header)
target_compile_features(application-source-check PRIVATE cxx_std_23)
target_compile_definitions(application-source-check PRIVATE CONGREGATION_VERSION="test")
target_include_directories(application-source-check PRIVATE "${APP_ROOT}/App" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(application-source-check PRIVATE canvas_view LVUI::Native iiImageCodec::iiImageCodec)
add_executable(interactive-canvas "${APP_ROOT}/tests/InteractiveCanvasStroke.cpp")
target_link_libraries(interactive-canvas PRIVATE canvas_view)
enable_testing()
add_test(NAME Congregation.InteractiveCanvasStroke COMMAND interactive-canvas)
set_tests_properties(Congregation.InteractiveCanvasStroke PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
set(CONGREGATION_IIGUI_TEST_HEADERS "" CACHE PATH "Matching iiGUI private test header overlay")
if(CONGREGATION_IIGUI_TEST_HEADERS)
    if(NOT EXISTS "${CONGREGATION_IIGUI_TEST_HEADERS}/iiGUI/WidgetState.h")
        message(FATAL_ERROR "Set CONGREGATION_IIGUI_TEST_HEADERS to the matching iiGUI test overlay")
    endif()
    add_executable(long-stroke-presentation "${APP_ROOT}/tests/LongStrokePresentation.cpp")
    target_include_directories(long-stroke-presentation PRIVATE "${CONGREGATION_IIGUI_TEST_HEADERS}")
    target_link_libraries(long-stroke-presentation PRIVATE canvas_view)
    add_test(NAME Congregation.LongStrokePresentation COMMAND long-stroke-presentation)
    set_tests_properties(Congregation.LongStrokePresentation PROPERTIES TIMEOUT 600
        ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
    set(_presentation_test Congregation.LongStrokePresentation)
    get_filename_component(_lvui_test_prefix "${LVUI_DIR}/../../.." ABSOLUTE)
    add_executable(tool-panel-headless "${APP_ROOT}/tests/ToolPanelHeadless.cpp"
        "${APP_ROOT}/App/Native/ToolPanel.cpp" "${APP_ROOT}/App/Native/ToolPanelBackend.cpp" "${APP_ROOT}/App/Native/OwnedWidgetStyle.cpp"
        "${APP_ROOT}/App/Native/EditorLayout.cpp" "${APP_ROOT}/App/Native/EditorPalette.cpp"
        "${APP_ROOT}/App/Native/EditorBackend.cpp" "${APP_ROOT}/App/Native/EditorColor.cpp"
        "${APP_ROOT}/App/Native/PresentationList.cpp"
        "${_lvui_test_prefix}/share/LVUI/examples/Integrations/App/ColorSelectionSurface.cpp")
    add_dependencies(tool-panel-headless style_header tool_panel_generated)
    target_compile_features(tool-panel-headless PRIVATE cxx_std_23)
    target_include_directories(tool-panel-headless PRIVATE "${APP_ROOT}/App" "${CMAKE_CURRENT_BINARY_DIR}"
        "${CONGREGATION_IIGUI_TEST_HEADERS}")
    target_link_libraries(tool-panel-headless PRIVATE tool_settings LVUI::Native LVUI::Color
        iiGUI::iiGUI iiColorManager::iiColorManager)
    add_test(NAME Congregation.ToolPanelHeadless COMMAND tool-panel-headless)
    set_tests_properties(Congregation.ToolPanelHeadless PROPERTIES TIMEOUT 600
        ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
    set(_panel_test Congregation.ToolPanelHeadless)
endif()
add_executable(interactive-chunk "${APP_ROOT}/tests/InteractiveChunkStroke.cpp")
target_compile_features(interactive-chunk PRIVATE cxx_std_23)
target_link_libraries(interactive-chunk PRIVATE artwork)
add_test(NAME Congregation.InteractiveChunkStroke COMMAND interactive-chunk)
set_tests_properties(Congregation.InteractiveChunkStroke PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")

add_executable(interactive-raster-history "${APP_ROOT}/tests/InteractiveRasterHistory.cpp")
target_compile_features(interactive-raster-history PRIVATE cxx_std_23)
target_link_libraries(interactive-raster-history PRIVATE artwork)
add_test(NAME Congregation.InteractiveRasterHistory COMMAND interactive-raster-history)
set_tests_properties(Congregation.InteractiveRasterHistory PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
add_executable(society-canvas-storage "${APP_ROOT}/tests/SocietyCanvasStorageTest.cpp")
target_compile_features(society-canvas-storage PRIVATE cxx_std_23)
target_link_libraries(society-canvas-storage PRIVATE artwork)
add_test(NAME Congregation.SocietyCanvasStorage COMMAND society-canvas-storage)
set_tests_properties(Congregation.SocietyCanvasStorage PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
add_executable(startup-canvas-size "${APP_ROOT}/tests/StartupCanvasSize.cpp")
target_compile_features(startup-canvas-size PRIVATE cxx_std_23)
target_include_directories(startup-canvas-size PRIVATE "${APP_ROOT}/App")
target_link_libraries(startup-canvas-size PRIVATE iiGUI::iiGUI)
add_test(NAME Congregation.StartupCanvasSize COMMAND startup-canvas-size)
add_executable(editor-layout "${APP_ROOT}/tests/tst_editorlayout.cpp" "${APP_ROOT}/App/Native/EditorPalette.cpp")
target_compile_features(editor-layout PRIVATE cxx_std_23)
target_include_directories(editor-layout PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(editor-layout PRIVATE canvas_view)
add_test(NAME Congregation.EditorLayout COMMAND editor-layout)
add_executable(canvas-touch-input "${APP_ROOT}/tests/CanvasTouchInput.cpp")
target_include_directories(canvas-touch-input PRIVATE "${APP_ROOT}/../../SDK/iiGUI/src/Platform")
target_link_libraries(canvas-touch-input PRIVATE canvas_view)
add_test(NAME Congregation.CanvasTouchInput COMMAND canvas-touch-input)
set_tests_properties(Congregation.CanvasTouchInput PROPERTIES TIMEOUT 120 ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")

add_executable(stroke-performance "${APP_ROOT}/tests/StrokePerformanceIntegration.cpp")
target_compile_features(stroke-performance PRIVATE cxx_std_23)
target_link_libraries(stroke-performance PRIVATE artwork)
add_test(NAME Congregation.stroke_performance COMMAND stroke-performance)
add_executable(stroke-profile "${APP_ROOT}/tests/StrokeProfileTest.cpp")
target_compile_features(stroke-profile PRIVATE cxx_std_23)
add_test(NAME Congregation.StrokeProfile COMMAND stroke-profile)
add_test(NAME Congregation.stroke_history_4k COMMAND stroke-performance --4k-only)
add_test(NAME Congregation.stroke_history_variants_4k COMMAND stroke-performance --4k-variants-only)
set_tests_properties(Congregation.stroke_performance Congregation.stroke_history_4k
    Congregation.stroke_history_variants_4k PROPERTIES TIMEOUT 900
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")

foreach(test IN ITEMS BrushStrokeIntegration PenInputIntegration TaperIntegration StrokeStreamingIntegration SelectionIntegration FramePaintIntegration)
    add_executable(${test} "${APP_ROOT}/tests/${test}.cpp")
    target_link_libraries(${test} PRIVATE canvas_view)
    add_test(NAME Congregation.${test} COMMAND ${test})
    set_tests_properties(Congregation.${test} PROPERTIES TIMEOUT 600
        ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
endforeach()
add_executable(brush-tools-integration "${APP_ROOT}/tests/BrushToolsIntegration.cpp")
target_link_libraries(brush-tools-integration PRIVATE artwork tool_settings)
add_test(NAME Congregation.BrushToolsIntegration COMMAND brush-tools-integration)
set_tests_properties(Congregation.BrushToolsIntegration PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")

foreach(test IN ITEMS FiniteChunkIntegration CompactChunkStrokeIntegration TransformedChunkStrokeIntegration HistoryBudgetIntegration)
    add_executable(${test} "${APP_ROOT}/tests/${test}.cpp")
    target_compile_features(${test} PRIVATE cxx_std_23)
    target_link_libraries(${test} PRIVATE artwork)
    add_test(NAME Congregation.${test} COMMAND ${test})
    set_tests_properties(Congregation.${test} PROPERTIES TIMEOUT 600
        ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
endforeach()

# A developer shell can export DYLD paths to other installed SDK versions.
# The test must execute the same iiPaintEngine binary selected by CMake.
set(_clear_sdk_loader_paths
    "DYLD_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FALLBACK_FRAMEWORK_PATH=unset:;DYLD_VERSIONED_LIBRARY_PATH=unset:;DYLD_VERSIONED_FRAMEWORK_PATH=unset:")
set_tests_properties(Congregation.InteractiveCanvasStroke ${_presentation_test} ${_panel_test} Congregation.InteractiveChunkStroke Congregation.InteractiveRasterHistory Congregation.SocietyCanvasStorage Congregation.CanvasTouchInput
    Congregation.stroke_performance Congregation.stroke_history_4k Congregation.stroke_history_variants_4k
    Congregation.BrushStrokeIntegration Congregation.BrushToolsIntegration Congregation.PenInputIntegration
    Congregation.TaperIntegration Congregation.StrokeStreamingIntegration Congregation.SelectionIntegration Congregation.FramePaintIntegration
    Congregation.FiniteChunkIntegration Congregation.CompactChunkStrokeIntegration
    Congregation.TransformedChunkStrokeIntegration Congregation.HistoryBudgetIntegration
    PROPERTIES ENVIRONMENT_MODIFICATION "${_clear_sdk_loader_paths}")
