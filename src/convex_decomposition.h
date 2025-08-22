#ifndef CONVEX_DECOMPOSITION_H
#define CONVEX_DECOMPOSITION_H

#include "stl_parser.h"

// Enum to define cutting plane generation methods
typedef enum {
    PLANE_METHOD_THREE_WORST_POINTS = 0,  // Use 3 worst concavity points
    PLANE_METHOD_TWO_WORST_PLUS_CENTER = 1  // Use 2 worst points + mesh center
} plane_generation_method_t;

// Structure to hold concavity analysis results
typedef struct {
    float concavity_score;          // 1.0 = fully convex, < 1.0 = has concavity
    float worst_point_1[3];         // 3D coordinates of the least convex point
    float worst_point_2[3];         // 3D coordinates of the second least convex point
    float worst_point_3[3];         // 3D coordinates of the third least convex point
    int worst_triangle_index_1;     // Index of the triangle that's least convex
    int worst_triangle_index_2;     // Index of the triangle that's second least convex
    int worst_triangle_index_3;     // Index of the triangle that's third least convex
} concavity_result_t;

// Structure to hold cutting plane information for visualization
typedef struct {
    float point1[3];     // First point defining the plane
    float point2[3];     // Second point defining the plane  
    float point3[3];     // Third point defining the plane
    float normal[3];     // Plane normal vector
    float center[3];     // Plane center point
    int is_valid;        // 1 if this plane data is valid, 0 otherwise
} cutting_plane_t;

// Forward declaration for the tree node
typedef struct mesh_tree_node mesh_tree_node_t;

// Tree node structure for mesh decomposition
struct mesh_tree_node {
    stl_file_t* mesh;                    // The mesh data for this node
    float concavity_score;               // Concavity score of this mesh
    int depth;                           // Depth level in the tree
    int is_leaf;                         // 1 if this is a leaf node (no further decomposition)
    cutting_plane_t cutting_plane;       // Information about the cutting plane used to create this node
    mesh_tree_node_t* left_child;       // First decomposed mesh (or NULL)
    mesh_tree_node_t* right_child;      // Second decomposed mesh (or NULL)
};

// Structure to hold decomposition results and tree statistics
typedef struct {
    mesh_tree_node_t* root;             // Root of the decomposition tree
    int total_nodes;                     // Total number of nodes in tree
    int leaf_nodes;                      // Number of leaf nodes (final meshes)
    int max_depth_reached;               // Maximum depth reached in tree
} decomposition_tree_t;

/**
 * Calculate concavity metric of a mesh using face normal analysis
 * @param stl - STL file structure containing the mesh data
 * @param result - Output structure containing concavity score and worst points
 * @return 1 on success, 0 on failure
 */
int check_concavity(const stl_file_t* stl, concavity_result_t* result);

/**
 * Recursively decompose a mesh into a tree structure
 * @param mesh - STL mesh to decompose
 * @param concavity_threshold - Target concavity score (0.0 to 1.0)
 * @param max_depth - Maximum recursion depth
 * @param plane_method - Method for generating cutting planes
 * @return Decomposition tree containing all mesh pieces, NULL on failure
 */
decomposition_tree_t* decompose_mesh_tree(const stl_file_t* mesh, float concavity_threshold, int max_depth, plane_generation_method_t plane_method);

/**
 * Create a new tree node
 * @param mesh - STL mesh for this node
 * @param depth - Depth level of this node
 * @return New tree node, NULL on failure
 */
mesh_tree_node_t* create_tree_node(stl_file_t* mesh, int depth);

/**
 * Free the entire decomposition tree
 * @param tree - Tree to free
 */
void free_decomposition_tree(decomposition_tree_t* tree);

/**
 * Free a tree node and all its children recursively
 * @param node - Node to free
 */
void free_tree_node(mesh_tree_node_t* node);

/**
 * Print tree structure for debugging
 * @param tree - Tree to print
 */
void print_decomposition_tree(const decomposition_tree_t* tree);

/**
 * Get all leaf nodes (final decomposed meshes) from the tree
 * @param tree - Tree to extract leaves from
 * @param leaf_array - Output array of leaf nodes (caller must allocate)
 * @param max_leaves - Maximum number of leaves to extract
 * @return Number of leaves extracted
 */
int get_leaf_nodes(const decomposition_tree_t* tree, mesh_tree_node_t** leaf_array, int max_leaves);

#endif // CONVEX_DECOMPOSITION_H
