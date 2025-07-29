#ifndef TOPOLOGY_EVALUATOR_H
#define TOPOLOGY_EVALUATOR_H

#include "stl_parser.h"
#include <stdint.h>

// Topology analysis types
typedef enum {
    TOPO_ANALYSIS_CONNECTIVITY,    // Vertex/edge connectivity analysis
    TOPO_ANALYSIS_CURVATURE,       // Surface curvature analysis
    TOPO_ANALYSIS_FEATURES,        // Feature detection (edges, corners)
    TOPO_ANALYSIS_DENSITY,         // Mesh density analysis
    TOPO_ANALYSIS_QUALITY,         // Mesh quality metrics
    TOPO_ANALYSIS_COMPLETE         // All analyses combined
} topology_analysis_type_t;

// Vertex structure for topology analysis
typedef struct {
    float position[3];             // Vertex position
    unsigned int* connected_vertices; // Indices of connected vertices
    unsigned int num_connections;   // Number of connected vertices
    unsigned int capacity;         // Allocated capacity
    float curvature;               // Local curvature at vertex
    unsigned int valence;          // Number of triangles sharing this vertex
} topology_vertex_t;

// Edge structure
typedef struct {
    unsigned int vertex1;          // First vertex index
    unsigned int vertex2;          // Second vertex index
    unsigned int triangle1;        // First triangle index
    unsigned int triangle2;        // Second triangle index (or -1 if boundary)
    float length;                  // Edge length
    float dihedral_angle;          // Angle between adjacent faces
    int is_boundary;               // Is this a boundary edge?
} topology_edge_t;

// Triangle structure for topology
typedef struct {
    unsigned int vertices[3];      // Vertex indices
    unsigned int edges[3];         // Edge indices
    float area;                    // Triangle area
    float normal[3];               // Face normal
    float curvature;               // Local curvature
    float aspect_ratio;            // Triangle quality metric
} topology_triangle_t;

// Feature detection results
typedef struct {
    unsigned int* sharp_edges;     // Indices of sharp edges
    unsigned int num_sharp_edges;  // Number of sharp edges
    unsigned int* corners;         // Indices of corner vertices
    unsigned int num_corners;      // Number of corners
    unsigned int* flat_regions;    // Indices of flat region triangles
    unsigned int num_flat_regions; // Number of flat regions
    float sharp_edge_threshold;    // Threshold for sharp edge detection
    float corner_threshold;        // Threshold for corner detection
} feature_detection_t;

// Mesh density analysis
typedef struct {
    float* vertex_density;         // Density at each vertex
    float* triangle_density;       // Density at each triangle
    float average_density;         // Overall mesh density
    float density_variance;        // Variance in density
    float min_density;             // Minimum density
    float max_density;             // Maximum density
    unsigned int* high_density_regions; // High density region indices
    unsigned int num_high_density_regions;
    unsigned int* low_density_regions;  // Low density region indices
    unsigned int num_low_density_regions;
} density_analysis_t;

// Mesh quality metrics
typedef struct {
    float* triangle_quality;       // Quality metric for each triangle
    float average_quality;         // Average mesh quality
    float min_quality;             // Minimum quality
    float max_quality;             // Maximum quality
    unsigned int* poor_quality_triangles; // Poor quality triangle indices
    unsigned int num_poor_quality;
    float aspect_ratio_threshold;  // Threshold for aspect ratio
    float angle_threshold;         // Threshold for angle quality
} quality_analysis_t;

// Curvature analysis
typedef struct {
    float* vertex_curvature;       // Curvature at each vertex
    float* triangle_curvature;     // Curvature at each triangle
    float average_curvature;       // Average curvature
    float curvature_variance;      // Variance in curvature
    float min_curvature;           // Minimum curvature
    float max_curvature;           // Maximum curvature
    unsigned int* high_curvature_regions; // High curvature region indices
    unsigned int num_high_curvature_regions;
    unsigned int* low_curvature_regions;  // Low curvature region indices
    unsigned int num_low_curvature_regions;
} curvature_analysis_t;

// Complete topology evaluation result
typedef struct {
    topology_vertex_t* vertices;   // Vertex topology data
    topology_edge_t* edges;        // Edge topology data
    topology_triangle_t* triangles; // Triangle topology data
    unsigned int num_vertices;     // Number of unique vertices
    unsigned int num_edges;        // Number of edges
    unsigned int num_triangles;    // Number of triangles
    
    feature_detection_t features;  // Feature detection results
    density_analysis_t density;    // Density analysis results
    quality_analysis_t quality;    // Quality analysis results
    curvature_analysis_t curvature; // Curvature analysis results
    
    // Connectivity statistics
    unsigned int num_boundary_edges;
    unsigned int num_manifold_regions;
    unsigned int num_non_manifold_vertices;
    unsigned int num_isolated_vertices;
    
    // Topology summary
    float connectivity_score;      // Overall connectivity quality
    float complexity_score;        // Mesh complexity metric
    float feature_richness;        // Feature richness metric
} topology_evaluation_t;

// Function declarations

// Main evaluation functions
topology_evaluation_t* evaluate_topology(const stl_file_t* stl, topology_analysis_type_t analysis_type);
void free_topology_evaluation(topology_evaluation_t* eval);

// Analysis functions
int analyze_connectivity(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_curvature(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_features(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_density(const stl_file_t* stl, topology_evaluation_t* eval);
int analyze_quality(const stl_file_t* stl, topology_evaluation_t* eval);

// Utility functions
unsigned int find_unique_vertices(const stl_file_t* stl, topology_vertex_t* vertices);
unsigned int build_edge_list(const stl_file_t* stl, topology_evaluation_t* eval);
float calculate_vertex_curvature(const stl_file_t* stl, unsigned int vertex_idx, 
                                const topology_evaluation_t* eval);
float calculate_triangle_curvature(const stl_triangle_t* triangle, 
                                  const topology_evaluation_t* eval);
float calculate_dihedral_angle(const stl_triangle_t* tri1, const stl_triangle_t* tri2, 
                              unsigned int shared_edge);
float calculate_triangle_quality(const stl_triangle_t* triangle);
float calculate_triangle_aspect_ratio(const stl_triangle_t* triangle);

// Feature detection functions
int detect_sharp_edges(const stl_file_t* stl, topology_evaluation_t* eval, float threshold);
int detect_corners(const stl_file_t* stl, topology_evaluation_t* eval, float threshold);
int detect_flat_regions(const stl_file_t* stl, topology_evaluation_t* eval, float threshold);

// Analysis and reporting functions
void print_topology_summary(const topology_evaluation_t* eval);
void print_connectivity_analysis(const topology_evaluation_t* eval);
void print_curvature_analysis(const topology_evaluation_t* eval);
void print_feature_analysis(const topology_evaluation_t* eval);
void print_density_analysis(const topology_evaluation_t* eval);
void print_quality_analysis(const topology_evaluation_t* eval);
void export_topology_report(const topology_evaluation_t* eval, const char* filename);

// Geometry utilities
float distance_3d(const float* p1, const float* p2);
float dot_product_3d(const float* v1, const float* v2);
void cross_product_3d(const float* v1, const float* v2, float* result);
void normalize_vector_3d(float* vector);
float vector_length_3d(const float* vector);
float angle_between_vectors(const float* v1, const float* v2);

// Slicing optimization suggestions
typedef struct {
    float recommended_layer_height;     // Based on curvature analysis
    float recommended_infill_density;   // Based on density analysis
    unsigned int recommended_shells;    // Based on quality analysis
    float recommended_speed;            // Based on feature complexity
    char* slicing_strategy;             // Suggested slicing approach
} slicing_recommendations_t;

slicing_recommendations_t* generate_slicing_recommendations(const topology_evaluation_t* eval);
void free_slicing_recommendations(slicing_recommendations_t* recs);
void print_slicing_recommendations(const slicing_recommendations_t* recs);

#endif // TOPOLOGY_EVALUATOR_H 