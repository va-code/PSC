#include <stdio.h>
#include <stdlib.h>
#include "stl_parser.h"
#include "PSC_model_inspector.h"
#include "convex_decomposition.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("PSC Convex Decomposition Visualizer\n");
        printf("Usage: %s <stl_file> <threshold> <max_depth> [plane_method] [show_planes]\n", argv[0]);
        printf("  threshold: 0.0-1.0 (0.8 recommended)\n");
        printf("  max_depth: maximum recursion depth (3-5 recommended)\n");
        printf("  plane_method: 0=three_worst_points, 1=two_worst_plus_center (default: 1)\n");
        printf("  show_planes: 0=hide, 1=show cutting planes (default: 0)\n");
        printf("Example: %s model.stl 0.8 4 1 1\n", argv[0]);
        return 1;
    }
    
    char* stl_file = argv[1];
    float threshold = atof(argv[2]);
    int max_depth = atoi(argv[3]);
    plane_generation_method_t plane_method = PLANE_METHOD_TWO_WORST_PLUS_CENTER;  // Default
    int show_planes = 0;  // Default: don't show cutting planes
    
    // Optional 4th parameter for plane method
    if (argc >= 5) {
        int method = atoi(argv[4]);
        if (method == 0) {
            plane_method = PLANE_METHOD_THREE_WORST_POINTS;
        } else if (method == 1) {
            plane_method = PLANE_METHOD_TWO_WORST_PLUS_CENTER;
        } else {
            printf("Warning: Invalid plane method %d, using default (1)\n", method);
        }
    }
    
    // Optional 5th parameter for showing cutting planes
    if (argc >= 6) {
        show_planes = atoi(argv[5]);
        if (show_planes != 0 && show_planes != 1) {
            printf("Warning: Invalid show_planes value %d, using default (0)\n", show_planes);
            show_planes = 0;
        }
    }
    
    printf("PSC Convex Decomposition Visualizer\n");
    printf("===================================\n");
    printf("STL file: %s\n", stl_file);
    printf("Threshold: %.3f\n", threshold);
    printf("Max depth: %d\n", max_depth);
    printf("Plane method: %s\n", 
           plane_method == PLANE_METHOD_THREE_WORST_POINTS ? "3 worst points" : "2 worst + center");
    printf("Show cutting planes: %s\n\n", show_planes ? "Yes" : "No");
    
    // Load STL file
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file: %s\n", stl_file);
        return 1;
    }
    
    // Run convex decomposition visualization
    display_convex_decomposition_tree(stl, threshold, max_depth, plane_method, show_planes);
    
    // Cleanup
    free_stl(stl);
    
    return 0;
}
