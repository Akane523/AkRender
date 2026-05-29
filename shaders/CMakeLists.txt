if(AKRENDER_PRECOMPILE_SHADERS)
    include(SlangShader)

    # -----------------------------------------------------------------------
    # Triangle shaders
    # -----------------------------------------------------------------------
    set(TRIANGLE_SHADER_SOURCES
        triangle.vert.slang
        triangle.frag.slang
    )

    # -----------------------------------------------------------------------
    # PBR shaders
    # -----------------------------------------------------------------------
    set(PBR_SHADER_SOURCES
        pbr.vert.slang
        pbr.frag.slang
    )

    # Collect all shaders into one cmakerc resource library attached to
    # AkRenderCore.  The SlangShader helper compiles each .slang to .spv
    # and embeds the results using cmakerc under the "shaders" namespace.
    akrender_add_slang_shaders(AkRenderCore
        NAMESPACE shaders
        SOURCES
            ${TRIANGLE_SHADER_SOURCES}
            ${PBR_SHADER_SOURCES}
    )
endif()
