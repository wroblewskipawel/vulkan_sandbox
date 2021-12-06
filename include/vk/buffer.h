#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <vector>

#include "vk/image.h"

namespace vks {

class Context;

class Buffer {
   public:
    Buffer(Device& device, VkDeviceSize _size, VkBufferUsageFlags usage,
           const std::vector<uint32_t>& queue_families);
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&&) = delete;
    Buffer(Buffer&& other)
        : device{other.device}, m_buffer{other.m_buffer}, m_size{other.m_size} {
        other.m_buffer = VK_NULL_HANDLE;
    };

    ~Buffer();

    VkDeviceSize size() const { return m_size; }
    VkMemoryRequirements memoryRequirements() const;

   private:
    friend class StagingBuffer;
    friend class ResourcePack;

    VkBuffer operator*() { return m_buffer; }
    void bindMemory(VkDeviceMemory memory, VkDeviceSize offset);

    Device& device;

    VkBuffer m_buffer;
    VkDeviceSize m_size;
};

class StagingBuffer {
   public:
    StagingBuffer(Device& device, VkDeviceSize size);
    StagingBuffer(const StagingBuffer&) = delete;
    StagingBuffer& operator=(const StagingBuffer&) = delete;
    StagingBuffer& operator=(StagingBuffer&&) = delete;
    StagingBuffer(StagingBuffer&&) = delete;

    ~StagingBuffer();

    void copyBuffer(Buffer& dst, VkDeviceSize offset, const void* src,
                    VkDeviceSize size);
    void copyImage(Image2D& dst, const std::vector<uint8_t>& src);

   private:
    void waitFence();

    VkCommandBuffer beginTransferCommand();
    void endTransferCommand(VkCommandBuffer command);

    Device& device;

    VkCommandPool m_pool;
    VkDeviceMemory m_memory;
    VkBuffer m_buffer;
    VkFence m_copy_fence;
    VkDeviceSize m_size;
};

}  // namespace vks