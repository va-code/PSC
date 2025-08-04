#ifndef GPU_ACCELERATOR_H
#define GPU_ACCELERATOR_H

#include "stl_parser.h"
#include "topology_evaluator.h"
#include "convex_decomposition.h"
#include "bvh.h"
#include "slicer.h"
#include <stdint.h>

// GPU acceleration modes
typedef enum {
    GPU_MODE_CPU_ONLY,        // Force CPU-only execution
    GPU_MODE_GPU_PREFERRED,   // Try GPU, fallback to CPU
    GPU_MODE_GPU_ONLY,        // Force GPU-only execution
    GPU_MODE_AUTO            // Automatic selection based on system capabilities
} gpu_mode_t;

// GPU capabilities structure
typedef struct {
    int has_opengl_compute;   // OpenGL compute shader support
    int has_opengl_43;        // OpenGL 4.3+ support
    int max_compute_units;    // Maximum compute units
    int max_work_group_size;  // Maximum work group size
    int max_shared_memory;    // Maximum shared memory
    char vendor[256];         // GPU vendor string
    char renderer[256];       // GPU renderer string
    char version[256];        // OpenGL version string
} gpu_capabilities_t;

// GPU context structure
typedef struct {
    void* window;             // GLFW window handle
    void* context;            // OpenGL context
    gpu_capabilities_t caps;  // GPU capabilities
    int is_initialized;       // Initialization status
    gpu_mode_t current_mode;  // Current execution mode
} gpu_context_t;

// GPU buffer structure
typedef struct {
    unsigned int vbo;         // Vertex buffer object
    unsigned int ssbo;        // Shader storage buffer object
    size_t size;              // Buffer size in bytes
    int is_mapped;            // Mapping status
} gpu_buffer_t;

// GPU program structure
typedef struct {
    unsigned int program;     // OpenGL program ID
    unsigned int compute_shader; // Compute shader ID
    int is_linked;            // Link status
} gpu_program_t;

// GPU-optimized convex hull structure for compute shaders
typedef struct {
    float vertices[1024][3];  // Fixed-size vertex array for GPU
    unsigned int num_vertices;
    unsigned int faces[2048][3]; // Fixed-size face array for GPU
    unsigned int num_faces;
    float bounds[6];          // Bounding box
} gpu_convex_hull_t;

// GPU-optimized convex node structure
typedef struct {
    unsigned int node_id;
    unsigned int node_type;   // 0 = leaf, 1 = internal
    float bounds[6];
    float concavity;
    
    union {
        struct {
            unsigned int part_start_index;
            unsigned int num_triangles;
        } leaf;
        
        struct {
            unsigned int left_child_id;
            unsigned int right_child_id;
        } internal;
    } data;
} gpu_convex_node_t;

// GPU-optimized BVH node structure
typedef struct {
    unsigned int node_type;   // 0 = leaf, 1 = internal
    float bounds[6];
    
    union {
        struct {
            unsigned int triangle_start_index;
            unsigned int num_triangles;
        } leaf;
        
        struct {
            unsigned int left_child_id;
            unsigned int right_child_id;
        } internal;
    } data;
} gpu_bvh_node_t;

// Function declarations

// GPU context management
gpu_context_t* gpu_init(gpu_mode_t mode);
void cleanup_gpu(gpu_context_t* ctx);
int is_gpu_available(const gpu_context_t* ctx);
gpu_capabilities_t get_gpu_capabilities(const gpu_context_t* ctx);
void print_gpu_capabilities(const gpu_capabilities_t* caps);
int gpu_is_available(const gpu_context_t* ctx);
gpu_capabilities_t gpu_get_capabilities(const gpu_context_t* ctx);
void gpu_print_capabilities(const gpu_capabilities_t* caps);
void gpu_cleanup(gpu_context_t* ctx);

// GPU buffer management
gpu_buffer_t* gpu_create_buffer(size_t size, const void* data);
void destroy_gpu_buffer(gpu_buffer_t* buffer);
void* gpu_map_buffer(gpu_buffer_t* buffer, int write_only);
void unmap_gpu_buffer(gpu_buffer_t* buffer);
void bind_gpu_buffer(gpu_buffer_t* buffer, unsigned int binding_point);

// GPU program management
gpu_program_t* gpu_create_compute_program(const char* compute_source);
void destroy_gpu_program(gpu_program_t* program);
int use_gpu_program(gpu_program_t* program);
int dispatch_gpu_compute(unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);

// GPU-accelerated topology analysis
int analyze_gpu_connectivity(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int analyze_gpu_curvature(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int analyze_gpu_features(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int analyze_gpu_density(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int analyze_gpu_quality(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_features(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_density(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);

// GPU-accelerated convex decomposition
int compute_gpu_convex_hull(const point3d_t* points, unsigned int num_points, 
                           convex_hull_t* hull, gpu_context_t* ctx);
int compute_gpu_part_concavity(const convex_part_t* part, const stl_file_t* stl, 
                              float* concavity, gpu_context_t* ctx);
int decompose_gpu_hierarchical_part(convex_part_t* part, const stl_file_t* stl,
                                   unsigned int node_id, unsigned int max_parts,
                                   float concavity_tolerance, unsigned int* next_node_id,
                                   gpu_context_t* ctx);
int decompose_gpu_approximate_convex(const stl_file_t* stl, 
                                    unsigned int max_parts, 
                                    float quality_threshold,
                                    float concavity_tolerance,
                                    convex_decomposition_t* decomp,
                                    gpu_context_t* ctx);
int sort_gpu_triangles_by_axis(const stl_file_t* stl, unsigned int* indices, 
                              unsigned int num_triangles, int axis, gpu_context_t* ctx);
int decompose_gpu_voxel_based(const stl_file_t* stl, float voxel_size,
                             unsigned int min_triangles_per_voxel,
                             convex_decomposition_t* decomp, gpu_context_t* ctx);
int build_gpu_adjacency_lists(convex_decomposition_t* decomp, gpu_context_t* ctx);

// GPU-accelerated BVH construction
int build_gpu_bvh(const stl_file_t* stl, bvh_tree_t* bvh, gpu_context_t* ctx);
int build_gpu_bvh_recursive(const stl_file_t* stl, unsigned int* triangle_indices,
                           unsigned int num_triangles, unsigned int depth,
                           unsigned int max_depth, unsigned int max_triangles_per_leaf,
                           sort_axis_t sort_axis, bvh_node_t* node, gpu_context_t* ctx);
int sort_gpu_triangles_multi_axis(const stl_file_t* stl, unsigned int* indices,
                                 unsigned int num_triangles, sort_axis_t axis, gpu_context_t* ctx);
int compute_gpu_bounding_boxes(const stl_file_t* stl, unsigned int* triangle_indices,
                              unsigned int num_triangles, float* bounding_boxes, gpu_context_t* ctx);
int calculate_gpu_node_bounds(bvh_node_t* node, const stl_file_t* stl, gpu_context_t* ctx);

// GPU-accelerated slicing operations
int generate_gpu_contours(const stl_file_t* stl, float z_height, 
                         contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx);
int generate_gpu_contours_with_bvh(const stl_file_t* stl, const spatial_partition_t* partition,
                                  float z_height, unsigned int partition_id,
                                  contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx);
int generate_gpu_contours_with_convex_parts(const stl_file_t* stl, const convex_decomposition_t* decomp,
                                           float z_height, unsigned int part_id,
                                           contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx);
int generate_gpu_infill(const contour_t* contours, unsigned int num_contours,
                       const slicing_params_t* params, point2d_t* infill_points, 
                       unsigned int* num_infill_points, gpu_context_t* ctx);

// Shader source code
extern const char* topology_connectivity_compute_shader;
extern const char* topology_curvature_compute_shader;
extern const char* topology_features_compute_shader;
extern const char* topology_density_compute_shader;
extern const char* topology_quality_compute_shader;
extern const char* convex_hull_compute_shader;
extern const char* convex_decomposition_compute_shader;
extern const char* convex_concavity_compute_shader;
extern const char* triangle_sort_compute_shader;
extern const char* bvh_construction_compute_shader;
extern const char* bvh_bounds_compute_shader;
extern const char* slicing_contours_compute_shader;
extern const char* slicing_infill_compute_shader;
extern const char* adjacency_compute_shader;

// Utility functions
int check_gpu_error(const char* operation);
void sync_gpu();
float get_gpu_time();

// Fallback functions (CPU implementations)
int analyze_cpu_connectivity(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_cpu_curvature(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_cpu_features(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_cpu_density(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_cpu_quality(const stl_file_t* stl, topology_evaluation_t* eval);

#endif // GPU_ACCELERATOR_H 