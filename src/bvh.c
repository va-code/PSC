#include "bvh.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// Comparison structure for qsort
typedef struct {
    const stl_file_t* stl;
    sort_axis_t sort_axis;
} sort_context_t;

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

void bvh_free(bvh_tree_t* bvh) {
    if (!bvh) return;
    
    if (bvh->root) {
        bvh_free_node(bvh->root);
    }
    free(bvh);
}

void bvh_free_node(bvh_node_t* node) {
    if (!node) return;
    
    if (node->type == BVH_INTERNAL) {
        bvh_free_node(node->data.internal.left);
        bvh_free_node(node->data.internal.right);
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
        bvh_calculate_bounds(node, stl);
        
        return node;
    }
    
    // Sort triangles by current axis
    bvh_sort_triangles_by_axis(triangle_indices, num_triangles, stl, current_axis);
    
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
        if (node->data.internal.left) bvh_free_node(node->data.internal.left);
        if (node->data.internal.right) bvh_free_node(node->data.internal.right);
        free(node);
        return NULL;
    }
    
    // Calculate bounds for this internal node
    bvh_calculate_bounds(node, stl);
    
    return node;
}

void bvh_calculate_bounds(bvh_node_t* node, const stl_file_t* stl) {
    if (!node || !stl) return;
    
    if (node->type == BVH_LEAF) {
        // Calculate bounds from triangles in this leaf
        if (node->data.leaf.num_triangles == 0) return;
        
        // Initialize bounds with first triangle
        unsigned int first_triangle_idx = node->data.leaf.triangle_indices[0];
        const stl_triangle_t* first_triangle = &stl->triangles[first_triangle_idx];
        
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

void bvh_sort_triangles_by_axis(unsigned int* triangle_indices, unsigned int num_triangles,
                                const stl_file_t* stl, sort_axis_t sort_axis) {
    if (!triangle_indices || !stl || num_triangles == 0) return;
    
    sort_context_t context = {stl, sort_axis};
    qsort_r(triangle_indices, num_triangles, sizeof(unsigned int), bvh_compare_triangles, &context);
}

float bvh_get_center_coordinate(const stl_triangle_t* triangle, sort_axis_t axis) {
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

int bvh_compare_triangles(const void* a, const void* b, void* arg) {
    sort_context_t* context = (sort_context_t*)arg;
    unsigned int idx_a = *(unsigned int*)a;
    unsigned int idx_b = *(unsigned int*)b;
    
    const stl_triangle_t* triangle_a = &context->stl->triangles[idx_a];
    const stl_triangle_t* triangle_b = &context->stl->triangles[idx_b];
    
    float coord_a = bvh_get_center_coordinate(triangle_a, context->sort_axis);
    float coord_b = bvh_get_center_coordinate(triangle_b, context->sort_axis);
    
    if (coord_a < coord_b) return -1;
    if (coord_a > coord_b) return 1;
    return 0;
}

// Spatial partitioning functions
spatial_partition_t* spatial_partition_create(const stl_file_t* stl, unsigned int num_partitions,
                                             sort_axis_t sort_axis) {
    if (!stl || num_partitions == 0) return NULL;
    
    spatial_partition_t* partition = malloc(sizeof(spatial_partition_t));
    if (!partition) return NULL;
    
    partition->num_partitions = num_partitions;
    partition->partition_ids = malloc(stl->num_triangles * sizeof(unsigned int));
    partition->partition_bounds = malloc(num_partitions * 6 * sizeof(float));
    
    if (!partition->partition_ids || !partition->partition_bounds) {
        spatial_partition_free(partition);
        return NULL;
    }
    
    // Create BVH for efficient spatial queries
    partition->bvh = bvh_create(stl, 10);
    if (!partition->bvh) {
        spatial_partition_free(partition);
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

void spatial_partition_free(spatial_partition_t* partition) {
    if (!partition) return;
    
    if (partition->bvh) {
        bvh_free(partition->bvh);
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

void spatial_partition_print_info(const spatial_partition_t* partition) {
    if (!partition) return;
    
    printf("Spatial Partition Information:\n");
    printf("Number of partitions: %u\n", partition->num_partitions);
    
    for (unsigned int i = 0; i < partition->num_partitions; i++) {
        unsigned int base_idx = i * 6;
        printf("Partition %u: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               i,
               partition->partition_bounds[base_idx + 0], partition->partition_bounds[base_idx + 3],
               partition->partition_bounds[base_idx + 1], partition->partition_bounds[base_idx + 4],
               partition->partition_bounds[base_idx + 2], partition->partition_bounds[base_idx + 5]);
    }
}

// Utility functions
void bvh_print_tree(const bvh_tree_t* bvh, int depth) {
    if (!bvh || !bvh->root) return;
    
    printf("BVH Tree (max depth: %u, max triangles per leaf: %u):\n", 
           bvh->max_depth, bvh->max_triangles_per_leaf);
    bvh_print_node(bvh->root, 0);
}

void bvh_print_node(const bvh_node_t* node, int depth) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    if (node->type == BVH_LEAF) {
        printf("Leaf: %u triangles, bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               node->data.leaf.num_triangles,
               node->bounds[0], node->bounds[3],
               node->bounds[1], node->bounds[4],
               node->bounds[2], node->bounds[5]);
    } else {
        printf("Internal: bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
               node->bounds[0], node->bounds[3],
               node->bounds[1], node->bounds[4],
               node->bounds[2], node->bounds[5]);
        
        bvh_print_node(node->data.internal.left, depth + 1);
        bvh_print_node(node->data.internal.right, depth + 1);
    }
}

float bvh_calculate_surface_area(const float bounds[6]) {
    float width = bounds[3] - bounds[0];
    float height = bounds[4] - bounds[1];
    float depth = bounds[5] - bounds[2];
    
    return 2.0f * (width * height + width * depth + height * depth);
}

int bvh_intersects_bounds(const float bounds1[6], const float bounds2[6]) {
    return (bounds1[0] <= bounds2[3] && bounds1[3] >= bounds2[0] &&
            bounds1[1] <= bounds2[4] && bounds1[4] >= bounds2[1] &&
            bounds1[2] <= bounds2[5] && bounds1[5] >= bounds2[2]);
} 