// Include once at the end of manifest.cpp, after make_manifest() is defined.
namespace AkRender::ShaderSetGenerator::detail {

struct ManifestEntryRegistration
{
  explicit ManifestEntryRegistration(ManifestBuilderFn builder)
  {
    register_manifest_builder(builder);
  }
};

Manifest invoke_user_manifest()
{
  return make_manifest();
}

[[maybe_unused]] const ManifestEntryRegistration
    _akrender_shader_set_manifest_registration{invoke_user_manifest};

} // namespace AkRender::ShaderSetGenerator::detail
