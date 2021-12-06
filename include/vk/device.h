#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <magic_enum.hpp>
#include <optional>
#include <string>
#include <vector>

#include "window.h"

using namespace magic_enum;

namespace vks {

class Device {
   public:
    Device(Window& window);
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) = delete;
    Device(Device&&) = delete;
    ~Device();

   private:
    friend class GraphicsPipeline;
    friend class Swapchain;
    friend class RenderPass;
    friend class Buffer;
    friend class Context;
    friend class ResourcePack;

    friend class StagingBuffer;
    friend class ImageView2D;
    friend class Image2D;
    friend class Sampler;
    friend class MaterialUniform;

    VkDevice operator*() { return m_device; }

    std::vector<uint32_t> getQueueIndices(VkQueueFlags queues) const;
    uint32_t getMemoryIndex(uint32_t type_bits,
                            VkMemoryPropertyFlags properties) const;

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo();

    static VkBool32 VKAPI_PTR
    messengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                      VkDebugUtilsMessageTypeFlagsEXT message_types,
                      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                      const void* user_data);

    static std::vector<const char*> requiredInstanceExtensions(Window& window);
    static std::vector<const char*> requiredValidationLayers();
    static std::vector<const char*> requiredDeviceExtensions();
    static VkPhysicalDeviceFeatures requiredDeviceFeatures();

    void createInstance(Window& window);
    void pickPhysicalDevice();
    void createDevice();

    struct PhysicalDeviceInfo {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceMemoryProperties memory_properties;
        VkSurfaceCapabilitiesKHR surface_capabilities;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkFormat depth_format;
        struct {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
            uint32_t present;
        } queue_families;
    } device_info;

    const PhysicalDeviceInfo& info() const { return device_info; }

    static bool isSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                           PhysicalDeviceInfo& info);
    static bool getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface,
                                 PhysicalDeviceInfo& info);
    static void getSurfaceProperties(VkPhysicalDevice device,
                                     VkSurfaceKHR surface,
                                     PhysicalDeviceInfo& info);
    static bool deviceExtensionsSupported(VkPhysicalDevice device);

    static bool deviceFeaturesSupported(
        const VkPhysicalDeviceFeatures& features);

    static VkFormat getSupportedImageFormat(
        VkPhysicalDevice device, const std::vector<VkFormat>& formats,
        VkImageTiling tiling, VkFormatFeatureFlags features);

    struct {
        VkQueue graphics;
        VkQueue compute;
        VkQueue transfer;
        VkQueue present;
    } queues;

    VkInstance m_instance;
    VkDevice m_device;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physical_device;
    VkDebugUtilsMessengerEXT m_messenger;
};
}  // namespace vks
