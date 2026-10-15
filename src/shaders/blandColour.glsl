#version 450

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec2 v_texcoord;
layout (location = 3) in vec3 v_tangent;

layout(location = 0) out vec4 outColor;

void main() {
    //no texture, just colour
    outColor = v_color;
}