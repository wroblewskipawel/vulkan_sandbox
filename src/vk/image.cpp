#include "vk/image.h"

#include "vk/context.h"

namespace vks {

Image2D::Image2D(Device& device, uint32_t width, uint32_t height,
                 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                 const std::vector<uint32_t>& queue_families, uint32_t levels,
                 uint32_t layers)
    : device{device},
      m_width{width},
      m_height{height},
      m_format{format},
      m_levels{levels},
      m_layers{layers} {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.tiling = tiling;
    image_info.format = m_format;
    image_info.extent = {m_width, m_height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.arrayLayers = m_layers;
    image_info.mipLevels = m_levels;

    image_info.queueFamilyIndexCount = queue_families.size();
    image_info.pQueueFamilyIndices = queue_families.data();
    image_info.sharingMode = queue_families.size() != 1
                                 ? VK_SHARING_MODE_CONCURRENT
                                 : VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(*device, &image_info, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }
}

VkMemoryRequirements Image2D::memoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(*device, m_image, &requirements);
    return requirements;
}

void Image2D::bindMemory(VkDeviceMemory memory, VkDeviceSize offset) {
    if (vkBindImageMemory(*device, m_image, memory, offset) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind image memory");
    };
}

Image2D::~Image2D() {
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(*device, m_image, nullptr);
    }
}

ImageView2D::ImageView2D(Device& device, Image2D& image,
                         VkImageAspectFlags aspect, uint32_t base_level,
                         uint32_t base_layer)
    : device{device} {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.format = image.m_format;
    view_info.image = image.m_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseArrayLayer = base_layer;
    view_info.subresourceRange.baseMipLevel = base_level;
    view_info.subresourceRange.layerCount = image.m_layers;
    view_info.subresourceRange.levelCount = image.m_levels;

    if (vkCreateImageView(*device, &view_info, nullptr, &m_view) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
}

ImageView2D::~ImageView2D() {
    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(*device, m_view, nullptr);
    }
}

}  // namespace vks