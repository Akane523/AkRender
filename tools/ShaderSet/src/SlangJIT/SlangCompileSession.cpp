#include <AkRender/SlangJIT/SlangCompileSession.hpp>

#include <slang.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace AkRender::ShaderSet
{

namespace
{

class BlobGuard
{
public:
  explicit BlobGuard(slang::IBlob *blob = nullptr) : m_blob(blob) {}
  ~BlobGuard()
  {
    if (m_blob)
      m_blob->release();
  }
  BlobGuard(const BlobGuard &) = delete;
  BlobGuard &operator=(const BlobGuard &) = delete;
  [[nodiscard]] slang::IBlob *get() const noexcept { return m_blob; }
  [[nodiscard]] slang::IBlob **addr() noexcept { return &m_blob; }

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

std::string readTextFile(const std::filesystem::path &path)
{
  std::ifstream ifs(path);
  if (!ifs)
    return {};
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

std::vector<std::byte> blobToBytes(slang::IBlob *blob)
{
  if (!blob || blob->getBufferSize() == 0)
    return {};
  const auto *data = static_cast<const std::byte *>(blob->getBufferPointer());
  return {data, data + blob->getBufferSize()};
}

} // namespace

SlangStage toSlangStage(Stage stage)
{
  switch (stage)
  {
    case Stage::Vertex:
      return SLANG_STAGE_VERTEX;
    case Stage::Fragment:
      return SLANG_STAGE_FRAGMENT;
    case Stage::Compute:
      return SLANG_STAGE_COMPUTE;
    case Stage::Geometry:
      return SLANG_STAGE_GEOMETRY;
    case Stage::TessControl:
      return SLANG_STAGE_HULL;
    case Stage::TessEval:
      return SLANG_STAGE_DOMAIN;
    case Stage::Mesh:
      return SLANG_STAGE_MESH;
    case Stage::Task:
      return SLANG_STAGE_AMPLIFICATION;
    case Stage::RayGeneration:
      return SLANG_STAGE_RAY_GENERATION;
    case Stage::RayIntersection:
      return SLANG_STAGE_INTERSECTION;
    case Stage::RayAnyHit:
      return SLANG_STAGE_ANY_HIT;
    case Stage::RayClosestHit:
      return SLANG_STAGE_CLOSEST_HIT;
    case Stage::RayMiss:
      return SLANG_STAGE_MISS;
    case Stage::RayCallable:
      return SLANG_STAGE_CALLABLE;
    case Stage::Amplification:
      return SLANG_STAGE_AMPLIFICATION;
  }
  return SLANG_STAGE_COMPUTE;
}

struct SlangCompileSession::Impl
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

SlangCompileSession::SlangCompileSession() : m_impl(std::make_unique<Impl>())
{
  if (SLANG_FAILED(slang::createGlobalSession(&m_impl->globalSession)))
  {
    m_impl.reset();
    throw std::runtime_error(
        "SlangCompileSession: failed to create global session");
  }
}

SlangCompileSession::~SlangCompileSession() = default;

bool SlangCompileSession::hasSession() const noexcept
{
  return m_impl && m_impl->session;
}

bool SlangCompileSession::createSession(
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
    macroNames.emplace_back(d.name);
    macroValues.emplace_back(d.value);
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

void SlangCompileSession::destroySession()
{
  if (!m_impl)
    return;
  if (m_impl->session)
  {
    m_impl->session->release();
    m_impl->session = nullptr;
  }
}

bool SlangCompileSession::loadModuleFromIR(std::string_view moduleName,
                                           const void *data, size_t size)
{
  if (!m_impl || !m_impl->session || !data || size == 0)
    return false;

  BlobGuard sourceBlob(slang_createBlob(data, size));
  if (!sourceBlob.get())
    return false;

  SlangInt moduleVersion = 0;
  const char *irModuleName = nullptr;
  const char *irCompilerVersion = nullptr;
  if (SLANG_SUCCEEDED(slang_loadModuleInfoFromIRBlob(
          m_impl->session, data, size, moduleVersion, irCompilerVersion,
          irModuleName)) &&
      irModuleName && irModuleName[0] != '\0')
  {
    moduleName = irModuleName;
  }

  std::string nm(moduleName);
  BlobGuard diag;
  slang::IModule *module = m_impl->session->loadModuleFromIRBlob(
      nm.c_str(), nm.c_str(), sourceBlob.get(), diag.addr());
  if (module != nullptr)
    return true;

  if (diag.get())
  {
    const char *text = static_cast<const char *>(diag.get()->getBufferPointer());
    if (text && text[0] != '\0')
      std::cerr << "SlangCompileSession: " << text << '\n';
  }

  return false;
}

bool SlangCompileSession::loadModuleFromSource(std::string_view moduleName,
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

ModuleIRResult SlangCompileSession::compileModuleFromFile(
    std::string_view moduleName, const std::filesystem::path &sourcePath,
    std::span<const PreprocessorDefine> defines)
{
  ModuleIRResult result;

  if (!m_impl || !m_impl->session)
  {
    result.diagnostic = "SlangCompileSession: no active session";
    return result;
  }

  const std::string source = readTextFile(sourcePath);
  if (source.empty())
  {
    result.diagnostic =
        "SlangCompileSession: cannot read source file: " + sourcePath.string();
    return result;
  }

  std::string nm(moduleName);
  BlobGuard diag;
  slang::IModule *module = m_impl->session->loadModuleFromSourceString(
      nm.c_str(), sourcePath.string().c_str(), source.c_str(), diag.addr());
  if (!module)
  {
    result.diagnostic = getDiag(diag.get());
    if (result.diagnostic.empty())
      result.diagnostic = "SlangCompileSession: failed to load module '" +
                          nm + "' from " + sourcePath.string();
    return result;
  }

  for (SlangInt32 i = 0; i < module->getDependencyFileCount(); ++i)
  {
    if (const char *dep = module->getDependencyFilePath(i))
      result.dependency_files.emplace_back(dep);
  }

  BlobGuard serialized;
  if (SLANG_FAILED(module->serialize(serialized.addr())) || !serialized.get())
  {
    result.diagnostic = "SlangCompileSession: failed to serialize module '" +
                        nm + "'";
    return result;
  }

  result.ir = blobToBytes(serialized.get());
  result.success = !result.ir.empty();
  return result;
}

CompileResult SlangCompileSession::compileEntryPoint(
    std::string_view entryPointName, Stage stage)
{
  CompileResult r;
  if (!m_impl || !m_impl->session)
  {
    r.diagnostic = "SlangCompileSession: no active session";
    return r;
  }
  if (m_impl->session->getLoadedModuleCount() == 0)
  {
    r.diagnostic = "SlangCompileSession: no modules loaded";
    return r;
  }

  std::string epName(entryPointName);
  slang::IEntryPoint *foundEP = nullptr;
  const SlangInt nMod = m_impl->session->getLoadedModuleCount();

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
            epName.c_str(), toSlangStage(stage), &ep, d.addr())) &&
        ep)
    {
      foundEP = ep;
      break;
    }
  }

  if (!foundEP)
  {
    r.diagnostic = "SlangCompileSession: entry point '";
    r.diagnostic += entryPointName;
    r.diagnostic += "' not found";
    return r;
  }

  std::vector<slang::IComponentType *> comps;
  comps.reserve(static_cast<size_t>(nMod) + 1);
  for (SlangInt i = 0; i < nMod; ++i)
  {
    if (auto *mod = m_impl->session->getLoadedModule(i))
      comps.push_back(mod);
  }
  comps.push_back(foundEP);

  BlobGuard linkDiag;
  slang::IComponentType *composite = nullptr;
  const SlangResult lr = m_impl->session->createCompositeComponentType(
      comps.data(), static_cast<SlangInt>(comps.size()), &composite,
      linkDiag.addr());
  if (SLANG_FAILED(lr) || !composite)
  {
    r.diagnostic = "SlangCompileSession: link failed: ";
    r.diagnostic += getDiag(linkDiag.get());
    return r;
  }

  constexpr SlangInt epIdx = 0;
  slang::IBlob *outCode = nullptr;
  slang::IBlob *outDiag = nullptr;
  const SlangResult cr =
      composite->getEntryPointCode(epIdx, 0, &outCode, &outDiag);
  BlobGuard cg(outCode), dg(outDiag);

  if (SLANG_FAILED(cr) || !outCode)
  {
    r.diagnostic = "SlangCompileSession: codegen failed";
    const std::string d = getDiag(outDiag);
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
  const std::string d = getDiag(outDiag);
  if (!d.empty())
    r.diagnostic = d;

  composite->release();
  return r;
}

} // namespace AkRender::ShaderSet
