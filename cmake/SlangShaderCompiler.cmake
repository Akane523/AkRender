# ── SlangShaderCompiler.cmake ────────────────────────────────────────────────
#
# CMake module for Slang shader compilation integrated into the build graph.
# Treats shaders and modules as build targets — like add_library / target_link_libraries.
#
# All compilation outputs are exposed as target properties (queryable via
# get_target_property).  This cleanly separates compilation from consumption:
# use the same shader target whether you want file-path defines, binary
# embedding, or runtime loading.
#
# ═══════════════════════════  API  ════════════════════════════════════════════
#
#   slang_add_module(<name>
#       SOURCES <file>...
#       [MODULE_NAME <name>]
#   )
#     Pre-compiles .slang sources into a .slang-module.  Creates an INTERFACE
#     library target <name>.
#
#     Target properties (query with get_target_property):
#       SLANG_MODULE_SOURCE_DIRS  — source directories (list)
#       SLANG_MODULE_OUTPUT_DIR   — where the .slang-module is written
#       SLANG_MODULE_FILES        — absolute source-file paths (list)
#
#   slang_add_shader(<name>
#       SOURCES <file.slang>...
#       [MODULES <module-target>...]
#       [SLANGC_OPTIONS <option>...]
#   )
#     Compiles .slang sources to SPIR-V.  Creates an INTERFACE library
#     target <name>.
#
#     Target properties:
#       SLANG_SPV_FILES           — absolute .spv output paths (list)
#       SLANG_SHADER_NAMES        — plain shader names, one per .spv (list)
#       SLANG_SHADER_OUTPUT_DIR   — directory containing the .spv files
#       SLANG_SHADER_SOURCES      — absolute source-file paths (list)
#
#   target_link_shaders(<target> <shader-target>...)
#     Adds a build-dependency on compiled shaders.  Sets property
#     SLANG_LINKED_SHADERS on <target> for downstream introspection.
#     Does NOT inject any compile definitions.
#
#   slang_target_shaders(<target>
#       SOURCES <file.slang>...
#       [MODULES <module-target>...]
#       [SLANGC_OPTIONS <option>...]
#   )
#     Compiles shaders and attaches them directly to <target> (no intermediate
#     shader library).  Sets property SLANG_LINKED_SHADERS on <target> with
#     the collected .spv paths.
# ══════════════════════════════════════════════════════════════════════════════
#   Slang IR-module / SPIR-V pipeline
# ══════════════════════════════════════════════════════════════════════════════
#
#   ┌──────────────┐     slangc -emit-ir          ┌──────────────────┐
#   │ math.slang   │ ──────────────────────────►  │ math.slang-module│
#   └──────────────┘     -module-name math         └──────────────────┘
#                                                         │
#                                          import math;    │ -I <dir>
#   ┌──────────────┐     slangc -target spirv              ▼
#   │ vert.slang   │ ──────────────────────────►  ┌──────────────────┐
#   └──────────────┘                              │   vert.spv       │
#                                                 └──────────────────┘
#
#   The shader step uses -depfile for automatic transitive dependency tracking.

# ══════════════════════════════════════════════════════════════════════════════
# Require Slang
if(NOT TARGET Slang::Compiler AND NOT Slang_FOUND)
    find_package(Slang REQUIRED)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Default Slang compiler options for SPIR-V output
set(SLANG_DEFAULT_SPIRV_OPTIONS -target spirv -matrix-layout-row-major -O2
    CACHE STRING "Default slangc options for SPIR-V compilation"
)

# ══════════════════════════════════════════════════════════════════════════════
# _slang_abs_paths — normalise a list of relative/absolute file paths
macro(_slang_abs_paths OUTVAR FILES)
    set(${OUTVAR} "")
    foreach(_f ${FILES})
        if(NOT IS_ABSOLUTE "${_f}")
            get_filename_component(_abs "${_f}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            set(_abs "${_f}")
        endif()
        list(APPEND ${OUTVAR} "${_abs}")
    endforeach()
endmacro()

# ══════════════════════════════════════════════════════════════════════════════
# _slang_collect_module_includes — gather -I flags from module targets
#
# For each module target, collects:
#   - The module's source directory   (-I for import resolution of .slang)
#   - The module's output directory   (-I for linking .slang-module)
#
# Sets _INCLUDE_FLAGS and _MODULE_DEPS_ABS in parent scope.
function(_slang_collect_module_includes MODULE_TARGETS)
    set(_flags "")
    set(_deps "")

    foreach(_mod ${MODULE_TARGETS})
        if(NOT TARGET ${_mod})
            message(WARNING "Module target '${_mod}' not found")
            continue()
        endif()

        get_target_property(_src_dirs ${_mod} SLANG_MODULE_SOURCE_DIRS)
        get_target_property(_out_dir  ${_mod} SLANG_MODULE_OUTPUT_DIR)
        get_target_property(_mod_files ${_mod} SLANG_MODULE_FILES)

        if(_src_dirs)
            foreach(_d ${_src_dirs})
                list(APPEND _flags "-I" "${_d}")
            endforeach()
        endif()
        if(_out_dir)
            list(APPEND _flags "-I" "${_out_dir}")
        endif()
        if(_mod_files)
            list(APPEND _deps ${_mod_files})
        endif()
    endforeach()

    set(_INCLUDE_FLAGS "${_flags}" PARENT_SCOPE)
    set(_MODULE_DEPS_ABS "${_deps}" PARENT_SCOPE)
endfunction()

# ══════════════════════════════════════════════════════════════════════════════
#  slang_add_module
# ══════════════════════════════════════════════════════════════════════════════
#
#   slang_add_module(<name>
#       SOURCES <file.slang>...
#       [MODULE_NAME <name>]
#   )
#
# Compiles one or more .slang files into a .slang-module (Slang IR container).
# Creates INTERFACE library target <name>.
#
# Target properties set on <name>:
#   SLANG_MODULE_SOURCE_DIRS  — absolute source directories (list)
#   SLANG_MODULE_OUTPUT_DIR   — where .slang-module is written
#   SLANG_MODULE_FILES        — absolute source-file paths (list)
#
function(slang_add_module TARGET_NAME)
    cmake_parse_arguments(ARG "" "MODULE_NAME" "SOURCES" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "slang_add_module(${TARGET_NAME}) requires SOURCES")
    endif()

    if(NOT ARG_MODULE_NAME)
        set(ARG_MODULE_NAME "${TARGET_NAME}")
    endif()

    _slang_abs_paths(_sources_abs "${ARG_SOURCES}")

    # Deduce source directories (one or more, for the -I search path)
    set(_src_dirs "")
    foreach(_s ${_sources_abs})
        get_filename_component(_d "${_s}" DIRECTORY)
        list(APPEND _src_dirs "${_d}")
    endforeach()
    list(REMOVE_DUPLICATES _src_dirs)

    set(_out_dir "${CMAKE_BINARY_DIR}/slang-modules/${TARGET_NAME}")
    set(_module_output "${_out_dir}/${ARG_MODULE_NAME}.slang-module")

    file(MAKE_DIRECTORY "${_out_dir}")

    add_custom_command(
        OUTPUT "${_module_output}"
        COMMAND ${SLANGC_EXECUTABLE}
            ${_sources_abs}
            -emit-ir
            -module-name "${ARG_MODULE_NAME}"
            -o "${_module_output}"
        DEPENDS ${_sources_abs}
        COMMENT "Slang module: ${ARG_MODULE_NAME}.slang-module"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Stamp target
    set(_stamp "${TARGET_NAME}_module_compile")
    add_custom_target("${_stamp}" DEPENDS "${_module_output}")

    # INTERFACE library target — consumers link to this name
    add_library(${TARGET_NAME} INTERFACE)
    add_dependencies(${TARGET_NAME} "${_stamp}")

    set_target_properties(${TARGET_NAME} PROPERTIES
        SLANG_MODULE_SOURCE_DIRS "${_src_dirs}"
        SLANG_MODULE_OUTPUT_DIR  "${_out_dir}"
        SLANG_MODULE_FILES       "${_sources_abs}"
    )
endfunction()

# ══════════════════════════════════════════════════════════════════════════════
#  slang_add_shader
# ══════════════════════════════════════════════════════════════════════════════
#
#   slang_add_shader(<name>
#       SOURCES <file.slang>...
#       [MODULES <module-target>...]
#       [SLANGC_OPTIONS <option>...]
#   )
#
# Compiles .slang files to SPIR-V.  If MODULES are specified, their source
# and output directories are added as -I paths so `import` works.
#
#     Target properties:
#       SLANG_SPV_FILES           — absolute .spv output paths (list)
#       SLANG_SHADER_NAMES        — plain shader names, one per .spv (list)
#       SLANG_SHADER_OUTPUT_DIR   — directory containing the .spv files
#       SLANG_SHADER_SOURCES      — absolute source-file paths (list)
#
function(slang_add_shader TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;MODULES;SLANGC_OPTIONS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "slang_add_shader(${TARGET_NAME}) requires SOURCES")
    endif()

    if(NOT ARG_SLANGC_OPTIONS)
        set(ARG_SLANGC_OPTIONS ${SLANG_DEFAULT_SPIRV_OPTIONS})
    endif()

    _slang_abs_paths(_sources_abs "${ARG_SOURCES}")

    # Collect include flags and module dependencies
    _slang_collect_module_includes("${ARG_MODULES}")

    set(_out_dir "${CMAKE_BINARY_DIR}/shaders/${TARGET_NAME}")
    file(MAKE_DIRECTORY "${_out_dir}")

    set(_spv_outputs "")
    set(_shader_names "")

    foreach(_shader_file ${_sources_abs})
        get_filename_component(_name "${_shader_file}" NAME_WE)
        set(_spv "${_out_dir}/${_name}.spv")
        set(_depfile "${_out_dir}/${_name}.d")

        add_custom_command(
            OUTPUT "${_spv}"
            COMMAND ${SLANGC_EXECUTABLE}
                "${_shader_file}"
                ${ARG_SLANGC_OPTIONS}
                ${_INCLUDE_FLAGS}
                -o "${_spv}"
                -depfile "${_depfile}"
            DEPENDS ${_sources_abs} ${_MODULE_DEPS_ABS}
            DEPFILE "${_depfile}"
            COMMENT "Slang SPIR-V: ${_name}.slang -> ${_name}.spv"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        list(APPEND _spv_outputs "${_spv}")
        list(APPEND _shader_names "${_name}")
    endforeach()

    # Stamp target
    set(_stamp "${TARGET_NAME}_shader_compile")
    add_custom_target("${_stamp}" DEPENDS ${_spv_outputs})

    # INTERFACE library target
    add_library(${TARGET_NAME} INTERFACE)
    add_dependencies(${TARGET_NAME} "${_stamp}")
    if(ARG_MODULES)
        add_dependencies(${TARGET_NAME} ${ARG_MODULES})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        SLANG_SPV_FILES         "${_spv_outputs}"
        SLANG_SHADER_NAMES      "${_shader_names}"
        SLANG_SHADER_OUTPUT_DIR "${_out_dir}"
        SLANG_SHADER_SOURCES    "${_sources_abs}"
    )
endfunction()

# ══════════════════════════════════════════════════════════════════════════════
#  target_link_shaders
# ══════════════════════════════════════════════════════════════════════════════
#
#   target_link_shaders(<target> <shader-target>...)
#
# Adds a pure build-dependency from <target> to each shader target so that
# .spv files are compiled before <target>.  Sets property SLANG_LINKED_SHADERS
# on <target> for downstream introspection (list of linked shader target names).
# Does NOT inject any compile definitions.
#
function(target_link_shaders TARGET)
    set(_linked "")

    # Collect previously linked shaders so we accumulate across calls
    get_target_property(_existing ${TARGET} SLANG_LINKED_SHADERS)
    if(_existing)
        list(APPEND _linked ${_existing})
    endif()

    foreach(_shader_lib ${ARGN})
        if(NOT TARGET ${_shader_lib})
            message(WARNING "target_link_shaders: '${_shader_lib}' is not a target")
            continue()
        endif()

        add_dependencies(${TARGET} ${_shader_lib})
        list(APPEND _linked "${_shader_lib}")
    endforeach()

    if(_linked)
        list(REMOVE_DUPLICATES _linked)
        set_target_properties(${TARGET} PROPERTIES
            SLANG_LINKED_SHADERS "${_linked}"
        )
    endif()
endfunction()

# ══════════════════════════════════════════════════════════════════════════════
#  slang_target_shaders
# ══════════════════════════════════════════════════════════════════════════════
#
#   slang_target_shaders(<target>
#       SOURCES <file.slang>...
#       [MODULES <module-target>...]
#       [SLANGC_OPTIONS <option>...]
#   )
#
# Compiles shaders and attaches them directly to <target>.  Sets property
# SLANG_LINKED_SHADERS on <target> with an internally-generated synthetic
# target name so get_target_property consumers can find the .spv paths.
#
function(slang_target_shaders TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES;MODULES;SLANGC_OPTIONS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "slang_target_shaders(${TARGET}) requires SOURCES")
    endif()

    if(NOT ARG_SLANGC_OPTIONS)
        set(ARG_SLANGC_OPTIONS ${SLANG_DEFAULT_SPIRV_OPTIONS})
    endif()

    _slang_abs_paths(_sources_abs "${ARG_SOURCES}")
    _slang_collect_module_includes("${ARG_MODULES}")

    set(_out_dir "${CMAKE_BINARY_DIR}/shaders/${TARGET}")
    file(MAKE_DIRECTORY "${_out_dir}")

    set(_spv_outputs "")
    set(_shader_names "")

    foreach(_shader_file ${_sources_abs})
        get_filename_component(_name "${_shader_file}" NAME_WE)
        set(_spv "${_out_dir}/${_name}.spv")
        set(_depfile "${_out_dir}/${_name}.d")

        add_custom_command(
            OUTPUT "${_spv}"
            COMMAND ${SLANGC_EXECUTABLE}
                "${_shader_file}"
                ${ARG_SLANGC_OPTIONS}
                ${_INCLUDE_FLAGS}
                -o "${_spv}"
                -depfile "${_depfile}"
            DEPENDS ${_sources_abs} ${_MODULE_DEPS_ABS}
            DEPFILE "${_depfile}"
            COMMENT "Slang SPIR-V: ${_name}.slang -> ${_name}.spv"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        # Attach .spv as a dependency
        add_custom_target("${TARGET}_shader_${_name}" DEPENDS "${_spv}")
        add_dependencies(${TARGET} "${TARGET}_shader_${_name}")
        list(APPEND _spv_outputs "${_spv}")
        list(APPEND _shader_names "${_name}")
    endforeach()

    # Create a synthetic INTERFACE target to hold the output properties
    set(_synth "${TARGET}_slang_shaders")
    if(NOT TARGET "${_synth}")
        add_library("${_synth}" INTERFACE)
    endif()
    set_target_properties("${_synth}" PROPERTIES
        SLANG_SPV_FILES         "${_spv_outputs}"
        SLANG_SHADER_NAMES      "${_shader_names}"
        SLANG_SHADER_OUTPUT_DIR "${_out_dir}"
        SLANG_SHADER_SOURCES    "${_sources_abs}"
    )

    # Record linkage so get_target_property consumers can introspect
    set_target_properties(${TARGET} PROPERTIES
        SLANG_LINKED_SHADERS "${_synth}"
    )
endfunction()
