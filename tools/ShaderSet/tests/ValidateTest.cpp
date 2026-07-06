#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/Validate.hpp>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace
{

using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::ValidateOptions;
using AkRender::ShaderSetGenerator::validate;
namespace Config = AkRender::ShaderSetGenerator::Config;

ValidateOptions opts_for(const fs::path &dir)
{
  return ValidateOptions{
      .manifest_dir = dir,
      .require_embedded_resources = true,
      .check_sources = true,
  };
}

TEST(ValidateTest, AcceptsValidManifest)
{
  Manifest manifest;
  manifest.embed_at("example", {"binary-resource.txt"},
                      Config::VirtualPath{"/example"});

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
  EXPECT_EQ(errors.front().message, "no embedded binary resources in manifest");
}

TEST(ValidateTest, RejectsDuplicateResourceName)
{
  Manifest manifest;
  manifest.embed_at("dup", {"binary-resource.txt"},
                    Config::VirtualPath{"/one"});
  manifest.embed_at("dup", {"binary-resource.txt"},
                    Config::VirtualPath{"/two"});

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
  manifest.embed_at("first", {"binary-resource.txt"},
                    Config::VirtualPath{"/same"});
  manifest.embed_at("second", {"binary-resource.txt"},
                    Config::VirtualPath{"/same"});

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(errors.front().message, "duplicate virtual path '/same'");
}

TEST(ValidateTest, RejectsMissingSourceFile)
{
  Manifest manifest;
  manifest.embed_at("missing", {"does-not-exist.bin"},
                    Config::VirtualPath{"/missing"});

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

  const auto errors =
      validate(manifest, ValidateOptions{.manifest_dir = ".",
                                         .require_embedded_resources = true,
                                         .check_sources = false});
  ASSERT_GE(errors.size(), 2u);
}

TEST(ValidateTest, RejectsVirtualPathPrefixConflict)
{
  Manifest manifest;
  manifest.embed_at("dir", {"binary-resource.txt"},
                    Config::VirtualPath{"/assets"});
  manifest.embed_at("file", {"binary-resource.txt"},
                    Config::VirtualPath{"/assets/data.bin"});

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
  manifest.embed_at("foo-bar", {"binary-resource.txt"},
                    Config::VirtualPath{"/one"});
  manifest.embed_at("foo_bar", {"binary-resource.txt"},
                    Config::VirtualPath{"/two"});

  const fs::path dir =
      fs::path(__FILE__).parent_path() / "GeneratorExample";
  const auto errors = validate(manifest, opts_for(dir));
  ASSERT_FALSE(errors.empty());
  EXPECT_TRUE(errors.front().message.find("identifier sanitization") !=
              std::string::npos);
}

} // namespace
