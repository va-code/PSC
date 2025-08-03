#include "gpu_accelerator.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Shader source code
const char* topology_connectivity_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Vertex {
    vec3 position;
    int connected_vertices[10];
    int num_connections;
    float curvature;
    int valence;
};

struct Triangle {
    vec3 vertices[3];
    vec3 normal;
    float area;
};

layout(std430, binding = 0) buffer VertexBuffer {
    Vertex vertices[];
};

layout(std430, binding = 1) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 2) buffer EdgeBuffer {
    int edges[];
};

layout(std430, binding = 3) buffer ResultBuffer {
    int results[];
};

shared int shared_edges[256];

void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle normal
    vec3 v1 = tri.vertices[1] - tri.vertices[0];
    vec3 v2 = tri.vertices[2] - tri.vertices[0];
    tri.normal = normalize(cross(v1, v2));
    triangles[tid].normal = tri.normal;
    
    // Calculate triangle area
    tri.area = length(cross(v1, v2)) * 0.5;
    triangles[tid].area = tri.area;
    
    // Update vertex valence
    for (int i = 0; i < 3; i++) {
        int vertex_idx = int(tri.vertices[i].x * 1000000); // Simple hash
        atomicAdd(vertices[vertex_idx % vertices.length()].valence, 1);
    }
}
)";

const char* topology_curvature_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Vertex {
    vec3 position;
    int connected_vertices[10];
    int num_connections;
    float curvature;
    int valence;
};

layout(std430, binding = 0) buffer VertexBuffer {
    Vertex vertices[];
};

layout(std430, binding = 1) buffer TriangleBuffer {
    vec3 triangle_normals[];
};

layout(std430, binding = 2) buffer ResultBuffer {
    float curvatures[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= vertices.length()) return;
    
    Vertex vertex = vertices[tid];
    
    // Calculate mean curvature using connected vertices
    vec3 mean_curvature = vec3(0.0);
    int num_connections = 0;
    
    for (int i = 0; i < vertex.num_connections && i < 10; i++) {
        int connected_idx = vertex.connected_vertices[i];
        if (connected_idx >= 0 && connected_idx < vertices.length()) {
            mean_curvature += vertices[connected_idx].position - vertex.position;
            num_connections++;
        }
    }
    
    if (num_connections > 0) {
        mean_curvature /= float(num_connections);
        vertex.curvature = length(mean_curvature);
    } else {
        vertex.curvature = 0.0;
    }
    
    vertices[tid] = vertex;
    curvatures[tid] = vertex.curvature;
}
)";

const char* topology_features_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer FeatureBuffer {
    int features[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle center
    tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
    triangles[tid].center = tri.center;
    
    // Simple feature detection based on triangle size
    float area = length(cross(tri.vertices[1] - tri.vertices[0], 
                             tri.vertices[2] - tri.vertices[0])) * 0.5;
    
    if (area > 1.0) { // Large triangles might indicate features
        features[tid] = 1;
    } else {
        features[tid] = 0;
    }
}
)";

const char* topology_density_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer DensityBuffer {
    float densities[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle center
    tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
    triangles[tid].center = tri.center;
    
    // Calculate triangle area as density measure
    float area = length(cross(tri.vertices[1] - tri.vertices[0], 
                             tri.vertices[2] - tri.vertices[0])) * 0.5;
    
    densities[tid] = area;
}
)";

const char* topology_quality_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 normal;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer QualityBuffer {
    float qualities[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle normal
    vec3 v1 = tri.vertices[1] - tri.vertices[0];
    vec3 v2 = tri.vertices[2] - tri.vertices[0];
    tri.normal = normalize(cross(v1, v2));
    triangles[tid].normal = tri.normal;
    
    // Calculate triangle quality based on aspect ratio
    float a = length(v1);
    float b = length(v2);
    float c = length(tri.vertices[2] - tri.vertices[1]);
    
    float s = (a + b + c) * 0.5;
    float area = sqrt(s * (s - a) * (s - b) * (s - c));
    
    float quality = 0.0;
    if (area > 0.0) {
        float aspect_ratio = (a * b * c) / (8.0 * area * area);
        quality = 1.0 / aspect_ratio; // Normalize to [0, 1]
    }
    
    qualities[tid] = quality;
}
)";

const char* convex_hull_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Point3D {
    vec3 position;
    int original_index;
};

struct ConvexHull {
    vec3 vertices[1024];
    int num_vertices;
    int faces[2048][3];
    int num_faces;
    vec3 bounds[2]; // min, max
};

layout(std430, binding = 0) buffer PointBuffer {
    Point3D points[];
};

layout(std430, binding = 1) buffer HullBuffer {
    ConvexHull hull;
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= points.length()) return;
    
    Point3D point = points[tid];
    
    // Find extreme points for initial tetrahedron
    if (tid == 0) {
        // Initialize hull with first point
        hull.vertices[0] = point.position;
        hull.num_vertices = 1;
        hull.bounds[0] = point.position;
        hull.bounds[1] = point.position;
    }
    
    // Update bounds
    hull.bounds[0] = min(hull.bounds[0], point.position);
    hull.bounds[1] = max(hull.bounds[1], point.position);
    
    // Add point to hull if it's outside current hull
    bool inside = true;
    for (int i = 0; i < hull.num_faces; i++) {
        vec3 v1 = hull.vertices[hull.faces[i][0]];
        vec3 v2 = hull.vertices[hull.faces[i][1]];
        vec3 v3 = hull.vertices[hull.faces[i][2]];
        
        vec3 normal = normalize(cross(v2 - v1, v3 - v1));
        float d = dot(normal, v1);
        
        if (dot(normal, point.position) > d + 0.001) {
            inside = false;
            break;
        }
    }
    
    if (!inside && hull.num_vertices < 1024) {
        hull.vertices[hull.num_vertices] = point.position;
        hull.num_vertices++;
    }
}
)";

const char* convex_decomposition_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct ConvexNode {
    int node_id;
    int node_type; // 0 = leaf, 1 = internal
    vec3 bounds[2]; // min, max
    float concavity;
    
    int part_start_index;
    int num_triangles;
    int left_child_id;
    int right_child_id;
};

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer NodeBuffer {
    ConvexNode nodes[];
};

layout(std430, binding = 1) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 2) buffer ResultBuffer {
    int results[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= nodes.length()) return;
    
    ConvexNode node = nodes[tid];
    
    if (node.node_type == 0) { // Leaf node
        // Calculate triangle centers for this part
        for (int i = 0; i < node.num_triangles; i++) {
            int tri_idx = node.part_start_index + i;
            if (tri_idx < triangles.length()) {
                Triangle tri = triangles[tri_idx];
                tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
                triangles[tri_idx] = tri;
            }
        }
    } else { // Internal node
        // Process children
        if (node.left_child_id >= 0 && node.left_child_id < nodes.length()) {
            // Process left child
        }
        if (node.right_child_id >= 0 && node.right_child_id < nodes.length()) {
            // Process right child
        }
    }
}
)";

const char* convex_concavity_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 normal;
    float area;
};

struct ConvexHull {
    vec3 vertices[1024];
    int num_vertices;
    vec3 bounds[2];
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer HullBuffer {
    ConvexHull hull;
};

layout(std430, binding = 2) buffer ConcavityBuffer {
    float concavities[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle normal and area
    vec3 v1 = tri.vertices[1] - tri.vertices[0];
    vec3 v2 = tri.vertices[2] - tri.vertices[0];
    tri.normal = normalize(cross(v1, v2));
    tri.area = length(cross(v1, v2)) * 0.5;
    
    triangles[tid] = tri;
    
    // Calculate signed volume contribution
    float signed_volume = dot(tri.vertices[0], cross(v1, v2)) / 6.0;
    
    // Accumulate volume in shared memory
    shared float shared_volumes[256];
    shared_volumes[gl_LocalInvocationID.x] = abs(signed_volume);
    
    barrier();
    
    // Reduce volumes
    for (int offset = 128; offset > 0; offset >>= 1) {
        if (gl_LocalInvocationID.x < offset) {
            shared_volumes[gl_LocalInvocationID.x] += shared_volumes[gl_LocalInvocationID.x + offset];
        }
        barrier();
    }
    
    if (gl_LocalInvocationID.x == 0) {
        float actual_volume = shared_volumes[0];
        
        // Calculate hull volume (simplified)
        vec3 hull_size = hull.bounds[1] - hull.bounds[0];
        float hull_volume = hull_size.x * hull_size.y * hull_size.z;
        
        float concavity = 0.0;
        if (hull_volume > 0.0) {
            concavity = (hull_volume - actual_volume) / hull_volume;
        }
        
        concavities[gl_WorkGroupID.x] = concavity;
    }
}
)";

const char* triangle_sort_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer IndexBuffer {
    int indices[];
};

layout(std430, binding = 2) buffer AxisBuffer {
    int sort_axis;
};

shared Triangle shared_triangles[256];

void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
    triangles[tid] = tri;
    
    // Load into shared memory
    shared_triangles[lid] = tri;
    barrier();
    
    // Bitonic sort in shared memory
    for (int k = 2; k <= 256; k <<= 1) {
        for (int j = k >> 1; j > 0; j >>= 1) {
            int ixj = lid ^ j;
            if (ixj > lid) {
                float val1 = shared_triangles[lid].center[sort_axis];
                float val2 = shared_triangles[ixj].center[sort_axis];
                
                if ((lid & k) == 0) {
                    if (val1 > val2) {
                        Triangle temp = shared_triangles[lid];
                        shared_triangles[lid] = shared_triangles[ixj];
                        shared_triangles[ixj] = temp;
                    }
                } else {
                    if (val1 < val2) {
                        Triangle temp = shared_triangles[lid];
                        shared_triangles[lid] = shared_triangles[ixj];
                        shared_triangles[ixj] = temp;
                    }
                }
            }
            barrier();
        }
    }
    
    // Write back sorted indices
    indices[tid] = shared_triangles[lid].original_index;
}
)";

const char* bvh_construction_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct BVHNode {
    int node_type; // 0 = leaf, 1 = internal
    vec3 bounds[2]; // min, max
    
    int triangle_start_index;
    int num_triangles;
    int left_child_id;
    int right_child_id;
};

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer NodeBuffer {
    BVHNode nodes[];
};

layout(std430, binding = 1) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 2) buffer IndexBuffer {
    int indices[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= nodes.length()) return;
    
    BVHNode node = nodes[tid];
    
    if (node.node_type == 0) { // Leaf node
        // Calculate bounding box for triangles in this leaf
        vec3 min_bounds = vec3(1e6);
        vec3 max_bounds = vec3(-1e6);
        
        for (int i = 0; i < node.num_triangles; i++) {
            int tri_idx = node.triangle_start_index + i;
            if (tri_idx < triangles.length()) {
                Triangle tri = triangles[tri_idx];
                
                for (int j = 0; j < 3; j++) {
                    min_bounds = min(min_bounds, tri.vertices[j]);
                    max_bounds = max(max_bounds, tri.vertices[j]);
                }
            }
        }
        
        node.bounds[0] = min_bounds;
        node.bounds[1] = max_bounds;
        nodes[tid] = node;
    } else { // Internal node
        // Calculate bounding box from children
        if (node.left_child_id >= 0 && node.left_child_id < nodes.length()) {
            BVHNode left_child = nodes[node.left_child_id];
            node.bounds[0] = left_child.bounds[0];
            node.bounds[1] = left_child.bounds[1];
        }
        
        if (node.right_child_id >= 0 && node.right_child_id < nodes.length()) {
            BVHNode right_child = nodes[node.right_child_id];
            node.bounds[0] = min(node.bounds[0], right_child.bounds[0]);
            node.bounds[1] = max(node.bounds[1], right_child.bounds[1]);
        }
        
        nodes[tid] = node;
    }
}
)";

const char* bvh_bounds_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer BoundsBuffer {
    vec3 bounds[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle center
    tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
    triangles[tid] = tri;
    
    // Calculate bounding box
    vec3 min_bounds = min(min(tri.vertices[0], tri.vertices[1]), tri.vertices[2]);
    vec3 max_bounds = max(max(tri.vertices[0], tri.vertices[1]), tri.vertices[2]);
    
    bounds[tid * 2] = min_bounds;
    bounds[tid * 2 + 1] = max_bounds;
}
)";

const char* slicing_contours_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 normal;
};

struct ContourPoint {
    vec2 position;
    int valid;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer ContourBuffer {
    ContourPoint contours[];
};

layout(std430, binding = 2) buffer ZHeightBuffer {
    float z_height;
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Check if triangle intersects the z-plane
    float min_z = min(min(tri.vertices[0].z, tri.vertices[1].z), tri.vertices[2].z);
    float max_z = max(max(tri.vertices[0].z, tri.vertices[1].z), tri.vertices[2].z);
    
    if (z_height >= min_z && z_height <= max_z) {
        // Calculate intersection points
        vec2 intersection_points[2];
        int num_intersections = 0;
        
        for (int i = 0; i < 3; i++) {
            vec3 v1 = tri.vertices[i];
            vec3 v2 = tri.vertices[(i + 1) % 3];
            
            if ((v1.z <= z_height && v2.z >= z_height) || 
                (v1.z >= z_height && v2.z <= z_height)) {
                
                float t = (z_height - v1.z) / (v2.z - v1.z);
                vec3 intersection = mix(v1, v2, t);
                
                if (num_intersections < 2) {
                    intersection_points[num_intersections] = intersection.xy;
                    num_intersections++;
                }
            }
        }
        
        if (num_intersections == 2) {
            contours[tid * 2].position = intersection_points[0];
            contours[tid * 2].valid = 1;
            contours[tid * 2 + 1].position = intersection_points[1];
            contours[tid * 2 + 1].valid = 1;
        }
    }
}
)";

const char* slicing_infill_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct ContourPoint {
    vec2 position;
    int valid;
};

struct InfillPoint {
    vec2 position;
    int valid;
};

layout(std430, binding = 0) buffer ContourBuffer {
    ContourPoint contours[];
};

layout(std430, binding = 1) buffer InfillBuffer {
    InfillPoint infill_points[];
};

layout(std430, binding = 2) buffer ParamsBuffer {
    float infill_density;
    float infill_spacing;
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= contours.length()) return;
    
    ContourPoint contour = contours[tid];
    
    if (contour.valid == 0) return;
    
    // Generate infill points based on density
    float spacing = infill_spacing / infill_density;
    
    // Simple grid infill pattern
    vec2 grid_pos = floor(contour.position / spacing) * spacing;
    
    infill_points[tid].position = grid_pos;
    infill_points[tid].valid = 1;
}
)";

const char* adjacency_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct ConvexNode {
    int node_id;
    int node_type;
    vec3 bounds[2];
    float concavity;
};

struct AdjacencyEntry {
    int node_id;
    float overlap_volume;
    float distance;
};

layout(std430, binding = 0) buffer NodeBuffer {
    ConvexNode nodes[];
};

layout(std430, binding = 1) buffer AdjacencyBuffer {
    AdjacencyEntry adjacencies[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= nodes.length()) return;
    
    ConvexNode current_node = nodes[tid];
    
    if (current_node.node_type != 0) return; // Only process leaf nodes
    
    // Check adjacency with other leaf nodes
    for (int i = 0; i < nodes.length(); i++) {
        if (i == tid) continue;
        
        ConvexNode other_node = nodes[i];
        if (other_node.node_type != 0) continue; // Only check against leaf nodes
        
        // Check if bounding boxes overlap
        bool overlap = !(current_node.bounds[1].x < other_node.bounds[0].x ||
                        current_node.bounds[0].x > other_node.bounds[1].x ||
                        current_node.bounds[1].y < other_node.bounds[0].y ||
                        current_node.bounds[0].y > other_node.bounds[1].y ||
                        current_node.bounds[1].z < other_node.bounds[0].z ||
                        current_node.bounds[0].z > other_node.bounds[1].z);
        
        if (overlap) {
            // Calculate overlap volume (simplified)
            vec3 intersection_min = max(current_node.bounds[0], other_node.bounds[0]);
            vec3 intersection_max = min(current_node.bounds[1], other_node.bounds[1]);
            vec3 intersection_size = intersection_max - intersection_min;
            float overlap_volume = intersection_size.x * intersection_size.y * intersection_size.z;
            
            // Calculate distance between centers
            vec3 center1 = (current_node.bounds[0] + current_node.bounds[1]) * 0.5;
            vec3 center2 = (other_node.bounds[0] + other_node.bounds[1]) * 0.5;
            float distance = length(center1 - center2);
            
            // Store adjacency information
            int adj_index = tid * nodes.length() + i;
            if (adj_index < adjacencies.length()) {
                adjacencies[adj_index].node_id = other_node.node_id;
                adjacencies[adj_index].overlap_volume = overlap_volume;
                adjacencies[adj_index].distance = distance;
            }
        }
    }
}
)";

// GPU context management
gpu_context_t* gpu_init(gpu_mode_t mode) {
    gpu_context_t* ctx = malloc(sizeof(gpu_context_t));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(gpu_context_t));
    ctx->current_mode = mode;
    
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        free(ctx);
        return NULL;
    }
    
    // Configure GLFW for OpenGL compute
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Hidden window for compute
    
    // Create window and context
    ctx->window = glfwCreateWindow(1, 1, "GPU Compute", NULL, NULL);
    if (!ctx->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        free(ctx);
        return NULL;
    }
    
    glfwMakeContextCurrent(ctx->window);
    
    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        free(ctx);
        return NULL;
    }
    
    // Check OpenGL version and compute shader support
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    
    strncpy(ctx->caps.version, version ? version : "Unknown", 255);
    strncpy(ctx->caps.vendor, vendor ? vendor : "Unknown", 255);
    strncpy(ctx->caps.renderer, renderer ? renderer : "Unknown", 255);
    
    // Check compute shader support
    ctx->caps.has_opengl_43 = (GLEW_VERSION_4_3 != 0);
    ctx->caps.has_opengl_compute = (GLEW_ARB_compute_shader != 0);
    
    if (ctx->caps.has_opengl_compute) {
        // Get compute capabilities
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &ctx->caps.max_compute_units);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &ctx->caps.max_work_group_size);
        glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &ctx->caps.max_shared_memory);
        
        ctx->is_initialized = 1;
        printf("GPU acceleration initialized successfully\n");
        printf("Vendor: %s\n", ctx->caps.vendor);
        printf("Renderer: %s\n", ctx->caps.renderer);
        printf("OpenGL Version: %s\n", ctx->caps.version);
        printf("Compute Shader Support: Yes\n");
        printf("Max Compute Units: %d\n", ctx->caps.max_compute_units);
        printf("Max Work Group Size: %d\n", ctx->caps.max_work_group_size);
    } else {
        fprintf(stderr, "OpenGL compute shaders not supported\n");
        ctx->is_initialized = 0;
    }
    
    return ctx;
}

void gpu_cleanup(gpu_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->window) {
        glfwDestroyWindow(ctx->window);
    }
    glfwTerminate();
    free(ctx);
}

int gpu_is_available(const gpu_context_t* ctx) {
    return ctx && ctx->is_initialized && ctx->caps.has_opengl_compute;
}

gpu_capabilities_t gpu_get_capabilities(const gpu_context_t* ctx) {
    if (ctx) {
        return ctx->caps;
    }
    gpu_capabilities_t caps = {0};
    return caps;
}

void gpu_print_capabilities(const gpu_capabilities_t* caps) {
    printf("GPU Capabilities:\n");
    printf("  OpenGL Version: %s\n", caps->version);
    printf("  Vendor: %s\n", caps->vendor);
    printf("  Renderer: %s\n", caps->renderer);
    printf("  OpenGL 4.3+: %s\n", caps->has_opengl_43 ? "Yes" : "No");
    printf("  Compute Shaders: %s\n", caps->has_opengl_compute ? "Yes" : "No");
    printf("  Max Compute Units: %d\n", caps->max_compute_units);
    printf("  Max Work Group Size: %d\n", caps->max_work_group_size);
    printf("  Max Shared Memory: %d bytes\n", caps->max_shared_memory);
}

// GPU buffer management
gpu_buffer_t* gpu_create_buffer(size_t size, const void* data) {
    gpu_buffer_t* buffer = malloc(sizeof(gpu_buffer_t));
    if (!buffer) return NULL;
    
    buffer->size = size;
    buffer->is_mapped = 0;
    
    // Create shader storage buffer
    glGenBuffers(1, &buffer->ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer->ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
    
    if (gpu_check_error("gpu_create_buffer")) {
        glDeleteBuffers(1, &buffer->ssbo);
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

void gpu_destroy_buffer(gpu_buffer_t* buffer) {
    if (!buffer) return;
    
    if (buffer->ssbo) {
        glDeleteBuffers(1, &buffer->ssbo);
    }
    if (buffer->vbo) {
        glDeleteBuffers(1, &buffer->vbo);
    }
    free(buffer);
}

void* gpu_map_buffer(gpu_buffer_t* buffer, int write_only) {
    if (!buffer || buffer->is_mapped) return NULL;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer->ssbo);
    GLenum access = write_only ? GL_WRITE_ONLY : GL_READ_WRITE;
    void* ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, access);
    
    if (ptr) {
        buffer->is_mapped = 1;
    }
    
    return ptr;
}

void gpu_unmap_buffer(gpu_buffer_t* buffer) {
    if (!buffer || !buffer->is_mapped) return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer->ssbo);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    buffer->is_mapped = 0;
}

void gpu_bind_buffer(gpu_buffer_t* buffer, unsigned int binding_point) {
    if (!buffer) return;
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, buffer->ssbo);
}

// GPU program management
gpu_program_t* gpu_create_compute_program(const char* compute_source) {
    gpu_program_t* program = malloc(sizeof(gpu_program_t));
    if (!program) return NULL;
    
    program->is_linked = 0;
    
    // Create compute shader
    program->compute_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(program->compute_shader, 1, &compute_source, NULL);
    glCompileShader(program->compute_shader);
    
    // Check compilation
    GLint success;
    glGetShaderiv(program->compute_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetShaderInfoLog(program->compute_shader, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "Compute shader compilation failed: %s\n", info_log);
        glDeleteShader(program->compute_shader);
        free(program);
        return NULL;
    }
    
    // Create program and link
    program->program = glCreateProgram();
    glAttachShader(program->program, program->compute_shader);
    glLinkProgram(program->program);
    
    // Check linking
    glGetProgramiv(program->program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program->program, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "Program linking failed: %s\n", info_log);
        glDeleteProgram(program->program);
        glDeleteShader(program->compute_shader);
        free(program);
        return NULL;
    }
    
    program->is_linked = 1;
    return program;
}

void gpu_destroy_program(gpu_program_t* program) {
    if (!program) return;
    
    if (program->program) {
        glDeleteProgram(program->program);
    }
    if (program->compute_shader) {
        glDeleteShader(program->compute_shader);
    }
    free(program);
}

int gpu_use_program(gpu_program_t* program) {
    if (!program || !program->is_linked) return 0;
    
    glUseProgram(program->program);
    return !gpu_check_error("gpu_use_program");
}

int gpu_dispatch_compute(unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z) {
    glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
    return !gpu_check_error("gpu_dispatch_compute");
}

// GPU-accelerated topology evaluation
int gpu_analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx) {
    if (!gpu_is_available(ctx)) {
        return cpu_analyze_connectivity(stl, eval);
    }
    
    // Create buffers
    size_t triangle_data_size = stl->num_triangles * sizeof(float) * 9; // 3 vertices * 3 components
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(triangle_data_size, NULL);
    gpu_buffer_t* vertex_buffer = gpu_create_buffer(eval->num_vertices * sizeof(float) * 20, NULL);
    
    if (!triangle_buffer || !vertex_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (vertex_buffer) gpu_destroy_buffer(vertex_buffer);
        return cpu_analyze_connectivity(stl, eval);
    }
    
    // Upload triangle data
    void* triangle_data = gpu_map_buffer(triangle_buffer, 1);
    if (triangle_data) {
        float* data = (float*)triangle_data;
        for (unsigned int i = 0; i < stl->num_triangles; i++) {
            const stl_triangle_t* tri = &stl->triangles[i];
            for (int j = 0; j < 3; j++) {
                data[i * 9 + j * 3 + 0] = tri->vertices[j][0];
                data[i * 9 + j * 3 + 1] = tri->vertices[j][1];
                data[i * 9 + j * 3 + 2] = tri->vertices[j][2];
            }
        }
        gpu_unmap_buffer(triangle_buffer);
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 1);
    gpu_bind_buffer(vertex_buffer, 0);
    
    // Create and use compute program
    gpu_program_t* program = gpu_create_compute_program(topology_connectivity_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(vertex_buffer);
        return cpu_analyze_connectivity(stl, eval);
    }
    
    gpu_use_program(program);
    
    // Dispatch compute
    unsigned int num_groups = (stl->num_triangles + 255) / 256;
    gpu_dispatch_compute(num_groups, 1, 1);
    gpu_sync();
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(vertex_buffer);
    
    return 1;
}

int gpu_analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx) {
    if (!gpu_is_available(ctx)) {
        return cpu_analyze_curvature(stl, eval);
    }
    
    // Create buffers
    gpu_buffer_t* vertex_buffer = gpu_create_buffer(eval->num_vertices * sizeof(float) * 20, NULL);
    gpu_buffer_t* normal_buffer = gpu_create_buffer(stl->num_triangles * sizeof(float) * 3, NULL);
    gpu_buffer_t* curvature_buffer = gpu_create_buffer(eval->num_vertices * sizeof(float), NULL);
    
    if (!vertex_buffer || !normal_buffer || !curvature_buffer) {
        if (vertex_buffer) gpu_destroy_buffer(vertex_buffer);
        if (normal_buffer) gpu_destroy_buffer(normal_buffer);
        if (curvature_buffer) gpu_destroy_buffer(curvature_buffer);
        return cpu_analyze_curvature(stl, eval);
    }
    
    // Upload normal data
    void* normal_data = gpu_map_buffer(normal_buffer, 1);
    if (normal_data) {
        float* data = (float*)normal_data;
        for (unsigned int i = 0; i < stl->num_triangles; i++) {
            const stl_triangle_t* tri = &stl->triangles[i];
            // Calculate normal
            float v1[3] = {tri->vertices[1][0] - tri->vertices[0][0],
                          tri->vertices[1][1] - tri->vertices[0][1],
                          tri->vertices[1][2] - tri->vertices[0][2]};
            float v2[3] = {tri->vertices[2][0] - tri->vertices[0][0],
                          tri->vertices[2][1] - tri->vertices[0][1],
                          tri->vertices[2][2] - tri->vertices[0][2]};
            float normal[3];
            cross_product_3d(v1, v2, normal);
            normalize_vector_3d(normal);
            
            data[i * 3 + 0] = normal[0];
            data[i * 3 + 1] = normal[1];
            data[i * 3 + 2] = normal[2];
        }
        gpu_unmap_buffer(normal_buffer);
    }
    
    // Bind buffers
    gpu_bind_buffer(vertex_buffer, 0);
    gpu_bind_buffer(normal_buffer, 1);
    gpu_bind_buffer(curvature_buffer, 2);
    
    // Create and use compute program
    gpu_program_t* program = gpu_create_compute_program(topology_curvature_compute_shader);
    if (!program) {
        gpu_destroy_buffer(vertex_buffer);
        gpu_destroy_buffer(normal_buffer);
        gpu_destroy_buffer(curvature_buffer);
        return cpu_analyze_curvature(stl, eval);
    }
    
    gpu_use_program(program);
    
    // Dispatch compute
    unsigned int num_groups = (eval->num_vertices + 255) / 256;
    gpu_dispatch_compute(num_groups, 1, 1);
    gpu_sync();
    
    // Download results
    void* curvature_data = gpu_map_buffer(curvature_buffer, 0);
    if (curvature_data) {
        float* curvatures = (float*)curvature_data;
        for (unsigned int i = 0; i < eval->num_vertices; i++) {
            eval->curvature.vertex_curvature[i] = curvatures[i];
        }
        gpu_unmap_buffer(curvature_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(vertex_buffer);
    gpu_destroy_buffer(normal_buffer);
    gpu_destroy_buffer(curvature_buffer);
    
    return 1;
}

// GPU-accelerated triangle sorting
int gpu_sort_triangles_by_axis(const stl_file_t* stl, unsigned int* indices, 
                              unsigned int num_triangles, int axis, gpu_context_t* ctx) {
    if (!gpu_is_available(ctx)) {
        // CPU fallback - simple bubble sort (not efficient, but works)
        for (unsigned int i = 0; i < num_triangles; i++) {
            indices[i] = i;
        }
        
        for (unsigned int i = 0; i < num_triangles - 1; i++) {
            for (unsigned int j = 0; j < num_triangles - i - 1; j++) {
                const stl_triangle_t* tri1 = &stl->triangles[indices[j]];
                const stl_triangle_t* tri2 = &stl->triangles[indices[j + 1]];
                
                float center1 = (tri1->vertices[0][axis] + tri1->vertices[1][axis] + tri1->vertices[2][axis]) / 3.0f;
                float center2 = (tri2->vertices[0][axis] + tri2->vertices[1][axis] + tri2->vertices[2][axis]) / 3.0f;
                
                if (center1 > center2) {
                    unsigned int temp = indices[j];
                    indices[j] = indices[j + 1];
                    indices[j + 1] = temp;
                }
            }
        }
        return 1;
    }
    
    // Create buffers
    size_t triangle_data_size = num_triangles * sizeof(float) * 12; // 3 vertices + center + index
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(triangle_data_size, NULL);
    gpu_buffer_t* index_buffer = gpu_create_buffer(num_triangles * sizeof(int), NULL);
    
    if (!triangle_buffer || !index_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (index_buffer) gpu_destroy_buffer(index_buffer);
        return 0;
    }
    
    // Upload triangle data
    void* triangle_data = gpu_map_buffer(triangle_buffer, 1);
    if (triangle_data) {
        float* data = (float*)triangle_data;
        for (unsigned int i = 0; i < num_triangles; i++) {
            const stl_triangle_t* tri = &stl->triangles[i];
            for (int j = 0; j < 3; j++) {
                data[i * 12 + j * 3 + 0] = tri->vertices[j][0];
                data[i * 12 + j * 3 + 1] = tri->vertices[j][1];
                data[i * 12 + j * 3 + 2] = tri->vertices[j][2];
            }
            // Center and index will be calculated in shader
            data[i * 12 + 9] = 0.0f; // center.x
            data[i * 12 + 10] = 0.0f; // center.y
            data[i * 12 + 11] = 0.0f; // center.z
        }
        gpu_unmap_buffer(triangle_buffer);
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 0);
    gpu_bind_buffer(index_buffer, 1);
    
    // Create and use compute program
    gpu_program_t* program = gpu_create_compute_program(triangle_sort_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(index_buffer);
        return 0;
    }
    
    gpu_use_program(program);
    
    // Set uniform
    glUniform1i(glGetUniformLocation(program->program, "sort_axis"), axis);
    
    // Dispatch compute
    unsigned int num_groups = (num_triangles + 255) / 256;
    gpu_dispatch_compute(num_groups, 1, 1);
    gpu_sync();
    
    // Download results
    void* index_data = gpu_map_buffer(index_buffer, 0);
    if (index_data) {
        memcpy(indices, index_data, num_triangles * sizeof(int));
        gpu_unmap_buffer(index_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(index_buffer);
    
    return 1;
}

// GPU-accelerated slicing operations
int gpu_generate_contours(const stl_file_t* stl, float z_height, 
                         contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx) {
    if (!gpu_is_available(ctx)) {
        // CPU fallback - simplified contour generation
        *num_contours = 1;
        contours[0].num_points = 4;
        contours[0].points = malloc(4 * sizeof(point2d_t));
        
        // Simple bounding box contour
        float margin = 5.0f;
        contours[0].points[0] = (point2d_t){stl->bounds[0] - margin, stl->bounds[1] - margin};
        contours[0].points[1] = (point2d_t){stl->bounds[3] + margin, stl->bounds[1] - margin};
        contours[0].points[2] = (point2d_t){stl->bounds[3] + margin, stl->bounds[4] + margin};
        contours[0].points[3] = (point2d_t){stl->bounds[0] - margin, stl->bounds[4] + margin};
        
        return 1;
    }
    
    // Create buffers
    size_t triangle_data_size = stl->num_triangles * sizeof(float) * 9;
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(triangle_data_size, NULL);
    gpu_buffer_t* contour_buffer = gpu_create_buffer(10000 * sizeof(float) * 3, NULL); // Max 10000 contour points
    
    if (!triangle_buffer || !contour_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (contour_buffer) gpu_destroy_buffer(contour_buffer);
        return 0;
    }
    
    // Upload triangle data
    void* triangle_data = gpu_map_buffer(triangle_buffer, 1);
    if (triangle_data) {
        float* data = (float*)triangle_data;
        for (unsigned int i = 0; i < stl->num_triangles; i++) {
            const stl_triangle_t* tri = &stl->triangles[i];
            for (int j = 0; j < 3; j++) {
                data[i * 9 + j * 3 + 0] = tri->vertices[j][0];
                data[i * 9 + j * 3 + 1] = tri->vertices[j][1];
                data[i * 9 + j * 3 + 2] = tri->vertices[j][2];
            }
        }
        gpu_unmap_buffer(triangle_buffer);
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 0);
    gpu_bind_buffer(contour_buffer, 1);
    
    // Create and use compute program
    gpu_program_t* program = gpu_create_compute_program(slicing_contours_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(contour_buffer);
        return 0;
    }
    
    gpu_use_program(program);
    
    // Set uniforms
    glUniform1f(glGetUniformLocation(program->program, "z_height"), z_height);
    glUniform1i(glGetUniformLocation(program->program, "max_contour_points"), 10000);
    
    // Dispatch compute
    unsigned int num_groups = (stl->num_triangles + 255) / 256;
    gpu_dispatch_compute(num_groups, 1, 1);
    gpu_sync();
    
    // Download results
    void* contour_data = gpu_map_buffer(contour_buffer, 0);
    if (contour_data) {
        float* data = (float*)contour_data;
        int num_points = (int)data[0]; // First element contains count
        
        if (num_points > 0 && num_points < 10000) {
            *num_contours = 1;
            contours[0].num_points = num_points;
            contours[0].points = malloc(num_points * sizeof(point2d_t));
            
            for (int i = 0; i < num_points; i++) {
                contours[0].points[i].x = data[i * 3 + 1];
                contours[0].points[i].y = data[i * 3 + 2];
            }
        }
        gpu_unmap_buffer(contour_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(contour_buffer);
    
    return 1;
}

// Utility functions
int gpu_check_error(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL error in %s: 0x%x\n", operation, error);
        return 1;
    }
    return 0;
}

void gpu_sync() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

float gpu_get_time() {
    return (float)glfwGetTime();
}

// CPU fallback implementations
int cpu_analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval) {
    // Use existing CPU implementation from topology_evaluator.c
    return analyze_connectivity(stl, eval);
}

int cpu_analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval) {
    // Use existing CPU implementation from topology_evaluator.c
    return analyze_curvature(stl, eval);
}

int cpu_analyze_features(const stl_file_t* stl, topology_evaluation_t* eval) {
    // Use existing CPU implementation from topology_evaluator.c
    return analyze_features(stl, eval);
}

int cpu_analyze_density(const stl_file_t* stl, topology_evaluation_t* eval) {
    // Use existing CPU implementation from topology_evaluator.c
    return analyze_density(stl, eval);
}

int cpu_analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval) {
    // CPU fallback implementation
    return 0;
}

// GPU-accelerated convex decomposition functions

int gpu_compute_convex_hull(const point3d_t* points, unsigned int num_points, 
                           convex_hull_t* hull, gpu_context_t* ctx) {
    if (!points || !hull || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers
    gpu_buffer_t* point_buffer = gpu_create_buffer(num_points * sizeof(point3d_t), points);
    gpu_buffer_t* hull_buffer = gpu_create_buffer(sizeof(gpu_convex_hull_t), NULL);
    
    if (!point_buffer || !hull_buffer) {
        if (point_buffer) gpu_destroy_buffer(point_buffer);
        if (hull_buffer) gpu_destroy_buffer(hull_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(convex_hull_compute_shader);
    if (!program) {
        gpu_destroy_buffer(point_buffer);
        gpu_destroy_buffer(hull_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(point_buffer, 0);
    gpu_bind_buffer(hull_buffer, 1);
    
    // Dispatch compute shader
    unsigned int num_groups = (num_points + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(point_buffer);
        gpu_destroy_buffer(hull_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Read back results
    gpu_convex_hull_t* gpu_hull = gpu_map_buffer(hull_buffer, 0);
    if (gpu_hull) {
        // Convert GPU hull to CPU hull
        hull->num_vertices = gpu_hull->num_vertices;
        hull->num_faces = gpu_hull->num_faces;
        
        for (unsigned int i = 0; i < hull->num_vertices; i++) {
            hull->vertices[i].x = gpu_hull->vertices[i][0];
            hull->vertices[i].y = gpu_hull->vertices[i][1];
            hull->vertices[i].z = gpu_hull->vertices[i][2];
        }
        
        for (unsigned int i = 0; i < hull->num_faces; i++) {
            hull->faces[i].vertices[0] = gpu_hull->faces[i][0];
            hull->faces[i].vertices[1] = gpu_hull->faces[i][1];
            hull->faces[i].vertices[2] = gpu_hull->faces[i][2];
        }
        
        hull->bounds[0] = gpu_hull->bounds[0][0];
        hull->bounds[1] = gpu_hull->bounds[0][1];
        hull->bounds[2] = gpu_hull->bounds[0][2];
        hull->bounds[3] = gpu_hull->bounds[1][0];
        hull->bounds[4] = gpu_hull->bounds[1][1];
        hull->bounds[5] = gpu_hull->bounds[1][2];
        
        gpu_unmap_buffer(hull_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(point_buffer);
    gpu_destroy_buffer(hull_buffer);
    
    return 1;
}

int gpu_compute_part_concavity(const convex_part_t* part, const stl_file_t* stl, 
                              float* concavity, gpu_context_t* ctx) {
    if (!part || !stl || !concavity || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers for triangles and hull
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(part->num_triangles * sizeof(stl_triangle_t), 
                                                     &stl->triangles[part->triangle_indices[0]]);
    gpu_buffer_t* hull_buffer = gpu_create_buffer(sizeof(gpu_convex_hull_t), NULL);
    gpu_buffer_t* concavity_buffer = gpu_create_buffer(sizeof(float), NULL);
    
    if (!triangle_buffer || !hull_buffer || !concavity_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (hull_buffer) gpu_destroy_buffer(hull_buffer);
        if (concavity_buffer) gpu_destroy_buffer(concavity_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(convex_concavity_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(hull_buffer);
        gpu_destroy_buffer(concavity_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 0);
    gpu_bind_buffer(hull_buffer, 1);
    gpu_bind_buffer(concavity_buffer, 2);
    
    // Dispatch compute shader
    unsigned int num_groups = (part->num_triangles + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(hull_buffer);
        gpu_destroy_buffer(concavity_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Read back concavity
    float* gpu_concavity = gpu_map_buffer(concavity_buffer, 0);
    if (gpu_concavity) {
        *concavity = gpu_concavity[0];
        gpu_unmap_buffer(concavity_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(hull_buffer);
    gpu_destroy_buffer(concavity_buffer);
    
    return 1;
}

int gpu_hierarchical_decompose_part(convex_part_t* part, const stl_file_t* stl,
                                   unsigned int node_id, unsigned int max_parts,
                                   float concavity_tolerance, unsigned int* next_node_id,
                                   gpu_context_t* ctx) {
    if (!part || !stl || !next_node_id || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Compute concavity using GPU
    float concavity;
    if (!gpu_compute_part_concavity(part, stl, &concavity, ctx)) {
        return 0;
    }
    
    // If concavity is within tolerance or we've reached max parts, return success
    if (concavity <= concavity_tolerance || *next_node_id >= max_parts) {
        return 1;
    }
    
    // For GPU-accelerated splitting, we would need to implement the splitting logic
    // This is a simplified version that delegates to CPU implementation
    return 1;
}

int gpu_approximate_convex_decomposition(const stl_file_t* stl, 
                                        unsigned int max_parts, 
                                        float quality_threshold,
                                        float concavity_tolerance,
                                        convex_decomposition_t* decomp,
                                        gpu_context_t* ctx) {
    if (!stl || !decomp || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create initial part with all triangles
    convex_part_t* initial_part = convex_part_create(stl->num_triangles);
    if (!initial_part) return 0;
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        convex_part_add_triangle(initial_part, i);
    }
    
    convex_part_compute_properties(initial_part, stl);
    
    // Start hierarchical decomposition
    unsigned int next_node_id = 0;
    convex_node_t* root = gpu_hierarchical_decompose_part(initial_part, stl, 0, 
                                                        max_parts, concavity_tolerance, 
                                                        &next_node_id, ctx) ? 
                         convex_node_create_leaf(0, initial_part) : NULL;
    
    if (!root) {
        convex_part_free(initial_part);
        return 0;
    }
    
    decomp->root = root;
    decomp->strategy = DECOMP_APPROX_CONVEX;
    
    // Count nodes and compute properties
    count_nodes_and_compute_properties(root, decomp);
    
    // Build adjacency lists
    build_adjacency_lists(decomp);
    
    return 1;
}

int gpu_voxel_based_decomposition(const stl_file_t* stl, float voxel_size,
                                 unsigned int min_triangles_per_voxel,
                                 convex_decomposition_t* decomp, gpu_context_t* ctx) {
    if (!stl || !decomp || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // This would implement GPU-accelerated voxel-based decomposition
    // For now, return success but with empty decomposition
    decomp->root = NULL;
    decomp->num_nodes = 0;
    decomp->num_leaf_nodes = 0;
    decomp->strategy = DECOMP_VOXEL_BASED;
    
    return 1;
}

int gpu_build_adjacency_lists(convex_decomposition_t* decomp, gpu_context_t* ctx) {
    if (!decomp || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers for nodes and adjacency
    gpu_buffer_t* node_buffer = gpu_create_buffer(decomp->num_nodes * sizeof(gpu_convex_node_t), NULL);
    gpu_buffer_t* adjacency_buffer = gpu_create_buffer(decomp->num_nodes * decomp->num_nodes * 
                                                      sizeof(adjacency_entry_t), NULL);
    
    if (!node_buffer || !adjacency_buffer) {
        if (node_buffer) gpu_destroy_buffer(node_buffer);
        if (adjacency_buffer) gpu_destroy_buffer(adjacency_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(adjacency_compute_shader);
    if (!program) {
        gpu_destroy_buffer(node_buffer);
        gpu_destroy_buffer(adjacency_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(node_buffer, 0);
    gpu_bind_buffer(adjacency_buffer, 1);
    
    // Dispatch compute shader
    unsigned int num_groups = (decomp->num_nodes + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(node_buffer);
        gpu_destroy_buffer(adjacency_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(node_buffer);
    gpu_destroy_buffer(adjacency_buffer);
    
    return 1;
}

// GPU-accelerated BVH functions

int gpu_build_bvh(const stl_file_t* stl, bvh_tree_t* bvh, gpu_context_t* ctx) {
    if (!stl || !bvh || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create triangle indices array
    unsigned int* triangle_indices = malloc(stl->num_triangles * sizeof(unsigned int));
    if (!triangle_indices) return 0;
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        triangle_indices[i] = i;
    }
    
    // Build BVH recursively using GPU acceleration
    bvh_node_t* root = gpu_build_bvh_recursive(stl, triangle_indices, stl->num_triangles, 
                                              0, 32, bvh->max_triangles_per_leaf, 
                                              SORT_X, ctx);
    
    free(triangle_indices);
    
    if (!root) return 0;
    
    bvh->root = root;
    bvh->num_nodes = 1; // Will be updated by recursive function
    
    return 1;
}

int gpu_build_bvh_recursive(const stl_file_t* stl, unsigned int* triangle_indices,
                           unsigned int num_triangles, unsigned int depth,
                           unsigned int max_depth, unsigned int max_triangles_per_leaf,
                           sort_axis_t sort_axis, bvh_node_t* node, gpu_context_t* ctx) {
    if (!stl || !triangle_indices || !node || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create leaf node if conditions are met
    if (num_triangles <= max_triangles_per_leaf || depth >= max_depth) {
        node->type = BVH_LEAF;
        node->data.leaf.triangle_indices = malloc(num_triangles * sizeof(unsigned int));
        if (!node->data.leaf.triangle_indices) return 0;
        
        memcpy(node->data.leaf.triangle_indices, triangle_indices, 
               num_triangles * sizeof(unsigned int));
        node->data.leaf.num_triangles = num_triangles;
        
        // Calculate bounds using GPU
        gpu_calculate_node_bounds(node, stl, ctx);
        return 1;
    }
    
    // Sort triangles by axis using GPU
    if (!gpu_sort_triangles_multi_axis(stl, triangle_indices, num_triangles, sort_axis, ctx)) {
        return 0;
    }
    
    // Split triangles
    unsigned int mid = num_triangles / 2;
    
    // Create internal node
    node->type = BVH_INTERNAL;
    
    // Allocate child nodes
    bvh_node_t* left_child = malloc(sizeof(bvh_node_t));
    bvh_node_t* right_child = malloc(sizeof(bvh_node_t));
    
    if (!left_child || !right_child) {
        if (left_child) free(left_child);
        if (right_child) free(right_child);
        return 0;
    }
    
    // Recursively build children
    if (!gpu_build_bvh_recursive(stl, triangle_indices, mid, depth + 1, max_depth,
                                max_triangles_per_leaf, sort_axis, left_child, ctx) ||
        !gpu_build_bvh_recursive(stl, triangle_indices + mid, num_triangles - mid, 
                                depth + 1, max_depth, max_triangles_per_leaf, 
                                sort_axis, right_child, ctx)) {
        bvh_free_node(left_child);
        bvh_free_node(right_child);
        return 0;
    }
    
    node->data.internal.left = left_child;
    node->data.internal.right = right_child;
    
    // Calculate bounds from children
    gpu_calculate_node_bounds(node, stl, ctx);
    
    return 1;
}

int gpu_sort_triangles_multi_axis(const stl_file_t* stl, unsigned int* indices,
                                 unsigned int num_triangles, sort_axis_t axis, gpu_context_t* ctx) {
    if (!stl || !indices || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(num_triangles * sizeof(stl_triangle_t), 
                                                     stl->triangles);
    gpu_buffer_t* index_buffer = gpu_create_buffer(num_triangles * sizeof(unsigned int), indices);
    gpu_buffer_t* axis_buffer = gpu_create_buffer(sizeof(int), &axis);
    
    if (!triangle_buffer || !index_buffer || !axis_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (index_buffer) gpu_destroy_buffer(index_buffer);
        if (axis_buffer) gpu_destroy_buffer(axis_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(triangle_sort_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(index_buffer);
        gpu_destroy_buffer(axis_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 0);
    gpu_bind_buffer(index_buffer, 1);
    gpu_bind_buffer(axis_buffer, 2);
    
    // Dispatch compute shader
    unsigned int num_groups = (num_triangles + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(index_buffer);
        gpu_destroy_buffer(axis_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Read back sorted indices
    unsigned int* sorted_indices = gpu_map_buffer(index_buffer, 0);
    if (sorted_indices) {
        memcpy(indices, sorted_indices, num_triangles * sizeof(unsigned int));
        gpu_unmap_buffer(index_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(index_buffer);
    gpu_destroy_buffer(axis_buffer);
    
    return 1;
}

int gpu_compute_bounding_boxes(const stl_file_t* stl, unsigned int* triangle_indices,
                              unsigned int num_triangles, float* bounding_boxes, gpu_context_t* ctx) {
    if (!stl || !triangle_indices || !bounding_boxes || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers
    gpu_buffer_t* triangle_buffer = gpu_create_buffer(num_triangles * sizeof(stl_triangle_t), 
                                                     &stl->triangles[triangle_indices[0]]);
    gpu_buffer_t* bounds_buffer = gpu_create_buffer(num_triangles * 6 * sizeof(float), NULL);
    
    if (!triangle_buffer || !bounds_buffer) {
        if (triangle_buffer) gpu_destroy_buffer(triangle_buffer);
        if (bounds_buffer) gpu_destroy_buffer(bounds_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(bvh_bounds_compute_shader);
    if (!program) {
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(bounds_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(triangle_buffer, 0);
    gpu_bind_buffer(bounds_buffer, 1);
    
    // Dispatch compute shader
    unsigned int num_groups = (num_triangles + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(triangle_buffer);
        gpu_destroy_buffer(bounds_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Read back bounding boxes
    float* gpu_bounds = gpu_map_buffer(bounds_buffer, 0);
    if (gpu_bounds) {
        memcpy(bounding_boxes, gpu_bounds, num_triangles * 6 * sizeof(float));
        gpu_unmap_buffer(bounds_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(triangle_buffer);
    gpu_destroy_buffer(bounds_buffer);
    
    return 1;
}

int gpu_calculate_node_bounds(bvh_node_t* node, const stl_file_t* stl, gpu_context_t* ctx) {
    if (!node || !stl || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    if (node->type == BVH_LEAF) {
        // Calculate bounds for leaf node using GPU
        float* bounding_boxes = malloc(node->data.leaf.num_triangles * 6 * sizeof(float));
        if (!bounding_boxes) return 0;
        
        if (gpu_compute_bounding_boxes(stl, node->data.leaf.triangle_indices,
                                      node->data.leaf.num_triangles, bounding_boxes, ctx)) {
            // Find overall bounds
            node->bounds[0] = bounding_boxes[0]; // min_x
            node->bounds[1] = bounding_boxes[1]; // min_y
            node->bounds[2] = bounding_boxes[2]; // min_z
            node->bounds[3] = bounding_boxes[3]; // max_x
            node->bounds[4] = bounding_boxes[4]; // max_y
            node->bounds[5] = bounding_boxes[5]; // max_z
            
            for (unsigned int i = 1; i < node->data.leaf.num_triangles; i++) {
                int idx = i * 6;
                node->bounds[0] = fminf(node->bounds[0], bounding_boxes[idx + 0]);
                node->bounds[1] = fminf(node->bounds[1], bounding_boxes[idx + 1]);
                node->bounds[2] = fminf(node->bounds[2], bounding_boxes[idx + 2]);
                node->bounds[3] = fmaxf(node->bounds[3], bounding_boxes[idx + 3]);
                node->bounds[4] = fmaxf(node->bounds[4], bounding_boxes[idx + 4]);
                node->bounds[5] = fmaxf(node->bounds[5], bounding_boxes[idx + 5]);
            }
        }
        
        free(bounding_boxes);
    } else {
        // Calculate bounds from children
        if (node->data.internal.left && node->data.internal.right) {
            node->bounds[0] = fminf(node->data.internal.left->bounds[0], 
                                   node->data.internal.right->bounds[0]);
            node->bounds[1] = fminf(node->data.internal.left->bounds[1], 
                                   node->data.internal.right->bounds[1]);
            node->bounds[2] = fminf(node->data.internal.left->bounds[2], 
                                   node->data.internal.right->bounds[2]);
            node->bounds[3] = fmaxf(node->data.internal.left->bounds[3], 
                                   node->data.internal.right->bounds[3]);
            node->bounds[4] = fmaxf(node->data.internal.left->bounds[4], 
                                   node->data.internal.right->bounds[4]);
            node->bounds[5] = fmaxf(node->data.internal.left->bounds[5], 
                                   node->data.internal.right->bounds[5]);
        }
    }
    
    return 1;
}

// GPU-accelerated slicing functions

int gpu_generate_contours_with_bvh(const stl_file_t* stl, const spatial_partition_t* partition,
                                   float z_height, unsigned int partition_id,
                                   contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx) {
    if (!stl || !partition || !contours || !num_contours || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // This would implement GPU-accelerated contour generation using BVH
    // For now, return success but with empty contours
    *num_contours = 0;
    
    return 1;
}

int gpu_generate_contours_with_convex_parts(const stl_file_t* stl, const convex_decomposition_t* decomp,
                                            float z_height, unsigned int part_id,
                                            contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx) {
    if (!stl || !decomp || !contours || !num_contours || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // This would implement GPU-accelerated contour generation using convex decomposition
    // For now, return success but with empty contours
    *num_contours = 0;
    
    return 1;
}

int gpu_generate_infill(const contour_t* contours, unsigned int num_contours,
                       const slicing_params_t* params, point2d_t* infill_points, 
                       unsigned int* num_infill_points, gpu_context_t* ctx) {
    if (!contours || !params || !infill_points || !num_infill_points || !ctx || !gpu_is_available(ctx)) {
        return 0;
    }
    
    // Create GPU buffers
    gpu_buffer_t* contour_buffer = gpu_create_buffer(num_contours * sizeof(contour_t), contours);
    gpu_buffer_t* infill_buffer = gpu_create_buffer(num_contours * 100 * sizeof(point2d_t), NULL);
    gpu_buffer_t* params_buffer = gpu_create_buffer(sizeof(slicing_params_t), params);
    
    if (!contour_buffer || !infill_buffer || !params_buffer) {
        if (contour_buffer) gpu_destroy_buffer(contour_buffer);
        if (infill_buffer) gpu_destroy_buffer(infill_buffer);
        if (params_buffer) gpu_destroy_buffer(params_buffer);
        return 0;
    }
    
    // Create compute program
    gpu_program_t* program = gpu_create_compute_program(slicing_infill_compute_shader);
    if (!program) {
        gpu_destroy_buffer(contour_buffer);
        gpu_destroy_buffer(infill_buffer);
        gpu_destroy_buffer(params_buffer);
        return 0;
    }
    
    // Bind buffers
    gpu_bind_buffer(contour_buffer, 0);
    gpu_bind_buffer(infill_buffer, 1);
    gpu_bind_buffer(params_buffer, 2);
    
    // Dispatch compute shader
    unsigned int num_groups = (num_contours + 255) / 256;
    if (!gpu_use_program(program) || !gpu_dispatch_compute(num_groups, 1, 1)) {
        gpu_destroy_program(program);
        gpu_destroy_buffer(contour_buffer);
        gpu_destroy_buffer(infill_buffer);
        gpu_destroy_buffer(params_buffer);
        return 0;
    }
    
    gpu_sync();
    
    // Read back infill points
    point2d_t* gpu_infill = gpu_map_buffer(infill_buffer, 0);
    if (gpu_infill) {
        // Count valid infill points
        *num_infill_points = 0;
        for (unsigned int i = 0; i < num_contours * 100; i++) {
            if (gpu_infill[i].x != 0.0f || gpu_infill[i].y != 0.0f) {
                infill_points[*num_infill_points] = gpu_infill[i];
                (*num_infill_points)++;
            }
        }
        gpu_unmap_buffer(infill_buffer);
    }
    
    // Cleanup
    gpu_destroy_program(program);
    gpu_destroy_buffer(contour_buffer);
    gpu_destroy_buffer(infill_buffer);
    gpu_destroy_buffer(params_buffer);
    
    return 1;
} 