#version 460 core
#define VULKAN 100

layout(location=0) in vec3 pos;
layout(location=1) in vec3 norm;
layout(location=2) in vec2 tex;

layout(push_constant) uniform Model {
    mat4 camera;
    mat4 model;
} model_transform;

layout(location=0) out VS_OUT {
    vec3 norm;
    vec2 tex;
} vs_out;

void main() {
    vs_out.norm = norm;
    vs_out.tex = tex;
    gl_Position = model_transform.camera * model_transform.model * vec4(pos, 1.0);
}