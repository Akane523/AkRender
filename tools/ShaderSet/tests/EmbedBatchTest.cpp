#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

using AkRender::ShaderSetGenerator::EmbedBatch;
using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::TreeNamePolicy;
namespace Config = AkRender::ShaderSetGenerator::Config;

TEST(EmbedBatchTest, ParallelMappingStoresComposedPaths)
{
  Manifest manifest;

  manifest.embed_parallel({"/shaders"}, "shaders", {
      {"vert", {"vert.spv"}},
      {"frag", {"frag.spv"}},
  });

  ASSERT_EQ(manifest.num_binary_resources(), 2u);

  const auto *vert = manifest.find_binary_resource("vert");
  ASSERT_NE(vert, nullptr);
  EXPECT_EQ(vert->source_path.path, fs::path("shaders/vert.spv"));
  EXPECT_EQ(std::get<Config::Embed>(vert->seek_type).virtual_path.value,
            "/shaders/vert.spv");

  const auto *frag = manifest.find_binary_resource("frag");
  ASSERT_NE(frag, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(frag->seek_type).virtual_path.value,
            "/shaders/frag.spv");
}

TEST(EmbedBatchTest, ByNameMappingUsesResourceName)
{
  Manifest manifest;

  EmbedBatch(manifest)
      .vfs_prefix({"/"})
      .source_root(".")
      .map_by_name()
      .file("config", {"data/config.bin"});

  const auto *resource = manifest.find_binary_resource("config");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/config");
}

TEST(EmbedBatchTest, FileAtUsesExactVirtualPath)
{
  Manifest manifest;

  EmbedBatch(manifest)
      .source_root(".")
      .map_parallel()
      .file_at("legacy", {"old/legacy.bin"}, {"/compat/legacy.bin"});

  const auto *resource = manifest.find_binary_resource("legacy");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/compat/legacy.bin");
}

TEST(EmbedBatchTest, SourceRootScopeComposesPhysicalPaths)
{
  Manifest manifest;

  {
    auto scope = manifest.push_source_root("assets");
    EmbedBatch(manifest)
        .source_root("textures")
        .vfs_prefix({"/textures"})
        .map_parallel()
        .file("albedo", {"albedo.png"});
  }

  const auto *resource = manifest.find_binary_resource("albedo");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path, fs::path("assets/textures/albedo.png"));
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/textures/albedo.png");
}

TEST(EmbedBatchTest, EmbedTreeUsesRelativePathNames)
{
  Manifest manifest;

  const fs::path tree_dir =
      fs::path(__FILE__).parent_path() / "EmbedTreeExample/tree";
  manifest.embed_tree(tree_dir, {"/tree"}, TreeNamePolicy::RelativePath);

  ASSERT_EQ(manifest.num_binary_resources(), 2u);

  const auto *nested = manifest.find_binary_resource("nested_leaf.txt");
  ASSERT_NE(nested, nullptr);
  EXPECT_TRUE(nested->source_path.path.generic_string().ends_with(
      "tree/nested/leaf.txt"));
  EXPECT_EQ(std::get<Config::Embed>(nested->seek_type).virtual_path.value,
            "/tree/nested/leaf.txt");

  const auto *root = manifest.find_binary_resource("root.txt");
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(root->seek_type).virtual_path.value,
            "/tree/root.txt");
}

TEST(EmbedBatchTest, MissingMappingIsRejected)
{
  Manifest manifest;

  EXPECT_THROW(
      {
        EmbedBatch(manifest)
            .source_root(".")
            .vfs_prefix({"/data"})
            .file("config", {"config.bin"});
      },
      std::invalid_argument);
}

TEST(EmbedBatchTest, BasenameMappingUsesSourceFilename)
{
  Manifest manifest;

  EmbedBatch(manifest)
      .source_root("generated")
      .vfs_prefix({"/spv"})
      .map_basename()
      .file("frag", {"pipeline/frag.spv"});

  const auto *resource = manifest.find_binary_resource("frag");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->source_path.path, fs::path("generated/pipeline/frag.spv"));
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/spv/frag.spv");
}

TEST(EmbedBatchTest, EmbedBatchInheritsManifestVfsPrefix)
{
  Manifest manifest;
  manifest.set_vfs_prefix({"/pack"});

  EmbedBatch(manifest)
      .source_root("data")
      .map_by_name()
      .file("cfg", {"cfg.bin"});

  const auto *resource = manifest.find_binary_resource("cfg");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(resource->seek_type).virtual_path.value,
            "/pack/cfg");
}

TEST(EmbedBatchTest, EmbedTreeStemPolicyUsesFilenameStem)
{
  Manifest manifest;

  const fs::path tree_dir =
      fs::path(__FILE__).parent_path() / "EmbedTreeExample/tree";
  manifest.embed_tree(tree_dir, {"/tree"}, TreeNamePolicy::Stem);

  const auto *root = manifest.find_binary_resource("root");
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(root->seek_type).virtual_path.value,
            "/tree/root.txt");

  const auto *leaf = manifest.find_binary_resource("leaf");
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(std::get<Config::Embed>(leaf->seek_type).virtual_path.value,
            "/tree/nested/leaf.txt");
}

TEST(EmbedBatchTest, EmbedTreeStemPolicyRejectsDuplicateNames)
{
  Manifest manifest;

  const fs::path tree_dir =
      fs::path(__FILE__).parent_path() / "EmbedTreeExample/stem_dup";
  EXPECT_THROW(manifest.embed_tree(tree_dir, {"/dup"}, TreeNamePolicy::Stem),
               std::invalid_argument);
}

} // namespace
