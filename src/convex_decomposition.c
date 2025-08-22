#include "convex_decomposition.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to represent connected components
typedef struct {
    int* triangle_indices;
    int count;
    int capacity;
} connected_component_t;

// Check if two triangles share at least 2 vertices (connected)
// This is faster than full edge matching and works well for most STL meshes
int triangles_are_connected(const stl_triangle_t* tri1, const stl_triangle_t* tri2) {
    const float EPSILON = 1e-6f;
    int shared_vertices = 0;
    
    // Count shared vertices between the two triangles
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float distance = sqrtf(
                (tri1->vertices[i][0] - tri2->vertices[j][0]) * (tri1->vertices[i][0] - tri2->vertices[j][0]) +
                (tri1->vertices[i][1] - tri2->vertices[j][1]) * (tri1->vertices[i][1] - tri2->vertices[j][1]) +
                (tri1->vertices[i][2] - tri2->vertices[j][2]) * (tri1->vertices[i][2] - tri2->vertices[j][2])
            );
            
            if (distance < EPSILON) {
                shared_vertices++;
                break; // Don't count the same vertex multiple times
            }
        }
    }
    
    // Triangles are connected if they share at least 2 vertices (an edge)
    return shared_vertices >= 2;
}

// Find connected components in a mesh using flood-fill algorithm
connected_component_t* find_connected_components(const stl_file_t* mesh, int* num_components) {
    if (!mesh || mesh->num_triangles == 0) {
        *num_components = 0;
        return NULL;
    }
    
    // Performance safeguard: skip component analysis for very large meshes
    if (mesh->num_triangles > 1000) {
        printf("Mesh too large (%u triangles) for component analysis, skipping\n", mesh->num_triangles);
        *num_components = 1; // Assume single component
        return NULL;
    }
    
    *num_components = 0;
    int* visited = calloc(mesh->num_triangles, sizeof(int));
    if (!visited) return NULL;
    
    // Allocate space for components (worst case: every triangle is separate)
    connected_component_t* components = malloc(mesh->num_triangles * sizeof(connected_component_t));
    if (!components) {
        free(visited);
        return NULL;
    }
    
    for (unsigned int i = 0; i < mesh->num_triangles; i++) {
        if (visited[i]) continue;
        
        // Start new component
        connected_component_t* current_component = &components[*num_components];
        current_component->capacity = 16;
        current_component->triangle_indices = malloc(current_component->capacity * sizeof(int));
        current_component->count = 0;
        
        if (!current_component->triangle_indices) {
            // Cleanup on failure
            for (int j = 0; j < *num_components; j++) {
                free(components[j].triangle_indices);
            }
            free(components);
            free(visited);
            return NULL;
        }
        
        // Flood-fill to find all connected triangles
        int* stack = malloc(mesh->num_triangles * sizeof(int));
        if (!stack) {
            free(current_component->triangle_indices);
            for (int j = 0; j < *num_components; j++) {
                free(components[j].triangle_indices);
            }
            free(components);
            free(visited);
            return NULL;
        }
        
        int stack_size = 0;
        stack[stack_size++] = i;
        
        while (stack_size > 0) {
            int current_triangle = stack[--stack_size];
            if (visited[current_triangle]) continue;
            
            visited[current_triangle] = 1;
            
            // Add to current component
            if (current_component->count >= current_component->capacity) {
                current_component->capacity *= 2;
                current_component->triangle_indices = realloc(current_component->triangle_indices, 
                                                            current_component->capacity * sizeof(int));
            }
            current_component->triangle_indices[current_component->count++] = current_triangle;
            
            // Check all other unvisited triangles for connectivity
            for (unsigned int j = 0; j < mesh->num_triangles; j++) {
                if (!visited[j] && triangles_are_connected(&mesh->triangles[current_triangle], &mesh->triangles[j])) {
                    stack[stack_size++] = j;
                }
            }
        }
        
        free(stack);
        (*num_components)++;
    }
    
    free(visited);
    return components;
}

// Create a submesh from a connected component
stl_file_t* create_submesh_from_component(const stl_file_t* original_mesh, const connected_component_t* component) {
    if (!original_mesh || !component || component->count == 0) {
        return NULL;
    }
    
    stl_file_t* submesh = malloc(sizeof(stl_file_t));
    if (!submesh) return NULL;
    
    // Copy header
    memcpy(submesh->header, original_mesh->header, 80);
    submesh->num_triangles = component->count;
    
    // Allocate triangles
    submesh->triangles = malloc(submesh->num_triangles * sizeof(stl_triangle_t));
    if (!submesh->triangles) {
        free(submesh);
        return NULL;
    }
    
    // Copy triangles from component
    for (int i = 0; i < component->count; i++) {
        int triangle_index = component->triangle_indices[i];
        
        // Bounds check
        if (triangle_index < 0 || triangle_index >= (int)original_mesh->num_triangles) {
            printf("ERROR: Invalid triangle index %d (mesh has %u triangles)\n", 
                   triangle_index, original_mesh->num_triangles);
            free(submesh->triangles);
            free(submesh);
            return NULL;
        }
        
        memcpy(&submesh->triangles[i], &original_mesh->triangles[triangle_index], sizeof(stl_triangle_t));
    }
    
    return submesh;
}

// Structure to represent a separation plane between components
typedef struct {
    float point[3];     // A point on the plane
    float normal[3];    // Plane normal vector
    float distance;     // Distance between component centroids
    int is_valid;       // 1 if this is a valid separation plane
} separation_plane_t;

// Calculate the centroid of a connected component
void calculate_component_centroid(const stl_file_t* mesh, const connected_component_t* component, float* centroid) {
    centroid[0] = centroid[1] = centroid[2] = 0.0f;
    int vertex_count = 0;
    
    for (int i = 0; i < component->count; i++) {
        int triangle_idx = component->triangle_indices[i];
        if (triangle_idx >= 0 && triangle_idx < (int)mesh->num_triangles) {
            const stl_triangle_t* triangle = &mesh->triangles[triangle_idx];
            
            for (int j = 0; j < 3; j++) {
                centroid[0] += triangle->vertices[j][0];
                centroid[1] += triangle->vertices[j][1];
                centroid[2] += triangle->vertices[j][2];
                vertex_count++;
            }
        }
    }
    
    if (vertex_count > 0) {
        centroid[0] /= vertex_count;
        centroid[1] /= vertex_count;
        centroid[2] /= vertex_count;
    }
}

// Find the optimal separation plane between two connected components
separation_plane_t find_separation_plane(const stl_file_t* mesh, 
                                        const connected_component_t* comp1, 
                                        const connected_component_t* comp2) {
    separation_plane_t result = {0};
    result.is_valid = 0;
    
    if (!mesh || !comp1 || !comp2 || comp1->count == 0 || comp2->count == 0) {
        return result;
    }
    
    // Calculate centroids of both components
    float centroid1[3], centroid2[3];
    calculate_component_centroid(mesh, comp1, centroid1);
    calculate_component_centroid(mesh, comp2, centroid2);
    
    // Calculate the vector between centroids
    float separation_vector[3] = {
        centroid2[0] - centroid1[0],
        centroid2[1] - centroid1[1],
        centroid2[2] - centroid1[2]
    };
    
    // Calculate distance between centroids
    result.distance = sqrtf(
        separation_vector[0] * separation_vector[0] +
        separation_vector[1] * separation_vector[1] +
        separation_vector[2] * separation_vector[2]
    );
    
    if (result.distance < 1e-6f) {
        // Components are too close or overlapping
        return result;
    }
    
    // Normalize the separation vector to get the plane normal
    result.normal[0] = separation_vector[0] / result.distance;
    result.normal[1] = separation_vector[1] / result.distance;
    result.normal[2] = separation_vector[2] / result.distance;
    
    // Place the plane at the midpoint between centroids
    result.point[0] = (centroid1[0] + centroid2[0]) / 2.0f;
    result.point[1] = (centroid1[1] + centroid2[1]) / 2.0f;
    result.point[2] = (centroid1[2] + centroid2[2]) / 2.0f;
    
    printf("Separation plane: point=(%.3f,%.3f,%.3f), normal=(%.3f,%.3f,%.3f), distance=%.3f\n",
           result.point[0], result.point[1], result.point[2],
           result.normal[0], result.normal[1], result.normal[2], result.distance);
    
    result.is_valid = 1;
    return result;
}

// Ray-triangle intersection using Möller-Trumbore algorithm
// Returns 1 if intersection found, 0 otherwise
// Sets t, u, v as barycentric coordinates if intersection found
int ray_triangle_intersect(const float* ray_origin, const float* ray_direction,
                          const float* v0, const float* v1, const float* v2,
                          float* t, float* u, float* v) {
    const float EPSILON = 1e-8f;
    
    // Calculate two edges of the triangle
    float edge1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
    float edge2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
    
    // Calculate cross product of ray direction and edge2
    float h[3] = {
        ray_direction[1] * edge2[2] - ray_direction[2] * edge2[1],
        ray_direction[2] * edge2[0] - ray_direction[0] * edge2[2],
        ray_direction[0] * edge2[1] - ray_direction[1] * edge2[0]
    };
    
    // Calculate determinant
    float a = edge1[0] * h[0] + edge1[1] * h[1] + edge1[2] * h[2];
    
    if (a > -EPSILON && a < EPSILON) {
        return 0; // Ray is parallel to triangle
    }
    
    float f = 1.0f / a;
    float s[3] = {ray_origin[0] - v0[0], ray_origin[1] - v0[1], ray_origin[2] - v0[2]};
    
    *u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
    if (*u < 0.0f || *u > 1.0f) {
        return 0;
    }
    
    float q[3] = {
        s[1] * edge1[2] - s[2] * edge1[1],
        s[2] * edge1[0] - s[0] * edge1[2],
        s[0] * edge1[1] - s[1] * edge1[0]
    };
    
    *v = f * (ray_direction[0] * q[0] + ray_direction[1] * q[1] + ray_direction[2] * q[2]);
    if (*v < 0.0f || *u + *v > 1.0f) {
        return 0;
    }
    
    *t = f * (edge2[0] * q[0] + edge2[1] * q[1] + edge2[2] * q[2]);
    
    return (*t > EPSILON); // Intersection found
}

// Mesh structure (if needed for other functions)
typedef struct {
    // vertices vector<Vertex>;
    // faces vector<Face>;
    float concavity_threshold;
} mesh;

// Helper function to update tree statistics
void update_tree_stats(decomposition_tree_t* tree, mesh_tree_node_t* node) {
    if (!tree || !node) return;
    
    tree->total_nodes++;
    if (node->depth > tree->max_depth_reached) {
        tree->max_depth_reached = node->depth;
    }
    if (node->is_leaf) {
        tree->leaf_nodes++;
    }
}

int check_concavity(const stl_file_t* stl, concavity_result_t* result) {
    if (!stl || stl->num_triangles == 0 || !result) {
        return 0; // Invalid input
    }
    
    // Initialize result structure
    result->concavity_score = 0.0f;
    result->worst_point_1[0] = result->worst_point_1[1] = result->worst_point_1[2] = 0.0f;
    result->worst_point_2[0] = result->worst_point_2[1] = result->worst_point_2[2] = 0.0f;
    result->worst_point_3[0] = result->worst_point_3[1] = result->worst_point_3[2] = 0.0f;
    result->worst_triangle_index_1 = -1;
    result->worst_triangle_index_2 = -1;
    result->worst_triangle_index_3 = -1;
    
    // For very small meshes, assume convex
    if (stl->num_triangles < 4) {
        result->concavity_score = 1.0f;
        return 1;
    }
    
    // Calculate mesh center point
    float center[3] = {0.0f, 0.0f, 0.0f};
    int vertex_count = 0;
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        for (int j = 0; j < 3; j++) {
            center[0] += stl->triangles[i].vertices[j][0];
            center[1] += stl->triangles[i].vertices[j][1];
            center[2] += stl->triangles[i].vertices[j][2];
            vertex_count++;
        }
    }
    
    center[0] /= vertex_count;
    center[1] /= vertex_count;
    center[2] /= vertex_count;
    
    // ROBUST GEOMETRY-BASED CONCAVITY DETECTION
    // Instead of relying on unreliable STL normals, use geometric convexity tests
    
    float worst_concavity_1 = 0.0f;  // Worst concavity score (higher = more concave)
    float worst_concavity_2 = 0.0f;  // Second worst
    float worst_concavity_3 = 0.0f;  // Third worst
    int total_samples = 0;
    int convex_samples = 0;
    
    // Sample triangles and test geometric convexity using ray-casting approach
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* triangle = &stl->triangles[i];
        
        // Calculate triangle centroid
        float centroid[3] = {
            (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f,
            (triangle->vertices[0][1] + triangle->vertices[1][1] + triangle->vertices[2][1]) / 3.0f,
            (triangle->vertices[0][2] + triangle->vertices[1][2] + triangle->vertices[2][2]) / 3.0f
        };
        
        // Calculate distance from centroid to mesh center
        float center_distance = sqrtf(
            (centroid[0] - center[0]) * (centroid[0] - center[0]) +
            (centroid[1] - center[1]) * (centroid[1] - center[1]) +
            (centroid[2] - center[2]) * (centroid[2] - center[2])
        );
        
        if (center_distance < 1e-6f) continue; // Skip if too close to center
        
        // Test concavity by checking how "inward" this point is relative to other geometry
        // Count how many other triangles are between this point and the mesh center
        float ray_direction[3] = {
            (center[0] - centroid[0]) / center_distance,
            (center[1] - centroid[1]) / center_distance,
            (center[2] - centroid[2]) / center_distance
        };
        
        int ray_intersections = 0;
        
        // Cast ray from centroid toward center and count intersections
        for (unsigned int j = 0; j < stl->num_triangles; j++) {
            if (i == j) continue; // Skip self
            
            const stl_triangle_t* other = &stl->triangles[j];
            
            // Simple ray-triangle intersection test
            // If ray intersects another triangle, this point is more "inside" (concave)
            float t, u, v;
            if (ray_triangle_intersect(centroid, ray_direction, 
                                     other->vertices[0], other->vertices[1], other->vertices[2],
                                     &t, &u, &v)) {
                if (t > 1e-6f && t < center_distance) { // Valid intersection between centroid and center
                    ray_intersections++;
                    if (ray_intersections >= 3) break; // Limit to avoid excessive computation
                }
            }
        }
        
        // Concavity score: more intersections = more concave
        // Normalize by distance to handle scale variations
        float concavity = (float)ray_intersections / (center_distance + 1.0f);
        
        total_samples++;
        
        // Optional debug: Print concavity for first few triangles
        /*if (i < 5) {
            printf("Triangle %d: concavity=%.3f, intersections=%d, centroid=(%.3f,%.3f,%.3f)\n",
                   i, concavity, ray_intersections, centroid[0], centroid[1], centroid[2]);
        }*/
        
        // Ensure spatial diversity - don't select points too close to existing ones
        float min_distance_1 = sqrtf(
            (centroid[0] - result->worst_point_1[0]) * (centroid[0] - result->worst_point_1[0]) +
            (centroid[1] - result->worst_point_1[1]) * (centroid[1] - result->worst_point_1[1]) +
            (centroid[2] - result->worst_point_1[2]) * (centroid[2] - result->worst_point_1[2])
        );
        float min_distance_2 = sqrtf(
            (centroid[0] - result->worst_point_2[0]) * (centroid[0] - result->worst_point_2[0]) +
            (centroid[1] - result->worst_point_2[1]) * (centroid[1] - result->worst_point_2[1]) +
            (centroid[2] - result->worst_point_2[2]) * (centroid[2] - result->worst_point_2[2])
        );
        
        const float MIN_SPATIAL_DISTANCE = 0.5f; // Minimum distance between worst points
        
        // Track the three worst (most concave) points with spatial diversity
        if (concavity > worst_concavity_1) {
            // New worst point found - shift previous worst to second and third place
            worst_concavity_3 = worst_concavity_2;
            result->worst_point_3[0] = result->worst_point_2[0];
            result->worst_point_3[1] = result->worst_point_2[1];
            result->worst_point_3[2] = result->worst_point_2[2];
            result->worst_triangle_index_3 = result->worst_triangle_index_2;
            
            worst_concavity_2 = worst_concavity_1;
            result->worst_point_2[0] = result->worst_point_1[0];
            result->worst_point_2[1] = result->worst_point_1[1];
            result->worst_point_2[2] = result->worst_point_1[2];
            result->worst_triangle_index_2 = result->worst_triangle_index_1;
            
            // Update the new worst
            worst_concavity_1 = concavity;
            result->worst_point_1[0] = centroid[0];
            result->worst_point_1[1] = centroid[1];
            result->worst_point_1[2] = centroid[2];
            result->worst_triangle_index_1 = (int)i;
        } else if (concavity > worst_concavity_2 && min_distance_1 >= MIN_SPATIAL_DISTANCE) {
            // New second worst point found - shift previous second worst to third place
            worst_concavity_3 = worst_concavity_2;
            result->worst_point_3[0] = result->worst_point_2[0];
            result->worst_point_3[1] = result->worst_point_2[1];
            result->worst_point_3[2] = result->worst_point_2[2];
            result->worst_triangle_index_3 = result->worst_triangle_index_2;
            
            worst_concavity_2 = concavity;
            result->worst_point_2[0] = centroid[0];
            result->worst_point_2[1] = centroid[1];
            result->worst_point_2[2] = centroid[2];
            result->worst_triangle_index_2 = (int)i;
        } else if (concavity > worst_concavity_3 && 
                   min_distance_1 >= MIN_SPATIAL_DISTANCE && 
                   min_distance_2 >= MIN_SPATIAL_DISTANCE) {
            // New third worst point found
            worst_concavity_3 = concavity;
            result->worst_point_3[0] = centroid[0];
            result->worst_point_3[1] = centroid[1];
            result->worst_point_3[2] = centroid[2];
            result->worst_triangle_index_3 = (int)i;
        }
        
        // If low concavity, consider it convex-like
        if (concavity < 0.1f) {
            convex_samples++;
        }
    }
    
    if (total_samples == 0) {
        result->concavity_score = 1.0f;
        return 1;
    }
    
    // Calculate the ratio of convex-like samples
    float convexity_ratio = (float)convex_samples / (float)total_samples;
    
    // Debug: Print final worst points and their concavity scores
    printf("DEBUG: Worst concavities: %.3f, %.3f, %.3f\n", worst_concavity_1, worst_concavity_2, worst_concavity_3);
    printf("DEBUG: Convex samples: %d/%d (%.1f%%)\n", convex_samples, total_samples, convexity_ratio * 100.0f);
    printf("DEBUG: Worst points:\n");
    printf("  1: (%.3f, %.3f, %.3f) triangle %d\n", result->worst_point_1[0], result->worst_point_1[1], result->worst_point_1[2], result->worst_triangle_index_1);
    printf("  2: (%.3f, %.3f, %.3f) triangle %d\n", result->worst_point_2[0], result->worst_point_2[1], result->worst_point_2[2], result->worst_triangle_index_2);
    printf("  3: (%.3f, %.3f, %.3f) triangle %d\n", result->worst_point_3[0], result->worst_point_3[1], result->worst_point_3[2], result->worst_triangle_index_3);
    
    // Apply a smoothing function to make the metric more intuitive
    // Use a power function to emphasize the difference between highly convex and concave meshes
    result->concavity_score = convexity_ratio * convexity_ratio; // Square for smoothing
    
    return 1; // Success
}

// Helper function to calculate the signed distance from a point to a plane
float point_plane_distance(const float* point, const float* plane_normal, const float* plane_point) {
    float vec[3] = {
        point[0] - plane_point[0],
        point[1] - plane_point[1], 
        point[2] - plane_point[2]
    };
    return vec[0] * plane_normal[0] + vec[1] * plane_normal[1] + vec[2] * plane_normal[2];
}

// Line-plane intersection function
// Returns 1 if intersection found, 0 if line is parallel to plane
int line_plane_intersection(const float* line_start, const float* line_end, 
                           const float* plane_normal, const float* plane_point, 
                           float* intersection_point) {
    float line_dir[3] = {
        line_end[0] - line_start[0],
        line_end[1] - line_start[1],
        line_end[2] - line_start[2]
    };
    
    float denom = plane_normal[0] * line_dir[0] + 
                  plane_normal[1] * line_dir[1] + 
                  plane_normal[2] * line_dir[2];
    
    // Check if line is parallel to plane
    if (fabs(denom) < 1e-6f) {
        return 0;
    }
    
    float t = point_plane_distance(plane_point, plane_normal, line_start) / denom;
    
    // Check if intersection is within the line segment
    if (t < 0.0f || t > 1.0f) {
        return 0;
    }
    
    // Calculate intersection point
    intersection_point[0] = line_start[0] + t * line_dir[0];
    intersection_point[1] = line_start[1] + t * line_dir[1];
    intersection_point[2] = line_start[2] + t * line_dir[2];
    
    return 1;
}

// Classification of triangle vertices relative to cutting plane
typedef enum {
    VERTEX_ON_PLANE = 0,
    VERTEX_POSITIVE = 1,
    VERTEX_NEGATIVE = -1
} vertex_classification_t;

// Classify a vertex relative to the cutting plane
vertex_classification_t classify_vertex(const float* vertex, const float* plane_normal, const float* plane_point) {
    float distance = point_plane_distance(vertex, plane_normal, plane_point);
    
    if (fabs(distance) < 1e-6f) {
        return VERTEX_ON_PLANE;
    } else if (distance > 0.0f) {
        return VERTEX_POSITIVE;
    } else {
        return VERTEX_NEGATIVE;
    }
}

// Structure to hold triangle cutting results
typedef struct {
    stl_triangle_t triangles[4];  // Maximum 4 triangles can result from cutting one triangle
    int count;                    // Number of triangles generated
} triangle_cut_result_t;

// Helper function to create a triangle from 3 vertices and a normal
void create_triangle(stl_triangle_t* triangle, const float* v1, const float* v2, const float* v3, const float* normal) {
    // Copy vertices
    memcpy(triangle->vertices[0], v1, 3 * sizeof(float));
    memcpy(triangle->vertices[1], v2, 3 * sizeof(float));
    memcpy(triangle->vertices[2], v3, 3 * sizeof(float));
    
    // Copy normal
    memcpy(triangle->normal, normal, 3 * sizeof(float));
}

// Cut a single triangle by a plane - this is the core geometric operation
int cut_triangle_by_plane(const stl_triangle_t* triangle, const float* plane_normal, const float* plane_point, 
                         triangle_cut_result_t* positive_result, triangle_cut_result_t* negative_result) {
    
    positive_result->count = 0;
    negative_result->count = 0;
    
    // Classify all three vertices
    vertex_classification_t class0 = classify_vertex(triangle->vertices[0], plane_normal, plane_point);
    vertex_classification_t class1 = classify_vertex(triangle->vertices[1], plane_normal, plane_point);
    vertex_classification_t class2 = classify_vertex(triangle->vertices[2], plane_normal, plane_point);
    
    // Count vertices on each side
    int pos_count = (class0 == VERTEX_POSITIVE) + (class1 == VERTEX_POSITIVE) + (class2 == VERTEX_POSITIVE);
    int neg_count = (class0 == VERTEX_NEGATIVE) + (class1 == VERTEX_NEGATIVE) + (class2 == VERTEX_NEGATIVE);
    int on_plane_count = (class0 == VERTEX_ON_PLANE) + (class1 == VERTEX_ON_PLANE) + (class2 == VERTEX_ON_PLANE);
    
    // Case 1: All vertices on same side of plane - no cutting needed
    if (pos_count == 3) {
        // All vertices on positive side
        create_triangle(&positive_result->triangles[0], triangle->vertices[0], triangle->vertices[1], triangle->vertices[2], triangle->normal);
        positive_result->count = 1;
        return 1;
    }
    if (neg_count == 3) {
        // All vertices on negative side
        create_triangle(&negative_result->triangles[0], triangle->vertices[0], triangle->vertices[1], triangle->vertices[2], triangle->normal);
        negative_result->count = 1;
        return 1;
    }
    
    // Case 2: Triangle lies entirely on the plane
    if (on_plane_count == 3) {
        // Triangle is coplanar - assign to positive side by default
        create_triangle(&positive_result->triangles[0], triangle->vertices[0], triangle->vertices[1], triangle->vertices[2], triangle->normal);
        positive_result->count = 1;
        return 1;
    }
    
    // Case 3: Triangle intersects the plane - need to cut it
    float intersection_points[2][3];  // At most 2 intersection points for a triangle
    int intersection_edges[2];        // Track which edges the intersections are on
    int intersection_count = 0;
    
    // Check each edge for intersection with the plane
    const float* vertices[3] = {triangle->vertices[0], triangle->vertices[1], triangle->vertices[2]};
    vertex_classification_t classes[3] = {class0, class1, class2};
    
    for (int i = 0; i < 3; i++) {
        int next = (i + 1) % 3;
        
        // Check if this edge crosses the plane
        if ((classes[i] == VERTEX_POSITIVE && classes[next] == VERTEX_NEGATIVE) ||
            (classes[i] == VERTEX_NEGATIVE && classes[next] == VERTEX_POSITIVE)) {
            
            if (intersection_count < 2) {
                if (line_plane_intersection(vertices[i], vertices[next], plane_normal, plane_point, 
                                          intersection_points[intersection_count])) {
                    intersection_edges[intersection_count] = i;  // Record which edge this intersection is on
                    intersection_count++;
                }
            }
        }
    }
    
    // Must have exactly 2 intersection points for a proper triangle cut
    if (intersection_count != 2) {
        // Fallback: assign whole triangle to side with most vertices
        if (pos_count > neg_count) {
            create_triangle(&positive_result->triangles[0], triangle->vertices[0], triangle->vertices[1], triangle->vertices[2], triangle->normal);
            positive_result->count = 1;
        } else {
            create_triangle(&negative_result->triangles[0], triangle->vertices[0], triangle->vertices[1], triangle->vertices[2], triangle->normal);
            negative_result->count = 1;
        }
        return 1;
    }
    
    // Now we have 2 intersection points - create new triangles
    // Order intersection points correctly based on the edges they're on
    float* int1 = intersection_points[0];
    float* int2 = intersection_points[1];
    int edge1 = intersection_edges[0];
    // int edge2 = intersection_edges[1];  // Currently unused
    
    // Determine which vertex is isolated (different side from the other two)
    if (pos_count == 1) {
        // One vertex on positive side, two on negative side
        // Find the isolated positive vertex
        for (int i = 0; i < 3; i++) {
            if (classes[i] == VERTEX_POSITIVE) {
                // Create one triangle on positive side (isolated vertex + 2 intersection points)
                create_triangle(&positive_result->triangles[0], vertices[i], int1, int2, triangle->normal);
                positive_result->count = 1;
                
                // Create quadrilateral on negative side from the two remaining vertices + intersection points
                int v1 = (i + 1) % 3;
                int v2 = (i + 2) % 3;
                
                // Order intersection points correctly relative to v1 and v2
                // If edge1 connects to v1, then int1 should be paired with v1
                float* first_int, *second_int;
                if (edge1 == i || edge1 == v1) {
                    // int1 is on edge from isolated vertex or connects to v1
                    first_int = int1;
                    second_int = int2;
                } else {
                    // int2 is on edge that connects to v1
                    first_int = int2;
                    second_int = int1;
                }
                
                // Create two triangles forming the quadrilateral (v1, v2, second_int, first_int)
                create_triangle(&negative_result->triangles[0], vertices[v1], vertices[v2], second_int, triangle->normal);
                create_triangle(&negative_result->triangles[1], vertices[v1], second_int, first_int, triangle->normal);
                negative_result->count = 2;
                break;
            }
        }
    } else if (neg_count == 1) {
        // One vertex on negative side, two on positive side
        // Find the isolated negative vertex
        for (int i = 0; i < 3; i++) {
            if (classes[i] == VERTEX_NEGATIVE) {
                // Create one triangle on negative side (isolated vertex + 2 intersection points)
                create_triangle(&negative_result->triangles[0], vertices[i], int1, int2, triangle->normal);
                negative_result->count = 1;
                
                // Create quadrilateral on positive side from the two remaining vertices + intersection points
                int v1 = (i + 1) % 3;
                int v2 = (i + 2) % 3;
                
                // Order intersection points correctly relative to v1 and v2
                // If edge1 connects to v1, then int1 should be paired with v1
                float* first_int, *second_int;
                if (edge1 == i || edge1 == v1) {
                    // int1 is on edge from isolated vertex or connects to v1
                    first_int = int1;
                    second_int = int2;
                } else {
                    // int2 is on edge that connects to v1
                    first_int = int2;
                    second_int = int1;
                }
                
                // Create two triangles forming the quadrilateral (v1, v2, second_int, first_int)
                create_triangle(&positive_result->triangles[0], vertices[v1], vertices[v2], second_int, triangle->normal);
                create_triangle(&positive_result->triangles[1], vertices[v1], second_int, first_int, triangle->normal);
                positive_result->count = 2;
                break;
            }
        }
    }
    
    return 1;
}

// Geometric mesh cutting using plane-triangle intersection
stl_file_t* cut_mesh_by_plane(const stl_file_t* mesh, const float* point1, const float* point2, const float* point3, 
                             int side, plane_generation_method_t plane_method) {
    if (!mesh || !point1 || !point2 || !point3) {
        return NULL;
    }
    
    // Calculate plane normal based on the selected method
    float vec1[3], vec2[3];
    float* plane_point;
    
    if (plane_method == PLANE_METHOD_THREE_WORST_POINTS) {
        // Use three worst points to define the plane
        vec1[0] = point2[0] - point1[0];
        vec1[1] = point2[1] - point1[1];
        vec1[2] = point2[2] - point1[2];
        
        vec2[0] = point3[0] - point1[0];
        vec2[1] = point3[1] - point1[1];
        vec2[2] = point3[2] - point1[2];
        
        plane_point = (float*)point1;  // Use first worst point as plane reference
    } else {
        // Use two worst points + mesh center (point3 is the center)
        vec1[0] = point2[0] - point1[0];
        vec1[1] = point2[1] - point1[1];
        vec1[2] = point2[2] - point1[2];
        
        vec2[0] = point3[0] - point1[0];  // vector from point1 to center
        vec2[1] = point3[1] - point1[1];
        vec2[2] = point3[2] - point1[2];
        
        plane_point = (float*)point3;  // Use mesh center as plane reference
    }
    
    // Cross product to get plane normal
    float plane_normal[3] = {
        vec1[1] * vec2[2] - vec1[2] * vec2[1],
        vec1[2] * vec2[0] - vec1[0] * vec2[2],
        vec1[0] * vec2[1] - vec1[1] * vec2[0]
    };
    
    // Normalize the plane normal
    float normal_length = sqrtf(plane_normal[0] * plane_normal[0] + 
                               plane_normal[1] * plane_normal[1] + 
                               plane_normal[2] * plane_normal[2]);
    if (normal_length < 1e-6f) {
        return NULL; // Degenerate plane
    }
    
    plane_normal[0] /= normal_length;
    plane_normal[1] /= normal_length;
    plane_normal[2] /= normal_length;
    
    // First pass: count how many triangles we'll need
    unsigned int total_triangle_count = 0;
    for (unsigned int i = 0; i < mesh->num_triangles; i++) {
        triangle_cut_result_t pos_result, neg_result;
        cut_triangle_by_plane(&mesh->triangles[i], plane_normal, plane_point, &pos_result, &neg_result);
        
        if (side == 0) {
            total_triangle_count += neg_result.count;  // Negative side
        } else {
            total_triangle_count += pos_result.count;  // Positive side
        }
    }
    
    if (total_triangle_count == 0) {
        return NULL; // No triangles on this side
    }
    
    printf("    Geometric cutting: %u input triangles -> %u output triangles on side %d\n", 
           mesh->num_triangles, total_triangle_count, side);
    
    // Create result mesh
    stl_file_t* result = malloc(sizeof(stl_file_t));
    if (!result) {
        return NULL;
    }
    
    // Copy header from original mesh
    memcpy(result->header, mesh->header, 80);
    result->num_triangles = total_triangle_count;
    
    // Allocate triangles array
    result->triangles = malloc(total_triangle_count * sizeof(stl_triangle_t));
    if (!result->triangles) {
        free(result);
        return NULL;
    }
    
    // Second pass: actually cut triangles and store results
    unsigned int result_idx = 0;
    for (unsigned int i = 0; i < mesh->num_triangles; i++) {
        triangle_cut_result_t pos_result, neg_result;
        cut_triangle_by_plane(&mesh->triangles[i], plane_normal, plane_point, &pos_result, &neg_result);
        
        triangle_cut_result_t* target_result = (side == 0) ? &neg_result : &pos_result;
        
        // Copy all triangles from the target side
        for (int j = 0; j < target_result->count; j++) {
            memcpy(&result->triangles[result_idx], &target_result->triangles[j], sizeof(stl_triangle_t));
            result_idx++;
        }
    }
    
    // Calculate bounds for the new mesh
    calculate_stl_bounds(result);
    
    return result;
}

// Create a new tree node
mesh_tree_node_t* create_tree_node(stl_file_t* mesh, int depth) {
    mesh_tree_node_t* node = malloc(sizeof(mesh_tree_node_t));
    if (!node) return NULL;
    
    node->mesh = mesh;
    node->depth = depth;
    node->is_leaf = 0;
    node->concavity_score = 0.0f;
    node->left_child = NULL;
    node->right_child = NULL;
    
    // Initialize cutting plane data as invalid
    node->cutting_plane.is_valid = 0;
    
    return node;
}

// Free a tree node and all its children recursively
void free_tree_node(mesh_tree_node_t* node) {
    if (!node) return;
    
    free_tree_node(node->left_child);
    free_tree_node(node->right_child);
    
    if (node->mesh) {
        free_stl(node->mesh);
    }
    free(node);
}

// Free the entire decomposition tree
void free_decomposition_tree(decomposition_tree_t* tree) {
    if (!tree) return;
    
    free_tree_node(tree->root);
    free(tree);
}

// Recursive helper for building the decomposition tree
mesh_tree_node_t* decompose_node_recursive(stl_file_t* mesh, float concavity_threshold, int current_depth, int max_depth, decomposition_tree_t* tree, plane_generation_method_t plane_method) {
    if (!mesh) return NULL;
    
    // Create node for this mesh
    mesh_tree_node_t* node = create_tree_node(mesh, current_depth);
    if (!node) return NULL;
    
    // Check current mesh concavity
    concavity_result_t concavity_result;
    if (!check_concavity(mesh, &concavity_result)) {
        node->is_leaf = 1;
        node->concavity_score = 0.0f;
        update_tree_stats(tree, node);
        return node;
    }
    
    node->concavity_score = concavity_result.concavity_score;
    
    // Base cases - make this a leaf node
    if (current_depth >= max_depth) {
        printf("Node at depth %d reached max depth limit\n", current_depth);
        node->is_leaf = 1;
        update_tree_stats(tree, node);
        return node;
    }
    
    if (concavity_result.concavity_score >= concavity_threshold) {
        printf("Node at depth %d reached acceptable concavity: %.3f\n", current_depth, concavity_result.concavity_score);
        node->is_leaf = 1;
        update_tree_stats(tree, node);
        return node;
    }
    
    // Mesh needs decomposition
    printf("Decomposing node at depth %d (concavity: %.3f)\n", current_depth, concavity_result.concavity_score);
    
    // FIRST: Check if mesh already contains multiple disconnected components
    int num_components = 0;
    connected_component_t* components = find_connected_components(mesh, &num_components);
    
    // Variables for separation plane approach
    int use_separation_plane = 0;
    float point1[3], point2[3], sep_point3[3];
    float point3[3];  // Will be set by either separation plane or concavity method
    
    if (num_components > 1) {
        printf("Found %d disconnected components, attempting separation plane approach\n", num_components);
        printf("Component sizes: ");
        for (int i = 0; i < num_components; i++) {
            printf("%d ", components[i].count);
        }
        printf("\n");
        
        // Try to find a separation plane between components instead of direct separation
        if (num_components == 2) {
            // Find the separation plane between the two components
            separation_plane_t sep_plane = find_separation_plane(mesh, &components[0], &components[1]);
            
            if (sep_plane.is_valid) {
                printf("Using separation plane approach for disconnected components\n");
                
                // Clean up components since we're using geometric cutting
                for (int i = 0; i < num_components; i++) {
                    free(components[i].triangle_indices);
                }
                free(components);
                components = NULL;  // Prevent double-free later
                
                // Use the separation plane for geometric cutting
                // Convert separation plane to the format expected by cut_mesh_by_plane
                // We'll create three points defining the plane
                
                // Use the plane point as point1
                memcpy(point1, sep_plane.point, 3 * sizeof(float));
                
                // Create two additional points on the plane to define it
                // Find a perpendicular vector to the normal
                float perpendicular[3];
                if (fabsf(sep_plane.normal[0]) < 0.9f) {
                    // Normal is not primarily in X direction, use X axis
                    perpendicular[0] = 1.0f;
                    perpendicular[1] = 0.0f;
                    perpendicular[2] = 0.0f;
                } else {
                    // Normal is primarily in X direction, use Y axis
                    perpendicular[0] = 0.0f;
                    perpendicular[1] = 1.0f;
                    perpendicular[2] = 0.0f;
                }
                
                // Make perpendicular truly perpendicular using cross product
                float temp[3] = {
                    sep_plane.normal[1] * perpendicular[2] - sep_plane.normal[2] * perpendicular[1],
                    sep_plane.normal[2] * perpendicular[0] - sep_plane.normal[0] * perpendicular[2],
                    sep_plane.normal[0] * perpendicular[1] - sep_plane.normal[1] * perpendicular[0]
                };
                
                // Normalize the perpendicular vector
                float perp_length = sqrtf(temp[0] * temp[0] + temp[1] * temp[1] + temp[2] * temp[2]);
                if (perp_length > 1e-6f) {
                    temp[0] /= perp_length;
                    temp[1] /= perp_length;
                    temp[2] /= perp_length;
                }
                
                // Create point2 and point3 on the plane
                point2[0] = point1[0] + temp[0];
                point2[1] = point1[1] + temp[1];
                point2[2] = point1[2] + temp[2];
                
                // Create third point using cross product of normal and first perpendicular
                float second_perp[3] = {
                    sep_plane.normal[1] * temp[2] - sep_plane.normal[2] * temp[1],
                    sep_plane.normal[2] * temp[0] - sep_plane.normal[0] * temp[2],
                    sep_plane.normal[0] * temp[1] - sep_plane.normal[1] * temp[0]
                };
                
                sep_point3[0] = point1[0] + second_perp[0];
                sep_point3[1] = point1[1] + second_perp[1];
                sep_point3[2] = point1[2] + second_perp[2];
                
                printf("  Separation cutting plane points:\n");
                printf("    Point 1: (%.3f, %.3f, %.3f)\n", point1[0], point1[1], point1[2]);
                printf("    Point 2: (%.3f, %.3f, %.3f)\n", point2[0], point2[1], point2[2]);
                printf("    Point 3: (%.3f, %.3f, %.3f)\n", sep_point3[0], sep_point3[1], sep_point3[2]);
                
                // Set flag to use separation plane and continue to geometric cutting
                use_separation_plane = 1;
                // Copy sep_point3 to point3 for later use in cutting plane storage
                memcpy(point3, sep_point3, 3 * sizeof(float));
                
                // Skip the fallback component separation logic
                goto skip_component_fallback;
            } else {
                printf("Failed to find valid separation plane, falling back to direct component separation\n");
                // Fall back to direct component separation (original logic)
                // This is the fallback case where separation plane couldn't be determined
            }
            
            // Fallback: Direct component separation (original approach)
            stl_file_t* submesh1 = create_submesh_from_component(mesh, &components[0]);
            stl_file_t* submesh2 = create_submesh_from_component(mesh, &components[1]);
            
            if (submesh1 && submesh2) {
                // Check concavity of each component before decomposing further
                concavity_result_t component1_concavity, component2_concavity;
                int comp1_valid = check_concavity(submesh1, &component1_concavity);
                int comp2_valid = check_concavity(submesh2, &component2_concavity);
                
                printf("Fallback: Component 1 concavity: %.3f, Component 2 concavity: %.3f\n", 
                       comp1_valid ? component1_concavity.concavity_score : -1.0f,
                       comp2_valid ? component2_concavity.concavity_score : -1.0f);
                
                // Create appropriate child nodes based on component concavity
                // Note: decompose_node_recursive takes ownership of the mesh pointers
                if (comp1_valid && component1_concavity.concavity_score >= concavity_threshold) {
                    // Component 1 is acceptably convex - create leaf node directly
                    printf("Fallback: Component 1 is acceptably convex (%.3f), creating leaf node\n", component1_concavity.concavity_score);
                    node->left_child = create_tree_node(submesh1, current_depth + 1);
                    node->left_child->is_leaf = 1;
                    node->left_child->concavity_score = component1_concavity.concavity_score;
                    update_tree_stats(tree, node->left_child);
                } else {
                    // Component 1 needs further decomposition
                    printf("Fallback: Component 1 needs decomposition (%.3f)\n", comp1_valid ? component1_concavity.concavity_score : -1.0f);
                    node->left_child = decompose_node_recursive(submesh1, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                }
                
                if (comp2_valid && component2_concavity.concavity_score >= concavity_threshold) {
                    // Component 2 is acceptably convex - create leaf node directly
                    printf("Fallback: Component 2 is acceptably convex (%.3f), creating leaf node\n", component2_concavity.concavity_score);
                    node->right_child = create_tree_node(submesh2, current_depth + 1);
                    node->right_child->is_leaf = 1;
                    node->right_child->concavity_score = component2_concavity.concavity_score;
                    update_tree_stats(tree, node->right_child);
                } else {
                    // Component 2 needs further decomposition
                    printf("Fallback: Component 2 needs decomposition (%.3f)\n", comp2_valid ? component2_concavity.concavity_score : -1.0f);
                    node->right_child = decompose_node_recursive(submesh2, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                }
                
                // Cleanup components (but NOT the submeshes - they're now owned by the tree nodes)
                for (int i = 0; i < num_components; i++) {
                    free(components[i].triangle_indices);
                }
                free(components);
                
                // Don't free the submeshes here - they're now owned by the child nodes
                
                return node;
            }
            
            // Cleanup on failure
            if (submesh1) free_stl(submesh1);
            if (submesh2) free_stl(submesh2);
        } else {
            // Multiple components (>2) - for now, just take the two largest
            printf("Warning: %d components found, using two largest for binary decomposition\n", num_components);
            
            // Find two largest components
            int largest_idx = 0, second_largest_idx = 1;
            if (components[1].count > components[0].count) {
                largest_idx = 1;
                second_largest_idx = 0;
            }
            
            for (int i = 2; i < num_components; i++) {
                if (components[i].count > components[largest_idx].count) {
                    second_largest_idx = largest_idx;
                    largest_idx = i;
                } else if (components[i].count > components[second_largest_idx].count) {
                    second_largest_idx = i;
                }
            }
            
            stl_file_t* submesh1 = create_submesh_from_component(mesh, &components[largest_idx]);
            stl_file_t* submesh2 = create_submesh_from_component(mesh, &components[second_largest_idx]);
            
            if (submesh1 && submesh2) {
                // Check concavity of each component before decomposing further
                concavity_result_t component1_concavity, component2_concavity;
                int comp1_valid = check_concavity(submesh1, &component1_concavity);
                int comp2_valid = check_concavity(submesh2, &component2_concavity);
                
                printf("Largest component concavity: %.3f, Second largest concavity: %.3f\n", 
                       comp1_valid ? component1_concavity.concavity_score : -1.0f,
                       comp2_valid ? component2_concavity.concavity_score : -1.0f);
                
                // Create appropriate child nodes based on component concavity
                if (comp1_valid && component1_concavity.concavity_score >= concavity_threshold) {
                    // Component 1 is acceptably convex - create leaf node directly
                    printf("Largest component is acceptably convex (%.3f), creating leaf node\n", component1_concavity.concavity_score);
                    node->left_child = create_tree_node(submesh1, current_depth + 1);
                    node->left_child->is_leaf = 1;
                    node->left_child->concavity_score = component1_concavity.concavity_score;
                    update_tree_stats(tree, node->left_child);
                } else {
                    // Component 1 needs further decomposition
                    printf("Largest component needs decomposition (%.3f)\n", comp1_valid ? component1_concavity.concavity_score : -1.0f);
                    node->left_child = decompose_node_recursive(submesh1, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                }
                
                if (comp2_valid && component2_concavity.concavity_score >= concavity_threshold) {
                    // Component 2 is acceptably convex - create leaf node directly
                    printf("Second largest component is acceptably convex (%.3f), creating leaf node\n", component2_concavity.concavity_score);
                    node->right_child = create_tree_node(submesh2, current_depth + 1);
                    node->right_child->is_leaf = 1;
                    node->right_child->concavity_score = component2_concavity.concavity_score;
                    update_tree_stats(tree, node->right_child);
                } else {
                    // Component 2 needs further decomposition
                    printf("Second largest component needs decomposition (%.3f)\n", comp2_valid ? component2_concavity.concavity_score : -1.0f);
                    node->right_child = decompose_node_recursive(submesh2, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                }
                
                // Cleanup components (but NOT the submeshes - they're now owned by the tree nodes)
                for (int i = 0; i < num_components; i++) {
                    free(components[i].triangle_indices);
                }
                free(components);
                
                // Don't free the submeshes here - they're now owned by the child nodes
                
                return node;
            }
            
            // Cleanup on failure
            if (submesh1) free_stl(submesh1);
            if (submesh2) free_stl(submesh2);
        }
        
        // If component separation failed, fall through to geometric cutting
        printf("Component separation failed, falling back to geometric cutting\n");
    }
    
    skip_component_fallback:
    
    // Cleanup components if we're not using them
    if (components) {
        for (int i = 0; i < num_components; i++) {
            free(components[i].triangle_indices);
        }
        free(components);
    }
    
    // SECOND: Attempt geometric cutting (original logic or separation plane)
    // Prepare cutting plane points based on the method
    // Check if we have separation plane points defined (from goto path)
    stl_file_t* mesh_left = NULL;
    stl_file_t* mesh_right = NULL;
    
    // If we have separation plane defined, use those points
    // Otherwise, use the concavity-based approach
    if (use_separation_plane) {
        // This path is used when we have a separation plane between disconnected components
        printf("  Using separation plane for geometric cutting\n");
        
        // Use a custom cutting method for separation planes
        mesh_left = cut_mesh_by_plane(mesh, point1, point2, sep_point3, 0, PLANE_METHOD_THREE_WORST_POINTS);
        mesh_right = cut_mesh_by_plane(mesh, point1, point2, sep_point3, 1, PLANE_METHOD_THREE_WORST_POINTS);
    } else {
        // Normal concavity-based cutting approach
        if (plane_method == PLANE_METHOD_TWO_WORST_PLUS_CENTER) {
            // Calculate mesh center for third point
            point3[0] = (mesh->bounds[0] + mesh->bounds[3]) / 2.0f;
            point3[1] = (mesh->bounds[1] + mesh->bounds[4]) / 2.0f;
            point3[2] = (mesh->bounds[2] + mesh->bounds[5]) / 2.0f;
        } else {
            // Use third worst point
            point3[0] = concavity_result.worst_point_3[0];
            point3[1] = concavity_result.worst_point_3[1];
            point3[2] = concavity_result.worst_point_3[2];
        }
        
        // Debug: Print the 3 points defining the cutting plane
        printf("  Cutting plane at depth %d (%s method):\n", current_depth, 
               plane_method == PLANE_METHOD_THREE_WORST_POINTS ? "3 worst points" : "2 worst + center");
        printf("    Point 1 (worst): (%.3f, %.3f, %.3f)\n", 
               concavity_result.worst_point_1[0], concavity_result.worst_point_1[1], concavity_result.worst_point_1[2]);
        printf("    Point 2 (worst): (%.3f, %.3f, %.3f)\n", 
               concavity_result.worst_point_2[0], concavity_result.worst_point_2[1], concavity_result.worst_point_2[2]);
        printf("    Point 3 (%s): (%.3f, %.3f, %.3f)\n", 
               plane_method == PLANE_METHOD_THREE_WORST_POINTS ? "worst" : "center",
               point3[0], point3[1], point3[2]);
        
        // Cut mesh using the selected plane method
        mesh_left = cut_mesh_by_plane(mesh, concavity_result.worst_point_1, 
                                     concavity_result.worst_point_2, point3, 0, plane_method);
        mesh_right = cut_mesh_by_plane(mesh, concavity_result.worst_point_1, 
                                      concavity_result.worst_point_2, point3, 1, plane_method);
    }
    
    if (!mesh_left || !mesh_right) {
        // Cutting failed, make this a leaf node
        printf("Mesh cutting failed at depth %d, making leaf node\n", current_depth);
        if (mesh_left) free_stl(mesh_left);
        if (mesh_right) free_stl(mesh_right);
        
        node->is_leaf = 1;
        update_tree_stats(tree, node);
        return node;
    }
    
    // Validate that the cut actually separated the mesh meaningfully
    // If one side has nearly all triangles and the other has very few, 
    // the cut may have passed between disconnected components
    unsigned int total_triangles = mesh_left->num_triangles + mesh_right->num_triangles;
    float left_ratio = (float)mesh_left->num_triangles / (float)total_triangles;
    float right_ratio = (float)mesh_right->num_triangles / (float)total_triangles;
    
    const float MIN_CUT_RATIO = 0.1f; // Each side should have at least 10% of triangles
    
    if (left_ratio < MIN_CUT_RATIO || right_ratio < MIN_CUT_RATIO) {
        printf("Warning: Unbalanced cut at depth %d (%.1f%% vs %.1f%%), may indicate disconnected components\n", 
               current_depth, left_ratio * 100.0f, right_ratio * 100.0f);
        
        // Check if the smaller side forms disconnected components
        stl_file_t* smaller_mesh = (mesh_left->num_triangles < mesh_right->num_triangles) ? mesh_left : mesh_right;
        int small_components = 0;
        connected_component_t* small_comps = find_connected_components(smaller_mesh, &small_components);
        
        if (small_components > 1) {
            printf("Confirmed: Cut passed between disconnected components (%d found in smaller side)\n", small_components);
            
            // Clean up the failed cut result
            if (small_comps) {
                for (int i = 0; i < small_components; i++) {
                    free(small_comps[i].triangle_indices);
                }
                free(small_comps);
            }
            
            // Fall back to component-based separation on the original mesh
            int orig_components = 0;
            connected_component_t* orig_comps = find_connected_components(mesh, &orig_components);
            
            if (orig_comps && orig_components >= 2) {
                printf("Retrying with component-based separation\n");
                
                // Clean up failed geometric cut
                free_stl(mesh_left);
                free_stl(mesh_right);
                
                // Use component separation logic
                if (orig_components == 2) {
                    stl_file_t* submesh1 = create_submesh_from_component(mesh, &orig_comps[0]);
                    stl_file_t* submesh2 = create_submesh_from_component(mesh, &orig_comps[1]);
                    
                    if (submesh1 && submesh2) {
                        // Check concavity of each component before decomposing further
                        concavity_result_t component1_concavity, component2_concavity;
                        int comp1_valid = check_concavity(submesh1, &component1_concavity);
                        int comp2_valid = check_concavity(submesh2, &component2_concavity);
                        
                        printf("Retry: Component 1 concavity: %.3f, Component 2 concavity: %.3f\n", 
                               comp1_valid ? component1_concavity.concavity_score : -1.0f,
                               comp2_valid ? component2_concavity.concavity_score : -1.0f);
                        
                        // Create appropriate child nodes based on component concavity
                        if (comp1_valid && component1_concavity.concavity_score >= concavity_threshold) {
                            // Component 1 is acceptably convex - create leaf node directly
                            printf("Retry: Component 1 is acceptably convex (%.3f), creating leaf node\n", component1_concavity.concavity_score);
                            node->left_child = create_tree_node(submesh1, current_depth + 1);
                            node->left_child->is_leaf = 1;
                            node->left_child->concavity_score = component1_concavity.concavity_score;
                            update_tree_stats(tree, node->left_child);
                        } else {
                            // Component 1 needs further decomposition
                            printf("Retry: Component 1 needs decomposition (%.3f)\n", comp1_valid ? component1_concavity.concavity_score : -1.0f);
                            node->left_child = decompose_node_recursive(submesh1, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                        }
                        
                        if (comp2_valid && component2_concavity.concavity_score >= concavity_threshold) {
                            // Component 2 is acceptably convex - create leaf node directly
                            printf("Retry: Component 2 is acceptably convex (%.3f), creating leaf node\n", component2_concavity.concavity_score);
                            node->right_child = create_tree_node(submesh2, current_depth + 1);
                            node->right_child->is_leaf = 1;
                            node->right_child->concavity_score = component2_concavity.concavity_score;
                            update_tree_stats(tree, node->right_child);
                        } else {
                            // Component 2 needs further decomposition
                            printf("Retry: Component 2 needs decomposition (%.3f)\n", comp2_valid ? component2_concavity.concavity_score : -1.0f);
                            node->right_child = decompose_node_recursive(submesh2, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
                        }
                        
                        // Cleanup components (but NOT the submeshes - they're now owned by the tree nodes)
                        for (int i = 0; i < orig_components; i++) {
                            free(orig_comps[i].triangle_indices);
                        }
                        free(orig_comps);
                        
                        // Don't free the submeshes here - they're now owned by the child nodes
                        
                        return node;
                    }
                    
                    if (submesh1) free_stl(submesh1);
                    if (submesh2) free_stl(submesh2);
                }
                
                // Cleanup components
                for (int i = 0; i < orig_components; i++) {
                    free(orig_comps[i].triangle_indices);
                }
                free(orig_comps);
            }
            
            // If component separation also failed, make this a leaf
            printf("Component separation retry failed, making leaf node\n");
            node->is_leaf = 1;
            update_tree_stats(tree, node);
            return node;
        }
        
        // Clean up component analysis
        if (small_comps) {
            for (int i = 0; i < small_components; i++) {
                free(small_comps[i].triangle_indices);
            }
            free(small_comps);
        }
    }
    
    // Successfully cut mesh - update tree stats for this internal node
    update_tree_stats(tree, node);
    
    // Store cutting plane information for visualization
    node->cutting_plane.is_valid = 1;
    if (use_separation_plane) {
        // Use separation plane points
        memcpy(node->cutting_plane.point1, point1, 3 * sizeof(float));
        memcpy(node->cutting_plane.point2, point2, 3 * sizeof(float));
        memcpy(node->cutting_plane.point3, point3, 3 * sizeof(float));
    } else {
        // Use concavity-based points
        memcpy(node->cutting_plane.point1, concavity_result.worst_point_1, 3 * sizeof(float));
        memcpy(node->cutting_plane.point2, concavity_result.worst_point_2, 3 * sizeof(float));
        memcpy(node->cutting_plane.point3, point3, 3 * sizeof(float));
    }
    
    // Calculate and store plane normal and center
    float vec1[3] = {
        node->cutting_plane.point2[0] - node->cutting_plane.point1[0],
        node->cutting_plane.point2[1] - node->cutting_plane.point1[1],
        node->cutting_plane.point2[2] - node->cutting_plane.point1[2]
    };
    float vec2[3] = {
        node->cutting_plane.point3[0] - node->cutting_plane.point1[0],
        node->cutting_plane.point3[1] - node->cutting_plane.point1[1],
        node->cutting_plane.point3[2] - node->cutting_plane.point1[2]
    };
    
    // Cross product for normal
    node->cutting_plane.normal[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
    node->cutting_plane.normal[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
    node->cutting_plane.normal[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];
    
    // Normalize the normal
    float normal_length = sqrtf(
        node->cutting_plane.normal[0] * node->cutting_plane.normal[0] +
        node->cutting_plane.normal[1] * node->cutting_plane.normal[1] +
        node->cutting_plane.normal[2] * node->cutting_plane.normal[2]
    );
    if (normal_length > 1e-6f) {
        node->cutting_plane.normal[0] /= normal_length;
        node->cutting_plane.normal[1] /= normal_length;
        node->cutting_plane.normal[2] /= normal_length;
    }
    
    // Calculate plane center as average of the three points
    node->cutting_plane.center[0] = (node->cutting_plane.point1[0] + node->cutting_plane.point2[0] + node->cutting_plane.point3[0]) / 3.0f;
    node->cutting_plane.center[1] = (node->cutting_plane.point1[1] + node->cutting_plane.point2[1] + node->cutting_plane.point3[1]) / 3.0f;
    node->cutting_plane.center[2] = (node->cutting_plane.point1[2] + node->cutting_plane.point2[2] + node->cutting_plane.point3[2]) / 3.0f;
    
    // Recursively decompose the two pieces
    node->left_child = decompose_node_recursive(mesh_left, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
    node->right_child = decompose_node_recursive(mesh_right, concavity_threshold, current_depth + 1, max_depth, tree, plane_method);
    
    return node;
}

// Main decomposition function that returns a tree
decomposition_tree_t* decompose_mesh_tree(const stl_file_t* mesh, float concavity_threshold, int max_depth, plane_generation_method_t plane_method) {
    if (!mesh) return NULL;
    
    // Create tree structure
    decomposition_tree_t* tree = malloc(sizeof(decomposition_tree_t));
    if (!tree) return NULL;
    
    // Initialize tree statistics
    tree->total_nodes = 0;
    tree->leaf_nodes = 0;
    tree->max_depth_reached = 0;
    
    // Make a deep copy of the input mesh for the root node
    stl_file_t* root_mesh = malloc(sizeof(stl_file_t));
    if (!root_mesh) {
        free(tree);
        return NULL;
    }
    
    // Copy mesh header and metadata
    memcpy(root_mesh->header, mesh->header, 80);
    root_mesh->num_triangles = mesh->num_triangles;
    memcpy(root_mesh->bounds, mesh->bounds, 6 * sizeof(float));
    
    // Deep copy triangles array
    root_mesh->triangles = malloc(mesh->num_triangles * sizeof(stl_triangle_t));
    if (!root_mesh->triangles) {
        free(root_mesh);
        free(tree);
        return NULL;
    }
    memcpy(root_mesh->triangles, mesh->triangles, mesh->num_triangles * sizeof(stl_triangle_t));
    
    // Build the decomposition tree recursively
    tree->root = decompose_node_recursive(root_mesh, concavity_threshold, 0, max_depth, tree, plane_method);
    
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    
    printf("Decomposition complete: %d total nodes, %d leaf nodes, max depth: %d\n", 
           tree->total_nodes, tree->leaf_nodes, tree->max_depth_reached);
    
    return tree;
}

// Helper function for printing tree nodes recursively
void print_tree_node(const mesh_tree_node_t* node, int indent) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    printf("Node depth=%d, concavity=%.3f, triangles=%u, %s\n", 
           node->depth, 
           node->concavity_score, 
           node->mesh ? node->mesh->num_triangles : 0,
           node->is_leaf ? "LEAF" : "INTERNAL");
    
    // Recursively print children
    if (node->left_child) {
        print_tree_node(node->left_child, indent + 1);
    }
    if (node->right_child) {
        print_tree_node(node->right_child, indent + 1);
    }
}

// Print tree structure for debugging
void print_decomposition_tree(const decomposition_tree_t* tree) {
    if (!tree) {
        printf("Tree is NULL\n");
        return;
    }
    
    printf("Decomposition Tree Structure:\n");
    printf("============================\n");
    printf("Total nodes: %d\n", tree->total_nodes);
    printf("Leaf nodes: %d\n", tree->leaf_nodes);
    printf("Max depth reached: %d\n", tree->max_depth_reached);
    printf("\nTree structure:\n");
    
    print_tree_node(tree->root, 0);
    printf("\n");
}

// Helper function for collecting leaf nodes recursively
void collect_leaf_nodes(const mesh_tree_node_t* node, mesh_tree_node_t** leaf_array, int* count, int max_leaves) {
    if (!node || *count >= max_leaves) return;
    
    if (node->is_leaf) {
        // Debug: Validate mesh data before adding to array
        if (!node->mesh) {
            printf("WARNING: Leaf node has NULL mesh at count %d\n", *count);
            return;
        }
        if (node->mesh->num_triangles > 1000000) { // Sanity check
            printf("WARNING: Leaf node has suspicious triangle count %u at count %d\n", 
                   node->mesh->num_triangles, *count);
            return;
        }
        
        leaf_array[*count] = (mesh_tree_node_t*)node;  // Cast away const
        (*count)++;
        return;
    }
    
    // Recursively collect from children
    if (node->left_child) {
        collect_leaf_nodes(node->left_child, leaf_array, count, max_leaves);
    }
    if (node->right_child) {
        collect_leaf_nodes(node->right_child, leaf_array, count, max_leaves);
    }
}

// Get all leaf nodes (final decomposed meshes) from the tree
int get_leaf_nodes(const decomposition_tree_t* tree, mesh_tree_node_t** leaf_array, int max_leaves) {
    if (!tree || !tree->root || !leaf_array) return 0;
    
    int count = 0;
    collect_leaf_nodes(tree->root, leaf_array, &count, max_leaves);
    return count;
    }