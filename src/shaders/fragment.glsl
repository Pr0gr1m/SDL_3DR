//#version 460
//
//layout (location = 0) in vec4 v_color;
//layout (location = 1) in vec3 v_normal;
//layout (location = 2) in vec2 v_texcoord;
//layout (location = 3) in vec3 v_tangent;
//
//layout (set = 0, binding = 0) uniform sampler2D normalMap;
////layout (set = 0, binding = 1) uniform sampler2D colourMap;
//
//layout (location = 0) out vec4 FragColor;
//
//void main()
//{
//    //vec4 sampleColour = texture(colourMap, v_texcoord);
//
//    vec3 faceNormal = normalize(v_normal);
//    vec3 tangent = normalize(v_tangent - faceNormal * dot(v_tangent, faceNormal));
//    vec3 bitangent = normalize(cross(faceNormal, tangent));
//    mat3 tangentToWorld = mat3(tangent, bitangent, faceNormal);
//
//    vec3 sampledNormal = texture(normalMap, v_texcoord).rgb * 2.0 - 1.0;
//    vec3 normal = normalize(tangentToWorld * sampledNormal);
//
//    vec3 lightDir = normalize(vec3(-0.35, 0.85, 0.45));
//    float ambient = 0.25;
//    float diffuse = max(dot(normal, lightDir), 0.0);
//
//    vec3 litColor = v_color.rgb * (ambient + diffuse * 0.85);
//
//    FragColor = vec4(litColor.xyz, 1);
//
//    //    FragColor = vec4(sampleColour.xyz, 1);
//    //    FragColor = vec4(sampleColour.xyz, 1);
//    //    FragColor = vec4(colourTexture.xyz, );
//    //    FragColor = vec4(1, 0, 0, 1.0);   // green
//}

#version 460

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec2 v_texcoord;
layout (location = 3) in vec3 v_tangent;

layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 0) uniform sampler2D colourMap;

layout (location = 0) out vec4 FragColor;

void main()
{
    vec3 sampledNormal = texture(normalMap, v_texcoord).rgb * 2.0 - 1.0;
    vec3 sampledColour = texture(colourMap, v_texcoord).rgb * 2.0 - 1.0;

    FragColor = vec4(sampledNormal, v_color.a);

    vec3 faceNormal = normalize(v_normal);
    vec3 tangent = normalize(v_tangent - faceNormal * dot(v_tangent, faceNormal));
    vec3 bitangent = normalize(cross(faceNormal, tangent));
    mat3 tangentToWorld = mat3(tangent, bitangent, faceNormal);

    vec3 normal = normalize(tangentToWorld * sampledNormal);

    vec3 lightDir = normalize(vec3(-0.35, 0.85, 0.45));
    float ambient = 0.25;
    float diffuse = max(dot(normal, lightDir), 0.0);

    vec3 litColor = v_color.rgb * (ambient + diffuse * 0.85);
    //    FragColor = vec4(sampledNormal, v_color.a);
}