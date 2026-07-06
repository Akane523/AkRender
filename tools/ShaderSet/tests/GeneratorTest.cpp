#include <AkRender/ShaderSet/VirtualFileSystem.hpp>

#include <gtest/gtest.h>

#include <string_view>

#include "test_manifest.hpp"

namespace
{

using namespace AkRender::ShaderSet;

TEST(ShaderSetGeneratorTest, GeneratedVfsLookup)
{
  EXPECT_NE(test_manifest::test_manifest_fs.lookup("/example_data"), nullptr);
  EXPECT_TRUE(
      test_manifest::test_manifest_fs.lookup("/example_data")->is_file());
  EXPECT_EQ(test_manifest::test_manifest_fs.lookup("/missing"), nullptr);
}

TEST(ShaderSetGeneratorTest, GeneratedRecordMetadata)
{
  const Record rec = test_manifest::test_manifest_fs.record("/example_data");
  ASSERT_FALSE(rec.empty());
  EXPECT_EQ(rec.offset, 0u);
  EXPECT_EQ(rec.size, 21u);
}

TEST(ShaderSetGeneratorTest, ReadEmbeddedBinaryResource)
{
  const auto data =
      test_manifest::test_manifest_view.read("/example_data");
  ASSERT_EQ(data.size(), 21u);

  const std::string_view content(
      reinterpret_cast<const char *>(data.data()), data.size());
  EXPECT_EQ(content, "Hello Binary Resource");
}

TEST(ShaderSetGeneratorTest, ViewDelegatesToFilesystem)
{
  EXPECT_EQ(test_manifest::test_manifest_view.lookup("/example_data"),
            test_manifest::test_manifest_fs.lookup("/example_data"));
  EXPECT_EQ(test_manifest::test_manifest_view.record("/example_data").size,
            21u);
  EXPECT_TRUE(test_manifest::test_manifest_view.read("/missing").empty());
}

TEST(ShaderSetGeneratorTest, TypedBinaryResourceLookup)
{
  const BinaryResourceDesc *desc =
      test_manifest::find_binary_resource("example_data");
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->manifest_name, "example_data");
  EXPECT_EQ(desc->vfs_path, "/example_data");
  EXPECT_EQ(desc->data.offset, 0u);
  EXPECT_EQ(desc->data.size, 21u);
}

TEST(ShaderSetGeneratorTest, TypedResourceConstants)
{
  const BinaryResourceDesc &desc = test_manifest::resources::example_data;
  EXPECT_EQ(desc.manifest_name, "example_data");
  EXPECT_EQ(desc.vfs_path, "/example_data");
  EXPECT_EQ(desc.data.offset, 0u);
  EXPECT_EQ(desc.data.size, 21u);
  EXPECT_EQ(test_manifest::find_binary_resource("missing"), nullptr);
}

} // namespace
