cmake_minimum_required(VERSION 3.21)
project(AkRender VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -- Options

option(AKRENDER_SHADER_BUILD_TEST "Build shader manager tests" ON)

# ── Testing ─────────────────────────────────────────────────────────────────
if(AKRENDER_SHADER_BUILD_TEST)
    include(CTest)
    enable_testing()
endif()

# ── CMake module path ────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Include Slang modules ────────────────────────────────────────────────────
find_package(slang CONFIG REQUIRED)
include(SlangShaderCompiler)

# ── Find dependencies ────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
set(Stb_INCLUDE_DIR "${Stb_INCLUDE_DIR}" CACHE INTERNAL "STB include directory" FORCE)

# AkRender Shader Manager
add_subdirectory(tools/ShaderSet)

# ── AkRender library ─────────────────────────────────────────────────────────
# STATIC library: links all renderer dependencies + carries implementation code.
# Consumers just do: target_link_libraries(my_app PRIVATE AkRender::AkRender)
add_library(AkRender STATIC)
add_library(AkRender::AkRender ALIAS AkRender)

target_link_libraries(AkRender PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glfw
    glm::glm
    spdlog::spdlog
    slang::slang
)
target_include_directories(AkRender PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${Stb_INCLUDE_DIR}>
)

# Platform-specific Vulkan surface extension
if(WIN32)
    target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_METAL_EXT)
else()
    target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_XLIB_KHR)
endif()

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(src)

# ── Install ──────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

# Library binary + export
install(TARGETS AkRender
    EXPORT AkRenderTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Public headers
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/AkRender"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(EXPORT AkRenderTargets
    NAMESPACE AkRender::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

# Install CMake modules so consumers can do find_package(AkRender)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SlangShaderCompiler.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AkRenderConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)
