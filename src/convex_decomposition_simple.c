#include "coacd_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple CoACD-based convex decomposition that works with your adjacency system
int decompose_mesh_with_coacd(const stl_file_t* input_mesh, float concavity_threshold, 
                              stl_file_t*** output_meshes, int* num_meshes) {
    if (!input_mesh || !output_meshes || !num_meshes) {
        printf("ERROR: Invalid parameters for CoACD decomposition\n");
        return 0;
    }
    
    printf("CoACD Decomposition: Starting with %u triangles\n", input_mesh->num_triangles);
    printf("CoACD Decomposition: Concavity threshold = %.3f\n", concavity_threshold);
    
    // Initialize CoACD
    if (!coacd_init()) {
        printf("ERROR: Failed to initialize CoACD\n");
        return 0;
    }
    
    // Set up CoACD parameters
    coacd_params_t params = COACD_DEFAULT_PARAMS;
    params.threshold = (double)concavity_threshold;
    
    // Adjust parameters based on input size
    if (input_mesh->num_triangles > 10000) {
        // For large meshes, use more aggressive settings
        params.sample_resolution = 1000;
        params.mcts_iteration = 50;
        params.mcts_max_depth = 2;
        printf("CoACD Decomposition: Using optimized settings for large mesh\n");
    } else if (input_mesh->num_triangles < 100) {
        // For small meshes, use more precise settings
        params.sample_resolution = 5000;
        params.mcts_iteration = 200;
        params.mcts_max_depth = 4;
        printf("CoACD Decomposition: Using precise settings for small mesh\n");
    }
    
    // Set log level to reduce output
    coacd_set_log_level("warn");
    
    // Perform decomposition
    coacd_result_t* result = coacd_decompose(input_mesh, &params);
    if (!result) {
        printf("ERROR: CoACD decomposition failed\n");
        // CoACD cleanup not needed
        return 0;
    }
    
    printf("CoACD Decomposition: Generated %d convex hulls\n", result->count);
    
    // Allocate output array
    *output_meshes = malloc(result->count * sizeof(stl_file_t*));
    if (!*output_meshes) {
        printf("ERROR: Failed to allocate memory for output meshes\n");
        coacd_free_result(result);
        // CoACD cleanup not needed
        return 0;
    }
    
    // Copy meshes to output array
    for (int i = 0; i < result->count; i++) {
        (*output_meshes)[i] = result->meshes[i];
    }
    
    *num_meshes = result->count;
    
    // Free the result structure but keep the meshes
    free(result);
    
    // Don't cleanup CoACD here - the caller will handle it
    printf("CoACD Decomposition: Successfully prepared %d convex hulls for adjacency analysis\n", *num_meshes);
    
    return 1;
}

// Helper function to save CoACD results as individual STL files
int save_convex_hulls(const stl_file_t** meshes, int num_meshes, const char* base_filename) {
    if (!meshes || num_meshes <= 0 || !base_filename) {
        printf("ERROR: Invalid parameters for saving convex hulls\n");
        return 0;
    }
    
    printf("Saving %d convex hulls to files...\n", num_meshes);
    
    for (int i = 0; i < num_meshes; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s_part_%03d.stl", base_filename, i);
        
        if (!write_stl_file(meshes[i], filename)) {
            printf("ERROR: Failed to save convex hull %d to %s\n", i, filename);
            return 0;
        }
        
        printf("Saved convex hull %d (%u triangles) to %s\n", 
               i, meshes[i]->num_triangles, filename);
    }
    
    return 1;
}

// Helper function to print convex hulls summary
void print_convex_hulls_summary(const stl_file_t** meshes, int num_meshes) {
    if (!meshes || num_meshes <= 0) {
        printf("No convex hulls to summarize\n");
        return;
    }
    
    printf("\nCoACD Convex Hulls Summary:\n");
    printf("===========================\n");
    printf("Total convex hulls: %d\n", num_meshes);
    
    unsigned int total_triangles = 0;
    for (int i = 0; i < num_meshes; i++) {
        if (meshes[i]) {
            total_triangles += meshes[i]->num_triangles;
            printf("  Hull %d: %u triangles\n", i, meshes[i]->num_triangles);
        }
    }
    
    printf("Total triangles in all hulls: %u\n", total_triangles);
    printf("Average triangles per hull: %.1f\n", 
           num_meshes > 0 ? (float)total_triangles / num_meshes : 0.0f);
    printf("\n");
}

// Cleanup function for convex hulls
void free_convex_hulls(stl_file_t** meshes, int num_meshes) {
    if (!meshes) return;
    
    for (int i = 0; i < num_meshes; i++) {
        if (meshes[i]) {
            free_stl(meshes[i]);
        }
    }
    
    free(meshes);
}
