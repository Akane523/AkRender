add_library(AkRenderShaders SHARED)

target_include_directories(AkRenderShaders PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(AkRenderShaders PRIVATE
    src/ShaderSetGenerator/Manifest.cpp
)

# ── Tests ────────────────────────────────────────────────────────────────────

if (AKRENDER_SHADER_BUILD_TEST)
    find_package(GTest CONFIG REQUIRED)

    add_executable(AkRenderShaderTest
        tests/VirtualFileSystemTest.cpp
    )
    target_link_libraries(AkRenderShaderTest PRIVATE
        AkRenderShaders
        GTest::gtest_main
    )
    target_compile_options(AkRenderShaderTest PRIVATE -Wno-deprecated-declarations)

    add_test(NAME VirtualFileSystemTest
        COMMAND AkRenderShaderTest
    )
endif()