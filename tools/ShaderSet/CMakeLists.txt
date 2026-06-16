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

# ── Shader Set Generator — offline code generation tool ────────────────────
# NOTE: make_manifest() is provided by the consumer.  For testing, we link the
# example implementation from example/GeneratorExample/.

function(add_shader_set name manifest_source)
    add_executable(
        ${name}
        src/ShaderSetGenerator/Generator.cpp
        src/ShaderSetGenerator/Manifest.cpp
        ${manifest_source}
    )
    target_link_libraries(${name} PRIVATE
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