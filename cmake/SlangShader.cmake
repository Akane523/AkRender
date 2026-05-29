# cmake/SlangShader.cmake
# ---------------------------------------------------------------------------
# akrender_add_slang_shaders(<target>
#     SOURCES   file.slang ...
#     [OUTPUT_DIR   <dir>]               default: ${CMAKE_CURRENT_BINARY_DIR}/spv
#     [NAMESPACE    <ns>]                default: shaders
#     [SLANG_FLAGS  flag ...]            extra slangc flags
# )
#
# For every source file this function:
#   1. Calls slangc to compile each named entry point to a SPIR-V .spv file.
#   2. Collects all .spv files into a cmakerc resource library named
#      <target>_shaders_rc (NAMESPACE <ns>).
#   3. Links the resource library into <target>.
#
# Entry points are deduced from the filename stem suffix:
#   foo.vert.slang  ->  -stage vertex   -entry vertMain
#   foo.frag.slang  ->  -stage fragment -entry fragMain
#   foo.comp.slang  ->  -stage compute  -entry compMain
#   foo.rgen.slang  ->  -stage raygeneration -entry rgenMain
#   foo.rchit.slang ->  -stage closesthit    -entry rchitMain
#   foo.rahit.slang ->  -stage anyhit         -entry rahitMain
#   foo.rmiss.slang ->  -stage miss           -entry missMain
# Files without a recognised suffix are compiled without explicit stage/entry.
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.28)

function(akrender_add_slang_shaders TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARGS
        ""
        "OUTPUT_DIR;NAMESPACE"
        "SOURCES;SLANG_FLAGS"
    )

    if(NOT ARGS_OUTPUT_DIR)
        set(ARGS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/spv")
    endif()
    if(NOT ARGS_NAMESPACE)
        set(ARGS_NAMESPACE "shaders")
    endif()

    file(MAKE_DIRECTORY "${ARGS_OUTPUT_DIR}")

    # ------------------------------------------------------------------
    # Locate slangc
    # ------------------------------------------------------------------
    if(TARGET slangc)
        set(_slangc $<TARGET_FILE:slangc>)
    else()
        find_program(_slangc slangc
            HINTS "$ENV{SLANG_DIR}/bin" "${SLANG_DIR}/bin"
            DOC   "Path to slangc compiler"
        )
        if(NOT _slangc)
            message(FATAL_ERROR
                "slangc not found. Install the Slang SDK or set SLANG_DIR, "
                "or enable AKRENDER_FETCH_SLANG to build it from source.")
        endif()
    endif()

    # ------------------------------------------------------------------
    # Compile each source file
    # ------------------------------------------------------------------
    set(_compiled_spvs "")

    foreach(_src IN LISTS ARGS_SOURCES)
        # Resolve absolute path
        if(NOT IS_ABSOLUTE "${_src}")
            set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endif()

        get_filename_component(_base "${_src}" NAME)   # e.g. triangle.vert.slang

        # Strip trailing .slang
        string(REGEX REPLACE "\\.slang$" "" _stem "${_base}")  # triangle.vert

        # Determine stage/entry from inner extension
        get_filename_component(_inner_ext "${_stem}" LAST_EXT) # .vert

        if(_inner_ext STREQUAL ".vert")
            set(_stage vertex)
            set(_entry vertMain)
        elseif(_inner_ext STREQUAL ".frag")
            set(_stage fragment)
            set(_entry fragMain)
        elseif(_inner_ext STREQUAL ".comp")
            set(_stage compute)
            set(_entry compMain)
        elseif(_inner_ext STREQUAL ".rgen")
            set(_stage raygeneration)
            set(_entry rgenMain)
        elseif(_inner_ext STREQUAL ".rchit")
            set(_stage closesthit)
            set(_entry rchitMain)
        elseif(_inner_ext STREQUAL ".rahit")
            set(_stage anyhit)
            set(_entry rahitMain)
        elseif(_inner_ext STREQUAL ".rmiss")
            set(_stage miss)
            set(_entry missMain)
        else()
            set(_stage "")
            set(_entry "")
        endif()

        # Output filename mirrors the full stem: triangle.vert.spv
        set(_spv "${ARGS_OUTPUT_DIR}/${_stem}.spv")

        if(_stage)
            add_custom_command(
                OUTPUT  "${_spv}"
                COMMAND "${_slangc}"
                        -target spirv
                        -profile spirv_1_5
                        -stage  "${_stage}"
                        -entry  "${_entry}"
                        ${ARGS_SLANG_FLAGS}
                        -o      "${_spv}"
                        "${_src}"
                DEPENDS "${_src}"
                COMMENT "Slang → SPIR-V: ${_base} [${_stage}]"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT  "${_spv}"
                COMMAND "${_slangc}"
                        -target spirv
                        -profile spirv_1_5
                        ${ARGS_SLANG_FLAGS}
                        -o      "${_spv}"
                        "${_src}"
                DEPENDS "${_src}"
                COMMENT "Slang → SPIR-V: ${_base}"
                VERBATIM
            )
        endif()

        list(APPEND _compiled_spvs "${_spv}")
    endforeach()

    # ------------------------------------------------------------------
    # Embed compiled SPIR-V via cmakerc
    # ------------------------------------------------------------------
    set(_rc_target "${TARGET_NAME}_shaders_rc")
    cmrc_add_resource_library("${_rc_target}"
        NAMESPACE "${ARGS_NAMESPACE}"
        ${_compiled_spvs}
    )
    set_target_properties("${_rc_target}" PROPERTIES FOLDER "Shaders/Resources")

    target_link_libraries("${TARGET_NAME}" PRIVATE "${_rc_target}")
endfunction()
