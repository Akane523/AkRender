#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <gtest/gtest.h>

namespace
{

using AkRender::ShaderSetGenerator::Config::normalize_vfs_path;
using AkRender::ShaderSetGenerator::Config::VirtualPath;
using AkRender::ShaderSetGenerator::Config::vfs_join;

TEST(VirtualPathTest, NormalizeAbsoluteFilePath)
{
  const auto path = normalize_vfs_path("/shaders/vert.spv");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path->value, "/shaders/vert.spv");
}

TEST(VirtualPathTest, NormalizeCollapsesRepeatedSlashes)
{
  const auto path = normalize_vfs_path("//shaders//vert.spv");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path->value, "/shaders/vert.spv");
}

TEST(VirtualPathTest, NormalizeAddsLeadingSlash)
{
  const auto path = normalize_vfs_path("shaders/vert.spv");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path->value, "/shaders/vert.spv");
}

TEST(VirtualPathTest, RejectsParentComponents)
{
  EXPECT_FALSE(normalize_vfs_path("/shaders/../vert.spv").has_value());
}

TEST(VirtualPathTest, RejectsTrailingSlash)
{
  EXPECT_FALSE(normalize_vfs_path("/shaders/").has_value());
}

TEST(VirtualPathTest, JoinPrefixAndLeaf)
{
  const VirtualPath prefix{"/shaders"};
  const auto joined = vfs_join(prefix, "vert.spv");
  ASSERT_TRUE(joined.has_value());
  EXPECT_EQ(joined->value, "/shaders/vert.spv");
}

TEST(VirtualPathTest, JoinNestedLeaf)
{
  const VirtualPath prefix{"/shaders"};
  const auto joined = vfs_join(prefix, "cs/main.cs.spv");
  ASSERT_TRUE(joined.has_value());
  EXPECT_EQ(joined->value, "/shaders/cs/main.cs.spv");
}

TEST(VirtualPathTest, FromNameCreatesRootFile)
{
  const auto path = VirtualPath::from_name("config");
  EXPECT_EQ(path.value, "/config");
}

TEST(VirtualPathTest, RejectsBackslashes)
{
  EXPECT_FALSE(normalize_vfs_path("\\shaders\\vert.spv").has_value());
}

TEST(VirtualPathTest, RejectsEmptyPath)
{
  EXPECT_FALSE(normalize_vfs_path("").has_value());
}

TEST(VirtualPathTest, JoinRejectsAbsoluteLeaf)
{
  const VirtualPath prefix{"/shaders"};
  EXPECT_FALSE(vfs_join(prefix, "/vert.spv").has_value());
}

TEST(VirtualPathTest, JoinRejectsEmptyLeaf)
{
  const VirtualPath prefix{"/shaders"};
  EXPECT_FALSE(vfs_join(prefix, "").has_value());
}

} // namespace
