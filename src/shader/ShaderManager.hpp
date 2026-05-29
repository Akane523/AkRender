#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Forward-declare Slang types so this header compiles without <slang.h>.
// The implementation (.cpp) includes <slang.h> under AKRENDER_SLANG_JIT.
// ---------------------------------------------------------------------------
namespace slang {
struct IGlobalSession;
} // namespace slang

namespace AkRender {

// ---------------------------------------------------------------------------
// ShaderStage
// ---------------------------------------------------------------------------
enum class ShaderStage : uint8_t {
    Vertex,
    Fragment,
    Compute,
    RayGeneration,
    ClosestHit,
    AnyHit,
    Miss,
};

// ---------------------------------------------------------------------------
// ShaderError
// ---------------------------------------------------------------------------
enum class ShaderError : uint8_t {
    /// Slang or GLSL compilation failed.
    CompilationFailed,
    /// A requested pre-compiled resource was not found in the embedded FS.
    ResourceNotFound,
    /// The loaded binary is not valid SPIR-V (wrong size or bad magic).
    InvalidSpirv,
    /// Could not create a Slang global / per-request session.
    SessionCreationFailed,
};

// ---------------------------------------------------------------------------
// ShaderManager
// ---------------------------------------------------------------------------
/// Manages shader compilation and loading via two complementary paths:
///
///  JIT mode (AKRENDER_SLANG_JIT)
///    Compiles Slang source strings or files at runtime using the Slang API.
///    Suitable for iteration and tooling.
///
///  Precompiled mode (AKRENDER_PRECOMPILE_SHADERS)
///    Loads SPIR-V blobs that were compiled at build time and embedded into
///    the binary by cmakerc.  Zero runtime compile overhead.
///
/// Both modes return the same `std::vector<uint32_t>` SPIR-V representation,
/// so the rest of the render pipeline is agnostic to the compile path.
class ShaderManager {
public:
    // -----------------------------------------------------------------------
    // Types
    // -----------------------------------------------------------------------
    enum class CompileMode : uint8_t {
        JIT,         ///< Runtime compilation via Slang (requires AKRENDER_SLANG_JIT)
        Precompiled, ///< Load cmakerc-embedded SPIR-V  (requires AKRENDER_PRECOMPILE_SHADERS)
    };

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    explicit ShaderManager(CompileMode mode = CompileMode::JIT);
    ~ShaderManager();

    ShaderManager(const ShaderManager&)            = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) noexcept;
    ShaderManager& operator=(ShaderManager&&) noexcept;

    // -----------------------------------------------------------------------
    // JIT path  (CompileMode::JIT, requires AKRENDER_SLANG_JIT)
    // -----------------------------------------------------------------------

    /// Compile a Slang shader from an in-memory source string.
    ///
    /// @param source      Slang source code.
    /// @param moduleName  Logical module name (used by Slang diagnostics).
    /// @param entryPoint  Name of the entry-point function inside @p source.
    /// @param stage       Target shader stage.
    /// @returns SPIR-V bytecode on success, or a ShaderError.
    [[nodiscard]]
    std::expected<std::vector<uint32_t>, ShaderError>
    compileFromSource(std::string_view source,
                      std::string_view moduleName,
                      std::string_view entryPoint,
                      ShaderStage      stage) const;

    /// Compile a Slang shader from a file on disk.
    ///
    /// @param path        Path to the .slang file.
    /// @param entryPoint  Name of the entry-point function.
    /// @param stage       Target shader stage.
    /// @returns SPIR-V bytecode on success, or a ShaderError.
    [[nodiscard]]
    std::expected<std::vector<uint32_t>, ShaderError>
    compileFromFile(std::string_view path,
                    std::string_view entryPoint,
                    ShaderStage      stage) const;

    // -----------------------------------------------------------------------
    // Pre-compiled path  (CompileMode::Precompiled, requires AKRENDER_PRECOMPILE_SHADERS)
    // -----------------------------------------------------------------------

    /// Load a pre-compiled SPIR-V shader from the cmakerc embedded filesystem.
    ///
    /// The @p resourcePath should match the path passed to
    /// `akrender_add_slang_shaders` in CMake, e.g. "triangle.vert.spv".
    ///
    /// @param resourcePath  Path within the "shaders" cmakerc namespace.
    /// @returns SPIR-V bytecode on success, or a ShaderError.
    [[nodiscard]]
    std::expected<std::vector<uint32_t>, ShaderError>
    loadPrecompiled(std::string_view resourcePath) const;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    [[nodiscard]] CompileMode mode() const noexcept { return m_mode; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl; ///< Holds Slang session (JIT path)
    CompileMode           m_mode;
};

// ---------------------------------------------------------------------------
// Helper: human-readable error description
// ---------------------------------------------------------------------------
[[nodiscard]] constexpr std::string_view toString(ShaderError err) noexcept {
    switch (err) {
    case ShaderError::CompilationFailed:    return "shader compilation failed";
    case ShaderError::ResourceNotFound:     return "pre-compiled resource not found";
    case ShaderError::InvalidSpirv:         return "invalid SPIR-V binary";
    case ShaderError::SessionCreationFailed: return "Slang session creation failed";
    }
    return "unknown shader error";
}

} // namespace AkRender
