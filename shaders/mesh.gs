#version 410 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec3 vertexPosition[];
out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 gWorld;
uniform mat4 gView;
uniform mat4 gProjection;

void main() {
    // Calculate face normal
    vec3 a = vertexPosition[1] - vertexPosition[0];
    vec3 b = vertexPosition[2] - vertexPosition[0];
    vec3 normal = normalize(cross(a, b));
    
    // Matrix for transforming normals
    mat3 normalMatrix = mat3(transpose(inverse(gWorld)));
    vec3 worldNormal = normalize(normalMatrix * normal);
    
    for (int i = 0; i < 3; i++) {
        vec4 worldPos = gWorld * vec4(vertexPosition[i], 1.0);
        gl_Position = gProjection * gView * worldPos;
        fragPosition = worldPos.xyz;
        fragNormal = worldNormal;
        EmitVertex();
    }
    
    EndPrimitive();
}
