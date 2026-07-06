#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <gtest/gtest.h>

namespace
{

using AkRender::ShaderSetGenerator::Manifest;
namespace Config = AkRender::ShaderSetGenerator::Config;
using AkRender::ShaderSet::Stage;

TEST(ManifestTest, AddAndFindBinaryResource)
{
  Manifest manifest;

  auto *res = manifest.add_binary_resource("texture");
  ASSERT_NE(res, nullptr);
  res->source_path = {"tex.png"};

  EXPECT_EQ(manifest.num_binary_resources(), 1u);
  EXPECT_NE(manifest.find_binary_resource("texture"), nullptr);
  EXPECT_EQ(manifest.find_binary_resource("texture")->source_path.path,
            "tex.png");
  EXPECT_EQ(manifest.find_binary_resource("missing"), nullptr);
}

TEST(ManifestTest, AddAndFindSpirVShader)
{
  Manifest manifest;

  auto *shader = manifest.add_spirv_shader("vert");
  ASSERT_NE(shader, nullptr);
  shader->source_path = "vert.spv";
  shader->entry_point = "mainVS";

  EXPECT_EQ(manifest.num_spirv_shaders(), 1u);
  EXPECT_NE(manifest.find_spirv_shader("vert"), nullptr);
  EXPECT_EQ(manifest.find_spirv_shader("vert")->entry_point, "mainVS");
}

TEST(ManifestTest, AddAndFindSlangModule)
{
  Manifest manifest;

  auto *mod = manifest.add_slang_module("math");
  ASSERT_NE(mod, nullptr);
  mod->source_paths = {"math.slang"};
  mod->module_name = "math_utils";

  EXPECT_EQ(manifest.num_slang_modules(), 1u);
  EXPECT_NE(manifest.find_slang_module("math"), nullptr);
  EXPECT_EQ(manifest.find_slang_module("math")->module_name, "math_utils");
}

TEST(ManifestTest, AddAndFindSlangShader)
{
  Manifest manifest;

  auto *shader = manifest.add_slang_shader("frag");
  ASSERT_NE(shader, nullptr);
  shader->source_path = "frag.slang";
  shader->stage = Stage::Fragment;
  shader->mode = Config::SlangOutputMode::SpirV;
  shader->dependencies = {"math"};

  EXPECT_EQ(manifest.num_slang_shaders(), 1u);
  EXPECT_NE(manifest.find_slang_shader("frag"), nullptr);
  EXPECT_EQ(manifest.find_slang_shader("frag")->mode,
            Config::SlangOutputMode::SpirV);
  ASSERT_EQ(manifest.find_slang_shader("frag")->dependencies.size(), 1u);
  EXPECT_EQ(manifest.find_slang_shader("frag")->dependencies.front(), "math");
}

TEST(ManifestTest, PointerStabilityAcrossAdditions)
{
  Manifest manifest;

  auto *first = manifest.add_binary_resource("first");
  auto *second = manifest.add_spirv_shader("second");

  manifest.add_slang_module("third");

  EXPECT_EQ(manifest.find_binary_resource("first"), first);
  EXPECT_EQ(manifest.find_spirv_shader("second"), second);
}

TEST(ManifestTest, ListAllEntries)
{
  Manifest manifest;

  manifest.add_binary_resource("a");
  manifest.add_spirv_shader("b");
  manifest.add_slang_module("c");
  manifest.add_slang_shader("d");

  EXPECT_EQ(manifest.binary_resources().size(), 1u);
  EXPECT_EQ(manifest.spirv_shaders().size(), 1u);
  EXPECT_EQ(manifest.slang_modules().size(), 1u);
  EXPECT_EQ(manifest.slang_shaders().size(), 1u);
}

TEST(ManifestTest, EmbedAtNormalizesVirtualPath)
{
  Manifest manifest;

  const auto *resource =
      manifest.embed_at("data", {"file.bin"}, Config::VirtualPath{"/assets//data.bin"});
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path, "file.bin");
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/assets/data.bin");
}

TEST(ManifestTest, EmbedAtRejectsInvalidVirtualPath)
{
  Manifest manifest;

  EXPECT_THROW(
      manifest.embed_at("data", {"file.bin"}, Config::VirtualPath{"/assets/"}),
      std::invalid_argument);
}

TEST(ManifestTest, VfsPrefixScopeRestoresPreviousPrefix)
{
  Manifest manifest;
  manifest.set_vfs_prefix({"/default"});

  {
    const auto scope = manifest.push_vfs_prefix({"/scoped"});
    (void)scope;
    EXPECT_EQ(manifest.vfs_prefix().value, "/scoped");
  }

  EXPECT_EQ(manifest.vfs_prefix().value, "/default");
}

TEST(ManifestTest, SourceRootScopeRestoresPreviousRoot)
{
  Manifest manifest;
  manifest.set_source_root("base");

  {
    const auto scope = manifest.push_source_root("nested");
    (void)scope;
    EXPECT_EQ(manifest.source_root(), "nested");
  }

  EXPECT_EQ(manifest.source_root(), "base");
}

} // namespace
