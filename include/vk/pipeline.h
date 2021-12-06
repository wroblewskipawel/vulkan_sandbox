#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "vk/device.h"
#include "vk/render_pass.h"
#include "vk/vertex.h"

namespace vks {

class GraphicsPipeline {
   public:
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline(GraphicsPipeline&& other)
        : device{other.device}, m_pipeline{other.m_pipeline} {
        other.m_pipeline = VK_NULL_HANDLE;
    };

    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

    ~GraphicsPipeline() {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(*device, m_pipeline, nullptr);
        }
    };

   private:
    friend class Context;
    Device& device;

    GraphicsPipeline(Device& device, RenderPass& renderPass,
                     const std::filesystem::path& dir, VkPipelineLayout layout,
                     const VertexAttribs& attribs);

    enum class Stage {
        Vertex = VK_SHADER_STAGE_VERTEX_BIT,
        Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
        TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessellationEval = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    };

    using ProgramSource = std::unordered_map<Stage, std::vector<char>>;

    std::vector<char> loadShaderSource(const std::filesystem::path& filepath);
    std::vector<VkPipelineShaderStageCreateInfo> createShaderModules(
        const ProgramSource& source);
    void buildPipeline(const ProgramSource& source, RenderPass& renderPass,
                       VkPipelineLayout layout, const VertexAttribs& attribs);

    VkPipeline m_pipeline;
};

}  // namespace vks