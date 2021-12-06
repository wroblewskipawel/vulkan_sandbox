#pragma once

#include <vector>

#include "resources.h"

namespace vks {

struct VertexAttribs {
    static VertexAttribs defaultAttributes() {
        std::vector<VkVertexInputBindingDescription> bindings(1);

        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(Model::Vertex);

        std::vector<VkVertexInputAttributeDescription> attributes(3);
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].location = 0;
        attributes[0].offset = offsetof(Model::Vertex, pos);

        attributes[1].binding = 0;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].location = 1;
        attributes[1].offset = offsetof(Model::Vertex, norm);

        attributes[2].binding = 0;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].location = 2;
        attributes[2].offset = offsetof(Model::Vertex, tex);

        return {bindings, attributes};
    }
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};
}  // namespace vks
