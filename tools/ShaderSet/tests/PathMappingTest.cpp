#include <AkRender/ShaderSetGenerator/PathMapping.hpp>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace
{

namespace Config = AkRender::ShaderSetGenerator::Config;

TEST(PathMappingTest, ResolveRelativeSourceAgainstRoot)
{
  const auto resolved =
      Config::resolve_source_path({"shaders/vert.spv"}, "assets");
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, fs::path("assets/shaders/vert.spv"));
}

TEST(PathMappingTest, ResolveAbsoluteSourceIgnoresRoot)
{
  const fs::path absolute = fs::path("/tmp/data.bin");
  const auto resolved = Config::resolve_source_path({absolute}, "ignored");
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, absolute);
}

TEST(PathMappingTest, ResolveRelativeSourceRequiresRoot)
{
  EXPECT_FALSE(Config::resolve_source_path({"file.bin"}, "").has_value());
}

TEST(PathMappingTest, ResolveParallelVfsMapping)
{
  const Config::VfsMapping mapping{.kind = Config::VfsMapping::Kind::Parallel};
  const auto resolved = Config::resolve_vfs_path(
      mapping, "vert", {"vert.spv"}, "shaders", Config::VirtualPath{"/shaders"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/shaders/vert.spv");
}

TEST(PathMappingTest, ResolveBasenameVfsMapping)
{
  const Config::VfsMapping mapping{.kind = Config::VfsMapping::Kind::Basename};
  const auto resolved = Config::resolve_vfs_path(
      mapping, "ignored", {"pipeline/frag.spv"}, ".",
      Config::VirtualPath{"/spv"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/spv/frag.spv");
}

TEST(PathMappingTest, ResolveByNameVfsMapping)
{
  const Config::VfsMapping mapping{.kind = Config::VfsMapping::Kind::ByName};
  const auto resolved = Config::resolve_vfs_path(
      mapping, "game_config", {"data/config.bin"}, ".",
      Config::VirtualPath{"/"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/game_config");
}

TEST(PathMappingTest, ResolveExactVfsMapping)
{
  const Config::VfsMapping mapping{
      .kind = Config::VfsMapping::Kind::Exact,
      .exact_path = Config::VirtualPath{"/custom/path.bin"},
  };
  const auto resolved = Config::resolve_vfs_path(
      mapping, "name", {"any/path.bin"}, ".", Config::VirtualPath{"/unused"});
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->value, "/custom/path.bin");
}

TEST(PathMappingTest, ParallelMappingRejectsMissingPrefix)
{
  const Config::VfsMapping mapping{.kind = Config::VfsMapping::Kind::Parallel};
  EXPECT_FALSE(Config::resolve_vfs_path(mapping, "vert", {"vert.spv"}, "shaders",
                                        Config::VirtualPath{})
                  .has_value());
}

} // namespace
