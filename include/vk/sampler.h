#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>

#include "device.h"

namespace vks {
class Sampler {
   public:
    enum class Type { Linear };

    Sampler(const Sampler&) = delete;
    Sampler(Sampler&& other)
        : device{other.device}, m_sampler{other.m_sampler} {
        other.m_sampler = VK_NULL_HANDLE;
    }

    Sampler& operator=(const Sampler&) = delete;
    Sampler& operator=(Sampler&&) = delete;

    ~Sampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(*device, m_sampler, nullptr);
        }
    }

   private:
    friend class ResourcePack;
    friend class Context;

    static VkSamplerCreateInfo info(Type type) {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.compareEnable = VK_FALSE;

        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        switch (type) {
            case Type::Linear:
                sampler_info.magFilter = VK_FILTER_LINEAR;
                sampler_info.minFilter = VK_FILTER_LINEAR;
                sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

                sampler_info.minLod = 0.0f;
                sampler_info.mipLodBias = 0.0f;
                sampler_info.maxLod = 0.0f;
                break;
            default:
                throw std::logic_error(
                    "Sampler type configuration not implemented");
        }
        return sampler_info;
    }

    Sampler(Device& device, Type type) : device{device} {
        auto sampler_info = info(type);
        sampler_info.maxAnisotropy =
            device.info().properties.limits.maxSamplerAnisotropy;
        if (vkCreateSampler(*device, &sampler_info, nullptr, &m_sampler) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create sampler");
        }
    }

    VkSampler operator*() { return m_sampler; }

    Device& device;
    VkSampler m_sampler;
};
}  // namespace vks