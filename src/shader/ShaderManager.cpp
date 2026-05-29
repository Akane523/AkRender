#include "ShaderManager.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <print>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Slang JIT headers  (compiled only when AKRENDER_SLANG_JIT is set)
// ---------------------------------------------------------------------------
#ifdef AKRENDER_SLANG_JIT
#  include <slang.h>
#  include <slang-com-ptr.h>
#endif

// ---------------------------------------------------------------------------
// CMakeRC embedded filesystem (compiled only when AKRENDER_PRECOMPILE_SHADERS)
// ---------------------------------------------------------------------------
#ifdef AKRENDER_PRECOMPILE_SHADERS
#  include <cmrc/cmrc.hpp>
CMRC_DECLARE(shaders);
#endif

namespace AkRender {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

static constexpr uint32_t kSpvMagic = 0x07230203u;

/// Validate that a byte buffer looks like SPIR-V.
[[nodiscard]] bool isValidSpirv(const void* data, std::size_t bytes) noexcept {
    if (bytes < 4 || bytes % 4 != 0) return false;
    uint32_t magic{};
    std::memcpy(&magic, data, sizeof(magic));
    return magic == kSpvMagic;
}

/// Convert a raw byte span to a SPIR-V word vector.
[[nodiscard]] std::vector<uint32_t> bytesToSpirv(const void* data, std::size_t bytes) {
    std::vector<uint32_t> spv(bytes / sizeof(uint32_t));
    std::memcpy(spv.data(), data, bytes);
    return spv;
}

#ifdef AKRENDER_SLANG_JIT
/// Map AkRender::ShaderStage → SlangStage.
[[nodiscard]] SlangStage toSlangStage(ShaderStage stage) noexcept {
    switch (stage) {
    case ShaderStage::Vertex:        return SLANG_STAGE_VERTEX;
    case ShaderStage::Fragment:      return SLANG_STAGE_FRAGMENT;
    case ShaderStage::Compute:       return SLANG_STAGE_COMPUTE;
    case ShaderStage::RayGeneration: return SLANG_STAGE_RAY_GENERATION;
    case ShaderStage::ClosestHit:    return SLANG_STAGE_CLOSEST_HIT;
    case ShaderStage::AnyHit:        return SLANG_STAGE_ANY_HIT;
    case ShaderStage::Miss:          return SLANG_STAGE_MISS;
    }
    return SLANG_STAGE_NONE;
}
#endif // AKRENDER_SLANG_JIT

} // anonymous namespace

// ---------------------------------------------------------------------------
// Impl  (holds Slang global session for the JIT path)
// ---------------------------------------------------------------------------
struct ShaderManager::Impl {
#ifdef AKRENDER_SLANG_JIT
    Slang::ComPtr<slang::IGlobalSession> globalSession;
#endif
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
ShaderManager::ShaderManager(CompileMode mode)
    : m_impl(std::make_unique<Impl>())
    , m_mode(mode)
{
#ifdef AKRENDER_SLANG_JIT
    if (mode == CompileMode::JIT) {
        SlangResult result = slang::createGlobalSession(m_impl->globalSession.writeRef());
        if (SLANG_FAILED(result)) {
            throw std::runtime_error("ShaderManager: failed to create Slang global session");
        }
    }
#else
    if (mode == CompileMode::JIT) {
        throw std::runtime_error(
            "ShaderManager: JIT mode requested but AKRENDER_SLANG_JIT is disabled at compile time");
    }
#endif
}

ShaderManager::~ShaderManager() = default;

ShaderManager::ShaderManager(ShaderManager&&) noexcept            = default;
ShaderManager& ShaderManager::operator=(ShaderManager&&) noexcept = default;

// ---------------------------------------------------------------------------
// JIT path: compileFromSource
// ---------------------------------------------------------------------------
std::expected<std::vector<uint32_t>, ShaderError>
ShaderManager::compileFromSource(std::string_view source,
                                 std::string_view moduleName,
                                 std::string_view entryPoint,
                                 ShaderStage      stage) const
{
#ifdef AKRENDER_SLANG_JIT
    assert(m_mode == CompileMode::JIT);
    assert(m_impl && m_impl->globalSession);

    // ---- Session ----
    slang::TargetDesc targetDesc{};
    targetDesc.format  = SLANG_SPIRV;
    targetDesc.profile = m_impl->globalSession->findProfile("spirv_1_5");

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets     = &targetDesc;
    sessionDesc.targetCount = 1;

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(
            m_impl->globalSession->createSession(sessionDesc, session.writeRef()))) {
        return std::unexpected(ShaderError::SessionCreationFailed);
    }

    // ---- Load module from in-memory source ----
    Slang::ComPtr<slang::IBlob>   diagBlob;
    Slang::ComPtr<slang::IModule> module;
    module = session->loadModuleFromSourceString(
        std::string(moduleName).c_str(),
        (std::string(moduleName) + ".slang").c_str(),
        std::string(source).c_str(),
        diagBlob.writeRef()
    );

    if (diagBlob && diagBlob->getBufferSize() > 0) {
        std::print(stderr, "[Slang] {}\n",
            static_cast<const char*>(diagBlob->getBufferPointer()));
    }
    if (!module) {
        return std::unexpected(ShaderError::CompilationFailed);
    }

    // ---- Entry point ----
    Slang::ComPtr<slang::IEntryPoint> ep;
    if (SLANG_FAILED(module->findEntryPointByName(
            std::string(entryPoint).c_str(), ep.writeRef()))) {
        return std::unexpected(ShaderError::CompilationFailed);
    }

    // ---- Compose ----
    slang::IComponentType* components[] = { module.get(), ep.get() };
    Slang::ComPtr<slang::IComponentType> composite;
    if (SLANG_FAILED(session->createCompositeComponentType(
            components, 2, composite.writeRef(), diagBlob.writeRef()))) {
        return std::unexpected(ShaderError::CompilationFailed);
    }

    // ---- Link ----
    Slang::ComPtr<slang::IComponentType> linked;
    if (SLANG_FAILED(composite->link(linked.writeRef(), diagBlob.writeRef()))) {
        if (diagBlob && diagBlob->getBufferSize() > 0) {
            std::print(stderr, "[Slang link] {}\n",
                static_cast<const char*>(diagBlob->getBufferPointer()));
        }
        return std::unexpected(ShaderError::CompilationFailed);
    }

    // ---- Get SPIR-V ----
    Slang::ComPtr<slang::IBlob> spirvBlob;
    if (SLANG_FAILED(linked->getTargetCode(
            0, spirvBlob.writeRef(), diagBlob.writeRef()))) {
        return std::unexpected(ShaderError::CompilationFailed);
    }

    const void*  data  = spirvBlob->getBufferPointer();
    std::size_t  bytes = spirvBlob->getBufferSize();
    if (!isValidSpirv(data, bytes)) {
        return std::unexpected(ShaderError::InvalidSpirv);
    }

    return bytesToSpirv(data, bytes);

#else  // !AKRENDER_SLANG_JIT
    (void)source; (void)moduleName; (void)entryPoint; (void)stage;
    return std::unexpected(ShaderError::CompilationFailed);
#endif
}

// ---------------------------------------------------------------------------
// JIT path: compileFromFile
// ---------------------------------------------------------------------------
std::expected<std::vector<uint32_t>, ShaderError>
ShaderManager::compileFromFile(std::string_view path,
                               std::string_view entryPoint,
                               ShaderStage      stage) const
{
    std::ifstream f(std::string(path), std::ios::in);
    if (!f) return std::unexpected(ShaderError::ResourceNotFound);

    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string src = ss.str();

    // Derive a module name from the filename stem
    auto stem = std::string(path);
    if (auto pos = stem.rfind('/'); pos != std::string::npos)
        stem = stem.substr(pos + 1);
    if (auto pos = stem.rfind('.'); pos != std::string::npos)
        stem = stem.substr(0, pos);

    return compileFromSource(src, stem, entryPoint, stage);
}

// ---------------------------------------------------------------------------
// Pre-compiled path: loadPrecompiled
// ---------------------------------------------------------------------------
std::expected<std::vector<uint32_t>, ShaderError>
ShaderManager::loadPrecompiled(std::string_view resourcePath) const
{
#ifdef AKRENDER_PRECOMPILE_SHADERS
    auto fs = cmrc::shaders::get_filesystem();

    if (!fs.exists(std::string(resourcePath))) {
        return std::unexpected(ShaderError::ResourceNotFound);
    }

    auto file  = fs.open(std::string(resourcePath));
    auto bytes = static_cast<std::size_t>(file.end() - file.begin());

    if (!isValidSpirv(file.begin(), bytes)) {
        return std::unexpected(ShaderError::InvalidSpirv);
    }

    return bytesToSpirv(file.begin(), bytes);

#else
    (void)resourcePath;
    return std::unexpected(ShaderError::ResourceNotFound);
#endif
}

} // namespace AkRender
