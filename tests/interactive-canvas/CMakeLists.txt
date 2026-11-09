cmake_minimum_required(VERSION 3.31)
project(CongregationInteractiveCanvasTest LANGUAGES C CXX)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
file(REAL_PATH "${APP_ROOT}/resources" APP_RESOURCE_ROOT)
find_package(iiSDKCore 0.18 CONFIG REQUIRED COMPONENTS Runtime)
find_package(iiJson 0.1 CONFIG REQUIRED)
find_package(iiFileProviderNative CONFIG REQUIRED)
find_package(iiFileProviderCoreIO CONFIG REQUIRED)
find_package(iiFileProviderCompatibility CONFIG REQUIRED)
find_package(iiNetwork CONFIG REQUIRED)
find_package(iiAccountManagerNative CONFIG REQUIRED)
find_package(iiUpdateManagerNative CONFIG REQUIRED)
find_package(iiSharedCanvasNative 0.35.38 CONFIG REQUIRED)
get_target_property(canvas_include iiSharedCanvas::Native INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${canvas_include})
find_package(iiBrushTools CONFIG REQUIRED)
find_package(iiGraphicAdjust CONFIG REQUIRED)
find_package(iiGUI CONFIG REQUIRED)
find_package(LVUI CONFIG REQUIRED COMPONENTS Native)
get_target_property(iigui_include iiGUI::iiGUI INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${iigui_include})
get_target_property(lvui_include LVUI::Native INTERFACE_INCLUDE_DIRECTORIES)
include_directories(BEFORE ${lvui_include})
find_package(LibXml2 REQUIRED)
add_subdirectory("${APP_ROOT}/App/Engine" engine)
lvui_generate_styles(NAME CongregationStyle SOURCE "${APP_ROOT}/App/Native/views/Layout.lvui"
    NAMESPACE congregation.native.style OUTPUT_VARIABLE style_header)
add_custom_target(style_header DEPENDS "${style_header}")
add_library(artwork STATIC "${APP_ROOT}/App/Native/QuickMask.cpp" "${APP_ROOT}/App/Native/Artwork.cpp"
    "${APP_ROOT}/App/Native/ArtworkPerspective.cpp" "${APP_ROOT}/App/Native/Shapes.cpp"
    "${APP_ROOT}/App/Native/LegacyCanvas.cpp")
target_compile_features(artwork PUBLIC cxx_std_23)
target_include_directories(artwork PUBLIC "${APP_ROOT}/App")
target_compile_definitions(artwork PUBLIC CONGREGATION_NATIVE_WIDGET_ABI="${iiGUI_NATIVE_ABI_ID}")
target_link_libraries(artwork PUBLIC Congregation::Engine iiSharedCanvas::Native
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
add_executable(interactive-canvas "${APP_ROOT}/tests/InteractiveCanvasStroke.cpp")
target_link_libraries(interactive-canvas PRIVATE canvas_view)
enable_testing()
add_test(NAME Congregation.InteractiveCanvasStroke COMMAND interactive-canvas)
set_tests_properties(Congregation.InteractiveCanvasStroke PROPERTIES TIMEOUT 600
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
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

add_executable(stroke-performance "${APP_ROOT}/tests/StrokePerformanceIntegration.cpp")
target_compile_features(stroke-performance PRIVATE cxx_std_23)
target_link_libraries(stroke-performance PRIVATE artwork)
add_test(NAME Congregation.stroke_performance COMMAND stroke-performance)
set_tests_properties(Congregation.stroke_performance PROPERTIES TIMEOUT 900
    ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")

foreach(test IN ITEMS BrushStrokeIntegration PenInputIntegration TaperIntegration SelectionIntegration FramePaintIntegration)
    add_executable(${test} "${APP_ROOT}/tests/${test}.cpp")
    target_link_libraries(${test} PRIVATE canvas_view)
    add_test(NAME Congregation.${test} COMMAND ${test})
    set_tests_properties(Congregation.${test} PROPERTIES TIMEOUT 600
        ENVIRONMENT "TMPDIR=${APP_RESOURCE_ROOT}")
endforeach()

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
set_tests_properties(Congregation.InteractiveCanvasStroke Congregation.InteractiveChunkStroke Congregation.InteractiveRasterHistory
    Congregation.stroke_performance Congregation.BrushStrokeIntegration Congregation.PenInputIntegration
    Congregation.TaperIntegration Congregation.SelectionIntegration Congregation.FramePaintIntegration
    Congregation.FiniteChunkIntegration Congregation.CompactChunkStrokeIntegration
    Congregation.TransformedChunkStrokeIntegration Congregation.HistoryBudgetIntegration
    PROPERTIES ENVIRONMENT_MODIFICATION "${_clear_sdk_loader_paths}")
