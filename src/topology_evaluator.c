#include "topology_evaluator.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>
#include <stdbool.h>
#include <limits.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Main evaluation functions
topology_evaluation_t* evaluate_topology(const stl_file_t* stl, topology_analysis_type_t analysis_type) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    topology_evaluation_t* eval = malloc(sizeof(topology_evaluation_t));
    if (!eval) return NULL;
    
    // Initialize all fields to zero/NULL
    memset(eval, 0, sizeof(topology_evaluation_t));
    
    // Allocate vertex array (estimate: 3 vertices per triangle, but many will be shared)
    eval->num_vertices = stl->num_triangles * 3; // Overestimate, will be corrected
    eval->vertices = malloc(eval->num_vertices * sizeof(topology_vertex_t));
    if (!eval->vertices) {
        free(eval);
        return NULL;
    }
    
    // Initialize vertices
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        eval->vertices[i].connected_vertices = malloc(10 * sizeof(unsigned int)); // Start with space for 10 connections
        if (!eval->vertices[i].connected_vertices) {
            // Cleanup on failure
            for (unsigned int j = 0; j < i; j++) {
                free(eval->vertices[j].connected_vertices);
            }
            free(eval->vertices);
            free(eval);
            return NULL;
        }
        eval->vertices[i].num_connections = 0;
        eval->vertices[i].capacity = 10;
        eval->vertices[i].curvature = 0.0f;
        eval->vertices[i].valence = 0;
    }
    
    // Find unique vertices and build connectivity
    eval->num_vertices = find_unique_vertices(stl, eval->vertices);
    
    // Allocate triangle array first (needed by build_edge_list)
    eval->num_triangles = stl->num_triangles;
    eval->triangles = malloc(eval->num_triangles * sizeof(topology_triangle_t));
    if (!eval->triangles) {
        for (unsigned int i = 0; i < eval->num_vertices; i++) {
            free(eval->vertices[i].connected_vertices);
        }
        free(eval->vertices);
        free(eval);
        return NULL;
    }
    
    // Initialize triangles
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        eval->triangles[i].area = 0.0f;
        eval->triangles[i].curvature = 0.0f;
        eval->triangles[i].aspect_ratio = 0.0f;
        for (int j = 0; j < 3; j++) {
            eval->triangles[i].vertices[j] = UINT_MAX; // Use UINT_MAX as sentinel value
            eval->triangles[i].edges[j] = UINT_MAX;    // Use UINT_MAX as sentinel value
            eval->triangles[i].normal[j] = 0.0f;
        }
    }
    
    // Allocate edge array (estimate: 3 edges per triangle, but many will be shared)
    eval->num_edges = stl->num_triangles * 3; // Overestimate
    eval->edges = malloc(eval->num_edges * sizeof(topology_edge_t));
    if (!eval->edges) {
        free(eval->triangles);
        for (unsigned int i = 0; i < eval->num_vertices; i++) {
            free(eval->vertices[i].connected_vertices);
        }
        free(eval->vertices);
        free(eval);
        return NULL;
    }
    
    // Build edge list (no visualization callback)
    eval->num_edges = build_edge_list(stl, eval, NULL, NULL);
    
    // Perform requested analyses
    switch (analysis_type) {
        case TOPO_ANALYSIS_CONNECTIVITY:
            analyze_connectivity(stl, eval);
            break;
        case TOPO_ANALYSIS_CURVATURE:
            analyze_curvature(stl, eval);
            break;
        case TOPO_ANALYSIS_FEATURES:
            analyze_features(stl, eval);
            break;
        case TOPO_ANALYSIS_DENSITY:
            analyze_density(stl, eval);
            break;
        case TOPO_ANALYSIS_QUALITY:
            analyze_quality(stl, eval);
            break;
        case TOPO_ANALYSIS_HOLES:
            detect_holes(stl, eval);
            break;
        case TOPO_ANALYSIS_COMPLETE:
            analyze_connectivity(stl, eval);
            analyze_curvature(stl, eval);
            analyze_features(stl, eval);
            analyze_density(stl, eval);
            analyze_quality(stl, eval);
            detect_holes(stl, eval);
            break;
    }
    
    return eval;
}

void free_topology_evaluation(topology_evaluation_t* eval) {
    if (!eval) return;
    
    // Free vertices
    if (eval->vertices) {
        for (unsigned int i = 0; i < eval->num_vertices; i++) {
            if (eval->vertices[i].connected_vertices) {
                free(eval->vertices[i].connected_vertices);
            }
        }
        free(eval->vertices);
    }
    
    // Free edges
    if (eval->edges) {
        free(eval->edges);
    }
    
    // Free triangles
    if (eval->triangles) {
        free(eval->triangles);
    }
    
    // Free feature detection arrays
    if (eval->features.sharp_edges) free(eval->features.sharp_edges);
    if (eval->features.corners) free(eval->features.corners);
    if (eval->features.flat_regions) free(eval->features.flat_regions);
    
    // Free density analysis arrays
    if (eval->density.vertex_density) free(eval->density.vertex_density);
    if (eval->density.triangle_density) free(eval->density.triangle_density);
    if (eval->density.high_density_regions) free(eval->density.high_density_regions);
    if (eval->density.low_density_regions) free(eval->density.low_density_regions);
    
    // Free quality analysis arrays
    if (eval->quality.triangle_quality) free(eval->quality.triangle_quality);
    if (eval->quality.poor_quality_triangles) free(eval->quality.poor_quality_triangles);
    
    // Free curvature analysis arrays
    if (eval->curvature.vertex_curvature) free(eval->curvature.vertex_curvature);
    if (eval->curvature.triangle_curvature) free(eval->curvature.triangle_curvature);
    if (eval->curvature.high_curvature_regions) free(eval->curvature.high_curvature_regions);
    if (eval->curvature.low_curvature_regions) free(eval->curvature.low_curvature_regions);
    
    // Free hole detection arrays
    free_hole_detection(&eval->holes);
    
    free(eval);
}

// Analysis functions
int analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Set up triangle vertices and calculate valence
    float tolerance = 1e-6f;
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        
        for (int j = 0; j < 3; j++) {
            // Initialize vertex index to sentinel value
            eval->triangles[i].vertices[j] = UINT_MAX;
            
            // Find vertex index in our unique vertex list
            for (unsigned int k = 0; k < eval->num_vertices; k++) {
                if (calculate_distance_3d(tri->vertices[j], eval->vertices[k].position) < tolerance) {
                    eval->triangles[i].vertices[j] = k;
                    eval->vertices[k].valence++;
                    break;
                }
            }
            
            // Verify we found a matching vertex
            if (eval->triangles[i].vertices[j] == UINT_MAX) {
                fprintf(stderr, "Error: Could not find matching vertex for triangle %u vertex %d\n", i, j);
                return 0;
            }
        }
    }
    
    // Count boundary edges
    eval->num_boundary_edges = 0;
    for (unsigned int i = 0; i < eval->num_edges; i++) {
        if (eval->edges[i].is_boundary) {
            eval->num_boundary_edges++;
        }
    }
    
    // Count non-manifold vertices
    eval->num_non_manifold_vertices = 0;
    eval->num_isolated_vertices = 0;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        if (eval->vertices[i].valence == 0) {
            eval->num_isolated_vertices++;
        } else if (eval->vertices[i].valence > 6) { // Heuristic for non-manifold
            eval->num_non_manifold_vertices++;
        }
    }
    
    // Calculate connectivity score
    float total_connections = 0;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        total_connections += eval->vertices[i].num_connections;
    }
    
    if (eval->num_vertices > 0) {
        eval->connectivity_score = total_connections / (eval->num_vertices * 6.0f); // Normalize
    }
    
    return 1;
}

int analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Allocate curvature arrays
    eval->curvature.vertex_curvature = malloc(eval->num_vertices * sizeof(float));
    eval->curvature.triangle_curvature = malloc(eval->num_triangles * sizeof(float));
    
    if (!eval->curvature.vertex_curvature || !eval->curvature.triangle_curvature) {
        return 0;
    }
    
    // Calculate vertex curvatures
    float total_curvature = 0.0f;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        eval->curvature.vertex_curvature[i] = calculate_vertex_curvature(stl, i, eval);
        total_curvature += eval->curvature.vertex_curvature[i];
    }
    
    eval->curvature.average_curvature = total_curvature / eval->num_vertices;
    
    // Calculate triangle curvatures
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        eval->curvature.triangle_curvature[i] = calculate_triangle_curvature(&stl->triangles[i], eval);
    }
    
    // Find min/max curvature
    eval->curvature.min_curvature = FLT_MAX;
    eval->curvature.max_curvature = -FLT_MAX;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        if (eval->curvature.vertex_curvature[i] < eval->curvature.min_curvature) {
            eval->curvature.min_curvature = eval->curvature.vertex_curvature[i];
        }
        if (eval->curvature.vertex_curvature[i] > eval->curvature.max_curvature) {
            eval->curvature.max_curvature = eval->curvature.vertex_curvature[i];
        }
    }
    
    // Calculate variance
    float variance_sum = 0.0f;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        float diff = eval->curvature.vertex_curvature[i] - eval->curvature.average_curvature;
        variance_sum += diff * diff;
    }
    eval->curvature.curvature_variance = variance_sum / eval->num_vertices;
    
    return 1;
}

int analyze_features(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Initialize feature counts
    eval->features.num_sharp_edges = 0;
    eval->features.num_corners = 0;
    eval->features.num_flat_regions = 0;
    
    // Ensure we have curvature data for features
    if (!eval->curvature.triangle_curvature || !eval->curvature.vertex_curvature) {
        if (!analyze_curvature(stl, eval)) {
            return 0;
        }
    }
    
    // Detect sharp edges (default threshold: 30 degrees)
    if (!detect_sharp_edges(stl, eval, 30.0f * M_PI / 180.0f)) {
        return 0;
    }
    
    // Detect corners (default threshold: 45 degrees)
    if (!detect_corners(stl, eval, 45.0f * M_PI / 180.0f)) {
        return 0;
    }
    
    // Detect flat regions (default threshold: 5 degrees)
    if (!detect_flat_regions(stl, eval, 5.0f * M_PI / 180.0f)) {
        return 0;
    }
    
    // Calculate feature richness
    if (eval->num_edges + eval->num_vertices > 0) {
        eval->feature_richness = (eval->features.num_sharp_edges + eval->features.num_corners) / 
                                (float)(eval->num_edges + eval->num_vertices);
    } else {
        eval->feature_richness = 0.0f;
    }
    
    return 1;
}

int analyze_density(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Allocate density arrays
    eval->density.vertex_density = malloc(eval->num_vertices * sizeof(float));
    eval->density.triangle_density = malloc(eval->num_triangles * sizeof(float));
    
    if (!eval->density.vertex_density || !eval->density.triangle_density) {
        return 0;
    }
    
    // Calculate vertex density (number of connected triangles)
    float total_density = 0.0f;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        eval->density.vertex_density[i] = eval->vertices[i].valence;
        total_density += eval->density.vertex_density[i];
    }
    
    eval->density.average_density = total_density / eval->num_vertices;
    
    // Calculate triangle density (area-based)
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        float area = calculate_triangle_area(tri);
        eval->density.triangle_density[i] = 1.0f / area; // Inverse area = density
    }
    
    // Find min/max density
    eval->density.min_density = FLT_MAX;
    eval->density.max_density = -FLT_MAX;
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        if (eval->density.vertex_density[i] < eval->density.min_density) {
            eval->density.min_density = eval->density.vertex_density[i];
        }
        if (eval->density.vertex_density[i] > eval->density.max_density) {
            eval->density.max_density = eval->density.vertex_density[i];
        }
    }
    
    return 1;
}

int analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Allocate quality arrays
    eval->quality.triangle_quality = malloc(eval->num_triangles * sizeof(float));
    eval->quality.poor_quality_triangles = malloc(eval->num_triangles * sizeof(unsigned int));
    
    if (!eval->quality.triangle_quality || !eval->quality.poor_quality_triangles) {
        return 0;
    }
    
    // Calculate triangle quality
    float total_quality = 0.0f;
    eval->quality.num_poor_quality = 0;
    
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        eval->quality.triangle_quality[i] = calculate_triangle_quality(&stl->triangles[i]);
        total_quality += eval->quality.triangle_quality[i];
        
        if (eval->quality.triangle_quality[i] < 0.3f) { // Threshold for poor quality
            eval->quality.poor_quality_triangles[eval->quality.num_poor_quality] = i;
            eval->quality.num_poor_quality++;
        }
    }
    
    eval->quality.average_quality = total_quality / eval->num_triangles;
    
    // Find min/max quality
    eval->quality.min_quality = FLT_MAX;
    eval->quality.max_quality = -FLT_MAX;
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        if (eval->quality.triangle_quality[i] < eval->quality.min_quality) {
            eval->quality.min_quality = eval->quality.triangle_quality[i];
        }
        if (eval->quality.triangle_quality[i] > eval->quality.max_quality) {
            eval->quality.max_quality = eval->quality.triangle_quality[i];
        }
    }
    
    return 1;
}

// Utility functions
unsigned int find_unique_vertices(const stl_file_t* stl, topology_vertex_t* vertices) {
    if (!stl || !vertices) return 0;
    
    unsigned int unique_count = 0;
    float tolerance = 1e-6f; // Tolerance for vertex comparison
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        
        for (int j = 0; j < 3; j++) {
            int found = 0;
            
            // Check if this vertex already exists
            for (unsigned int k = 0; k < unique_count; k++) {
                if (calculate_distance_3d(tri->vertices[j], vertices[k].position) < tolerance) {
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                // Add new vertex
                memcpy(vertices[unique_count].position, tri->vertices[j], 3 * sizeof(float));
                // Note: connected_vertices array is already allocated in evaluate_topology
                vertices[unique_count].num_connections = 0;
                vertices[unique_count].curvature = 0.0f;
                vertices[unique_count].valence = 0;
                unique_count++;
            }
        }
    }
    
    return unique_count;
}

unsigned int build_edge_list(const stl_file_t* stl, topology_evaluation_t* eval, edge_callback_t callback, void* user_data) {
    if (!stl || !eval) return 0;
    
    unsigned int edge_count = 0;
    float tolerance = 1e-6f;
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        
        // Process each edge of the triangle
        for (int j = 0; j < 3; j++) {
            int v1_idx = j;
            int v2_idx = (j + 1) % 3;
            
            // Find vertex indices in our unique vertex list
            unsigned int vertex1_idx = 0, vertex2_idx = 0;
            
            for (unsigned int k = 0; k < eval->num_vertices; k++) {
                if (calculate_distance_3d(tri->vertices[v1_idx], eval->vertices[k].position) < tolerance) {
                    vertex1_idx = k;
                }
                if (calculate_distance_3d(tri->vertices[v2_idx], eval->vertices[k].position) < tolerance) {
                    vertex2_idx = k;
                }
            }
            
            // Check if this edge already exists
            int edge_exists = 0;
            unsigned int existing_edge_idx = 0;
            for (unsigned int k = 0; k < edge_count; k++) {
                if ((eval->edges[k].vertex1 == vertex1_idx && eval->edges[k].vertex2 == vertex2_idx) ||
                    (eval->edges[k].vertex1 == vertex2_idx && eval->edges[k].vertex2 == vertex1_idx)) {
                    edge_exists = 1;
                    existing_edge_idx = k;
                    eval->edges[k].triangle2 = i; // Second triangle sharing this edge
                    break;
                }
            }
            
            if (!edge_exists) {
                // Add new edge
                eval->edges[edge_count].vertex1 = vertex1_idx;
                eval->edges[edge_count].vertex2 = vertex2_idx;
                eval->edges[edge_count].triangle1 = i;
                eval->edges[edge_count].triangle2 = UINT_MAX; // Will be set if another triangle shares this edge
                eval->edges[edge_count].length = calculate_distance_3d(tri->vertices[v1_idx], tri->vertices[v2_idx]);
                eval->edges[edge_count].dihedral_angle = 0.0f; // Will be calculated later
                eval->edges[edge_count].is_boundary = 1; // Will be updated if shared
                
                // Set the edge index in the triangle
                eval->triangles[i].edges[j] = edge_count;
                
                // Notify callback of new edge
                if (callback) {
                    callback(edge_count, user_data);
                }
                edge_count++;
            } else {
                // Set the edge index in the triangle for existing edge
                eval->triangles[i].edges[j] = existing_edge_idx;
            }
        }
    }
    
    // Update boundary flags and calculate dihedral angles
    for (unsigned int i = 0; i < edge_count; i++) {
        if (eval->edges[i].triangle2 != UINT_MAX) {
            eval->edges[i].is_boundary = 0; // Not a boundary edge
            
            // Calculate dihedral angle
            const stl_triangle_t* tri1 = &stl->triangles[eval->edges[i].triangle1];
            const stl_triangle_t* tri2 = &stl->triangles[eval->edges[i].triangle2];
            eval->edges[i].dihedral_angle = calculate_dihedral_angle(tri1, tri2, i);
        }
    }
    
    return edge_count;
}

// Geometry utility functions
float calculate_distance_3d(const float* p1, const float* p2) {
    float dx = p2[0] - p1[0];
    float dy = p2[1] - p1[1];
    float dz = p2[2] - p1[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float dot_product_3d_vectors(const float* v1, const float* v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void cross_product_3d(const float* v1, const float* v2, float* result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

void normalize_vector_3d(float* vector) {
    float length = calculate_vector_length_3d(vector);
    if (length > 0.0f) {
        vector[0] /= length;
        vector[1] /= length;
        vector[2] /= length;
    }
}

float calculate_vector_length_3d(const float* vector) {
    return sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

float angle_between_vectors(const float* v1, const float* v2) {
    float dot = dot_product_3d_vectors(v1, v2);
    float len1 = calculate_vector_length_3d(v1);
    float len2 = calculate_vector_length_3d(v2);
    
    if (len1 > 0.0f && len2 > 0.0f) {
        float cos_angle = dot / (len1 * len2);
        if (cos_angle > 1.0f) cos_angle = 1.0f;
        if (cos_angle < -1.0f) cos_angle = -1.0f;
        return acosf(cos_angle);
    }
    return 0.0f;
}

// Analysis helper functions
float calculate_vertex_curvature(const stl_file_t* stl, unsigned int vertex_idx, 
                                const topology_evaluation_t* eval) {
    // Simplified curvature calculation based on adjacent face normals
    if (!eval || vertex_idx >= eval->num_vertices) return 0.0f;
    
    float total_curvature = 0.0f;
    unsigned int face_count = 0;
    
    // Find all triangles that share this vertex
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        float tolerance = 1e-6f;
        
        for (int j = 0; j < 3; j++) {
            if (calculate_distance_3d(tri->vertices[j], eval->vertices[vertex_idx].position) < tolerance) {
                // Calculate face normal
                float v1[3], v2[3], normal[3];
                for (int k = 0; k < 3; k++) {
                    v1[k] = tri->vertices[1][k] - tri->vertices[0][k];
                    v2[k] = tri->vertices[2][k] - tri->vertices[0][k];
                }
                cross_product_3d(v1, v2, normal);
                normalize_vector_3d(normal);
                
                total_curvature += calculate_vector_length_3d(normal);
                face_count++;
                break;
            }
        }
    }
    
    if (face_count > 0) {
        return total_curvature / face_count;
    }
    
    return 0.0f;
}

float calculate_triangle_curvature(const stl_triangle_t* triangle, 
                                  const topology_evaluation_t* eval __attribute__((unused))) {
    // Simplified triangle curvature based on area and perimeter
    if (!triangle) return 0.0f;
    
    float area = calculate_triangle_area(triangle);
    float perimeter = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        perimeter += calculate_distance_3d(triangle->vertices[i], triangle->vertices[j]);
    }
    
    if (perimeter > 0.0f) {
        return area / (perimeter * perimeter); // Rough curvature approximation
    }
    
    return 0.0f;
}

float calculate_dihedral_angle(const stl_triangle_t* tri1, const stl_triangle_t* tri2, 
                              unsigned int shared_edge __attribute__((unused))) {
    // Calculate angle between two adjacent faces
    if (!tri1 || !tri2) return 0.0f;
    
    // Calculate face normals
    float v1[3], v2[3], normal1[3], normal2[3];
    
    for (int i = 0; i < 3; i++) {
        v1[i] = tri1->vertices[1][i] - tri1->vertices[0][i];
        v2[i] = tri1->vertices[2][i] - tri1->vertices[0][i];
    }
    cross_product_3d(v1, v2, normal1);
    normalize_vector_3d(normal1);
    
    for (int i = 0; i < 3; i++) {
        v1[i] = tri2->vertices[1][i] - tri2->vertices[0][i];
        v2[i] = tri2->vertices[2][i] - tri2->vertices[0][i];
    }
    cross_product_3d(v1, v2, normal2);
    normalize_vector_3d(normal2);
    
    return angle_between_vectors(normal1, normal2);
}

float calculate_triangle_quality(const stl_triangle_t* triangle) {
    if (!triangle) return 0.0f;
    
    // Calculate aspect ratio
    float aspect_ratio = calculate_triangle_aspect_ratio(triangle);
    
    // Calculate angle quality
    float angles[3];
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        int k = (i + 2) % 3;
        
        float v1[3], v2[3];
        for (int m = 0; m < 3; m++) {
            v1[m] = triangle->vertices[j][m] - triangle->vertices[i][m];
            v2[m] = triangle->vertices[k][m] - triangle->vertices[i][m];
        }
        angles[i] = angle_between_vectors(v1, v2);
    }
    
    // Quality based on aspect ratio and angles
    float angle_quality = 1.0f;
    for (int i = 0; i < 3; i++) {
        float ideal_angle = M_PI / 3.0f; // 60 degrees
        float angle_diff = fabsf(angles[i] - ideal_angle) / ideal_angle;
        angle_quality *= (1.0f - angle_diff);
    }
    
    return (aspect_ratio + angle_quality) / 2.0f;
}

float calculate_triangle_aspect_ratio(const stl_triangle_t* triangle) {
    if (!triangle) return 0.0f;
    
    // Calculate edge lengths
    float edges[3];
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        edges[i] = calculate_distance_3d(triangle->vertices[i], triangle->vertices[j]);
    }
    
    // Find min and max edge lengths
    float min_edge = edges[0];
    float max_edge = edges[0];
    for (int i = 1; i < 3; i++) {
        if (edges[i] < min_edge) min_edge = edges[i];
        if (edges[i] > max_edge) max_edge = edges[i];
    }
    
    if (max_edge > 0.0f) {
        return min_edge / max_edge; // Aspect ratio (0 = degenerate, 1 = equilateral)
    }
    
    return 0.0f;
}

float calculate_triangle_area(const stl_triangle_t* triangle) {
    if (!triangle) return 0.0f;
    
    float v1[3], v2[3], cross[3];
    
    for (int i = 0; i < 3; i++) {
        v1[i] = triangle->vertices[1][i] - triangle->vertices[0][i];
        v2[i] = triangle->vertices[2][i] - triangle->vertices[0][i];
    }
    
    cross_product_3d(v1, v2, cross);
    return calculate_vector_length_3d(cross) / 2.0f;
}

// Feature detection functions
int detect_sharp_edges(const stl_file_t* stl, topology_evaluation_t* eval, float threshold) {
    if (!stl || !eval) return 0;
    
    eval->features.sharp_edges = malloc(eval->num_edges * sizeof(unsigned int));
    eval->features.num_sharp_edges = 0;
    eval->features.sharp_edge_threshold = threshold;
    
    for (unsigned int i = 0; i < eval->num_edges; i++) {
        if (eval->edges[i].dihedral_angle > threshold) {
            eval->features.sharp_edges[eval->features.num_sharp_edges] = i;
            eval->features.num_sharp_edges++;
        }
    }
    
    return 1;
}

int detect_corners(const stl_file_t* stl, topology_evaluation_t* eval, float threshold) {
    if (!stl || !eval) return 0;
    
    eval->features.corners = malloc(eval->num_vertices * sizeof(unsigned int));
    eval->features.num_corners = 0;
    eval->features.corner_threshold = threshold;
    
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        if (eval->vertices[i].curvature > threshold) {
            eval->features.corners[eval->features.num_corners] = i;
            eval->features.num_corners++;
        }
    }
    
    return 1;
}

int detect_flat_regions(const stl_file_t* stl, topology_evaluation_t* eval, float threshold) {
    if (!stl || !eval) return 0;
    
    // Check if curvature analysis has been performed
    if (!eval->curvature.triangle_curvature) {
        // Need to perform curvature analysis first
        if (!analyze_curvature(stl, eval)) {
            return 0;
        }
    }
    
    eval->features.flat_regions = malloc(eval->num_triangles * sizeof(unsigned int));
    if (!eval->features.flat_regions) {
        return 0;
    }
    eval->features.num_flat_regions = 0;
    
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        if (eval->curvature.triangle_curvature[i] < threshold) {
            eval->features.flat_regions[eval->features.num_flat_regions] = i;
            eval->features.num_flat_regions++;
        }
    }
    
    return 1;
}

// Analysis and reporting functions
void print_topology_summary(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Topology Analysis Summary\n");
    printf("========================\n");
    printf("Vertices: %u\n", eval->num_vertices);
    printf("Edges: %u\n", eval->num_edges);
    printf("Triangles: %u\n", eval->num_triangles);
    printf("Boundary edges: %u\n", eval->num_boundary_edges);
    printf("Non-manifold vertices: %u\n", eval->num_non_manifold_vertices);
    printf("Isolated vertices: %u\n", eval->num_isolated_vertices);
    printf("Connectivity score: %.3f\n", eval->connectivity_score);
    printf("Feature richness: %.3f\n", eval->feature_richness);
    printf("Holes detected: %u\n", eval->holes.num_loops);
    printf("\n");
}

void print_connectivity_analysis(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Connectivity Analysis\n");
    printf("====================\n");
    printf("Total vertices: %u\n", eval->num_vertices);
    printf("Total edges: %u\n", eval->num_edges);
    printf("Boundary edges: %u (%.1f%%)\n", eval->num_boundary_edges, 
           (float)eval->num_boundary_edges / eval->num_edges * 100.0f);
    printf("Non-manifold vertices: %u (%.1f%%)\n", eval->num_non_manifold_vertices,
           (float)eval->num_non_manifold_vertices / eval->num_vertices * 100.0f);
    printf("Connectivity score: %.3f\n", eval->connectivity_score);
    printf("\n");
}

void print_curvature_analysis(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Curvature Analysis\n");
    printf("==================\n");
    printf("Average curvature: %.6f\n", eval->curvature.average_curvature);
    printf("Curvature variance: %.6f\n", eval->curvature.curvature_variance);
    printf("Min curvature: %.6f\n", eval->curvature.min_curvature);
    printf("Max curvature: %.6f\n", eval->curvature.max_curvature);
    printf("High curvature regions: %u\n", eval->curvature.num_high_curvature_regions);
    printf("Low curvature regions: %u\n", eval->curvature.num_low_curvature_regions);
    printf("\n");
}

void print_feature_analysis(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Feature Analysis\n");
    printf("================\n");
    printf("Sharp edges: %u (threshold: %.1f°)\n", eval->features.num_sharp_edges,
           eval->features.sharp_edge_threshold * 180.0f / M_PI);
    printf("Corners: %u (threshold: %.1f°)\n", eval->features.num_corners,
           eval->features.corner_threshold * 180.0f / M_PI);
    printf("Flat regions: %u\n", eval->features.num_flat_regions);
    printf("Feature richness: %.3f\n", eval->feature_richness);
    printf("\n");
}

void print_density_analysis(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Density Analysis\n");
    printf("================\n");
    printf("Average density: %.3f\n", eval->density.average_density);
    printf("Density variance: %.3f\n", eval->density.density_variance);
    printf("Min density: %.3f\n", eval->density.min_density);
    printf("Max density: %.3f\n", eval->density.max_density);
    printf("High density regions: %u\n", eval->density.num_high_density_regions);
    printf("Low density regions: %u\n", eval->density.num_low_density_regions);
    printf("\n");
}

void print_quality_analysis(const topology_evaluation_t* eval) {
    if (!eval) return;
    
    printf("Quality Analysis\n");
    printf("================\n");
    printf("Average quality: %.3f\n", eval->quality.average_quality);
    printf("Min quality: %.3f\n", eval->quality.min_quality);
    printf("Max quality: %.3f\n", eval->quality.max_quality);
    printf("Poor quality triangles: %u (%.1f%%)\n", eval->quality.num_poor_quality,
           (float)eval->quality.num_poor_quality / eval->num_triangles * 100.0f);
    printf("\n");
}

// Slicing recommendations
slicing_recommendations_t* generate_slicing_recommendations(const topology_evaluation_t* eval) {
    if (!eval) return NULL;
    
    slicing_recommendations_t* recs = malloc(sizeof(slicing_recommendations_t));
    if (!recs) return NULL;
    
    // Layer height based on curvature
    if (eval->curvature.average_curvature > 0.1f) {
        recs->recommended_layer_height = 0.1f; // Fine layers for high curvature
    } else if (eval->curvature.average_curvature > 0.05f) {
        recs->recommended_layer_height = 0.2f; // Medium layers
    } else {
        recs->recommended_layer_height = 0.3f; // Coarse layers for flat surfaces
    }
    
    // Infill density based on feature complexity
    if (eval->feature_richness > 0.1f) {
        recs->recommended_infill_density = 0.8f; // High infill for complex features
    } else if (eval->feature_richness > 0.05f) {
        recs->recommended_infill_density = 0.6f; // Medium infill
    } else {
        recs->recommended_infill_density = 0.4f; // Low infill for simple shapes
    }
    
    // Shell count based on mesh quality
    if (eval->quality.average_quality < 0.5f) {
        recs->recommended_shells = 3; // More shells for poor quality mesh
    } else if (eval->quality.average_quality < 0.7f) {
        recs->recommended_shells = 2; // Standard shells
    } else {
        recs->recommended_shells = 1; // Fewer shells for high quality mesh
    }
    
    // Print speed based on complexity
    if (eval->complexity_score > 0.7f) {
        recs->recommended_speed = 30.0f; // Slow for complex models
    } else if (eval->complexity_score > 0.4f) {
        recs->recommended_speed = 60.0f; // Medium speed
    } else {
        recs->recommended_speed = 90.0f; // Fast for simple models
    }
    
    // Slicing strategy recommendation
    if (eval->feature_richness > 0.15f) {
        recs->slicing_strategy = "Use adaptive slicing with feature detection";
    } else if (eval->curvature.average_curvature > 0.08f) {
        recs->slicing_strategy = "Use variable layer height based on curvature";
    } else {
        recs->slicing_strategy = "Standard uniform layer slicing";
    }
    
    return recs;
}

void free_slicing_recommendations(slicing_recommendations_t* recs) {
    if (recs) {
        free(recs);
    }
}

void print_slicing_recommendations(const slicing_recommendations_t* recs) {
    if (!recs) return;
    
    printf("Slicing Recommendations\n");
    printf("======================\n");
    printf("Recommended layer height: %.2f mm\n", recs->recommended_layer_height);
    printf("Recommended infill density: %.1f%%\n", recs->recommended_infill_density * 100.0f);
    printf("Recommended shell count: %u\n", recs->recommended_shells);
    printf("Recommended print speed: %.1f mm/s\n", recs->recommended_speed);
    printf("Slicing strategy: %s\n", recs->slicing_strategy);
    printf("\n");
}

// Hole detection functions

// Helper function to find triangles connected to a given triangle
static void find_connected_triangles(const topology_evaluation_t* eval, 
                                   unsigned int triangle_idx, 
                                   unsigned int* connected_tris, 
                                   unsigned int* num_connected) {
    *num_connected = 0;
    
    // Get the edges of the current triangle
    unsigned int edges[3];
    for (int i = 0; i < 3; i++) {
        edges[i] = eval->triangles[triangle_idx].edges[i];
    }
    
    // Find triangles that share these edges
    for (unsigned int i = 0; i < eval->num_edges; i++) {
        for (int j = 0; j < 3; j++) {
            if (i == edges[j]) {
                // This edge belongs to our triangle
                if (eval->edges[i].triangle1 == triangle_idx && eval->edges[i].triangle2 != UINT_MAX) {
                    connected_tris[*num_connected] = eval->edges[i].triangle2;
                    (*num_connected)++;
                } else if (eval->edges[i].triangle2 == triangle_idx) {
                    connected_tris[*num_connected] = eval->edges[i].triangle1;
                    (*num_connected)++;
                }
            }
        }
    }
}

// Helper function to check if a loop is continuous
static int is_loop_continuous(const hole_loop_t* loop, const topology_evaluation_t* eval) {
    if (loop->num_edges < 3) return 0; // Need at least 3 edges for a valid loop
    
    // Check if each edge connects to the next
    for (unsigned int i = 0; i < loop->num_edges; i++) {
        unsigned int current_edge = loop->edge_indices[i];
        unsigned int next_edge = loop->edge_indices[(i + 1) % loop->num_edges];
        
        // Check if these edges share a vertex
        unsigned int v1 = eval->edges[current_edge].vertex1;
        unsigned int v2 = eval->edges[current_edge].vertex2;
        unsigned int v3 = eval->edges[next_edge].vertex1;
        unsigned int v4 = eval->edges[next_edge].vertex2;
        
        if (v1 != v3 && v1 != v4 && v2 != v3 && v2 != v4) {
            return 0; // Edges don't connect
        }
    }
    
    return 1; // Loop is continuous
}

// Helper function to calculate loop perimeter
static float calculate_loop_perimeter(const hole_loop_t* loop, const topology_evaluation_t* eval) {
    float perimeter = 0.0f;
    
    for (unsigned int i = 0; i < loop->num_edges; i++) {
        unsigned int edge_idx = loop->edge_indices[i];
        perimeter += eval->edges[edge_idx].length;
    }
    
    return perimeter;
}

// Helper function to check if loops share vertices
static void find_shared_vertices(const hole_detection_t* holes, 
                                const topology_evaluation_t* eval __attribute__((unused)),
                                unsigned int* shared_vertices, 
                                unsigned int* num_shared) {
    *num_shared = 0;
    
    if (holes->num_loops < 2) return;
    
    // For each pair of loops, check for shared vertices
    for (unsigned int i = 0; i < holes->num_loops; i++) {
        for (unsigned int j = i + 1; j < holes->num_loops; j++) {
            for (unsigned int k = 0; k < holes->loops[i].num_vertices; k++) {
                for (unsigned int l = 0; l < holes->loops[j].num_vertices; l++) {
                    if (holes->loops[i].vertex_indices[k] == holes->loops[j].vertex_indices[l]) {
                        // Check if this vertex is already in our shared list
                        int already_found = 0;
                        for (unsigned int m = 0; m < *num_shared; m++) {
                            if (shared_vertices[m] == holes->loops[i].vertex_indices[k]) {
                                already_found = 1;
                                break;
                            }
                        }
                        
                        if (!already_found) {
                            shared_vertices[*num_shared] = holes->loops[i].vertex_indices[k];
                            (*num_shared)++;
                        }
                    }
                }
            }
        }
    }
}

int detect_holes(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
    // Initialize hole detection structure
    eval->holes.loops = NULL;
    eval->holes.num_loops = 0;
    eval->holes.shared_vertices = NULL;
    eval->holes.num_shared_vertices = 0;
    eval->holes.has_intersecting_loops = 0;
    
    // Allocate visited array for triangles
    bool* visited = calloc(eval->num_triangles, sizeof(bool));
    if (!visited) return 0;
    
    // Allocate arrays for storing loops
    unsigned int max_loops = eval->num_triangles / 3; // Rough estimate
    eval->holes.loops = malloc(max_loops * sizeof(hole_loop_t));
    if (!eval->holes.loops) {
        free(visited);
        return 0;
    }
    
    // Allocate arrays for storing edges in loops
    unsigned int max_edges_per_loop = eval->num_edges;
    for (unsigned int i = 0; i < max_loops; i++) {
        eval->holes.loops[i].edge_indices = NULL;
        eval->holes.loops[i].vertex_indices = NULL;
        eval->holes.loops[i].num_edges = 0;
        eval->holes.loops[i].num_vertices = 0;
        eval->holes.loops[i].is_continuous = 0;
        eval->holes.loops[i].perimeter = 0.0f;
        
        // Allocate memory for all potential loops
        eval->holes.loops[i].edge_indices = malloc(max_edges_per_loop * sizeof(unsigned int));
        eval->holes.loops[i].vertex_indices = malloc(max_edges_per_loop * sizeof(unsigned int));
        if (!eval->holes.loops[i].edge_indices || !eval->holes.loops[i].vertex_indices) {
            // Cleanup on failure
            for (unsigned int j = 0; j < i; j++) {
                free(eval->holes.loops[j].edge_indices);
                free(eval->holes.loops[j].vertex_indices);
            }
            free(eval->holes.loops);
            free(visited);
            return 0;
        }
    }
    
    // Start from each unvisited triangle
    for (unsigned int start_tri = 0; start_tri < eval->num_triangles; start_tri++) {
        if (visited[start_tri]) continue;
        
        // Start a new potential loop
        unsigned int current_loop = eval->holes.num_loops;
        if (current_loop >= max_loops) break;
        
        // Use a stack for DFS traversal
        unsigned int* stack = malloc(eval->num_triangles * sizeof(unsigned int));
        unsigned int stack_size = 0;
        
        if (!stack) {
            free(visited);
            return 0;
        }
        
        // Start with the current triangle
        stack[stack_size++] = start_tri;
        visited[start_tri] = true;
        
        // Track edges that form the loop
        unsigned int* loop_edges = malloc(eval->num_edges * sizeof(unsigned int));
        unsigned int num_loop_edges = 0;
        
        if (!loop_edges) {
            free(stack);
            free(visited);
            return 0;
        }
        
        // DFS traversal to find connected triangles
        while (stack_size > 0) {
            unsigned int current_tri = stack[--stack_size];
            
            // Find connected triangles
            unsigned int connected_tris[10]; // Max 10 connected triangles per triangle
            unsigned int num_connected;
            find_connected_triangles(eval, current_tri, connected_tris, &num_connected);
            
            for (unsigned int i = 0; i < num_connected; i++) {
                unsigned int next_tri = connected_tris[i];
                
                if (!visited[next_tri]) {
                    // Add to stack for further exploration
                    stack[stack_size++] = next_tri;
                    visited[next_tri] = true;
                } else {
                    // This triangle has been visited before - potential loop edge
                    // Find the edge between current_tri and next_tri
                    for (unsigned int j = 0; j < eval->num_edges; j++) {
                        if ((eval->edges[j].triangle1 == current_tri && eval->edges[j].triangle2 == next_tri) ||
                            (eval->edges[j].triangle1 == next_tri && eval->edges[j].triangle2 == current_tri)) {
                            
                            // Check if this edge is already in our loop
                            int edge_already_in_loop = 0;
                            for (unsigned int k = 0; k < num_loop_edges; k++) {
                                if (loop_edges[k] == j) {
                                    edge_already_in_loop = 1;
                                    break;
                                }
                            }
                            
                            if (!edge_already_in_loop) {
                                loop_edges[num_loop_edges++] = j;
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // If we found edges that form a potential loop
        if (num_loop_edges > 0) {
            // Store the loop
            eval->holes.loops[current_loop].num_edges = num_loop_edges;
            memcpy(eval->holes.loops[current_loop].edge_indices, loop_edges, 
                   num_loop_edges * sizeof(unsigned int));
            
            // Extract unique vertices from the loop edges
            unsigned int* loop_vertices = malloc(eval->num_vertices * sizeof(unsigned int));
            unsigned int num_loop_vertices = 0;
            
            for (unsigned int i = 0; i < num_loop_edges; i++) {
                unsigned int edge_idx = loop_edges[i];
                unsigned int v1 = eval->edges[edge_idx].vertex1;
                unsigned int v2 = eval->edges[edge_idx].vertex2;
                
                // Add vertices if not already in list
                int v1_found = 0, v2_found = 0;
                for (unsigned int j = 0; j < num_loop_vertices; j++) {
                    if (loop_vertices[j] == v1) v1_found = 1;
                    if (loop_vertices[j] == v2) v2_found = 1;
                }
                
                if (!v1_found) {
                    loop_vertices[num_loop_vertices++] = v1;
                }
                if (!v2_found) {
                    loop_vertices[num_loop_vertices++] = v2;
                }
            }
            
            eval->holes.loops[current_loop].num_vertices = num_loop_vertices;
            memcpy(eval->holes.loops[current_loop].vertex_indices, loop_vertices,
                   num_loop_vertices * sizeof(unsigned int));
            
            // Check if loop is continuous
            eval->holes.loops[current_loop].is_continuous = 
                is_loop_continuous(&eval->holes.loops[current_loop], eval);
            
            // Calculate perimeter
            eval->holes.loops[current_loop].perimeter = 
                calculate_loop_perimeter(&eval->holes.loops[current_loop], eval);
            
            // Only keep continuous loops
            if (eval->holes.loops[current_loop].is_continuous) {
                eval->holes.num_loops++;
            } else {
                // Remove the loop if not continuous
                eval->holes.loops[current_loop].num_edges = 0;
                eval->holes.loops[current_loop].num_vertices = 0;
            }
            
            free(loop_vertices);
        }
        
        free(loop_edges);
        free(stack);
    }
    
    // Find shared vertices between loops
    if (eval->holes.num_loops > 0) {
        eval->holes.shared_vertices = malloc(eval->num_vertices * sizeof(unsigned int));
        if (!eval->holes.shared_vertices) {
            // Cleanup on failure
            for (unsigned int i = 0; i < max_loops; i++) {
                free(eval->holes.loops[i].edge_indices);
                free(eval->holes.loops[i].vertex_indices);
            }
            free(eval->holes.loops);
            free(visited);
            return 0;
        }
        
        find_shared_vertices(&eval->holes, eval, 
                            eval->holes.shared_vertices, 
                            &eval->holes.num_shared_vertices);
        
        eval->holes.has_intersecting_loops = (eval->holes.num_shared_vertices > 0);
    }
    
    free(visited);
    return 1;
}

void free_hole_detection(hole_detection_t* holes) {
    if (!holes) return;
    
    if (holes->loops) {
        for (unsigned int i = 0; i < holes->num_loops; i++) {
            if (holes->loops[i].edge_indices) {
                free(holes->loops[i].edge_indices);
            }
            if (holes->loops[i].vertex_indices) {
                free(holes->loops[i].vertex_indices);
            }
        }
        free(holes->loops);
    }
    
    if (holes->shared_vertices) {
        free(holes->shared_vertices);
    }
    
    holes->loops = NULL;
    holes->num_loops = 0;
    holes->shared_vertices = NULL;
    holes->num_shared_vertices = 0;
    holes->has_intersecting_loops = 0;
}

void print_hole_analysis(const hole_detection_t* holes) {
    if (!holes) return;
    
    printf("Hole Detection Analysis\n");
    printf("======================\n");
    printf("Number of loops detected: %u\n", holes->num_loops);
    printf("Number of shared vertices: %u\n", holes->num_shared_vertices);
    printf("Has intersecting loops: %s\n", holes->has_intersecting_loops ? "Yes" : "No");
    
    for (unsigned int i = 0; i < holes->num_loops; i++) {
        printf("Loop %u:\n", i + 1);
        printf("  Edges: %u\n", holes->loops[i].num_edges);
        printf("  Vertices: %u\n", holes->loops[i].num_vertices);
        printf("  Perimeter: %.3f\n", holes->loops[i].perimeter);
        printf("  Continuous: %s\n", holes->loops[i].is_continuous ? "Yes" : "No");
    }
    printf("\n");
} 