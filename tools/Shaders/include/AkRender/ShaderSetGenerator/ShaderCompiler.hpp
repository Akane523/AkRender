#pragma once

#include <filesystem>

#include <AkRender/ShaderSetGenerator/Manifest.hpp>


namespace AkRender::Shaders
{
    struct CompilerEnviroment {
        std::filesystem::path work_dir;
        std::filesystem::path slang_compiler_path;
    };
};