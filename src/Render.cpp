#include <AkRender/Render.hpp>

namespace AkRender {

// ── Private implementation ───────────────────────────────────────────────────
class RenderImpl {
    // TODO: Vulkan device, swapchain, etc.
};

// ── Pimpl forwarding ─────────────────────────────────────────────────────────
Render::Render()  : impl(new RenderImpl) {}
Render::~Render() { delete impl; }

} // namespace AkRender
