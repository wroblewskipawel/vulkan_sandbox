#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <magic_enum.hpp>
#include <stdexcept>

#include "resources.h"
#include "vk/device.h"

using namespace magic_enum;

namespace vks {
struct MaterialUniform {
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 emission;
    alignas(4) float roughness;
    alignas(4) float metalness;
    alignas(4) float pad[2];  // structure size multiple of 16 bytes

    static std::vector<VkDescriptorPoolSize> requiredDescriptorPoolSize(
        size_t count) {
        std::vector<VkDescriptorPoolSize> pool_sizes(2);

        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = count;

        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount =
            count * enum_count<Material::TextureMap>();

        return pool_sizes;
    }

    static VkDescriptorSetLayout descriptorLayout(Device& device) {
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        std::array<VkDescriptorSetLayoutBinding,
                   enum_count<Material::TextureMap>() + 1>
            bindings{};

        for (size_t i{0}; i < enum_count<Material::TextureMap>(); i++) {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        bindings.back().binding = bindings.size() - 1;
        bindings.back().descriptorCount = 1;
        bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings.back().stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        layout_info.bindingCount = bindings.size();
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout layout;
        if (vkCreateDescriptorSetLayout(*device, &layout_info, nullptr,
                                        &layout) != VK_SUCCESS) {
            throw std::runtime_error(
                "Failed to create material descriptor set layout");
        }
        return layout;
    }
};
}  // namespace vks