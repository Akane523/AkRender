cmake_minimum_required(VERSION 3.28)

project(AkRender
    VERSION 0.1.0
    DESCRIPTION "A toy renderer based on Vulkan and Shader Slang"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++26 standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(AKRENDER_BUILD_TESTS
    "Build AkRender unit tests"          ON)
option(AKRENDER_SLANG_JIT
    "Enable Slang JIT shader compilation at runtime"       ON)
option(AKRENDER_PRECOMPILE_SHADERS
    "Pre-compile shaders to SPIR-V and embed via cmakerc"  ON)
option(AKRENDER_FETCH_SLANG
    "Fetch and build Slang from source via FetchContent"   ON)
option(AKRENDER_VALIDATION_LAYERS
    "Enable Vulkan validation layers in debug builds"      ON)

# ---------------------------------------------------------------------------
# CMake module path
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Dependencies (Vulkan, GLFW, GLM, Slang, cmakerc, Catch2)
# ---------------------------------------------------------------------------
include(Dependencies)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(shaders)

if(AKRENDER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
