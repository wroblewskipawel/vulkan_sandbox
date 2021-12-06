#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <magic_enum.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "vk/device.h"
#include "vk/pipeline.h"
#include "vk/render_pass.h"
#include "vk/resource_pack.h"
#include "vk/sampler.h"
#include "vk/swapchain.h"
#include "vk/vertex.h"

namespace vks {

class PipelineHandle {
    friend class Context;
    PipelineHandle(size_t index) : index{index} {};
    const size_t index;
};

class ModelHandle {
    friend class Context;
    ModelHandle(size_t pack, size_t index) : pack{pack}, index{index} {};
    const size_t pack;
    const size_t index;
};

class Context {
   public:
    Context(Device& device);
    Context(const Context&) = delete;
    Context(Context&&) = delete;

    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context();

    void beginFrame(const glm::mat4& camera);
    void bindPipeline(PipelineHandle pipeline);
    void draw(ModelHandle model, const glm::mat4& transfrom);
    void endFrame();

    PipelineHandle loadPipeline(const std::filesystem::path& dir) {
        m_pipelines.emplace_back(
            GraphicsPipeline{device, m_render_pass, dir, m_pipeline_layout,
                             VertexAttribs::defaultAttributes()});
        return PipelineHandle{m_pipelines.size() - 1};
    }
    std::unordered_map<std::string, ModelHandle> loadResources(
        const std::vector<std::string>& model_names,
        const Resources& resources) {
        auto& pack = m_resource_packs.emplace_back(
            ResourcePack::build(*this, model_names, resources));

        std::unordered_map<std::string, ModelHandle> handles{};
        handles.reserve(model_names.size());
        for (const auto& name : model_names) {
            handles.emplace(name, ModelHandle{m_resource_packs.size() - 1,
                                              pack.model_index(name)});
        }
        return handles;
    }

   private:
    friend class StagingBuffer;
    friend class ResourcePack;
    friend class Image2D;
    friend class Device;
    friend class Buffer;

    void createSamplers();
    void createDescriptorLayouts();
    void createPipelineLayout();

    Sampler& sampler(Sampler::Type type) { return m_samplers.at(type); }

    Device& device;

    RenderPass m_render_pass;
    Swapchain m_swapchain;

    std::vector<ResourcePack> m_resource_packs;
    std::unordered_map<std::string, size_t> m_model_pack_index;
    std::unordered_map<Sampler::Type, Sampler> m_samplers;

    VkDescriptorSetLayout m_material_layout;
    VkPipelineLayout m_pipeline_layout;

    std::vector<GraphicsPipeline> m_pipelines;

    Swapchain::FrameState m_frame_state;
};
}  // namespace vks