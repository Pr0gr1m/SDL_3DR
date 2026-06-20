//#version 460
//
//layout(location = 0) in vec3 a_position;
//layout(location = 1) in vec4 a_color;
//layout(location = 2) in vec3 a_normal;
//layout(location = 3) in vec2 a_texcoord;
//layout(location = 4) in vec3 a_tangent;
//
//layout(row_major, set = 0, binding = 0) uniform Uniforms {
//    mat4 viewProjection;
//} ubo;
//
//layout(location = 0) out vec4 v_color;
//layout(location = 1) out vec3 v_normal;
//layout(location = 2) out vec2 v_texcoord;
//layout(location = 3) out vec3 v_tangent;
//
//void main()
//{
//    mat4 view = mat4(
//            1.0, 0.0, 0.0, 0.0,
//            0.0, 1.0, 0.0, -15.0,
//            0.0, 0.0, 1.0, -15.0,
//            0.0, 0.0, 0.0, 1.0
//    );
//
//    float f = 1.0 / tan(radians(60.0) / 2.0);  // ≈ 1.73205
//    float aspect = 1600.0 / 900.0;              // ≈ 1.77778
//    float near = 0.1;
//    float far = 100;
//
//    mat4 proj = mat4(
//            f / aspect, 0.0, 0.0, 0.0,
//            0.0, f, 0.0, 0.0,
//            0.0, 0.0, far / (near - far), -(far * near) / (far - near),
//            0.0, 0.0, -1.0, 0.0
//    );
//
//    mat4 viewProj = mat4(
//            0.974, 0.000, 0.000, 0.000,
//            0.000, 1.732, 0.000, 0.000,
//            0.000, 0.000, -1.001, -2.102,
//            0.000, 0.000, -1.000, 2.000
//    );
//
//    //    mat4 viewProj = proj * view;
//
//    gl_Position = vec4(a_position * 0.05, 1.0);
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
    mat4 identity = mat4(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
    );

    float fovD = radians(60);
    float f = 1.0 / tan(radians(60.0) / 2.0);  // ≈ 1.73205
    float aspect = 1600.0 / 900.0;              // ≈ 1.77778
    float near = 0.1;
    float far = 100;

    mat4 proj = mat4(
            1 / (aspect * tan(fovD / 2)), 0.000, 0.000, 0.000,
            0.000, 1 / (tan(fovD / 2)), 0.000, 0.000,
            0.000, 0.000, -1.001, -2.102,
            0.000, 0.000, -1.000, 2.000
    );

    vec3 position = vec3(0, 1, 0);
    vec3 right = vec3(1, 0, 0);
    vec3 up = vec3(0, 1, 0);
    vec3 forward = vec3(0, 0, 1);

    mat4 view = mat4(
            right.x, right.y, right.z, -dot(right, position),
            up.x, up.y, up.z, -dot(up, position),
            -forward.x, -forward.y, -forward.z, dot(forward, position),
            0, 0, 0, 1
    );

    mat4 viewProj = proj * view;

    gl_Position = ubo.viewProjection * vec4(a_position, 1.0);
    v_color = a_color;
    v_normal = a_normal;
    v_texcoord = a_texcoord;
    v_tangent = a_tangent;
}