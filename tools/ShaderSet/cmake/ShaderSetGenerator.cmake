# ── ShaderSetGenerator.cmake ─────────────────────────────────────────────────
#
# CMake module for offline ShaderSet code generation integrated into the build
# graph.  Requires AkRenderShaderSet (provided by find_package(AkRenderShaderSet)).
#
# Resource layout (relative to this file):
#
#   <module-dir>/
#     ShaderSetGenerator.cmake
#     template/ShaderSet.hpp.inja
#     template/ShaderSet.cpp.inja
#     src/ShaderSetGenerator/Generator.cpp
#     src/ShaderSetGenerator/Manifest.cpp
#
# In-tree AkRender development sets AKRENDER_SHADERSET_ROOT to the
# tools/ShaderSet source directory before including this module.
#
# ═══════════════════════════  API  ════════════════════════════════════════════
#
#   add_shader_set(<name> <manifest.cpp>)
#
# Compiles and runs ShaderSetGenerator with the consumer-provided
# make_manifest() implementation, embeds binary resources declared in the
# manifest, and produces a STATIC library <name> linked against
# AkRenderShaderSet.
#
# Directory layout (relative paths in the manifest resolve against the manifest
# file's parent directory):
#
#   my_shader_set/
#     manifest.cpp          ← passed to add_shader_set()
#     shaders/…             ← optional resource tree
#
# Target properties on <name>:
#   SHADER_SET_SOURCE_DIR   — absolute source tree root
#   SHADER_SET_BINARY_DIR   — absolute generated output directory

# ══════════════════════════════════════════════════════════════════════════════
# Resolve resource root (templates + generator sources)
#
# AKRENDER_SHADERSET_ROOT may be preset by the in-tree build (tools/ShaderSet).
# Otherwise derive it from this module's install location.  Resolution happens
# at include-time because CMAKE_CURRENT_LIST_FILE inside functions refers to
# the caller's CMakeLists.txt, not this file.

get_filename_component(_AKRENDER_SHADERSET_MODULE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

if(NOT AKRENDER_SHADERSET_ROOT)
    if(IS_DIRECTORY "${_AKRENDER_SHADERSET_MODULE_DIR}/template")
        set(AKRENDER_SHADERSET_ROOT "${_AKRENDER_SHADERSET_MODULE_DIR}" CACHE INTERNAL
            "AkRender ShaderSet resource root (templates + generator sources)")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
#  add_shader_set

function(add_shader_set name manifest_source)
    if(NOT AKRENDER_SHADERSET_ROOT)
        message(FATAL_ERROR
            "ShaderSetGenerator: cannot locate ShaderSet resources.\n"
            "  Expected: ${_AKRENDER_SHADERSET_MODULE_DIR}/template\n"
            "  Set AKRENDER_SHADERSET_ROOT to the ShaderSet package directory.")
    endif()

    set(_template_dir "${AKRENDER_SHADERSET_ROOT}/template")
    set(_generator_src_dir "${AKRENDER_SHADERSET_ROOT}/src/ShaderSetGenerator")

    foreach(_required
        "${_template_dir}/ShaderSet.hpp.inja"
        "${_template_dir}/ShaderSet.cpp.inja"
        "${_generator_src_dir}/Generator.cpp"
        "${_generator_src_dir}/Manifest.cpp"
        "${_generator_src_dir}/VirtualPath.cpp"
        "${_generator_src_dir}/PathMapping.cpp"
        "${_generator_src_dir}/VfsPlacement.cpp"
        "${_generator_src_dir}/ManifestRegister.cpp"
        "${_generator_src_dir}/RegisterInternals.cpp"
        "${_generator_src_dir}/ModuleBuilder.cpp"
        "${_generator_src_dir}/SlangBuilder.cpp"
        "${_generator_src_dir}/SpirVFileBuilder.cpp"
        "${_generator_src_dir}/Validate.cpp"
        "${_generator_src_dir}/ManifestCompile.cpp"
    )
        if(NOT EXISTS "${_required}")
            message(FATAL_ERROR
                "ShaderSetGenerator: missing resource '${_required}'.\n"
                "  AKRENDER_SHADERSET_ROOT = ${AKRENDER_SHADERSET_ROOT}")
        endif()
    endforeach()

    if(NOT TARGET AkRenderShaderSet)
        message(FATAL_ERROR
            "add_shader_set requires target AkRenderShaderSet.\n"
            "  Call find_package(AkRenderShaderSet REQUIRED) before add_shader_set().")
    endif()

    if(NOT TARGET pantor::inja)
        find_package(inja CONFIG REQUIRED)
    endif()
    if(NOT TARGET CLI11::CLI11)
        find_package(CLI11 CONFIG REQUIRED)
    endif()

    file(REAL_PATH "${manifest_source}" manifest_source_real)

    if(NOT EXISTS "${manifest_source_real}")
        message(FATAL_ERROR "Manifest source file '${manifest_source}' does not exist.")
    endif()

    message(STATUS "Adding shader set '${name}' with manifest source '${manifest_source_real}'")

    cmake_path(GET manifest_source_real PARENT_PATH _source_dir)
    file(RELATIVE_PATH _source_dir_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source_dir}")
    cmake_path(APPEND _binary_dir "${CMAKE_CURRENT_BINARY_DIR}" "${_source_dir_rel}")

    set(_hpp_out "${_binary_dir}/${name}.hpp")
    set(_cpp_out "${_binary_dir}/${name}.cpp")
    set(_depfile "${_binary_dir}/${name}.d")

    message(STATUS "\t source dir: ${_source_dir}")
    message(STATUS "\t binary dir: ${_binary_dir}")

    # Generator executable — links the consumer's make_manifest() definition.
    add_executable(${name}_generator
        "${_generator_src_dir}/Generator.cpp"
        "${_generator_src_dir}/Manifest.cpp"
        "${_generator_src_dir}/VirtualPath.cpp"
        "${_generator_src_dir}/PathMapping.cpp"
        "${_generator_src_dir}/VfsPlacement.cpp"
        "${_generator_src_dir}/ManifestRegister.cpp"
        "${_generator_src_dir}/RegisterInternals.cpp"
        "${_generator_src_dir}/ModuleBuilder.cpp"
        "${_generator_src_dir}/SlangBuilder.cpp"
        "${_generator_src_dir}/SpirVFileBuilder.cpp"
        "${_generator_src_dir}/Validate.cpp"
        "${_generator_src_dir}/ManifestCompile.cpp"
        "${manifest_source_real}"
    )
    target_link_libraries(${name}_generator PRIVATE
        AkRenderShaderSet
        pantor::inja
        CLI11::CLI11
    )

    add_custom_command(
        OUTPUT "${_hpp_out}" "${_cpp_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_binary_dir}"
        COMMAND $<TARGET_FILE:${name}_generator>
            --source-dir "${_source_dir}"
            --binary-dir "${_binary_dir}"
            --name "${name}"
            --template-dir "${_template_dir}"
            --depfile "${_depfile}"
        DEPENDS ${name}_generator
                "${manifest_source_real}"
                "${_template_dir}/ShaderSet.hpp.inja"
                "${_template_dir}/ShaderSet.cpp.inja"
        DEPFILE "${_depfile}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "ShaderSetGenerator: ${name}"
        VERBATIM
    )

    add_library(${name} STATIC "${_cpp_out}" "${_hpp_out}")
    target_include_directories(${name} PUBLIC "${_binary_dir}")
    target_link_libraries(${name} PUBLIC AkRenderShaderSet)

    set_target_properties(${name} PROPERTIES
        SHADER_SET_SOURCE_DIR "${_source_dir}"
        SHADER_SET_BINARY_DIR "${_binary_dir}"
    )
endfunction()
