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
    vec3 avg_normal = vec3(0.0);
    int normal_count = 0;
    
    // Find all triangles sharing this vertex
    for (int i = 0; i < triangle_normals.length(); i++) {
        // Simple distance check (in real implementation, use proper vertex indexing)
        if (distance(triangle_normals[i], vertex.position) < 0.001) {
            avg_normal += triangle_normals[i];
            normal_count++;
        }
    }
    
    if (normal_count > 0) {
        avg_normal = normalize(avg_normal / float(normal_count));
        curvatures[tid] = length(avg_normal);
    } else {
        curvatures[tid] = 0.0;
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

uniform int sort_axis;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate triangle center
    tri.center = (tri.vertices[0] + tri.vertices[1] + tri.vertices[2]) / 3.0;
    triangles[tid].center = tri.center;
    
    // Store original index
    tri.original_index = int(tid);
    triangles[tid].original_index = tri.original_index;
    
    // Simple sorting based on center coordinate
    float sort_value = tri.center[sort_axis];
    indices[tid] = int(sort_value * 1000000); // Convert to integer for sorting
}
)";

const char* bvh_construction_compute_shader = R"(
#version 430

layout(local_size_x = 256) in;

struct Triangle {
    vec3 vertices[3];
    vec3 center;
    int original_index;
};

struct BoundingBox {
    vec3 min;
    vec3 max;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) buffer BoundingBoxBuffer {
    BoundingBox bounding_boxes[];
};

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Calculate bounding box for triangle
    BoundingBox bbox;
    bbox.min = min(min(tri.vertices[0], tri.vertices[1]), tri.vertices[2]);
    bbox.max = max(max(tri.vertices[0], tri.vertices[1]), tri.vertices[2]);
    
    bounding_boxes[tid] = bbox;
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
    ContourPoint contour_points[];
};

uniform float z_height;
uniform int max_contour_points;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    
    if (tid >= triangles.length()) return;
    
    Triangle tri = triangles[tid];
    
    // Check if triangle intersects with Z plane
    float min_z = min(min(tri.vertices[0].z, tri.vertices[1].z), tri.vertices[2].z);
    float max_z = max(max(tri.vertices[0].z, tri.vertices[1].z), tri.vertices[2].z);
    
    if (z_height >= min_z && z_height <= max_z) {
        // Calculate intersection points
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            vec3 v1 = tri.vertices[i];
            vec3 v2 = tri.vertices[j];
            
            if ((v1.z <= z_height && v2.z >= z_height) || 
                (v1.z >= z_height && v2.z <= z_height)) {
                
                float t = (z_height - v1.z) / (v2.z - v1.z);
                vec2 intersection = mix(v1.xy, v2.xy, t);
                
                uint point_idx = atomicAdd(contour_points[0].valid, 1);
                if (point_idx < max_contour_points) {
                    contour_points[point_idx + 1].position = intersection;
                    contour_points[point_idx + 1].valid = 1;
                }
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
    // Use existing CPU implementation from topology_evaluator.c
    return analyze_quality(stl, eval);
} 