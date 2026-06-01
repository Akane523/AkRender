cmake_minimum_required(VERSION 3.21)
project(AkRender VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── CMake module path ────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Include Slang modules (provides FindSlang + SlangShaderCompiler) ─────────
include(SlangShaderCompiler)

# ── Find dependencies ────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
set(Stb_INCLUDE_DIR "${Stb_INCLUDE_DIR}" CACHE INTERNAL "STB include directory" FORCE)

# ── AkRender library ─────────────────────────────────────────────────────────
# INTERFACE library that aggregates all renderer dependencies.
# Consumers just do: target_link_libraries(my_app PRIVATE AkRender::AkRender)
add_library(AkRender INTERFACE)
add_library(AkRender::AkRender ALIAS AkRender)

target_link_libraries(AkRender INTERFACE
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glfw
    glm::glm
    spdlog::spdlog
    Slang::Compiler
)
target_include_directories(AkRender INTERFACE
    $<BUILD_INTERFACE:${Stb_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

install(TARGETS AkRender
    EXPORT AkRenderTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT AkRenderTargets
    NAMESPACE AkRender::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

# Install CMake modules so consumers can do find_package(AkRender)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SlangShaderCompiler.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindSlang.cmake"
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

# Ensure include dir exists in install tree (INTERFACE target requires it)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/.akrender_placeholder" "")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/.akrender_placeholder"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RENAME .akrender_keep
)
