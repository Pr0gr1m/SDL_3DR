#version 460
layout (location = 0) in vec4 v_color;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec2 v_texcoord;
layout (location = 3) in vec3 v_tangent;

layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2D colourMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;

void main()
{
    FragColor = texture(colourMap, v_texcoord);
}