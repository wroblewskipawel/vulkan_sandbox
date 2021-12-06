#include "vk/buffer.h"

#include <cstring>

#include "vk/context.h"

namespace vks {
StagingBuffer::StagingBuffer(Device& device, VkDeviceSize size)
    : device{device}, m_size{size} {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.queueFamilyIndexCount = 1;
    buffer_info.pQueueFamilyIndices = &device.info().queue_families.transfer;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.size = m_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(*device, &buffer_info, nullptr, &m_buffer) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer");
    }
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(*device, m_buffer, &requirements);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = device.getMemoryIndex(
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(*device, &alloc_info, nullptr, &m_memory) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }
    vkBindBufferMemory(*device, m_buffer, m_memory, 0);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device.info().queue_families.transfer;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(*device, &pool_info, nullptr, &m_pool) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to create staging buffer transfer command pool");
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (vkCreateFence(*device, &fence_info, nullptr, &m_copy_fence) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer fence");
    };
};

StagingBuffer::~StagingBuffer() {
    vkDestroyBuffer(*device, m_buffer, nullptr);
    vkFreeMemory(*device, m_memory, nullptr);
    vkDestroyFence(*device, m_copy_fence, nullptr);
    vkDestroyCommandPool(*device, m_pool, nullptr);
}

VkCommandBuffer StagingBuffer::beginTransferCommand() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_pool;
    alloc_info.commandBufferCount = 1;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer command;
    if (vkAllocateCommandBuffers(*device, &alloc_info, &command) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to alloacte staging buffer transfer command buffer");
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to begin recording staging buffer transfer command");
    }
    return command;
}

void StagingBuffer::endTransferCommand(VkCommandBuffer command) {
    if (vkEndCommandBuffer(command) != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to record staging buffer transfer command");
    };
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command;
    if (vkQueueSubmit(device.queues.transfer, 1, &submit_info, m_copy_fence) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to submit staging buffer transfer command");
    }
    waitFence();
    vkFreeCommandBuffers(*device, m_pool, 1, &command);
}

void StagingBuffer::copyBuffer(Buffer& dst, VkDeviceSize offset,
                               const void* src, VkDeviceSize size) {
    if (size > m_size) {
        throw std::runtime_error(
            "Not enough memory allocated for staging buffer");
    }

    if (dst.size() < offset + size) {
        throw std::runtime_error("Invalid destination buffer offset");
    }

    void* map{};
    vkMapMemory(*device, m_memory, 0, size, 0, &map);
    std::memcpy(map, src, size);
    vkUnmapMemory(*device, m_memory);

    auto command = beginTransferCommand();

    VkBufferCopy region{};
    region.dstOffset = offset;
    region.srcOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(command, m_buffer, *dst, 1, &region);

    endTransferCommand(command);
}
void StagingBuffer::copyImage(Image2D& dst, const std::vector<uint8_t>& src) {
    if (src.size() > m_size) {
        throw std::runtime_error(
            "Not enough memory allocated for staging buffer");
    }
    void* map{};
    vkMapMemory(*device, m_memory, 0, src.size(), 0, &map);
    std::memcpy(map, src.data(), src.size());
    vkUnmapMemory(*device, m_memory);

    auto command = beginTransferCommand();

    VkImageMemoryBarrier barier{};
    barier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barier.image = *dst;

    barier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barier.subresourceRange.baseArrayLayer = 0;
    barier.subresourceRange.baseMipLevel = 0;
    barier.subresourceRange.layerCount = 1;
    barier.subresourceRange.levelCount = 1;

    barier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barier.srcAccessMask = 0;
    barier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barier);

    VkBufferImageCopy region{};
    region.bufferImageHeight = dst.width();
    region.bufferRowLength = dst.height();
    region.bufferOffset = 0;
    region.imageExtent = {dst.width(), dst.height(), 1};
    region.imageOffset = {0, 0, 0};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(command, m_buffer, *dst,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barier.dstAccessMask = 0;

    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barier);

    endTransferCommand(command);
}

void StagingBuffer::waitFence() {
    if (vkWaitForFences(*device, 1, &m_copy_fence, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS) {
        throw std::runtime_error("Staging buffer fence timeout");
    };
    vkResetFences(*device, 1, &m_copy_fence);
}

Buffer::Buffer(Device& device, VkDeviceSize size, VkBufferUsageFlags usage,
               const std::vector<uint32_t>& queue_families)
    : device{device}, m_size{size} {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.usage = usage;
    buffer_info.size = m_size;
    buffer_info.queueFamilyIndexCount = queue_families.size();
    buffer_info.pQueueFamilyIndices = queue_families.data();
    buffer_info.sharingMode = queue_families.size() != 1
                                  ? VK_SHARING_MODE_CONCURRENT
                                  : VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(*device, &buffer_info, nullptr, &m_buffer) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }
}

Buffer::~Buffer() {
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(*device, m_buffer, nullptr);
    }
}

void Buffer::bindMemory(VkDeviceMemory memory, VkDeviceSize offset) {
    if (vkBindBufferMemory(*device, m_buffer, memory, offset) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind buffer memory");
    }
}

VkMemoryRequirements Buffer::memoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(*device, m_buffer, &requirements);
    return requirements;
}

}  // namespace vks