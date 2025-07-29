#include <stdio.h>
#include <stdlib.h>
#include "stl_parser.h"
#include "convex_decomposition.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <stl_file> [strategy] [max_parts] [quality] [concavity]\n", argv[0]);
        printf("Strategies: 0=approx, 1=exact, 2=hierarchical, 3=voxel\n");
        printf("Example: %s test_cube.stl 0 8 0.8 0.1\n", argv[0]);
        return 1;
    }
    
    char* filename = argv[1];
    decomposition_strategy_t strategy = (argc > 2) ? atoi(argv[2]) : DECOMP_APPROX_CONVEX;
    unsigned int max_parts = (argc > 3) ? atoi(argv[3]) : 8;
    float quality = (argc > 4) ? atof(argv[4]) : 0.8f;
    float concavity = (argc > 5) ? atof(argv[5]) : 0.1f;
    
    printf("Convex Decomposition Test Program\n");
    printf("================================\n\n");
    
    // Load STL file
    printf("Loading STL file: %s\n", filename);
    stl_file_t* stl = stl_load_file(filename);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file\n");
        return 1;
    }
    
    // Print STL information
    stl_print_info(stl);
    printf("\n");
    
    // Create convex decomposition
    printf("Creating convex decomposition...\n");
    printf("Strategy: %d, Max parts: %u, Quality threshold: %.2f, Concavity tolerance: %.2f\n", 
           strategy, max_parts, quality, concavity);
    
    // Create decomposition parameters
    decomposition_params_t params = {
        .strategy = strategy,
        .max_parts = max_parts,
        .quality_threshold = quality,
        .concavity_tolerance = concavity,
        .voxel_size = 1.0f,
        .min_triangles_per_voxel = 10
    };
    
    convex_decomposition_t* decomp = decompose_model(stl, &params);
    if (!decomp) {
        fprintf(stderr, "Error: Failed to create convex decomposition\n");
        stl_free(stl);
        return 1;
    }
    
    // Print decomposition information
    print_decomposition_info(decomp);
    
    // Test different strategies
    printf("\nTesting different decomposition strategies:\n");
    printf("===========================================\n");
    
    const char* strategy_names[] = {"Approximate", "Exact", "Hierarchical", "Voxel"};
    for (int s = 0; s < 4; s++) {
        printf("\n%s Convex Decomposition:\n", strategy_names[s]);
        printf("------------------------\n");
        
        decomposition_params_t test_params = {
            .strategy = s,
            .max_parts = max_parts,
            .quality_threshold = quality,
            .concavity_tolerance = concavity,
            .voxel_size = 1.0f,
            .min_triangles_per_voxel = 10
        };
        
        convex_decomposition_t* test_decomp = decompose_model(stl, &test_params);
        if (test_decomp) {
            print_decomposition_info(test_decomp);
            convex_decomposition_free(test_decomp);
        } else {
            printf("Failed to create decomposition\n");
        }
    }
    
    // Test different part counts
    printf("\nTesting different part counts:\n");
    printf("==============================\n");
    
    unsigned int part_counts[] = {2, 4, 8, 16};
    for (int p = 0; p < 4; p++) {
        printf("\nMax parts: %u\n", part_counts[p]);
        printf("-----------\n");
        
        decomposition_params_t test_params = {
            .strategy = strategy,
            .max_parts = part_counts[p],
            .quality_threshold = quality,
            .concavity_tolerance = concavity,
            .voxel_size = 1.0f,
            .min_triangles_per_voxel = 10
        };
        
        convex_decomposition_t* test_decomp = decompose_model(stl, &test_params);
        if (test_decomp) {
            print_decomposition_info(test_decomp);
            convex_decomposition_free(test_decomp);
        } else {
            printf("Failed to create decomposition\n");
        }
    }
    
    // Cleanup
    convex_decomposition_free(decomp);
    stl_free(stl);
    
    printf("\nConvex decomposition test completed successfully!\n");
    return 0;
} 