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
    // Sample albedo
    vec3 albedo = texture(colourMap, v_texcoord).rgb;

    // Sample tangent-space normal
    vec3 sampledNormal = texture(normalMap, v_texcoord).rgb;
    sampledNormal = sampledNormal * 2.0 - 1.0;

    // Construct TBN matrix
    vec3 N = normalize(v_normal);
    vec3 T = normalize(v_tangent - N * dot(v_tangent, N));
    vec3 B = normalize(cross(N, T));

    mat3 TBN = mat3(T, B, N);

    // Transform normal from tangent space to world/view space
    vec3 normal = normalize(TBN * sampledNormal);

    // Directional light
    vec3 lightDir = normalize(vec3(-0.35, 0.85, 0.45));

    float ambient = 0.25;
    float diffuse = max(dot(normal, lightDir), 0.0);

    vec3 lighting = vec3(ambient + diffuse * 0.75);

    FragColor = vec4(albedo * lighting, 1.0);
}