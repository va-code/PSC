#ifndef GPU_ACCELERATOR_H
#define GPU_ACCELERATOR_H

#include "stl_parser.h"
#include "topology_evaluator.h"
#include "convex_decomposition.h"
#include "bvh.h"
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

// Function declarations

// GPU context management
gpu_context_t* gpu_init(gpu_mode_t mode);
void gpu_cleanup(gpu_context_t* ctx);
int gpu_is_available(const gpu_context_t* ctx);
gpu_capabilities_t gpu_get_capabilities(const gpu_context_t* ctx);
void gpu_print_capabilities(const gpu_capabilities_t* caps);

// GPU buffer management
gpu_buffer_t* gpu_create_buffer(size_t size, const void* data);
void gpu_destroy_buffer(gpu_buffer_t* buffer);
void* gpu_map_buffer(gpu_buffer_t* buffer, int write_only);
void gpu_unmap_buffer(gpu_buffer_t* buffer);
void gpu_bind_buffer(gpu_buffer_t* buffer, unsigned int binding_point);

// GPU program management
gpu_program_t* gpu_create_compute_program(const char* compute_source);
void gpu_destroy_program(gpu_program_t* program);
int gpu_use_program(gpu_program_t* program);
int gpu_dispatch_compute(unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);

// GPU-accelerated topology evaluation
int gpu_analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_features(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_density(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);
int gpu_analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval, gpu_context_t* ctx);

// GPU-accelerated convex decomposition
int gpu_compute_convex_hull(const point3d_t* points, unsigned int num_points, 
                           convex_hull_t* hull, gpu_context_t* ctx);
int gpu_sort_triangles_by_axis(const stl_file_t* stl, unsigned int* indices, 
                              unsigned int num_triangles, int axis, gpu_context_t* ctx);
int gpu_voxel_based_decomposition(const stl_file_t* stl, float voxel_size,
                                 unsigned int min_triangles_per_voxel,
                                 convex_decomposition_t* decomp, gpu_context_t* ctx);

// GPU-accelerated BVH construction
int gpu_build_bvh(const stl_file_t* stl, bvh_tree_t* bvh, gpu_context_t* ctx);
int gpu_sort_triangles_multi_axis(const stl_file_t* stl, unsigned int* indices,
                                 unsigned int num_triangles, sort_axis_t axis, gpu_context_t* ctx);
int gpu_compute_bounding_boxes(const stl_file_t* stl, unsigned int* triangle_indices,
                              unsigned int num_triangles, float* bounding_boxes, gpu_context_t* ctx);

// GPU-accelerated slicing operations
int gpu_generate_contours(const stl_file_t* stl, float z_height, 
                         contour_t* contours, unsigned int* num_contours, gpu_context_t* ctx);
int gpu_generate_infill(const contour_t* contours, unsigned int num_contours,
                       const slicing_params_t* params, point2d_t* infill_points, 
                       unsigned int* num_infill_points, gpu_context_t* ctx);

// Shader source code
extern const char* topology_connectivity_compute_shader;
extern const char* topology_curvature_compute_shader;
extern const char* topology_features_compute_shader;
extern const char* topology_density_compute_shader;
extern const char* topology_quality_compute_shader;
extern const char* convex_hull_compute_shader;
extern const char* triangle_sort_compute_shader;
extern const char* bvh_construction_compute_shader;
extern const char* slicing_contours_compute_shader;
extern const char* slicing_infill_compute_shader;

// Utility functions
int gpu_check_error(const char* operation);
void gpu_sync();
float gpu_get_time();

// Fallback functions (CPU implementations)
int cpu_analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval);
int cpu_analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval);
int cpu_analyze_features(const stl_file_t* stl, topology_evaluation_t* eval);
int cpu_analyze_density(const stl_file_t* stl, topology_evaluation_t* eval);
int cpu_analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval);

#endif // GPU_ACCELERATOR_H 