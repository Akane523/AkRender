//===------ ManifestCompile.cpp -------------------------------------------===//

#include <AkRender/ShaderSetGenerator/ManifestCompile.hpp>

#include <AkRender/ShaderSetGenerator/Validate.hpp>
#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>
#include <AkRender/SlangJIT/SlangCompileSession.hpp>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

namespace
{

using AkRender::ShaderSet::CompileOptions;
using AkRender::ShaderSet::SlangCompileSession;
using AkRender::ShaderSet::Stage;
using AkRender::ShaderSet::TargetFormat;
using AkRender::ShaderSet::OptimizationLevel;
using AkRender::ShaderSet::FloatMode;
using AkRender::ShaderSet::MatrixLayout;

std::string stageToTemplate(Stage stage)
{
  switch (stage)
  {
    case Stage::Vertex:
      return "Vertex";
    case Stage::Fragment:
      return "Fragment";
    case Stage::Compute:
      return "Compute";
    case Stage::Geometry:
      return "Geometry";
    case Stage::TessControl:
      return "TessControl";
    case Stage::TessEval:
      return "TessEval";
    case Stage::Mesh:
      return "Mesh";
    case Stage::Task:
      return "Task";
    case Stage::RayGeneration:
      return "RayGeneration";
    case Stage::RayIntersection:
      return "RayIntersection";
    case Stage::RayAnyHit:
      return "RayAnyHit";
    case Stage::RayClosestHit:
      return "RayClosestHit";
    case Stage::RayMiss:
      return "RayMiss";
    case Stage::RayCallable:
      return "RayCallable";
    case Stage::Amplification:
      return "Amplification";
  }
  return "Compute";
}

std::string targetFormatToTemplate(TargetFormat fmt)
{
  switch (fmt)
  {
    case TargetFormat::SpirV:
      return "SpirV";
    case TargetFormat::DXIL:
      return "DXIL";
    case TargetFormat::WGSL:
      return "WGSL";
  }
  return "SpirV";
}

std::string optimizationToTemplate(OptimizationLevel level)
{
  switch (level)
  {
    case OptimizationLevel::O0:
      return "O0";
    case OptimizationLevel::O1:
      return "O1";
    case OptimizationLevel::O2:
      return "O2";
    case OptimizationLevel::O3:
      return "O3";
  }
  return "O2";
}

std::string floatModeToTemplate(FloatMode mode)
{
  switch (mode)
  {
    case FloatMode::Default:
      return "Default";
    case FloatMode::Precise:
      return "Precise";
    case FloatMode::Fast:
      return "Fast";
  }
  return "Default";
}

std::string matrixLayoutToTemplate(MatrixLayout layout)
{
  return layout == MatrixLayout::ColumnMajor ? "ColumnMajor" : "RowMajor";
}

fs::path resolvePath(const fs::path &source_dir, const fs::path &path)
{
  if (path.is_absolute())
    return path;
  return source_dir / path;
}

void addUniquePath(std::vector<fs::path> &paths, const fs::path &path)
{
  std::error_code ec;
  const fs::path normalized = fs::weakly_canonical(path, ec);
  const fs::path &stored = ec ? path : normalized;
  if (std::ranges::find(paths, stored) == paths.end())
    paths.push_back(stored);
}

std::vector<fs::path>
collectSearchPaths(const Manifest &manifest, const fs::path &source_dir)
{
  std::vector<fs::path> paths;

  for (const Config::SlangModule *mod : manifest.slang_modules())
  {
    for (const fs::path &src : mod->source_paths)
      addUniquePath(paths, resolvePath(source_dir, src).parent_path());
  }

  for (const Config::SlangShader *shader : manifest.slang_shaders())
  {
    if (!shader->source_path.empty())
      addUniquePath(paths,
                    resolvePath(source_dir, shader->source_path).parent_path());
  }

  addUniquePath(paths, source_dir);
  return paths;
}

std::string moduleIdentity(const Config::SlangModule &mod)
{
  return mod.module_name.empty() ? mod.name : mod.module_name;
}

std::string vfsPathValue(const Config::VirtualPath &path)
{
  return path.value;
}

[[nodiscard]] std::vector<std::byte>
readBinaryFile(const fs::path &path)
{
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs)
    throw std::runtime_error("cannot open file: " + path.string());

  const auto sz = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<std::byte> data(static_cast<std::size_t>(sz));
  ifs.read(reinterpret_cast<char *>(data.data()),
           static_cast<std::streamsize>(sz));
  return data;
}

void appendSegment(ShaderCodegenData &out, BlobSegment segment)
{
  out.segments.push_back(std::move(segment));
}

BlobSegment makeSegment(BlobSegmentKind kind, std::string manifest_name,
                        std::string vfs_path, std::vector<std::byte> data)
{
  return {
      .kind = kind,
      .manifest_name = std::move(manifest_name),
      .vfs_path = std::move(vfs_path),
      .data = std::move(data),
  };
}

std::vector<std::byte> spirvWordsToBytes(const std::vector<uint32_t> &words)
{
  std::vector<std::byte> bytes(words.size() * sizeof(uint32_t));
  std::memcpy(bytes.data(), words.data(), bytes.size());
  return bytes;
}

void logVerbose(bool verbose, std::string_view msg)
{
  if (verbose)
    std::cerr << msg << '\n';
}

struct CompiledModuleIR
{
  std::string import_name;
  std::vector<std::byte> ir;
};

} // namespace

ShaderCodegenData compile_manifest_shaders(const Manifest &manifest,
                                           const fs::path &source_dir,
                                           bool verbose)
{
  ShaderCodegenData out;

  const auto modules = manifest.slang_modules();
  const auto slang_shaders = manifest.slang_shaders();
  const auto spirv_shaders = manifest.spirv_shaders();

  if (modules.empty() && slang_shaders.empty() && spirv_shaders.empty())
    return out;

  const std::vector<fs::path> search_paths =
      collectSearchPaths(manifest, source_dir);

  SlangCompileSession session;
  std::unordered_map<std::string, CompiledModuleIR> compiled_modules;

  for (const Config::SlangModule *mod : modules)
  {
    if (mod->source_paths.empty())
    {
      throw std::runtime_error("slang module \"" + mod->name +
                               "\" has no source paths");
    }

    const fs::path primary_source =
        resolvePath(source_dir, mod->source_paths.front());
    const std::string identity = moduleIdentity(*mod);

    session.destroySession();
    if (!session.createSession(search_paths))
      throw std::runtime_error("failed to create Slang session");

    for (const auto &[name, compiled] : compiled_modules)
    {
      (void)name;
      if (!session.loadModuleFromIR(compiled.import_name, compiled.ir.data(),
                                    compiled.ir.size()))
      {
        throw std::runtime_error("failed to load compiled module '" + name +
                                 "' while compiling '" + mod->name + "'");
      }
    }

    const AkRender::ShaderSet::ModuleIRResult result =
        session.compileModuleFromFile(identity, primary_source);
    for (const fs::path &dep : result.dependency_files)
      out.source_dependencies.push_back(dep);

    if (!result.success)
    {
      std::ostringstream oss;
      oss << "failed to compile slang module \"" << mod->name << '"';
      if (!result.diagnostic.empty())
        oss << ": " << result.diagnostic;
      throw std::runtime_error(oss.str());
    }

    compiled_modules.emplace(
        mod->name,
        CompiledModuleIR{.import_name = identity, .ir = result.ir});

    const std::string vfs_path = vfsPathValue(mod->ir_vfs_path);
    appendSegment(out, makeSegment(BlobSegmentKind::ModuleIR, mod->name,
                                   vfs_path, result.ir));

    out.slang_modules.push_back({
        {"name", mod->name},
        {"ident", make_cpp_identifier(mod->name)},
        {"import_name", identity},
        {"vfs_path", vfs_path},
    });

    logVerbose(verbose, "  compiled module \"" + mod->name + "\" → " +
                            vfs_path + "  (" +
                            std::to_string(result.ir.size()) + " bytes)");
  }

  for (const Config::SpirV_Shader *shader : spirv_shaders)
  {
    const fs::path abs_source = resolvePath(source_dir, shader->source_path);
    out.source_dependencies.push_back(abs_source);

    const std::string vfs_path = vfsPathValue(shader->vfs_path);
    appendSegment(out, makeSegment(BlobSegmentKind::ShaderSpirV, shader->name,
                                   vfs_path, readBinaryFile(abs_source)));

    out.spirv_shaders.push_back({
        {"name", shader->name},
        {"ident", make_cpp_identifier(shader->name)},
        {"entry_point", shader->entry_point},
        {"stage", stageToTemplate(shader->stage)},
        {"vfs_path", vfs_path},
        {"from_slang_shader", false},
    });
  }

  for (const Config::SlangShader *shader : slang_shaders)
  {
    const fs::path abs_source = resolvePath(source_dir, shader->source_path);
    out.source_dependencies.push_back(abs_source);

    const bool want_ir =
        shader->mode == Config::SlangOutputMode::SlangIR ||
        shader->mode == Config::SlangOutputMode::Both;
    const bool want_spv =
        shader->mode == Config::SlangOutputMode::SpirV ||
        shader->mode == Config::SlangOutputMode::Both;

    session.destroySession();
    if (!session.createSession(search_paths, {}, shader->options))
    {
      throw std::runtime_error("failed to create Slang session for shader '" +
                               shader->name + "'");
    }

    for (const std::string &dep_name : shader->dependencies)
    {
      const auto it = compiled_modules.find(dep_name);
      if (it == compiled_modules.end())
      {
        throw std::runtime_error("slang shader \"" + shader->name +
                                 "\" depends on unknown module '" + dep_name +
                                 "'");
      }

      if (!session.loadModuleFromIR(it->second.import_name,
                                    it->second.ir.data(),
                                    it->second.ir.size()))
      {
        throw std::runtime_error("failed to load dependency module '" +
                                 dep_name + "' for shader '" + shader->name +
                                 "'");
      }
    }

    if (want_ir)
    {
      const std::string shader_module_name = "__shader_" + shader->name;
      const AkRender::ShaderSet::ModuleIRResult ir_result =
          session.compileModuleFromFile(shader_module_name, abs_source);
      for (const fs::path &dep : ir_result.dependency_files)
        out.source_dependencies.push_back(dep);

      if (!ir_result.success)
      {
        std::ostringstream oss;
        oss << "failed to compile slang shader IR \"" << shader->name << '"';
        if (!ir_result.diagnostic.empty())
          oss << ": " << ir_result.diagnostic;
        throw std::runtime_error(oss.str());
      }

      const std::string ir_vfs = vfsPathValue(shader->ir_vfs_path);
      appendSegment(out, makeSegment(BlobSegmentKind::ShaderIR, shader->name,
                                     ir_vfs, ir_result.ir));

      logVerbose(verbose, "  compiled shader IR \"" + shader->name + "\" → " +
                              ir_vfs + "  (" +
                              std::to_string(ir_result.ir.size()) + " bytes)");
    }

    std::string spirv_vfs;
    if (want_spv)
    {
      if (!want_ir)
      {
        const std::string shader_module_name = "__shader_" + shader->name;
        const AkRender::ShaderSet::ModuleIRResult load_result =
            session.compileModuleFromFile(shader_module_name, abs_source);
        for (const fs::path &dep : load_result.dependency_files)
          out.source_dependencies.push_back(dep);

        if (!load_result.success)
        {
          std::ostringstream oss;
          oss << "failed to compile slang shader \"" << shader->name << '"';
          if (!load_result.diagnostic.empty())
            oss << ": " << load_result.diagnostic;
          throw std::runtime_error(oss.str());
        }
      }

      const AkRender::ShaderSet::CompileResult spv_result =
          session.compileEntryPoint(shader->entry_point, shader->stage);
      if (!spv_result.success)
      {
        std::ostringstream oss;
        oss << "failed to generate SPIR-V for \"" << shader->name << '"';
        if (!spv_result.diagnostic.empty())
          oss << ": " << spv_result.diagnostic;
        throw std::runtime_error(oss.str());
      }

      spirv_vfs = vfsPathValue(shader->spv_vfs_path);
      appendSegment(out,
                    makeSegment(BlobSegmentKind::ShaderSpirV, shader->name,
                                spirv_vfs, spirvWordsToBytes(spv_result.binary)));

      if (shader->mode == Config::SlangOutputMode::SpirV ||
          shader->mode == Config::SlangOutputMode::Both)
      {
        out.spirv_shaders.push_back({
            {"name", shader->name},
            {"ident", make_cpp_identifier(shader->name)},
            {"entry_point", shader->entry_point},
            {"stage", stageToTemplate(shader->stage)},
            {"vfs_path", spirv_vfs},
            {"from_slang_shader",
             shader->mode == Config::SlangOutputMode::Both},
        });
      }

      logVerbose(verbose, "  compiled shader SPIR-V \"" + shader->name +
                              "\" → " + spirv_vfs);
    }

    if (want_ir)
    {
      inja::json dep_modules = inja::json::array();
      for (const std::string &dep_name : shader->dependencies)
        dep_modules.push_back(make_cpp_identifier(dep_name));

      const std::string ir_vfs = vfsPathValue(shader->ir_vfs_path);

      out.slang_shaders.push_back({
          {"name", shader->name},
          {"ident", make_cpp_identifier(shader->name)},
          {"entry_point", shader->entry_point},
          {"stage", stageToTemplate(shader->stage)},
          {"vfs_path", ir_vfs},
          {"dep_idents", dep_modules},
          {"dep_count", dep_modules.size()},
          {"target_format", targetFormatToTemplate(shader->options.target_format)},
          {"optimization",
           optimizationToTemplate(shader->options.optimization)},
          {"float_mode", floatModeToTemplate(shader->options.float_mode)},
          {"matrix_layout",
           matrixLayoutToTemplate(shader->options.matrix_layout)},
          {"debug_info", shader->options.debug_info},
          {"has_spirv", shader->mode == Config::SlangOutputMode::Both},
          {"spirv_vfs_path", spirv_vfs},
      });
    }
  }

  return out;
}

} // namespace AkRender::ShaderSetGenerator
