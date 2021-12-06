#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "vk/device.h"

namespace vks {

class Context;

class Image2D {
   public:
    static constexpr VkDeviceSize TexelSize(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_UNORM:
                return 4;
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_B8G8R8_UNORM:
                return 3;
            case VK_FORMAT_R8G8_UNORM:
                return 2;
            case VK_FORMAT_R8_UNORM:
                return 1;
            default:
                return 0;
        }
    };

    Image2D(Device& device, uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            const std::vector<uint32_t>& queue_families,
            uint32_t mip_levels = 1, uint32_t array_layers = 1);
    Image2D(const Image2D&) = delete;
    Image2D& operator=(const Image2D&) = delete;
    Image2D& operator=(Image2D&&) = delete;
    Image2D(Image2D&& other)
        : device{other.device},
          m_width{other.m_width},
          m_height{other.m_height},
          m_format{other.m_format},
          m_image{other.m_image},
          m_view{other.m_view} {
        other.m_image = VK_NULL_HANDLE;
        other.m_view = VK_NULL_HANDLE;
    };

    ~Image2D();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    VkFormat format() const { return m_format; }

    VkMemoryRequirements memoryRequirements() const;

   private:
    friend class StagingBuffer;
    friend class ResourcePack;
    friend class ImageView2D;

    VkImage operator*() { return m_image; }
    void bindMemory(VkDeviceMemory memory, VkDeviceSize offset);

    Device& device;

    uint32_t m_width;
    uint32_t m_height;
    VkFormat m_format;
    uint32_t m_layers;
    uint32_t m_levels;

    VkImage m_image;
    VkImageView m_view;
};

class ImageView2D {
   public:
    ImageView2D(Device& device, Image2D& image, VkImageAspectFlags aspect,
                uint32_t base_level = 0, uint32_t base_layer = 0);
    ImageView2D(const ImageView2D&) = delete;
    ImageView2D& operator=(const ImageView2D&) = delete;
    ImageView2D& operator=(ImageView2D&&) = delete;
    ImageView2D(ImageView2D&& other)
        : device{other.device}, m_view{other.m_view} {
        other.m_view = VK_NULL_HANDLE;
    }

    ~ImageView2D();

   private:
    friend class ResourcePack;

    VkImageView operator*() { return m_view; }

    Device& device;

    VkImageView m_view;
};
}  // namespace vks