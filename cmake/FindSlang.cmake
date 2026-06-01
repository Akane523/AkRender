# ── FindSlang.cmake ──────────────────────────────────────────────────────────
# Finds the Slang shader compiler (system package: shader-slang-dev).
#
# Exported targets:
#   Slang::Compiler   - slang-compiler shared library
#   Slang::RT         - slang-rt shared library (if found)
#
# Exported variables:
#   SLANG_FOUND           - TRUE if all required components found
#   SLANGC_EXECUTABLE     - path to slangc binary
#   SLANG_INCLUDE_DIR     - path to slang.h (under shader-slang/)
#   SLANG_COMPILER_LIBRARY - path to libslang-compiler.so
#   SLANG_RT_LIBRARY      - path to libslang-rt.so (may be NOTFOUND)
#
# Usage:
#   find_package(Slang REQUIRED)
#   target_link_libraries(my_app PRIVATE Slang::Compiler)

# ── slangc binary ────────────────────────────────────────────────────────────
find_program(SLANGC_EXECUTABLE slangc
    PATHS /usr/bin /usr/local/bin
    DOC "Slang shader compiler executable"
)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found. Install shader-slang package.")
endif()

# ── Headers ──────────────────────────────────────────────────────────────────
find_path(SLANG_INCLUDE_DIR
    NAMES slang.h slang-com-ptr.h
    PATH_SUFFIXES shader-slang
    PATHS /usr/include /usr/local/include
    DOC "Slang C++ headers directory"
)

if(NOT SLANG_INCLUDE_DIR)
    message(FATAL_ERROR "Slang headers not found. Install shader-slang-dev package.")
endif()

# ── Libraries ────────────────────────────────────────────────────────────────
find_library(SLANG_COMPILER_LIBRARY
    NAMES slang-compiler
    PATHS /usr/lib /usr/local/lib
    DOC "Slang compiler shared library"
)

if(NOT SLANG_COMPILER_LIBRARY)
    message(FATAL_ERROR "libslang-compiler not found. Install shader-slang-dev package.")
endif()

find_library(SLANG_RT_LIBRARY
    NAMES slang-rt
    PATHS /usr/lib /usr/local/lib
    DOC "Slang runtime library"
)

# ── Version ──────────────────────────────────────────────────────────────────
if(SLANGC_EXECUTABLE)
    execute_process(
        COMMAND ${SLANGC_EXECUTABLE} -help
        OUTPUT_VARIABLE _slangc_help
        ERROR_VARIABLE _slangc_help
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_slangc_help MATCHES "Slang Compiler v([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(SLANG_VERSION "${CMAKE_MATCH_1}")
    endif()
endif()

# ── Imported targets ─────────────────────────────────────────────────────────
if(NOT TARGET Slang::Compiler)
    add_library(Slang::Compiler UNKNOWN IMPORTED)
    set_target_properties(Slang::Compiler PROPERTIES
        IMPORTED_LOCATION "${SLANG_COMPILER_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
    )
    if(SLANG_RT_LIBRARY)
        set_target_properties(Slang::Compiler PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SLANG_RT_LIBRARY}"
        )
    endif()
endif()

if(SLANG_RT_LIBRARY AND NOT TARGET Slang::RT)
    add_library(Slang::RT UNKNOWN IMPORTED)
    set_target_properties(Slang::RT PROPERTIES
        IMPORTED_LOCATION "${SLANG_RT_LIBRARY}"
    )
endif()

# ── Standard find_package results ────────────────────────────────────────────
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
    REQUIRED_VARS SLANGC_EXECUTABLE SLANG_INCLUDE_DIR SLANG_COMPILER_LIBRARY
    VERSION_VAR SLANG_VERSION
    HANDLE_VERSION_RANGE
)

mark_as_advanced(
    SLANGC_EXECUTABLE
    SLANG_INCLUDE_DIR
    SLANG_COMPILER_LIBRARY
    SLANG_RT_LIBRARY
)
