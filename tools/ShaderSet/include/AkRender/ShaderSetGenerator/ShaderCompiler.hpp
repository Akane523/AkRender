#pragma once

#include <filesystem>

#include <AkRender/ShaderSetGenerator/Manifest.hpp>


namespace AkRender::ShaderSet
{
    struct CompilerEnviroment {
        std::filesystem::path work_dir;
        std::filesystem::path slang_compiler_path;
    };
};