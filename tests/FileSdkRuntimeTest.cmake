# A successful link can still load an older ABI-compatible library from a stale RPATH.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            --unset=DYLD_LIBRARY_PATH --unset=DYLD_FRAMEWORK_PATH
            --unset=DYLD_FALLBACK_LIBRARY_PATH
            "DYLD_PRINT_LIBRARIES=1" "QT_QPA_PLATFORM=offscreen"
            "${TEST_EXECUTABLE}"
            roundTripsNativeSharedCanvasDocument
            roundTripsRecentCanvasContainerWithEditableObjects
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 60)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "File SDK runtime consumer failed (${result}):\n${output}\n${error}")
endif()
foreach(library IN ITEMS IIFILEPROVIDER_LIBRARY IISHAREDCANVAS_LIBRARY IIPAINTENGINE_LIBRARY)
    get_filename_component(expected "${${library}}" REALPATH)
    string(FIND "${output}\n${error}" "${expected}" loaded_position)
    if(loaded_position LESS 0)
        message(FATAL_ERROR "Configured SDK was not loaded: ${expected}\n${output}\n${error}")
    endif()
    message(STATUS "Loaded configured SDK: ${expected}")
endforeach()
