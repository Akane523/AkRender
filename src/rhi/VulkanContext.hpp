#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

namespace AkRender {

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------
enum class VulkanError : uint8_t {
    InstanceCreationFailed,
    NoSuitablePhysicalDevice,
    DeviceCreationFailed,
    SurfaceCreationFailed,
    ValidationLayerUnavailable,
    ExtensionUnavailable,
};

[[nodiscard]] constexpr std::string_view toString(VulkanError err) noexcept {
    switch (err) {
    case VulkanError::InstanceCreationFailed:    return "Vulkan instance creation failed";
    case VulkanError::NoSuitablePhysicalDevice:  return "no suitable Vulkan physical device";
    case VulkanError::DeviceCreationFailed:      return "Vulkan device creation failed";
    case VulkanError::SurfaceCreationFailed:     return "Vulkan surface creation failed";
    case VulkanError::ValidationLayerUnavailable: return "requested validation layer is unavailable";
    case VulkanError::ExtensionUnavailable:       return "requested extension is unavailable";
    }
    return "unknown Vulkan error";
}

// ---------------------------------------------------------------------------
// Queue family indices
// ---------------------------------------------------------------------------
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;

    [[nodiscard]] bool isComplete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

// ---------------------------------------------------------------------------
// VulkanContext
//
// Owns the Vulkan instance, debug messenger, physical device, logical device,
// and per-family queue handles.  Designed to be created once and shared.
// ---------------------------------------------------------------------------
class VulkanContext {
public:
    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    struct Config {
        std::string_view appName        = "AkRender";
        uint32_t         appVersion     = VK_MAKE_API_VERSION(0, 0, 1, 0);
        bool             enableValidation = true;

        /// Window surface needed for queue-family selection and swapchain.
        /// Pass VK_NULL_HANDLE if running headless / compute-only.
        VkSurfaceKHR     surface        = VK_NULL_HANDLE;

        /// Extra instance extensions required by the caller (e.g. GLFW).
        std::span<const char* const> instanceExtensions;
        /// Extra device extensions required by the caller.
        std::span<const char* const> deviceExtensions;
    };

    // -----------------------------------------------------------------------
    // Factory
    // -----------------------------------------------------------------------
    [[nodiscard]]
    static std::expected<VulkanContext, VulkanError> create(const Config& cfg);

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    [[nodiscard]] VkInstance       instance()       const noexcept { return m_instance;       }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VkDevice         device()         const noexcept { return m_device;         }

    [[nodiscard]] VkQueue  graphicsQueue() const noexcept { return m_graphicsQueue; }
    [[nodiscard]] VkQueue  presentQueue()  const noexcept { return m_presentQueue;  }
    [[nodiscard]] VkQueue  computeQueue()  const noexcept { return m_computeQueue;  }
    [[nodiscard]] VkQueue  transferQueue() const noexcept { return m_transferQueue; }

    [[nodiscard]] const QueueFamilyIndices& queueFamilies() const noexcept {
        return m_queueFamilies;
    }

    [[nodiscard]] VkPhysicalDeviceProperties deviceProperties() const noexcept {
        return m_deviceProperties;
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /// Create a VkShaderModule from a SPIR-V word vector.
    [[nodiscard]]
    std::expected<VkShaderModule, VkResult>
    createShaderModule(std::span<const uint32_t> spirv) const noexcept;

    /// Destroy a VkShaderModule owned by this device.
    void destroyShaderModule(VkShaderModule module) const noexcept;

private:
    VulkanContext() = default;

    // ---- instance-level objects ----
    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // ---- device-level objects ----
    VkPhysicalDevice              m_physicalDevice  = VK_NULL_HANDLE;
    VkDevice                      m_device          = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties    m_deviceProperties{};

    // ---- queues ----
    QueueFamilyIndices m_queueFamilies;
    VkQueue            m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue            m_presentQueue  = VK_NULL_HANDLE;
    VkQueue            m_computeQueue  = VK_NULL_HANDLE;
    VkQueue            m_transferQueue = VK_NULL_HANDLE;
};

} // namespace AkRender
