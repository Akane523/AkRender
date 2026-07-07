# ── AkRender::Core sources ───────────────────────────────────────────────────
# Sources are added to the top-level AkRenderCore target via target_sources.
# No file(GLOB) — new sources must be listed explicitly for correct rebuilds.

target_sources(AkRenderCore PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core.cpp"
)
