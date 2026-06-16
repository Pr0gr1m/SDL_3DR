//#version 460
//
//layout (location = 0) in vec3 a_position;
//layout (location = 1) in vec4 a_color;
//layout (location = 2) in vec3 a_normal;
//layout (location = 3) in vec2 a_texcoord;
//layout (location = 4) in vec3 a_tangent;
//
//layout (row_major, set = 1, binding = 0) uniform Uniforms {
//    mat4 viewProjection;
//} ubo;
//
//layout (location = 0) out vec4 v_color;
//layout (location = 1) out vec3 v_normal;
//layout (location = 2) out vec2 v_texcoord;
//layout (location = 3) out vec3 v_tangent;
//
//void main()
//{
//    gl_Position = ubo.viewProjection * vec4(a_position, 1.0);
//    v_color = a_color;
//    v_normal = a_normal;
//    v_texcoord = a_texcoord;
//    v_tangent = a_tangent;
//}

#version 460

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec4 a_color;
layout (location = 2) in vec3 a_normal;
layout (location = 3) in vec2 a_texcoord;
layout (location = 4) in vec3 a_tangent;

layout (row_major, set = 1, binding = 0) uniform Uniforms {
    mat4 viewProjection;
} ubo;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec2 v_texcoord;
layout (location = 3) out vec3 v_tangent;

void main()
{
    gl_Position = ubo.viewProjection * vec4(a_position, 1.0);
    v_color = a_color;
    v_normal = a_normal;
    v_texcoord = a_texcoord;
    v_tangent = a_tangent;
}