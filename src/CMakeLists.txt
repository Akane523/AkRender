# ── AkRender library sources ───────────────────────────────────────────────────
# Sources are added to the top-level AkRender target via target_sources.
# No file(GLOB) — new sources must be listed explicitly for correct rebuilds.

target_sources(AkRender PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Render.cpp"
)

# Internal include path — source files find each other here (PRIVATE, not exported)
target_include_directories(AkRender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
