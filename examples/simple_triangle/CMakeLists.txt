cmake_minimum_required(VERSION 3.21)
project(SimpleTriangle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# ── Find AkRender (must be installed first) ──────────────────────────────────
# Build AkRender then point CMAKE_PREFIX_PATH here:
#   cmake -B build -DCMAKE_PREFIX_PATH=<akrender-install-prefix>
find_package(AkRender REQUIRED)

# ── 1. Define a Slang module ─────────────────────────────────────────────────
# slang_add_module creates an INTERFACE target whose properties carry:
#   SLANG_MODULE_SOURCE_DIRS, SLANG_MODULE_OUTPUT_DIR, SLANG_MODULE_FILES
slang_add_module(math_module
    SOURCES modules/math_utils.slang
    MODULE_NAME math_utils
)

# ── 2. Define a shader target (depends on the module) ────────────────────────
# slang_add_shader creates an INTERFACE target whose properties carry:
#   SLANG_SPV_FILES, SLANG_SHADER_NAMES, SLANG_SHADER_OUTPUT_DIR,
#   SLANG_SHADER_SOURCES
slang_add_shader(triangle
    SOURCES shaders/triangle.slang
    MODULES math_module                # import math_utils resolved via -I
)

# ── 3. Query output properties — choose your consumption strategy ────────────
get_target_property(tri_spv    triangle SLANG_SPV_FILES)
get_target_property(tri_names  triangle SLANG_SHADER_NAMES)

message(STATUS "triangle .spv files:  ${tri_spv}")
message(STATUS "triangle shader names: ${tri_names}")

# ── 4. Generate a header from the properties (one of many possible approaches)
# Alternatives: xxd -i for binary embedding, or runtime file loading.
set(_header "${CMAKE_CURRENT_BINARY_DIR}/shader_data.h")
file(WRITE "${_header}"
    "#pragma once\n"
    "// Auto-generated — see CMakeLists.txt\n"
    "#define TRIANGLE_SPV \"${tri_spv}\"\n"
)

# ── 5. Build the C++ host ────────────────────────────────────────────────────
add_executable(simple_triangle src/main.cpp)
target_include_directories(simple_triangle PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(simple_triangle PRIVATE AkRender::AkRender)
target_link_shaders(simple_triangle triangle)
# target_link_shaders sets SLANG_LINKED_SHADERS on simple_triangle and adds
# a build dependency so .spv files are compiled before the C++ source.
