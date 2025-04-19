#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 cameraPosition;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform float screenWidth;
uniform float screenHeight;
uniform bool enableShadows;
uniform bool enableReflections;
uniform int maxBounces;
uniform float reflectivity;

// Scene objects
#define MAX_OBJECTS 16
#define OBJECT_TYPE_SPHERE 0
#define OBJECT_TYPE_CUBE 1
#define OBJECT_TYPE_MESH 2

struct Object {
    int type;
    vec3 position;
    vec3 size;        // radius for sphere, half-size for cube
    vec3 color;
    float reflectivity;
};

uniform Object objects[MAX_OBJECTS];
uniform int numObjects;

// Mesh data stored in texture
uniform sampler2D meshDataTexture;
uniform int numTriangles;
uniform int meshObjectIndex; // Index of the mesh object in the objects array
uniform int meshTextureSize;

// Light properties
#define MAX_LIGHTS 4
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

uniform Light lights[MAX_LIGHTS];
uniform int numLights;
uniform vec3 ambientLight;

// Ray structure
struct Ray {
    vec3 origin;
    vec3 direction;
};

// Triangle structure for Möller-Trumbore algorithm
struct Triangle {
    vec3 v0;
    vec3 v1;
    vec3 v2;
    vec3 normal;
};

// Hit information
struct HitInfo {
    bool hit;
    float t;
    vec3 position;
    vec3 normal;
    vec3 color;
    float reflectivity;
};

// Function to fetch triangle data from texture
Triangle getTriangleFromTexture(int triangleIndex) {
    Triangle tri;
    
    // Each triangle uses 3 texels (12 floats total)
    // Texture width is 4 texels, so each row is one triangle
    int textureWidth = 4;
    
    // Calculate texture coordinates
    int row = triangleIndex;
    
    // Read vertex 0 (first texel, xyz)
    vec4 texel0 = texelFetch(meshDataTexture, ivec2(0, row), 0);
    tri.v0 = texel0.xyz;
    
    // Read vertex 1 (first texel w component + second texel xy)
    vec4 texel1 = texelFetch(meshDataTexture, ivec2(1, row), 0);
    tri.v1 = vec3(texel0.w, texel1.xy);
    
    // Read vertex 2 (second texel zw + third texel x)
    vec4 texel2 = texelFetch(meshDataTexture, ivec2(2, row), 0);
    tri.v2 = vec3(texel1.zw, texel2.x);
    
    // Read normal (third texel yzw)
    tri.normal = vec3(texel2.yzw);
    
    return tri;
}

// Ray-Sphere intersection
bool intersectSphere(Ray ray, Object sphere, out HitInfo hitInfo) {
    vec3 oc = ray.origin - sphere.position;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.size.x * sphere.size.x;
    float discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0.0) {
        return false;
    }
    
    float t = (-b - sqrt(discriminant)) / (2.0 * a);
    if (t < 0.001) {
        t = (-b + sqrt(discriminant)) / (2.0 * a);
        if (t < 0.001) {
            return false;
        }
    }
    
    hitInfo.hit = true;
    hitInfo.t = t;
    hitInfo.position = ray.origin + t * ray.direction;
    hitInfo.normal = normalize(hitInfo.position - sphere.position);
    hitInfo.color = sphere.color;
    hitInfo.reflectivity = sphere.reflectivity;
    
    return true;
}

// Ray-AABB (cube) intersection
bool intersectCube(Ray ray, Object cube, out HitInfo hitInfo) {
    vec3 tMin = (cube.position - cube.size - ray.origin) / ray.direction;
    vec3 tMax = (cube.position + cube.size - ray.origin) / ray.direction;
    
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    
    if (tNear > tFar || tFar < 0.0) {
        return false;
    }
    
    float t = tNear > 0.001 ? tNear : tFar;
    if (t < 0.001) {
        return false;
    }
    
    hitInfo.hit = true;
    hitInfo.t = t;
    hitInfo.position = ray.origin + t * ray.direction;
    
    // Calculate normal based on which face was hit
    vec3 pc = hitInfo.position - cube.position;
    vec3 absPC = abs(pc);
    
    if (absPC.x > absPC.y && absPC.x > absPC.z) {
        hitInfo.normal = vec3(sign(pc.x), 0.0, 0.0);
    } else if (absPC.y > absPC.z) {
        hitInfo.normal = vec3(0.0, sign(pc.y), 0.0);
    } else {
        hitInfo.normal = vec3(0.0, 0.0, sign(pc.z));
    }
    
    hitInfo.color = cube.color;
    hitInfo.reflectivity = cube.reflectivity;
    
    return true;
}

// Ray-Triangle intersection using Möller-Trumbore algorithm
bool intersectTriangle(Ray ray, Triangle triangle, out HitInfo hitInfo) {
    const float EPSILON = 0.0000001;
    
    vec3 edge1 = triangle.v1 - triangle.v0;
    vec3 edge2 = triangle.v2 - triangle.v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    
    if (abs(a) < EPSILON)
        return false;    // Ray is parallel to triangle
    
    float f = 1.0 / a;
    vec3 s = ray.origin - triangle.v0;
    float u = f * dot(s, h);
    
    if (u < 0.0 || u > 1.0)
        return false;
    
    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    
    if (v < 0.0 || u + v > 1.0)
        return false;
    
    // At this stage, we can compute t to find out where the intersection point is on the line
    float t = f * dot(edge2, q);
    
    if (t > EPSILON) {
        hitInfo.hit = true;
        hitInfo.t = t;
        hitInfo.position = ray.origin + ray.direction * t;
        
        // Calculate the normal
        if (length(triangle.normal) > 0.0) {
            // Use the pre-computed normal if available
            hitInfo.normal = normalize(triangle.normal);
        } else {
            // Calculate normal from vertices
            hitInfo.normal = normalize(cross(edge1, edge2));
        }
        
        // Ensure normal faces the right direction
        if (dot(ray.direction, hitInfo.normal) > 0.0) {
            hitInfo.normal = -hitInfo.normal;
        }
        
        return true;
    }
    
    return false;
}

// Ray-Mesh intersection
bool intersectMesh(Ray ray, Object meshObj, out HitInfo hitInfo) {
    bool hit = false;
    hitInfo.hit = false;
    hitInfo.t = 1e30; // Large number
    
    // Apply mesh object position transformation to ray
    Ray localRay;
    localRay.origin = ray.origin - meshObj.position;
    localRay.direction = ray.direction;
    
    // Iterate over all triangles in the mesh
    for (int i = 0; i < numTriangles; i++) {
        // Get triangle data from texture
        Triangle tri = getTriangleFromTexture(i);
        
        HitInfo tempHitInfo;
        if (intersectTriangle(localRay, tri, tempHitInfo)) {
            if (tempHitInfo.t < hitInfo.t) {
                hitInfo = tempHitInfo;
                hitInfo.color = meshObj.color;
                hitInfo.reflectivity = meshObj.reflectivity;
                hit = true;
            }
        }
    }
    
    // Transform intersection point and normal back to world space
    if (hit) {
        hitInfo.position = hitInfo.position + meshObj.position;
    }
    
    return hit;
}

// Get the closest hit among all objects
bool traceRay(Ray ray, out HitInfo hitInfo, int skipObjectIndex) {
    hitInfo.hit = false;
    hitInfo.t = 1e30; // Large number
    
    for (int i = 0; i < numObjects; i++) {
        if (i == skipObjectIndex) continue;
        
        HitInfo tempHitInfo;
        bool hit = false;
        
        if (objects[i].type == OBJECT_TYPE_SPHERE) {
            hit = intersectSphere(ray, objects[i], tempHitInfo);
        } else if (objects[i].type == OBJECT_TYPE_CUBE) {
            hit = intersectCube(ray, objects[i], tempHitInfo);
        } else if (objects[i].type == OBJECT_TYPE_MESH && i == meshObjectIndex) {
            hit = intersectMesh(ray, objects[i], tempHitInfo);
        }
        
        if (hit && tempHitInfo.t < hitInfo.t) {
            hitInfo = tempHitInfo;
        }
    }
    
    return hitInfo.hit;
}

// Check if a point is in shadow
bool isInShadow(vec3 point, vec3 lightPosition) {
    vec3 lightDir = normalize(lightPosition - point);
    float lightDistance = length(lightPosition - point);
    
    Ray shadowRay;
    shadowRay.origin = point + 0.001 * lightDir; // Offset to avoid self-shadowing
    shadowRay.direction = lightDir;
    
    HitInfo shadowHit;
    if (traceRay(shadowRay, shadowHit, -1)) {
        return shadowHit.t < lightDistance;
    }
    
    return false;
}

// Main ray tracing function with reflections
vec3 traceScene(Ray primaryRay) {
    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    Ray currentRay = primaryRay;
    int bounceCount = 0;
    int lastHitObject = -1;
    
    while (bounceCount <= maxBounces) {
        HitInfo hitInfo;
        
        if (!traceRay(currentRay, hitInfo, lastHitObject)) {
            // Ray missed any object, return background color
            finalColor += throughput * ambientLight * 0.5;
            break;
        }
        
        // Calculate lighting (Phong model)
        vec3 ambient = ambientLight * hitInfo.color;
        vec3 diffuseAndSpecular = vec3(0.0);
        
        for (int i = 0; i < numLights; i++) {
            vec3 lightDir = normalize(lights[i].position - hitInfo.position);
            float diffFactor = max(dot(lightDir, hitInfo.normal), 0.0);
            
            // Shadow check
            bool shadowed = false;
            if (enableShadows) {
                shadowed = isInShadow(hitInfo.position, lights[i].position);
            }
            
            if (!shadowed) {
                // Diffuse component
                vec3 diffuse = diffFactor * lights[i].color * lights[i].intensity * hitInfo.color;
                
                // Specular component (Blinn-Phong)
                vec3 viewDir = normalize(cameraPosition - hitInfo.position);
                vec3 halfwayDir = normalize(lightDir + viewDir);
                float spec = pow(max(dot(hitInfo.normal, halfwayDir), 0.0), 32.0);
                vec3 specular = spec * lights[i].color * lights[i].intensity * 0.5;
                
                diffuseAndSpecular += diffuse + specular;
            }
        }
        
        // Add direct lighting
        finalColor += throughput * (ambient + diffuseAndSpecular);
        
        // Stop if we've reached max bounces or reflectivity is too low
        if (!enableReflections || bounceCount >= maxBounces || hitInfo.reflectivity < 0.01) {
            break;
        }
        
        // Set up reflection ray for next bounce
        currentRay.origin = hitInfo.position;
        currentRay.direction = reflect(currentRay.direction, hitInfo.normal);
        
        // Adjust throughput for next bounce based on reflectivity
        throughput *= hitInfo.reflectivity;
        bounceCount++;
    }
    
    return finalColor;
}

void main() {
    // Calculate the ray direction from camera through current fragment
    float aspectRatio = screenWidth / screenHeight;
    float fov = 45.0; // Match the perspective projection in the main program
    
    vec2 uv = TexCoords * 2.0 - 1.0;
    uv.x *= aspectRatio;
    
    // Create the ray
    Ray ray;
    ray.origin = cameraPosition;
    
    // Calculate the ray direction in world space
    float tanFov = tan(radians(fov) * 0.5);
    vec3 rayDir = normalize(vec3(uv.x * tanFov, uv.y * tanFov, -1.0));
    
    // Transform ray direction from camera space to world space
    mat4 invView = inverse(viewMatrix);
    vec4 worldRayDir = invView * vec4(rayDir, 0.0);
    ray.direction = normalize(worldRayDir.xyz);
    
    // Trace the ray and get the color
    vec3 color = traceScene(ray);
    
    // Apply gamma correction and output final color
    color = pow(color, vec3(1.0/2.2)); // Gamma correction
    FragColor = vec4(color, 1.0);
}
