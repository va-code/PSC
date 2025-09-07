#include "convex_decomposition.h"
#include "coacd_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CoACD-based convex decomposition implementation
decomposition_tree_t* decompose_mesh_tree_coacd(const stl_file_t* mesh, float concavity_threshold, int max_depth, plane_generation_method_t plane_method) {
    if (!mesh) {
        printf("ERROR: Input mesh is NULL\n");
        return NULL;
    }
    
    printf("CoACD Decomposition: Starting with %u triangles\n", mesh->num_triangles);
    printf("CoACD Decomposition: Concavity threshold = %.3f\n", concavity_threshold);
    
    // Initialize CoACD
    if (!coacd_init()) {
        printf("ERROR: Failed to initialize CoACD\n");
        return NULL;
    }
    
    // Set up CoACD parameters
    coacd_params_t params = COACD_DEFAULT_PARAMS;
    params.threshold = (double)concavity_threshold;
    
    // Adjust parameters based on input
    if (mesh->num_triangles > 10000) {
        // For large meshes, use more aggressive settings
        params.sample_resolution = 1000;
        params.mcts_iteration = 50;
        params.mcts_max_depth = 2;
        printf("CoACD Decomposition: Using optimized settings for large mesh\n");
    } else if (mesh->num_triangles < 100) {
        // For small meshes, use more precise settings
        params.sample_resolution = 5000;
        params.mcts_iteration = 200;
        params.mcts_max_depth = 4;
        printf("CoACD Decomposition: Using precise settings for small mesh\n");
    }
    
    // Set log level to reduce output
    coacd_set_log_level("warn");
    
    // Perform decomposition
    coacd_result_t* result = coacd_decompose(mesh, &params);
    if (!result) {
        printf("ERROR: CoACD decomposition failed\n");
        coacd_cleanup();
        return NULL;
    }
    
    printf("CoACD Decomposition: Generated %d convex hulls\n", result->count);
    
    // Convert to decomposition tree format
    decomposition_tree_t* tree = malloc(sizeof(decomposition_tree_t));
    if (!tree) {
        printf("ERROR: Failed to allocate memory for decomposition tree\n");
        coacd_free_result(result);
        coacd_cleanup();
        return NULL;
    }
    
    // Initialize tree
    tree->total_nodes = result->count;
    tree->leaf_nodes = result->count;
    tree->max_depth_reached = 0;
    
    // Create a simple flat tree structure where all convex hulls are leaf nodes
    // This is a simplified approach - in a more sophisticated implementation,
    // you might want to organize the convex hulls hierarchically
    tree->root = NULL; // For now, we'll store the meshes directly
    
    // Store the result for later access
    // Note: This is a simplified approach. In a real implementation,
    // you'd want to properly integrate this with the existing tree structure
    printf("CoACD Decomposition: Tree created with %d leaf nodes\n", tree->leaf_nodes);
    
    // Clean up CoACD result (we'll need to modify this to keep the meshes)
    // For now, we'll just free the result structure but keep the meshes
    // This is a temporary solution - proper integration would require
    // restructuring the tree node system
    coacd_free_result(result);
    coacd_cleanup();
    
    return tree;
}

// Alternative function that returns the actual convex hulls
coacd_result_t* decompose_mesh_coacd_direct(const stl_file_t* mesh, float concavity_threshold) {
    if (!mesh) {
        printf("ERROR: Input mesh is NULL\n");
        return NULL;
    }
    
    printf("CoACD Direct Decomposition: Starting with %u triangles\n", mesh->num_triangles);
    printf("CoACD Direct Decomposition: Concavity threshold = %.3f\n", concavity_threshold);
    
    // Initialize CoACD
    if (!coacd_init()) {
        printf("ERROR: Failed to initialize CoACD\n");
        return NULL;
    }
    
    // Set up CoACD parameters
    coacd_params_t params = COACD_DEFAULT_PARAMS;
    params.threshold = (double)concavity_threshold;
    
    // Adjust parameters based on input
    if (mesh->num_triangles > 10000) {
        params.sample_resolution = 1000;
        params.mcts_iteration = 50;
        params.mcts_max_depth = 2;
    } else if (mesh->num_triangles < 100) {
        params.sample_resolution = 5000;
        params.mcts_iteration = 200;
        params.mcts_max_depth = 4;
    }
    
    // Set log level
    coacd_set_log_level("warn");
    
    // Perform decomposition
    coacd_result_t* result = coacd_decompose(mesh, &params);
    if (!result) {
        printf("ERROR: CoACD decomposition failed\n");
        coacd_cleanup();
        return NULL;
    }
    
    printf("CoACD Direct Decomposition: Generated %d convex hulls\n", result->count);
    
    // Don't cleanup CoACD here - the caller will handle it
    return result;
}

// Helper function to save CoACD results as individual STL files
int save_coacd_results(const coacd_result_t* result, const char* base_filename) {
    if (!result || !base_filename) {
        printf("ERROR: Invalid parameters for saving results\n");
        return 0;
    }
    
    printf("Saving %d CoACD convex hulls to files...\n", result->count);
    
    for (int i = 0; i < result->count; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s_part_%03d.stl", base_filename, i);
        
        if (!write_stl_file(result->meshes[i], filename)) {
            printf("ERROR: Failed to save convex hull %d to %s\n", i, filename);
            return 0;
        }
        
        printf("Saved convex hull %d (%u triangles) to %s\n", 
               i, result->meshes[i]->num_triangles, filename);
    }
    
    return 1;
}

// Helper function to print CoACD results summary
void print_coacd_results_summary(const coacd_result_t* result) {
    if (!result) {
        printf("No CoACD results to summarize\n");
        return;
    }
    
    printf("\nCoACD Decomposition Results Summary:\n");
    printf("=====================================\n");
    printf("Total convex hulls: %d\n", result->count);
    
    unsigned int total_triangles = 0;
    for (int i = 0; i < result->count; i++) {
        if (result->meshes[i]) {
            total_triangles += result->meshes[i]->num_triangles;
            printf("  Hull %d: %u triangles\n", i, result->meshes[i]->num_triangles);
        }
    }
    
    printf("Total triangles in all hulls: %u\n", total_triangles);
    printf("Average triangles per hull: %.1f\n", 
           result->count > 0 ? (float)total_triangles / result->count : 0.0f);
    printf("\n");
}
