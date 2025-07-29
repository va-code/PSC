#ifndef CONVEX_DECOMPOSITION_H
#define CONVEX_DECOMPOSITION_H

#include "stl_parser.h"
#include <stdint.h>

// Convex hull algorithms
typedef enum {
    CONVEX_HULL_GRAHAM_SCAN,    // Graham's scan algorithm
    CONVEX_HULL_JARVIS_MARCH,   // Jarvis march (gift wrapping)
    CONVEX_HULL_QUICKHULL,      // QuickHull algorithm
    CONVEX_HULL_CHAN            // Chan's algorithm
} convex_hull_algorithm_t;

// Decomposition strategies
typedef enum {
    DECOMP_APPROX_CONVEX,       // Approximate convex decomposition
    DECOMP_EXACT_CONVEX,        // Exact convex decomposition
    DECOMP_HIERARCHICAL,        // Hierarchical decomposition
    DECOMP_VOXEL_BASED          // Voxel-based decomposition
} decomposition_strategy_t;

// Decomposition parameters
typedef struct {
    decomposition_strategy_t strategy;
    unsigned int max_parts;
    float quality_threshold;
    float concavity_tolerance;   // Maximum allowed concavity (0.0 = perfectly convex, 1.0 = any shape)
    float voxel_size;           // For voxel-based decomposition
    unsigned int min_triangles_per_voxel; // For voxel-based decomposition
} decomposition_params_t;

// Point structure for 2D operations
typedef struct {
    float x, y;
} point2d_t;

// Point structure for 3D operations
typedef struct {
    float x, y, z;
} point3d_t;

// Convex hull structure
typedef struct {
    point3d_t* vertices;        // Vertices of the convex hull
    unsigned int num_vertices;   // Number of vertices
    unsigned int capacity;       // Allocated capacity
    float bounds[6];            // Bounding box [min_x, min_y, min_z, max_x, max_y, max_z]
} convex_hull_t;

// Convex part structure
typedef struct {
    convex_hull_t hull;         // Convex hull of this part
    unsigned int* triangle_indices; // Indices of triangles in this part
    unsigned int num_triangles; // Number of triangles
    unsigned int capacity;      // Allocated capacity
    float center[3];           // Centroid of the part
    float volume;              // Approximate volume
} convex_part_t;

// Decomposition result structure
typedef struct {
    convex_part_t* parts;       // Array of convex parts
    unsigned int num_parts;     // Number of parts
    unsigned int capacity;      // Allocated capacity
    decomposition_strategy_t strategy; // Strategy used
    float total_volume;         // Total volume of all parts
    float decomposition_quality; // Quality metric (0.0 to 1.0)
} convex_decomposition_t;

// Function declarations

// Convex hull operations
convex_hull_t* convex_hull_create(unsigned int initial_capacity);
void convex_hull_free(convex_hull_t* hull);
void convex_hull_add_vertex(convex_hull_t* hull, float x, float y, float z);
convex_hull_t* compute_convex_hull_3d(const point3d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm);
convex_hull_t* compute_convex_hull_2d(const point2d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm);

// Convex part operations
convex_part_t* convex_part_create(unsigned int initial_capacity);
void convex_part_free(convex_part_t* part);
void convex_part_add_triangle(convex_part_t* part, unsigned int triangle_index);
void convex_part_compute_properties(convex_part_t* part, const stl_file_t* stl);

// Decomposition operations
convex_decomposition_t* convex_decomposition_create(unsigned int initial_capacity);
void convex_decomposition_free(convex_decomposition_t* decomp);
void convex_decomposition_add_part(convex_decomposition_t* decomp, convex_part_t* part);

// Main decomposition functions
convex_decomposition_t* decompose_model(const stl_file_t* stl, const decomposition_params_t* params);
convex_decomposition_t* decompose_model_simple(const stl_file_t* stl, decomposition_strategy_t strategy,
                                             unsigned int max_parts, float quality_threshold);
convex_decomposition_t* approximate_convex_decomposition(const stl_file_t* stl, 
                                                        unsigned int max_parts, 
                                                        float quality_threshold,
                                                        float concavity_tolerance);
convex_decomposition_t* hierarchical_decomposition(const stl_file_t* stl, 
                                                  unsigned int max_depth,
                                                  float split_threshold);
convex_decomposition_t* voxel_based_decomposition(const stl_file_t* stl, 
                                                 float voxel_size,
                                                 unsigned int min_triangles_per_voxel);
void hierarchical_split_part(convex_part_t* part, const stl_file_t* stl, 
                           convex_decomposition_t* decomp, unsigned int depth,
                           unsigned int max_depth, float split_threshold);
void approximate_decompose_part(convex_part_t* part, const stl_file_t* stl,
                              convex_decomposition_t* decomp, unsigned int max_parts,
                              float concavity_tolerance);

// Utility functions
float compute_volume(const convex_hull_t* hull);
float compute_centroid(const convex_hull_t* hull, float* center);
float compute_decomposition_quality(const convex_decomposition_t* decomp);
float compute_part_concavity(const convex_part_t* part, const stl_file_t* stl);
int is_point_inside_hull(const convex_hull_t* hull, float x, float y, float z);
int hulls_intersect(const convex_hull_t* hull1, const convex_hull_t* hull2);
float compute_hull_distance(const convex_hull_t* hull1, const convex_hull_t* hull2);

// Analysis and visualization
void print_decomposition_info(const convex_decomposition_t* decomp);
void print_part_info(const convex_part_t* part, unsigned int part_index);
void export_decomposition_to_stl(const convex_decomposition_t* decomp, const char* filename);

// Geometry utilities
float cross_product_2d(float x1, float y1, float x2, float y2);
float dot_product_3d(float x1, float y1, float z1, float x2, float y2, float z2);
float distance_3d(float x1, float y1, float z1, float x2, float y2, float z2);
int orientation_2d(float x1, float y1, float x2, float y2, float x3, float y3);

#endif // CONVEX_DECOMPOSITION_H 