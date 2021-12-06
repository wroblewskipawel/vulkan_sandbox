#pragma once

#include <vulkan/vulkan.h>

#include "vk/device.h"

namespace vks {

class RenderPass {
   public:
    RenderPass(const RenderPass&) = delete;
    RenderPass(RenderPass&&) = delete;

    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass& operator=(RenderPass&&) = delete;

    ~RenderPass();

    VkRenderPass operator*() { return m_render_pass; }

   private:
    friend class Context;
    RenderPass(Device& device);

    Device& device;

    VkRenderPass m_render_pass;
};

}  // namespace vks