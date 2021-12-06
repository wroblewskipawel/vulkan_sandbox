#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "resources.h"
#include "vk/buffer.h"
#include "vk/image.h"
#include "vk/material.h"

namespace vks {

class Context;

class ResourcePack {
   public:
    ResourcePack(const ResourcePack&) = delete;
    ResourcePack(ResourcePack&& other)
        : device{other.device},
          m_buffers{std::move(other.m_buffers)},
          m_materials{std::move(other.m_materials)},
          m_model_indices{std::move(other.m_model_indices)},
          m_model_offsets{std::move(other.m_model_offsets)},
          m_texture_images{std::move(other.m_texture_images)},
          m_texture_views{std::move(other.m_texture_views)},
          m_memory{other.m_memory} {
        other.m_memory = VK_NULL_HANDLE;
    };

    ResourcePack& operator=(const ResourcePack&) = delete;
    ResourcePack& operator=(ResourcePack&&) = delete;

    ~ResourcePack();

    size_t model_index(const std::string& name) const {
        return m_model_indices.at(name);
    }

    void draw(VkCommandBuffer cmd, size_t model_index,
              VkPipelineLayout layout) {
        auto offsets = m_model_offsets[model_index];
        VkDeviceSize vertex_offset =
            offsets.vertex_offset * sizeof(Model::Vertex);
        VkBuffer vertex_buffer = *m_buffers.vertex;

        VkBuffer index_buffer = *m_buffers.index;
        VkDeviceSize index_offset = offsets.index_offset * sizeof(uint32_t);

        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &vertex_offset);
        vkCmdBindIndexBuffer(cmd, index_buffer, index_offset,
                             VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
            &m_materials.descriptors[offsets.material_index], 0, nullptr);
        vkCmdDrawIndexed(cmd, offsets.index_count, 1, 0, 0, 0);
    }

   private:
    Device& device;

    friend class Context;
    static ResourcePack build(Context& _context,
                              const std::vector<std::string>& model_names,
                              const Resources& resources);

    struct ModelOffset {
        size_t vertex_offset;
        size_t index_offset;
        size_t index_count;
        size_t material_index;
    };

    struct Buffers {
        Buffers(Buffer&& vertex, Buffer&& index, Buffer&& uniform)
            : vertex{std::move(vertex)},
              index{std::move(index)},
              uniform{std::move(uniform)} {};
        Buffers(Buffers&&) = default;

        Buffers& operator=(const Buffers&) = delete;
        Buffers& operator=(Buffers&&) = delete;

        Buffer vertex;
        Buffer index;
        Buffer uniform;
    } m_buffers;

    static void getResourceNames(const std::vector<std::string>& model_names,
                                 const Resources& resources,
                                 std::vector<ModelOffset>& model_offsets,
                                 std::vector<std::string>& material_names,
                                 std::vector<std::string>& texture_names,
                                 VkDeviceSize& vertex_buffer_size,
                                 VkDeviceSize& index_buffer_size,
                                 VkDeviceSize& staging_buffer_size);

    static Buffers createBuffers(Device& device,
                                 VkMemoryRequirements& vertex_requirements,
                                 VkMemoryRequirements& index_requirements,
                                 VkMemoryRequirements& uniform_requirements,
                                 const std::vector<uint32_t>& queue_indices);

    using TextureRequirements = std::pair<size_t, VkMemoryRequirements>;
    static std::vector<Image2D> createTextureImages(
        Device& device, const std::vector<std::string>& texture_names,
        const Resources& resources, const std::vector<uint32_t>& queue_indices,
        std::vector<TextureRequirements>& requirements,
        VkDeviceSize& staging_buffer_size);

    static VkDeviceMemory allocateMemory(
        Device& device, const VkMemoryRequirements& vertex_requirements,
        const VkMemoryRequirements& index_requirements,
        const VkMemoryRequirements& uniform_requirements,
        const std::vector<TextureRequirements>& texture_requirements,
        Buffers& buffers, std::vector<Image2D>& images);

    static void copyResources(Context& context,
                              VkDeviceSize staging_buffer_size,
                              const std::vector<std::string>& model_names,
                              const std::vector<ModelOffset>& model_offsets,
                              const std::vector<std::string>& material_names,
                              const std::vector<std::string>& texture_names,
                              const Resources& resources, Buffers& buffers,
                              std::vector<Image2D>& images);
    static std::vector<ImageView2D> createTextureImageViews(
        Device& device, std::vector<Image2D>& images);

    struct Materials {
        Materials(VkDescriptorPool pool,
                  std::vector<VkDescriptorSet>&& descriptors)
            : descriptors{std::move(descriptors)}, pool{pool} {};
        Materials(Materials&&) = default;

        Materials& operator=(const Materials&) = delete;
        Materials& operator=(Material&&) = delete;

        std::vector<VkDescriptorSet> descriptors;
        VkDescriptorPool pool;
    } m_materials;

    static Materials createMaterialDescriptors(
        Context& context, const std::vector<std::string>& material_names,
        const std::vector<std::string>& texture_names,
        const Resources& resources, Buffers& buffers,
        std::vector<ImageView2D>& texture_views);

    ResourcePack(Device& device, Buffers&& buffers, Materials&& materials,
                 std::unordered_map<std::string, size_t> model_indices,
                 std::vector<ModelOffset>&& model_offsets,
                 std::vector<Image2D>&& texture_images,
                 std::vector<ImageView2D>&& texture_views,
                 VkDeviceMemory memory)
        : device{device},
          m_buffers{std::move(buffers)},
          m_materials{std::move(materials)},
          m_model_indices{std::move(model_indices)},
          m_model_offsets{std::move(model_offsets)},
          m_texture_images{std::move(texture_images)},
          m_texture_views{std::move(texture_views)},
          m_memory{memory} {};

    std::unordered_map<std::string, size_t> m_model_indices;
    std::vector<ModelOffset> m_model_offsets;
    std::vector<Image2D> m_texture_images;
    std::vector<ImageView2D> m_texture_views;

    VkDeviceMemory m_memory;
};
}  // namespace vks