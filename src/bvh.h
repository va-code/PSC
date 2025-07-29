#ifndef BVH_H
#define BVH_H

#include "stl_parser.h"
#include <stdint.h>

// BVH node types
typedef enum {
    BVH_LEAF,    // Leaf node containing triangles
    BVH_INTERNAL // Internal node with children
} bvh_node_type_t;

// BVH node structure
typedef struct bvh_node {
    bvh_node_type_t type;
    float bounds[6];  // [min_x, min_y, min_z, max_x, max_y, max_z]
    
    union {
        struct {
            unsigned int* triangle_indices;  // Indices into the original triangle array
            unsigned int num_triangles;
        } leaf;
        
        struct {
            struct bvh_node* left;
            struct bvh_node* right;
        } internal;
    } data;
} bvh_node_t;

// BVH tree structure
typedef struct {
    bvh_node_t* root;
    unsigned int num_nodes;
    unsigned int max_depth;
    unsigned int max_triangles_per_leaf;
} bvh_tree_t;

// Spatial partition structure
typedef struct {
    bvh_tree_t* bvh;
    unsigned int* partition_ids;  // Maps triangle index to partition ID
    unsigned int num_partitions;
    float* partition_bounds;      // Bounds for each partition [min_x, min_y, min_z, max_x, max_y, max_z]
} spatial_partition_t;

// Coordinate sorting options
typedef enum {
    SORT_X,  // Sort by X coordinate
    SORT_Y,  // Sort by Y coordinate
    SORT_Z,  // Sort by Z coordinate
    SORT_XY, // Sort by X, then Y
    SORT_XZ, // Sort by X, then Z
    SORT_YZ, // Sort by Y, then Z
    SORT_XYZ // Sort by X, then Y, then Z
} sort_axis_t;

// Function declarations
bvh_tree_t* bvh_create(const stl_file_t* stl, unsigned int max_triangles_per_leaf);
void bvh_free(bvh_tree_t* bvh);
void bvh_free_node(bvh_node_t* node);
bvh_node_t* bvh_build_recursive(const stl_file_t* stl, unsigned int* triangle_indices, 
                                unsigned int num_triangles, unsigned int depth, 
                                unsigned int max_depth, unsigned int max_triangles_per_leaf,
                                sort_axis_t sort_axis);
void bvh_calculate_bounds(bvh_node_t* node, const stl_file_t* stl);
void bvh_sort_triangles_by_axis(unsigned int* triangle_indices, unsigned int num_triangles,
                                const stl_file_t* stl, sort_axis_t sort_axis);
float bvh_get_center_coordinate(const stl_triangle_t* triangle, sort_axis_t axis);
int bvh_compare_triangles(const void* a, const void* b, void* arg);

// Spatial partitioning functions
spatial_partition_t* spatial_partition_create(const stl_file_t* stl, unsigned int num_partitions,
                                             sort_axis_t sort_axis);
void spatial_partition_free(spatial_partition_t* partition);
unsigned int* spatial_partition_get_triangles_in_region(const spatial_partition_t* partition,
                                                        float bounds[6], unsigned int* num_triangles);
void spatial_partition_print_info(const spatial_partition_t* partition);

// Utility functions
void bvh_print_tree(const bvh_tree_t* bvh, int depth);
void bvh_print_node(const bvh_node_t* node, int depth);
float bvh_calculate_surface_area(const float bounds[6]);
int bvh_intersects_bounds(const float bounds1[6], const float bounds2[6]);

#endif // BVH_H 