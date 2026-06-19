find_package(inja CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

# ── AkRenderSlangJIT — Slang JIT compilation types and runtime ──────────────
add_library(AkRenderSlangJIT STATIC)
target_include_directories(AkRenderSlangJIT PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_sources(AkRenderSlangJIT PRIVATE
    src/SlangJIT/SlangJIT.cpp
)
target_link_libraries(AkRenderSlangJIT PUBLIC
    slang::slang
)

# ── AkRenderShaderSet — shader descriptor library + SlangJIT bridge ────────
add_library(AkRenderShaderSet STATIC)

target_include_directories(AkRenderShaderSet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(AkRenderShaderSet PRIVATE
    src/ShaderSet/ShaderDescriptor.cpp
)

target_link_libraries(AkRenderShaderSet PUBLIC
    AkRenderSlangJIT
)

# Use the fixed template directory for now.
set(_template_dir ${CMAKE_CURRENT_LIST_DIR}/template)

# ── Shader Set Generator — offline code generation tool ────────────────────
# NOTE: make_manifest() is provided by the consumer.  For testing, we link the
# example implementation from example/GeneratorExample/.

function(add_shader_set name manifest_source)
    file(REAL_PATH ${manifest_source} manifest_source_real)

    if (NOT EXISTS ${manifest_source_real})
        message(FATAL_ERROR "Manifest source file '${manifest_source}' does not exist.")
    endif()

    message(STATUS "Adding shader set '${name}' with manifest source '${manifest_source_real}'")

    cmake_path(GET manifest_source_real PARENT_PATH manifest_dir)
    set(_source_dir ${manifest_dir}/src)
    file(RELATIVE_PATH _source_dir_rel ${CMAKE_CURRENT_SOURCE_DIR} ${_source_dir})
    cmake_path(APPEND _binary_dir ${CMAKE_CURRENT_BINARY_DIR} ${_source_dir_rel})

    message(STATUS "\t source dir: ${_source_dir}")
    message(STATUS "\t binary dir: ${_binary_dir}")

    add_executable(
        ${name}_generator
        src/ShaderSetGenerator/Generator.cpp
        src/ShaderSetGenerator/Manifest.cpp
        ${manifest_source}
    )
    target_link_libraries(${name}_generator PRIVATE
        AkRenderShaderSet
        pantor::inja
        CLI11::CLI11
    )

    # Other part is not implemented yet.
endfunction()

# ── Tests ────────────────────────────────────────────────────────────────────

if (AKRENDER_SHADER_BUILD_TEST)
    find_package(GTest CONFIG REQUIRED)

    add_executable(AkRenderShaderTest
        tests/VirtualFileSystemTest.cpp
    )
    target_link_libraries(AkRenderShaderTest PRIVATE
        AkRenderShaderSet
        GTest::gtest_main
    )
    target_compile_options(AkRenderShaderTest PRIVATE -Wno-deprecated-declarations)

    add_shader_set(test_manifest tests/GeneratorExample/manifest.cpp)

    add_test(NAME VirtualFileSystemTest
        COMMAND AkRenderShaderTest
    )
endif()