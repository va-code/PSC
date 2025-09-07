#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "convex_decomposition_coacd.h"
#include "coacd_wrapper.h"

void print_usage(const char* program_name) {
    printf("Usage: %s <input.stl> [threshold] [max_depth]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  input.stl    - Input STL file to decompose\n");
    printf("  threshold    - Concavity threshold (0.01-1.0, default: 0.05)\n");
    printf("  max_depth    - Maximum decomposition depth (default: 3)\n");
    printf("\n");
    printf("This program uses CoACD (Collision-Aware Concavity) for convex decomposition.\n");
    printf("CoACD is a state-of-the-art algorithm that produces high-quality convex hulls\n");
    printf("with better collision detection properties than traditional methods.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s A.stl                    # Use default settings\n", program_name);
    printf("  %s A.stl 0.1                # Use threshold 0.1\n", program_name);
    printf("  %s A.stl 0.05 5             # Use threshold 0.05 and max depth 5\n", program_name);
}

int main(int argc, char* argv[]) {
    printf("CoACD Convex Decomposition Tool\n");
    printf("===============================\n\n");
    
    // Parse command line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    float threshold = 0.05f;  // Default threshold
    int max_depth = 3;        // Default max depth
    
    if (argc >= 3) {
        threshold = (float)atof(argv[2]);
        if (threshold < 0.01f || threshold > 1.0f) {
            printf("ERROR: Threshold must be between 0.01 and 1.0\n");
            return 1;
        }
    }
    
    if (argc >= 4) {
        max_depth = atoi(argv[3]);
        if (max_depth < 1 || max_depth > 10) {
            printf("ERROR: Max depth must be between 1 and 10\n");
            return 1;
        }
    }
    
    printf("Input file: %s\n", input_file);
    printf("Threshold: %.3f\n", threshold);
    printf("Max depth: %d\n", max_depth);
    printf("\n");
    
    // Load STL file
    printf("Loading STL file...\n");
    stl_file_t* mesh = load_stl_file(input_file);
    if (!mesh) {
        printf("ERROR: Failed to load STL file: %s\n", input_file);
        return 1;
    }
    
    printf("Loaded mesh: %u triangles\n", mesh->num_triangles);
    
    // Calculate mesh bounds
    calculate_stl_bounds(mesh);
    printf("Mesh bounds: X[%.3f, %.3f], Y[%.3f, %.3f], Z[%.3f, %.3f]\n",
           mesh->bounds[0], mesh->bounds[3],
           mesh->bounds[1], mesh->bounds[4],
           mesh->bounds[2], mesh->bounds[5]);
    printf("\n");
    
    // Perform CoACD decomposition
    printf("Starting CoACD convex decomposition...\n");
    printf("This may take a while for complex meshes...\n\n");
    
    coacd_result_t* result = decompose_mesh_coacd_direct(mesh, threshold);
    if (!result) {
        printf("ERROR: CoACD decomposition failed\n");
        free_stl(mesh);
        return 1;
    }
    
    // Print results summary
    print_coacd_results_summary(result);
    
    // Save results
    printf("Saving convex hulls to files...\n");
    
    // Create output directory if it doesn't exist
    system("mkdir -p test_output");
    
    // Generate base filename from input
    char base_filename[256];
    const char* last_slash = strrchr(input_file, '/');
    const char* filename = last_slash ? last_slash + 1 : input_file;
    
    // Remove .stl extension
    strncpy(base_filename, filename, sizeof(base_filename) - 1);
    base_filename[sizeof(base_filename) - 1] = '\0';
    char* dot = strrchr(base_filename, '.');
    if (dot) {
        *dot = '\0';
    }
    
    // Add path prefix
    char full_base[512];
    snprintf(full_base, sizeof(full_base), "test_output/%s", base_filename);
    
    if (!save_coacd_results(result, full_base)) {
        printf("ERROR: Failed to save some convex hulls\n");
        coacd_free_result(result);
        free_stl(mesh);
        return 1;
    }
    
    printf("Successfully saved %d convex hulls to test_output/\n", result->count);
    
    // Cleanup
    coacd_free_result(result);
    free_stl(mesh);
    
    printf("\nCoACD decomposition completed successfully!\n");
    return 0;
}
