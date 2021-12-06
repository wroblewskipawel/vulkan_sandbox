#include "vk/context.h"

#include <glm/gtc/type_ptr.hpp>

#include "vk/buffer.h"

using namespace magic_enum;

namespace vks {

Context::Context(Device& device)
    : device{device},
      m_render_pass{device},
      m_swapchain{device, m_render_pass} {
    createSamplers();
    createDescriptorLayouts();
    createPipelineLayout();
}

Context::~Context() {
    vkDeviceWaitIdle(*device);
    vkDestroyPipelineLayout(*device, m_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(*device, m_material_layout, nullptr);
}

void Context::createSamplers() {
    for (auto type : enum_values<Sampler::Type>()) {
        m_samplers.emplace(type, Sampler{device, type});
    }
}

void Context::createDescriptorLayouts() {
    m_material_layout = MaterialUniform::descriptorLayout(device);
}

void Context::createPipelineLayout() {
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    std::array<VkDescriptorSetLayout, 1> set_layouts{m_material_layout};

    layout_info.setLayoutCount = set_layouts.size();
    layout_info.pSetLayouts = set_layouts.data();

    std::array<VkPushConstantRange, 1> push_ranges{};

    push_ranges[0].offset = 0;
    push_ranges[0].size = 2 * sizeof(glm::mat4);
    push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layout_info.pushConstantRangeCount = push_ranges.size();
    layout_info.pPushConstantRanges = push_ranges.data();

    if (vkCreatePipelineLayout(*device, &layout_info, nullptr,
                               &m_pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline layout");
    };
}

void Context::beginFrame(const glm::mat4& camera) {
    m_frame_state = m_swapchain.acquireImage();

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_frame_state._command, &begin_info);

    std::array<VkClearValue, 2> clear{};
    clear[0].depthStencil = {1.0f, 0};
    clear[1].color = {0.2f, 0.2f, 0.2f, 1.0f};

    VkRenderPassBeginInfo pass_info{};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.renderPass = *m_render_pass;
    pass_info.framebuffer = m_frame_state._framebuffer;
    pass_info.clearValueCount = clear.size();
    pass_info.pClearValues = clear.data();
    pass_info.renderArea.extent =
        device.info().surface_capabilities.currentExtent;
    pass_info.renderArea.offset = {0U, 0U};

    vkCmdBeginRenderPass(m_frame_state._command, &pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdPushConstants(m_frame_state._command, m_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                       glm::value_ptr(camera));
};

void Context::bindPipeline(PipelineHandle pipeline) {
    vkCmdBindPipeline(m_frame_state._command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_pipelines[pipeline.index].m_pipeline);
}

void Context::draw(ModelHandle model, const glm::mat4& transfrom) {
    vkCmdPushConstants(m_frame_state._command, m_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4),
                       sizeof(glm::mat4), glm::value_ptr(transfrom));
    m_resource_packs[model.pack].draw(m_frame_state._command, model.index,
                                      m_pipeline_layout);
}

void Context::endFrame() {
    vkCmdEndRenderPass(m_frame_state._command);
    if (vkEndCommandBuffer(m_frame_state._command) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record frame graphics commands");
    }

    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_frame_state._command;

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &m_frame_state._draw_ready;
    submit_info.pWaitDstStageMask = &wait_stage;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_frame_state._draw_finished;

    if (vkQueueSubmit(device.queues.graphics, 1, &submit_info,
                      m_frame_state._submit_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    };

    m_swapchain.presentImage(m_frame_state);
}

}  // namespace  vks
