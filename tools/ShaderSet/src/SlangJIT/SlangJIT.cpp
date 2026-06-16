#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <slang.h>

#include <stdexcept>

namespace AkRender::ShaderSet
{

// ═════════════════════════════════════════════════════════════════════════════
//  Internal helpers
// ═════════════════════════════════════════════════════════════════════════════

namespace
{

class BlobGuard
{
public:
  explicit BlobGuard(slang::IBlob *blob = nullptr) : m_blob(blob)
  {
  }
  ~BlobGuard()
  {
    if (m_blob)
      m_blob->release();
  }
  BlobGuard(const BlobGuard &) = delete;
  BlobGuard &operator=(const BlobGuard &) = delete;
  [[nodiscard]] slang::IBlob *get() const noexcept
  {
    return m_blob;
  }
  [[nodiscard]] slang::IBlob **addr() noexcept
  {
    return &m_blob;
  }

private:
  slang::IBlob *m_blob;
};

std::string getDiag(slang::IBlob *blob)
{
  if (!blob)
    return {};
  const char *t = static_cast<const char *>(blob->getBufferPointer());
  return t ? std::string(t) : std::string{};
}

SlangCompileTarget toSlangTarget(TargetFormat fmt)
{
  switch (fmt)
  {
    case TargetFormat::SpirV:
      return SLANG_SPIRV;
    case TargetFormat::DXIL:
      return SLANG_DXIL;
    case TargetFormat::WGSL:
      return SLANG_WGSL_SPIRV;
  }
  return SLANG_SPIRV;
}

SlangFloatingPointMode toSlangFloat(FloatMode m)
{
  switch (m)
  {
    case FloatMode::Default:
      return SLANG_FLOATING_POINT_MODE_DEFAULT;
    case FloatMode::Precise:
      return SLANG_FLOATING_POINT_MODE_PRECISE;
    case FloatMode::Fast:
      return SLANG_FLOATING_POINT_MODE_FAST;
  }
  return SLANG_FLOATING_POINT_MODE_DEFAULT;
}

SlangMatrixLayoutMode toSlangMatrix(MatrixLayout ml)
{
  return (ml == MatrixLayout::ColumnMajor) ? SLANG_MATRIX_LAYOUT_COLUMN_MAJOR
                                           : SLANG_MATRIX_LAYOUT_ROW_MAJOR;
}

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
//  SlangJITCompiler::Impl
// ═════════════════════════════════════════════════════════════════════════════

struct SlangJITCompiler::Impl
{
  slang::IGlobalSession *globalSession = nullptr;
  slang::ISession *session = nullptr;

  ~Impl()
  {
    if (session)
    {
      session->release();
      session = nullptr;
    }
    if (globalSession)
    {
      globalSession->release();
      globalSession = nullptr;
    }
  }
};

// ═════════════════════════════════════════════════════════════════════════════
//  SlangJITCompiler
// ═════════════════════════════════════════════════════════════════════════════

SlangJITCompiler::SlangJITCompiler() : m_impl(std::make_unique<Impl>())
{
  if (SLANG_FAILED(slang::createGlobalSession(nullptr, &m_impl->globalSession)))
  {
    m_impl.reset();
    throw std::runtime_error(
        "SlangJITCompiler: failed to create global session");
  }
}

SlangJITCompiler::~SlangJITCompiler() = default;

bool SlangJITCompiler::createSession(
    std::span<const std::filesystem::path> searchPaths,
    std::span<const PreprocessorDefine> defines, const CompileOptions &options)
{
  if (!m_impl || !m_impl->globalSession)
    return false;
  destroySession();

  slang::TargetDesc td{};
  td.structureSize = sizeof(slang::TargetDesc);
  td.format = toSlangTarget(options.target_format);
  td.floatingPointMode = toSlangFloat(options.float_mode);

  slang::SessionDesc sd{};
  sd.structureSize = sizeof(slang::SessionDesc);
  sd.targets = &td;
  sd.targetCount = 1;
  sd.defaultMatrixLayoutMode = toSlangMatrix(options.matrix_layout);

  std::vector<std::string> paths;
  std::vector<const char *> pathPtrs;
  for (const auto &p : searchPaths)
  {
    paths.push_back(p.string());
    pathPtrs.push_back(paths.back().c_str());
  }
  if (!pathPtrs.empty())
  {
    sd.searchPaths = pathPtrs.data();
    sd.searchPathCount = static_cast<SlangInt>(pathPtrs.size());
  }

  std::vector<slang::PreprocessorMacroDesc> macroDescs;
  std::vector<std::string> macroNames, macroValues;
  for (const auto &d : defines)
  {
    macroNames.push_back(std::string(d.name));
    macroValues.push_back(std::string(d.value));
    slang::PreprocessorMacroDesc md{};
    md.name = macroNames.back().c_str();
    md.value = macroValues.back().c_str();
    macroDescs.push_back(md);
  }
  if (!macroDescs.empty())
  {
    sd.preprocessorMacros = macroDescs.data();
    sd.preprocessorMacroCount = static_cast<SlangInt>(macroDescs.size());
  }

  slang::ISession *s = nullptr;
  if (SLANG_FAILED(m_impl->globalSession->createSession(sd, &s)) || !s)
    return false;

  m_impl->session = s;
  return true;
}

void SlangJITCompiler::destroySession()
{
  if (!m_impl)
    return;
  if (m_impl->session)
  {
    m_impl->session->release();
    m_impl->session = nullptr;
  }
}

bool SlangJITCompiler::loadModuleFromIR(std::string_view moduleName,
                                        const void *data, size_t size)
{
  if (!m_impl || !m_impl->session || !data || size == 0)
    return false;

  std::string nm(moduleName);
  BlobGuard diag;
  slang::IModule *m = slang_loadModuleFromIRBlob(
      m_impl->session, nm.c_str(), nullptr, data, size, diag.addr());
  return m != nullptr;
}

bool SlangJITCompiler::loadModuleFromSource(std::string_view moduleName,
                                            std::string_view source,
                                            std::string_view filePath)
{
  if (!m_impl || !m_impl->session || source.empty())
    return false;

  std::string nm(moduleName), fp(filePath), src(source);
  BlobGuard diag;
  slang::IModule *m = m_impl->session->loadModuleFromSourceString(
      nm.c_str(), fp.c_str(), src.c_str(), diag.addr());
  return m != nullptr;
}

CompileResult
SlangJITCompiler::compileEntryPoint(std::string_view entryPointName)
{
  CompileResult r;
  if (!m_impl || !m_impl->session)
  {
    r.diagnostic = "SlangJITCompiler: no active session";
    return r;
  }
  if (m_impl->session->getLoadedModuleCount() == 0)
  {
    r.diagnostic = "SlangJITCompiler: no modules loaded";
    return r;
  }

  std::string epName(entryPointName);
  slang::IEntryPoint *foundEP = nullptr;
  SlangInt nMod = m_impl->session->getLoadedModuleCount();

  for (SlangInt i = 0; i < nMod; ++i)
  {
    slang::IModule *mod = m_impl->session->getLoadedModule(i);
    if (!mod)
      continue;
    slang::IEntryPoint *ep = nullptr;
    if (SLANG_SUCCEEDED(mod->findEntryPointByName(epName.c_str(), &ep)) && ep)
    {
      foundEP = ep;
      break;
    }
    BlobGuard d;
    if (SLANG_SUCCEEDED(mod->findAndCheckEntryPoint(
            epName.c_str(), SLANG_STAGE_COMPUTE, &ep, d.addr())) &&
        ep)
    {
      foundEP = ep;
      break;
    }
  }

  if (!foundEP)
  {
    r.diagnostic = "SlangJITCompiler: entry point '";
    r.diagnostic += entryPointName;
    r.diagnostic += "' not found";
    return r;
  }

  std::vector<slang::IComponentType *> comps;
  comps.reserve(static_cast<size_t>(nMod) + 1);
  for (SlangInt i = 0; i < nMod; ++i)
  {
    auto *mod = m_impl->session->getLoadedModule(i);
    if (mod)
      comps.push_back(mod);
  }
  comps.push_back(foundEP);

  BlobGuard linkDiag;
  slang::IComponentType *composite = nullptr;
  SlangResult lr = m_impl->session->createCompositeComponentType(
      comps.data(), static_cast<SlangInt>(comps.size()), &composite,
      linkDiag.addr());
  if (SLANG_FAILED(lr) || !composite)
  {
    r.diagnostic = "SlangJITCompiler: link failed: ";
    r.diagnostic += getDiag(linkDiag.get());
    return r;
  }

  // Modules contribute 0 entry points by default; the single entry point we
  // explicitly added is the only one, so its index is always 0.
  constexpr SlangInt epIdx = 0;

  slang::IBlob *outCode = nullptr, *outDiag = nullptr;
  SlangResult cr = composite->getEntryPointCode(epIdx, 0, &outCode, &outDiag);
  BlobGuard cg(outCode), dg(outDiag);

  if (SLANG_FAILED(cr) || !outCode)
  {
    r.diagnostic = "SlangJITCompiler: codegen failed";
    std::string d = getDiag(outDiag);
    if (!d.empty())
    {
      r.diagnostic += ": ";
      r.diagnostic += d;
    }
    composite->release();
    return r;
  }

  const auto *words =
      static_cast<const uint32_t *>(outCode->getBufferPointer());
  r.binary.assign(words, words + outCode->getBufferSize() / sizeof(uint32_t));
  r.success = true;
  std::string d = getDiag(outDiag);
  if (!d.empty())
    r.diagnostic = std::move(d);

  composite->release();
  return r;
}

} // namespace AkRender::ShaderSet
