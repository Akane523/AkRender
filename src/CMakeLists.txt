# ── AkRender library target ──────────────────────────────────────────────────
# Auto-discover sources; just drop .cpp / .h files into src/

file(GLOB AKRENDER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB AKRENDER_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

if(AKRENDER_SOURCES)
    add_library(akrender STATIC ${AKRENDER_SOURCES} ${AKRENDER_HEADERS})

    # The INTERFACE target AkRender carries all public dependencies
    target_link_libraries(akrender PUBLIC AkRender)

    target_include_directories(akrender PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # ── Install ──────────────────────────────────────────────────────────
    include(GNUInstallDirs)

    install(TARGETS akrender
        EXPORT AkRenderTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES ${AKRENDER_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AkRender
    )
else()
    message(STATUS "No .cpp sources in src/ — library target will be built once sources are added")
endif()
