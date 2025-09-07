#ifndef COACD_WRAPPER_H
#define COACD_WRAPPER_H

#include "stl_parser.h"

// Forward declaration to avoid circular dependency
typedef struct decomposition_tree decomposition_tree_t;

#ifdef __cplusplus
extern "C" {
#endif

// Structure to hold CoACD decomposition results
typedef struct {
    stl_file_t** meshes;        // Array of decomposed STL meshes
    int count;                  // Number of decomposed meshes
    int capacity;               // Allocated capacity
} coacd_result_t;

// CoACD parameters structure
typedef struct {
    double threshold;           // Concavity threshold (0.01-1.0)
    int max_convex_hull;       // Maximum number of convex hulls (-1 for unlimited)
    int preprocess_mode;        // 0=auto, 1=on, 2=off
    int prep_resolution;        // Preprocessing resolution (20-100)
    int sample_resolution;      // Sampling resolution (1000-10000)
    int mcts_nodes;            // MCTS nodes (10-40)
    int mcts_iteration;        // MCTS iterations (60-2000)
    int mcts_max_depth;        // MCTS max depth (2-7)
    int pca;                   // Enable PCA preprocessing (0/1)
    int merge;                 // Enable merge postprocessing (0/1)
    int decimate;              // Enable decimation (0/1)
    int max_ch_vertex;         // Max vertices per convex hull (when decimate=1)
    int extrude;               // Enable extrusion (0/1)
    double extrude_margin;     // Extrusion margin (when extrude=1)
    int apx_mode;              // Approximation mode (0=convex hull, 1=box)
    unsigned int seed;         // Random seed (0 for random)
} coacd_params_t;

// Default CoACD parameters
#define COACD_DEFAULT_PARAMS { \
    .threshold = 0.05, \
    .max_convex_hull = -1, \
    .preprocess_mode = 0, \
    .prep_resolution = 50, \
    .sample_resolution = 2000, \
    .mcts_nodes = 20, \
    .mcts_iteration = 100, \
    .mcts_max_depth = 3, \
    .pca = 0, \
    .merge = 1, \
    .decimate = 0, \
    .max_ch_vertex = 256, \
    .extrude = 0, \
    .extrude_margin = 0.01, \
    .apx_mode = 0, \
    .seed = 0 \
}

/**
 * Initialize CoACD library
 * @return 1 on success, 0 on failure
 */
int coacd_init(void);

// Note: coacd_cleanup function removed - not needed for CoACD

/**
 * Decompose an STL mesh using CoACD
 * @param input_mesh - Input STL mesh to decompose
 * @param params - CoACD parameters (use COACD_DEFAULT_PARAMS for defaults)
 * @return Decomposition result containing array of convex meshes, NULL on failure
 * 
 * Note: The caller is responsible for freeing the result using coacd_free_result()
 */
coacd_result_t* coacd_decompose(const stl_file_t* input_mesh, const coacd_params_t* params);

/**
 * Free a CoACD decomposition result
 * @param result - Result to free (can be NULL)
 */
void coacd_free_result(coacd_result_t* result);

// Note: coacd_result_to_tree function removed to avoid dependency on decomposition_tree_t

/**
 * Set CoACD log level
 * @param level - Log level string ("off", "debug", "info", "warn", "error", "critical")
 */
void coacd_set_log_level(const char* level);

#ifdef __cplusplus
}
#endif

#endif // COACD_WRAPPER_H
