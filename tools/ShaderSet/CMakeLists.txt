find_package(inja CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ── AkRenderSlangJIT — Slang JIT compilation types and runtime ──────────────
add_library(AkRenderSlangJIT STATIC)

target_include_directories(AkRenderSlangJIT PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_sources(AkRenderSlangJIT PRIVATE
    src/SlangJIT/SlangJIT.cpp
    src/SlangJIT/SlangCompileSession.cpp
)
target_link_libraries(AkRenderSlangJIT PUBLIC
    slang::slang
)

# ── AkRenderShaderSet — shader descriptor library + SlangJIT bridge ────────
add_library(AkRenderShaderSet STATIC)

target_include_directories(AkRenderShaderSet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(AkRenderShaderSet PRIVATE
    src/ShaderSet/ShaderDescriptor.cpp
    src/ShaderSet/ShaderSetRuntime.cpp
)

target_link_libraries(AkRenderShaderSet PUBLIC
    AkRenderSlangJIT
)

# ── Shader Set Generator module ──────────────────────────────────────────────
set(AKRENDER_SHADERSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ShaderSetGenerator.cmake")

# ── Tests ────────────────────────────────────────────────────────────────────

if (AKRENDER_SHADER_BUILD_TEST)
    find_package(GTest CONFIG REQUIRED)

    add_shader_set(test_manifest tests/GeneratorExample/manifest.cpp)
    add_shader_set(shader_compile_manifest tests/ShaderCompileExample/manifest.cpp)

    set(_test_manifest_gen_dir
        "${CMAKE_CURRENT_BINARY_DIR}/tests/GeneratorExample")
    set(_shader_compile_manifest_gen_dir
        "${CMAKE_CURRENT_BINARY_DIR}/tests/ShaderCompileExample")

    add_executable(AkRenderShaderTest
        tests/VirtualFileSystemTest.cpp
        tests/VirtualPathTest.cpp
        tests/PathMappingTest.cpp
        tests/VfsPlacementTest.cpp
        tests/ValidateTest.cpp
        tests/ManifestRegisterTest.cpp
        tests/BuilderTest.cpp
        tests/ManifestTest.cpp
        tests/GeneratorTest.cpp
        tests/ShaderCompileTest.cpp
        tests/ShaderDescriptorTest.cpp
        tests/ShaderSetRuntimeTest.cpp
        tests/DepfileTest.cpp
        src/ShaderSetGenerator/Manifest.cpp
        src/ShaderSetGenerator/VirtualPath.cpp
        src/ShaderSetGenerator/PathMapping.cpp
        src/ShaderSetGenerator/VfsPlacement.cpp
        src/ShaderSetGenerator/ManifestRegister.cpp
        src/ShaderSetGenerator/RegisterInternals.cpp
        src/ShaderSetGenerator/ModuleBuilder.cpp
        src/ShaderSetGenerator/SlangBuilder.cpp
        src/ShaderSetGenerator/SpirVFileBuilder.cpp
        src/ShaderSetGenerator/Validate.cpp
    )
    target_link_libraries(AkRenderShaderTest PRIVATE
        AkRenderShaderSet
        test_manifest
        shader_compile_manifest
        GTest::gtest_main
    )
    target_compile_options(AkRenderShaderTest PRIVATE -Wno-deprecated-declarations)
    target_compile_definitions(AkRenderShaderTest PRIVATE
        "TEST_MANIFEST_DEPFILE=\"${_test_manifest_gen_dir}/test_manifest.d\""
        "SHADER_COMPILE_MANIFEST_DEPFILE=\"${_shader_compile_manifest_gen_dir}/shader_compile_manifest.d\""
    )
    add_dependencies(AkRenderShaderTest test_manifest shader_compile_manifest)

    add_test(NAME AkRenderShaderUnitTests
        COMMAND AkRenderShaderTest
    )

    add_test(NAME ShaderSetGeneratedOutputsExist
        COMMAND ${CMAKE_COMMAND}
            -DFILE=${_test_manifest_gen_dir}/test_manifest.hpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AssertFileExists.cmake
    )
    add_test(NAME ShaderSetGeneratedSourceExists
        COMMAND ${CMAKE_COMMAND}
            -DFILE=${_test_manifest_gen_dir}/test_manifest.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AssertFileExists.cmake
    )
    add_test(NAME ShaderSetDepfileExists
        COMMAND ${CMAKE_COMMAND}
            -DFILE=${_test_manifest_gen_dir}/test_manifest.d
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AssertFileExists.cmake
    )
endif()

# ── Install (independent AkRenderShaderSet package) ───────────────────────────

set(_akrender_shaderset_cmake_dir
    ${CMAKE_INSTALL_LIBDIR}/cmake/AkRenderShaderSet)

install(TARGETS AkRenderShaderSet AkRenderSlangJIT
    EXPORT AkRenderShaderSetTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT AkRenderShaderSetTargets
    NAMESPACE AkRenderShaderSet::
    DESTINATION ${_akrender_shaderset_cmake_dir}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/AkRender/ShaderSet"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AkRender
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/AkRender/ShaderSetGenerator"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AkRender
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inc"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/AkRender/SlangJIT"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AkRender
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ShaderSetGenerator.cmake"
    DESTINATION ${_akrender_shaderset_cmake_dir}
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/template/ShaderSet.hpp.inja"
    "${CMAKE_CURRENT_SOURCE_DIR}/template/ShaderSet.cpp.inja"
    DESTINATION ${_akrender_shaderset_cmake_dir}/template
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/Generator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/Manifest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/VirtualPath.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/PathMapping.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/VfsPlacement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/ManifestRegister.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/RegisterInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/ModuleBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/SlangBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/SpirVFileBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/Validate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderSetGenerator/ManifestCompile.cpp"
    DESTINATION ${_akrender_shaderset_cmake_dir}/src/ShaderSetGenerator
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AkRenderShaderSetConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderShaderSetConfig.cmake"
    INSTALL_DESTINATION ${_akrender_shaderset_cmake_dir}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderShaderSetConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderShaderSetConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderShaderSetConfigVersion.cmake"
    DESTINATION ${_akrender_shaderset_cmake_dir}
)
