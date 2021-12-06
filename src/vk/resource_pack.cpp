#include "vk/resource_pack.h"

#include <iterator>
#include <magic_enum.hpp>
#include <unordered_set>

#include "vk/context.h"

using namespace std::string_literals;
using namespace magic_enum;

namespace vks {

ResourcePack::~ResourcePack() {
    if (m_memory != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(*device, m_materials.pool, nullptr);
        vkFreeMemory(*device, m_memory, nullptr);
    }
}

ResourcePack ResourcePack::build(Context& context,
                                 const std::vector<std::string>& model_names,
                                 const Resources& resources) {
    auto queue_indices = context.device.getQueueIndices(VK_QUEUE_GRAPHICS_BIT |
                                                        VK_QUEUE_TRANSFER_BIT);

    VkDeviceSize vertex_buffer_size{};
    VkDeviceSize index_buffer_size{};
    VkDeviceSize staging_buffer_size{};

    std::vector<ModelOffset> model_offsets{};
    std::vector<std::string> material_names{};
    std::vector<std::string> texture_names{};

    getResourceNames(model_names, resources, model_offsets, material_names,
                     texture_names, vertex_buffer_size, index_buffer_size,
                     staging_buffer_size);

    VkMemoryRequirements vertex_requirements{};
    vertex_requirements.size = vertex_buffer_size;
    VkMemoryRequirements index_requirements{};
    index_requirements.size = index_buffer_size;
    VkMemoryRequirements uniform_requirements{};
    uniform_requirements.size = material_names.size() * sizeof(MaterialUniform);

    auto buffers =
        createBuffers(context.device, vertex_requirements, index_requirements,
                      uniform_requirements, queue_indices);

    std::vector<TextureRequirements> texture_requirements{};
    auto texture_images = createTextureImages(
        context.device, texture_names, resources, queue_indices,
        texture_requirements, staging_buffer_size);

    auto memory = allocateMemory(context.device, vertex_requirements,
                                 index_requirements, uniform_requirements,
                                 texture_requirements, buffers, texture_images);

    copyResources(context, staging_buffer_size, model_names, model_offsets,
                  material_names, texture_names, resources, buffers,
                  texture_images);

    auto texture_views =
        createTextureImageViews(context.device, texture_images);

    auto materials =
        createMaterialDescriptors(context, material_names, texture_names,
                                  resources, buffers, texture_views);

    std::unordered_map<std::string, size_t> model_indices{};
    model_indices.reserve(model_names.size());
    for (size_t i{0}; i < model_names.size(); i++) {
        model_indices.emplace(model_names[i], i);
    }

    return {context.device,           std::move(buffers),
            std::move(materials),     std::move(model_indices),
            std::move(model_offsets), std::move(texture_images),
            std::move(texture_views), memory};
};

void ResourcePack::getResourceNames(const std::vector<std::string>& model_names,
                                    const Resources& resources,
                                    std::vector<ModelOffset>& model_offsets,
                                    std::vector<std::string>& material_names,
                                    std::vector<std::string>& texture_names,
                                    VkDeviceSize& vertex_buffer_size,
                                    VkDeviceSize& index_buffer_size,
                                    VkDeviceSize& staging_buffer_size) {
    VkDeviceSize vertex_offset{0};
    VkDeviceSize index_offset{0};

    staging_buffer_size = 0;
    vertex_buffer_size = 0;
    index_buffer_size = 0;
    model_offsets.reserve(model_names.size());

    std::unordered_set<std::string> unique_materials{};

    for (const auto& name : model_names) {
        const auto& model = resources.models.at(name);
        model_offsets.push_back(ModelOffset{vertex_offset, index_offset,
                                            model.indices().size(), SIZE_MAX});

        vertex_offset += model.vertices().size();
        index_offset += model.indices().size();

        VkDeviceSize vertex_bytes =
            model.vertices().size() * sizeof(Model::Vertex);
        VkDeviceSize index_bytes = model.indices().size() * sizeof(uint32_t);

        staging_buffer_size =
            std::max(staging_buffer_size, std::max(vertex_bytes, index_bytes));

        unique_materials.insert(model.material());

        vertex_buffer_size += vertex_bytes;
        index_buffer_size += index_bytes;
    }

    std::unordered_set<std::string> unique_textures{};

    for (const auto& name : unique_materials) {
        const auto& material = resources.materials.at(name);
        for (const auto& name : material.textures()) {
            if (!name.empty()) {
                unique_textures.insert(name);
            }
        }
    }

    material_names.reserve(unique_materials.size());
    std::copy(std::make_move_iterator(unique_materials.begin()),
              std::make_move_iterator(unique_materials.end()),
              std::back_inserter(material_names));

    texture_names.reserve(unique_textures.size() + 1);
    std::copy(std::make_move_iterator(unique_textures.begin()),
              std::make_move_iterator(unique_textures.end()),
              std::back_inserter(texture_names));
    texture_names.emplace_back(""s);

    staging_buffer_size = std::max(
        staging_buffer_size, material_names.size() * sizeof(MaterialUniform));

    std::unordered_map<std::string, size_t> material_indices{};
    material_indices.reserve(material_names.size());
    for (size_t i{0}; i < material_names.size(); i++) {
        material_indices.emplace(material_names[i], i);
    }
    for (size_t i{0}; i < model_names.size(); i++) {
        const auto& model = resources.models.at(model_names[i]);
        model_offsets[i].material_index = material_indices.at(model.material());
    }
};

ResourcePack::Buffers ResourcePack::createBuffers(
    Device& device, VkMemoryRequirements& vertex_requirements,
    VkMemoryRequirements& index_requirements,
    VkMemoryRequirements& uniform_requirements,
    const std::vector<uint32_t>& queue_indices) {
    Buffer vertex(
        device, vertex_requirements.size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        queue_indices);

    Buffer index(
        device, index_requirements.size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        queue_indices);

    Buffer uniform(
        device, uniform_requirements.size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        queue_indices);

    vertex_requirements = vertex.memoryRequirements();
    index_requirements = index.memoryRequirements();
    uniform_requirements = uniform.memoryRequirements();

    return {std::move(vertex), std::move(index), std::move(uniform)};
}

std::vector<Image2D> ResourcePack::createTextureImages(
    Device& device, const std::vector<std::string>& texture_names,
    const Resources& resources, const std::vector<uint32_t>& queue_indices,
    std::vector<TextureRequirements>& requirements,
    VkDeviceSize& staging_buffer_size) {
    std::vector<Image2D> texture_images{};

    texture_images.reserve(texture_names.size());
    requirements.reserve(texture_names.size());

    for (size_t i{0}; i < texture_names.size() - 1; i++) {
        const auto& texture = resources.textures.at(texture_names[i]);
        texture_images.emplace_back(
            device, texture.width(), texture.height(), VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            queue_indices);
        requirements.emplace_back(i,
                                  texture_images.back().memoryRequirements());
        staging_buffer_size =
            std::max(staging_buffer_size, texture.data().size());
    }

    // EMPTY TEXTURE
    texture_images.emplace_back(
        device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        queue_indices);
    requirements.emplace_back(texture_names.size() - 1,
                              texture_images.back().memoryRequirements());
    staging_buffer_size = std::max(staging_buffer_size, (VkDeviceSize)4);

    std::sort(requirements.begin(), requirements.end(), [](auto& a, auto& b) {
        return a.second.alignment > b.second.alignment;
    });

    return texture_images;
}

VkDeviceMemory ResourcePack::allocateMemory(
    Device& device, const VkMemoryRequirements& vertex_requirements,
    const VkMemoryRequirements& index_requirements,
    const VkMemoryRequirements& uniform_requirements,
    const std::vector<TextureRequirements>& texture_requirements,
    Buffers& buffers, std::vector<Image2D>& images) {
    VkDeviceSize alloc_size{};
    uint32_t type_bits{UINT32_MAX};

    auto memoryOffset = [&](const VkMemoryRequirements& req) mutable {
        VkDeviceSize offset =
            (alloc_size + (req.alignment - 1)) / req.alignment * req.alignment;
        alloc_size = offset + req.size;
        type_bits &= req.memoryTypeBits;
        return offset;
    };

    VkDeviceSize vertex_offset = memoryOffset(vertex_requirements);
    VkDeviceSize index_offset = memoryOffset(index_requirements);
    VkDeviceSize uniform_offset = memoryOffset(uniform_requirements);

    std::vector<VkDeviceSize> texture_offsets(texture_requirements.size(), 0);
    for (size_t i{0}; i < texture_requirements.size(); i++) {
        texture_offsets[i] = memoryOffset(texture_requirements[i].second);
    }

    VkDeviceMemory memory{};
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = alloc_size;
    alloc_info.memoryTypeIndex =
        device.getMemoryIndex(type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(*device, &alloc_info, nullptr, &memory) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to allocate resource pack device memory");
    }

    buffers.vertex.bindMemory(memory, vertex_offset);
    buffers.index.bindMemory(memory, index_offset);
    buffers.uniform.bindMemory(memory, uniform_offset);

    for (size_t i{0}; i < texture_requirements.size(); i++) {
        auto index = texture_requirements[i].first;
        images[index].bindMemory(memory, texture_offsets[i]);
    }

    return memory;
}

void ResourcePack::copyResources(Context& context,
                                 VkDeviceSize staging_buffer_size,
                                 const std::vector<std::string>& model_names,
                                 const std::vector<ModelOffset>& model_offsets,
                                 const std::vector<std::string>& material_names,
                                 const std::vector<std::string>& texture_names,
                                 const Resources& resources, Buffers& buffers,
                                 std::vector<Image2D>& images) {
    StagingBuffer staging_buffer{context.device, staging_buffer_size};

    for (size_t i{0}; i < model_names.size(); i++) {
        const auto& name = model_names[i];
        const auto& model = resources.models.at(name);
        const auto& offsets = model_offsets[i];
        staging_buffer.copyBuffer(
            buffers.index, offsets.index_offset * sizeof(uint32_t),
            model.indices().data(), offsets.index_count * sizeof(uint32_t));
        staging_buffer.copyBuffer(
            buffers.vertex, offsets.vertex_offset * sizeof(Model::Vertex),
            model.vertices().data(),
            model.vertices().size() * sizeof(Model::Vertex));
    }

    std::vector<MaterialUniform> material_uniforms{material_names.size()};

    for (size_t i{0}; i < material_names.size(); i++) {
        const auto& material = resources.materials.at(material_names[i]);
        material_uniforms[i].diffuse = material.diffuse();
        material_uniforms[i].ambient = material.ambient();
        material_uniforms[i].emission = material.emission();
        material_uniforms[i].roughness = material.roughness();
        material_uniforms[i].metalness = material.metalness();
    }

    staging_buffer.copyBuffer(
        buffers.uniform, 0, material_uniforms.data(),
        material_uniforms.size() * sizeof(MaterialUniform));

    for (size_t i{0}; i < texture_names.size() - 1; i++) {
        auto& texture_name = texture_names[i];
        auto& texture = resources.textures.at(texture_name);
        staging_buffer.copyImage(images[i], texture.data());
    }
    staging_buffer.copyImage(images.back(), {0, 0, 0, 0});
}

std::vector<ImageView2D> ResourcePack::createTextureImageViews(
    Device& device, std::vector<Image2D>& images) {
    std::vector<ImageView2D> texture_views{};
    texture_views.reserve(images.size());
    for (auto& image : images) {
        texture_views.emplace_back(device, image, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    return texture_views;
}

void createNullTexture(Device& device) {}

ResourcePack::Materials ResourcePack::createMaterialDescriptors(
    Context& context, const std::vector<std::string>& material_names,
    const std::vector<std::string>& texture_names, const Resources& resources,
    Buffers& buffers, std::vector<ImageView2D>& texture_views) {
    size_t material_count = material_names.size();

    auto pool_sizes =
        MaterialUniform::requiredDescriptorPoolSize(material_count);
    VkDescriptorPoolCreateInfo pool_info{};

    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = material_count;

    VkDescriptorPool pool{};
    if (vkCreateDescriptorPool(*context.device, &pool_info, nullptr, &pool) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to create resource pack materials descriptor pool");
    }

    std::unordered_map<std::string, size_t> texture_indices{};
    texture_indices.reserve(texture_names.size());
    for (size_t i{0}; i < texture_names.size(); i++) {
        texture_indices.emplace(texture_names[i], i);
    }

    std::vector<VkDescriptorSetLayout> layouts(material_count,
                                               context.m_material_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = material_count;
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptor_sets(material_count);
    if (vkAllocateDescriptorSets(*context.device, &alloc_info,
                                 descriptor_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    std::array<VkDescriptorImageInfo, enum_count<Material::TextureMap>()>
        image_infos{};

    for (auto& info : image_infos) {
        info.sampler = *context.sampler(Sampler::Type::Linear);
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = *buffers.uniform;
    buffer_info.range = sizeof(MaterialUniform);

    std::array<VkWriteDescriptorSet, 2> write_info{};
    write_info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info[0].dstBinding = 0;
    write_info[0].descriptorCount = enum_count<Material::TextureMap>();
    write_info[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_info[0].pImageInfo = image_infos.data();

    write_info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info[1].descriptorCount = 1;
    write_info[1].dstBinding = enum_count<Material::TextureMap>();
    write_info[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_info[1].pBufferInfo = &buffer_info;

    for (size_t i{0}; i < material_count; i++) {
        const auto& material = resources.materials.at(material_names[i]);

        buffer_info.offset = i * sizeof(MaterialUniform);

        for (auto map : enum_values<Material::TextureMap>()) {
            image_infos[enum_integer(map)].imageView =
                *texture_views[texture_indices.at(
                    material.textures()[enum_integer(map)])];
        }

        write_info[0].dstSet = descriptor_sets[i];
        write_info[1].dstSet = descriptor_sets[i];

        vkUpdateDescriptorSets(*context.device, write_info.size(),
                               write_info.data(), 0, nullptr);
    };

    return {pool, std::move(descriptor_sets)};
}

}  // namespace vks