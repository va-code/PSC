#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "convex_decomposition_simple.h"

void print_usage(const char* program_name) {
    printf("Usage: %s <input.stl> [threshold]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  input.stl    - Input STL file to decompose\n");
    printf("  threshold    - Concavity threshold (0.01-1.0, default: 0.05)\n");
    printf("\n");
    printf("This program uses CoACD for convex decomposition and then analyzes\n");
    printf("adjacency relationships between the resulting convex hulls.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s A.stl                    # Use default threshold 0.05\n", program_name);
    printf("  %s A.stl 0.1                # Use threshold 0.1\n", program_name);
}

int main(int argc, char* argv[]) {
    printf("CoACD Convex Decomposition with Adjacency Analysis\n");
    printf("==================================================\n\n");
    
    // Parse command line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    float threshold = 0.05f;  // Default threshold
    
    if (argc >= 3) {
        threshold = (float)atof(argv[2]);
        if (threshold < 0.01f || threshold > 1.0f) {
            printf("ERROR: Threshold must be between 0.01 and 1.0\n");
            return 1;
        }
    }
    
    printf("Input file: %s\n", input_file);
    printf("Threshold: %.3f\n", threshold);
    printf("\n");
    
    // Load STL file
    printf("Loading STL file...\n");
    stl_file_t* mesh = stl_load_file(input_file);
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
    
    stl_file_t** convex_hulls = NULL;
    int num_hulls = 0;
    
    if (!decompose_mesh_with_coacd(mesh, threshold, &convex_hulls, &num_hulls)) {
        printf("ERROR: CoACD decomposition failed\n");
        free_stl(mesh);
        return 1;
    }
    
    // Print results summary
    print_convex_hulls_summary((const stl_file_t**)convex_hulls, num_hulls);
    
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
    
    if (!save_convex_hulls((const stl_file_t**)convex_hulls, num_hulls, full_base)) {
        printf("ERROR: Failed to save some convex hulls\n");
        free_convex_hulls(convex_hulls, num_hulls);
        free_stl(mesh);
        return 1;
    }
    
    printf("Successfully saved %d convex hulls to test_output/\n", num_hulls);
    
    // Cleanup
    free_convex_hulls(convex_hulls, num_hulls);
    free_stl(mesh);
    
    printf("\nCoACD decomposition completed successfully!\n");
    printf("You can now use your existing adjacency analysis tools on the generated convex hulls.\n");
    return 0;
}
