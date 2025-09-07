#ifndef CONVEX_DECOMPOSITION_COACD_H
#define CONVEX_DECOMPOSITION_COACD_H

#include "convex_decomposition.h"
#include "coacd_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decompose a mesh using CoACD algorithm
 * @param mesh - Input STL mesh to decompose
 * @param concavity_threshold - Concavity threshold (0.01-1.0)
 * @param max_depth - Maximum decomposition depth (unused for CoACD)
 * @param plane_method - Plane generation method (unused for CoACD)
 * @return Decomposition tree, NULL on failure
 */
decomposition_tree_t* decompose_mesh_tree_coacd(const stl_file_t* mesh, float concavity_threshold, int max_depth, plane_generation_method_t plane_method);

/**
 * Decompose a mesh using CoACD algorithm (direct result)
 * @param mesh - Input STL mesh to decompose
 * @param concavity_threshold - Concavity threshold (0.01-1.0)
 * @return CoACD result containing convex hulls, NULL on failure
 */
coacd_result_t* decompose_mesh_coacd_direct(const stl_file_t* mesh, float concavity_threshold);

/**
 * Save CoACD results as individual STL files
 * @param result - CoACD decomposition result
 * @param base_filename - Base filename for output files
 * @return 1 on success, 0 on failure
 */
int save_coacd_results(const coacd_result_t* result, const char* base_filename);

/**
 * Print CoACD results summary
 * @param result - CoACD decomposition result
 */
void print_coacd_results_summary(const coacd_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // CONVEX_DECOMPOSITION_COACD_H
