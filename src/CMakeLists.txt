add_subdirectory(Core)

if(TARGET AkRender)
    target_sources(AkRender PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Render.cpp"
    )

    # Internal include path — source files find each other here (PRIVATE, not exported)
    target_include_directories(AkRender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
