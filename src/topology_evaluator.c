#include "topology_evaluator.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>

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
        eval->vertices[i].connected_vertices = NULL;
        eval->vertices[i].num_connections = 0;
        eval->vertices[i].capacity = 0;
        eval->vertices[i].curvature = 0.0f;
        eval->vertices[i].valence = 0;
    }
    
    // Find unique vertices and build connectivity
    eval->num_vertices = find_unique_vertices(stl, eval->vertices);
    
    // Allocate edge array (estimate: 3 edges per triangle, but many will be shared)
    eval->num_edges = stl->num_triangles * 3; // Overestimate
    eval->edges = malloc(eval->num_edges * sizeof(topology_edge_t));
    if (!eval->edges) {
        free(eval->vertices);
        free(eval);
        return NULL;
    }
    
    // Build edge list
    eval->num_edges = build_edge_list(stl, eval);
    
    // Allocate triangle array
    eval->num_triangles = stl->num_triangles;
    eval->triangles = malloc(eval->num_triangles * sizeof(topology_triangle_t));
    if (!eval->triangles) {
        free(eval->edges);
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
            eval->triangles[i].vertices[j] = 0;
            eval->triangles[i].edges[j] = 0;
            eval->triangles[i].normal[j] = 0.0f;
        }
    }
    
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
        case TOPO_ANALYSIS_COMPLETE:
            analyze_connectivity(stl, eval);
            analyze_curvature(stl, eval);
            analyze_features(stl, eval);
            analyze_density(stl, eval);
            analyze_quality(stl, eval);
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
    
    free(eval);
}

// Analysis functions
int analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval) {
    if (!stl || !eval) return 0;
    
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
    
    // Detect sharp edges (default threshold: 30 degrees)
    detect_sharp_edges(stl, eval, 30.0f * M_PI / 180.0f);
    
    // Detect corners (default threshold: 45 degrees)
    detect_corners(stl, eval, 45.0f * M_PI / 180.0f);
    
    // Detect flat regions (default threshold: 5 degrees)
    detect_flat_regions(stl, eval, 5.0f * M_PI / 180.0f);
    
    // Calculate feature richness
    eval->feature_richness = (eval->features.num_sharp_edges + eval->features.num_corners) / 
                            (float)(eval->num_edges + eval->num_vertices);
    
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
                if (distance_3d(tri->vertices[j], vertices[k].position) < tolerance) {
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                // Add new vertex
                memcpy(vertices[unique_count].position, tri->vertices[j], 3 * sizeof(float));
                vertices[unique_count].connected_vertices = malloc(10 * sizeof(unsigned int));
                vertices[unique_count].capacity = 10;
                vertices[unique_count].num_connections = 0;
                vertices[unique_count].curvature = 0.0f;
                vertices[unique_count].valence = 0;
                unique_count++;
            }
        }
    }
    
    return unique_count;
}

unsigned int build_edge_list(const stl_file_t* stl, topology_evaluation_t* eval) {
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
                if (distance_3d(tri->vertices[v1_idx], eval->vertices[k].position) < tolerance) {
                    vertex1_idx = k;
                }
                if (distance_3d(tri->vertices[v2_idx], eval->vertices[k].position) < tolerance) {
                    vertex2_idx = k;
                }
            }
            
            // Check if this edge already exists
            int edge_exists = 0;
            for (unsigned int k = 0; k < edge_count; k++) {
                if ((eval->edges[k].vertex1 == vertex1_idx && eval->edges[k].vertex2 == vertex2_idx) ||
                    (eval->edges[k].vertex1 == vertex2_idx && eval->edges[k].vertex2 == vertex1_idx)) {
                    edge_exists = 1;
                    eval->edges[k].triangle2 = i; // Second triangle sharing this edge
                    break;
                }
            }
            
            if (!edge_exists) {
                // Add new edge
                eval->edges[edge_count].vertex1 = vertex1_idx;
                eval->edges[edge_count].vertex2 = vertex2_idx;
                eval->edges[edge_count].triangle1 = i;
                eval->edges[edge_count].triangle2 = -1; // Will be set if another triangle shares this edge
                eval->edges[edge_count].length = distance_3d(tri->vertices[v1_idx], tri->vertices[v2_idx]);
                eval->edges[edge_count].dihedral_angle = 0.0f; // Will be calculated later
                eval->edges[edge_count].is_boundary = 1; // Will be updated if shared
                
                edge_count++;
            }
        }
    }
    
    // Update boundary flags and calculate dihedral angles
    for (unsigned int i = 0; i < edge_count; i++) {
        if (eval->edges[i].triangle2 != -1) {
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
float distance_3d(const float* p1, const float* p2) {
    float dx = p2[0] - p1[0];
    float dy = p2[1] - p1[1];
    float dz = p2[2] - p1[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float dot_product_3d(const float* v1, const float* v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void cross_product_3d(const float* v1, const float* v2, float* result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

void normalize_vector_3d(float* vector) {
    float length = vector_length_3d(vector);
    if (length > 0.0f) {
        vector[0] /= length;
        vector[1] /= length;
        vector[2] /= length;
    }
}

float vector_length_3d(const float* vector) {
    return sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

float angle_between_vectors(const float* v1, const float* v2) {
    float dot = dot_product_3d(v1, v2);
    float len1 = vector_length_3d(v1);
    float len2 = vector_length_3d(v2);
    
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
            if (distance_3d(tri->vertices[j], eval->vertices[vertex_idx].position) < tolerance) {
                // Calculate face normal
                float v1[3], v2[3], normal[3];
                for (int k = 0; k < 3; k++) {
                    v1[k] = tri->vertices[1][k] - tri->vertices[0][k];
                    v2[k] = tri->vertices[2][k] - tri->vertices[0][k];
                }
                cross_product_3d(v1, v2, normal);
                normalize_vector_3d(normal);
                
                total_curvature += vector_length_3d(normal);
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
                                  const topology_evaluation_t* eval) {
    // Simplified triangle curvature based on area and perimeter
    if (!triangle) return 0.0f;
    
    float area = calculate_triangle_area(triangle);
    float perimeter = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        perimeter += distance_3d(triangle->vertices[i], triangle->vertices[j]);
    }
    
    if (perimeter > 0.0f) {
        return area / (perimeter * perimeter); // Rough curvature approximation
    }
    
    return 0.0f;
}

float calculate_dihedral_angle(const stl_triangle_t* tri1, const stl_triangle_t* tri2, 
                              unsigned int shared_edge) {
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
        edges[i] = distance_3d(triangle->vertices[i], triangle->vertices[j]);
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
    return vector_length_3d(cross) / 2.0f;
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
    
    eval->features.flat_regions = malloc(eval->num_triangles * sizeof(unsigned int));
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