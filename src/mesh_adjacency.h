#ifndef MESH_ADJACENCY_H
#define MESH_ADJACENCY_H

#include "stl_parser.h"
#include "convex_decomposition_simple.h"

// Structure to represent adjacency between two meshes
typedef struct {
    int mesh1_index;           // Index of first mesh
    int mesh2_index;           // Index of second mesh
    int shared_vertices;        // Number of shared vertices
    float contact_area;         // Approximate contact area between meshes
} mesh_adjacency_t;

// Structure to hold adjacency information for all meshes
typedef struct {
    int num_meshes;                    // Total number of meshes
    int num_adjacencies;              // Total number of adjacency relationships
    mesh_adjacency_t* adjacencies;    // Array of adjacency relationships
    int* adjacency_counts;            // Number of adjacencies per mesh
    int** adjacency_lists;            // Lists of adjacent mesh indices for each mesh
} adjacency_graph_t;

// Structure to hold export information for a mesh
typedef struct {
    char filename[256];        // Output filename for this mesh
    int mesh_index;            // Index in the original decomposition
    float concavity_score;     // Concavity score of this mesh
    int depth;                 // Depth in decomposition tree
    int num_triangles;         // Number of triangles in this mesh
    float bounds[6];           // Bounding box of this mesh
} mesh_export_info_t;

// Structure to hold complete export data
typedef struct {
    char base_filename[256];           // Base filename for output files
    int num_meshes;                    // Number of meshes to export
    mesh_export_info_t* mesh_info;     // Information about each mesh
    adjacency_graph_t* adjacency;      // Adjacency information
    decomposition_tree_t* tree;        // Original decomposition tree
} mesh_export_data_t;

/**
 * Detect adjacency between decomposed meshes
 * @param leaves - Array of leaf nodes from decomposition
 * @param num_leaves - Number of leaf nodes
 * @return Adjacency graph structure, NULL on failure
 */
adjacency_graph_t* detect_mesh_adjacency(mesh_tree_node_t** leaves, int num_leaves);

/**
 * Check if two meshes are adjacent by finding shared vertices
 * @param mesh1 - First mesh
 * @param mesh2 - Second mesh
 * @param adjacency - Output adjacency information
 * @return 1 if meshes are adjacent, 0 otherwise
 */
int check_mesh_adjacency(const stl_file_t* mesh1, const stl_file_t* mesh2, mesh_adjacency_t* adjacency);

/**
 * Create export data structure for decomposed meshes
 * @param tree - Decomposition tree
 * @param base_filename - Base filename for output files
 * @return Export data structure, NULL on failure
 */
mesh_export_data_t* create_export_data(const decomposition_tree_t* tree, const char* base_filename);

/**
 * Export individual meshes to STL files
 * @param export_data - Export data structure
 * @param output_dir - Output directory for STL files
 * @return 1 on success, 0 on failure
 */
int export_individual_meshes(const mesh_export_data_t* export_data, const char* output_dir);

/**
 * Export adjacency information to a text file
 * @param export_data - Export data structure
 * @param output_dir - Output directory for adjacency file
 * @return 1 on success, 0 on failure
 */
int export_adjacency_info(const mesh_export_data_t* export_data, const char* output_dir);

/**
 * Export complete decomposition information to JSON format
 * @param export_data - Export data structure
 * @param output_dir - Output directory for JSON file
 * @return 1 on success, 0 on failure
 */
int export_decomposition_info_json(const mesh_export_data_t* export_data, const char* output_dir);

/**
 * Free adjacency graph structure
 * @param graph - Adjacency graph to free
 */
void free_adjacency_graph(adjacency_graph_t* graph);

/**
 * Free export data structure
 * @param export_data - Export data to free
 */
void free_export_data(mesh_export_data_t* export_data);

#endif // MESH_ADJACENCY_H
