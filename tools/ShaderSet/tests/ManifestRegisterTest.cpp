#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

using AkRender::ShaderSetGenerator::embed_parallel;
using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::TreeNamePolicy;
using namespace AkRender::ShaderSetGenerator;
namespace Config = AkRender::ShaderSetGenerator::Config;
using AkRender::ShaderSet::Stage;

const fs::path kTreeDir = fs::path(__FILE__).parent_path() / "TreeExample/tree";
const fs::path kStemDupDir =
    fs::path(__FILE__).parent_path() / "TreeExample/stem_dup";

// --- disk embeds -----------------------------------------------------------

TEST(ManifestRegisterTest, ParallelMappingStoresComposedPaths)
{
  Manifest manifest;

  embed_parallel(manifest, {"/shaders"}, "shaders",
                 {
                     {"vert", {"vert.spv"}},
                     {"frag", {"frag.spv"}},
                 });

  ASSERT_EQ(manifest.num_binary_resources(), 2u);

  const auto *vert = manifest.find_binary_resource("vert");
  ASSERT_NE(vert, nullptr);
  EXPECT_EQ(vert->source_path.path, fs::path("shaders/vert.spv"));
  EXPECT_EQ(vert->vfs_path.value, "/shaders/vert.spv");

  const auto *frag = manifest.find_binary_resource("frag");
  ASSERT_NE(frag, nullptr);
  EXPECT_EQ(frag->vfs_path.value, "/shaders/frag.spv");
}

TEST(ManifestRegisterTest, ByNameMappingUsesResourceName)
{
  Manifest manifest;

  open(manifest) | into({"/"}) | from(".") | map_by_name()
      | file("config", {"data/config.bin"}) | register_all();

  const auto *resource = manifest.find_binary_resource("config");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->vfs_path.value, "/config");
}

TEST(ManifestRegisterTest, FileAtUsesExactVirtualPath)
{
  Manifest manifest;

  open(manifest) | from(".") | map_parallel()
      | file_at("legacy", {"old/legacy.bin"}, {"/compat/legacy.bin"})
      | register_all();

  const auto *resource = manifest.find_binary_resource("legacy");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->vfs_path.value, "/compat/legacy.bin");
}

TEST(ManifestRegisterTest, FileAtNormalizesVirtualPath)
{
  Manifest manifest;

  open(manifest)
      | file_at("data", {"file.bin"}, Config::VirtualPath{"/assets//data.bin"})
      | register_all();

  const auto *resource = manifest.find_binary_resource("data");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path, "file.bin");
  EXPECT_EQ(resource->vfs_path.value, "/assets/data.bin");
}

TEST(ManifestRegisterTest, FileAtRejectsInvalidVirtualPath)
{
  Manifest manifest;

  EXPECT_THROW(
      open(manifest)
          | file_at("data", {"file.bin"}, Config::VirtualPath{"/assets/"})
          | register_all(),
      std::invalid_argument);
}

TEST(ManifestRegisterTest, SourceRootScopeComposesPhysicalPaths)
{
  Manifest manifest;

  {
    auto scope = manifest.push_source_root("assets");
    (void)scope;
    open(manifest) | from("textures") | into({"/textures"}) | map_parallel()
        | file("albedo", {"albedo.png"}) | register_all();
  }

  const auto *resource = manifest.find_binary_resource("albedo");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path, fs::path("assets/textures/albedo.png"));
  EXPECT_EQ(resource->vfs_path.value, "/textures/albedo.png");
}

TEST(ManifestRegisterTest, TreeUsesRelativePathNames)
{
  Manifest manifest;

  open(manifest) | tree(kTreeDir, {"/tree"}, TreeNamePolicy::RelativePath)
      | register_all();

  ASSERT_EQ(manifest.num_binary_resources(), 2u);

  const auto *nested = manifest.find_binary_resource("nested_leaf.txt");
  ASSERT_NE(nested, nullptr);
  EXPECT_TRUE(nested->source_path.path.generic_string().ends_with(
      "tree/nested/leaf.txt"));
  EXPECT_EQ(nested->vfs_path.value, "/tree/nested/leaf.txt");

  const auto *root = manifest.find_binary_resource("root.txt");
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->vfs_path.value, "/tree/root.txt");
}

TEST(ManifestRegisterTest, MissingMappingIsRejected)
{
  Manifest manifest;

  EXPECT_THROW(open(manifest) | from(".") | into({"/data"})
                   | file("config", {"config.bin"}) | register_all(),
               std::invalid_argument);
}

TEST(ManifestRegisterTest, BasenameMappingUsesSourceFilename)
{
  Manifest manifest;

  open(manifest) | from("generated") | into({"/spv"}) | map_basename()
      | file("frag", {"pipeline/frag.spv"}) | register_all();

  const auto *resource = manifest.find_binary_resource("frag");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path,
            fs::path("generated/pipeline/frag.spv"));
  EXPECT_EQ(resource->vfs_path.value, "/spv/frag.spv");
}

TEST(ManifestRegisterTest, InheritsManifestVfsPrefix)
{
  Manifest manifest;
  manifest.set_vfs_prefix({"/pack"});

  open(manifest) | from("data") | map_by_name() | file("cfg", {"cfg.bin"})
      | register_all();

  const auto *resource = manifest.find_binary_resource("cfg");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->vfs_path.value, "/pack/cfg");
}

TEST(ManifestRegisterTest, TreeStemPolicyUsesFilenameStem)
{
  Manifest manifest;

  open(manifest) | tree(kTreeDir, {"/tree"}, TreeNamePolicy::Stem)
      | register_all();

  const auto *root = manifest.find_binary_resource("root");
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->vfs_path.value, "/tree/root.txt");

  const auto *leaf = manifest.find_binary_resource("leaf");
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(leaf->vfs_path.value, "/tree/nested/leaf.txt");
}

TEST(ManifestRegisterTest, TreeStemPolicyRejectsDuplicateNames)
{
  Manifest manifest;

  EXPECT_THROW(open(manifest)
                   | tree(kStemDupDir, {"/dup"}, TreeNamePolicy::Stem)
                   | register_all(),
               std::invalid_argument);
}

TEST(ManifestRegisterTest, FilesRegistersMultipleEntries)
{
  Manifest manifest;

  open(manifest) | into({"/data"}) | from(".") | map_parallel()
      | files({
          {"a", {"a.bin"}},
          {"b", {"b.bin"}},
      })
      | register_all();

  EXPECT_NE(manifest.find_binary_resource("a"), nullptr);
  EXPECT_NE(manifest.find_binary_resource("b"), nullptr);
  EXPECT_EQ(manifest.find_binary_resource("a")->vfs_path.value, "/data/a.bin");
  EXPECT_EQ(manifest.find_binary_resource("b")->vfs_path.value, "/data/b.bin");
}

// --- shader registration ---------------------------------------------------

TEST(ManifestRegisterTest, ModuleRegistersDefaultIrPath)
{
  Manifest manifest;

  open(manifest) | module("math", {"math.slang"}, "math_utils")
      | register_all();

  const auto *mod = manifest.find_slang_module("math");
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->module_name, "math_utils");
  EXPECT_EQ(mod->ir_vfs_path.value, "/shaders/slang/math_utils.slang-module");
}

TEST(ManifestRegisterTest, IrShaderRegistersDefaultIrPath)
{
  Manifest manifest;

  open(manifest) | module("math", {"math.slang"})
      | ir("vert", {"vert.slang"}, "vsMain", Stage::Vertex, {"math"})
      | register_all();

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->mode, Config::SlangOutputMode::SlangIR);
  EXPECT_EQ(shader->ir_vfs_path.value, "/shaders/slang/vert.slang-module");
  EXPECT_TRUE(shader->spv_vfs_path.empty());
  ASSERT_EQ(shader->dependencies.size(), 1u);
  EXPECT_EQ(shader->dependencies.front(), "math");
}

TEST(ManifestRegisterTest, SpirvShaderRegistersDefaultSpvPath)
{
  Manifest manifest;

  open(manifest) | module("math", {"math.slang"})
      | spirv("frag", {"frag.slang"}, "fsMain", Stage::Fragment, {"math"})
      | register_all();

  const auto *shader = manifest.find_slang_shader("frag");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->mode, Config::SlangOutputMode::SpirV);
  EXPECT_TRUE(shader->ir_vfs_path.empty());
  EXPECT_EQ(shader->spv_vfs_path.value, "/shaders/spv/frag.spv");
}

TEST(ManifestRegisterTest, BothModeRegistersIrAndSpvPaths)
{
  Manifest manifest;

  open(manifest) | both("combo", {"combo.slang"}, "main", Stage::Compute, {})
      | register_all();

  const auto *shader = manifest.find_slang_shader("combo");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->mode, Config::SlangOutputMode::Both);
  EXPECT_EQ(shader->ir_vfs_path.value, "/shaders/slang/combo.slang-module");
  EXPECT_EQ(shader->spv_vfs_path.value, "/shaders/spv/combo.spv");
}

TEST(ManifestRegisterTest, ParallelMappingAppliesToShaderArtifacts)
{
  Manifest manifest;

  open(manifest) | into({"/pack"}) | from(".") | map_parallel()
      | module("math", {"shaders/math.slang"}, "math_utils")
      | ir("vert", {"shaders/vert.slang"}, "vsMain", Stage::Vertex, {"math"})
      | register_all();

  const auto *mod = manifest.find_slang_module("math");
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->ir_vfs_path.value, "/pack/shaders/math.slang-module");

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->ir_vfs_path.value, "/pack/shaders/vert.slang-module");
}

TEST(ManifestRegisterTest, SpirvFileRegistersDiskShader)
{
  Manifest manifest;

  open(manifest) | into({"/spv"}) | from(".") | map_parallel()
      | spirv_file("prebuilt", {"pipeline/frag.spv"}, "main", Stage::Fragment)
      | register_all();

  const auto *shader = manifest.find_spirv_shader("prebuilt");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->entry_point, "main");
  EXPECT_EQ(shader->stage, Stage::Fragment);
  EXPECT_EQ(shader->vfs_path.value, "/spv/pipeline/frag.spv");
}

TEST(ManifestRegisterTest, ModuleRejectsEmptySources)
{
  Manifest manifest;

  EXPECT_THROW(open(manifest) | module("empty", {}) | register_all(),
               std::invalid_argument);
}

} // namespace
