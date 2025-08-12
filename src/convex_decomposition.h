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

// Point structure for 3D operations
typedef struct {
    float x, y, z;
} point3d_t;

// Face structure for 3D convex hull
typedef struct {
    unsigned int vertices[3];    // Indices of the three vertices
    point3d_t normal;           // Face normal vector
    float d;                    // Distance from origin to plane
} face_t;

// Convex hull structure
typedef struct {
    point3d_t* vertices;        // Vertices of the convex hull
    unsigned int num_vertices;   // Number of vertices
    unsigned int capacity;       // Allocated capacity
    face_t* faces;              // Triangular faces of the convex hull
    unsigned int num_faces;      // Number of faces
    unsigned int face_capacity;  // Allocated face capacity
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

// Convex decomposition node types
typedef enum {
    CONVEX_LEAF,    // Leaf node containing a convex part
    CONVEX_INTERNAL // Internal node with children
} convex_node_type_t;

// Convex decomposition node structure
typedef struct convex_node {
    convex_node_type_t type;
    unsigned int node_id;       // Unique identifier for this node
    float bounds[6];           // Bounding box [min_x, min_y, min_z, max_x, max_y, max_z]
    float concavity;           // Concavity measure of this node
    
    union {
        struct {
            convex_part_t* part;  // The convex part for leaf nodes
        } leaf;
        
        struct {
            struct convex_node* left;
            struct convex_node* right;
        } internal;
    } data;
} convex_node_t;

// Adjacency list entry
typedef struct {
    unsigned int node_id;       // ID of adjacent node
    float overlap_volume;       // Volume of overlap between nodes
    float distance;            // Distance between node centers
} adjacency_entry_t;

// Adjacency list for a node
typedef struct {
    unsigned int node_id;           // ID of the node this list belongs to
    adjacency_entry_t* entries;     // Array of adjacent nodes
    unsigned int num_adjacent;      // Number of adjacent nodes
    unsigned int capacity;          // Allocated capacity
} adjacency_list_t;

// Decomposition result structure
typedef struct {
    convex_node_t* root;           // Root of the hierarchical tree
    unsigned int num_nodes;        // Total number of nodes
    unsigned int num_leaf_nodes;   // Number of leaf nodes (final parts)
    unsigned int max_depth;        // Maximum depth of the tree
    decomposition_strategy_t strategy; // Strategy used
    float total_volume;            // Total volume of all parts
    float decomposition_quality;   // Quality metric (0.0 to 1.0)
    
    // Adjacency information
    adjacency_list_t** adjacency_lists; // Array of adjacency lists for each node
    unsigned int num_adjacency_lists;  // Number of adjacency lists
} convex_decomposition_t;

// Function declarations

// Convex hull operations
convex_hull_t* convex_hull_create(unsigned int initial_capacity);
void free_convex_hull(convex_hull_t* hull);
void add_convex_hull_vertex(convex_hull_t* hull, float x, float y, float z);
void add_convex_hull_face(convex_hull_t* hull, unsigned int v1, unsigned int v2, unsigned int v3);
convex_hull_t* compute_convex_hull_3d(const point3d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm);

// Convex part operations
convex_part_t* convex_part_create(unsigned int initial_capacity);
void free_convex_part(convex_part_t* part);
void add_convex_part_triangle(convex_part_t* part, unsigned int triangle_index);
void compute_convex_part_properties(convex_part_t* part, const stl_file_t* stl);

// Convex node operations
convex_node_t* convex_node_create_leaf(unsigned int node_id, convex_part_t* part);
convex_node_t* convex_node_create_internal(unsigned int node_id, convex_node_t* left, convex_node_t* right);
void free_convex_node(convex_node_t* node);
void compute_convex_node_bounds(convex_node_t* node);
void compute_convex_node_concavity(convex_node_t* node, const stl_file_t* stl);

// Adjacency operations
adjacency_list_t* adjacency_list_create(unsigned int node_id, unsigned int initial_capacity);
void adjacency_list_free(adjacency_list_t* list);
void add_adjacency_list_entry(adjacency_list_t* list, unsigned int adjacent_node_id, 
                             float overlap_volume, float distance);
void build_adjacency_lists(convex_decomposition_t* decomp);
void build_adjacency_for_leaf_nodes(convex_node_t* node, convex_decomposition_t* decomp);
void check_adjacency_with_other_leaves(convex_node_t* current_node, convex_node_t* other_node, 
                                      adjacency_list_t* list, convex_decomposition_t* decomp);
void count_nodes_and_compute_properties(convex_node_t* node, convex_decomposition_t* decomp);
void collect_leaf_volumes(convex_node_t* node, float* volumes, unsigned int* index);

// Decomposition operations
convex_decomposition_t* convex_decomposition_create(unsigned int initial_capacity);
void free_convex_decomposition(convex_decomposition_t* decomp);


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

// Hierarchical decomposition functions
convex_node_t* hierarchical_decompose_part(convex_part_t* part, const stl_file_t* stl,
                                          unsigned int node_id, unsigned int max_parts,
                                          float concavity_tolerance, unsigned int* next_node_id);


// Utility functions
float compute_volume(const convex_hull_t* hull);
float compute_centroid(const convex_hull_t* hull, float* center);
float compute_decomposition_quality(const convex_decomposition_t* decomp);
float compute_part_concavity(const convex_part_t* part, const stl_file_t* stl);
int is_point_inside_hull(const convex_hull_t* hull, float x, float y, float z);
int hulls_intersect(const convex_hull_t* hull1, const convex_hull_t* hull2);
float compute_hull_distance(const convex_hull_t* hull1, const convex_hull_t* hull2);
float compute_overlap_volume(const convex_hull_t* hull1, const convex_hull_t* hull2);

// Analysis and visualization
void print_convex_decomposition_info(const convex_decomposition_t* decomp);
void print_convex_decomposition_info_to_file(const convex_decomposition_t* decomp, FILE* file);
void print_convex_part_info(const convex_part_t* part, unsigned int part_index);
void print_convex_part_info_to_file(const convex_part_t* part, unsigned int part_index, FILE* file);
void print_convex_node_info(const convex_node_t* node, unsigned int depth);
void print_convex_node_info_to_file(const convex_node_t* node, unsigned int depth, FILE* file);
void export_convex_decomposition_to_stl(const convex_decomposition_t* decomp, const char* filename);
void print_decomposition_info(const convex_decomposition_t* decomp);
void print_decomposition_info_to_file(const convex_decomposition_t* decomp, FILE* file);
void convex_decomposition_free(convex_decomposition_t* decomp);

// Aliases for backward compatibility
#define print_convex_decomposition_info print_decomposition_info
#define free_convex_decomposition convex_decomposition_free

// Geometry utilities
float cross_product_2d(float x1, float y1, float x2, float y2);
float dot_product_3d(float x1, float y1, float z1, float x2, float y2, float z2);
float distance_3d(float x1, float y1, float z1, float x2, float y2, float z2);
int orientation_2d(float x1, float y1, float x2, float y2, float x3, float y3);

#endif // CONVEX_DECOMPOSITION_H 