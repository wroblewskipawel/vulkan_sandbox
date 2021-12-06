#include "vk/pipeline.h"

#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>

#include "resources.h"

using namespace std::string_literals;

namespace vks {

GraphicsPipeline::GraphicsPipeline(Device& device, RenderPass& renderPass,
                                   const std::filesystem::path& dir,
                                   VkPipelineLayout layout,
                                   const VertexAttribs& attribs)
    : device(device) {
    ProgramSource source{};
    for (auto entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto path = entry.path();
            if (path.extension() != ".spv"s) {
                throw std::runtime_error("Invalid shader source format at: "s +
                                         path.string());
            }
            auto stage = [&]() -> Stage {
                auto stem = path.stem();
                if (stem == "vert"s) {
                    return Stage::Vertex;
                } else if (stem == "frag"s) {
                    return Stage::Fragment;
                } else if (stem == "tesc"s) {
                    return Stage::TessellationControl;
                } else if (stem == "tese"s) {
                    return Stage::TessellationEval;
                } else if (stem == "geom"s) {
                    return Stage::Geometry;
                }
                throw std::runtime_error(
                    "Invalid shader source file name at: "s + path.string());
            }();
            source.emplace(stage, loadShaderSource(path));
        }
    }
    if (source.count(Stage::Vertex) && source.count(Stage::Fragment)) {
        buildPipeline(source, renderPass, layout, attribs);
    } else {
        throw std::runtime_error(
            "Source for vertex and fragment shaders not fount at: "s +
            dir.string());
    }
}

std::vector<char> GraphicsPipeline::loadShaderSource(
    const std::filesystem::path& filepath) {
    std::ifstream fs(filepath, std::ios::ate | std::ios::binary);
    fs.exceptions(std::ios::badbit | std::ios::failbit);
    if (fs.is_open()) {
        size_t byte_size = fs.tellg();
        std::vector<char> shader_source(byte_size);
        fs.seekg(0, std::ios::beg);
        fs.read(shader_source.data(), byte_size);

        return shader_source;
    } else {
        throw std::runtime_error("Failed to open file at: "s +
                                 filepath.string());
    }
}

void GraphicsPipeline::buildPipeline(const ProgramSource& source,
                                     RenderPass& renderPass,
                                     VkPipelineLayout layout,
                                     const VertexAttribs& attribs) {
    VkPipelineInputAssemblyStateCreateInfo assembly_info{};
    assembly_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineVertexInputStateCreateInfo input_state{};
    input_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input_state.vertexBindingDescriptionCount = attribs.bindings.size();
    input_state.pVertexBindingDescriptions = attribs.bindings.data();
    input_state.vertexAttributeDescriptionCount = attribs.attributes.size();
    input_state.pVertexAttributeDescriptions = attribs.attributes.data();

    const auto& capabilities = device.info().surface_capabilities;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(capabilities.currentExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.height = -static_cast<float>(capabilities.currentExtent.height);
    viewport.width = static_cast<float>(capabilities.currentExtent.width);
    VkRect2D scissors{};
    scissors.extent = {capabilities.currentExtent.width,
                       capabilities.currentExtent.height};
    scissors.offset = {0, 0};
    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissors;

    VkPipelineRasterizationStateCreateInfo rasterization_info{};
    rasterization_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_info.depthBiasEnable = VK_FALSE;
    rasterization_info.depthClampEnable = VK_FALSE;
    rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_state{};
    blend_state.blendEnable = VK_TRUE;
    blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

    blend_state.colorBlendOp = VK_BLEND_OP_ADD;
    blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend_info{};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_state;

    VkPipelineDepthStencilStateCreateInfo depth_info{};
    depth_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = VK_TRUE;
    depth_info.depthWriteEnable = VK_TRUE;
    depth_info.depthCompareOp = VK_COMPARE_OP_LESS;

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pInputAssemblyState = &assembly_info;
    create_info.pMultisampleState = &multisample_info;
    create_info.pRasterizationState = &rasterization_info;
    create_info.pVertexInputState = &input_state;
    create_info.pViewportState = &viewport_info;
    create_info.pDepthStencilState = &depth_info;
    create_info.pColorBlendState = &blend_info;
    create_info.renderPass = *renderPass;
    create_info.layout = layout;
    create_info.subpass = 0;

    auto modules = createShaderModules(source);

    create_info.stageCount = modules.size();
    create_info.pStages = modules.data();

    if (vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &create_info,
                                  nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline");
    }

    for (auto& module : modules) {
        vkDestroyShaderModule(*device, module.module, nullptr);
    }
}

std::vector<VkPipelineShaderStageCreateInfo>
GraphicsPipeline::createShaderModules(const ProgramSource& source) {
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(source.size());
    for (auto& [stage, code] : source) {
        VkShaderModule module;
        VkShaderModuleCreateInfo module_info{};
        module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_info.codeSize = code.size();
        module_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        if (vkCreateShaderModule(*device, &module_info, nullptr, &module) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create vulkan shader module");
        }

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.module = module;
        stage_info.stage = static_cast<VkShaderStageFlagBits>(stage);
        stage_info.pName = "main";
        shader_stages.push_back(stage_info);
    }
    return shader_stages;
}

}  // namespace vks