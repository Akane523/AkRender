#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>
#include <AkRender/ShaderSetGenerator/Validate.hpp>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace
{

using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::ValidateOptions;
using AkRender::ShaderSetGenerator::ValidationError;
using AkRender::ShaderSetGenerator::validate;
using namespace AkRender::ShaderSetGenerator;
namespace Config = AkRender::ShaderSetGenerator::Config;
using AkRender::ShaderSet::Stage;

ValidateOptions opts_for(const fs::path &dir)
{
  return ValidateOptions{
      .manifest_dir = dir,
      .require_embedded_resources = true,
      .check_sources = true,
  };
}

ValidateOptions opts_no_source_check(const fs::path &dir)
{
  return ValidateOptions{
      .manifest_dir = dir,
      .require_embedded_resources = true,
      .check_sources = false,
  };
}

bool has_error(const std::vector<ValidationError> &errors,
               std::string_view resource, std::string_view message)
{
  for (const ValidationError &error : errors)
  {
    if (error.resource == resource && error.message == message)
      return true;
  }
  return false;
}

// --- binary resources ------------------------------------------------------

TEST(ValidateTest, AcceptsValidManifest)
{
  Manifest manifest;
  open(manifest) | file_at("example", {"binary-resource.txt"},
                             Config::VirtualPath{"/example"}) | register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  EXPECT_TRUE(errors.empty());
}

TEST(ValidateTest, RejectsEmptyManifest)
{
  const Manifest manifest;
  const auto errors = validate(manifest, opts_for("."));
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors.front().message, "manifest declares no embeddable resources");
}

TEST(ValidateTest, RejectsDuplicateResourceName)
{
  Manifest manifest;
  open(manifest) | file_at("dup", {"binary-resource.txt"},
                             Config::VirtualPath{"/one"}) | register_all();
  open(manifest) | file_at("dup", {"binary-resource.txt"},
                             Config::VirtualPath{"/two"}) | register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(errors.front().resource, "dup");
  EXPECT_EQ(errors.front().message, "duplicate resource name");
}

TEST(ValidateTest, RejectsDuplicateVirtualPath)
{
  Manifest manifest;
  open(manifest) | file_at("first", {"binary-resource.txt"},
                             Config::VirtualPath{"/same"}) | register_all();
  open(manifest) | file_at("second", {"binary-resource.txt"},
                             Config::VirtualPath{"/same"}) | register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(errors.front().message, "duplicate virtual path '/same'");
}

TEST(ValidateTest, RejectsMissingSourceFile)
{
  Manifest manifest;
  open(manifest) | file_at("missing", {"does-not-exist.bin"},
                             Config::VirtualPath{"/missing"}) | register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(errors.front().resource, "missing");
  EXPECT_TRUE(errors.front().message.starts_with("missing source file:"));
}

TEST(ValidateTest, RejectsUnconfiguredBinaryResource)
{
  Manifest manifest;
  manifest.add_binary_resource("raw");

  const auto errors = validate(
      manifest, ValidateOptions{.manifest_dir = ".",
                                .require_embedded_resources = true,
                                .check_sources = false});
  ASSERT_EQ(errors.size(), 2u);
  EXPECT_TRUE(has_error(errors, "raw", "source path is empty"));
  EXPECT_TRUE(has_error(errors, "raw", "virtual path is empty"));
}

TEST(ValidateTest, RejectsVirtualPathPrefixConflict)
{
  Manifest manifest;
  open(manifest) | file_at("dir", {"binary-resource.txt"},
                             Config::VirtualPath{"/assets"}) | register_all();
  open(manifest) | file_at("file", {"binary-resource.txt"},
                             Config::VirtualPath{"/assets/data.bin"}) |
      register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(errors.front().message.find("virtual path conflict") !=
              std::string::npos);
}

TEST(ValidateTest, RejectsSanitizedNameCollision)
{
  Manifest manifest;
  open(manifest) | file_at("foo-bar", {"binary-resource.txt"},
                             Config::VirtualPath{"/one"}) | register_all();
  open(manifest) | file_at("foo_bar", {"binary-resource.txt"},
                             Config::VirtualPath{"/two"}) | register_all();

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(errors.front().message.find("identifier sanitization") !=
              std::string::npos);
}

// --- shader entries --------------------------------------------------------

TEST(ValidateTest, AcceptsRegisteredSlangPipeline)
{
  Manifest manifest;
  open(manifest) | module("math", {"math.slang"}, "math_utils") |
      ir("vert", {"vert.slang"}, "vsMain", Stage::Vertex, {"math"}) |
      register_all();

  const auto errors = validate(manifest, opts_no_source_check("."));
  EXPECT_TRUE(errors.empty());
}

TEST(ValidateTest, RejectsSlangShaderWithUnknownDependency)
{
  Manifest manifest;
  EXPECT_THROW(
      open(manifest) |
          ir("vert", {"vert.slang"}, "vsMain", Stage::Vertex, {"missing"}) |
          register_all(),
      std::invalid_argument);
}

TEST(ValidateTest, RejectsSlangModuleWithoutSources)
{
  Manifest manifest;
  auto *mod = manifest.add_slang_module("empty");
  mod->ir_vfs_path = {"/shaders/slang/empty.slang-module"};

  const auto errors = validate(manifest, opts_no_source_check("."));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(has_error(errors, "empty", "slang module has no source paths"));
}

TEST(ValidateTest, RejectsBothModeWithMissingSpvPath)
{
  Manifest manifest;
  auto *shader = manifest.add_slang_shader("broken");
  shader->source_path = "broken.slang";
  shader->stage = Stage::Fragment;
  shader->mode = Config::SlangOutputMode::Both;
  shader->ir_vfs_path = {"/shaders/slang/broken.slang-module"};

  const auto errors = validate(manifest, opts_no_source_check("."));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(has_error(errors, "broken", "virtual path is empty"));
}

TEST(ValidateTest, RejectsDuplicateShaderArtifactPaths)
{
  Manifest manifest;

  auto *a = manifest.add_slang_shader("a");
  a->source_path = "a.slang";
  a->stage = Stage::Vertex;
  a->mode = Config::SlangOutputMode::SlangIR;
  a->ir_vfs_path = {"/shaders/slang/shared.slang-module"};

  auto *b = manifest.add_slang_shader("b");
  b->source_path = "b.slang";
  b->stage = Stage::Fragment;
  b->mode = Config::SlangOutputMode::SlangIR;
  b->ir_vfs_path = {"/shaders/slang/shared.slang-module"};

  const auto errors = validate(manifest, opts_no_source_check("."));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(errors.front().message.find("duplicate virtual path") !=
              std::string::npos);
}

} // namespace
