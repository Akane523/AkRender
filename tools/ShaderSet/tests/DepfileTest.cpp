#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef TEST_MANIFEST_DEPFILE
#error "TEST_MANIFEST_DEPFILE must be defined by CMake"
#endif
#ifndef SHADER_COMPILE_MANIFEST_DEPFILE
#error "SHADER_COMPILE_MANIFEST_DEPFILE must be defined by CMake"
#endif

namespace
{

std::string strip_quotes(std::string value)
{
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

std::vector<std::string> tokenize_dep_paths(std::string_view text)
{
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;

  for (char ch : text)
  {
    if (ch == '"')
    {
      in_quotes = !in_quotes;
      current.push_back(ch);
      continue;
    }

    if (!in_quotes && std::isspace(static_cast<unsigned char>(ch)))
    {
      if (!current.empty())
      {
        tokens.push_back(strip_quotes(current));
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    tokens.push_back(strip_quotes(current));

  return tokens;
}

struct DepRule
{
  std::string target;
  std::vector<std::string> deps;
};

std::vector<DepRule> parse_depfile(const std::string &path)
{
  std::ifstream ifs(path);
  if (!ifs)
    return {};

  std::vector<DepRule> rules;
  std::string line;
  while (std::getline(ifs, line))
  {
    if (line.empty())
      continue;

    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;

    DepRule rule;
    rule.target = strip_quotes(line.substr(0, colon));
    rule.deps = tokenize_dep_paths(line.substr(colon + 1));
    rules.push_back(std::move(rule));
  }

  return rules;
}

const DepRule *find_rule(const std::vector<DepRule> &rules,
                         std::string_view suffix)
{
  for (const DepRule &rule : rules)
  {
    if (rule.target.size() >= suffix.size()
        && rule.target.compare(rule.target.size() - suffix.size(),
                               suffix.size(), suffix)
               == 0)
      return &rule;
  }
  return nullptr;
}

std::set<std::string> to_set(const std::vector<std::string> &values)
{
  return {values.begin(), values.end()};
}

TEST(ShaderSetDepfileTest, TestManifestListsBothOutputsWithMatchingDeps)
{
  const std::vector<DepRule> rules = parse_depfile(TEST_MANIFEST_DEPFILE);
  ASSERT_FALSE(rules.empty());

  const DepRule *hpp = find_rule(rules, "test_manifest.hpp");
  const DepRule *cpp = find_rule(rules, "test_manifest.cpp");
  ASSERT_NE(hpp, nullptr);
  ASSERT_NE(cpp, nullptr);
  ASSERT_FALSE(hpp->deps.empty());
  EXPECT_EQ(to_set(hpp->deps), to_set(cpp->deps));
}

TEST(ShaderSetDepfileTest, ShaderCompileManifestCppTracksSlangSources)
{
  const std::vector<DepRule> rules =
      parse_depfile(SHADER_COMPILE_MANIFEST_DEPFILE);
  ASSERT_FALSE(rules.empty());

  const DepRule *hpp = find_rule(rules, "shader_compile_manifest.hpp");
  const DepRule *cpp = find_rule(rules, "shader_compile_manifest.cpp");
  ASSERT_NE(hpp, nullptr);
  ASSERT_NE(cpp, nullptr);
  ASSERT_FALSE(hpp->deps.empty());
  EXPECT_EQ(to_set(hpp->deps), to_set(cpp->deps));

  const auto has_slash_slang = [](const std::string &path)
  { return path.find(".slang") != std::string::npos; };
  EXPECT_TRUE(std::any_of(cpp->deps.begin(), cpp->deps.end(), has_slash_slang));
  EXPECT_TRUE(std::any_of(hpp->deps.begin(), hpp->deps.end(), has_slash_slang));
}

TEST(ShaderSetDepfileTest, DependenciesAreUniquePerTarget)
{
  const std::vector<DepRule> rules =
      parse_depfile(SHADER_COMPILE_MANIFEST_DEPFILE);
  for (const DepRule &rule : rules)
  {
    const std::set<std::string> unique = to_set(rule.deps);
    EXPECT_EQ(unique.size(), rule.deps.size()) << rule.target;
  }
}

} // namespace
