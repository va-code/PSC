#include "bvh.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// Comparison structure for qsort
//this structure takes is a stored verson of the stl where it has sorted based on an appliedaxis so the bvh can work more efficiently
//I am a bit comfused what axis it comes from
typedef struct {
    const stl_file_t* stl;
    sort_axis_t sort_axis;
} sort_context_t;

// Global context for sorting (not thread-safe, but works for our use case)
static sort_context_t g_sort_context;
//takes a pointer to a sorted stl and a max number of triangles to be included in the final nodes.
bvh_tree_t* bvh_create(const stl_file_t* stl, unsigned int max_triangles_per_leaf) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    bvh_tree_t* bvh = malloc(sizeof(bvh_tree_t));
    if (!bvh) return NULL;
    
    bvh->max_triangles_per_leaf = max_triangles_per_leaf;
    bvh->num_nodes = 0;
    bvh->max_depth = 0;
    
    // Create array of triangle indices
    unsigned int* triangle_indices = malloc(stl->num_triangles * sizeof(unsigned int));
    if (!triangle_indices) {
        free(bvh);
        return NULL;
    }
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        triangle_indices[i] = i;
    }
    
    // Build BVH tree recursively (default to SORT_XYZ for balanced tree)
    bvh->root = bvh_build_recursive(stl, triangle_indices, stl->num_triangles, 0, 20, max_triangles_per_leaf, SORT_XYZ);
    
    free(triangle_indices);
    
    if (!bvh->root) {
        free(bvh);
        return NULL;
    }
    
    return bvh;
}
//takes a bvh tree and frees it's memory when done with it
void free_bvh(bvh_tree_t* bvh) {
    if (!bvh) return;
    
    if (bvh->root) {
        free_bvh_node(bvh->root);
    }
    free(bvh);
}
//used under bvh_free to free the memory of each node that was used
void free_bvh_node(bvh_node_t* node) {
    if (!node) return;
    
    if (node->type == BVH_INTERNAL) {
        free_bvh_node(node->data.internal.left);
        free_bvh_node(node->data.internal.right);
    } else if (node->type == BVH_LEAF) {
        if (node->data.leaf.triangle_indices) {
            free(node->data.leaf.triangle_indices);
        }
    }
    
    free(node);
}

bvh_node_t* bvh_build_recursive(const stl_file_t* stl, unsigned int* triangle_indices, 
                                unsigned int num_triangles, unsigned int depth, 
                                unsigned int max_depth, unsigned int max_triangles_per_leaf,
                                sort_axis_t sort_axis) {
    if (!stl || !triangle_indices || num_triangles == 0) return NULL;
    
    bvh_node_t* node = malloc(sizeof(bvh_node_t));
    if (!node) return NULL;
    
    // Determine sort axis based on depth (cycle through X, Y, Z for better balance)
    sort_axis_t current_axis = (sort_axis_t)(depth % 3);
    
    // If we have few triangles or reached max depth, create leaf node
    if (num_triangles <= max_triangles_per_leaf || depth >= max_depth) {
        node->type = BVH_LEAF;
        node->data.leaf.num_triangles = num_triangles;
        node->data.leaf.triangle_indices = malloc(num_triangles * sizeof(unsigned int));
        
        if (!node->data.leaf.triangle_indices) {
            free(node);
            return NULL;
        }
        
        // Copy triangle indices
        memcpy(node->data.leaf.triangle_indices, triangle_indices, num_triangles * sizeof(unsigned int));
        
        // Calculate bounds for this leaf
        calculate_bvh_bounds(node, stl);
        
        return node;
    }
    
    // Sort triangles by current axis
    sort_triangles_by_axis(triangle_indices, num_triangles, stl, current_axis);
    
    // Split triangles into two groups
    unsigned int mid = num_triangles / 2;
    
    // Create internal node
    node->type = BVH_INTERNAL;
    
    // Recursively build left and right children
    node->data.internal.left = bvh_build_recursive(stl, triangle_indices, mid, depth + 1, 
                                                  max_depth, max_triangles_per_leaf, sort_axis);
    node->data.internal.right = bvh_build_recursive(stl, triangle_indices + mid, num_triangles - mid, 
                                                   depth + 1, max_depth, max_triangles_per_leaf, sort_axis);
    
    if (!node->data.internal.left || !node->data.internal.right) {
        // Cleanup on failure
        if (node->data.internal.left) free_bvh_node(node->data.internal.left);
        if (node->data.internal.right) free_bvh_node(node->data.internal.right);
        free(node);
        return NULL;
    }
    
    // Calculate bounds for this internal node
    calculate_bvh_bounds(node, stl);
    
    return node;
}

void calculate_bvh_bounds(bvh_node_t* node, const stl_file_t* stl) {
    if (!node || !stl) return;
    
    if (node->type == BVH_LEAF) {
        // Calculate bounds from triangles in this leaf
        if (node->data.leaf.num_triangles == 0) return;
        
        // Initialize bounds
        
        node->bounds[0] = node->bounds[1] = node->bounds[2] = FLT_MAX;  // min
        node->bounds[3] = node->bounds[4] = node->bounds[5] = -FLT_MAX; // max
        
        for (unsigned int i = 0; i < node->data.leaf.num_triangles; i++) {
            unsigned int triangle_idx = node->data.leaf.triangle_indices[i];
            const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
            
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    float val = triangle->vertices[j][k];
                    if (val < node->bounds[k]) node->bounds[k] = val;     // min
                    if (val > node->bounds[k+3]) node->bounds[k+3] = val; // max
                }
            }
        }
    } else if (node->type == BVH_INTERNAL) {
        // Calculate bounds from children
        if (node->data.internal.left && node->data.internal.right) {
            // Initialize with left child bounds
            memcpy(node->bounds, node->data.internal.left->bounds, 6 * sizeof(float));
            
            // Expand with right child bounds
            for (int i = 0; i < 3; i++) {
                if (node->data.internal.right->bounds[i] < node->bounds[i]) {
                    node->bounds[i] = node->data.internal.right->bounds[i];
                }
                if (node->data.internal.right->bounds[i+3] > node->bounds[i+3]) {
                    node->bounds[i+3] = node->data.internal.right->bounds[i+3];
                }
            }
        }
    }
}

void sort_triangles_by_axis(unsigned int* triangle_indices, unsigned int num_triangles,
                                const stl_file_t* stl, sort_axis_t sort_axis) {
    if (!triangle_indices || !stl || num_triangles == 0) return;
    
    g_sort_context.stl = stl;
    g_sort_context.sort_axis = sort_axis;
    qsort(triangle_indices, num_triangles, sizeof(unsigned int), compare_triangles);
}

float get_center_coordinate(const stl_triangle_t* triangle, sort_axis_t axis) {
    if (!triangle) return 0.0f;
    
    float center[3] = {0.0f, 0.0f, 0.0f};
    
    // Calculate triangle center
    for (int i = 0; i < 3; i++) {
        center[0] += triangle->vertices[i][0];
        center[1] += triangle->vertices[i][1];
        center[2] += triangle->vertices[i][2];
    }
    
    center[0] /= 3.0f;
    center[1] /= 3.0f;
    center[2] /= 3.0f;
    
    // Return coordinate for specified axis
    switch (axis) {
        case SORT_X: return center[0];
        case SORT_Y: return center[1];
        case SORT_Z: return center[2];
        default: return center[0]; // Default to X
    }
}

// Global context for sorting (not thread-safe, but works for our use case)
static sort_context_t g_sort_context;

int compare_triangles(const void* a, const void* b) {
    unsigned int idx_a = *(unsigned int*)a;
    unsigned int idx_b = *(unsigned int*)b;
    
    const stl_triangle_t* triangle_a = &g_sort_context.stl->triangles[idx_a];
    const stl_triangle_t* triangle_b = &g_sort_context.stl->triangles[idx_b];
    
    float coord_a = get_center_coordinate(triangle_a, g_sort_context.sort_axis);
    float coord_b = get_center_coordinate(triangle_b, g_sort_context.sort_axis);
    
    if (coord_a < coord_b) return -1;
    if (coord_a > coord_b) return 1;
    return 0;
}

// Spatial partitioning functions
spatial_partition_t* spatial_partition_create(const stl_file_t* stl, unsigned int num_partitions,
                                             sort_axis_t sort_axis) {
    (void)sort_axis; // Unused parameter
    if (!stl || num_partitions == 0) return NULL;
    
    spatial_partition_t* partition = malloc(sizeof(spatial_partition_t));
    if (!partition) return NULL;
    
    partition->num_partitions = num_partitions;
    partition->partition_ids = malloc(stl->num_triangles * sizeof(unsigned int));
    partition->partition_bounds = malloc(num_partitions * 6 * sizeof(float));
    
    if (!partition->partition_ids || !partition->partition_bounds) {
        free_spatial_partition(partition);
        return NULL;
    }
    
    // Create BVH for efficient spatial queries
    partition->bvh = bvh_create(stl, 10);
    if (!partition->bvh) {
        free_spatial_partition(partition);
        return NULL;
    }
    
    // Create partitions based on bounding box
    float total_width = stl->bounds[3] - stl->bounds[0];
    float partition_width = total_width / num_partitions;
    
    for (unsigned int i = 0; i < num_partitions; i++) {
        unsigned int base_idx = i * 6;
        partition->partition_bounds[base_idx + 0] = stl->bounds[0] + i * partition_width; // min_x
        partition->partition_bounds[base_idx + 1] = stl->bounds[1]; // min_y
        partition->partition_bounds[base_idx + 2] = stl->bounds[2]; // min_z
        partition->partition_bounds[base_idx + 3] = stl->bounds[0] + (i + 1) * partition_width; // max_x
        partition->partition_bounds[base_idx + 4] = stl->bounds[4]; // max_y
        partition->partition_bounds[base_idx + 5] = stl->bounds[5]; // max_z
    }
    
    // Assign triangles to partitions
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* triangle = &stl->triangles[i];
        float center_x = (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f;
        
        // Find which partition this triangle belongs to
        unsigned int partition_id = 0;
        for (unsigned int j = 0; j < num_partitions; j++) {
            if (center_x >= partition->partition_bounds[j * 6] && 
                center_x < partition->partition_bounds[j * 6 + 3]) {
                partition_id = j;
                break;
            }
        }
        partition->partition_ids[i] = partition_id;
    }
    
    return partition;
}

void free_spatial_partition(spatial_partition_t* partition) {
    if (!partition) return;
    
    if (partition->bvh) {
        free_bvh(partition->bvh);
    }
    if (partition->partition_ids) {
        free(partition->partition_ids);
    }
    if (partition->partition_bounds) {
        free(partition->partition_bounds);
    }
    free(partition);
}

unsigned int* spatial_partition_get_triangles_in_region(const spatial_partition_t* partition,
                                                        float bounds[6], unsigned int* num_triangles) {
    if (!partition || !bounds || !num_triangles) return NULL;
    
    // This is a simplified implementation
    // In a full implementation, you would traverse the BVH to find intersecting triangles
    
    *num_triangles = 0;
    // TODO: Implement BVH traversal to find triangles in region
    return NULL;
}

void print_spatial_partition_info_to_file(const spatial_partition_t* partition, FILE* file) {
    if (!partition || !file) return;
    
    fprintf(file, "Spatial Partition Information:\n");
    fprintf(file, "Number of partitions: %u\n", partition->num_partitions);
    
    for (unsigned int i = 0; i < partition->num_partitions; i++) {
        unsigned int base_idx = i * 6;
        fprintf(file, "Partition %u: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               i,
               partition->partition_bounds[base_idx + 0], partition->partition_bounds[base_idx + 3],
               partition->partition_bounds[base_idx + 1], partition->partition_bounds[base_idx + 4],
               partition->partition_bounds[base_idx + 2], partition->partition_bounds[base_idx + 5]);
    }
}

void print_spatial_partition_info(const spatial_partition_t* partition) {
    print_spatial_partition_info_to_file(partition, stdout);
}

// Utility functions
void print_bvh_tree_to_file(const bvh_tree_t* bvh, int depth, FILE* file) {
    (void)depth; // Unused parameter
    if (!bvh || !bvh->root || !file) return;
    
    fprintf(file, "BVH Tree (max depth: %u, max triangles per leaf: %u):\n", 
           bvh->max_depth, bvh->max_triangles_per_leaf);
    print_bvh_node_to_file(bvh->root, 0, file);
}

void print_bvh_node_to_file(const bvh_node_t* node, int depth, FILE* file) {
    if (!node || !file) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        fprintf(file, "  ");
    }
    
    if (node->type == BVH_LEAF) {
        fprintf(file, "Leaf: %u triangles, bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               node->data.leaf.num_triangles,
               node->bounds[0], node->bounds[3],
               node->bounds[1], node->bounds[4],
               node->bounds[2], node->bounds[5]);
    } else {
        fprintf(file, "Internal: bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               node->bounds[0], node->bounds[3],
               node->bounds[1], node->bounds[4],
               node->bounds[2], node->bounds[5]);
        
        print_bvh_node_to_file(node->data.internal.left, depth + 1, file);
        print_bvh_node_to_file(node->data.internal.right, depth + 1, file);
    }
}

void print_bvh_tree(const bvh_tree_t* bvh, int depth) {
    print_bvh_tree_to_file(bvh, depth, stdout);
}

void print_bvh_node(const bvh_node_t* node, int depth) {
    print_bvh_node_to_file(node, depth, stdout);
}

float calculate_surface_area(const float bounds[6]) {
    float width = bounds[3] - bounds[0];
    float height = bounds[4] - bounds[1];
    float depth = bounds[5] - bounds[2];
    
    return 2.0f * (width * height + width * depth + height * depth);
}

int intersects_bounds(const float bounds1[6], const float bounds2[6]) {
    return (bounds1[0] <= bounds2[3] && bounds1[3] >= bounds2[0] &&
            bounds1[1] <= bounds2[4] && bounds1[4] >= bounds2[1] &&
            bounds1[2] <= bounds2[5] && bounds1[5] >= bounds2[2]);
}

void spatial_partition_free(spatial_partition_t* partition) {
    free_spatial_partition(partition);
}

void spatial_partition_print_info(const spatial_partition_t* partition) {
    print_spatial_partition_info(partition);
} 