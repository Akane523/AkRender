cmake_minimum_required(VERSION 3.21)
project(AkRender VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ──────────────────────────────────────────────────────────────────

option(AKRENDER_SHADER_BUILD_TEST "Build ShaderSet unit tests" ON)
option(AKRENDER_BUILD_FRAMEWORK "Build the AkRender bootstrap framework target" ON)

# ── Testing ─────────────────────────────────────────────────────────────────
if(AKRENDER_SHADER_BUILD_TEST)
    include(CTest)
    enable_testing()
endif()

# ── CMake module path ────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Find dependencies ────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

if(AKRENDER_BUILD_FRAMEWORK)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(Stb REQUIRED)
    set(Stb_INCLUDE_DIR "${Stb_INCLUDE_DIR}" CACHE INTERNAL "STB include directory" FORCE)
endif()

find_package(slang CONFIG REQUIRED)

# ── ShaderSet (independent package) ──────────────────────────────────────────
add_subdirectory(tools/ShaderSet)

# ── AkRender::Core — Vulkan post-init helpers ────────────────────────────────
# Minimal library: Vulkan + VMA only. No windowing or platform surface glue.
add_library(AkRenderCore STATIC)
set_target_properties(AkRenderCore PROPERTIES EXPORT_NAME Core)
add_library(AkRender::Core ALIAS AkRenderCore)

target_link_libraries(AkRenderCore PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
)

target_include_directories(AkRenderCore PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ── AkRender::AkRender — optional bootstrap framework ────────────────────────
# Platform/window helpers built on top of Core. Not required to use Core.
if(AKRENDER_BUILD_FRAMEWORK)
    add_library(AkRender STATIC)
    add_library(AkRender::AkRender ALIAS AkRender)

    target_link_libraries(AkRender PUBLIC
        AkRenderCore
        glfw
        glm::glm
        spdlog::spdlog
    )

    target_include_directories(AkRender PUBLIC
        $<BUILD_INTERFACE:${Stb_INCLUDE_DIR}>
    )

    if(WIN32)
        target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_METAL_EXT)
    else()
        target_compile_definitions(AkRender INTERFACE VK_USE_PLATFORM_XLIB_KHR)
    endif()
endif()

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(src)

# ── Install ──────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

set(_akrender_install_targets AkRenderCore)
if(AKRENDER_BUILD_FRAMEWORK)
    list(APPEND _akrender_install_targets AkRender)
endif()

install(TARGETS ${_akrender_install_targets}
    EXPORT AkRenderTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/AkRender"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(EXPORT AkRenderTargets
    NAMESPACE AkRender::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AkRenderConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AkRenderConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AkRender
)
