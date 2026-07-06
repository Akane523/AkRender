#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <gtest/gtest.h>

namespace
{

using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::VfsPlacement;
using namespace AkRender::ShaderSetGenerator;
namespace Config = AkRender::ShaderSetGenerator::Config;

TEST(VfsPlacementTest, DefaultsInheritManifestRoots)
{
  Manifest manifest;
  manifest.set_source_root("assets");
  manifest.set_vfs_prefix({"/pack"});

  const VfsPlacement placement = VfsPlacement::defaults(manifest);
  EXPECT_EQ(placement.source_root, "assets");
  EXPECT_EQ(placement.vfs_prefix.value, "/pack");
  EXPECT_FALSE(placement.mapping_set);
}

TEST(VfsPlacementTest, ResolveRequiresMapping)
{
  const VfsPlacement placement;
  const auto result = placement.resolve("cfg", Config::SourcePath{"cfg.bin"});
  ASSERT_FALSE(result.has_value());
  EXPECT_TRUE(result.error().find("mapping not selected") != std::string::npos);
}

TEST(VfsPlacementTest, PipelineAdjustsLayout)
{
  Manifest manifest;
  VfsPlacement placement = map_parallel(with_vfs_prefix(
      with_source_root(VfsPlacement::defaults(manifest), "src"), {"/data"}));

  EXPECT_EQ(placement.source_root, "src");
  EXPECT_EQ(placement.vfs_prefix.value, "/data");
  ASSERT_TRUE(placement.mapping_set);
  EXPECT_EQ(placement.mapping.kind, Config::VfsMapping::Kind::Parallel);

  const auto resolved =
      placement.resolve("tex", Config::SourcePath{"images/tex.png"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/data/images/tex.png");
}

TEST(VfsPlacementTest, MapExactUsesFixedPath)
{
  const VfsPlacement placement = map_exact(VfsPlacement{}, {"/fixed/path.bin"});

  const auto resolved =
      placement.resolve("ignored", Config::SourcePath{"any.bin"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/fixed/path.bin");
}

} // namespace
