#include "vk/device.h"

#include <cstring>
#include <optional>
#include <stdexcept>
#include <unordered_set>

using namespace std::string_literals;

namespace vks {

std::vector<std::string> REQUIRED_INSTANCE_EXTENSIONS{
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
std::vector<std::string> REQUIRED_DEVICE_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};
std::vector<std::string> REQUIRED_VALIDATION_LAYERS{
    "VK_LAYER_KHRONOS_validation"s};
std::vector<VkFormat> PREFERRED_SURFACE_FORMATS{
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_B8G8R8A8_UNORM,
};

VkPhysicalDeviceFeatures Device::requiredDeviceFeatures() {
    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    return features;
}

VkDebugUtilsMessengerCreateInfoEXT Device::messengerInfo() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.pfnUserCallback =
        (PFN_vkDebugUtilsMessengerCallbackEXT)messengerCallback;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    return create_info;
}

VkBool32 VKAPI_PTR Device::messengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    const void* user_data) {
    std::string type_description{};
    if (message_types | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type_description += "GENERAL;";
    }
    if (message_types | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type_description += "VALIDATION;";
    }
    if (message_types | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type_description += "PERFORMANCE;";
    }
    std::string severity_description = [&]() {
        switch (message_severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                return "ERROR";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                return "WARNING";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                return "INFO";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                return "VERBOSE";
            default:
                return "UNKNOWN";
        }
    }();
    std::string message{callback_data->pMessage};
    std::cerr << "Vulkan Validation [" + type_description + "][" +
                     severity_description + "]: " + message
              << std::endl;
    return VK_FALSE;
}

Device::Device(Window& window) {
    createInstance(window);
    pickPhysicalDevice();
    createDevice();
}

Device::~Device() {
    vkDeviceWaitIdle(m_device);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_messenger != VK_NULL_HANDLE) {
        auto destoryMessenger =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance,
                                      "vkDestroyDebugUtilsMessengerEXT"));
        destoryMessenger(m_instance, m_messenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
}

void Device::createInstance(Window& window) {
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    auto enabled_extensions = requiredInstanceExtensions(window);
    create_info.enabledExtensionCount = enabled_extensions.size();
    create_info.ppEnabledExtensionNames = enabled_extensions.data();

    auto enabled_layers = requiredValidationLayers();
    create_info.enabledLayerCount = enabled_layers.size();
    create_info.ppEnabledLayerNames = enabled_layers.data();

    auto messenger_info = messengerInfo();

    VkApplicationInfo app_info{};
    app_info.apiVersion = VK_API_VERSION_1_2;

    create_info.pApplicationInfo = &app_info;
    create_info.pNext = &messenger_info;

    if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance");
    }

    auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createMessenger(m_instance, &messenger_info, nullptr, &m_messenger) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug messenger");
    }

    window.createVkSurface(m_instance, m_surface);
}

std::vector<const char*> Device::requiredInstanceExtensions(Window& window) {
    std::vector<const char*> supported;

    uint32_t window_extension_count{};
    auto window_extensions =
        window.requiredSurfaceExtensions(window_extension_count);
    if (!window_extensions) {
        throw std::runtime_error("Surface extensions not supported");
    }

    supported.resize(window_extension_count);
    for (size_t i{0}; i < window_extension_count; i++) {
        supported[i] = window_extensions[i];
    }

    uint32_t count{};
    vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateInstanceExtensionProperties(NULL, &count, properties.data());

    for (const auto& name : REQUIRED_INSTANCE_EXTENSIONS) {
        if (std::find_if(properties.begin(), properties.end(),
                         [&](auto& properties) {
                             return std::strcmp(properties.extensionName,
                                                name.c_str()) == 0;
                         }) != properties.end()) {
            supported.push_back(name.c_str());
        } else {
            throw std::runtime_error("Instance extension " + name +
                                     " not supported");
        }
    }
    return supported;
}

std::vector<const char*> Device::requiredValidationLayers() {
    std::vector<const char*> supported{};

    uint32_t count{};
    vkEnumerateInstanceLayerProperties(&count, NULL);
    std::vector<VkLayerProperties> properties(count);
    vkEnumerateInstanceLayerProperties(&count, properties.data());
    for (const auto& name : REQUIRED_VALIDATION_LAYERS) {
        if (std::find_if(
                properties.begin(), properties.end(), [&](auto& properties) {
                    return std::strcmp(properties.layerName, name.c_str()) == 0;
                }) != properties.end()) {
            supported.push_back(name.c_str());
        } else {
            throw std::runtime_error("Layer " + name + " not supported");
        }
    }
    return supported;
}

std::vector<const char*> Device::requiredDeviceExtensions() {
    std::vector<const char*> extensions;
    for (auto& name : REQUIRED_DEVICE_EXTENSIONS) {
        extensions.push_back(name.c_str());
    }
    return extensions;
}

void Device::pickPhysicalDevice() {
    uint32_t count{};
    vkEnumeratePhysicalDevices(m_instance, &count, NULL);
    std::vector<VkPhysicalDevice> physical_devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, physical_devices.data());

    for (auto device : physical_devices) {
        if (isSuitable(device, m_surface, device_info)) {
            m_physical_device = device;
            return;
        }
    }

    throw std::runtime_error("Failed to pick suitable physical device");
}

bool Device::isSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                        PhysicalDeviceInfo& info) {
    vkGetPhysicalDeviceProperties(device, &info.properties);
    if (!(info.properties.deviceType &
          (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU |
           VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))) {
        return false;
    }

    if (!deviceExtensionsSupported(device)) {
        return false;
    }
    VkPhysicalDeviceFeatures device_features{};
    vkGetPhysicalDeviceFeatures(device, &device_features);
    if (!deviceFeaturesSupported(device_features)) {
        return false;
    }
    if (!getQueueFamilies(device, surface, info)) {
        return false;
    }
    getSurfaceProperties(device, surface, info);

    vkGetPhysicalDeviceMemoryProperties(device, &info.memory_properties);

    info.depth_format = getSupportedImageFormat(
        device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    return true;
}

bool Device::deviceExtensionsSupported(VkPhysicalDevice device) {
    uint32_t count{};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                         properties.data());
    for (const auto& name : REQUIRED_DEVICE_EXTENSIONS) {
        if (std::find_if(properties.begin(), properties.end(),
                         [&](auto& properties) {
                             return std::strcmp(properties.extensionName,
                                                name.c_str()) == 0;
                         }) == properties.end()) {
            return false;
        }
    }
    return true;
}

bool Device::deviceFeaturesSupported(
    const VkPhysicalDeviceFeatures& supported) {
    auto required = requiredDeviceFeatures();
    for (size_t i{0}; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
         i++) {
        if (*(reinterpret_cast<const VkBool32*>(&required) + i) &&
            !*(reinterpret_cast<const VkBool32*>(&supported) + i)) {
            return false;
        }
    }
    return true;
}

bool Device::getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface,
                              PhysicalDeviceInfo& info) {
    uint32_t count{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    std::optional<uint32_t> graphics_queue{};
    std::optional<uint32_t> compute_queue{};
    std::optional<uint32_t> transfer_queue{};
    std::optional<uint32_t> present_queue{};

    for (uint32_t i{0}; i < properties.size(); i++) {
        const auto& family = properties[i];
        if (!graphics_queue.has_value() &&
            family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue = i;
        }
        if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (compute_queue.has_value() && graphics_queue.has_value() &&
                compute_queue.value() == graphics_queue.value()) {
                compute_queue = i;
            } else if (!compute_queue.has_value()) {
                compute_queue = i;
            }
        }
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (transfer_queue.has_value() && graphics_queue.has_value() &&
                transfer_queue.value() == graphics_queue.value()) {
                transfer_queue = i;
            } else if (!transfer_queue.has_value()) {
                transfer_queue = i;
            }
        }
        VkBool32 is_present_queue{VK_FALSE};
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &is_present_queue);
        if (is_present_queue) {
            present_queue = i;
        }
    }
    if (!transfer_queue.has_value()) {
        transfer_queue = compute_queue;
    }
    if (!graphics_queue.has_value() || !compute_queue.has_value() ||
        !transfer_queue.has_value() || !present_queue.has_value()) {
        return false;
    }
    info.queue_families.graphics = graphics_queue.value();
    info.queue_families.compute = compute_queue.value();
    info.queue_families.transfer = transfer_queue.value();
    info.queue_families.present = present_queue.value();
    return true;
}

void Device::getSurfaceProperties(VkPhysicalDevice device, VkSurfaceKHR surface,
                                  PhysicalDeviceInfo& info) {
    uint32_t count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, NULL);
    std::vector<VkSurfaceFormatKHR> formats{count};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count,
                                         formats.data());

    std::optional<VkSurfaceFormatKHR> surface_format{};
    for (VkFormat format : PREFERRED_SURFACE_FORMATS) {
        auto item = std::find_if(formats.begin(), formats.end(),
                                 [&](auto& surface_format) {
                                     return surface_format.format == format;
                                 });
        if (item != formats.end()) {
            surface_format = *item;
        }
    }
    info.surface_format = surface_format.value_or(formats[0]);

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count,
                                              modes.data());

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) !=
        modes.end()) {
        info.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    } else {
        info.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &info.surface_capabilities);
}

VkFormat Device::getSupportedImageFormat(VkPhysicalDevice device,
                                         const std::vector<VkFormat>& formats,
                                         VkImageTiling tiling,
                                         VkFormatFeatureFlags features) {
    for (auto format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (properties.optimalTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_LINEAR &&
                   (properties.linearTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find suitable image format");
}

std::vector<uint32_t> Device::getQueueIndices(VkQueueFlags queues) const {
    std::unordered_set<uint32_t> unique_indices;
    if (queues & VK_QUEUE_GRAPHICS_BIT) {
        unique_indices.insert(device_info.queue_families.graphics);
    }
    if (queues & VK_QUEUE_COMPUTE_BIT) {
        unique_indices.insert(device_info.queue_families.compute);
    }
    if (queues & VK_QUEUE_TRANSFER_BIT) {
        unique_indices.insert(device_info.queue_families.transfer);
    }
    return {unique_indices.begin(), unique_indices.end()};
};

uint32_t Device::getMemoryIndex(uint32_t type_bits,
                                VkMemoryPropertyFlags properties) const {
    for (size_t i{0}; i < device_info.memory_properties.memoryTypeCount; i++) {
        if ((1 << i) & type_bits &&
            (device_info.memory_properties.memoryTypes[i].propertyFlags &
             properties) == properties) {
            return i;
        };
    }
    throw std::runtime_error("Failed to find suitable memory index");
}

void Device::createDevice() {
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    auto extensions = requiredDeviceExtensions();
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();

    auto features = requiredDeviceFeatures();
    create_info.pEnabledFeatures = &features;

    std::unordered_set<uint32_t> queue_families{
        device_info.queue_families.graphics,
        device_info.queue_families.compute,
        device_info.queue_families.transfer,
        device_info.queue_families.present,
    };
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_families.size());
    float priority = 1.0f;

    for (uint32_t family : queue_families) {
        queue_create_infos.push_back(VkDeviceQueueCreateInfo{});
        auto& create_info = queue_create_infos.back();
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        create_info.queueFamilyIndex = family;
        create_info.queueCount = 1;
        create_info.pQueuePriorities = &priority;
        create_info.pNext = nullptr;
    }

    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pQueueCreateInfos = queue_create_infos.data();
    if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create vulkan logical device");
    };

    vkGetDeviceQueue(m_device, device_info.queue_families.graphics, 0,
                     &queues.graphics);
    vkGetDeviceQueue(m_device, device_info.queue_families.compute, 0,
                     &queues.compute);
    vkGetDeviceQueue(m_device, device_info.queue_families.transfer, 0,
                     &queues.transfer);
    vkGetDeviceQueue(m_device, device_info.queue_families.present, 0,
                     &queues.present);
}

}  // namespace vks