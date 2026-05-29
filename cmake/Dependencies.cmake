include(FetchContent)

# ---------------------------------------------------------------------------
# Vulkan  (requires system Vulkan SDK or runtime loader)
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# ---------------------------------------------------------------------------
# GLFW  – windowing and input
# ---------------------------------------------------------------------------
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ---------------------------------------------------------------------------
# GLM  – header-only mathematics library
# ---------------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# Slang  – shader language compiler (JIT + offline compilation)
# ---------------------------------------------------------------------------
if(AKRENDER_SLANG_JIT OR AKRENDER_PRECOMPILE_SHADERS)
    if(AKRENDER_FETCH_SLANG)
        FetchContent_Declare(slang
            GIT_REPOSITORY https://github.com/shader-slang/slang.git
            GIT_TAG        v2025.6.1
            GIT_SHALLOW    TRUE
        )
        # Disable optional Slang targets we do not need
        set(SLANG_ENABLE_TESTS     OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_EXAMPLES  OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_GFX       OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_SLANGD    OFF CACHE BOOL "" FORCE)
        set(SLANG_ENABLE_REPLAYER  OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(slang)
    else()
        # User is expected to provide SLANG_DIR or have Slang installed
        find_package(Slang REQUIRED
            HINTS "$ENV{SLANG_DIR}" "${SLANG_DIR}"
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# CMakeRC  – embed binary files (compiled SPIR-V) into the executable
# ---------------------------------------------------------------------------
if(AKRENDER_PRECOMPILE_SHADERS)
    FetchContent_Declare(cmakerc
        GIT_REPOSITORY https://github.com/vector-of-bool/cmakerc.git
        GIT_TAG        2.0.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(cmakerc)
    include("${cmakerc_SOURCE_DIR}/CMakeRC.cmake")
endif()

# ---------------------------------------------------------------------------
# Catch2  – unit-testing framework
# ---------------------------------------------------------------------------
if(AKRENDER_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
    include(CTest)
    include(Catch)
endif()
