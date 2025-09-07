#ifndef CONVEX_DECOMPOSITION_SIMPLE_H
#define CONVEX_DECOMPOSITION_SIMPLE_H

#include "stl_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decompose a mesh using CoACD algorithm (simplified version)
 * @param input_mesh - Input STL mesh to decompose
 * @param concavity_threshold - Concavity threshold (0.01-1.0)
 * @param output_meshes - Output array of convex hull meshes (caller must free)
 * @param num_meshes - Number of convex hulls generated
 * @return 1 on success, 0 on failure
 */
int decompose_mesh_with_coacd(const stl_file_t* input_mesh, float concavity_threshold, 
                              stl_file_t*** output_meshes, int* num_meshes);

/**
 * Save convex hulls as individual STL files
 * @param meshes - Array of convex hull meshes
 * @param num_meshes - Number of meshes
 * @param base_filename - Base filename for output files
 * @return 1 on success, 0 on failure
 */
int save_convex_hulls(const stl_file_t** meshes, int num_meshes, const char* base_filename);

/**
 * Print convex hulls summary
 * @param meshes - Array of convex hull meshes
 * @param num_meshes - Number of meshes
 */
void print_convex_hulls_summary(const stl_file_t** meshes, int num_meshes);

/**
 * Free convex hulls array
 * @param meshes - Array of convex hull meshes
 * @param num_meshes - Number of meshes
 */
void free_convex_hulls(stl_file_t** meshes, int num_meshes);

#ifdef __cplusplus
}
#endif

#endif // CONVEX_DECOMPOSITION_SIMPLE_H
