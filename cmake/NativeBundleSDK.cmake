# Only shared SDK targets need to be copied into a macOS application bundle.
function(congregation_bundle_sdk_arguments output target)
    get_target_property(_type "${target}" TYPE)
    if(_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
        set(${output} --library "$<TARGET_FILE:${target}>"
            --library-dir "$<TARGET_FILE_DIR:${target}>" PARENT_SCOPE)
    elseif(_type STREQUAL "STATIC_LIBRARY")
        set(${output} "" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unsupported SDK library type for ${target}: ${_type}")
    endif()
endfunction()
