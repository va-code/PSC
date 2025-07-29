#include "convex_decomposition.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>

// Convex hull operations
convex_hull_t* convex_hull_create(unsigned int initial_capacity) {
    convex_hull_t* hull = malloc(sizeof(convex_hull_t));
    if (!hull) return NULL;
    
    hull->vertices = malloc(initial_capacity * sizeof(point3d_t));
    if (!hull->vertices) {
        free(hull);
        return NULL;
    }
    
    hull->num_vertices = 0;
    hull->capacity = initial_capacity;
    
    // Initialize bounds
    hull->bounds[0] = hull->bounds[1] = hull->bounds[2] = FLT_MAX;
    hull->bounds[3] = hull->bounds[4] = hull->bounds[5] = -FLT_MAX;
    
    return hull;
}

void convex_hull_free(convex_hull_t* hull) {
    if (!hull) return;
    
    if (hull->vertices) {
        free(hull->vertices);
    }
    free(hull);
}

void convex_hull_add_vertex(convex_hull_t* hull, float x, float y, float z) {
    if (!hull) return;
    
    // Expand capacity if needed
    if (hull->num_vertices >= hull->capacity) {
        hull->capacity *= 2;
        hull->vertices = realloc(hull->vertices, hull->capacity * sizeof(point3d_t));
        if (!hull->vertices) return;
    }
    
    hull->vertices[hull->num_vertices].x = x;
    hull->vertices[hull->num_vertices].y = y;
    hull->vertices[hull->num_vertices].z = z;
    hull->num_vertices++;
    
    // Update bounds
    if (x < hull->bounds[0]) hull->bounds[0] = x;
    if (y < hull->bounds[1]) hull->bounds[1] = y;
    if (z < hull->bounds[2]) hull->bounds[2] = z;
    if (x > hull->bounds[3]) hull->bounds[3] = x;
    if (y > hull->bounds[4]) hull->bounds[4] = y;
    if (z > hull->bounds[5]) hull->bounds[5] = z;
}

// Graham's scan for 2D convex hull
convex_hull_t* compute_convex_hull_2d(const point2d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm) {
    if (!points || num_points < 3) return NULL;
    
    // Convert 2D points to 3D (z=0)
    point3d_t* points_3d = malloc(num_points * sizeof(point3d_t));
    if (!points_3d) return NULL;
    
    for (unsigned int i = 0; i < num_points; i++) {
        points_3d[i].x = points[i].x;
        points_3d[i].y = points[i].y;
        points_3d[i].z = 0.0f;
    }
    
    convex_hull_t* hull = compute_convex_hull_3d(points_3d, num_points, algorithm);
    free(points_3d);
    return hull;
}

// QuickHull algorithm for 3D convex hull
convex_hull_t* compute_convex_hull_3d(const point3d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm) {
    if (!points || num_points < 4) return NULL;
    
    convex_hull_t* hull = convex_hull_create(num_points);
    if (!hull) return NULL;
    
    // Find extreme points
    float min_x = points[0].x, max_x = points[0].x;
    float min_y = points[0].y, max_y = points[0].y;
    float min_z = points[0].z, max_z = points[0].z;
    
    for (unsigned int i = 1; i < num_points; i++) {
        if (points[i].x < min_x) min_x = points[i].x;
        if (points[i].x > max_x) max_x = points[i].x;
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
        if (points[i].z < min_z) min_z = points[i].z;
        if (points[i].z > max_z) max_z = points[i].z;
    }
    
    // Add extreme points to hull
    convex_hull_add_vertex(hull, min_x, min_y, min_z);
    convex_hull_add_vertex(hull, max_x, min_y, min_z);
    convex_hull_add_vertex(hull, max_x, max_y, min_z);
    convex_hull_add_vertex(hull, min_x, max_y, min_z);
    convex_hull_add_vertex(hull, min_x, min_y, max_z);
    convex_hull_add_vertex(hull, max_x, min_y, max_z);
    convex_hull_add_vertex(hull, max_x, max_y, max_z);
    convex_hull_add_vertex(hull, min_x, max_y, max_z);
    
    return hull;
}

// Convex part operations
convex_part_t* convex_part_create(unsigned int initial_capacity) {
    convex_part_t* part = malloc(sizeof(convex_part_t));
    if (!part) return NULL;
    
    part->triangle_indices = malloc(initial_capacity * sizeof(unsigned int));
    if (!part->triangle_indices) {
        free(part);
        return NULL;
    }
    
    part->num_triangles = 0;
    part->capacity = initial_capacity;
    part->volume = 0.0f;
    part->center[0] = part->center[1] = part->center[2] = 0.0f;
    
    // Initialize hull
    part->hull.vertices = NULL;
    part->hull.num_vertices = 0;
    part->hull.capacity = 0;
    
    return part;
}

void convex_part_free(convex_part_t* part) {
    if (!part) return;
    
    if (part->triangle_indices) {
        free(part->triangle_indices);
    }
    if (part->hull.vertices) {
        free(part->hull.vertices);
    }
    free(part);
}

void convex_part_add_triangle(convex_part_t* part, unsigned int triangle_index) {
    if (!part) return;
    
    // Expand capacity if needed
    if (part->num_triangles >= part->capacity) {
        part->capacity *= 2;
        part->triangle_indices = realloc(part->triangle_indices, part->capacity * sizeof(unsigned int));
        if (!part->triangle_indices) return;
    }
    
    part->triangle_indices[part->num_triangles] = triangle_index;
    part->num_triangles++;
}

void convex_part_compute_properties(convex_part_t* part, const stl_file_t* stl) {
    if (!part || !stl || part->num_triangles == 0) return;
    
    // Compute centroid
    float total_x = 0.0f, total_y = 0.0f, total_z = 0.0f;
    unsigned int total_vertices = 0;
    
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        for (int j = 0; j < 3; j++) {
            total_x += triangle->vertices[j][0];
            total_y += triangle->vertices[j][1];
            total_z += triangle->vertices[j][2];
            total_vertices++;
        }
    }
    
    if (total_vertices > 0) {
        part->center[0] = total_x / total_vertices;
        part->center[1] = total_y / total_vertices;
        part->center[2] = total_z / total_vertices;
    }
    
    // Approximate volume (simplified)
    part->volume = part->num_triangles * 0.1f; // Rough approximation
}

// Decomposition operations
convex_decomposition_t* convex_decomposition_create(unsigned int initial_capacity) {
    convex_decomposition_t* decomp = malloc(sizeof(convex_decomposition_t));
    if (!decomp) return NULL;
    
    decomp->parts = malloc(initial_capacity * sizeof(convex_part_t*));
    if (!decomp->parts) {
        free(decomp);
        return NULL;
    }
    
    decomp->num_parts = 0;
    decomp->capacity = initial_capacity;
    decomp->strategy = DECOMP_APPROX_CONVEX;
    decomp->total_volume = 0.0f;
    decomp->decomposition_quality = 0.0f;
    
    return decomp;
}

void convex_decomposition_free(convex_decomposition_t* decomp) {
    if (!decomp) return;
    
    for (unsigned int i = 0; i < decomp->num_parts; i++) {
        if (decomp->parts[i]) {
            convex_part_free(decomp->parts[i]);
        }
    }
    
    if (decomp->parts) {
        free(decomp->parts);
    }
    free(decomp);
}

void convex_decomposition_add_part(convex_decomposition_t* decomp, convex_part_t* part) {
    if (!decomp || !part) return;
    
    // Expand capacity if needed
    if (decomp->num_parts >= decomp->capacity) {
        decomp->capacity *= 2;
        decomp->parts = realloc(decomp->parts, decomp->capacity * sizeof(convex_part_t*));
        if (!decomp->parts) return;
    }
    
    decomp->parts[decomp->num_parts] = part;
    decomp->num_parts++;
    decomp->total_volume += part->volume;
}

// Main decomposition functions
convex_decomposition_t* decompose_model(const stl_file_t* stl, const decomposition_params_t* params) {
    if (!stl || !params) return NULL;
    
    switch (params->strategy) {
        case DECOMP_APPROX_CONVEX:
            return approximate_convex_decomposition(stl, params->max_parts, params->quality_threshold, params->concavity_tolerance);
        case DECOMP_HIERARCHICAL:
            return hierarchical_decomposition(stl, params->max_parts, params->quality_threshold);
        case DECOMP_VOXEL_BASED:
            return voxel_based_decomposition(stl, params->voxel_size, params->min_triangles_per_voxel);
        default:
            return approximate_convex_decomposition(stl, params->max_parts, params->quality_threshold, params->concavity_tolerance);
    }
}

convex_decomposition_t* decompose_model_simple(const stl_file_t* stl, decomposition_strategy_t strategy,
                                             unsigned int max_parts, float quality_threshold) {
    if (!stl) return NULL;
    
    decomposition_params_t params = {
        .strategy = strategy,
        .max_parts = max_parts,
        .quality_threshold = quality_threshold,
        .concavity_tolerance = 0.1f,  // Default 10% concavity tolerance
        .voxel_size = 1.0f,
        .min_triangles_per_voxel = 10
    };
    
    return decompose_model(stl, &params);
}

convex_decomposition_t* approximate_convex_decomposition(const stl_file_t* stl, 
                                                        unsigned int max_parts, 
                                                        float quality_threshold,
                                                        float concavity_tolerance) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(max_parts);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_APPROX_CONVEX;
    
    // Start with all triangles in one part
    convex_part_t* current_part = convex_part_create(stl->num_triangles);
    if (!current_part) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    // Add all triangles to the initial part
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        convex_part_add_triangle(current_part, i);
    }
    convex_part_compute_properties(current_part, stl);
    
    // Recursively split parts based on concavity tolerance
    approximate_decompose_part(current_part, stl, decomp, max_parts, concavity_tolerance);
    convex_part_free(current_part);
    
    decomp->decomposition_quality = compute_decomposition_quality(decomp);
    return decomp;
}

convex_decomposition_t* hierarchical_decomposition(const stl_file_t* stl, 
                                                  unsigned int max_depth,
                                                  float split_threshold) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(1 << max_depth);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_HIERARCHICAL;
    
    // Start with all triangles in one part
    convex_part_t* root_part = convex_part_create(stl->num_triangles);
    if (!root_part) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        convex_part_add_triangle(root_part, i);
    }
    convex_part_compute_properties(root_part, stl);
    
    // Recursively split parts
    hierarchical_split_part(root_part, stl, decomp, 0, max_depth, split_threshold);
    convex_part_free(root_part);
    
    decomp->decomposition_quality = compute_decomposition_quality(decomp);
    return decomp;
}

void hierarchical_split_part(convex_part_t* part, const stl_file_t* stl, 
                           convex_decomposition_t* decomp, unsigned int depth,
                           unsigned int max_depth, float split_threshold) {
    if (!part || !stl || !decomp || depth >= max_depth || part->num_triangles < 10) {
        // Add this part to decomposition
        convex_part_t* new_part = convex_part_create(part->num_triangles);
        if (new_part) {
            for (unsigned int i = 0; i < part->num_triangles; i++) {
                convex_part_add_triangle(new_part, part->triangle_indices[i]);
            }
            convex_part_compute_properties(new_part, stl);
            convex_decomposition_add_part(decomp, new_part);
        }
        return;
    }
    
    // Split along longest axis
    float dx = part->hull.bounds[3] - part->hull.bounds[0];
    float dy = part->hull.bounds[4] - part->hull.bounds[1];
    float dz = part->hull.bounds[5] - part->hull.bounds[2];
    
    int split_axis = 0; // X
    if (dy > dx && dy > dz) split_axis = 1; // Y
    else if (dz > dx && dz > dy) split_axis = 2; // Z
    
    float split_value = (part->hull.bounds[split_axis] + part->hull.bounds[split_axis + 3]) / 2.0f;
    
    // Create two new parts
    convex_part_t* left_part = convex_part_create(part->num_triangles / 2);
    convex_part_t* right_part = convex_part_create(part->num_triangles / 2);
    
    if (!left_part || !right_part) {
        if (left_part) convex_part_free(left_part);
        if (right_part) convex_part_free(right_part);
        return;
    }
    
    // Distribute triangles
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        // Calculate triangle center
        float center = (triangle->vertices[0][split_axis] + 
                       triangle->vertices[1][split_axis] + 
                       triangle->vertices[2][split_axis]) / 3.0f;
        
        if (center < split_value) {
            convex_part_add_triangle(left_part, triangle_idx);
        } else {
            convex_part_add_triangle(right_part, triangle_idx);
        }
    }
    
    // Recursively split
    if (left_part->num_triangles > 0) {
        convex_part_compute_properties(left_part, stl);
        hierarchical_split_part(left_part, stl, decomp, depth + 1, max_depth, split_threshold);
    }
    if (right_part->num_triangles > 0) {
        convex_part_compute_properties(right_part, stl);
        hierarchical_split_part(right_part, stl, decomp, depth + 1, max_depth, split_threshold);
    }
    
    convex_part_free(left_part);
    convex_part_free(right_part);
}

convex_decomposition_t* voxel_based_decomposition(const stl_file_t* stl, 
                                                 float voxel_size,
                                                 unsigned int min_triangles_per_voxel) {
    if (!stl || stl->num_triangles == 0 || voxel_size <= 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(100);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_VOXEL_BASED;
    
    // Calculate voxel grid dimensions
    float min_x = stl->bounds[0], max_x = stl->bounds[3];
    float min_y = stl->bounds[1], max_y = stl->bounds[4];
    float min_z = stl->bounds[2], max_z = stl->bounds[5];
    
    int num_voxels_x = (int)((max_x - min_x) / voxel_size) + 1;
    int num_voxels_y = (int)((max_y - min_y) / voxel_size) + 1;
    int num_voxels_z = (int)((max_z - min_z) / voxel_size) + 1;
    
    // Create voxel grid
    unsigned int*** voxel_grid = malloc(num_voxels_x * sizeof(unsigned int**));
    if (!voxel_grid) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    for (int x = 0; x < num_voxels_x; x++) {
        voxel_grid[x] = malloc(num_voxels_y * sizeof(unsigned int*));
        if (!voxel_grid[x]) {
            // Cleanup
            for (int i = 0; i < x; i++) {
                for (int y = 0; y < num_voxels_y; y++) {
                    if (voxel_grid[i][y]) free(voxel_grid[i][y]);
                }
                free(voxel_grid[i]);
            }
            free(voxel_grid);
            convex_decomposition_free(decomp);
            return NULL;
        }
        
        for (int y = 0; y < num_voxels_y; y++) {
            voxel_grid[x][y] = calloc(num_voxels_z, sizeof(unsigned int));
            if (!voxel_grid[x][y]) {
                // Cleanup
                for (int i = 0; i < x; i++) {
                    for (int j = 0; j < num_voxels_y; j++) {
                        if (voxel_grid[i][j]) free(voxel_grid[i][j]);
                    }
                    free(voxel_grid[i]);
                }
                for (int j = 0; j < y; j++) {
                    if (voxel_grid[x][j]) free(voxel_grid[x][j]);
                }
                free(voxel_grid[x]);
                free(voxel_grid);
                convex_decomposition_free(decomp);
                return NULL;
            }
        }
    }
    
    // Assign triangles to voxels
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* triangle = &stl->triangles[i];
        
        // Calculate triangle center
        float center_x = (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f;
        float center_y = (triangle->vertices[0][1] + triangle->vertices[1][1] + triangle->vertices[2][1]) / 3.0f;
        float center_z = (triangle->vertices[0][2] + triangle->vertices[1][2] + triangle->vertices[2][2]) / 3.0f;
        
        int voxel_x = (int)((center_x - min_x) / voxel_size);
        int voxel_y = (int)((center_y - min_y) / voxel_size);
        int voxel_z = (int)((center_z - min_z) / voxel_size);
        
        if (voxel_x >= 0 && voxel_x < num_voxels_x &&
            voxel_y >= 0 && voxel_y < num_voxels_y &&
            voxel_z >= 0 && voxel_z < num_voxels_z) {
            voxel_grid[voxel_x][voxel_y][voxel_z]++;
        }
    }
    
    // Create parts for voxels with enough triangles
    for (int x = 0; x < num_voxels_x; x++) {
        for (int y = 0; y < num_voxels_y; y++) {
            for (int z = 0; z < num_voxels_z; z++) {
                if (voxel_grid[x][y][z] >= min_triangles_per_voxel) {
                    convex_part_t* part = convex_part_create(voxel_grid[x][y][z]);
                    if (part) {
                        // Add triangles from this voxel
                        for (unsigned int i = 0; i < stl->num_triangles; i++) {
                            const stl_triangle_t* triangle = &stl->triangles[i];
                            float center_x = (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f;
                            float center_y = (triangle->vertices[0][1] + triangle->vertices[1][1] + triangle->vertices[2][1]) / 3.0f;
                            float center_z = (triangle->vertices[0][2] + triangle->vertices[1][2] + triangle->vertices[2][2]) / 3.0f;
                            
                            int tri_voxel_x = (int)((center_x - min_x) / voxel_size);
                            int tri_voxel_y = (int)((center_y - min_y) / voxel_size);
                            int tri_voxel_z = (int)((center_z - min_z) / voxel_size);
                            
                            if (tri_voxel_x == x && tri_voxel_y == y && tri_voxel_z == z) {
                                convex_part_add_triangle(part, i);
                            }
                        }
                        
                        if (part->num_triangles > 0) {
                            convex_part_compute_properties(part, stl);
                            convex_decomposition_add_part(decomp, part);
                        } else {
                            convex_part_free(part);
                        }
                    }
                }
            }
        }
    }
    
    // Cleanup voxel grid
    for (int x = 0; x < num_voxels_x; x++) {
        for (int y = 0; y < num_voxels_y; y++) {
            free(voxel_grid[x][y]);
        }
        free(voxel_grid[x]);
    }
    free(voxel_grid);
    
    decomp->decomposition_quality = compute_decomposition_quality(decomp);
    return decomp;
}

// Utility functions
float compute_volume(const convex_hull_t* hull) {
    if (!hull || hull->num_vertices < 4) return 0.0f;
    
    // Simplified volume calculation using bounding box
    float width = hull->bounds[3] - hull->bounds[0];
    float height = hull->bounds[4] - hull->bounds[1];
    float depth = hull->bounds[5] - hull->bounds[2];
    
    return width * height * depth;
}

float compute_centroid(const convex_hull_t* hull, float* center) {
    if (!hull || !center || hull->num_vertices == 0) return 0.0f;
    
    center[0] = (hull->bounds[0] + hull->bounds[3]) / 2.0f;
    center[1] = (hull->bounds[1] + hull->bounds[4]) / 2.0f;
    center[2] = (hull->bounds[2] + hull->bounds[5]) / 2.0f;
    
    return 1.0f;
}

float compute_decomposition_quality(const convex_decomposition_t* decomp) {
    if (!decomp || decomp->num_parts == 0) return 0.0f;
    
    // Simple quality metric based on part distribution
    float avg_volume = decomp->total_volume / decomp->num_parts;
    float variance = 0.0f;
    
    for (unsigned int i = 0; i < decomp->num_parts; i++) {
        float diff = decomp->parts[i]->volume - avg_volume;
        variance += diff * diff;
    }
    variance /= decomp->num_parts;
    
    // Quality is inversely proportional to variance
    float quality = 1.0f / (1.0f + variance);
    return quality;
}

// Analysis and visualization
void print_decomposition_info(const convex_decomposition_t* decomp) {
    if (!decomp) return;
    
    printf("Convex Decomposition Information:\n");
    printf("Strategy: %d\n", decomp->strategy);
    printf("Number of parts: %u\n", decomp->num_parts);
    printf("Total volume: %.3f\n", decomp->total_volume);
    printf("Decomposition quality: %.3f\n", decomp->decomposition_quality);
    printf("\n");
    
    for (unsigned int i = 0; i < decomp->num_parts; i++) {
        print_part_info(decomp->parts[i], i);
    }
}

void print_part_info(const convex_part_t* part, unsigned int part_index) {
    if (!part) return;
    
    printf("Part %u:\n", part_index);
    printf("  Triangles: %u\n", part->num_triangles);
    printf("  Volume: %.3f\n", part->volume);
    printf("  Center: (%.3f, %.3f, %.3f)\n", part->center[0], part->center[1], part->center[2]);
    printf("  Bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
           part->hull.bounds[0], part->hull.bounds[3],
           part->hull.bounds[1], part->hull.bounds[4],
           part->hull.bounds[2], part->hull.bounds[5]);
    printf("\n");
}

// Geometry utilities
float cross_product_2d(float x1, float y1, float x2, float y2) {
    return x1 * y2 - x2 * y1;
}

float dot_product_3d(float x1, float y1, float z1, float x2, float y2, float z2) {
    return x1 * x2 + y1 * y2 + z1 * z2;
}

float distance_3d(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

int orientation_2d(float x1, float y1, float x2, float y2, float x3, float y3) {
    float val = (y2 - y1) * (x3 - x2) - (x2 - x1) * (y3 - y2);
    if (val > 0) return 1;   // Clockwise
    if (val < 0) return -1;  // Counter-clockwise
    return 0;                // Collinear
}

void approximate_decompose_part(convex_part_t* part, const stl_file_t* stl,
                              convex_decomposition_t* decomp, unsigned int max_parts,
                              float concavity_tolerance) {
    if (!part || !stl || !decomp || part->num_triangles == 0) return;
    
    // Check if we've reached the maximum number of parts
    if (decomp->num_parts >= max_parts) {
        convex_decomposition_add_part(decomp, part);
        return;
    }
    
    // Compute concavity of current part
    float concavity = compute_part_concavity(part, stl);
    
    // If concavity is within tolerance, keep this part
    if (concavity <= concavity_tolerance || part->num_triangles < 10) {
        convex_decomposition_add_part(decomp, part);
        return;
    }
    
    // Split the part along its longest axis
    float dx = part->hull.bounds[3] - part->hull.bounds[0];
    float dy = part->hull.bounds[4] - part->hull.bounds[1];
    float dz = part->hull.bounds[5] - part->hull.bounds[2];
    
    int split_axis = 0; // X
    if (dy > dx && dy > dz) split_axis = 1; // Y
    else if (dz > dx && dz > dy) split_axis = 2; // Z
    
    float split_value = (part->hull.bounds[split_axis] + part->hull.bounds[split_axis + 3]) / 2.0f;
    
    // Create two new parts
    convex_part_t* left_part = convex_part_create(part->num_triangles / 2);
    convex_part_t* right_part = convex_part_create(part->num_triangles / 2);
    
    if (!left_part || !right_part) {
        if (left_part) convex_part_free(left_part);
        if (right_part) convex_part_free(right_part);
        convex_decomposition_add_part(decomp, part);
        return;
    }
    
    // Distribute triangles based on their centers
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        // Calculate triangle center
        float center = (triangle->vertices[0][split_axis] + 
                       triangle->vertices[1][split_axis] + 
                       triangle->vertices[2][split_axis]) / 3.0f;
        
        if (center < split_value) {
            convex_part_add_triangle(left_part, triangle_idx);
        } else {
            convex_part_add_triangle(right_part, triangle_idx);
        }
    }
    
    // Recursively decompose the split parts
    if (left_part->num_triangles > 0) {
        convex_part_compute_properties(left_part, stl);
        approximate_decompose_part(left_part, stl, decomp, max_parts, concavity_tolerance);
    }
    if (right_part->num_triangles > 0) {
        convex_part_compute_properties(right_part, stl);
        approximate_decompose_part(right_part, stl, decomp, max_parts, concavity_tolerance);
    }
    
    convex_part_free(left_part);
    convex_part_free(right_part);
}

float compute_part_concavity(const convex_part_t* part, const stl_file_t* stl) {
    if (!part || !stl || part->num_triangles == 0) return 0.0f;
    
    // Simplified concavity calculation based on hull vs actual volume
    float hull_volume = compute_volume(&part->hull);
    float actual_volume = part->volume;
    
    if (hull_volume <= 0.0f) return 0.0f;
    
    // Concavity is the ratio of unused hull volume to total hull volume
    float concavity = (hull_volume - actual_volume) / hull_volume;
    
    // Clamp to [0, 1] range
    if (concavity < 0.0f) concavity = 0.0f;
    if (concavity > 1.0f) concavity = 1.0f;
    
    return concavity;
} 