#pragma once

#include <tiny_obj_loader.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <magic_enum.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using namespace magic_enum;

namespace vks {

struct Resources;

class Texture {
   public:
    Texture(const std::filesystem::path& filepath);

    const std::vector<uint8_t> data() const { return m_data; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

   private:
    std::vector<uint8_t> m_data;
    uint32_t m_width;
    uint32_t m_height;
};

class Material {
   public:
    enum class TextureMap {
        Diffuse,
        Normal,
        Metallic,
        Roughness,
        Ambient,
        Emission,
    };

    Material(const std::filesystem::path& texture_root,
             const tinyobj::material_t& material);

    const std::array<std::string, enum_count<TextureMap>()>& textures() const {
        return m_textures;
    }
    const glm::vec3 diffuse() const { return m_diffuse; }
    const glm::vec3 ambient() const { return m_ambient; }
    const glm::vec3 emission() const { return m_emission; }
    float roughness() const { return m_roughness; }
    float metalness() const { return m_metalness; }

   private:
    friend class Model;

    std::array<std::string, enum_count<TextureMap>()> m_textures;
    glm::vec3 m_diffuse;
    glm::vec3 m_ambient;
    glm::vec3 m_emission;
    float m_roughness;
    float m_metalness;
};

class Model {
   public:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 norm;
        glm::vec2 tex;
    };

    static void load(const std::filesystem::path& filepath,
                     Resources& resources);

    Model(const std::string& material, std::vector<Vertex>&& vertices,
          std::vector<uint32_t>&& indices)
        : m_material{material}, m_vertices{vertices}, m_indices{indices} {}

    const std::string& material() const { return m_material; }
    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<uint32_t>& indices() const { return m_indices; }

   private:
    std::string m_material;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
};

struct Resources {
    std::unordered_map<std::string, Model> models;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Material> materials;
};

}  // namespace vks