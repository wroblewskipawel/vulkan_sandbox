#include "vk/swapchain.h"

#include <algorithm>

namespace vks {
Swapchain::Swapchain(Device& device, RenderPass& renderPass) : device{device} {
    createSwapchain();
    createDepthBuffer();
    createFramebuffers(renderPass);
    createSynchronizationPrimitives();
    createCommandBuffers();
    m_current_frame = 0;
}

Swapchain::~Swapchain() {
    for (size_t i{0}; i < m_images.size(); i++) {
        vkDestroySemaphore(*device, m_image_draw_finished[i], nullptr);
        vkDestroySemaphore(*device, m_image_draw_ready[i], nullptr);
        vkDestroyFence(*device, m_image_available[i], nullptr);
        vkDestroyFramebuffer(*device, m_framebuffers[i], nullptr);
        vkDestroyImageView(*device, m_image_views[i], nullptr);
    }
    vkDestroyImageView(*device, m_depth_buffer.view, nullptr);
    vkDestroyImage(*device, m_depth_buffer.image, nullptr);
    vkFreeMemory(*device, m_depth_buffer.memory, nullptr);
    vkDestroySwapchainKHR(*device, m_swapchain, nullptr);
    vkDestroyCommandPool(*device, m_pool, nullptr);
}

void Swapchain::createSwapchain() {
    const auto& capabilities = device.info().surface_capabilities;
    auto surface_format = device.info().surface_format;
    auto depth_format = device.info().depth_format;
    auto present_mode = device.info().present_mode;

    m_extent = capabilities.currentExtent;
    m_extent.height =
        std::clamp(m_extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    m_extent.width =
        std::clamp(m_extent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);

    std::array<uint32_t, 2> queue_families{
        device.info().queue_families.graphics,
        device.info().queue_families.present};
    auto [sharing_mode, count] = queue_families[0] != queue_families[1]
                                     ? std::pair{VK_SHARING_MODE_CONCURRENT, 2U}
                                     : std::pair{VK_SHARING_MODE_EXCLUSIVE, 1U};

    auto min_images = capabilities.maxImageCount != 0
                          ? std::min(capabilities.minImageCount + 1,
                                     capabilities.maxImageCount)
                          : capabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = device.m_surface;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = m_extent;
    create_info.imageSharingMode = sharing_mode;
    create_info.queueFamilyIndexCount = count;
    create_info.pQueueFamilyIndices = queue_families.data();
    create_info.presentMode = present_mode;
    create_info.preTransform = capabilities.currentTransform;
    create_info.clipped = VK_TRUE;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.minImageCount = min_images;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(*device, &create_info, nullptr, &m_swapchain) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }
}

void Swapchain::createDepthBuffer() {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.extent = {m_extent.width, m_extent.height, 1};
    image_info.queueFamilyIndexCount = 1;
    image_info.pQueueFamilyIndices = &device.info().queue_families.graphics;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = device.info().depth_format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(*device, &image_info, nullptr, &m_depth_buffer.image) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth buffer image");
    }
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(*device, m_depth_buffer.image, &requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = device.getMemoryIndex(
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(*device, &alloc_info, nullptr,
                         &m_depth_buffer.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth buffer memory");
    }
    vkBindImageMemory(*device, m_depth_buffer.image, m_depth_buffer.memory, 0);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.format = device.info().depth_format;
    view_info.image = m_depth_buffer.image;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.subresourceRange.levelCount = 1;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    if (vkCreateImageView(*device, &view_info, nullptr, &m_depth_buffer.view) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth buffer image view");
    }
}

void Swapchain::createFramebuffers(RenderPass& renderPass) {
    uint32_t count{};
    vkGetSwapchainImagesKHR(*device, m_swapchain, &count, nullptr);
    m_images.resize(count);
    vkGetSwapchainImagesKHR(*device, m_swapchain, &count, m_images.data());

    m_image_views.resize(count);
    for (size_t i{0}; i < m_images.size(); i++) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_images[i];
        view_info.format = device.info().surface_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.layerCount = 1;
        view_info.subresourceRange.levelCount = 1;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (vkCreateImageView(*device, &view_info, nullptr,
                              &m_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }

    std::array<VkImageView, 2> attachments{};
    attachments[0] = m_depth_buffer.view;

    m_framebuffers.resize(count);
    for (size_t i{0}; i < m_images.size(); i++) {
        attachments[1] = m_image_views[i];

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.attachmentCount = 2;
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = m_extent.width;
        framebuffer_info.height = m_extent.height;
        framebuffer_info.layers = 1;
        framebuffer_info.renderPass = *renderPass;
        if (vkCreateFramebuffer(*device, &framebuffer_info, nullptr,
                                &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain framebuffers");
        }
    }
}

void Swapchain::createSynchronizationPrimitives() {
    m_image_available.resize(m_images.size());
    m_image_draw_finished.resize(m_images.size());
    m_image_draw_ready.resize(m_images.size());

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i{0}; i < m_images.size(); i++) {
        auto rcreated = vkCreateSemaphore(*device, &semaphore_info, nullptr,
                                          &m_image_draw_ready[i]);
        auto fcreated = vkCreateSemaphore(*device, &semaphore_info, nullptr,
                                          &m_image_draw_finished[i]);
        auto acreated =
            vkCreateFence(*device, &fence_info, nullptr, &m_image_available[i]);
        if (rcreated != VK_SUCCESS || fcreated != VK_SUCCESS ||
            acreated != VK_SUCCESS) {
            throw std::runtime_error(
                "Failed to create swapchain synchronization primitives");
        }
    }
}

void Swapchain::createCommandBuffers() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device.info().queue_families.graphics;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(*device, &pool_info, nullptr, &m_pool) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to create swapchian graphics commands pool");
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_pool;
    alloc_info.commandBufferCount = m_images.size();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    m_commands.resize(m_images.size());

    if (vkAllocateCommandBuffers(*device, &alloc_info, m_commands.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to allocate swapchian command buffers");
    }
}

Swapchain::FrameState Swapchain::acquireImage() {
    FrameState state{};
    state._draw_finished = m_image_draw_finished[m_current_frame];
    state._draw_ready = m_image_draw_ready[m_current_frame];
    if (vkAcquireNextImageKHR(*device, m_swapchain, UINT64_MAX,
                              state._draw_ready, VK_NULL_HANDLE,
                              &state._index) != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire next swapchain image");
    };
    state._submit_fence = m_image_available[state._index];
    if (vkWaitForFences(*device, 1, &state._submit_fence, VK_TRUE,
                        UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Swapchain image available fence timeout");
    }
    vkResetFences(*device, 1, &state._submit_fence);
    state._framebuffer = m_framebuffers[state._index];
    state._command = m_commands[state._index];
    return state;
}

void Swapchain::presentImage(FrameState& state) {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &state._index;
    present_info.pSwapchains = &m_swapchain;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &state._draw_finished;

    if (vkQueuePresentKHR(device.queues.present, &present_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    };
    m_current_frame = (m_current_frame + 1) % m_images.size();
}

}  // namespace  vks
