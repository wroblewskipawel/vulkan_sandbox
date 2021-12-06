#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "resources.h"

#include <stb_image.h>

#include <cstring>
#include <stdexcept>
#include <unordered_set>

using namespace std::string_literals;

namespace vks {
void Model::load(const std::filesystem::path& filepath, Resources& resources) {
    auto root_path = filepath.parent_path();
    tinyobj::ObjReaderConfig config{};
    config.triangulate = true;
    tinyobj::ObjReader reader{};

    if (!reader.ParseFromFile(filepath.string(), config)) {
        std::string error_message{"failed to load model at path: " +
                                  filepath.string()};
        if (!reader.Error().empty()) {
            error_message += '\n';
            error_message += reader.Error();
        }
        throw std::runtime_error(error_message);
    }

    auto& obj_attribs = reader.GetAttrib();
    auto& obj_materials = reader.GetMaterials();
    auto& obj_shapes = reader.GetShapes();

    std::vector<std::string> material_names{};
    for (size_t i{0}; i < obj_materials.size(); i++) {
        auto name = filepath.stem().string() + '.';
        if (obj_materials[i].name.empty()) {
            name += obj_materials[i].name;
        } else {
            name += "mat" + std::to_string(i);
        }
        auto [item, inserted] =
            resources.materials.try_emplace(name, root_path, obj_materials[i]);
        if (inserted) {
            for (auto& texture_name : item->second.m_textures) {
                if (!texture_name.empty()) {
                    resources.textures.try_emplace(texture_name, texture_name);
                }
            }
        }
        material_names.emplace_back(std::move(name));
    }

    for (size_t i{0}; i < obj_shapes.size(); i++) {
        auto& shape = obj_shapes[i];
        auto& mesh = shape.mesh;

        std::unordered_map<
            size_t, std::pair<std::vector<Vertex>, std::vector<uint32_t>>>
            data{};
        size_t index_offset{0};
        for (size_t f{0}; f < mesh.num_face_vertices.size(); f++) {
            auto& vertices = data[mesh.material_ids[f]].first;
            auto& indices = data[mesh.material_ids[f]].second;

            for (size_t v{0}; v < 3; v++) {
                Vertex vert{};
                tinyobj::index_t index = mesh.indices[index_offset + v];
                std::memcpy(
                    &vert.pos,
                    &obj_attribs.vertices[3 * size_t(index.vertex_index)],
                    sizeof(glm::vec3));
                if (index.normal_index >= 0) {
                    std::memcpy(
                        &vert.norm,
                        &obj_attribs.normals[3 * (size_t)index.normal_index],
                        sizeof(glm::vec3));
                }
                if (index.texcoord_index >= 0) {
                    std::memcpy(
                        &vert.tex,
                        &obj_attribs
                             .texcoords[2 * (size_t)index.texcoord_index],
                        sizeof(glm::vec2));
                }
                indices.push_back(vertices.size());
                vertices.push_back(vert);
            }
            index_offset += 3;
        }
        for (auto& [material_id, mesh_data] : data) {
            auto& material_name = material_names[material_id];
            auto model_name =
                filepath.stem().string() + '.' + shape.name + "." +
                std::string(material_name.begin() + material_name.find('.') + 1,
                            material_name.end());
            resources.models.try_emplace(std::move(model_name), material_name,
                                         std::move(mesh_data.first),
                                         std::move(mesh_data.second));
        }
    }
}

Material::Material(const std::filesystem::path& texture_root,
                   const tinyobj::material_t& material) {
    std::memcpy(&m_diffuse, material.diffuse, sizeof(glm::vec3));
    std::memcpy(&m_ambient, material.ambient, sizeof(glm::vec3));
    std::memcpy(&m_emission, material.emission, sizeof(glm::vec3));
    m_roughness = material.roughness;
    m_metalness = material.metallic;
    if (!material.diffuse_texname.empty()) {
        m_textures[enum_integer(TextureMap::Diffuse)] =
            (texture_root / material.diffuse_texname).string();
    }
    if (!material.normal_texname.empty()) {
        m_textures[enum_integer(TextureMap::Normal)] =
            (texture_root / material.normal_texname).string();
    }
    if (!material.metallic_texname.empty()) {
        m_textures[enum_integer(TextureMap::Metallic)] =
            (texture_root / material.metallic_texname).string();
    }
    if (!material.roughness_texname.empty()) {
        m_textures[enum_integer(TextureMap::Roughness)] =
            (texture_root / material.roughness_texname).string();
    }
    if (!material.ambient_texname.empty()) {
        m_textures[enum_integer(TextureMap::Ambient)] =
            (texture_root / material.ambient_texname).string();
    }
    if (!material.emissive_texname.empty()) {
        m_textures[enum_integer(TextureMap::Emission)] =
            (texture_root / material.emissive_texname).string();
    }
};

Texture::Texture(const std::filesystem::path& filepath) {
    int width{}, height{}, comp{};
    stbi_set_flip_vertically_on_load(true);
    auto* image_data =
        stbi_load(filepath.string().c_str(), &width, &height, &comp, 4);
    if (image_data) {
        m_width = width;
        m_height = height;
        m_data.resize(m_width * m_height * 4);
        std::memcpy(m_data.data(), image_data, m_data.size());
        stbi_image_free(image_data);
    } else {
        throw std::runtime_error("failed to load texture at: " +
                                 filepath.string());
    }
}

}  // namespace vks