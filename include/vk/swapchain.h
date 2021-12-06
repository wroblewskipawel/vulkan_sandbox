#pragma once

#include <vulkan/vulkan.h>

#include "vk/device.h"
#include "vk/render_pass.h"

namespace vks {

class Swapchain {
   private:
    Swapchain(Device& device, RenderPass& renderPass);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;
    Swapchain(Swapchain&&) = delete;

    struct FrameState {
        uint32_t _index;
        VkFramebuffer _framebuffer;
        VkSemaphore _draw_ready;
        VkSemaphore _draw_finished;
        VkFence _submit_fence;
        VkCommandBuffer _command;
    };

    FrameState acquireImage();
    void presentImage(FrameState& state);

   public:
    friend class Context;
    Device& device;

    void createSwapchain();
    void createDepthBuffer();
    void createFramebuffers(RenderPass& renderPass);
    void createSynchronizationPrimitives();
    void createCommandBuffers();

    VkSwapchainKHR m_swapchain;

    struct {
        VkDeviceMemory memory;
        VkImage image;
        VkImageView view;
    } m_depth_buffer;

    VkExtent2D m_extent;

    VkCommandPool m_pool;
    std::vector<VkCommandBuffer> m_commands;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkImageView> m_image_views;
    std::vector<VkImage> m_images;
    std::vector<VkFence> m_image_available;
    std::vector<VkSemaphore> m_image_draw_finished;
    std::vector<VkSemaphore> m_image_draw_ready;

    uint32_t m_current_frame;
};

}  // namespace vks