#version 460 core
#define VULKAN 100

layout(location=0) out vec4 frag_color;

layout(set=0, binding=0) uniform sampler2D diffuse_tex;
layout(set=0, binding=0) uniform sampler2D normal_tex;
layout(set=0, binding=0) uniform sampler2D metallic_tex;
layout(set=0, binding=0) uniform sampler2D roughness_tex;
layout(set=0, binding=0) uniform sampler2D ambient_tex;
layout(set=0, binding=0) uniform sampler2D emission_tex;

layout(set=0, binding=6) uniform Material {
    vec3 diffuse;
    vec3 ambient;
    vec3 emission;
    float roughness;
    float metalness;
} material;

layout(location=0) in VS_OUT {
    vec3 norm;
    vec2 tex;
} fs_in;

void main() {
    frag_color = texture(diffuse_tex, fs_in.tex);
}

